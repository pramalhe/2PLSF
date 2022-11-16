#pragma once

#include <atomic>
#include <exception>
#include <setjmp.h>

#include "../../zardoshti/common/bytelock_t.h"
#include "../../zardoshti/common/deferred.h"
#include "../../zardoshti/common/minivector.h"
#include "../../zardoshti/common/pad_word.h"
#include "../../zardoshti/common/platform.h"
#include "../../zardoshti/common/undolog_nonatomic.h"

/// TLRWEager is an STM algorithm with the following characteristics:
/// - Uses "bytelocks" as readers/writer locks for encounter-time write locking
///   and pessimistic read locking
/// - Performs speculative writes in-place (uses undo)
/// - Uses a wait-then-self-abort strategy to avoid deadlocks
/// - Inherently privatization safe and validation-free
///
/// TLRWEager can be customized in the following ways:
/// - Size of bytelock table (and bytelock size / max thread count)
/// - EpochManager (just for irrevocability... quiescence isn't needed)
/// - Contention manager
/// - Stack Frame (to bring some of the caller frame into tx scope)
/// - Allocator (to become irrevocable on too many allocations)
/// - Deadlock avoidance tuning (read tries, read spins, write tries, write
///   spins)
///
/// Note that the published TLRW algorithm has bytelock support for up to some
/// fixed number of transactions, and all other transactions use a fall-back.
/// Our implementation simply forbids more transactions than there are slots
///
/// Also, please be warned that TLRW is *extremely* succeptible to deadlock.
/// The four integer tuning parameters can have a tremendous impact on overall
/// performance, and even then, for some workloads, it is almost impossible to
/// find a good contention manager (other than irrevocability) for ensuring good
/// performance.
///
/// Lastly, note that TLRWEager is a pessimistic TM: threads must hold actual
/// locks before writing *or* reading.  Consequently, TLRW does not have any
/// issues with respect to the C++ memory model, and can use a nonatomic undolog
/// without racing.
template <class BYTELOCKTABLE, class EPOCH, class CM, class STACKFRAME,
          class ALLOCATOR, int READ_TRIES, int READ_SPINS, int WRITE_TRIES,
          int WRITE_SPINS>
class TLRWEager {
  /// Globals is a wrapper around all of the global variables used by TLRWEager
  struct Globals {
    /// The table of bytelocks for concurrency control
    BYTELOCKTABLE bytelocks;

    /// The contention management metadata
    typename CM::Globals cm;

    /// Quiescence support
    typename EPOCH::Globals epoch;
  };

  /// All metadata shared among threads
  static Globals globals;

  /// Checkpoint lets us reset registers and the instruction pointer in the
  /// event of an abort
  jmp_buf *checkpoint = nullptr;

  /// For managing thread Ids, Quiescence, and Irrevocability
  EPOCH epoch;

  /// Contention manager
  CM cm;

  /// For managing the stack frame
  STACKFRAME frame;

  /// The slot used by this thread
  uint64_t my_slot;

  /// all of the bytelocks this transaction has read-locked
  MiniVector<typename BYTELOCKTABLE::bytelock_t *> readset;

  /// all of the bytelocks this transaction has write-locked
  MiniVector<typename BYTELOCKTABLE::bytelock_t *> lockset;

  /// original values that this transaction overwrote
  UndoLog_Nonatomic undolog;

  /// The allocator manages malloc, free, and aligned alloc
  ALLOCATOR allocator;

  /// deferredActions manages all functions that should run after transaction
  /// commit.
  DeferredActionHandler deferredActions;

public:
  /// Return the irrevocability state of the thread
  bool isIrrevoc() { return epoch.isIrrevoc(); }

  /// Set the current bottom of the transactional part of the stack
  void adjustStackBottom(void *addr) { frame.setBottom(addr); }

  /// construct a thread's transaction context by zeroing its nesting depth and
  /// giving it an ID.  We also cache its lock token.
  TLRWEager() : epoch(globals.epoch), cm() {
    my_slot = epoch.id;
    // crash immediately if we have an invalid bytelock
    globals.bytelocks.validate_id(my_slot);
  }

  /// Instrumentation to run at the beginning of a transaction boundary.
  void beginTx(jmp_buf *b) {
    // onBegin == false -> flat nesting
    if (frame.onBegin()) {
      // Save the checkpoint and set the stack bottom
      checkpoint = b;
      frame.setBottom(b);

      // Start logging allocations
      allocator.onBegin();

      // Wait until there are no irrevocable transactions.
      epoch.onBegin(globals.epoch, 1);

      // Notify CM of intention to start.  If return true, become irrevocable
      if (cm.beforeBegin(globals.cm)) {
        becomeIrrevocable();
      }
    }
  }

  /// Instrumentation to run at the end of a transaction boundary.
  void commitTx() {
    // onEnd == false -> flat nesting
    if (frame.onEnd()) {
      // Irrevocable commit is easy, because we reset the lists when we became
      // irrevocable
      if (epoch.isIrrevoc()) {
        epoch.onCommitIrrevoc(globals.epoch);
        cm.afterCommit(globals.cm);
        deferredActions.onCommit();
        frame.onCommit();
        return;
      }

      // depart epoch table (fence) and then release locks
      //
      // NB: there may be unnecessary fences in this code
      epoch.clearEpoch(globals.epoch);
      // Drop all write locks
      for (auto bl : lockset) {
        bl->owner.store(0, std::memory_order_relaxed);
      }
      // Drop all read locks
      for (auto bl : readset) {
        bl->readers[my_slot].store(0, std::memory_order_relaxed);
      }
      // clear lists
      undolog.clear();
      lockset.clear();
      readset.clear();
      cm.afterCommit(globals.cm);
      epoch.quiesce(globals.epoch, 2);
      allocator.onCommit();
      deferredActions.onCommit();
      frame.onCommit();
    }
  }

