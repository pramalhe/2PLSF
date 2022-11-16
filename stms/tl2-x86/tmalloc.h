/* =============================================================================
 *
 * tmalloc.h
 *
 * Memory allocator with extra metadata for TM.
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 */


#ifndef TMALLOC_H
#define TMALLOC_H 1


#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct tmalloc {
    long size;
    long capacity;
    void** elements;
} tmalloc_t;


/* =============================================================================
 * tmalloc_reserve
 * =============================================================================
 */
void*
tmalloc_reserve (size_t size);


/* =============================================================================
 * tmalloc_release
 * =============================================================================
 */
void
tmalloc_release (void* dataPtr);


/* =============================================================================
 * tmalloc_alloc
 * -- Returns NULL if failed
 * =============================================================================
 */
tmalloc_t*
tmalloc_alloc (long initCapacity);


/* =============================================================================
 * tmalloc_free
 * =============================================================================
 */
void
tmalloc_free (tmalloc_t* tmallocPtr)

;
/* =============================================================================
 * tmalloc_append
 *
 * Returns 0 if fail, else 1
 * =============================================================================
 */
long
tmalloc_append (tmalloc_t* tmallocPtr, void* dataPtr);


/* =============================================================================
 * tmalloc_clear
 * =============================================================================
 */
void
tmalloc_clear (tmalloc_t* tmallocPtr);


/* =============================================================================
 * tmalloc_releaseAllForward
 *
 * If visit() is NULL, it will not be performed
 * =============================================================================
 */
void
tmalloc_releaseAllForward (tmalloc_t* tmallocPtr, void (*visit)(void*, size_t));


/* =============================================================================
 * tmalloc_releaseAllReverse
 *
 * If visit() is NULL, it will not be performed
 * =============================================================================
 */
void
tmalloc_releaseAllReverse (tmalloc_t* tmallocPtr, void (*visit)(void*, size_t));


#ifdef __cplusplus
}
#endif


#endif /* TMALLOC_H */


/* =============================================================================
 *
 * End of tmalloc.h
 *
 * =============================================================================
 */
