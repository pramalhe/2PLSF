/// cm.h provides a set of contention managers that can be used by a TM
/// implementation.  These contention managers all provide the same public
/// interface, so that they are interchangeable in TM algorithms.

#pragma once

#include "../../zardoshti/common/pad_word.h"
#include "../../zardoshti/common/platform.h"

/// NoopCM is a contention manager that does no contention management.
class NoopCM {
public:
  /// NoopCM::Globals is empty, but to simplify the use of this class as a
  /// template parameter to TM algorithms, we need to define a Globals class
  /// anyway.
  class Globals {};

  /// Construct a no-op contention manager
  NoopCM() {}

  /// CM code to run before beginning a transaction
  /// @returns true if the transaction should become irrevocable
  bool beforeBegin(Globals &) { return false; }

  /// CM code to run after a transaction finishes cleaning up from an abort
  void afterAbort(Globals &, uint64_t) {}

  /// CM code to run after a transaction finishes cleaning up from a commit
  void afterCommit(Globals &) {}
};

/// HourglassCM is a contention manager that imprecisely restricts transaction
/// start in order to help distressed transactions to make progress.
///
/// The ABORTTHRESHOLD template parameter determines how many consecutive
/// aborts a thread incurs before acquiring the hourglass.
template <int ABORTTHRESHOLD> class HourglassCM {
public:
  /// Hourglass depends on the ability to globally track the ID of the thread
  /// holding the hourglass
  class Globals {
  public:
    /// The owner of the hourglass
    ///
    /// Note that we use -1 to indicate unowned, so that zero-based thread ids
    /// will work
    pad_dword_t owner;

    /// Construct an HourglassCM::Globals by setting the owner to -1 (no owner)
    Globals() { owner.val = -1; }
  };

private:
  /// The number of consecutive aborts by the current thread
  int consecAborts;

  /// A flag for tracking if this thread has the hourglass
  bool inHourglass;

public:
  /// Construct an hourglass contention manager
  HourglassCM() : consecAborts(0), inHourglass(0) {}

  /// CM code to run before beginning a transaction
  /// @returns true if the transaction should become irrevocable
  bool beforeBegin(Globals &g) {
    // Common case: either nobody has the hourglass, or this thread does
    if (g.owner.val == (uintptr_t)-1 || inHourglass) {
      return false;
    }
    // Another thread has the hourglass, so this thread must wait
    while (g.owner.val != (uintptr_t)-1) {
      ;
    }
    return false;
  }

  /// CM code to run after a transaction finishes cleaning up from an abort
  void afterAbort(Globals &g, uint64_t id) {
    // If this thread has aborted too many times, it should try to get the
    // hourglass unless it has it already
    if (++consecAborts > ABORTTHRESHOLD && !inHourglass) {
      uint64_t oldval = -1;
      if (g.owner.val.compare_exchange_strong(oldval, id)) {
        inHourglass = true;
      }
    }
  }

  /// CM code to run after a transaction finishes cleaning up from a commit
  void afterCommit(Globals &g) {
    consecAborts = 0;
    // If the thread has the hourglass, it should release it
    if (inHourglass) {
      inHourglass = false;
      g.owner.val = -1;
    }
  }
};

/// ExpBackoffCM is a contention manager that does randomized exponential
/// backoff on abort
///
/// The backoff threshold can be tuned via MIN and MIX, which are the logarithms
/// of the shortest and longest backoff times.  Backoff times are in # cycles,
/// via the CPU tick counter.
template <int MIN, int MAX> class ExpBackoffCM {
public:
  /// ExpBackoffCM::Globals is empty, but to simplify the use of this class as a
  /// template parameter to TM algorithms, we need to define a Globals class
  /// anyway.
  class Globals {};

private:
  /// The number of consecutive aborts by the current thread
  int consecAborts;

  /// A seed to use for random number generation
  unsigned seed;

public:
  /// Construct an exponential backoff contention manager
  ExpBackoffCM() : consecAborts(0), seed((uintptr_t)(&consecAborts)) {}

  /// CM code to run before beginning a transaction
  /// @returns true if the transaction should become irrevocable
  bool beforeBegin(Globals &) { return false; }

  /// CM code to run after a transaction finishes cleaning up from an abort
  void afterAbort(Globals &, uint64_t) {
    exp_backoff(++consecAborts, seed, MIN, MAX);
  }

  /// CM code to run after a transaction finishes cleaning up from a commit
  void afterCommit(Globals &) { consecAborts = 0; }
};

