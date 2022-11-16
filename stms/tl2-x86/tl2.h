/* =============================================================================
 *
 * tl2.h
 *
 * Transactional Locking 2 software transactional memory
 *
 * =============================================================================
 *
 * Copyright (C) Sun Microsystems Inc., 2006.  All Rights Reserved.
 * Authors: Dave Dice, Nir Shavit, Ori Shalev.
 *
 * TL2: Transactional Locking for Disjoint Access Parallelism
 *
 * Transactional Locking II,
 * Dave Dice, Ori Shalev, Nir Shavit
 * DISC 2006, Sept 2006, Stockholm, Sweden.
 *
 * =============================================================================
 *
 * Modified by Chi Cao Minh (caominh@stanford.edu)
 *
 * See VERSIONS for revision history
 *
 * =============================================================================
 */

#ifndef TL2_H
#define TL2_H 1














#include <stdint.h>
#include "tmalloc.h"


#  include <setjmp.h>






typedef struct _Thread Thread;

#ifdef __cplusplus
extern "C" {
#endif





#  include <setjmp.h>
#  define SIGSETJMP(env, savesigs)      sigsetjmp(env, savesigs)
#  define SIGLONGJMP(env, val)          siglongjmp(env, val); assert(0)



/*
 * Prototypes
 */




void     TxStart       (Thread*, sigjmp_buf*, int*);

Thread*  TxNewThread   ();





void     TxFreeThread  (Thread*);
void     TxInitThread  (Thread*, long id);
int      TxCommit      (Thread*);
void     TxAbort       (Thread*);
intptr_t TxLoad        (Thread*, volatile intptr_t*);
void     TxStore       (Thread*, volatile intptr_t*, intptr_t);
void     TxStoreLocal  (Thread*, volatile intptr_t*, intptr_t);
void     TxOnce        ();
void     TxShutdown    ();

void*    TxAlloc       (Thread*, size_t);
void     TxFree        (Thread*, void*);







#ifdef __cplusplus
}
#endif


#endif /* TL2_H */


/* =============================================================================
 *
 * End of tl2.h
 *
 * =============================================================================
 */
