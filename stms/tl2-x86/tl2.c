/* =============================================================================
 *
 * tl2.c
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


#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include "platform.h"
#include "tl2.h"
#include "tmalloc.h"
#include "util.h"

#if defined(TL2_RESIZE_HASHLOG) && !defined(TL2_OPTIM_HASHLOG)
#  error TL2_OPTIM_HASHLOG must be defined for TL2_RESIZE_HASHLOG
#endif













static void txSterilize (void*, size_t);


__INLINE__ long ReadSetCoherent (Thread*);
__INLINE__ long ReadSetCoherentPessimistic (Thread*);


enum tl2_config {
    TL2_INIT_WRSET_NUM_ENTRY = 1024,
    TL2_INIT_RDSET_NUM_ENTRY = 8192,
    TL2_INIT_LOCAL_NUM_ENTRY = 1024,
};

#ifdef TL2_OPTIM_HASHLOG
enum hashlog_config {
  HASHLOG_INIT_NUM_LOG           = 256,
  HASHLOG_INIT_NUM_ENTRY_PER_LOG = 8,
  HASHLOG_RESIZE_RATIO           = 4, /* avg num of entries per bucket threshold */
  HASHLOG_GROWTH_FACTOR          = 2,
};
/* Enable HashLog resizing with #define TL2_RESIZE_HASHLOG*/
#  ifdef __LP64__
#    define HASHLOG_SHIFT           (3)
#  else
#    define HASHLOG_SHIFT           (2)
#  endif
#  define HASHLOG_HASH(addr)        ((unsigned long)(addr) >> HASHLOG_SHIFT)
#  define TL2_INIT_WRSET_NUM_ENTRY  (HASHLOG_INIT_NUM_ENTRY_PER_LOG)
#endif

typedef enum {
    TIDLE       = 0, /* Non-transactional */
    TTXN        = 1, /* Transactional mode */
    TABORTING   = 3, /* aborting - abort pending */
    TABORTED    = 5, /* defunct - moribund txn */
    TCOMMITTING = 7,
} Modes;

typedef enum {
    LOCKBIT  = 1,
    NADA
} ManifestContants;

#  define PROF_STM_ABORT_BEGIN()        /* nothing */
#  define PROF_STM_ABORT_END()          /* nothing */
#  define PROF_STM_COMMIT_BEGIN()       /* nothing */
#  define PROF_STM_COMMIT_END()         /* nothing */
#  define PROF_STM_NEWTHREAD_BEGIN()    /* nothing */
#  define PROF_STM_NEWTHREAD_END()      /* nothing */
#  define PROF_STM_READ_BEGIN()         /* nothing */
#  define PROF_STM_READ_END()           /* nothing */
#  define PROF_STM_READLOCAL_BEGIN()    /* nothing */
#  define PROF_STM_READLOCAL_END()      /* nothing */
#  define PROF_STM_START_BEGIN()        /* nothing */
#  define PROF_STM_START_END()          /* nothing */
#  define PROF_STM_STERILIZE_BEGIN()    /* nothing */
#  define PROF_STM_STERILIZE_END()      /* nothing */
#  define PROF_STM_WRITE_BEGIN()        /* nothing */
#  define PROF_STM_WRITE_END()          /* nothing */
#  define PROF_STM_WRITELOCAL_BEGIN()   /* nothing */
#  define PROF_STM_WRITELOCAL_END()     /* nothing */
#  define PROF_STM_SUCCESS()            /* nothing */



typedef int            BitMap;
typedef uintptr_t      vwLock;  /* (Version,LOCKBIT) */
typedef unsigned char  byte;

/* Read set and write-set log entry */
typedef struct _AVPair {
    struct _AVPair* Next;
    struct _AVPair* Prev;
    volatile intptr_t* Addr;
    intptr_t Valu;
    volatile vwLock* LockFor; /* points to the vwLock covering Addr */
    vwLock rdv;               /* read-version @ time of 1st read - observed */
#ifndef TL2_EAGER
    byte Held;
#endif /* TL2_EAGER */
    struct _Thread* Owner;
    long Ordinal;
} AVPair;

typedef struct _Log {
    AVPair* List;
    AVPair* put;        /* Insert position - cursor */
    AVPair* tail;       /* CCM: Pointer to last valid entry */
    AVPair* end;        /* CCM: Pointer to last entry */
    long ovf;           /* Overflow - request to grow */
#ifndef TL2_OPTIM_HASHLOG
    BitMap BloomFilter; /* Address exclusion fast-path test */
#endif
} Log;

#ifdef TL2_OPTIM_HASHLOG
typedef struct _HashLog {
    Log* logs;
    long numLog;
    long numEntry;
    BitMap BloomFilter; /* Address exclusion fast-path test */
} HashLog;
#endif

struct _Thread {
    long UniqID;
    volatile long Mode;
    volatile long HoldsLocks; /* passed start of update */
    volatile long Retries;
    volatile vwLock rv;
    vwLock wv;
    vwLock abv;
#ifdef TL2_EAGER
    vwLock maxv;
    AVPair tmpLockEntry;
#endif /* TL2_EAGER */
    int* ROFlag;
    int IsRO;
    long Starts;
    long Aborts; /* Tally of # of aborts */
    unsigned long long rng;
    unsigned long long xorrng [1];
    void* memCache;
    tmalloc_t* allocPtr; /* CCM: speculatively allocated */
    tmalloc_t* freePtr;  /* CCM: speculatively free'd */
    Log rdSet;
#ifdef TL2_OPTIM_HASHLOG
    HashLog wrSet;
#else
    Log wrSet;
#endif
    Log LocalUndo;
    sigjmp_buf* envPtr;
#ifdef TL2_STATS
    long stats[12];
    long TxST;
    long TxLD;
#endif /* TL2_STATS */
};


/* #############################################################################
 * GENERIC INFRASTRUCTURE
 * #############################################################################
 */

static pthread_key_t    global_key_self;
static struct sigaction global_act_oldsigbus;
static struct sigaction global_act_oldsigsegv;

/* CCM: misaligned address (0xFF bytes) to generate bus error / segfault */
#define TL2_USE_AFTER_FREE_MARKER       (-1)


#ifndef TL2_CACHE_LINE_SIZE
#  define TL2_CACHE_LINE_SIZE           (64)
#endif


/* =============================================================================
 * MarsagliaXORV
 *
 * Simplistlic low-quality Marsaglia SHIFT-XOR RNG.
 * Bijective except for the trailing mask operation.
 * =============================================================================
 */
__INLINE__ unsigned long long
MarsagliaXORV (unsigned long long x)
{
    if (x == 0) {
        x = 1;
    }
    x ^= x << 6;
    x ^= x >> 21;
    x ^= x << 7;
    return x;
}


/* =============================================================================
 * MarsagliaXOR
 *
 * Simplistlic low-quality Marsaglia SHIFT-XOR RNG.
 * Bijective except for the trailing mask operation.
 * =============================================================================
 */
__INLINE__ unsigned long long
MarsagliaXOR (unsigned long long* seed)
{
    unsigned long long x = MarsagliaXORV(*seed);
    *seed = x;
    return x;
}


/* =============================================================================
 * TSRandom
 * =============================================================================
 */
__INLINE__ unsigned long long
TSRandom (Thread* Self)
{
    return MarsagliaXOR(&Self->rng);
}


/* =============================================================================
 * AtomicAdd
 * =============================================================================
 */
__INLINE__ intptr_t
AtomicAdd (volatile intptr_t* addr, intptr_t dx)
{
    intptr_t v;
    for (v = *addr; CAS(addr, v, v+dx) != v; v = *addr) {}
    return (v+dx);
}


/* =============================================================================
 * AtomicIncrement
 * =============================================================================
 */
__INLINE__ intptr_t
AtomicIncrement (volatile intptr_t* addr)
{
    intptr_t v;
    for (v = *addr; CAS(addr, v, v+1) != v; v = *addr) {}
    return (v+1);
}


/* #############################################################################
 * GLOBAL VERSION-CLOCK MANAGEMENT
 * #############################################################################
 */

/*
 * Consider 4M alignment for LockTab so we can use large-page support.
 * Alternately, we could mmap() the region with anonymous DZF pages.
 */
#  define _TABSZ  (1<< 20)
static volatile vwLock LockTab[_TABSZ];

/*
 * We use GClock[32] as the global counter.  It must be the sole occupant
 * of its cache line to avoid false sharing.  Even so, accesses to
 * GCLock will cause significant cache coherence & communication costs
 * as it is multi-read multi-write.
 */
static volatile vwLock GClock[TL2_CACHE_LINE_SIZE];
#define _GCLOCK  GClock[32]


/* =============================================================================
 * GVInit
 * =============================================================================
 */
__INLINE__ void
GVInit ()
{
    _GCLOCK = 0;
}


/* =============================================================================
 * GVRead
 * =============================================================================
 */
