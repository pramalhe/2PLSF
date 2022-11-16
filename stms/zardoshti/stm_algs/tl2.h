#pragma once

#include <atomic>
#include <setjmp.h>

#include "../../zardoshti/common/deferred.h"
#include "../../zardoshti/common/minivector.h"
#include "../../zardoshti/common/orec_t.h"
#include "../../zardoshti/common/pad_word.h"
#include "../../zardoshti/common/platform.h"

/// TL2 is an STM algorithm with the following characteristics:
/// - Uses orecs for commit-time write locking, optimistic read locking
/// - Uses a global clock (counter) to avoid validation
/// - Performs speculative writes out of place (uses redo)
/// - Does *not* use timestamp extension.  This leads to something akin to "ALA"
///   semantics for publication (privatization semantics depend on the choice of
///   quiescence)
///
/// TL2 can be customized in the following ways:
/// - Size of orec table
/// - Redo Log (data structure and granularity of chunks)
/// - EpochManager (quiescence and irrevocability)
/// - Contention manager
/// - Stack Frame (to bring some of the caller frame into tx scope)
/// - Allocator (to become irrevocable on too many allocations)
///
/// By most accounts, TL2 seems like a "degenerate" version of OrecLazy... after
/// all, OrecLazy can do timestamp extension, whereas TL2 cannot.  In some past
/// works, (e.g., Marathe and Moir's 2008 PPoPP nonblocking STM), this
/// difference between timestamp extension and not was a significant factor in
/// scaling for some workloads.
///
/// An essential feature of this TL2 implementation is that we have added an
/// additional customization.  Two separate algorithms (the "Detlefs STM" and
/// Spear's "OrecALA") applied an idea from Adl-tabatabai's eager STM, and moved
/// the global counter increment after writeback.  In lazy STM, this avoids a
/// memory fence in the read function on relaxed architectures.  The boolean
/// SINGLEFENCEOPT parameter turns this feature on.
template <class ORECTABLE, class REDOLOG, class EPOCH, class CM,
          class STACKFRAME, class ALLOCATOR, bool SINGLEFENCEOPT>
class TL2 {
  /// Globals is a wrapper around all of the global variables used by TL2
  struct Globals {
    /// The table of orecs for concurrency control
    ORECTABLE orecs;

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

  /// For managing thread IDs, Quiescence, and Irrevocability
  EPOCH epoch;

  /// Contention manager
  CM cm;

  /// For managing the stack frame
  STACKFRAME frame;

  /// The value of the global clock when this transaction started/validated
  uint64_t start_time = 0;

  /// The lock token used by this thread
  uint64_t my_lock;

  /// all of the orecs this transaction has read
  MiniVector<orec_t *> readset;

  /// all of the orecs this transaction has locked
  MiniVector<orec_t *> lockset;

