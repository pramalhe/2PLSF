
#pragma once

#include <atomic>
#include <x86intrin.h>

#include "../../zardoshti/common/orec_t.h"

/// CounterTimesource uses a monotonically increasing shared memory counter as
/// the timesource
class CounterTimesource {
  /// The value of the global clock when this transaction started/validated
  pad_dword_t timestamp;

public:
  typedef uint64_t time_snapshot_t;

  /// Runs the orec_t.h implemntation to get the current time
  uint64_t get_time() { return timestamp.val; }

  /// get_time_strong_ordering is the same for CounterTimesource
  uint64_t get_time_strong_ordering() { return timestamp.val; }

  /// Runs the orec_t.h implementation of increment_get()
  uint64_t increment_get() { return 1 + timestamp.val.fetch_add(1); }

  /// Increment the clock, and ignore the new value.  This is useful when doing
  /// abort-time bumping in undo-based STM.
  void increment() { timestamp.val++; }
};

/// RdtscpTimesource uses the hardware clock cycle counter with rdtscp as the
/// timesource
class RdtscpTimesource {

public:
  typedef std::atomic<uint64_t> time_snapshot_t;

  /// Use rdtscp to get the hardware clock cycle count
  uint64_t get_time() {
    unsigned int dummy;
    return __rdtscp(&dummy);
  }

  /// Use rdtscp to get the hardware clock cycle count, but enforces strong
  /// ordering with an atomic add
  uint64_t get_time_strong_ordering() { return get_time(); }

  /// Not needed for RdtscpTimesource. Instead we do the same thing as
  /// get_time()
  uint64_t increment_get() { return get_time_strong_ordering(); }

  /// No-op for RdtscpTimesource
  void increment() {}
};