__INLINE__ vwLock
GVRead (Thread* Self)
{
#if 1
    return _GCLOCK;
#else
    /* Optional optimization: Avoid self-induced aborts (Use with GV5 or GV6) */
    vwLock gc = _GCLOCK;
    vwLock wv = Self->wv;
    if (wv > gc) {
        CAS(&_GCLOCK, gc, wv);
        return _GCLOCK;
    }
    return gc;
#endif
}


/*
 * GVGenerateWV():
 *
 * Conceptually, we'd like to fetch-and-add _GCLOCK.  In practice, however,
 * that naive approach, while safe and correct, results in CAS contention
 * and SMP cache coherency-related performance penalties.  As such, we
 * use either various schemes (GV4,GV5 or GV6) to reduce traffic on _GCLOCK.
 *
 * Global Version-Clock invariant:
 * I1: The generated WV must be > any previously observed (read) R
 */

#ifndef _GVCONFIGURATION
#  define _GVCONFIGURATION              4
#endif

#if _GVCONFIGURATION == 4
#  define _GVFLAVOR                     "GV4"
#  define GVGenerateWV                  GVGenerateWV_GV4
#endif

#if _GVCONFIGURATION == 5
#  define _GVFLAVOR                     "GV5"
#  define GVGenerateWV                  GVGenerateWV_GV5
#endif

#if _GVCONFIGURATION == 6
#  define _GVFLAVOR                     "GV6"
#  define GVGenerateWV                  GVGenerateWV_GV6
#endif


/* =============================================================================
 * GVGenerateWV_GV4
 *
 * The GV4 form of GVGenerateWV() does not have a CAS retry loop. If the CAS
 * fails then we have 2 writers that are racing, trying to bump the global
 * clock. One increment succeeded and one failed. Because the 2 writers hold
 * locks at the time we bump, we know that their write-sets don't intersect. If
 * the write-set of one thread intersects the read-set of the other then we know
 * that one will subsequently fail validation (either because the lock associated
 * with the read-set entry is held by the other thread, or because the other
 * thread already made the update and dropped the lock, installing the new
 * version #). In this particular case it's safe if two threads call
 * GVGenerateWV() concurrently and they both generate the same (duplicate) WV.
 * That is, if we have writers that concurrently try to increment the
 * clock-version and then we allow them both to use the same wv. The failing
 * thread can "borrow" the wv of the successful thread.
 * =============================================================================
 */
__INLINE__ vwLock
GVGenerateWV_GV4 (Thread* Self, vwLock maxv)
{
    vwLock gv = _GCLOCK;
    vwLock wv = gv + 2;
    vwLock k = CAS(&_GCLOCK, gv, wv);
    if (k != gv) {
        wv = k;
    }
    ASSERT((wv & LOCKBIT) == 0);
    ASSERT(wv != 0); /* overflow */
    ASSERT(wv > Self->wv);
    ASSERT(wv > Self->rv);
    ASSERT(wv > maxv);
    Self->wv = wv;
    return wv;
}


/* =============================================================================
 * GVGenerateWV_GV5
 *
 * Simply compute WV = GCLOCK + 2.
 *
 *  This increases the false+ abort-rate but reduces cache-coherence traffic.
 *  We only increment _GCLOCK at abort-time and perhaps TxStart()-time.
 *  The rate at which _GCLOCK advances controls performance and abort-rate.
 *  That is, the rate at which _GCLOCK advances is really a performance
 *  concern--related to false+ abort rates--rather than a correctness issue.
 *
 *  CONSIDER: use MAX(_GCLOCK, Self->rv, Self->wv, maxv, VERSION(Self->abv))+2
 * =============================================================================
 */
__INLINE__ vwLock
GVGenerateWV_GV5 (Thread* Self, vwLock maxv)
{
    vwLock wv = _GCLOCK + 2;
    if (maxv > wv) {
        wv = maxv + 2;
    }
    ASSERT(wv != 0); /* overflow */
    ASSERT(wv > Self->rv);
    ASSERT(wv >= Self->wv);
    Self->wv = wv;
    return wv;
}


/* =============================================================================
 * GVGenerateWV_GV6
 *
 * Composite of GV4 and GV5
 *
 * Trade-off -- abort-rate vs SMP cache-coherence costs.
 *
 * TODO: make the frequence mask adaptive at runtime.
 * Let the abort-rate or abort:success ratio drive the mask.
 * =============================================================================
 */
__INLINE__ vwLock
GVGenerateWV_GV6 (Thread* Self, vwLock maxv)
{
    long rnd = (long)MarsagliaXOR(Self->xorrng);
    if ((rnd & 0x1F) == 0) {
        return GVGenerateWV_GV4(Self, maxv);
    } else {
        return GVGenerateWV_GV5(Self, maxv);
    }
}


/* =============================================================================
 * GVAbort
 *
 * GV5 and GV6 admit single-threaded false+ aborts.
 *
 * Consider the following scenario:
 *
 * GCLOCK is initially 10.  TxStart() fetches GCLOCK, observing 10, and
 * sets RV accordingly.  The thread calls TXST().  At commit-time the thread
 * computes WV = 12 in GVComputeWV().  T1 stores WV (12) in various versioned
 * lock words covered by the write-set.  The transaction commits successfully.
 * The thread then runs a 2nd txn. TxStart() fetches _GCLOCK == 12 and sets RV
 * accordingly.  The thread then calls TXLD() to fetch a variable written in the
 * 1st txn and observes Version# == 12, which is > RV.  The thread aborts.
 * This is false+ abort as there is no actual interference.
 * We can recover by incrementing _GCLOCK at abort-time if we find
 * that RV == GCLOCK and Self->Abv > GCLOCK.
 *
 * Alternately we can attempt to avoid the false+ abort by advancing
 * _GCLOCK at GVRead()-time if we find that the thread's previous WV is >
 * than the current _GCLOCK value
 * =============================================================================
 */
__INLINE__ long
GVAbort (Thread* Self)
{
#if _GVCONFIGURATION != 4
    vwLock abv = Self->abv;
    if (abv & LOCKBIT) {
        return 0; /* normal interference */
    }
    vwLock gv = _GCLOCK;
    if (Self->rv == gv && abv > gv) {
        CAS(&_GCLOCK, gv, abv); /* advance to either (gv+2) or abv */
        /* If this was a GV5/GV6-specific false+ abort then do not delay */
        return 1; /* false+ abort */
    }
#endif
    return 0; /* normal interference */
}


/* #############################################################################
 * TL2 INFRASTRUCTURE
 * #############################################################################
 */

volatile long StartTally         = 0;
volatile long AbortTally         = 0;
volatile long ReadOverflowTally  = 0;
volatile long WriteOverflowTally = 0;
volatile long LocalOverflowTally = 0;
#define TL2_TALLY_MAX          (((unsigned long)(-1)) >> 1)

#ifdef TL2_STATS
volatile long global_stats[4096];
#endif



/*
 * With PS the versioned lock words (the LockTab array) are table stable and
 * references will never fault.  Under PO, however, fetches by a doomed
 * zombie txn can fault if the referent is free()ed and unmapped
 */
#if 0
#  define LDLOCK(a)                     LDNF(a)  /* for PO */
#else
#  define LDLOCK(a)                     *(a)     /* for PS */
#endif


/*
 * We use a degenerate Bloom filter with only one hash function generating
 * a single bit.  A traditional Bloom filter use multiple hash functions and
 * multiple bits.  Relatedly, the size our filter is small, so it can saturate
 * and become useless with a rather small write-set.
 * A better solution might be small per-thread hash tables keyed by address that
 * point into the write-set.
 * Beware that 0x1F == (MIN(sizeof(int),sizeof(intptr_t))*8)-
 */

#define FILTERHASH(a)                   ((UNS(a) >> 2) ^ (UNS(a) >> 5))
#define FILTERBITS(a)                   (1 << (FILTERHASH(a) & 0x1F))

/*
 * PSLOCK: maps variable address to lock address.
 * For PW the mapping is simply (UNS(addr)+sizeof(int))
 * COLOR attempts to place the lock(metadata) and the data on
 * different D$ indexes.
 */

#  define TABMSK                        (_TABSZ-1)

/*
#define COLOR                           0
#define COLOR                           (256-16)
*/
#define COLOR                           (128)

/*
 * Alternate experimental mapping functions ....
 * #define PSLOCK(a)     (LockTab + 0)                                   // PS1
 * #define PSLOCK(a)     (LockTab + ((UNS(a) >> 2) & 0x1FF))             // S512
 * #define PSLOCK(a)     (LockTab + (((UNS(a) >> 2) & (TABMSK & ~0x7)))) // PS1M
 * #define PSLOCK(a)     (LockTab + (((UNS(a) >> 6) & (TABMSK & ~0x7)))) // PS1M
 * #define PSLOCK(a)     (LockTab + ((((UNS(a) >> 2)|0xF) & TABMSK)))    // PS1M
 * #define PSLOCK(a)     (LockTab + (-(UNS(a) >> 2) & TABMSK))
 * #define PSLOCK(a)     (LockTab + ((UNS(a) >> 6) & TABMSK))
 * #define PSLOCK(a)     (LockTab + ((UNS(a) >> 2) & TABMSK))            // PS1
 */

