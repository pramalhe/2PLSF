/*
 * Copyright 2021-2022
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#pragma once

#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>
#include <functional>
#include <cstring>      // std::memcpy()
#include <csetjmp>      // Needed by sigjmp_buf

// 2PL with Distributed rw-lock Undo-log - Starvation-Free (with Wait-Or-Die)
// This is the same as 2PLUndo except we use a rw-lock with distributed read-indicator.
// The only code difference is on the LockManager class.
// We don't support ranges/strings.
//
// Aborts may occur due to read-write or write-write lock conflicts during the transaction.
// There are not aborts at commit time because there is no read-set validation.
//
// TODO:
// - Replace UNLOCKED 0 with UNLOCKED = 2^16-1 and then the ownership of the write-locks to be tid instead of tid+1
// - Add support for ranges. Will be needed if we want to integrate with DBx1000
//

static const bool DEBUG_2PLSF = false;

namespace twoplundodist {

//
// User configurable variables.
// Feel free to change these if you need larger transactions, more allocations per transaction, or more threads.
//

// Maximum number of registered threads that can execute transactions.
static const int REGISTRY_MAX_THREADS = 256;
// Maximum number of allocations in one transaction
static const uint64_t TX_MAX_ALLOCS = 10*1024;
// Maximum number of deallocations in one transaction
static const uint64_t TX_MAX_RETIRES = 10*1024;

static const int TX_IS_NONE = 0;
static const int TX_IS_READ = 1;
static const int TX_IS_UPDATE = 2;

static const uint64_t NO_TIMESTAMP = 0xFFFFFFFFFFFFFFFFULL;

#ifdef __x86_64__
#define Pause()  __asm__ __volatile__ ( "pause" : : : )
#else
#define Pause() {}  // On non-x86 we simply spin
#endif

//
// Thread Registry stuff
//
extern void thread_registry_deregister_thread(const int tid);

// An helper class to do the checkin and checkout of the thread registry
struct ThreadCheckInCheckOut {
    static const int NOT_ASSIGNED = -1;
    int tid { NOT_ASSIGNED };
    ~ThreadCheckInCheckOut() {
        if (tid == NOT_ASSIGNED) return;
        thread_registry_deregister_thread(tid);
    }
};

extern thread_local ThreadCheckInCheckOut tl_tcico;

// Forward declaration of global/singleton instance
class ThreadRegistry;
extern ThreadRegistry gThreadRegistry;

/*
 * <h1> Registry for threads </h1>
 *
 * This is singleton type class that allows assignement of a unique id to each thread.
 * The first time a thread calls ThreadRegistry::getTID() it will allocate a free slot in 'usedTID[]'.
 * This tid wil be saved in a thread-local variable of the type ThreadCheckInCheckOut which
 * upon destruction of the thread will call the destructor of ThreadCheckInCheckOut and free the
 * corresponding slot to be used by a later thread.
 */
class ThreadRegistry {
private:
    alignas(128) std::atomic<bool>      usedTID[REGISTRY_MAX_THREADS];   // Which TIDs are in use by threads
    alignas(128) std::atomic<int>       maxTid {-1};                     // Highest TID (+1) in use by threads

public:
    ThreadRegistry() {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            usedTID[it].store(false, std::memory_order_relaxed);
        }
    }

    // Progress condition: wait-free bounded (by the number of threads)
    int register_thread_new(void) {
        for (int tid = 0; tid < REGISTRY_MAX_THREADS; tid++) {
            if (usedTID[tid].load(std::memory_order_acquire)) continue;
            bool unused = false;
            if (!usedTID[tid].compare_exchange_strong(unused, true)) continue;
            // Increase the current maximum to cover our thread id
            int curMax = maxTid.load();
            while (curMax <= tid) {
                maxTid.compare_exchange_strong(curMax, tid+1);
                curMax = maxTid.load();
            }
            tl_tcico.tid = tid;
            return tid;
        }
        std::cout << "ERROR: Too many threads, registry can only hold " << REGISTRY_MAX_THREADS << " threads\n";
        assert(false);
    }

    // Progress condition: wait-free population oblivious
    inline void deregister_thread(const int tid) {
        usedTID[tid].store(false, std::memory_order_release);
    }

    // Progress condition: wait-free population oblivious
    static inline uint64_t getMaxThreads(void) {
        return gThreadRegistry.maxTid.load(std::memory_order_acquire);
    }

    // Progress condition: wait-free bounded (by the number of threads)
    static inline int getTID(void) {
        int tid = tl_tcico.tid;
        if (tid != ThreadCheckInCheckOut::NOT_ASSIGNED) return tid;
        return gThreadRegistry.register_thread_new();
    }
};


