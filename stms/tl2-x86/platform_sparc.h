/* =============================================================================
 *
 * platform_sparc.h
 *
 * SPARC-specific bindings
 *
 * =============================================================================
 */


#ifndef PLATFORM_SPARC_H
#define PLATFORM_SPARC_H 1


#ifndef PLATFORM_H
#  error include "platform.h" for "platform_sparc.h"
#endif


#include <stdint.h>
#include "common.h"


/* =============================================================================
 * Compare-and-swap
 *
 * =============================================================================
 */
__INLINE__ intptr_t
cas (intptr_t newVal, intptr_t oldVal, intptr_t* ptr)
{
    intptr_t prevVal;

    __asm__ __volatile__(
#ifdef __LP64__
        "casx [%2],%3,%1"
#else
        "cas [%2],%3,%1"
#endif
        : "=r"(prevVal)
        : "0"(newVal), "r"(ptr), "r"(oldVal)
        : "memory"
    );

    return prevVal;
}


/* =============================================================================
 * Memory Barriers
 * =============================================================================
 */
#define MEMBARLDLD()   /* nothing */
#define MEMBARSTST()   /* nothing */
#define MEMBARSTLD()   __asm__ __volatile__ ("membar #StoreLoad" : : :"memory")


/* =============================================================================
 * Prefetching
 *
 * We use PREFETCHW in LD...CAS and LD...ST circumstances to force the $line
 * directly into M-state, avoiding RTS->RTO upgrade txns.
 * =============================================================================
 */
__INLINE__ void
prefetchw (volatile void* x)
{
    __asm__ __volatile__ ("prefetch %0,2" :: "m" (x));
}


/* =============================================================================
 * Non-faulting load
 * =============================================================================
 */
__INLINE__ intptr_t
LDNF (volatile intptr_t* a)
{
    intptr_t x;

    __asm__ __volatile__ (
#ifdef __LP64__
        "ldxa [%1]0x82, %0"
#else
        "ldswa [%1]0x82, %0" /* 0x82 = #ASI_PNF = Addr Space Primary Non-Fault */
#endif
        : "=&r"(x)
        : "r"(a)
        : "memory"
    );

    return x;
}


/* =============================================================================
 * MP-polite spinning
 * -- Ideally we would like to drop the priority of our CMT strand.
 * =============================================================================
 */
#define PAUSE()  /* nothing */


/* =============================================================================
 * Timer functions
 * =============================================================================
 */
#define TL2_TIMER_READ() gethrtime()


#endif /* PLATFORM_SPARC_H */


/* =============================================================================
 *
 * End of platform_sparc.h
 *
 * =============================================================================
 */