/*
 * ILP32 vs LP64.  PSSHIFT == Log2(sizeof(intptr_t)).
 */
#define PSSHIFT                         ((sizeof(void*) == 4) ? 2 : 3)

#  define PSLOCK(a) (LockTab + (((UNS(a)+COLOR) >> PSSHIFT) & TABMSK)) /* PS1M */

/*
 * CCM: for debugging
 */
volatile vwLock*
pslock (volatile intptr_t* Addr)
{
    return PSLOCK(Addr);
}


/* =============================================================================
 * MakeList
 *
 * Allocate the primary list as a large chunk so we can guarantee ascending &
 * adjacent addresses through the list. This improves D$ and DTLB behavior.
 * =============================================================================
 */
__INLINE__ AVPair*
MakeList (long sz, Thread* Self)
{
    AVPair* ap = (AVPair*) malloc((sizeof(*ap) * sz) + TL2_CACHE_LINE_SIZE);
    assert(ap);
    memset(ap, 0, sizeof(*ap) * sz);
    AVPair* List = ap;
    AVPair* Tail = NULL;
    long i;
    for (i = 0; i < sz; i++) {
        AVPair* e = ap++;
        e->Next    = ap;
        e->Prev    = Tail;
        e->Owner   = Self;
        e->Ordinal = i;
        Tail = e;
    }
    Tail->Next = NULL;

    return List;
}


/* =============================================================================
 * FreeList
 * =============================================================================
 */
 void FreeList (Log*, long) __attribute__ ((noinline));
/*__INLINE__*/ void
FreeList (Log* k, long sz)
{
    /* Free appended overflow entries first */
    AVPair* e = k->end;
    if (e != NULL) {
        while (e->Ordinal >= sz) {
            AVPair* tmp = e;
            e = e->Prev;
            free(tmp);
        }
    }

    /* Free continguous beginning */
    free(k->List);
}


/* =============================================================================
 * ExtendList
 *
 * Postpend at the tail. We want the front of the list, which sees the most
 * traffic, to remains contiguous.
 * =============================================================================
 */
__INLINE__ AVPair*
ExtendList (AVPair* tail)
{
    AVPair* e = (AVPair*)malloc(sizeof(*e));
    assert(e);
    memset(e, 0, sizeof(*e));
    tail->Next = e;
    e->Prev    = tail;
    e->Next    = NULL;
    e->Owner   = tail->Owner;
    e->Ordinal = tail->Ordinal + 1;
    /*e->Held    = 0; -- done by memset*/
    return e;
}


#ifdef TL2_OPTIM_HASHLOG
/* =============================================================================
 * MakeHashLog
 * =============================================================================
 */
__INLINE__ void
MakeHashLog (HashLog* hlPtr, long numLog, long numEntryPerLog, Thread* Self)
{
    hlPtr->numEntry = 0;
    hlPtr->numLog = numLog;
    hlPtr->logs = (Log*)calloc(numLog, sizeof(Log));
    long i;
    for (i = 0; i < numLog; i++) {
        hlPtr->logs[i].List = MakeList(numEntryPerLog, Self);
        hlPtr->logs[i].put = hlPtr->logs[i].List;
    }
}


/* =============================================================================
 * FreeHashLog
 * =============================================================================
 */
__INLINE__ void
FreeHashLog (HashLog* hlPtr, long numEntryPerLog)
{
    long numLog = hlPtr->numLog;
    long i;
    for (i = 0; i < numLog; i++) {
        FreeList(&(hlPtr->logs[i]), numEntryPerLog);
    }
    free(hlPtr->logs);
}


#  ifdef TL2_RESIZE_HASHLOG
/* =============================================================================
 * ResizeHashLog
 * =============================================================================
 */
__INLINE__ void
ResizeHashLog (HashLog* hlPtr, Thread* Self)
{
    long oldNumLog = hlPtr->numLog;
    long newNumLog = oldNumLog * HASHLOG_GROWTH_FACTOR;
    Log* oldLogs = hlPtr->logs;
    Log* newLogs;
    Log* end;
    Log* log;

    /* Create new logs */
    newLogs = (Log*)calloc(newNumLog, sizeof(Log));
    end = newLogs + newNumLog;
    for (log = newLogs; log != end; log++) {
        log->List = MakeList(HASHLOG_INIT_NUM_ENTRY_PER_LOG, Self);
        log->put = log->List;
    }

    /* Move from old logs to new ones */
    end = oldLogs + oldNumLog;
    for (log = oldLogs; log != end; log++) {
        AVPair* oldEntry;
        AVPair* const End = log->put;
        for (oldEntry = log->List; oldEntry != End; oldEntry = oldEntry->Next) {
            volatile intptr_t* addr = oldEntry->Addr;
            long hash = HASHLOG_HASH(addr) % newNumLog;
            Log* newLog = &newLogs[hash];
            AVPair* newEntry = newLog->put;
            if (newEntry == NULL) {
                newEntry = ExtendList(newLog->tail);
                newLog->end = newEntry;
            }
            newLog->tail      = newEntry;
            newLog->put       = newEntry->Next;
            newEntry->Addr    = addr;
            newEntry->Valu    = oldEntry->Valu;
            newEntry->LockFor = oldEntry->LockFor;
            newEntry->Held    = oldEntry->Held;
            newEntry->rdv     = oldEntry->rdv;
        }
    }

    FreeHashLog(hlPtr, HASHLOG_INIT_NUM_ENTRY_PER_LOG);

    /* Point HashLog to new logs */
    hlPtr->numLog = newNumLog;
    hlPtr->logs = newLogs;
}
#  endif /* TL2_RESIZE_HASHLOG */
#endif /* TL2_OPTIM_HASHLOG */


/* =============================================================================
 * WriteBackForward
 *
 * Transfer the data in the log its ultimate location.
 * =============================================================================
 */
__INLINE__ void
WriteBackForward (Log* k)
{
    AVPair* e;
    AVPair* End = k->put;
    for (e = k->List; e != End; e = e->Next) {
        *(e->Addr) = e->Valu;
    }
}


/* =============================================================================
 * WriteBackReverse
 *
 * Transfer the data in the log its ultimate location.
 * =============================================================================
 */
__INLINE__ void
WriteBackReverse (Log* k)
{
    AVPair* e;
    for (e = k->tail; e != NULL; e = e->Prev) {
        *(e->Addr) = e->Valu;
    }
}


/* =============================================================================
 * FindFirst
 *
 * Search for first log entry that contains lock.
 * =============================================================================
 */
__INLINE__ AVPair*
FindFirst (Log* k, volatile vwLock* Lock)
{
    AVPair* e;
    AVPair* const End = k->put;
    for (e = k->List; e != End; e = e->Next) {
        if (e->LockFor == Lock) {
            return e;
        }
    }
    return NULL;
}


/* =============================================================================
 * RecordStore
 * =============================================================================
 */

#ifdef TL2_EAGER
__INLINE__ AVPair*
RecordStore (Log* k,
             volatile intptr_t* Addr,
             intptr_t Valu,
             volatile vwLock* Lock,
             vwLock cv)
{
    AVPair* e = k->put;
    if (e == NULL) {
        k->ovf++;
        e = ExtendList(k->tail);
        k->end = e;
    }
    ASSERT(Addr != NULL);
    k->tail    = e;
    k->put     = e->Next;
    e->Addr    = Addr;
    e->Valu    = Valu;
    e->LockFor = Lock;
    e->rdv     = cv;

    return e;
}
#else /* !TL2_EAGER */
__INLINE__ void
RecordStore (Log* k, volatile intptr_t* Addr, intptr_t Valu, volatile vwLock* Lock)
{
    /*
     * As an optimization we could squash multiple stores to the same location.
     * Maintain FIFO order to avoid WAW hazards.
     * TODO-FIXME - CONSIDER
     * Keep Self->LockSet as a sorted linked list of unique LockFor addresses.
     * We'd scan the LockSet for Lock.  If not found we'd insert a new
     * LockRecord at the appropriate location in the list.
     * Call InsertIfAbsent (Self, LockFor)
     */
    AVPair* e = k->put;
    if (e == NULL) {
        k->ovf++;
        e = ExtendList(k->tail);
        k->end = e;
    }
    ASSERT(Addr != NULL);
    k->tail    = e;
    k->put     = e->Next;
    e->Addr    = Addr;
    e->Valu    = Valu;
    e->LockFor = Lock;
    e->Held    = 0;
    e->rdv     = LOCKBIT; /* use either 0 or LOCKBIT */
}
#endif /* !TL2_EAGER */


/* =============================================================================
 * SaveForRollBack
 * =============================================================================
 */