// Needed by our microbenchmarks
struct tmbase { };


// In case the transactions aborts, we can rollback our allocations, hiding the type information inside the lambda.
// Sure, we could keep everything in std::function, but this uses less memory.
struct Deletable {
    void* obj {nullptr};         // Pointer to object to be deleted
    void (*reclaim)(void*);      // A wrapper to keep the type of the underlying object
};


// Number of rw-locks. _Must_ be a power of 2.
static const uint64_t NUM_RWL = 4*1024*1024;
// Number of read-indicators per wstate. At least 1 but more if we want to share a read-indicator
// across multiple rw-locks, as doing so will make it faster to acquire read-locks on consecutive data.
// Preferably this should be a power of 2 so that the division can be optimized into a shift.
static const uint32_t RI_PER_RWL = 1;
// Number of read indicators.
static const uint64_t NUM_RI = NUM_RWL;
// Number of words needed for the read-indicators
static const uint64_t NUM_RI_WORDS = NUM_RI*REGISTRY_MAX_THREADS/64;
// We reserve 16 bits for the tid of the write lock. Use 0 to represent UNLOCKED and the others are tid+1
static const uint16_t UNLOCKED = 0;

// Function that hashes an address to a write-indicator index.
// 2^5 => one lock for every 32 bytes (half a cache line)
inline static uint64_t addr2writeIdx(const void* addr) { return (((uint64_t)(addr) >> 5) & (NUM_RWL-1)); }

// This function converts a widx to a ridx
inline static uint32_t writeIdx2readIdx(uint32_t widx, int16_t tid) {
    // Get the word of the ri based on the widx and the tid
    return tid*NUM_RI_WORDS/REGISTRY_MAX_THREADS + ((widx/RI_PER_RWL)/64);
}

inline static uint64_t writeIdx2readMask(uint32_t widx) {
    return (1ULL << ((widx)%64));
}

// This is a set of the read-locks acquired. Works with ranges.
// Because of the ranges, we split the insertion of a new entry into two steps:
// 1) addEntry() we add/replace the last entry in the read-set
// 2) advanceEntry() If the lock was taken, we advance the log, otherwise, it means we already read-locked this before.
// By having these two functions, we can separate the read-set from the Lock Manager.
struct ReadSet {
    struct ReadSetEntry {
        uint32_t widx;
    };

    // We pre-allocate a read-set with this many entries and if more are needed,
    // we grow them dynamically during the transaction and then delete them when
    // reset() is called on beginTx().
    static const int64_t MAX_READ_SET_ENTRIES = 64*1024;

    ReadSetEntry            entries[MAX_READ_SET_ENTRIES];
    uint32_t                size {0};          // Number of loads in the readSet for the current transaction

    void reset() {
        size = 0;
    }

    // TODO: make a proper growable read-set
    inline void addEntry(const void* addr) {
        // If you see this assert(), then increase MAX_READ_SET_ENTRIES
        assert(size != MAX_READ_SET_ENTRIES);
        entries[size].widx = addr2writeIdx(addr);
        size++;
    }
};


// The write-set is a log of the words modified during the transaction.
struct WriteSet {
    // We pre-allocate a write-set with this many entries and if more are needed,
    // we grow them dynamically during the transaction and then delete them when
    // reset() is called on beginTx().
    static const int64_t MAX_WRITE_SET_ENTRIES = 64*1024;

    struct WriteSetEntry {
        void*     addr;
        uint64_t  data;  // old value
    };

    WriteSetEntry entries[MAX_WRITE_SET_ENTRIES];     // undo log of stores
    int64_t       size {0};          // Number of stores in the writeSet for the current transaction

    inline void reset() {
        size = 0;
    }

    // Adds a modification to the undo log
    inline void addEntry(const void* addr) {
        assert(size != MAX_WRITE_SET_ENTRIES);
        entries[size].addr = (void*)addr;
        entries[size].data = *(uint64_t*)addr;
        size++;
    }

    inline void rollbackSingleEntry(int32_t i) {
        WriteSetEntry* entry = &entries[i];
        *(uint64_t*)entry->addr = entry->data;
    }
};




