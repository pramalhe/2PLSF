/// stackframe.h provides implementations of the stack frame manager.  The stack
/// frame manager is responsible for two tasks:
///
/// - It tracks nesting, to know if a thread is in a transaction, and to enable
///   subsumption (flat) nesting
///
/// - It tracks the top of the non-transactional stack, so that loads/stores to
///   the transactional region can skip instrumentation

#pragma once

#include <cstdint>

/// BasicStackFrameManager does not understand anything about the stack frame
/// boundary.  It just manages nesting for the TM.  This makes it unsuitable
/// for STM, but ideal for CGL and HTM.
class BasicStackFrameManager {
  /// The nesting depth
  size_t nesting;

public:
  /// Construct by zeroing all fields
  BasicStackFrameManager() : nesting(0) {}

  /// When a transaction begins, increment nesting and return true if this is a
  /// top-level transaction
  bool onBegin() { return ++nesting == 1; }

  /// When a transaction ends, decrement nesting and return true if this is a
  /// top-level transaction
  bool onEnd() { return --nesting == 0; }

  /// When a transaction aborts, reset nesting
  void onAbort() { nesting = 0; }

  /// No-op
  void setBottom(void *) {}

  /// No-op
  bool onStack(void *) { return false; }

  /// No-op
  void onCommit() {}
};

/// OptimizedStackFrameManager is a stack frame manager that is able to change
/// the stack frame boundary.  This lets the programmer indicate that the top of
/// the nontransactional stack is not shared and can be accessed without
/// instrumentation.
///
/// A small amount of this support is necessary at all times, e.g., to prevent
/// redo/undo to dead stack frames, or to prevent NOrec validation of stack
/// reads.  The choice of API determines whether the programmer can manually
/// expand the stack frame, e.g., to include local variables declared outside of
/// the transaction.
///
/// NB: When the optimization is explicitly used at the API level, the
///     programmer may need to manually checkpoint some variables, if they
///     aren't written on every control flow path through the transaction.
class OptimizedStackFrameManager {
  /// An address we can use to identify the current bottom of the transactional
  /// part of the stack
  uintptr_t stackBottom;

  /// The nesting depth
  size_t nesting;

public:
  /// Construct by zeroing all fields
  OptimizedStackFrameManager() : stackBottom(0), nesting(0) {}

  /// When a transaction begins, increment nesting and return true if this is a
  /// top-level transaction
  bool onBegin() { return ++nesting == 1; }

  /// When a transaction ends, decrement nesting and return true if this is a
  /// top-level transaction
  bool onEnd() { return --nesting == 0; }

  /// When a transaction aborts, reset nesting
  void onAbort() { nesting = 0; }

  /// Set the bottom of the stack, but do so only if the bottom is currently
  /// unset
  void setBottom(void *b) {
    if (!stackBottom) {
      stackBottom = (uintptr_t)b;
    }
  }

  /// Return true if the given pointer is to the transactional part of the
  /// stack.
  bool onStack(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    return (addr < stackBottom && addr > (uintptr_t)&addr);
  }

  /// When a transaction has committed, clear its stack bottom.  This is
  /// necessary, so that setBottom can be called correctly from outside of a
  /// transaction.
  void onCommit() { stackBottom = 0; }
};