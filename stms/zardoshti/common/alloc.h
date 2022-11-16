/// alloc.h provides a set of allocation managers that can be used by a TM
/// implementation.  These allocation managers all provide the same public
/// interface, so that they are interchangeable in TM algorithms.

#pragma once

#include <cstdlib>
#include <functional>

#include "../../zardoshti/common/minivector.h"

/// The ImmediateAllocationManager is a degenerate manager, suitable only for
/// use in always-irrevocable, no-instrumentation TM, such as Mutex and HTM.
class ImmediateAllocationManager {
public:
  /// Prepare the manager at the start of a transaction
  void onBegin() {}

  /// Notify the manager at the end of a transaction
  void onCommit() {}

  /// Notify the manager when the transaction aborts
  ///
  /// NB: we don't expect visible aborts when using this manager
  void onAbort() {}

  /// Allocate memory within a transaction
  ///
  /// NB: the function pointer is ignored in this allocation manager
  void *alloc(size_t size, std::function<void()>) { return malloc(size); }

  /// Allocate memory that is aligned on a byte boundary as specified by A
  ///
  /// NB: the function pointer is ignored in this allocation manager
  void *alignAlloc(size_t A, size_t size, std::function<void()>) {
    return aligned_alloc(A, size);
  }

  /// Reclaim memory within a transaction
  void reclaim(void *addr) { free(addr); }

  /// Return true if the given address is within the range returned by the most
  /// recent allocation, but do so only if CAPTURE is true
  bool checkCaptured(void *) { return false; }
};

/// The BasicAllocationManager provides a mechanism for a transaction to log its
/// allocations and frees, and to finalize or undo them if the transaction
/// commits or aborts.  It also supports the "capture" optimization, which
/// tracks the most recent allocation and suggests to the TM that accesses in
/// that allocation shouldn't be instrumented.
template <bool CAPTURE> class BasicAllocationManager {
protected:
  /// a list of all not-yet-committed allocations in this transaction
  MiniVector<void *> mallocs;

  /// a list of all not-yet-committed deallocations in this transaction
  MiniVector<void *> frees;

  /// Track if allocation management is active or not
  bool active = false;

  /// Address of last allocation
  void *lastAlloc;

  /// Size of last allocation
  size_t lastSize;

public:
  /// Indicate that logging should begin
  void onBegin() { active = true; }

  /// When a transaction commits, finalize its mallocs and frees.  Note that
  /// this should be called *after* privatization is ensured.
  void onCommit() {
    mallocs.clear();
    for (auto a : frees) {
      free(a);
    }
    frees.clear();
    active = false;
    lastAlloc = nullptr;
    lastSize = 0;
  }

  /// When a transaction aborts, drop its frees and reclaim its mallocs
  void onAbort() {
    frees.clear();
    for (auto p : mallocs) {
      free(p);
    }
    mallocs.clear();
    active = false;
    lastAlloc = nullptr;
    lastSize = 0;
  }

  /// To allocate memory, we must also log it, so we can reclaim it if the
  /// transaction aborts
  ///
  /// NB: the function pointer is ignored in this allocation manager
  void *alloc(size_t size, std::function<void()>) {
    void *res = malloc(size);
    if (active) {
      mallocs.push_back(res);
      lastAlloc = res;
      lastSize = size;
    }
    return res;
  }

  /// Allocate memory that is aligned on a byte boundary as specified by A
  ///
  /// NB: the function pointer is ignored in this allocation manager
  void *alignAlloc(size_t A, size_t size, std::function<void()>) {
    void *res = aligned_alloc(A, size);
    if (active) {
      mallocs.push_back(res);
      lastAlloc = res;
      lastSize = size;
    }
    return res;
  }

  /// To free memory, we simply wait until the transaction has committed, and
  /// then we free.
  void reclaim(void *addr) {
    if (active) {
      frees.push_back(addr);
    } else {
      free(addr);
    }
  }

  /// Return true if the given address is within the range returned by the most
  /// recent allocation, but do so only if CAPTURE is true
  bool checkCaptured(void *addr) {
    if (CAPTURE) {
      uintptr_t lstart = (uintptr_t)lastAlloc;
      uintptr_t lend = lstart + lastSize;
      uintptr_t a = (uintptr_t)addr;
      return a >= lstart && a < lend;
    } else {
      return false;
    }
  }
};

/// The BoundedAllocationManager extends the BasicAllocationManager with a
/// mechanism for counting the number of allocations within a transaction.  If
/// the count exceeds MAXALLOCS, allocations will execute a callback.  The
/// expectation is that the callback will make the transaction irrevocable.
template <int MAXALLOCS, bool CAPTURE>
class BoundedAllocationManager : public BasicAllocationManager<CAPTURE> {
public:
  /// Allocate memory.  If the threshold is crossed, call the callback
  void *alloc(size_t size, std::function<void()> callback) {
    void *res = BasicAllocationManager<CAPTURE>::alloc(size, callback);
    if (this->active && this->mallocs.size() >= MAXALLOCS) {
      callback();
    }
    return res;
  }

  /// Allocate aligned memory.  If the threshold is crossed, call the callback
  void *alignAlloc(size_t A, size_t size, std::function<void()> callback) {
    void *res = BasicAllocationManager<CAPTURE>::alignAlloc(A, size, callback);
    if (this->active && this->mallocs.size() >= MAXALLOCS) {
      callback();
    }
    return res;
  }
};