// Forward declaration
struct OpData;
// This is used by addToLog() to know which OpDesc instance to use for the current transaction
extern thread_local OpData* tl_opdata;
// This is used by tmtype::load() to figure out if it needs to save a load on the read-set or not


// Its purpose is to hold thread-local data
struct OpData {
    std::jmp_buf          env;
    uint64_t              attempt {0};
    uint64_t              tid;
    WriteSet              writeSet;                    // The write set
    ReadSet               readSet;                     // The read set
    uint64_t              myTS {NO_TIMESTAMP};
    uint64_t              oTS {NO_TIMESTAMP};
    uint16_t              otid {REGISTRY_MAX_THREADS};
    uint64_t              myrand;
    uint64_t              numAborts {0};
    uint64_t              numCommits {0};
    uint64_t              numFrees {0};                // Number of calls to tmDelete() in this transaction (owner thread only)
    void*                 flog[TX_MAX_RETIRES];        // List of retired objects during the transaction (owner thread only)
    uint64_t              numAllocs {0};               // Number of calls to tmNew() in this transaction (owner thread only)
    Deletable             alog[TX_MAX_ALLOCS];         // List of newly allocated objects during the transaction (owner thread only)
};


[[noreturn]] void abortTx(OpData* myd);

class STM;
extern STM gSTM;


// Two-Phase locking with Distributed reader-writer lock based on C-RW-WP with read-indicator and tid for writer.
// Each read-indicator cover multiple write indicators. In other words, the read lock protects multiple rw-locks.
// This was made to save memory, otherwise we would need too much space for the read-indicator
class STM {

public:
    struct tmbase : public twoplundodist::tmbase { };

    static const int CLPAD = 128/sizeof(uint64_t);
    // Contains thread-local metadata
    alignas(128) OpData                *opDesc;
    // Global clock
    alignas(128) std::atomic<uint64_t>  conflictClock {1};
    // Array of write-indicators
    alignas(128) std::atomic<uint64_t>* wlocks;
    // Array of read-indicators
    alignas(128) std::atomic<uint64_t>* readIndicators;


    STM() {
        opDesc = new OpData[REGISTRY_MAX_THREADS];
        for (int i=0; i < REGISTRY_MAX_THREADS; i++) opDesc[i].tid = i;
        for (int i=0; i < REGISTRY_MAX_THREADS; i++) opDesc[i].myrand = (i+1)*12345678901234567ULL;
        wlocks = new std::atomic<uint64_t>[NUM_RWL];
        for (uint64_t i = 0; i < NUM_RWL; i++) wlocks[i].store(0, std::memory_order_relaxed);
        readIndicators = new std::atomic<uint64_t>[NUM_RI_WORDS];
        for (uint64_t i = 0; i < NUM_RI_WORDS; i++) readIndicators[i].store(0, std::memory_order_relaxed);
    }

    ~STM() {
        uint64_t totalAborts = 0;
        uint64_t totalCommits = 0;
        for (int i=0; i < REGISTRY_MAX_THREADS; i++) {
            totalAborts += opDesc[i].numAborts;
            totalCommits += opDesc[i].numCommits;
        }
        printf("totalAborts=%ld  totalCommits=%ld  abortRatio=%.1f%% \n", totalAborts, totalCommits, 100.*totalAborts/(1+totalCommits));
        delete[] opDesc;
        delete[] wlocks;
        delete[] readIndicators;
    }

    static std::string className() { return "2PL-Undo-Dist"; }

    inline void beginTx(OpData* myd) {
        // Clear the logs of the previous transaction
        myd->numAllocs = 0;
        myd->numFrees = 0;
        myd->writeSet.reset();
        myd->readSet.reset();
        if (myd->attempt > 0) backoff(myd, myd->attempt);
        myd->attempt++;
    }

    // Once we get to the commit stage, there is no longer the possibility of aborts
    inline void endTx(OpData* myd, const int tid) {
        // Unlock write locks
        for (uint64_t i=0; i < myd->writeSet.size; i++) unlockWrite(myd->writeSet.entries[i].addr, tid);
        // Unlock the read locks
        unlockAllReadLocks(myd, tid);
        // Execute de-allocations
        for (uint64_t i = 0; i < myd->numFrees; i++) std::free(myd->flog[i]);
        myd->numCommits++;
        myd->attempt = 0;
        tl_opdata = nullptr;
    }

