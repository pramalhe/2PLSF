/// epochs.h provides a set of EpochManagers, which handles the assignment of
/// unique IDs to threads, and also allow a TM to customize the way it handles
/// two related tasks:
///
/// - The management of quiescence
/// - The management of irrevocability
///
/// The foundation of the EpochManager is the Epoch Table (ET).  The ET has an
/// atomic integer per thread.  Threads set their integers to the time at which
/// they begin a transaction, and then clear (to -1) when they end a
/// transaction.  The integers make it possible to observe both (a) if any
/// thread is in a transaction, and (b) if any thread is executing with a stale
/// view of memory.  These properties allow threads to know when certain actions
/// (like freeing memory) can proceed.

#pragma once

#include <cstddef>

#include "../../zardoshti/common/pad_word.h"

/// BasicEpochManager is an Epoch manager that can assign unique IDs, but which
/// does not support quiescence or irrevocability.  Since we use it as the base
/// for all other EpochManagers, we place the Epoch table in it, even though it
/// does not use the Epoch table.
template <int MAXTHREADS> class BasicEpochManager {
public:
  /// Globals encapsulates the shared data that threads' EpochManagers will use
  /// to coordinate.
  class Globals {
  public:
    /// A monotonically increasing counter for assigning thread IDs
    pad_dword_t idGenerator;

    /// The epoch table for tracking if threads are in a transaction or not.
    pad_word_t epochs[MAXTHREADS];

    /// Construct a BasicEpochManager::Globals by zeroing the generator and
    /// initializing the epoch table to all -1s
    Globals() {
      idGenerator.val = 0;
      for (int i = 0; i < MAXTHREADS; ++i) {
        epochs[i].val = -1;
      }
    }

    /// Return the total number of threads in the system
    uintptr_t getThreads() { return idGenerator.val; }
  };

  /// The unique Id of the thread to which this EpochManager belongs
  size_t id;

  /// Construct a thread's instance of the BasicEpochManager by giving the
  /// thread a unique id.
  BasicEpochManager(Globals &g) : id(g.idGenerator.val++) {
    if (id >= MAXTHREADS) {
      std::terminate();
    }
  }

  /// Return whether the thread is irrevocable or not
  bool isIrrevoc() { return false; }

  /// Clearing a thread's value in the epoch table is a no-op
  void clearEpoch(Globals &) {}

  /// Setting a thread's value in the epoch table is a no-op
  void setEpoch(Globals &, uintptr_t) {}

  /// When a thread starts, we need not take any action, because there is no
  /// irrevocability or quiescence
  void onBegin(Globals &, uintptr_t) {}

  /// A thread should never commit as irrevocable with this EpochManager
  void onCommitIrrevoc(Globals &) { std::terminate(); }

  /// Quiescing is a no-op for this EpochManager
  void quiesce(Globals &, uintptr_t) {}

  /// Returns false, since there are never irrevocable threads
  bool existIrrevoc(Globals &) { return false; }

  /// Attempts to become irrevocable will always fail.  It is up to the TM to
  /// decide how to respond.
  bool tryIrrevoc(Globals &) { return false; }
};

/// QuiesceEpochManager is an Epoch manager that supports quiescence, but not
/// irrevocability.
template <int MAXTHREADS>
class QuiesceEpochManager : public BasicEpochManager<MAXTHREADS> {
public:
  /// QuiesceEpochManager::Globals does not extend BasicEpochManager::Globals,
  /// but to simplify the use of this class as a template parameter to TM
  /// algorithms, we need to define a Globals class anyway.
  class Globals : public BasicEpochManager<MAXTHREADS>::Globals {};

  /// Construct a thread's instance of the QuiesceEpochManager by giving the
  /// thread a unique id.
  QuiesceEpochManager(Globals &g) : BasicEpochManager<MAXTHREADS>(g) {}

  /// Clear a thread's value in the epoch table
  void clearEpoch(Globals &g) { g.epochs[this->id].val = -1LL; }

  /// Set a thread's value in the epoch table
  void setEpoch(Globals &g, uintptr_t time) { g.epochs[this->id].val = time; }

  /// When a thread starts, we don't need to wait on irrevocable transactions,
  /// but we do need to update the epoch table so that the thread is visible to
  /// other threads when they call quiesce()
  void onBegin(Globals &g, uintptr_t time) { g.epochs[this->id].val = time; }

