/// platform.h provides some platform details and platform-specific functions,
/// so that we can avoid inline assembly or platform-specific code in our
/// implementations

#pragma once

#include <ctime>
#include <pthread.h>

/// A constant to help us with padding things to a cache line.
const int CACHELINE_BYTES = 64;

/// Yield the CPU
inline void yield_cpu() { pthread_yield(); }

/// The cheapest Linux clock with good enough resolution to manage backoff
inline uint64_t getElapsedTime() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return (((long long)t.tv_sec) * 1000000000L) + ((long long)t.tv_nsec);
}

/// Spin briefly
void spin64() {
  for (int i = 0; i < 64; ++i)
    __asm__ volatile("nop");
}

/// Spin for a user-specified number of instructions
void spinX(int x) {
  for (int i = 0; i < x; ++i)
    __asm__ volatile("nop");
}

/// use rdtscp for high-precision tick counter with pipeline stall
inline uint64_t tickp() {
  uint32_t tmp[2];
  asm volatile("rdtscp" : "=a"(tmp[1]), "=d"(tmp[0]) : "c"(0x10) : "memory");
  return (((uint64_t)tmp[0]) << 32) | tmp[1];
}

/// Produce a random number using a simple PRNG
inline int rand_r_32(unsigned int *seed) {
  unsigned int next = *seed;
  int result;

  next *= 1103515245;
  next += 12345;
  result = (unsigned int)(next / 65536) % 2048;

  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (unsigned int)(next / 65536) % 1024;

  next *= 1103515245;
  next += 12345;
  result <<= 10;
  result ^= (unsigned int)(next / 65536) % 1024;

  *seed = next;
  return result;
}

/// Perform randomized exponential backoff.  We wait for a random number of CPU
/// ticks, so that preemption during backoff is handled cleanly
void exp_backoff(uint32_t consec_aborts, uint32_t &seed, uint32_t MIN,
                 uint32_t MAX) {
  // how many bits should we use to pick an amount of time to wait?
  uint32_t bits = consec_aborts + MIN - 1;
  bits = (bits > MAX) ? MAX : bits;
  // get a random amount of time to wait, bounded by an exponentially
  // increasing limit
  int32_t delay = rand_r_32(&seed);
  delay &= ((1 << bits) - 1);
  // wait until at least that many ns have passed
  // unsigned long long start = getElapsedTime();
  unsigned long long start = tickp();
  unsigned long long stop_at = start + delay;
  while (tickp() < stop_at) {
    spin64();
  }
}