    inline void abortTx(OpData* myd, bool enableRollback=true) {
        if (DEBUG_2PLSF) printf("abortTxn() writeSet.size=%ld\n", myd->writeSet.size);
        // Undo the modifications in reverse order
        if (enableRollback && myd->writeSet.size != 0) {
            for (int32_t i=myd->writeSet.size-1; i >= 0; i--) myd->writeSet.rollbackSingleEntry(i);
        }
        // Unlock write locks
        for (int i=0; i < myd->writeSet.size; i++) unlockWrite(myd->writeSet.entries[i].addr, myd->tid);
        // Unlock the read locks
        unlockAllReadLocks(myd, myd->tid);
        // Undo allocations
        if (DEBUG_2PLSF) printf("abortTx(): undoing %ld allocations\n", myd->numAllocs);
        for (unsigned i = 0; i < myd->numAllocs; i++) myd->alog[i].reclaim(myd->alog[i].obj);
        myd->numAborts++;
    }

    // Transaction with a non-void return
    template<typename R, typename F> R transaction(F&& func, int txType=TX_IS_UPDATE) {
        const int tid = ThreadRegistry::getTID();
        OpData* myd = &opDesc[tid];
        if (tl_opdata != nullptr) return func();
        tl_opdata = myd;
        setjmp(myd->env);
        beginTx(myd);
        R retval = func();
        endTx(myd, tid);
        return retval;
    }

    // Same as above, but returns void
    template<typename F> void transaction(F&& func, int txType=TX_IS_UPDATE) {
        const int tid = ThreadRegistry::getTID();
        OpData* myd = &opDesc[tid];
        if (tl_opdata != nullptr) {
            func();
            return ;
        }
        tl_opdata = myd;
        setjmp(myd->env);
        beginTx(myd);
        func();
        endTx(myd, tid);
    }

    // It's silly that these have to be static, but we need them for the (SPS) benchmarks due to templatization
    template<typename R, typename F> static R updateTx(F&& func) { return gSTM.transaction<R>(func, TX_IS_UPDATE); }
    template<typename R, typename F> static R readTx(F&& func) { return gSTM.transaction<R>(func, TX_IS_READ); }
    template<typename F> static void updateTx(F&& func) { gSTM.transaction(func, TX_IS_UPDATE); }
    template<typename F> static void readTx(F&& func) { gSTM.transaction(func, TX_IS_READ); }

    // When inside a transaction, the user can't call "new" directly because if
    // the transaction fails, it would leak the memory of these allocations.
    // Instead, we provide an allocator that keeps pointers to these objects
    // in a log, and in the event of a failed commit of the transaction, it will
    // delete the objects so that there are no leaks.
    template <typename T, typename... Args> static T* tmNew(Args&&... args) {
        T* ptr = (T*)std::malloc(sizeof(T));
        OpData* myd = tl_opdata;
        if (myd != nullptr) {
            assert(myd->numAllocs != TX_MAX_ALLOCS);
            Deletable& del = myd->alog[myd->numAllocs++];
            del.obj = ptr;
            del.reclaim = [](void* obj) { std::free(obj); };
            new (ptr) T(std::forward<Args>(args)...);  // new placement
            del.reclaim = [](void* obj) { static_cast<T*>(obj)->~T(); std::free(obj); };
        } else {
            new (ptr) T(std::forward<Args>(args)...);  // new placement
        }
        return ptr;
    }

    // The user can not directly delete objects in the transaction because the
    // transaction may fail and needs to be retried and other threads may be
    // using those objects.
    // Instead, it has to call retire() for the objects it intends to delete.
    // The retire() puts the objects in the rlog, and only when the transaction
    // commits, the objects are put in the Hazard Eras retired list.
    // The del.delEra is filled in retireRetiresFromLog().
    // TODO: Add static_assert to check if T is of tmbase
    template<typename T> static void tmDelete(T* obj) {
        if (obj == nullptr) return;
        obj->~T(); // Execute destructor as part of the current transaction
        OpData* myopd = tl_opdata;
        if (myopd == nullptr) {
            std::free(obj);  // Outside a transaction, just delete the object
            return;
        }
        assert(myopd->numFrees != TX_MAX_RETIRES);
        myopd->flog[myopd->numFrees++] = obj;
    }

