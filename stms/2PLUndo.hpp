/*
 * Copyright 2021
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

// 2PL with naive rw-lock Undo-log
//
// The highest eight bits of the rw-locks are the tid of the writer and the remaining 56 bits
// are reserved one per thread for the read indicator. Unlocking is done with fetch_add().
// No global clock.
// Aborts may occur due to read-write or write-write lock conflicts during the transaction.
// There are not aborts at commit time because there is no read-set validation.
//
// TODO:
// b) Replace throw-catch with setjmp/longjmp
// c) Replace lock with strong trylocks https://dl.acm.org/doi/pdf/10.1145/3178487.3178519
//

namespace twoplundo {

//
// User configurable variables.
// Feel free to change these if you need larger transactions, more allocations per transaction, or more threads.
//

// Maximum number of registered threads that can execute transactions. We only have 56 bits available for the read-indicator
static const int REGISTRY_MAX_THREADS = 56;
// Maximum number of stores in the WriteSet per transaction
static const uint64_t TX_MAX_STORES = 128*1024;
// Maximum number of loads in one transaction
static const uint64_t TX_MAX_LOADS = 128*1024;
// Maximum number of allocations in one transaction
static const uint64_t TX_MAX_ALLOCS = 10*1024;
// Maximum number of deallocations in one transaction
static const uint64_t TX_MAX_RETIRES = 10*1024;

static const int TX_IS_NONE = 0;
static const int TX_IS_READ = 1;
static const int TX_IS_UPDATE = 2;


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


// Two-Phase locking with naive reader-writer lock based on C-RW-WP with read-indicator and tid for writer.
// Each lock is a single word.
struct LockManager {
    // Number of striped locks. _Must_ be a power of 2.
    static const uint64_t NUM_LOCKS = 4*1024*1024;
    // We reserve 8 bits for the tid of the write lock. Use 0 to represent UNLOCKED and the others are tid+1
    static const uint64_t UNLOCKED = 0;
    // Array of locks
    alignas(128) std::atomic<uint64_t>* lockArray;
    // Just padding to prevent false sharing
    alignas(128) uint64_t padding {0};

    LockManager() {
        lockArray = new std::atomic<uint64_t>[NUM_LOCKS];
        for (uint64_t i = 0; i < NUM_LOCKS; i++) lockArray[i].store(0, std::memory_order_relaxed);
    }

    ~LockManager() {
        delete[] lockArray;
    }

    // Function that hashes an address to a lock index.
    // 2^5 => one lock for every half a cache line (32 bytes)
    inline static uint64_t hidx(const void* addr) { return (((uint64_t)(addr) >> 5) & (NUM_LOCKS-1)); }

    // Given a lock value, return the tid of the writer owning it.
    inline static uint64_t getWriteState(uint64_t lock) { return (lock >> 56); }

    inline static uint64_t getReadIndicator(uint64_t lock) { return (lock & 0xFFFFFFFFFFFFFFUL); }

    // Given a lock value, return true if I'm already holding it in read mode
    inline static bool isReadLockedByMe(uint64_t lock, uint64_t tid) { return lock & (1ULL << tid); }

    // Returns true if the read indicator is empty or I'm the only one there
    inline static bool isEmptyRI(uint64_t lock, uint64_t tid) { return !(getReadIndicator(lock) & ~(1ULL << tid)); }

    // Returns true if the lock is already acquired by me in write mode, or read mode, or was successfuly acquired in read mode
    // This function is on the hot-path of the load interposing.
    inline bool tryReadLock(const void* addr, uint64_t tid) {
        uint64_t lidx = hidx(addr);
        uint64_t lock = lockArray[lidx].load(std::memory_order_acquire);
        if (isReadLockedByMe(lock, tid)) return true;
        uint64_t wstate = getWriteState(lock);
        if (wstate == tid+1) return true;
        if (wstate != UNLOCKED) return false;
        lock = lockArray[lidx].fetch_add(1ULL << tid);
        wstate = getWriteState(lock);
        if (wstate == tid+1 || wstate == UNLOCKED) return true;
        lockArray[lidx].fetch_sub(1ULL << tid);
        return false;
    }

    // Returns true if the lock is already acquired by me in write mode.
    // This function is on the hot path of the store interposing.
    inline bool tryWriteLock(const void* addr, uint64_t tid) {
        uint64_t lidx = hidx(addr);
        uint64_t lock = lockArray[lidx].load(std::memory_order_acquire);
        uint64_t wstate = getWriteState(lock);
        if (wstate == tid+1) return true;
        if (wstate != UNLOCKED) return false;
        if (!isEmptyRI(lock, tid)) return false;
        return lockArray[lidx].compare_exchange_strong(lock, getReadIndicator(lock) | ((tid+1) << 56));
    }

    // Unlocks both read and write locks with a single fetch_add()
    inline void unlock(const void *addr, uint64_t tid) {
        uint64_t lidx = hidx(addr);
        uint64_t lock = lockArray[lidx].load(std::memory_order_acquire);
        uint64_t wstate = getWriteState(lock);
        uint64_t decr = 0;
        if (isReadLockedByMe(lock, tid)) decr += 1ULL << tid;
        if (wstate == tid+1) decr += (tid+1) << 56;
        if (decr == 0) return;
        lockArray[lidx].fetch_sub(decr);
    }
};


// In case the transactions aborts, we can rollback our allocations, hiding the type information inside the lambda.
// Sure, we could keep everything in std::function, but this uses less memory.
struct Deletable {
    void* obj {nullptr};         // Pointer to object to be deleted
    void (*reclaim)(void*);      // A wrapper to keep the type of the underlying object
};


struct ReadSet {
    const void*   log[TX_MAX_LOADS];
    uint64_t      numLoads {0};          // Number of loads in the readSet for the current transaction

    inline void reset() {
        numLoads = 0;
    }

    inline void add(const void* addr) {
        log[numLoads++] = addr;
        assert (numLoads != TX_MAX_LOADS);
    }
};


// A single entry in the write-set
struct WriteSetEntry {
    uint64_t*      addr;     // Address of value+sequence to change
    uint64_t       oldVal;   // Desired value to change to
};


// The write-set is a log of the words modified during the transaction.
struct WriteSet {
    WriteSetEntry         log[TX_MAX_STORES];     // Redo log of stores
    uint64_t              numStores {0};          // Number of stores in the writeSet for the current transaction

    // Adds a modification to the undo log
    inline void add(void* addr, uint64_t oldVal) {
        WriteSetEntry* e = &log[numStores++];
        assert(numStores < TX_MAX_STORES);
        e->addr = (uint64_t*)addr;
        e->oldVal = oldVal;
    }

    inline void reset() {
        numStores = 0;
    }
};



// Forward declaration
struct OpData;
// This is used by addToLog() to know which OpDesc instance to use for the current transaction
extern thread_local OpData* tl_opdata;
// This is used by tmtype::load() to figure out if it needs to save a load on the read-set or not
extern thread_local int tl_tx_type;


// Its purpose is to hold thread-local data
struct OpData {
    uint64_t              tid;
    WriteSet              writeSet;                    // The write set
    ReadSet               readSet;                     // The read set
    uint64_t              nestedTrans {0};             // Thread-local: Number of nested transactions
    uint64_t              myrand;
    uint64_t              numAborts {0};
    uint64_t              numCommits {0};
    uint64_t              numFrees {0};                // Number of calls to tmDelete() in this transaction (owner thread only)
    void*                 flog[TX_MAX_RETIRES];        // List of retired objects during the transaction (owner thread only)
    uint64_t              numAllocs {0};               // Number of calls to tmNew() in this transaction (owner thread only)
    Deletable             alog[TX_MAX_ALLOCS];         // List of newly allocated objects during the transaction (owner thread only)
};



// Used to identify aborted transactions
struct AbortedTx {};
static constexpr AbortedTx AbortedTxException {};

class STM;
extern STM gSTM;


class STM {

private:
    OpData                 *opDesc;
public:
    struct tmbase : public twoplundo::tmbase { };

    LockManager             lockManager {};

    STM() {
        opDesc = new OpData[REGISTRY_MAX_THREADS];
        for (int i=0; i < REGISTRY_MAX_THREADS; i++) opDesc[i].tid = i;
        for (int i=0; i < REGISTRY_MAX_THREADS; i++) opDesc[i].myrand = (i+1)*12345678901234567ULL;
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
    }

    static std::string className() { return "2PL-Undo"; }

    inline void beginTx(OpData* myd) {
        // Clear the logs of the previous transaction
        myd->numAllocs = 0;
        myd->numFrees = 0;
        myd->writeSet.reset();
        myd->readSet.reset();
    }

    // Once we get to the commit stage, there is no longer the possibility of aborts
    inline bool endTx(OpData* myd, const int tid) {
        // Unlock the read locks
        for (uint64_t i=0; i < myd->readSet.numLoads; i++) lockManager.unlock(myd->readSet.log[i], tid);
        // Unlock write locks
        for (uint64_t i=0; i < myd->writeSet.numStores; i++) lockManager.unlock(myd->writeSet.log[i].addr, tid);
        // Execute de-allocations
        for (uint64_t i = 0; i < myd->numFrees; i++) std::free(myd->flog[i]);
        myd->numCommits++;
        return true;
    }

    // This is called by the user to abort a transaction, or when a
    // tmtype::load()/store() in the user code sees a new transaction (throws an exception).
    inline void abortTransaction(OpData* myd, const int tid) {
        // Undo the modifications in reverse order
        for (int i=myd->writeSet.numStores-1; i >= 0; i--) {
            WriteSetEntry* entry = &myd->writeSet.log[i];
            *entry->addr = entry->oldVal;
        }
        // Unlock the read locks
        for (int i=0; i < myd->readSet.numLoads; i++) lockManager.unlock(myd->readSet.log[i], tid);
        // Unlock write locks
        for (int i=0; i < myd->writeSet.numStores; i++) lockManager.unlock(myd->writeSet.log[i].addr, tid);
        // Undo allocations
        for (unsigned i = 0; i < myd->numAllocs; i++) myd->alog[i].reclaim(myd->alog[i].obj);
        myd->numAborts++;
    }

    // Transaction with a non-void return
    template<typename R, typename F> R transaction(F&& func, int txType=TX_IS_UPDATE) {
        const int tid = ThreadRegistry::getTID();
        OpData* myd = &opDesc[tid];
        if (myd->nestedTrans > 0) return func();
        tl_opdata = myd;
        tl_tx_type = txType;
        ++myd->nestedTrans;
        R retval {};
        uint64_t attempt = 0;
        while (true) {
            backoff(myd, attempt);
            beginTx(myd);
            try {
                retval = func();
            } catch (AbortedTx&) {
                attempt++;
                abortTransaction(myd, tid);
                continue;
            }
            if (endTx(myd, tid)) break;
        }
        tl_opdata = nullptr;
        tl_tx_type = TX_IS_NONE;
        --myd->nestedTrans;
        return retval;
    }

    // Same as above, but returns void
    template<typename F> void transaction(F&& func, int txType=TX_IS_UPDATE) {
        const int tid = ThreadRegistry::getTID();
        OpData* myopd = &opDesc[tid];
        if (myopd->nestedTrans > 0) {
            func();
            return ;
        }
        tl_opdata = myopd;
        tl_tx_type = txType;
        ++myopd->nestedTrans;
        uint64_t attempt = 0;
        while(true) {
            backoff(myopd, attempt);
            beginTx(myopd);
            try {
                func();
            } catch (AbortedTx&) {
                attempt++;
                abortTransaction(myopd, tid);
                continue;
            }
            if (endTx(myopd, tid)) break;
        }
        tl_opdata = nullptr;
        tl_tx_type = TX_IS_NONE;
        --myopd->nestedTrans;
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
        if (ptr == nullptr) return nullptr;
        std::memset(ptr, 0, size);
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

private:
    // Random number generator used by the backoff scheme
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


// T is typically a pointer to a node, but it can be integers or other stuff, as long as it fits in 64 bits
template<typename T> struct tmtype {
    T val;   // Unlike TL2, 'val' doesn't need to be atomic

    tmtype() { }

    tmtype(T initVal) {
        pstore(initVal);
    }

    // Casting operator
    operator T() {
        return pload();
    }

    // Casting to const
    operator T() const {
        return pload();
    }

    // Prefix increment operator: ++x
    void operator++ () {
        pstore(pload()+1);
    }

    // Prefix decrement operator: --x
    void operator-- () {
        pstore(pload()-1);
    }

    void operator++ (int) {
        pstore(pload()+1);
    }

    void operator-- (int) {
        pstore(pload()-1);
    }

    // Equals operator: first downcast to T and then compare
    bool operator == (const T& otherval) const {
        return pload() == otherval;
    }

    // Difference operator: first downcast to T and then compare
    bool operator != (const T& otherval) const {
        return pload() != otherval;
    }

    // Relational operators
    bool operator < (const T& rhs) {
        return pload() < rhs;
    }
    bool operator > (const T& rhs) {
        return pload() > rhs;
    }
    bool operator <= (const T& rhs) {
        return pload() <= rhs;
    }
    bool operator >= (const T& rhs) {
        return pload() >= rhs;
    }

    // Operator arrow ->
    T operator->() {
        return pload();
    }

    // Copy constructor
    tmtype<T>(const tmtype<T>& other) {
        pstore(other.pload());
    }

    // Assignment operator from an tmtype
    tmtype<T>& operator=(const tmtype<T>& other) {
        pstore(other.pload());
        return *this;
    }

    // Assignment operator from a value
    tmtype<T>& operator=(T value) {
        pstore(value);
        return *this;
    }

    // Operator &
    T* operator&() {
        return (T*)&val;
    }

    // TODO: lock all the words of this tmtype, not just the first one
    inline void pstore(T newVal) {
        OpData* const myd = tl_opdata;
        if (myd == nullptr) { // Check if we're outside a transaction
            val = newVal;
            return;
        }
        if (gSTM.lockManager.tryWriteLock(&val, myd->tid)) {
            myd->writeSet.add(&val, (uint64_t)val);
            val = newVal;
            return;
        }
        throw AbortedTxException;
    }

    // TODO: lock all the words of this tmtype, not just the first one
    inline T pload() const {
        OpData* const myd = tl_opdata;
        // Check if we're outside a transaction
        if (myd == nullptr) return val;
        if (gSTM.lockManager.tryReadLock(&val, myd->tid)) {
            myd->readSet.add(&val);
            return val;
        }
        throw AbortedTxException;
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

//
// Place these in a .cpp if you include this header from different files (compilation units)
//
STM gSTM {};
// Thread-local data of the current ongoing transaction
thread_local OpData* tl_opdata {nullptr};
// Represents one of three possible states
thread_local int tl_tx_type { TX_IS_NONE };
// Global/singleton to hold all the thread registry functionality
ThreadRegistry gThreadRegistry {};
// This is where every thread stores the tid it has been assigned when it calls getTID() for the first time.
// When the thread dies, the destructor of ThreadCheckInCheckOut will be called and de-register the thread.
thread_local ThreadCheckInCheckOut tl_tcico {};
// Helper function for thread de-registration
void thread_registry_deregister_thread(const int tid) {
    gThreadRegistry.deregister_thread(tid);
}


}