__INLINE__ void
SaveForRollBack (Log* k, volatile intptr_t* Addr, intptr_t Valu)
{
    AVPair* e = k->put;
    if (e == NULL) {
        k->ovf++;
        e = ExtendList(k->tail);
        k->end = e;
    }
    k->tail    = e;
    k->put     = e->Next;
    e->Addr    = Addr;
    e->Valu    = Valu;
    e->LockFor = NULL;
}


/* =============================================================================
 * TrackLoad
 * =============================================================================
 */
__INLINE__ int
TrackLoad (Thread* Self, volatile vwLock* LockFor)
{
    Log* k = &Self->rdSet;

    /*
     * Consider collapsing back-to-back track loads ...
     * if the previous LockFor and rdv match the incoming arguments then
     * simply return
     */

    /*
     * Read log overflow suggests a rogue or incoherent transaction.
     * Consider calling SpeculativeReadSetCoherent() and, if needed, TxAbort().
     * This lets us distinguish between a doomed txn that's gone rogue
     * and a large transaction that legitimately overflows the buffer.
     * In the latter case we might extend the buffer or chain an overflow
     * buffer onto "k".
     * Options: print, abort, panic, extend, ignore & discard
     * Beware of inlining effects - TrackLoad() is performance-critical.
     * Decreasing the sample period tunable in TxValid() will reduce the
     * rate of overflows caused by zombie transactions.
     */

    AVPair* e = k->put;
    if (e == NULL) {
        if (!ReadSetCoherentPessimistic(Self)) {
            return 0;
        }
        k->ovf++;
        e = ExtendList(k->tail);
        k->end = e;
    }

    k->tail    = e;
    k->put     = e->Next;
    e->LockFor = LockFor;
    /* Note that Valu and Addr fields are undefined for tracked loads */

    return 1;
}





/* =============================================================================
 * useAfterFreeHandler
 *
 * CCM: txSterilize prevents deallocating memory that has current writers and
 * future readers will abort themselves. This function handles the case when
 * we try to access a pointer to memory that has been deallocated. It relies
 * on a marker set by TxFree.
 * =============================================================================
 */
static void
useAfterFreeHandler (int signum, siginfo_t* siginfo, void* context)
{
    Thread* Self = (Thread*)pthread_getspecific(global_key_self);

    if (Self == NULL || Self->Mode == TIDLE) {
        psignal(signum, NULL);
        exit(siginfo->si_errno);
    }

    if (Self->Mode == TTXN) {
        if (!ReadSetCoherentPessimistic(Self)) {
            TxAbort(Self);
        }
    }

    psignal(signum, NULL);
    abort();
}


/* =============================================================================
 * registerUseAfterFreeHandler
 * =============================================================================
 */
static void
registerUseAfterFreeHandler ()
{
    struct sigaction act;

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = &useAfterFreeHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(SIGBUS, &act, &global_act_oldsigbus) != 0) {
        perror("Error: Failed to register SIGBUS handler");
        exit(1);
    }

    if (sigaction(SIGSEGV, &act, &global_act_oldsigsegv) != 0) {
        perror("Error: Failed to register SIGSEGV handler");
        exit(1);
    }
}


/* =============================================================================
 * restoreUseAfterFreeHandler
 * =============================================================================
 */
static void
restoreUseAfterFreeHandler ()
{
    if (sigaction(SIGBUS, &global_act_oldsigbus, NULL) != 0) {
        perror("Error: Failed to restore SIGBUS handler");
        exit(1);
    }

    if (sigaction(SIGSEGV, &global_act_oldsigsegv, NULL) != 0) {
        perror("Error: Failed to restore SIGSEGV handler");
        exit(1);
    }
}




/* =============================================================================
 * TxOnce
 * =============================================================================
 */
void
TxOnce ()
{
    CTASSERT((_TABSZ & (_TABSZ-1)) == 0); /* must be power of 2 */

    GVInit();
    printf("TL2 system ready: GV=%s\n", _GVFLAVOR);

    pthread_key_create(&global_key_self, NULL); /* CCM: do before we register handler */
    registerUseAfterFreeHandler();

}


/* =============================================================================
 * TxShutdown
 * =============================================================================
 */
void
TxShutdown ()
{
    printf("TL2 system shutdown:\n"
           "  GCLOCK=0x%lX Starts=%li Aborts=%li\n"
           "  Overflows: R=%li W=%li L=%li\n",
           (unsigned long)_GCLOCK, StartTally, AbortTally,
           ReadOverflowTally, WriteOverflowTally, LocalOverflowTally);

#ifdef TL2_STATS
    unsigned i;
    for (i = 0; i < DIM(global_stats); i++) {
        if (global_stats[i] != 0) {
            printf("  %d: %d\n", i, global_stats[i]);
        }
    }
#endif

    pthread_key_delete(global_key_self);

    restoreUseAfterFreeHandler();

    MEMBARSTLD();
}


/* =============================================================================
 * TxNewThread
 * =============================================================================
 */
Thread*
TxNewThread ()
{
    PROF_STM_NEWTHREAD_BEGIN();

    Thread* t = (Thread*)malloc(sizeof(Thread));
    assert(t);

    PROF_STM_NEWTHREAD_END();

    return t;
}




/* =============================================================================
 * TxFreeThread
 * =============================================================================
 */
void
TxFreeThread (Thread* t)
{
    AtomicAdd((volatile intptr_t*)((void*)(&ReadOverflowTally)),  t->rdSet.ovf);

    long wrSetOvf = 0;
    Log* wr;
#ifdef TL2_OPTIM_HASHLOG
    long numLog = t->wrSet.numLog;
    Log* logs = t->wrSet.logs;
    Log* end = logs + numLog;
    for (wr = logs; wr != end; wr++)
#else
    wr = &t->wrSet;
#endif /* TL2_OPTIM_HASHLOG*/
    {
        wrSetOvf += wr->ovf;
    }
    AtomicAdd((volatile intptr_t*)((void*)(&WriteOverflowTally)), wrSetOvf);

    AtomicAdd((volatile intptr_t*)((void*)(&LocalOverflowTally)), t->LocalUndo.ovf);

    AtomicAdd((volatile intptr_t*)((void*)(&StartTally)),         t->Starts);
    AtomicAdd((volatile intptr_t*)((void*)(&AbortTally)),         t->Aborts);

    tmalloc_free(t->allocPtr);
    tmalloc_free(t->freePtr);

    FreeList(&(t->rdSet),     TL2_INIT_RDSET_NUM_ENTRY);
#ifdef TL2_OPTIM_HASHLOG
    FreeHashLog(&(t->wrSet),  HASHLOG_INIT_NUM_ENTRY_PER_LOG);
#else
    FreeList(&(t->wrSet),     TL2_INIT_WRSET_NUM_ENTRY);
#endif
    FreeList(&(t->LocalUndo), TL2_INIT_LOCAL_NUM_ENTRY);

    free(t);
}


/* =============================================================================
 * TxInitThread
 * =============================================================================
 */
void
TxInitThread (Thread* t, long id)
{
    /* CCM: so we can access TL2's thread metadata in signal handlers */
    pthread_setspecific(global_key_self, (void*)t);

    memset(t, 0, sizeof(*t));     /* Default value for most members */

    t->UniqID = id;
    t->rng = id + 1;
    t->xorrng[0] = t->rng;

#ifdef TL2_OPTIM_HASHLOG
    MakeHashLog(&t->wrSet, HASHLOG_INIT_NUM_LOG, HASHLOG_INIT_NUM_ENTRY_PER_LOG, t);
#else
    t->wrSet.List = MakeList(TL2_INIT_WRSET_NUM_ENTRY, t);
    t->wrSet.put = t->wrSet.List;
#endif

    t->rdSet.List = MakeList(TL2_INIT_RDSET_NUM_ENTRY, t);
    t->rdSet.put = t->rdSet.List;

    t->LocalUndo.List = MakeList(TL2_INIT_LOCAL_NUM_ENTRY, t);
    t->LocalUndo.put = t->LocalUndo.List;

    t->allocPtr = tmalloc_alloc(1);
    assert(t->allocPtr);
    t->freePtr = tmalloc_alloc(1);
    assert(t->freePtr);

#ifdef TL2_EAGER
    t->tmpLockEntry.Owner = t;
#endif /* TL2_EAGER */
}


/* =============================================================================
 * txReset
 * =============================================================================
 */
__INLINE__ void
txReset (Thread* Self)
{
    Self->Mode = TIDLE;

#ifdef TL2_OPTIM_HASHLOG
    if (Self->wrSet.numEntry > 0) {
        long numLog = Self->wrSet.numLog;
        Log* logs = Self->wrSet.logs;
        Log* end = logs + numLog;
        Log* log;
        for (log = logs; log != end; log++) {
            log->put = log->List;
            log->tail = NULL;
        }
    }
    Self->wrSet.numEntry = 0;
#else /* !TL2_OPTIM_HASHLOG */
    Self->wrSet.put = Self->wrSet.List;
    Self->wrSet.tail = NULL;
#endif /* !TL2_OPTIM_HASHLOG */

    Self->wrSet.BloomFilter = 0;
    Self->rdSet.put = Self->rdSet.List;
    Self->rdSet.tail = NULL;

    Self->LocalUndo.put = Self->LocalUndo.List;
    Self->LocalUndo.tail = NULL;
    Self->HoldsLocks = 0;

#ifdef TL2_EAGER
    Self->maxv = 0;
#endif
}