    // We snap a tmbase at the beginning of the allocation
    static void* tmMalloc(size_t size) {
        void* ptr = std::malloc(size);
        OpData* myopd = tl_opdata;
        if (myopd != nullptr) {
            assert(myopd->numAllocs != TX_MAX_ALLOCS);
            Deletable& del = myopd->alog[myopd->numAllocs++];
            del.obj = ptr;
            del.reclaim = [](void* obj) { std::free(obj); };
        }
        return ptr;
    }

    // We assume there is a tmbase allocated in the beginning of the allocation
    static void tmFree(void* obj) {
        if (obj == nullptr) return;
        OpData* myopd = tl_opdata;
        if (myopd == nullptr) {
            std::free(obj);  // Outside a transaction, just free the object
            return;
        }
        assert(myopd->numFrees != TX_MAX_RETIRES);
        myopd->flog[myopd->numFrees++] = (tmbase*)obj;
    }
/*
    static void* tmMemcpy(void* dst, const void* src, std::size_t count) {
        OpData* const myd = tl_opdata;
        if (myd != nullptr) {
            gSTM.tryWaitReadLock(myd, src, count);
            gSTM.tryWaitWriteLock(myd, dst, count);
        }
        return std::memcpy(dst, src, count);
    }

    static int tmMemcmp(const void* lhs, const void* rhs, std::size_t count) {
        OpData* const myd = tl_opdata;
        if (myd != nullptr) {
            gSTM.tryWaitReadLock(myd, (void*)lhs, count);
            gSTM.tryWaitReadLock(myd, (void*)rhs, count);
        }
        return std::memcmp(lhs, rhs, count);
    }

    static int tmStrcmp(const char* lhs, const char* rhs, std::size_t count) {
        OpData* const myd = tl_opdata;
        if (myd != nullptr) {
            gSTM.tryWaitReadLock(myd, (void*)lhs, count);
            gSTM.tryWaitReadLock(myd, (void*)rhs, count);
        }
        return std::strncmp(lhs, rhs, count);
    }

    static void* tmMemset(void* dst, int ch, std::size_t count) {
        OpData* const myd = tl_opdata;
        if (myd != nullptr) gSTM.tryWaitWriteLock(myd, dst, count);
        return std::memset(dst, ch, count);
    }
*/

    inline bool tryWaitReadLock(OpData* myd, const void* addr) {
        uint32_t widx = addr2writeIdx(addr);
        // Get the word of the ri based on the widx and the tid
        uint32_t ridx = writeIdx2readIdx(widx, myd->tid);
        // Don't set the bit if it's already set
        uint64_t ri = readIndicators[ridx].load(std::memory_order_relaxed);
        uint64_t newri = (ri | writeIdx2readMask(widx));
        // If we already arrived, it means we have the read-lock
        if (newri == ri) return true;
        myd->readSet.addEntry(addr);
        // Arrive on the read-indicator. Exchange is faster than fetch_add() on x86
        readIndicators[ridx].exchange(newri);
        // Check the writer's cohort lock state
        uint16_t wstate = wlocks[widx].load(std::memory_order_acquire);
        if (wstate == UNLOCKED || wstate == myd->tid+1) return true;
        // Looks like there is a writer holding this lock. Enter slow-path
        return false;
    }

    // Returns true if the lock is already acquired by me in write mode.
    // This function is on the hot path of the store interposing.
    inline bool tryWaitWriteLock(OpData* myd, const void* addr) {
        uint32_t widx = addr2writeIdx(addr);
        uint64_t wstate = wlocks[widx].load(std::memory_order_acquire);
        // Check if it's already write-locked by me, or if can take the lock
        if (wstate == myd->tid+1)  {
            myd->writeSet.addEntry(addr);
            return true;
        }
        if (wstate == UNLOCKED && wlocks[widx].compare_exchange_strong(wstate, myd->tid+1)) {
            myd->writeSet.addEntry(addr);
            if (isEmpty(widx, myd->tid)) return true;
        }
        return false;
    }

    // Unlocks both write locks with store release
    inline void unlockWrite(const void *addr, uint16_t tid) {
        uint64_t widx = addr2writeIdx(addr);
        uint64_t wstate = wlocks[widx].load(std::memory_order_relaxed);
        // If write-locked by me, unlock it
        if (wstate == tid+1) wlocks[widx].store(UNLOCKED, std::memory_order_release);
    }

