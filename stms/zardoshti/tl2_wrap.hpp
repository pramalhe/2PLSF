
#pragma once

// This is a wrapper to TL2 taken from the github repository:
// https://github.com/mfs409/llvm-transmem/tree/master/tm/libs/stm_algs

// The algorithm we are using:
#include "../zardoshti/api/constants.h"
#include "../zardoshti/common/alloc.h"
#include "../zardoshti/common/cm.h"
#include "../zardoshti/common/epochs.h"
#include "../zardoshti/common/orec_t.h"
#include "../zardoshti/common/redolog_atomic.h"
#include "../zardoshti/common/stackframe.h"
#include "../zardoshti/stm_algs/tl2.h"



namespace tl2 {

thread_local int tl_nested_trans {0};


/// TxThread is a shorthand for the instantiated version of the TM algorithm, so
/// that we can use common macros to define the API:
typedef TL2<OrecTable<NUM_STRIPES, OREC_COVERAGE, CounterTimesource>,
            RedoLog_Atomic<2 << OREC_COVERAGE>,
            IrrevocQuiesceEpochManager<MAX_THREADS>,
            ExpBackoffCM<BACKOFF_MIN, BACKOFF_MAX>, OptimizedStackFrameManager,
            BoundedAllocationManager<MALLOC_THRESHOLD, true>, false>
    TxThread;


thread_local TxThread *self = nullptr;
static TxThread *get_self() {
  if (__builtin_expect(self == nullptr, false)) {
    self = new TxThread();
  }
  return self;
}

// Needed by our microbenchmarks
struct tmbase { };


/*
 * <h1> Wrapper for TL2
 *
 */
class STM {

public:
    STM()  {  }

    ~STM() { }

    struct tmbase : public tl2::tmbase { };

    static std::string className() { return "tl2"; }


    template<class F> static void transaction(F&& func) {
        if (tl_nested_trans > 0) {
            func();
            return;
        }
        ++tl_nested_trans;
        /* get TxThread before making checkpoint, so it doesn't re-run on abort */
        TxThread *self = get_self();
        jmp_buf _jmpbuf;
        setjmp(_jmpbuf);
        self->beginTx(&_jmpbuf);
        func();
        self->commitTx();
        --tl_nested_trans;
    }

    // Transaction with a non-void return
    template<typename R, typename F> static R transaction(F&& func) {
        if (tl_nested_trans > 0) {
            return func();
        }
        ++tl_nested_trans;
        /* get TxThread before making checkpoint, so it doesn't re-run on abort */
        TxThread *self = get_self();
        jmp_buf _jmpbuf;
        setjmp(_jmpbuf);
        self->beginTx(&_jmpbuf);
        R ret = func();
        self->commitTx();
        --tl_nested_trans;
        return ret;
    }


    // It's silly that these have to be static, but we need them for the (SPS) benchmarks due to templatization
    template<typename R, typename F> static R updateTx(F&& func) { return transaction<R>(func); }
    template<typename R, typename F> static R readTx(F&& func) { return transaction<R>(func); }
    template<typename F> static void updateTx(F&& func) { transaction(func); }
    template<typename F> static void readTx(F&& func) { transaction(func); }

    /*
     * Allocator
     * Must be called from within a transaction
     */
    template <typename T, typename... Args> static T* tmNew(Args&&... args) {
        TxThread *self = get_self();
        void *addr = self->txAlloc(sizeof(T));
        return new (addr) T(std::forward<Args>(args)...); // placement new
    }

    /*
     * De-allocator
     * Must be called from within a transaction
     */
    template<typename T> static void tmDelete(T* obj) {
        if (obj == nullptr) return;
        obj->~T();
        TxThread *self = get_self();
        self->txFree(obj);
    }

    static void* tmMalloc(size_t size) {
        TxThread *self = get_self();
        assert(self != nullptr);
        return self->txAlloc(size);
    }

    static void tmFree(void* obj) {
        TxThread *self = get_self();
        assert(self != nullptr);
        self->txFree(obj);
    }
};


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
        get_self()->write<uint64_t>((uint64_t*)&val, (uint64_t)newVal);
    }

    inline T pload() const {
        return (T)get_self()->read<uint64_t>((uint64_t*)&val);
    }
};

//
// Wrapper methods to the global TM instance. The user should use these:
//
template<typename R, typename F> static R updateTx(F&& func) { return STM::transaction<R>(func); }
template<typename R, typename F> static R readTx(F&& func) { return STM::transaction<R>(func); }
template<typename F> static void updateTx(F&& func) { STM::transaction(func); }
template<typename F> static void readTx(F&& func) { STM::transaction(func); }
template<typename T, typename... Args> T* tmNew(Args&&... args) { return STM::tmNew<T>(args...); }
template<typename T> void tmDelete(T* obj) { STM::tmDelete<T>(obj); }
static void* tmMalloc(size_t size) { return STM::tmMalloc(size); }
static void tmFree(void* obj) { STM::tmFree(obj); }

} // end of namespace  tl2


/// Define TxThread::Globals.  Note that defining it this way ensures it is
/// initialized before any threads are created.
template <class O, class R, class E, class C, class S, class A, bool SFO>
typename TL2<O, R, E, C, S, A, SFO>::Globals
    TL2<O, R, E, C, S, A, SFO>::globals;

