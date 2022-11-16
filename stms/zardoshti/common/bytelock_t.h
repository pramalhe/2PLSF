#pragma once

#include <atomic>

/// BytelockTable is a table of bytelocks, for TLRW-style algorithms.  Unlike
/// the published TLRW, our BytelockTable does not allow an unlimited number of
/// threads (albeit via two classes of readers), but instead enforces a fixed
/// maximum number of threads, based on the number of dedicated slots in each
/// bytelock.
template <int NUM_BYTELOCKS, int COVERAGE, int THREADS> class BytelockTable {
public:
  /// bytelock_t is a simplified version of the TLRW bytelock.  It consists of a
  /// word for storing the ID of the writer (if any) and a byte per visible
  /// reader, which stores a 1 or 0 depending on whether the corresponding
  /// thread is a reader or not.
  ///
  /// In the original TLRW, bytelocks had an overflow counter for supporting
  /// additional threads.  Another option is to have more slots.  In our
  /// implementation, every reader is slotted, and a template instantiation
  /// decides on the maximum number of readers.
  ///
  /// Note that we leave low-level use of the bytelock up to the TM algorithm.
  /// There is no "acquire_for_read" or "release" function in this file.
  struct bytelock_t {
    /// The thread who owns the bytelock
    std::atomic<uintptr_t> owner;

    /// The slots for readers
    std::atomic<char> readers[THREADS];
  };

private:
  /// The bytelock table
  bytelock_t bytelocks[NUM_BYTELOCKS];

public:
  /// Given an address, return a pointer to the corresponding bytelock table
  /// entry
  bytelock_t *get(void *addr) {
    return &bytelocks[(reinterpret_cast<uintptr_t>(addr) >> COVERAGE) %
                      NUM_BYTELOCKS];
  }

  /// Terminate the program if we have more threads than we have available slots
  void validate_id(int id) {
    if (id >= THREADS)
      std::terminate();
  }
};