  /// To allocate memory, we must also log it, so we can reclaim it if the
  /// transaction aborts
  void *txAlloc(size_t size) {
    return allocator.alloc(size, [&]() { becomeIrrevocable(); });
  }

  /// To allocate aligned memory, we must also log it, so we can reclaim it if
  /// the transaction aborts
  void *txAAlloc(size_t A, size_t size) {
    return allocator.alignAlloc(A, size, [&]() { becomeIrrevocable(); });
  }

  /// To free memory, we simply wait until the transaction has committed, and
  /// then we free.
  void txFree(void *addr) { allocator.reclaim(addr); }

  /// Transactional read
  template <typename T> T read(T *addr) {
    // No instrumentation if on stack
    if (accessDirectly(addr)) {
      return *addr;
    }

    // Get the bytelock addr, and check the easy cases
    auto bl = globals.bytelocks.get(addr);
    if (bl->readers[my_slot] != 0 || bl->owner == (my_slot + 1)) {
      return *addr;
    }

    // We might as well log the lock now.  If we abort, releasing won't be an
    // issue.
    readset.push_back(bl);

    // Do read acquisition in a loop, because we might need to try multiple
    // times
    int tries = 0;
    while (true) {
      // In the best case, we write to the readers slot, and then see that there
      // is no owner.
      bl->readers[my_slot] = 1; // NB: this involves a fence!
      if (bl->owner == 0) {     // NB: this may produce an unnecessary fence
        return *addr;
      }

      // In the best case, the owner is about to finish, and isn't waiting
      // on this thread, so if we release the lock and wait a bit, things
      // clear up
      bl->readers[my_slot] = 0;
      if (++tries == READ_TRIES) {
        abortTx();
      }
      spinX(READ_SPINS);
    }
  }

  /// Transactional write
  template <typename T> void write(T *addr, T val) {
    // No instrumentation if on stack
    if (accessDirectly(addr)) {
      *addr = val;
      return;
    }

    // get the bytelock addr, and check the easy case
    auto bl = globals.bytelocks.get(addr);
    if (bl->owner == (my_slot + 1)) {
      // NB: still need to undo-log first
      UndoLog_Nonatomic::undo_t u;
      u.initFromAddr(addr);
      undolog.push_back(u);
      *addr = val;
      return;
    }

    // For deadlock avoidance, we abort immediately if we can't get the
    // owner field
    uintptr_t unheld = 0;
    if (!bl->owner.compare_exchange_strong(unheld, (my_slot + 1))) {
      abortTx();
    }
    lockset.push_back(bl);

    // Drop our read lock, to make the checks easier
    bl->readers[my_slot] = 0;
    // Having the lock isn't enough... We need to wait for readers to drain out
    int tries = 0;
    while (true) {
      uintptr_t count = globals.epoch.getThreads();
      unsigned char conflicts = 0;
      for (uintptr_t i = 0; i < count; ++i) {
        conflicts |= bl->readers[i];
      }
      if (!conflicts) {
        break;
      }
      if (++tries == WRITE_TRIES) {
        abortTx();
      }
      spinX(WRITE_SPINS);
    }

    // If we made it out of the loop, there are no readers, so we can write
    UndoLog_Nonatomic::undo_t u;
    u.initFromAddr(addr);
    undolog.push_back(u);
    *addr = val;
  }

  /// Instrumentation to become irrevocable in-flight.  This is essentially an
  /// early commit
  void becomeIrrevocable() {
    // Immediately return if we are already irrevocable
    if (epoch.isIrrevoc()) {
      return;
    }

    // try_irrevoc will return true only if we got the token and quiesced
    if (!epoch.tryIrrevoc(globals.epoch)) {
      abortTx();
    }

    // No need to validate, so release locks now
    for (auto bl : lockset) {
      bl->owner.store(0, std::memory_order_relaxed);
    }
    for (auto bl : readset) {
      bl->readers[my_slot].store(0, std::memory_order_relaxed);
    }
    // clear lists
    allocator.onCommit();
    readset.clear();
    lockset.clear();
    undolog.clear();
  }

  /// Register an action to run after transaction commit
  void registerCommitHandler(void (*func)(void *), void *args) {
    deferredActions.registerHandler(func, args);
  }

private:
  /// Abort the transaction. We must handle mallocs and frees, and we need to
  /// ensure that the TLRWEager object is in an appropriate state for starting a
  /// new transaction.  Note that we *will* call beginTx again, unlike libITM.
  void abortTx() {
    // undo any writes
    undolog.undo_writes_nonatomic();

    // At this point, we can exit the epoch so that other threads don't have to
    // wait on this thread
    epoch.clearEpoch(globals.epoch);
    cm.afterAbort(globals.cm, epoch.id);

    // Drop all read locks
    for (auto bl : readset) {
      bl->readers[my_slot].store(0, std::memory_order_relaxed);
    }
    // Drop all write locks
    for (auto bl : lockset) {
      bl->owner.store(0, std::memory_order_relaxed);
    }

    // reset all lists
    readset.clear();
    undolog.clear();
    lockset.clear();
    allocator.onAbort();
    deferredActions.onAbort();
    frame.onAbort();
    longjmp(*checkpoint, 1);
  }

  /// Check if the given address is on the thread's stack, and hence does not
  /// need instrumentation.  Note that if the thread is irrevocable, we also say
  /// that instrumentation is not needed.  Also, the allocator may suggest
  /// skipping instrumentation.
  bool accessDirectly(void *ptr) {
    if (epoch.isIrrevoc())
      return true;
    if (allocator.checkCaptured(ptr))
      return true;
    return frame.onStack(ptr);
  }
};