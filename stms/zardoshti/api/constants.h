/// constants.h provides some common values to use when instantiating TM
/// algorithm templates.  An instantiation of a TM algorithm is free to ignore
/// these constants and choose others.  These exist primarily to ensure that we
/// have reasonable defaults that are shared across similar instantiations,
/// without hard-coding constants into the instantiations themselves.

#pragma once

#include <cstdint>

/// log_2 of the number of bytes protected by an orec
const int OREC_COVERAGE = 4;

/// Quiescence benefits from a limit on the number of threads.  4096 is safe
const int MAX_THREADS = 4096;

/// The number of orecs in the system
const uint32_t NUM_STRIPES = 1048576;

/// A low threshold for tuning backoff
const uint32_t BACKOFF_MIN = 4;

/// A high threshold for tuning backoff
const uint32_t BACKOFF_MAX = 16;

/// A threshold for the number of consecutive aborts before a transaction should
/// become irrevocable.
const uint32_t ABORTS_THRESHOLD = 100;

/// A threshold for the number of mallocs in a transaction before it should
/// become irrevocable
const uint32_t MALLOC_THRESHOLD = 128;

/// Our default bytelock implementation constrains to one cache line, with an
/// 8-byte owner field, leaving 56 slots for readers
const int BYTELOCK_MAX_THREADS = 56;

/// Below are four tuning parameters for TLRW, which seem to provide
/// satisfactory performance in the common case.  The second best configuration
/// for most of the applications we studied was (8, 2048, 2, 512)

/// TLRW deadlock avoidance: times to try reading
const int32_t TLRW_READ_TRIES = 2;

/// TLRW deadlock avoidance: spin between read attempts
const int32_t TLRW_READ_SPINS = 512;

/// TLRW deadlock avoidance: times to try writing
const int32_t TLRW_WRITE_TRIES = 8;

/// TLRW deadlock avoidance: spin between write attempts
const int32_t TLRW_WRITE_SPINS = 2048;

/// Number of times an HTM transaction aborts before switching to serial
const int32_t NUM_HTM_RETRIES = 8;

/// RingSTM Ring Size: number of filters in the ring
const int32_t RING_SIZE = 1024;

/// RingSTM: Number of bits in a filter
const int32_t RING_FILTER_SIZE = 1024;

/// RingSTM: Granularity of regions that map to ring bits
const int32_t RING_COVERAGE = 5;

/// Reduced Hardware NOrec: how many postfix fails before fallback to STM
const int32_t NUM_POSTFIX_RETRIES = 8;