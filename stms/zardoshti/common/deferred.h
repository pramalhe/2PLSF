/// deferred.h provides support for registering "on commit" handlers, which run
/// after a transaction completes.

#pragma once

#include <cstdlib>
#include <functional>

#include "../../zardoshti/common/minivector.h"

/// The DeferredActionHandler stores a list of actions to perform, and when a
/// transaction commits, it performs those actions in the order they were
/// registered.
class DeferredActionHandler {
  /// a list of all actions to perform upon transaction commit
  MiniVector<std::pair<void (*)(void *), void *>> actions;

public:
  /// Register a function to run after the transaction commits
  void registerHandler(void (*func)(void *), void *args) {
    actions.push_back({func, args});
  }

  /// Execute all deferred actions upon transaction commit, and then clear the
  /// list of actions
  void onCommit() {
    for (auto i : actions) {
      i.first(i.second);
    }
    actions.clear();
  }

  /// Clear the list of pending actions when a transaction aborts
  void onAbort() { actions.clear(); }
};