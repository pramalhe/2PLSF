#pragma once

#include <cstdint>

/// UndoLog is really just a vector of address/value pairs that are used by
/// Eager TMs to un-do writes to memory in the event that the transaction
/// aborts.
class UndoLog_Nonatomic {
public:
  /// undo_t represents a single entry in an undo log.  It consists of a
  /// pointer, a value, and something to indicate the type that is logged
  struct undo_t {
    /// A lightweight enum to represent the type of element logged in this
    /// undo_t. Valid values are 0/1/2/3/4/5/6/7,  for
    /// u8/u16/u32/u64/f32/f64/f80/pointer
    int type;

    /// A union storing the address, so that we can load/store through the
    /// address without casting
    union {
      uint8_t *u1;
      uint16_t *u2;
      uint32_t *u4;
      uint64_t *u8;
      float *f4;
      double *f8;
      //long double *f16;
      void **p;
    } addr;

    /// A union storing the value that has been logged.  Again, we use a union
    /// to avoid casting
    union {
      uint8_t u1;
      uint16_t u2;
      uint32_t u4;
      uint64_t u8;
      float f4;
      double f8;
      //long double f16;
      void *p;
    } val;

/// Initialize this undo_t from a pointer by setting addr to the pointer and
/// then dereferencing the pointer to get the value for val.  We use
/// overloading to do this in a type-safe way for the 8 primitive types in our
/// STM, and a macro to avoid lots of boilerplate code
#define MAKE_UNDOT_FUNCS(TYPE, FIELD, TYPEID)                                  \
  void initFromAddr(TYPE *a) {                                                 \
    addr.FIELD = a;                                                            \
    val.FIELD = *a;                                                            \
    type = TYPEID;                                                             \
  }                                                                            \
  void initFromVal(TYPE *a, TYPE v) {                                          \
    addr.FIELD = a;                                                            \
    val.FIELD = v;                                                             \
    type = TYPEID;                                                             \
  }
    MAKE_UNDOT_FUNCS(uint8_t, u1, 0);
    MAKE_UNDOT_FUNCS(uint16_t, u2, 1);
    MAKE_UNDOT_FUNCS(uint32_t, u4, 2);
    MAKE_UNDOT_FUNCS(uint64_t, u8, 3);
    MAKE_UNDOT_FUNCS(float, f4, 4);
    MAKE_UNDOT_FUNCS(double, f8, 5);
    //MAKE_UNDOT_FUNCS(long double, f16, 6);
    MAKE_UNDOT_FUNCS(void *, p, 7);

    /// Restore the value at addr to the value stored in val
    void restoreValue() {
      switch (type) {
      case 0:
        *addr.u1 = val.u1;
        break;
      case 1:
        *addr.u2 = val.u2;
        break;
      case 2:
        *addr.u4 = val.u4;
        break;
      case 3:
        *addr.u8 = val.u8;
        break;
      case 4:
        *addr.f4 = val.f4;
        break;
      case 5:
        *addr.f8 = val.f8;
        break;
      case 6:
//        *addr.f16 = val.f16;
//        break;
//      case 7:
        *addr.p = val.p;
        break;
      }
    }
  };

private:
  /// The vector that stores address/value pairs for undoing writes
  MiniVector<undo_t> undolog;

public:
  /// Construct an UndoLog by providing a default size for the underlying vector
  UndoLog_Nonatomic(const unsigned long _capacity = 64) : undolog(_capacity) {}

  /// Undo the writes stored in this UndoLog
  ///
  /// NB: for this "nonatomic" undolog, this is prone to races in the C++
  ///     memory model
  void undo_writes_atomic() {
    for (auto it = undolog.rbegin(), e = undolog.rend(); it != e; ++it) {
      it->restoreValue();
    }
  }

  /// Undo the writes stored in this UndoLog.  This is the version for when the
  /// TM uses pessimistic locking, and does not require atomics<> to be
  /// race-free.
  void undo_writes_nonatomic() {
    for (auto it = undolog.rbegin(), e = undolog.rend(); it != e; ++it) {
      it->restoreValue();
    }
  }

  /// Clear the UndoLog
  void clear() { undolog.clear(); }

  /// Insert an undo_t into the UndoLog
  void push_back(undo_t data) { undolog.push_back(data); }

  /// The UndoLog is responsible for providing a "correct" way for threads to
  /// read data from memory concurrently with threads writing that data from a
  /// transaction. Since this is the racy implementation of an undolog, the
  /// "correct" way is to just do a regular read, and pretend there isn't a
  /// race.
  template <typename T> static T perform_transactional_read(T *addr) {
    return *addr;
  }

  /// The UndoLog is responsible for providing a "correct" way for threads to
  /// write data from memory concurrently with threads reading that data from a
  /// transaction. Since this is the racy implementation of an undolog, the
  /// "correct" way is to just do a regular write, and pretend there isn't a
  /// race.
  template <typename T>
  static void perform_transactional_write(T *addr, T val) {
    *addr = val;
  }
};