/// IrrevocCM is a contention manager that instructs threads to use
/// irrevocability to ensure progress if there have been more than ABORTTHRESH
/// consecutive aborts.
template <int ABORTTHRESH> class IrrevocCM {
public:
  /// IrrevocCM::Globals is empty, but to simplify the use of this class as a
  /// template parameter to TM algorithms, we need to define a Globals class
  /// anyway.
  class Globals {};

private:
  /// The number of consecutive aborts by the current thread
  int consecAborts = 0;

public:
  /// Construct an irrevocability contention manager
  IrrevocCM() {}

  /// CM code to run before beginning a transaction
  /// @returns true if the transaction should become irrevocable
  bool beforeBegin(Globals &) { return consecAborts > ABORTTHRESH; }

  /// CM code to run after a transaction finishes cleaning up from an abort
  void afterAbort(Globals &, uint64_t) { ++consecAborts; }

  /// CM code to run after a transaction finishes cleaning up from a commit
  void afterCommit(Globals &) { consecAborts = 0; }
};

/// HourglassBackoffCM is a contention manager that combines Exponential Backoff
/// with Hourglass.
///
/// The ABORTTHRESHOLD template parameter determines how many consecutive
/// aborts a thread incurs before acquiring the hourglass.
///
/// The backoff threshold can be tuned via MIN and MIX, which are the logarithms
/// of the shortest and longest backoff times.  Backoff times are in # cycles,
/// via the CPU tick counter.
template <int ABORTTHRESHOLD, int MIN, int MAX> class HourglassBackoffCM {
public:
  /// Hourglass depends on the ability to globally track the ID of the thread
  /// holding the hourglass
  class Globals {
  public:
    /// The owner of the hourglass
    ///
    /// Note that we use -1 to indicate unowned, so that zero-based thread ids
    /// will work
    pad_dword_t owner;

    /// Construct an HourglassCM::Globals by setting the owner to -1 (no owner)
    Globals() { owner.val = -1; }
  };

private:
  /// The number of consecutive aborts by the current thread
  int consecAborts;

  /// A flag for tracking if this thread has the hourglass
  bool inHourglass;

  /// A seed to use for random number generation
  unsigned seed;

public:
  /// Construct an hourglass contention manager
  HourglassBackoffCM()
      : consecAborts(0), inHourglass(0), seed((uintptr_t)(&consecAborts)) {}

  /// CM code to run before beginning a transaction
  /// @returns true if the transaction should become irrevocable
  bool beforeBegin(Globals &g) {
    // Common case: either nobody has the hourglass, or this thread does
    if (g.owner.val == (uintptr_t)-1 || inHourglass) {
      return false;
    }
    // Another thread has the hourglass, so this thread must wait
    while (g.owner.val != (uintptr_t)-1) {
      ;
    }
    return false;
  }

  /// CM code to run after a transaction finishes cleaning up from an abort
  void afterAbort(Globals &g, uint64_t id) {
    // If this thread has aborted too many times, it should try to get the
    // hourglass unless it has it already
    if (++consecAborts > ABORTTHRESHOLD && !inHourglass) {
      uint64_t oldval = -1;
      if (g.owner.val.compare_exchange_strong(oldval, id)) {
        inHourglass = true;
      }
    }
    // If it has the hourglass, just try again
    else if (inHourglass) {
    }
    // Otherwise, exponential backoff
    else {
      exp_backoff(++consecAborts, seed, MIN, MAX);
    }
  }

  /// CM code to run after a transaction finishes cleaning up from a commit
  void afterCommit(Globals &g) {
    consecAborts = 0;
    // If the thread has the hourglass, it should release it
    if (inHourglass) {
      inHourglass = false;
      g.owner.val = -1;
    }
  }
};