#pragma once

#include <cstdlib>
#include <cstring>
#include <iterator>

/// MiniVector is a self-growing array, like std::vector, but with less overhead
template <class T> class MiniVector {
  /// The maximum number of things this MiniVector can store without triggering
  /// a resize
  unsigned long capacity;

  /// The number of items currently stored in the MiniVector
  unsigned long count;

  /// Storage for the items in the array
  T *items;

  /// Resize the array of items, and move current data into it
  void expand() {
    T *temp = items;
    capacity *= 2;
    items = static_cast<T *>(malloc(sizeof(T) * capacity));
    memcpy(items, temp, sizeof(T) * count);
    free(temp);
  }

public:
  /// Construct an empty MiniVector with a default capacity
  MiniVector(const unsigned long _capacity = 64)
      : capacity(_capacity), count(0),
        items(static_cast<T *>(malloc(sizeof(T) * capacity))) {}

  /// Reclaim memory when the MiniVector is destructed
  ~MiniVector() { free(items); }

  /// We assume that T does not have a destructor, and thus we can fast-clear
  /// the MiniVector
  void clear() { count = 0; }

  /// Return whether the MiniVector is empty or not
  bool empty() { return count == 0; }

  /// MiniVector insert
  ///
  /// We maintain the invariant that when insert() returns, there is always room
  /// for one more element to be added.  This means we may expand() after
  /// insertion, but doing so is rare.
  void push_back(T data) {
    items[count++] = data;

    // If the list is full, double it
    if (count == capacity)
      expand();
  }

  /// Getter to report the array size (to test for empty)
  unsigned long size() const { return count; }

  /// MiniVector's iterator is just a T*
  typedef T *iterator;

  /// Get an iterator to the start of the array
  iterator begin() const { return items; }

  /// Get an iterator to one past the end of the array
  iterator end() const { return items + count; }

  /// MiniVector's reverse iteration is based on std::reverse_iterator, because
  /// this is not performance-critical
  typedef std::reverse_iterator<iterator> reverse_iterator;

  /// Get the starting point for a reverse iterator
  reverse_iterator rbegin() { return reverse_iterator(end()); }

  /// Get the ending point for a reverse iterator
  reverse_iterator rend() { return reverse_iterator(begin()); }
};