/* =============================================================================
 * txCommitReset
 * =============================================================================
 */
__INLINE__ void
txCommitReset (Thread* Self)
{
    txReset(Self);
    Self->Retries = 0;
}


/*
 * Remarks on deadlock:
 * Indefinite spinning in the lock acquisition phase admits deadlock.
 * We can avoid deadlock by any of the following means:
 *
 * 1. Bounded spin with back-off and retry.
 *    If the we fail to acquire the lock within the interval we drop all
 *    held locks, delay (back-off - either random or exponential), and retry
 *    the entire txn.
 *
 * 2. Deadlock detection - detect and recover.
 *    Use a simple waits-for graph to detect deadlock.  We can recovery
 *    either by aborting *all* the participant threads, or we can arbitrate.
 *    One thread becomes the winner and is allowed to proceed or continue
 *    spinning.  All other threads are losers and must abort and retry.
 *
 * 3. Prevent or avoid deadlock by acquiring locks in some order.
 *    Address order using LockFor as the key is the most natural.
 *    Unfortunately this requires sorting -- See the LockRecord structure.
 */


/* =============================================================================
 * OwnerOf
 * =============================================================================
 */
__INLINE__ Thread*
OwnerOf (vwLock v)
{
    return ((v & LOCKBIT) ? (((AVPair*) (v ^ LOCKBIT))->Owner) : NULL);
}


/* =============================================================================
 * ReadSetCoherent
 *
 * Is the read-set mutually consistent? Can be called at any time--before the
 * caller acquires locks or after.
 * =============================================================================
 */
__INLINE__ long
ReadSetCoherent (Thread* Self)
{
    intptr_t dx = 0;
    vwLock rv = Self->rv;
    Log* const rd = &Self->rdSet;
    AVPair* const EndOfList = rd->put;
    AVPair* e;

    ASSERT((rv & LOCKBIT) == 0);

    for (e = rd->List; e != EndOfList; e = e->Next) {
        ASSERT(e->LockFor != NULL);
        vwLock v = LDLOCK(e->LockFor);
        if (v & LOCKBIT) {
#  ifdef TL2_EAGER
            AVPair* p = (AVPair*)(v & ~LOCKBIT);
            if (p->Owner == Self) {
                if (p->rdv > rv) {
                    return 0; /* someone wrote after we read (and wrote) this */
                }
            } else {
                return 0; /* someone else has locked (and written) this */
            }
#  else /* !TL2_EAGER */
            dx |= UNS(((AVPair*)(UNS(v) & ~LOCKBIT))->Owner) ^ UNS(Self);
#  endif /* !TL2_EAGER */
        } else {
            dx |= (v > rv);
        }
    }

    return (dx == 0);
}


/* =============================================================================
 * ReadSetCoherentPessimistic
 *
 * CCM: Like ReadSetCoherent(), but return 0 as soon as we discover inconsistent
 * =============================================================================
 */
__INLINE__ long
ReadSetCoherentPessimistic (Thread* Self)
{
    vwLock rv = Self->rv;
    Log* const rd = &Self->rdSet;
    AVPair* const EndOfList = rd->put;
    AVPair* e;

    ASSERT((rv & LOCKBIT) == 0);

    for (e = rd->List; e != EndOfList; e = e->Next) {
        ASSERT(e->LockFor != NULL);
        vwLock v = LDLOCK(e->LockFor);
        if (v & LOCKBIT) {
#  ifdef TL2_EAGER
            AVPair* p = (AVPair*)(v & ~LOCKBIT);
            if (p->Owner == Self) {
                if (p->rdv > rv) {
                    return 0; /* someone wrote after we read (and wrote) this */
                }
            } else {
                return 0; /* someone else has locked (and written) this */
            }
#  else /* !TL2_EAGER */
            if (UNS(((AVPair*)(UNS(v) ^ LOCKBIT))->Owner) != UNS(Self)) {
                return 0;
            }
#  endif /* !TL2_EAGER */
        } else {
            if (v > rv) {
               return 0;
            }
        }
    }

    return 1;
}


/* =============================================================================
 * RestoreLocks
 * =============================================================================
 */
__INLINE__ void
RestoreLocks (Thread* Self)
{
#  ifdef TL2_OPTIM_HASHLOG
    long numLog = Self->wrSet.numLog;
    Log* logs = Self->wrSet.logs;
    Log* end = logs + numLog;
    Log* wr;
    for (wr = logs; wr != end; wr++)
#  else /* !TL2_OPTIM_HASHLOG*/
    Log* wr = &Self->wrSet;
#  endif /* !TL2_OPTIM_HASHLOG*/
    {
        AVPair* p;
        AVPair* const End = wr->put;
        for (p = wr->List; p != End; p = p->Next) {
            ASSERT(p->Addr != NULL);
            ASSERT(p->LockFor != NULL);
#  ifndef TL2_EAGER
            if (p->Held == 0) {
                continue;
            }
#  endif /* !Tl2_EAGER */
            ASSERT(OwnerOf(*(p->LockFor)) == Self);
            ASSERT(*(p->LockFor) == (UNS(p)|LOCKBIT));
            ASSERT((p->rdv & LOCKBIT) == 0);
#  ifndef TL2_EAGER
            p->Held = 0;
#  endif /* !Tl2_EAGER */
            *(p->LockFor) = p->rdv;
        }
    }
#  ifndef TL2_EAGER
    Self->HoldsLocks = 0;
#  endif /* TL2_EAGER */
}


/* =============================================================================
 * DropLocks
 * =============================================================================
 */
__INLINE__ void
DropLocks (Thread* Self, vwLock wv)
{
#  ifdef TL2_OPTIM_HASHLOG
    ASSERT((wv & LOCKBIT) == 0);
    long numLog = Self->wrSet.numLog;
    Log* logs = Self->wrSet.logs;
    Log* end = logs + numLog;
    Log* wr;
    for (wr = logs; wr != end; wr++)
#  else /* !TL2_OPTIM_HASHLOG*/
    Log* wr = &Self->wrSet;
#  endif /* !TL2_OPTIM_HASHLOG*/
    {
        AVPair* p;
        AVPair* const End = wr->put;
        for (p = wr->List; p != End; p = p->Next) {
            ASSERT(p->Addr != NULL);
            ASSERT(p->LockFor != NULL);
#  ifndef TL2_EAGER
            if (p->Held == 0) {
                continue;
            }
            p->Held = 0;
#  endif /* !TL2_EAGER */
#  if _GVCONFIGURATION == 4
            ASSERT(wv > p->rdv);
#  else
            /* GV5 and GV6 admit wv == p->rdv */
            ASSERT(wv >= p->rdv);
#  endif
            ASSERT(OwnerOf(*(p->LockFor)) == Self);
            ASSERT(*(p->LockFor) == (UNS(p)|LOCKBIT));
            *(p->LockFor) = wv;
        }
    }
#  ifndef TL2_EAGER
    Self->HoldsLocks = 0;
#  endif /* TL2_EAGER */
}


/* =============================================================================
 * backoff
 * =============================================================================
 */
__INLINE__ void
backoff (Thread* Self, long attempt)
{
#ifdef TL2_BACKOFF_EXPONENTIAL
    unsigned long long n = 1 << ((attempt < 63) ? (attempt) : (63));
    unsigned long long stall = TSRandom(Self) % n;
#else
    unsigned long long stall = TSRandom(Self) & 0xF;
    stall += attempt >> 2;
    stall *= 10;
#endif
#if 0
    TL2_TIMER_T expiry = TL2_TIMER_READ() + stall;
    while (TL2_TIMER_READ() < expiry) {
        PAUSE();
    }
#else
    /* CCM: timer function may misbehave */
    //volatile typeof(stall) i = 0;
    volatile unsigned long long i = 0;
    while (i++ < stall) {
        PAUSE();
    }
#endif
}


/* =============================================================================
 * TryFastUpdate
 * =============================================================================
 */