  /// a redolog, since this is a lazy TM
  REDOLOG redolog;

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
  TL2() : epoch(globals.epoch), cm() {
    my_lock = ORECTABLE::make_lockword(epoch.id);
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

      // Get the start time, and put it into the epoch.  epoch.onBegin will wait
      // until there are no irrevocable transactions.
      start_time = globals.orecs.get_time();
      epoch.onBegin(globals.epoch, start_time);

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
      // fast-path for read-only transactions must still quiesce before freeing
      if (lockset.empty()) {
        epoch.clearEpoch(globals.epoch);
        readset.clear();
        cm.afterCommit(globals.cm);
        epoch.quiesce(globals.epoch, start_time);
        allocator.onCommit();
        deferredActions.onCommit();
        frame.onCommit();
        return;
      }
      // Commit a writer transaction:

      // acquire all locks for the write set
      acquireLocks();

      uint64_t end_time = 0;
      if (!SINGLEFENCEOPT) {
        // get a commit time (includes memory fence)
        end_time = globals.orecs.increment_get();
      }

      // validate if there were any intervening commits (always validate when
      // using SINGLEFENCEOPT)
      if (SINGLEFENCEOPT || (end_time != start_time + 1)) {
        for (auto i : readset) {
          uint64_t v = i->curr;
          if (v > start_time && v != my_lock) {
            abortTx();
          }
        }
      }

      // replay redo log and write back
      redolog.writeback_atomic();

      if (SINGLEFENCEOPT) {
        // get a commit time (includes memory fence)
        end_time = globals.orecs.increment_get();
      }

      // depart epoch table (fence) and then release locks
      // NB: these stores may result in unnecessary fences
      epoch.clearEpoch(globals.epoch);
      releaseLocks(end_time);

      // clear lists.  Quiesce before freeing
      redolog.reset();
      lockset.clear();
      readset.clear();
      cm.afterCommit(globals.cm);
      epoch.quiesce(globals.epoch, end_time);
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

  /// Transactional read:
  template <typename T> T read(T *addr) {
    // No instrumentation if on stack or we're irrevocable
    if (accessDirectly(addr)) {
      return *addr;
    }

    // Lookup in redo log to populate ret.  Note that prior casting can lead to
    // ret having only some bytes properly set
    T ret;
    int found_mask = redolog.find(addr, ret);
    // If we found all the bytes in the redo log, then it's easy
    int desired_mask = (1UL << sizeof(T)) - 1;
    if (desired_mask == found_mask) {
      return ret;
    }

    // get the orec addr, then do a lightweight (but abort-prone) consistent
    // read
    orec_t *o = globals.orecs.get(addr);

    // read the orec, then location, then orec
    local_orec_t pre, post;
    if (!SINGLEFENCEOPT) {
      pre.all = o->curr; // fenced read of o->curr
    }
    T from_mem = REDOLOG::perform_transactional_read(addr);
    post.all = o->curr; // fenced read of o->curr

    if (!SINGLEFENCEOPT) {
      // common case: new read to an unlocked, old location
      if ((pre.all == post.all) && (pre.all <= start_time)) {
        readset.push_back(o);
      } else {
        abortTx();
      }
    } else {
      // common case: new read to an unlocked, old location
      //
      // NB: by virtue of the timing of global clock increments in commit(), we
      //     can read locations that unlocked after we started, if they were
      //     updated by transactions that incremented before we started, so we
      //     don't need pre.
      if (post.all <= start_time) {
        readset.push_back(o);
      } else {
        abortTx();
      }
    }

    // If redolog was a partial hit, reconstruction is needed
    if (!found_mask) {
      return from_mem;
    }
    REDOLOG::reconstruct(from_mem, ret, found_mask);
    return ret;
  }

  /// Transactional write
  template <typename T> void write(T *addr, T val) {
    // No instrumentation if on stack or we're irrevocable
    if (accessDirectly(addr)) {
      *addr = val;
    } else {
      redolog.insert(addr, val);
      // get the orec addr
      orec_t *o = globals.orecs.get(addr);
      lockset.push_back(o);
    }
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

    // now validate.  If it fails, release irrevocability so other transactions
    // can run.
    for (auto o : readset) {
      local_orec_t lo;
      lo.all = o->curr;
      if (lo.all > start_time) {
        epoch.onCommitIrrevoc(globals.epoch);
        abortTx();
      }
    }

    // replay redo log
    redolog.writeback_nonatomic();

    // clear lists
    allocator.onCommit();
    readset.clear();
    redolog.reset();
    lockset.clear();
  }

  /// Register an action to run after transaction commit
  void registerCommitHandler(void (*func)(void *), void *args) {
    deferredActions.registerHandler(func, args);
  }

private:
  /// Abort the transaction.  We must handle mallocs and frees, and we need to
  /// ensure that the TL2 object is in an appropriate state for starting a
  /// new transaction.  Note that we *will* call beginTx again, unlike libITM.
  void abortTx() {
    // We can exit the Epoch right away, so that other threads don't have to
    // wait on this thread.
    epoch.clearEpoch(globals.epoch);
    cm.afterAbort(globals.cm, epoch.id);

    // release any locks held by this thread
    for (auto o : lockset) {
      if (o->curr == my_lock) {
        o->curr.store(o->prev);
      }
    }

    // reset all lists
    readset.clear();
    redolog.reset();
    lockset.clear();
    allocator.onAbort(); // this reclaims all mallocs
    deferredActions.onAbort();
    frame.onAbort();
    longjmp(*checkpoint, 1); // Takes us back to calling beginTx()
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

  /// During commit, the transaction acquires all locks for its write set
  void acquireLocks() {
    for (auto o : lockset) {
      local_orec_t pre;
      pre.all = o->curr;

      // If lock unheld, acquire; abort on fail to acquire
      if (pre.all <= start_time) {
        if (!o->curr.compare_exchange_strong(pre.all, my_lock)) {
          abortTx();
        }
        o->prev = pre.all;
      }
      // If lock is not held by me, abort
      else if (pre.all != my_lock) {
        abortTx();
      }
    }
  }

  /// Release the locks held by this transaction
  void releaseLocks(uint64_t end_time) {
    // NB: there may be unnecessary fences in this loop
    for (auto o : lockset) {
      if (o->curr == my_lock)
        o->curr = end_time;
    }
  }
};