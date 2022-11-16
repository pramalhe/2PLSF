/* =============================================================================
 *
 * util.h
 *
 * Collection of useful utility routines
 *
 * =============================================================================
 */


#ifndef UTIL_H
#define UTIL_H


#include <assert.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#define DIM(A)                          (sizeof(A)/sizeof((A)[0]))
#define UNS(a)                          ((uintptr_t)(a))
#define ASSERT(x)                       /* assert(x) */
#define CTASSERT(x)                     ({ int a[1-(2*!(x))]; a[0] = 0;})


/*
 * Shorthand for type conversion routines
 */

#define IP2F(v)                         intp2float(v)
#define F2IP(v)                         float2intp(v)

#define IPP2FP(v)                       intpp2floatpp(v)
#define FP2IPP(v)                       floatp2intpp(v)

#define IP2VP(v)                        intp2voidp(v)
#define VP2IP(v)                        voidp2intp(v)


/* =============================================================================
 * intp2float
 * =============================================================================
 */
static __inline__ float
intp2float (intptr_t val)
{
#ifdef __LP64__
    union {
        intptr_t i;
        float    f[2];
    } convert;
    convert.i = val;
    return convert.f[0];
#else
    union {
        intptr_t i;
        float    f;
    } convert;
    convert.i = val;
    return convert.f;
#endif
}


/* =============================================================================
 * float2intp
 * =============================================================================
 */
static __inline__ intptr_t
float2intp (float val)
{
#ifdef __LP64__
    union {
        intptr_t i;
        float    f[2];
    } convert;
    convert.f[0] = val;
    return convert.i;
#else
    union {
        intptr_t i;
        float    f;
    } convert;
    convert.f = val;
    return convert.i;
#endif
}


/* =============================================================================
 * intpp2floatp
 * =============================================================================
 */
static __inline__ float*
intpp2floatp (intptr_t* val)
{
    union {
        intptr_t* i;
        float*    f;
    } convert;
    convert.i = val;
    return convert.f;
}


/* =============================================================================
 * floatp2intpp
 * =============================================================================
 */
static __inline__ intptr_t*
floatp2intpp (float* val)
{
    union {
        intptr_t* i;
        float*    f;
    } convert;
    convert.f = val;
    return convert.i;
}


/* =============================================================================
 * intp2voidp
 * =============================================================================
 */
static __inline__ void*
intp2voidp (intptr_t val)
{
    union {
        intptr_t i;
        void*    v;
    } convert;
    convert.i = val;
    return convert.v;
}


/* =============================================================================
 * voidp2intp
 * =============================================================================
 */
static __inline__ intptr_t
voidp2intp (void* val)
{
    union {
        intptr_t i;
        void*    v;
    } convert;
    convert.v = val;
    return convert.i;
}


/* =============================================================================
 * CompileTimeAsserts
 *
 * Establish critical invariants and fail at compile-time rather than run-time
 * =============================================================================
 */
static __inline__ void
CompileTimeAsserts ()
{
#ifdef __LP64__
    CTASSERT(sizeof(intptr_t) == sizeof(long));
    CTASSERT(sizeof(long) == 8);
#else
    CTASSERT(sizeof(intptr_t) == sizeof(long));
    CTASSERT(sizeof(long) == 4);
#endif

    /*
     * For type conversions
     */
#ifdef __LP64__
    CTASSERT(2*sizeof(float) == sizeof(intptr_t));
#else
    CTASSERT(sizeof(float)   == sizeof(intptr_t));
#endif
    CTASSERT(sizeof(float*)  == sizeof(intptr_t));
    CTASSERT(sizeof(void*)   == sizeof(intptr_t));
}


#ifdef __cplusplus
}
#endif


#endif /* UTIL_H */
