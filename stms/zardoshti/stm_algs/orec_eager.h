#pragma once

#include <atomic>
#include <setjmp.h>

#include "../../zardoshti/common/deferred.h"
#include "../../zardoshti/common/minivector.h"
#include "../../zardoshti/common/orec_t.h"
#include "../../zardoshti/common/pad_word.h"
#include "../../zardoshti/common/platform.h"

/// OrecEager is an STM algorithm with the following characteristics:
/// - Uses orecs for encounter-time write locking, optimistic read locking
/// - Uses a global clock (counter) to avoid validation
/// - Performs speculative writes in-place (uses undo)
///
/// OrecEager can be customized in the following ways:
/// - Size of orec table
/// - EpochManager (quiescence and irrevocability)
/// - Contention manager
/// - Stack Frame (to bring some of the caller frame into tx scope)
/// - Allocator (to become irrevocable on too many allocations)
///
/// By using Irrevocability, Quiescence, and a CM with Exponential backoff +
/// Irrevocability, it is possible to generate an STM that is equivalent to the
/// "ml_wt" algorithm in GCC, except for the lack of read-for-write
/// optimizations.
template <class ORECTABLE, class UNDOLOG, class EPOCH, class CM,
          class STACKFRAME, class ALLOCATOR>
class OrecEager {
  /// Globals is a wrapper around all of the global variables used by OrecEager
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

  /// For managing thread Ids, Quiescence, and Irrevocability
  EPOCH epoch;

  /// Contention manager
  CM cm;

  /// For managing the stack frame
  STACKFRAME frame;

  /// The value of the global clock when this transaction started/validated
  uint64_t start_time;

  /// The lock token used by this thread
  uint64_t my_lock;

  /// all of the orecs this transaction has read
  MiniVector<orec_t *> readset;

  /// all of the orecs this transaction has locked
  MiniVector<orec_t *> lockset;

  /// original values that this transaction overwrote
  UNDOLOG undolog;

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

  /// construct a thread's transaction context
  OrecEager() : epoch(globals.epoch), cm() {
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

      // get a commit time (includes memory fence)
      uint64_t end_time = globals.orecs.increment_get();

      // validate if there were any intervening commits
      // NB: on relaxed architectures, there will be unnecessary fences
      if (end_time != start_time + 1) {
        for (auto o : readset) {
          uint64_t v = o->curr;
          if (v > start_time && v != my_lock) {
            abortTx();
          }
        }
      }

      // depart epoch table (fence) and then release locks
      // NB: these stores may result in unnecessary fences
      epoch.clearEpoch(globals.epoch);
      for (auto o : lockset) {
        o->curr = end_time;
      }

      // clear lists.  Quiesce before freeing
      undolog.clear();
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

  /// Transactional read
  template <typename T> T read(T *addr) {
    // No instrumentation if on stack or we're irrevocable
    if (accessDirectly(addr)) {
      return *addr;
    }

    // get the orec addr, then start loop to read a consistent value
    orec_t *o = globals.orecs.get(addr);
    while (true) {
      // read the orec, then location, then orec
      local_orec_t pre, post;
      pre.all = o->curr; // fenced read of o->curr
      T from_mem = UNDOLOG::perform_transactional_read(addr);
      if (pre.all == my_lock)
        return from_mem;  // if caller has lock, we're done
      post.all = o->curr; // fenced read of o->cirr

      // common case: new read to an unlocked, old location
      if ((pre.all == post.all) && (pre.all <= start_time)) {
        readset.push_back(o);
        return from_mem;
      }

      // abort if locked
      if (post.fields.lock) {
        abortTx();
      }

      // validate and then update start time, because orec is unlocked but too
      // new, then try again
      uintptr_t newts = globals.orecs.get_time();
      epoch.setEpoch(globals.epoch, newts);
      validate();
      start_time = newts;
    }
  }

  /// Transactional write
  template <typename T> void write(T *addr, T val) {
    // No instrumentation if on stack or we're irrevocable
    if (accessDirectly(addr)) {
      *addr = val;
      return;
    }

    // get the orec addr, then start loop to ensure a consistent value
    orec_t *o = globals.orecs.get(addr);
    while (true) {
      // read the orec BEFORE we do anything else
      local_orec_t pre;
      pre.all = o->curr;
      // If lock unheld and not too new, acquire; abort on fail to acquire
      if (pre.all <= start_time) {
        if (!o->curr.compare_exchange_strong(pre.all, my_lock)) {
          abortTx();
        }
        lockset.push_back(o);
        o->prev = pre.all; // for easy undo on abort... Cf. incarnation numbers
      }

      // If lock held by me, all good
      else if (pre.all == my_lock) {
      }

      // If lock held by other, abort
      else if (pre.fields.lock) {
        abortTx();
      }

      // Lock unheld, but too new... validate and then go to top
      else {
        uintptr_t newts = globals.orecs.get_time();
        epoch.setEpoch(globals.epoch, newts);
        validate();
        start_time = newts;
        continue;
      }

      // We have the lock.  log old value in case of hash conflict, then write
      typename UNDOLOG::undo_t u;
      u.initFromAddr(addr);
      undolog.push_back(u);
      UNDOLOG::perform_transactional_write(addr, val);
      return;
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
      if (lo.all > start_time && lo.all != my_lock) {
        epoch.onCommitIrrevoc(globals.epoch);
        // NB: this specific abort *could* use nonatomic undo logging, but we'll
        //     just use the existing undo logging
        abortTx();
      }
    }

    // release locks... need a commit time
    uint64_t end_time = globals.orecs.increment_get();
    for (auto o : lockset) {
      o->curr = end_time;
    }
    // clear lists
    allocator.onCommit();
    readset.clear();
    undolog.clear();
    lockset.clear();
  }

  /// Register an action to run after transaction commit
  void registerCommitHandler(void (*func)(void *), void *args) {
    deferredActions.registerHandler(func, args);
  }

private:
  /// Validation.  We need to make sure that all orecs that we've read
  /// have timestamps older than our start time, unless we locked those orecs.
  /// If we locked the orec, we did so when the time was smaller than our start
  /// time, so we're sure to be OK.
  void validate() {
    // NB: on relaxed architectures, we may have unnecessary fences here
    for (auto o : readset) {
      local_orec_t lo;
      lo.all = o->curr;
      if (lo.all > start_time && lo.all != my_lock) {
        abortTx();
      }
    }
  }

  /// Abort the transaction. We must handle mallocs and frees, and we need to
  /// ensure that the OrecEager object is in an appropriate state for starting a
  /// new transaction.  Note that we *will* call beginTx again, unlike libITM.
  void abortTx() {
    // undo any writes
    undolog.undo_writes_atomic();

    // At this point, we can exit the epoch so that other threads don't have to
    // wait on this thread
    epoch.clearEpoch(globals.epoch);
    cm.afterAbort(globals.cm, epoch.id);

    // release the locks and bump version numbers by one... track the highest
    // version number we write, in case it is greater than timestamp.val
    uintptr_t max = 0;
    for (auto o : lockset) {
      uint64_t val = o->prev + 1;
      o->curr = val;
      max = (val > max) ? val : max;
    }
    // if we bumped a version number to higher than the timestamp, we need to
    // increment the timestamp to preserve the invariant that the timestamp
    // val is >= all unocked orecs' values
    uintptr_t ts = globals.orecs.get_time();
    if (max > ts)
      globals.orecs.increment();

    // reset all lists
    readset.clear();
    undolog.clear();
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
};