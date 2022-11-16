#pragma once

#include <atomic>

#include "../../zardoshti/common/pad_word.h"
#include "../../zardoshti/common/timesource.h"

/// local_orec_t is a way of looking at a 64-bit int and telling if it indicates
/// a lock or a version number.  Set 'all' by copying from an atomic<uintptr_t>
///
/// Warning: this is only good for little endian (e.g., x86)
union local_orec_t {
  struct {
    uintptr_t id : (8 * sizeof(uintptr_t)) - 1;
    uintptr_t lock : 1;
  } fields;
  uintptr_t all;
};

/// The ownership record type is used by orec-based STMs to protect regions of
/// memory.
struct orec_t {
  /// curr is either a lock word, or a version number
  std::atomic<uintptr_t> curr;

  /// When a thread acquires the orec, it can store the previous version number
  /// in prev, so that it can use it to clean up in the case of aborts.
  uintptr_t prev;
};

/// OrecTable is a table of orecs, and a timestamp.  It is useful in TM
/// algorithms that use a global "clock" to determine the non-locked values of
/// orecs.
template <int NUM_ORECS, int COVERAGE, class TIMESOURCE> class OrecTable {
  /// The orec table
  orec_t orecs[NUM_ORECS];

public:
  typedef typename TIMESOURCE::time_snapshot_t time_snapshot_t;

  /// The global timestamp, for assigning commit orders and reducing
  /// validation
  TIMESOURCE timestamp;

  /// Map addresses to orec table entries
  orec_t *get(void *addr) {
    return &orecs[(reinterpret_cast<uintptr_t>(addr) >> COVERAGE) % NUM_ORECS];
  }

  /// Get the current value of the clock
  uintptr_t get_time() { return timestamp.get_time(); }

  /// Get the current value of the clock.  This version implies some stronger
  /// fencing behavior than the regular get_time.
  uintptr_t get_time_strong_ordering() {
    return timestamp.get_time_strong_ordering();
  }

  /// Increment the clock, and return the value it was incremented *to*
  uint64_t increment_get() { return timestamp.increment_get(); }

  /// Increment the clock, and ignore the new value.  This is useful when doing
  /// abort-time bumping in undo-based STM.
  void increment() { timestamp.increment(); }

  /// Create a locked orec from an id, so that a thread knows what value to use
  /// as its lock word
  static uintptr_t make_lockword(int id) {
    local_orec_t tmp;
    tmp.all = id;
    tmp.fields.lock = 1;
    return tmp.all;
  }
};