__INLINE__ long
TryFastUpdate (Thread* Self)
{
#  ifdef TL2_OPTIM_HASHLOG
    long numLog = Self->wrSet.numLog;
    Log* logs = Self->wrSet.logs;
    Log* end = logs + numLog;
    Log* wr;
#  endif /* !TL2_OPTIM_HASHLOG */

#  ifndef TL2_EAGER
#    ifdef TL2_OPTIM_HASHLOG
    Log* wr;
#    else /* !TL2_OPTIM_HASHLOG */
    Log* const wr = &Self->wrSet;
#    endif /* !TL2_OPTIM_HASHLOG */
    Log* const rd = &Self->rdSet;
    long ctr;
#  endif /* !TL2_EAGER */
    vwLock wv;

    ASSERT(Self->Mode == TTXN);

    /*
     * Optional optimization -- pre-validate the read-set.
     *
     * Consider: Call ReadSetCoherent() before grabbing write-locks.
     * Validate that the set of values we've fetched from pure READ objects
     * remain coherent.  This avoids the situation where a doomed transaction
     * grabs write locks and impedes or causes other potentially successful
     * transactions to spin or abort.
     *
     * A smarter tactic might be to only call ReadSetCoherent() when
     * Self->Retries > NN
     */

#  if 0
    if (!ReadSetCoherent(Self)) {
        return 0;
    }
#  endif

#  ifndef TL2_EAGER

    /*
     * Consider: if the write-set is long or Self->Retries is high we
     * could run a pre-pass and sort the write-locks by LockFor address.
     * We could either use a separate LockRecord list (sorted) or
     * link the write-set entries via SortedNext
     */

    /*
     * Lock-acquisition phase ...
     *
     * CONSIDER: While iterating over the locks that cover the write-set
     * track the maximum observed version# in maxv.
     * In GV4:   wv = GVComputeWV(); ASSERT wv > maxv
     * In GV5|6: wv = GVComputeWV(); if (maxv >= wv) wv = maxv + 2
     * This is strictly an optimization.
     * maxv isn't required for algorithmic correctness
     */
    Self->HoldsLocks = 1;
    ctr = 1000; /* Spin budget - TUNABLE */
    vwLock maxv = 0;
    AVPair* p;
#      ifdef TL2_OPTIM_HASHLOG
    for (wr = logs; wr != end; wr++)
#      endif /* TL2_OPTIM_HASHLOG*/
    {
        AVPair* const End = wr->put;
        for (p = wr->List; p != End; p = p->Next) {
            volatile vwLock* const LockFor = p->LockFor;
            vwLock cv;
            ASSERT(p->Addr != NULL);
            ASSERT(p->LockFor != NULL);
            ASSERT(p->Held == 0);
            ASSERT(p->Owner == Self);
            /* Consider prefetching only when Self->Retries == 0 */
            prefetchw(LockFor);
            cv = LDLOCK(LockFor);
            if ((cv & LOCKBIT) && ((AVPair*)(cv ^ LOCKBIT))->Owner == Self) {
                /* CCM: revalidate read because could be a hash collision */
                if (FindFirst(rd, LockFor) != NULL) {
                    if (((AVPair*)(cv ^ LOCKBIT))->rdv > Self->rv) {
                        Self->abv = cv;
                        return 0;
                    }
                }
                /* Already locked by an earlier iteration. */
                continue;
            }

            /* SIGTM does not maintain a read set */
            if (FindFirst(rd, LockFor) != NULL) {
                /*
                 * READ-WRITE stripe
                 */
                if ((cv & LOCKBIT) == 0 &&
                    cv <= Self->rv &&
                    UNS(CAS(LockFor, cv, (UNS(p)|UNS(LOCKBIT)))) == UNS(cv))
                {
                    if (cv > maxv) {
                        maxv = cv;
                    }
                    p->rdv  = cv;
                    p->Held = 1;
                    continue;
                }
                /*
                 * The stripe is either locked or the previously observed read-
                 * version changed.  We must abort. Spinning makes little sense.
                 * In theory we could spin if the read-version is the same but
                 * the lock is held in the faint hope that the owner might
                 * abort and revert the lock
                 */
                Self->abv = cv;
                return 0;
            } else
            {
                /*
                 * WRITE-ONLY stripe
                 * Note that we already have a fresh copy of *LockFor in cv.
                 * If we find a write-set element locked then we can either
                 * spin or try to find something useful to do, such as :
                 * A. Validate the read-set by calling ReadSetCoherent()
                 *    We can abort earlier if the transaction is doomed.
                 * B. optimistically proceed to the next element in the write-set.
                 *    Skip the current locked element and advance to the
                 *    next write-set element, later retrying the skipped elements
                 */
#      ifdef TL2_NOCM
                /* wkbaek: no spinning in NOCM mode */
                long c = 0;
#      else /* !TL2_NOCM */
                long c = ctr;
#      endif /* !TL2_NOCM */
                for (;;) {
                    cv = LDLOCK(LockFor);
                    /* CCM: for SIGTM, this IF and its true path need to be "atomic" */
                    if ((cv & LOCKBIT) == 0 &&
                        UNS(CAS(LockFor, cv, (UNS(p)|UNS(LOCKBIT)))) == UNS(cv))
                    {
                        if (cv > maxv) {
                            maxv = cv;
                        }
                        p->rdv  = cv; /* save so we can restore or increment */
                        p->Held = 1;
                        break;
                    }
                    if (--c < 0) {
                        /* Will fall through to TxAbort */
                        return 0;
                    }
                    /*
                     * Consider: while spinning we might validate
                     * the read-set by calling ReadSetCoherent()
                     */
                    PAUSE();
                }
            } /* write-only stripe */
        } /* foreach (entry in write-set) */
    }

#  endif /* !TL2_EAGER */


#    ifdef TL2_EAGER
    wv = GVGenerateWV(Self, Self->maxv);
#    else /* !TL2_EAGER */
    wv = GVGenerateWV(Self, maxv);
#    endif /* !TL2_EAGER */

    /*
     * We now hold all the locks for RW and W objects.
     * Next we validate that the values we've fetched from pure READ objects
     * remain coherent.
     *
     * If GVGenerateWV() is implemented as a simplistic atomic fetch-and-add
     * then we can optimize by skipping read-set validation in the common-case.
     * Namely,
     *   if (Self->rv != (wv-2) && !ReadSetCoherent(Self)) { ... abort ... }
     * That is, we could elide read-set validation for pure READ objects if
     * there were no intervening write txns between the fetch of _GCLOCK into
     * Self->rv in TxStart() and the increment of _GCLOCK in GVGenerateWV()
     */

    /*
     * CCM: for SIGTM, the read filter would have triggered an abort already
     * if the read-set was not consistent.
     */
    if (!ReadSetCoherent(Self)) {
        /*
         * The read-set is inconsistent.
         * The transaction is spoiled as the read-set is stale.
         * The candidate results produced by the txn and held in
         * the write-set are a function of the read-set, and thus invalid
         */
        return 0;
    }

    /*
     * We are now committed - this txn is successful.
     */

#  ifndef TL2_EAGER
#    ifdef TL2_OPTIM_HASHLOG
    for (wr = logs; wr != end; wr++)
#    endif /* TL2_OPTIM_HASHLOG*/
    {
        WriteBackForward(wr); /* write-back the deferred stores */
    }
#  endif /* !TL2_EAGER */
    MEMBARSTST(); /* Ensure the above stores are visible  */
    MEMBARSTLD(); /* PEDRO */
    DropLocks(Self, wv); /* Release locks and increment the version */

    /*
     * Ensure that all the prior STs have drained before starting the next
     * txn.  We want to avoid the scenario where STs from "this" txn
     * languish in the write-buffer and inadvertently satisfy LDs in
     * a subsequent txn via look-aside into the write-buffer
     */
    MEMBARSTLD();

    return 1; /* success */
}



/* =============================================================================
 * TxAbort
 *
 * Our mechanism admits mutual abort with no progress - livelock.
 * Consider the following scenario where T1 and T2 execute concurrently:
 * Thread T1:  WriteLock A; Read B LockWord; detect locked, abort, retry
 * Thread T2:  WriteLock B; Read A LockWord; detect locked, abort, retry
 *
 * Possible solutions:
 *
 * - Try backoff (random and/or exponential), with some mixture
 *   of yield or spinning.
 *
 * - Use a form of deadlock detection and arbitration.
 *
 * In practice it's likely that a semi-random semi-exponential back-off
 * would be best.
 * =============================================================================
 */
void
TxAbort (Thread* Self)
{
    PROF_STM_ABORT_BEGIN();


    Self->Mode = TABORTED;

#ifdef TL2_EAGER
    WriteBackReverse(&Self->wrSet);
    RestoreLocks(Self);
#else /* !TL2_EAGER */
    if (Self->HoldsLocks) {
        RestoreLocks(Self);
    }
#endif /* !TL2_EAGER */

    /* Clean up after an abort. Restore any modified locals */
    if (Self->LocalUndo.put != Self->LocalUndo.List) {
        WriteBackReverse(&Self->LocalUndo);
    }

    Self->Retries++;
    Self->Aborts++;

    if (GVAbort(Self)) {
        /* possibly advance _GCLOCK for GV5 or GV6 */
        goto __rollback;
    }

    /*
     * Beware: back-off is useful for highly contended environments
     * where N threads shows negative scalability over 1 thread.
     * Extreme back-off restricts parallelism and, in the extreme,
     * is tantamount to allowing the N parallel threads to run serially
     * 1 at-a-time in succession.
     *
     * Consider: make the back-off duration a function of:
     * - a random #
     * - the # of previous retries
     * - the size of the previous read-set
     * - the size of the previous write-set
     *
     * Consider using true CSMA-CD MAC style random exponential backoff
     */

#ifndef TL2_NOCM
    if (Self->Retries > 3) { /* TUNABLE */
        backoff(Self, Self->Retries);
    }
#endif

__rollback:

    tmalloc_releaseAllReverse(Self->allocPtr, NULL);
    tmalloc_clear(Self->freePtr);

    PROF_STM_ABORT_END();
    SIGLONGJMP(*Self->envPtr, 1);
    ASSERT(0);
}