  /// Wait for all threads to update their entries in the epoch table to be
  /// greater than the provided time.  Note that the table holds unsigned
  /// integers, so -1 is big.
  void quiesce(Globals &g, uintptr_t time) {
    uintptr_t count = g.idGenerator.val;
    for (uintptr_t i = 0; i < count; ++i) {
      if (i != this->id) { // don't wait on self :)
        while (g.epochs[i].val < time)
          ;
      }
    }
  }
};

/// IrrevocEpochManager is an Epoch manager that supports irrevocability, but
/// not quiescence.
template <int MAXTHREADS>
class IrrevocEpochManager : public BasicEpochManager<MAXTHREADS> {
public:
  /// IrrevocEpochManager adds an irrevocability token to the
  /// BasicEpochManager's Globals
  class Globals : public BasicEpochManager<MAXTHREADS>::Globals {
  public:
    /// A token that can be assigned to one thread at a time
    pad_dword_t token;

    /// Construct an IrrevocEpochManager::Globals by zeroing the token and
    /// forwarding to the base constructor
    Globals() : BasicEpochManager<MAXTHREADS>::Globals() { token.val = 0; }
  };

private:
  /// hasToken tracks if the current thread owns Globals::token
  bool hasToken;

public:
  /// Construct a thread's instance of the BasicEpochManager by clearing its
  /// hasToken field and forwarding to the base constructor.
  IrrevocEpochManager(Globals &g)
      : BasicEpochManager<MAXTHREADS>(g), hasToken(false) {}

  /// Return whether the thread is irrevocable or not
  bool isIrrevoc() { return hasToken; }

  /// Clear a thread's value in the epoch table
  void clearEpoch(Globals &g) { g.epochs[this->id].val = -1LL; }

  /// Set a thread's value in the epoch table
  void setEpoch(Globals &g, uintptr_t time) { g.epochs[this->id].val = time; }

  /// When a thread starts, block it until there are no irrevocable
  /// transactions, and also update the epoch table
  void onBegin(Globals &g, uintptr_t time) {
    while (true) {
      setEpoch(g, time);
      if (!g.token.val) {
        break;
      }
      clearEpoch(g);
      while (g.token.val)
        ;
    }
  }

  /// When a thread commits as irrevocable, reset it in the epoch table and
  /// release the token
  ///
  /// NB: this can also be used if a successful tryIrrevoc() is followed by an
  ///     unsuccessful validation during a thread's attempt to transition to
  ///     irrevocable.
  void onCommitIrrevoc(Globals &g) {
    clearEpoch(g);
    g.token.val = 0;
    hasToken = false;
  }

  /// Check if there exists an irrevocable thread
  bool existIrrevoc(Globals &g) { return g.token.val; }

  /// Try to become irrevocable, but possibly fail
  bool tryIrrevoc(Globals &g) {
    // If we are already irrevocable, succeed immediately
    if (hasToken) {
      return true;
    }
    // Attempt to get the token
    uint64_t oldval = 0;
    if (g.token.val || !g.token.val.compare_exchange_strong(oldval, 1)) {
      return false;
    }
    // Wait on all threads to exit transactions
    uintptr_t count = g.idGenerator.val;
    for (uintptr_t i = 0; i < count; ++i) {
      if (i != this->id) { // don't wait on self :)
        while (g.epochs[i].val != (uintptr_t)-1)
          ;
      }
    }
    // mark self irrevocable
    hasToken = true;
    return true;
  }
};

/// IrrevocQuiesceEpochManager is an Epoch manager that supports both
/// irrevocability and quiescence
template <int MAXTHREADS>
class IrrevocQuiesceEpochManager : public IrrevocEpochManager<MAXTHREADS> {
public:
  /// IrrevocQuiesceEpochManager::Globals does not extend
  /// IrrevocEpochManager::Globals, but to simplify the use of this class as a
  /// template parameter to TM algorithms, we need to define a Globals class
  /// anyway.
  class Globals : public IrrevocEpochManager<MAXTHREADS>::Globals {};

  /// Construct a thread's instance of the IrrevocQuiesceEpochManager by
  /// forwarding to the base constructor
  IrrevocQuiesceEpochManager(Globals &g) : IrrevocEpochManager<MAXTHREADS>(g) {}

  /// Wait for all threads to update their entries in the epoch table to be
  /// greater than the provided time.  Note that the table holds unsigned
  /// integers, so -1 is big.
  void quiesce(Globals &g, uintptr_t time) {
    uintptr_t count = g.idGenerator.val;
    for (uintptr_t i = 0; i < count; ++i) {
      if (i != this->id) { // don't wait on self :)
        while (g.epochs[i].val < time)
          ;
      }
    }
  }
};