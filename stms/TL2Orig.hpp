/*
 * Copyright 2018-2020
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Andreia Correia <andreia.veiga@unine.ch>
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


namespace tl2orig {

// Cleanup defines from the other include of ESTM or tinySTM
#undef TXTYPE
#undef TXPARAM
#undef TXPARAMS
#undef TXARG
#undef TXARGS

#undef STM_THREAD_T
#undef STM_SELF
#undef STM_RO_FLAG
#undef STM_MALLOC
#undef STM_FREE
#undef STM_JMPBUF_T
#undef STM_JMPBUF
#undef STM_VALID
#undef STM_RESTART
#undef STM_STARTUP
#undef STM_SHUTDOWN
#undef STM_NEW_THREAD
#undef STM_INIT_THREAD
#undef STM_FREE_THREAD
#undef STM_BEGIN
#undef STM_BEGIN_RD
#undef STM_BEGIN_WR
#undef STM_END

//#undef stm_word_t

#include "../stms/tl2-x86/stm.h"


struct tmbase {
};


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

extern thread_local ThreadCheckInCheckOut tl_gc_tcico;

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
 * RomulusLR relies on this to work properly.
 */
class ThreadRegistry {
public:
    static const int                    REGISTRY_MAX_THREADS = 256;

private:
    alignas(128) std::atomic<bool>      usedTID[REGISTRY_MAX_THREADS];   // Which TIDs are in use by threads
    alignas(128) std::atomic<int>       maxTid {-1};                     // Highest TID (+1) in use by threads
    alignas(128) Thread*                selfs[REGISTRY_MAX_THREADS];

public:
    ThreadRegistry() {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            usedTID[it].store(false, std::memory_order_relaxed);
            selfs[it] = NULL;
        }
    }

    // Progress Condition: wait-free bounded (by the number of threads)
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
            tl_gc_tcico.tid = tid;
            return tid;
        }
        std::cout << "ERROR: Too many threads, registry can only hold " << REGISTRY_MAX_THREADS << " threads\n";
        assert(false);
    }

    // Progress condition: wait-free population oblivious
    inline void deregister_thread(const int tid) {
        STM_FREE_THREAD(selfs[tid]);
        selfs[tid] = NULL;
        usedTID[tid].store(false, std::memory_order_release);
    }

    // Progress condition: wait-free population oblivious
    static inline uint64_t getMaxThreads(void) {
        return gThreadRegistry.maxTid.load(std::memory_order_acquire);
    }

    // Progress condition: wait-free bounded (by the number of threads)
    static inline Thread* getThread(void) {
        int tid = tl_gc_tcico.tid;
        if (tid == ThreadCheckInCheckOut::NOT_ASSIGNED) {
            tid = gThreadRegistry.register_thread_new();
            gThreadRegistry.selfs[tid] = STM_NEW_THREAD();
            STM_INIT_THREAD(gThreadRegistry.selfs[tid],tid);
        }
        return gThreadRegistry.selfs[tid];
    }
};



class TL2;
extern TL2 gTL2;

class TL2 {

private:
    // Maximum number of participating threads
    static const uint64_t MAX_THREADS = 128;

public:
    struct tmbase : public tl2orig::tmbase { };

    TL2(unsigned int maxThreads=MAX_THREADS) {
        STM_STARTUP();
    }

    ~TL2() {
        STM_SHUTDOWN();
    }

    static std::string className() { return "TL2-Orig"; }

    template<typename R, class F>
    static R updateTx(F&& func) {
        auto Self = ThreadRegistry::getThread();
        R retval{};
        STM_BEGIN_WR();
        retval = func();
        STM_END();
        return retval;
    }

    template<class F> static void updateTx(F&& func) {
        auto Self = ThreadRegistry::getThread();
        STM_BEGIN_WR();
        func();
        STM_END();
    }

    template<typename R, class F>
    static R readTx(F&& func) {
        auto Self = ThreadRegistry::getThread();
        R retval{};
        STM_BEGIN_RD();
        retval = func();
        STM_END();
        return retval;
    }

    template<class F>
    static void readTx(F&& func) {
        auto Self = ThreadRegistry::getThread();
        STM_BEGIN_RD();
        func();
        STM_END();
    }

    template <typename T, typename... Args>
    static T* tmNew(Args&&... args) {
        auto Self = ThreadRegistry::getThread();
        void* addr = STM_MALLOC(sizeof(T));
        assert(addr != NULL);
        T* ptr = new (addr) T(std::forward<Args>(args)...);
        return ptr;
    }

    template<typename T>
    static void tmDelete(T* obj) {
        if (obj == nullptr) return;
        obj->~T();
        auto Self = ThreadRegistry::getThread();
        STM_FREE(obj);
    }

    static void* tmMalloc(size_t size) {
        auto Self = ThreadRegistry::getThread();
        return STM_MALLOC(size);
    }

    static void tmFree(void* obj) {
        auto Self = ThreadRegistry::getThread();
        STM_FREE(obj);
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
        auto Self = ThreadRegistry::getThread();
        STM_WRITE(val, newVal);
    }

    inline T pload() const {
        auto Self = ThreadRegistry::getThread();
        return (T)STM_READ(val);
    }
};

extern TL2 gTL2;

// Wrapper methods to the global TM instance. The user should use these:
template<typename R, typename F> R updateTx(F&& mutativeFunc) { return gTL2.updateTx<R>(mutativeFunc); }
template<typename R, typename F> R readTx(F&& readFunc) { return gTL2.readTx<R>(readFunc); }
template<typename F> void updateTx(F&& mutativeFunc) { gTL2.updateTx(mutativeFunc); }
template<typename F> void readTx(F&& readFunc) { gTL2.readTx(readFunc); }

// Wrapper to not do any transaction
template<typename R, typename Func>
R notx(Func&& func) { return func(); }

template <typename T, typename... Args>
T* tmNew(Args&&... args) { return gTL2.tmNew<T>(args...); }

template<typename T>
void tmDelete(T* obj) { gTL2.tmDelete<T>(obj); }

//static int getTID(void) { return ThreadRegistry::getTID(); }


//
// Place these in a .cpp if you include this header in multiple files
//
TL2 gTL2 {};
// Global/singleton to hold all the thread registry functionality
ThreadRegistry gThreadRegistry {};
// This is where every thread stores the tid it has been assigned when it calls getTID() for the first time.
// When the thread dies, the destructor of ThreadCheckInCheckOut will be called and de-register the thread.
thread_local ThreadCheckInCheckOut tl_gc_tcico {};
// Helper function for thread de-registration
void thread_registry_deregister_thread(const int tid) {
    gThreadRegistry.deregister_thread(tid);
}



}

