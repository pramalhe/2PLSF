/* =============================================================================
 *
 * platform_x86.h
 *
 * x86-specific bindings
 *
 * =============================================================================
 */


#ifndef PLATFORM_X86_H
#define PLATFORM_X86_H 1


#include <stdint.h>
#include "common.h"


/* =============================================================================
 * Compare-and-swap
 *
 * CCM: Notes for implementing CAS on x86:
 *
 * /usr/include/asm-x86_64/system.h
 * http://www-128.ibm.com/developerworks/linux/library/l-solar/
 * http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html#Atomic-Builtins
 *
 * In C, CAS would be:
 *
 * static __inline__ intptr_t cas(intptr_t newv, intptr_t old, intptr_t* ptr) {
 *     intptr_t prev;
 *     pthread_mutex_lock(&lock);
 *     prev = *ptr;
 *     if (prev == old) {
 *         *ptr = newv;
 *     }
 *     pthread_mutex_unlock(&lock);
 *     return prev;
 * =============================================================================
 */
__INLINE__ intptr_t
cas (intptr_t newVal, intptr_t oldVal, volatile intptr_t* ptr)
{
    intptr_t prevVal;

    __asm__ __volatile__ (
        "lock \n"
#ifdef __LP64__
        "cmpxchgq %1,%2 \n"
#else
        "cmpxchgl %k1,%2 \n"
#endif
        : "=a" (prevVal)
        : "q"(newVal), "m"(*ptr), "0" (oldVal)
        : "memory"
    );

    return prevVal;
}


/* =============================================================================
 * Memory Barriers
 *
 * http://mail.nl.linux.org/kernelnewbies/2002-11/msg00127.html
 * =============================================================================
 */
#ifdef NEVER
#define MEMBARLDLD()                    /* nothing */
#define MEMBARSTST()                    /* nothing */
#define MEMBARSTLD()                    __asm__ __volatile__ ("" : : :"memory")
#else
#define MEMBARLDLD()                    __asm__ __volatile__ ("" : : : "memory")
#define MEMBARSTST()                    __asm__ __volatile__ ("" : : : "memory")
#define MEMBARSTLD()                    __asm__ __volatile__ ("mfence" : : : "memory")
#endif

/* =============================================================================
 * Prefetching
 *
 * We use PREFETCHW in LD...CAS and LD...ST circumstances to force the $line
 * directly into M-state, avoiding RTS->RTO upgrade txns.
 * =============================================================================
 */
#ifndef ARCH_HAS_PREFETCHW
__INLINE__ void
prefetchw (volatile void* x)
{
    /* nothing */
}
#endif


/* =============================================================================
 * Non-faulting load
 * =============================================================================
 */
#define LDNF(a)                         (*(a)) /* CCM: not yet implemented */


/* =============================================================================
 * MP-polite spinning
 *
 * Ideally we would like to drop the priority of our CMT strand.
 * =============================================================================
 */
#define PAUSE()                         /* nothing */


/* =============================================================================
 * Timer functions
 * =============================================================================
 */
/* CCM: low overhead timer; also works with simulator */
#define TL2_TIMER_READ() ({ \
    unsigned int lo; \
    unsigned int hi; \
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi)); \
    ((TL2_TIMER_T)hi) << 32 | lo; \
})

/*
Version with clock_gettime:

#define TL2_TIMER_READ() ({ \

      struct timespec time; \

      clock_gettime(CLOCK_MONOTONIC, &time); \

      (long)time.tv_sec * 1000000000L + (long)time.tv_nsec; \

})
*/

/*
Version with gettimeofday:

#define TL2_TIMER_READ() ({ \

    struct timeval time; \

    gettimeofday(&time, NULL); \

    (long)time.tv_sec * 1000000L + (long)time.tv_usec; \

})
*/


#endif /* PLATFORM_X86_H */


/* =============================================================================
 *
 * End of platform_x86.h
 *
 * =============================================================================
 */