    // Unlocks a read-lock with store-release
    inline void unlockRead(uint32_t widx, uint16_t tid) {
        // Get the word of the ri based on the widx and the tid
        uint32_t ridx = writeIdx2readIdx(widx, tid);
        // Check if it's already unlocked
        uint64_t ri = readIndicators[ridx].load(std::memory_order_relaxed);
        uint64_t rmask = writeIdx2readMask(widx);
        if ((ri & rmask) == 0) return;
        readIndicators[ridx].store(ri & (~rmask), std::memory_order_release);
    }

    // Helper function for rollback()
    inline bool rangeIsWriteLockedByMe(void* addr, uint64_t tid) {
        return true;
        /*
        uint32_t widxBeg = addr2writeIdx(addr);
        uint32_t widxEnd = addr2writeIdx((uint8_t*)addr + length-1);
        assert(widxEnd >= widxBeg); // TODO: deal with wraparounds?
        for (uint32_t widx = widxBeg; widx <= widxEnd; widx++) {
            if (wlocks[widx].load(std::memory_order_relaxed) != tid+1) return false;
        }
        return true;
        */
    }

    void unlockAllReadLocks(OpData* myd, const int tid) {
        for (uint32_t i=0; i < myd->readSet.size; i++) {
            unlockRead(myd->readSet.entries[i].widx, tid);
        }
    }

private:

    // Return true if the read-indicator is empty. Skip my own tid.
    // This is optimized to not have any branches inside the loop.
    inline bool isEmpty(uint32_t widx, uint32_t tid) {
        const uint32_t maxThreads = gThreadRegistry.getMaxThreads();
        uint64_t rmask = writeIdx2readMask(widx);
        uint64_t sum = 0;
        for (uint32_t itid = 0; itid < maxThreads; itid++) {
            uint32_t ridx = writeIdx2readIdx(widx, itid);
            uint64_t ri = readIndicators[ridx].load(std::memory_order_acquire);
            sum += ((ri >> (widx%64)) & 1);
        }
        uint64_t ri = readIndicators[writeIdx2readIdx(widx, tid)].load(std::memory_order_relaxed);
        sum -= ((ri >> (widx%64)) & 1);
        return sum == 0;
    }

    inline uint64_t marsagliaXORV(uint64_t x) {
        if (x == 0) x = 1;
        x ^= x << 6;
        x ^= x >> 21;
        x ^= x << 7;
        return x;
    }

    // Backoff for a random amount of steps, quadratic with the number of attempts
    inline void backoff(OpData* myopd, uint64_t attempt) {
        if (attempt < 2) return;
        if (attempt == 10000) printf("Ooops, looks like we're stuck attempt=%ld\n", attempt);
        myopd->myrand = marsagliaXORV(myopd->myrand);
        uint64_t stall = myopd->myrand & 0xFF;
        stall += attempt*attempt >> 3;
        stall *= 8;
        std::atomic<uint64_t> iter {0};
        while (iter.load() < stall) iter.store(iter.load()+1);
    }
};