/* =============================================================================
 * TxStore
 * =============================================================================
 */
#ifdef TL2_EAGER
void
TxStore (Thread* Self, volatile intptr_t* addr, intptr_t valu)
{
    PROF_STM_WRITE_BEGIN();

    ASSERT(Self->Mode == TTXN);
    if (Self->IsRO) {
        *(Self->ROFlag) = 0;
        PROF_STM_WRITE_END();
        TxAbort(Self);
        ASSERT(0);
    }

#  ifdef TL2_STATS
    Self->TxST++;
#  endif


    /*
     * Try to acquire the lock. If we are not the owner, we spin a
     * bit before deciding to abort. If we acquire the lock, we
     * set the lockbit in the lockword and use a temporary AVPair* value.
     * Later we update it with a the actual AVPair*.
     */

    volatile vwLock* LockFor = PSLOCK(addr);
    vwLock cv = LDLOCK(LockFor);

    if ((cv & LOCKBIT) && (((AVPair*)(cv ^ LOCKBIT))->Owner == Self)) {
        /*
         * We own this lock; update cv with the correct value for RecordStore.
         */
        cv = ((AVPair*)(cv ^ LOCKBIT))->rdv;
    } else {
#    ifdef TL2_NOCM
        /* wkbaek: in NOCM mode, no spinning */ 
        TxAbort(Self);
        ASSERT(0);
#    else /* !TL2_NOCM */
        /*
         * We do not own this lock, so try to acquire it.
         */
        long c = 100; /* TUNABLE */
        AVPair* p = &(Self->tmpLockEntry);
        for (;;) {
            cv = LDLOCK(LockFor);
            if ((cv & LOCKBIT) == 0 &&
                UNS(CAS(LockFor, cv, (UNS(p)|UNS(LOCKBIT)))) == UNS(cv))
            {
                break;
            }
            if (--c < 0) {
                PROF_STM_WRITE_END();
                TxAbort(Self);
                ASSERT(0);
            }
        }
#endif /* !TL2_NOCM */
    }


    Log* wr = &Self->wrSet;
    AVPair* e = RecordStore(wr, addr, *addr, LockFor, cv);
    if (cv > Self->maxv) {
        Self->maxv = cv;
    }

    *LockFor = UNS(e) | UNS(LOCKBIT);

    *addr = valu;

    PROF_STM_WRITE_END();
}
#else /* !TL2_EAGER */
void
TxStore (Thread* Self, volatile intptr_t* addr, intptr_t valu)
{
    PROF_STM_WRITE_BEGIN();

    volatile vwLock* LockFor;
    vwLock rdv;

    /*
     * In TL2 we're always coherent, so we should never see NULL stores.
     * In TL it was possible to see NULL stores in zombie txns.
     */

    ASSERT(Self->Mode == TTXN);
    if (Self->IsRO) {
        *(Self->ROFlag) = 0;
        PROF_STM_WRITE_END();
        TxAbort(Self);
        return;
    }

#  ifdef TL2_STATS
    Self->TxST++;
#  endif

  LockFor = PSLOCK(addr);

#  if 0
    /* CONSIDER: prefetch both the lock and the data */
    if (Self->Retries == 0) {
        prefetchw(addr);
        prefetchw(LockFor);
    }
#  endif

    /*
     * CONSIDER: spin briefly (bounded) while the object is locked,
     * periodically calling ReadSetCoherent(Self)
     */

#  if 1
    rdv = LDLOCK(LockFor);
#  else
    {
        long ctr = 100; /* TUNABLE */
        for (;;) {
            rdv = LDLOCK(LockFor);
            if ((rdv & LOCKBIT) == 0) {
                break;
            } else if ((ctr & 0x1F) == 0 && !ReadSetCoherent(Self)) {
                PROF_STM_WRITE_END();
                TxAbort(Self);
                return;
            } else if (--ctr < 0) {
                PROF_STM_WRITE_END();
                TxAbort(Self);
                return;
            }
        }
    }
#  endif

#  ifdef TL2_OPTIM_HASHLOG
    HashLog* wrSet = &Self->wrSet;
    long numLog = wrSet->numLog;
    long hash = HASHLOG_HASH(addr) % numLog;
    Log* wr = &wrSet->logs[hash];
#  else /* !TL2_OPTIM_HASHLOG */
    Log* wr = &Self->wrSet;
#  endif /* !TL2_OPTIM_HASHLOG */
    /*
     * Convert a redundant "idempotent" store to a tracked load.
     * This helps minimize the wrSet size and reduces false+ aborts.
     * Conceptually, "a = x" is equivalent to "if (a != x) a = x"
     * This is entirely optional
     */
    MEMBARLDLD();

    if (ALWAYS && LDNF(addr) == valu) {
        AVPair* e;
        for (e = wr->tail; e != NULL; e = e->Prev) {
            ASSERT(e->Addr != NULL);
            if (e->Addr == addr) {
                ASSERT(LockFor == e->LockFor);
                e->Valu = valu; /* CCM: update associated value in write-set */
                PROF_STM_WRITE_END();
                return;
            }
        }
        /* CCM: Not writing new value; convert to load */
        if ((rdv & LOCKBIT) == 0 && rdv <= Self->rv && LDLOCK(LockFor) == rdv) {
            if (!TrackLoad(Self, LockFor)) {
                PROF_STM_WRITE_END();
                TxAbort(Self);
            }
            PROF_STM_WRITE_END();
            return;
        }
    }

#    ifdef TL2_OPTIM_HASHLOG
    wrSet->BloomFilter |= FILTERBITS(addr);
#    else /* !TL2_OPTIM_HASHLOG */
    wr->BloomFilter |= FILTERBITS(addr);
#    endif /* !TL2_OPTIM_HASHLOG */


#  ifdef TL2_OPTIM_HASHLOG
    wrSet->numEntry++;
#  endif /* TL2_OPTIM_HASHLOG*/

    RecordStore(wr, addr, valu, LockFor);

#  if defined(TL2_OPTIM_HASHLOG) && defined(TL2_RESIZE_HASHLOG)
    long numEntry = wrSet->numEntry;
    if (numEntry > (numLog * HASHLOG_RESIZE_RATIO)) {
        ResizeHashLog(wrSet, Self);
    }
#  endif /* TL2_OPTIM_HASHLOG && TL2_RESIZE_HASHLOG */

    PROF_STM_WRITE_END();
}
#endif /* !TL2_EAGER */


/* =============================================================================
 * TxLoad
 * =============================================================================
 */
#ifdef TL2_EAGER
intptr_t
TxLoad (Thread* Self, volatile intptr_t* Addr)
{
    PROF_STM_READ_BEGIN();

    intptr_t Valu;

#  ifdef TL2_STATS
    Self->TxLD++;
#  endif

    ASSERT(Self->Mode == TTXN);


    volatile vwLock* LockFor = PSLOCK(Addr);
    vwLock cv = LDLOCK(LockFor);
    vwLock rdv = cv & ~LOCKBIT;
    MEMBARLDLD();
    Valu = LDNF(Addr);
    MEMBARLDLD();
    if ((rdv <= Self->rv && LDLOCK(LockFor) == rdv) ||
        ((cv & LOCKBIT) && (((AVPair*)rdv)->Owner == Self)))
    {
        if (!Self->IsRO) {
            if (!TrackLoad(Self, LockFor)) {
                PROF_STM_READ_END();
                TxAbort(Self);
            }
        }
        PROF_STM_READ_END();
        return Valu;
    }

    /*
     * The location is either currently locked or has been updated since this
     * txn started.  In the later case if the read-set is otherwise empty we
     * could simply re-load Self->rv = _GCLOCK and try again.  If the location
     * is locked it's fairly likely that the owner will release the lock by
     * writing a versioned write-lock value that is > Self->rv, so spinning
     * provides little profit.
     */

    Self->abv = rdv;
    PROF_STM_READ_END();
    TxAbort(Self);
    ASSERT(0);

    return 0;
}
#else /* !TL2_EAGER */
intptr_t
TxLoad (Thread* Self, volatile intptr_t* Addr)
{
    PROF_STM_READ_BEGIN();

    intptr_t Valu;

#  ifdef TL2_STATS
    Self->TxLD++;
#  endif

    ASSERT(Self->Mode == TTXN);

    /*
     * Preserve the illusion of processor consistency in run-ahead mode.
     * Look-aside: check the wrSet for RAW hazards.
     * This is optional, but it improves the quality and fidelity
     * of the wrset and rdset compiled during speculative mode.
     * Consider using a Bloom filter on the addresses in wrSet to give us
     * a statistically fast out if the address doesn't appear in the set
     */

    intptr_t msk = FILTERBITS(Addr);
    if ((Self->wrSet.BloomFilter & msk) == msk) {
#  ifdef TL2_OPTIM_HASHLOG
        Log* wr = &Self->wrSet.logs[HASHLOG_HASH(Addr) % Self->wrSet.numLog];
#  else /* !TL2_OPTIM_HASHLOG */
        Log* wr = &Self->wrSet;
#  endif /* !TL2_OPTIM_HASHLOG */
        AVPair* e;
        for (e = wr->tail; e != NULL; e = e->Prev) {
            ASSERT(e->Addr != NULL);
            if (e->Addr == Addr) {
                PROF_STM_READ_END();
                return e->Valu;
            }
        }
    }

    /*
     * TODO-FIXME:
     * Currently we set Self->rv in TxStart(). We might be better served to
     * defer reading Self->rv until the 1st transactional load.
     * if (Self->rv == 0) Self->rv = _GCLOCK
     */

    /*
     * Fetch tentative value
     * Use either SPARC non-fault loads or complicit signal handlers.
     * If the LD fails we'd like to call TxAbort()
     * TL2 does not permit zombie/doomed txns to run
     */
    volatile vwLock* LockFor = PSLOCK(Addr);
    vwLock rdv = LDLOCK(LockFor) & ~LOCKBIT;
    MEMBARLDLD();
    Valu = LDNF(Addr);
    MEMBARLDLD();
    if (rdv <= Self->rv && LDLOCK(LockFor) == rdv) {
        if (!Self->IsRO) {
            if (!TrackLoad(Self, LockFor)) { /* PEDRO: abort if ReadSetCoherentPessimistic() returns 0 */
                PROF_STM_READ_END();
                TxAbort(Self);
            }
        }
        PROF_STM_READ_END();
        //if (Valu == (intptr_t)42) printf("%lu  %lu\n", rdv, Self->rv);
        return Valu;
    }

    /*
     * The location is either currently locked or has been updated since this
     * txn started.  In the later case if the read-set is otherwise empty we
     * could simply re-load Self->rv = _GCLOCK and try again.  If the location
     * is locked it's fairly likely that the owner will release the lock by
     * writing a versioned write-lock value that is > Self->rv, so spinning
     * provides little profit.
     */

    Self->abv = rdv;
    PROF_STM_READ_END();
    TxAbort(Self);
    ASSERT(0);

    return 0;
}
#endif /* TL2_EAGER */



/* =============================================================================
 * txSterilize
 *
 * Use txSterilize() any time an object passes out of the transactional domain
 * and will be accessed solely with normal non-transactional load and store
 * operations.
 * =============================================================================
 */
static void
txSterilize (void* Base, size_t Length)
{
    PROF_STM_STERILIZE_BEGIN();

    intptr_t* Addr = (intptr_t*)Base;
    intptr_t* End = Addr + Length;
    ASSERT(Addr <= End);
    while (Addr < End) {
        volatile vwLock* Lock = PSLOCK(Addr);
        intptr_t val = *Lock;
        /* CCM: invalidate future readers */
        CAS(Lock, val, (_GCLOCK & ~LOCKBIT));
        Addr++;
    }
    memset(Base, (unsigned char)TL2_USE_AFTER_FREE_MARKER, Length);

    PROF_STM_STERILIZE_END();
}


/* =============================================================================
 * TxStoreLocal
 *
 * Update in-place, saving the original contents in the undo log
 * =============================================================================
 */
void
TxStoreLocal (Thread* Self, volatile intptr_t* Addr, intptr_t Valu)
{
    PROF_STM_WRITELOCAL_BEGIN();

    SaveForRollBack(&Self->LocalUndo, Addr, *Addr);
    *Addr = Valu;

    PROF_STM_WRITELOCAL_END();
}


/* =============================================================================
 * TxStart
 * =============================================================================
 */
void
TxStart (Thread* Self, sigjmp_buf* envPtr, int* ROFlag)
{
    PROF_STM_START_BEGIN();

    ASSERT(Self->Mode == TIDLE || Self->Mode == TABORTED);
    txReset(Self);

    Self->rv = GVRead(Self);
    ASSERT((Self->rv & LOCKBIT) == 0);
    MEMBARLDLD();

    Self->Mode = TTXN;
    Self->ROFlag = ROFlag;
    Self->IsRO = ROFlag ? *ROFlag : 0;
    Self->envPtr= envPtr;

    ASSERT(Self->LocalUndo.put == Self->LocalUndo.List);
    ASSERT(Self->wrSet.put == Self->wrSet.List);

    Self->Starts++;



    PROF_STM_START_END();
}


/* =============================================================================
 * TxCommit
 * =============================================================================
 */
int
TxCommit (Thread* Self)
{
    PROF_STM_COMMIT_BEGIN();

    ASSERT(Self->Mode == TTXN);

    /* Fast-path: Optional optimization for pure-readers */
#  ifdef TL2_OPTIM_HASHLOG
    if (Self->wrSet.numEntry == 0)
#  else /* !TL2_OPTIM_HASHLOG*/
    if (Self->wrSet.put == Self->wrSet.List)
#  endif /* !TL2_OPTIM_HASHLOG*/
    {
        /* Given TL2 the read-set is already known to be coherent. */
        txCommitReset(Self);
        tmalloc_clear(Self->allocPtr);
        tmalloc_releaseAllForward(Self->freePtr, &txSterilize);
#  if defined(TL2_OPTIM_HASHLOG) && defined(TL2_RESIZE_HASHLOG)
        if (Self->wrSet.numLog > HASHLOG_INIT_NUM_LOG) {
            /*
             * If we are read-only, reduce the number of logs so less time
             * iterating over logs only to find that they are empty.
             */
            Self->wrSet.numLog--;
        }
#  endif
        PROF_STM_COMMIT_END();
        PROF_STM_SUCCESS();
        return 1;
    }

    if (TryFastUpdate(Self)) {
        txCommitReset(Self);
        tmalloc_clear(Self->allocPtr);
        tmalloc_releaseAllForward(Self->freePtr, &txSterilize);
#if defined(TL2_OPTIM_HASHLOG) && defined(TL2_RESIZE_HASHLOG)
        if (Self->wrSet.numLog > HASHLOG_INIT_NUM_LOG &&
            Self->wrSet.numEntry < (HASHLOG_INIT_NUM_LOG * HASHLOG_RESIZE_RATIO))
        {
            /*
             * Current hash log is too large. Reduce the number of logs so less
             * time is spent iterating over logs only to find that they are empty.
             */
            Self->wrSet.numLog--;
        }
#endif
        PROF_STM_COMMIT_END();
        PROF_STM_SUCCESS();
        return 1;
    }

    PROF_STM_COMMIT_END();
    TxAbort(Self);
    ASSERT(0);

    return 0;
}






/* =============================================================================
 * TxAlloc
 *
 * CCM: simple transactional memory allocation
 * =============================================================================
 */
void*
TxAlloc (Thread* Self, size_t size)
{
    void* ptr = tmalloc_reserve(size);
    if (ptr) {
        tmalloc_append(Self->allocPtr, ptr);
    }

    return ptr;
}


/* =============================================================================
 * TxFree
 *
 * CCM: simple transactional memory de-allocation
 * =============================================================================
 */
void
TxFree (Thread* Self, void* ptr)
{
    tmalloc_append(Self->freePtr, ptr);

    /*
     * We want to lock these to make sure nobody can read them between
     * GVgenerateWV and txSterilize. This ensures future readers will
     * see the updated version number from sterilize().
     *
     * TODO: record in separate log to avoid making the write set large
     */
#  ifdef TL2_EAGER
    TxStore(Self, (volatile intptr_t*)ptr, 0);
#  else /* !TL2_EAGER */
    volatile vwLock* LockFor = PSLOCK(ptr);
#    ifdef TL2_OPTIM_HASHLOG
    HashLog* wrSet = &Self->wrSet;
    long numLog = wrSet->numLog;
    long hash = HASHLOG_HASH(UNS(ptr)) % numLog;
    Log* wr = &wrSet->logs[hash];
#    else /* !TL2_OPTIM_HASHLOG */
    Log* wr = &Self->wrSet;
#    endif /* !TL2_OPTIM_HASHLOG */
    RecordStore(wr, (volatile intptr_t*)ptr, 0, LockFor);
#  endif /* !TL2_EAGER */
}




/* =============================================================================
 *
 * End of tl2.c
 *
 * =============================================================================
 */