// T can be any 64 bit type
template<typename T> struct tmtype {
    T val;

    tmtype() { }
    tmtype(T initVal) { pstore(initVal); }
    // Casting operator
    operator T() { return pload(); }
    // Casting to const
    operator T() const { return pload(); }
    // Prefix increment operator: ++x
    void operator++ () { pstore(pload()+1); }
    // Prefix decrement operator: --x
    void operator-- () { pstore(pload()-1); }
    void operator++ (int) { pstore(pload()+1); }
    void operator-- (int) { pstore(pload()-1); }
    tmtype<T>& operator+= (const T& rhs) { pstore(pload() + rhs); return *this; }
    tmtype<T>& operator-= (const T& rhs) { pstore(pload() - rhs); return *this; }
    // Equals operator
    template <typename Y, typename = typename std::enable_if<std::is_convertible<Y, T>::value>::type>
    bool operator == (const tmtype<Y> &rhs) { return pload() == rhs; }
    // Difference operator: first downcast to T and then compare
    template <typename Y, typename = typename std::enable_if<std::is_convertible<Y, T>::value>::type>
    bool operator != (const tmtype<Y> &rhs) { return pload() != rhs; }
    // Relational operators
    template <typename Y, typename = typename std::enable_if<std::is_convertible<Y, T>::value>::type>
    bool operator < (const tmtype<Y> &rhs) { return pload() < rhs; }
    template <typename Y, typename = typename std::enable_if<std::is_convertible<Y, T>::value>::type>
    bool operator > (const tmtype<Y> &rhs) { return pload() > rhs; }
    template <typename Y, typename = typename std::enable_if<std::is_convertible<Y, T>::value>::type>
    bool operator <= (const tmtype<Y> &rhs) { return pload() <= rhs; }
    template <typename Y, typename = typename std::enable_if<std::is_convertible<Y, T>::value>::type>
    bool operator >= (const tmtype<Y> &rhs) { return pload() >= rhs; }
    // Operator arrow ->
    T operator->() { return pload(); }
    // Copy constructor
    tmtype<T>(const tmtype<T>& other) { pstore(other.pload()); }
    // Operator &
    T* operator&() { return (T*)this; }

    // Assignment operator from a persist<T>
    tmtype<T>& operator=(const tmtype<T>& other) {
        pstore(other.pload());
        return *this;
    }

    // Assignment operator from a value
    tmtype<T>& operator=(T value) {
        pstore(value);
        return *this;
    }

    inline void pstore(T newVal) {
        OpData* const myd = tl_opdata;
        if (myd == nullptr || gSTM.tryWaitWriteLock(myd, &val)) {
            val = newVal;
            return;
        }
        abortTx(myd);
    }

    inline T pload() const {
        OpData* const myd = tl_opdata;
        // Check if we're outside a transaction
        if (myd == nullptr) return val;
        if (!gSTM.tryWaitReadLock(myd, &val)) abortTx(myd);
        return val;
    }
};



//
// Wrapper methods to the global TM instance. The user should use these:
//
template<typename R, typename F> static R updateTx(F&& func) { return gSTM.transaction<R>(func, TX_IS_UPDATE); }
template<typename R, typename F> static R readTx(F&& func) { return gSTM.transaction<R>(func, TX_IS_READ); }
template<typename F> static void updateTx(F&& func) { gSTM.transaction(func, TX_IS_UPDATE); }
template<typename F> static void readTx(F&& func) { gSTM.transaction(func, TX_IS_READ); }
template<typename T, typename... Args> T* tmNew(Args&&... args) { return STM::tmNew<T>(args...); }
template<typename T> void tmDelete(T* obj) { STM::tmDelete<T>(obj); }
static void* tmMalloc(size_t size) { return STM::tmMalloc(size); }
static void tmFree(void* obj) { STM::tmFree(obj); }

// These are used by DBx1000
static inline bool tryReadLock(const void* addr, size_t length) { assert(tl_opdata != nullptr); return gSTM.tryWaitReadLock(tl_opdata, addr); }
static inline bool tryWriteLock(const void* addr, size_t length) { assert(tl_opdata != nullptr); return gSTM.tryWaitWriteLock(tl_opdata, addr); }
static void beginTxn() {
    const int tid = ThreadRegistry::getTID();
    OpData* myd = &gSTM.opDesc[tid];
    if (tl_opdata != nullptr) return; // This handles nesting
    tl_opdata = myd;
    gSTM.beginTx(myd);
};
static void endTxn() { gSTM.endTx(tl_opdata, ThreadRegistry::getTID()); };
static void abortTxn(bool enableRollback) { OpData* myd = tl_opdata; assert(myd != nullptr); gSTM.abortTx(myd, enableRollback); }


#ifndef INCLUDED_FROM_MULTIPLE_CPP
//
// Place these in a .cpp if you include this header from different files (compilation units)
//
STM gSTM {};
// Thread-local data of the current ongoing transaction
thread_local OpData* tl_opdata {nullptr};
// Global/singleton to hold all the thread registry functionality
ThreadRegistry gThreadRegistry {};
// This is where every thread stores the tid it has been assigned when it calls getTID() for the first time.
// When the thread dies, the destructor of ThreadCheckInCheckOut will be called and de-register the thread.
thread_local ThreadCheckInCheckOut tl_tcico {};
// Helper function for thread de-registration
void thread_registry_deregister_thread(const int tid) {
    gThreadRegistry.deregister_thread(tid);
}


[[noreturn]] void __attribute__ ((noinline)) abortTx(OpData* myd) {
    gSTM.abortTx(myd);
    std::longjmp(myd->env, 1);
}
#endif // INCLUDED_FROM_MULTIPLE_CPP

}

