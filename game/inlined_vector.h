#pragma once

#include <basic/array.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <type_traits>

// A vector that stores up to N elements inline and only spills to the heap if
// it grows beyond N. Copying one that fits inline does no heap allocation —
// which is the point: game states are copied per node during agent search, and
// a std::vector member would pay one allocation per list on every copy.
//
// Pick N per field from the common-case length; the heap spill keeps it correct
// if that's ever exceeded. Restricted to trivially-copyable elements (the games
// store ints and small PODs), so copies are plain memory moves.
//
// Implicitly converts to `array<T>` (a non-owning span), so functions can take
// `array<const T>` parameters and accept both Inlined_Vector and std::vector.
template <class T, int N>
struct Inlined_Vector {
  static_assert(
    std::is_trivially_copyable<T>::value,
    "Inlined_Vector requires a trivially-copyable element type"
  );

  T   inline_storage[N];
  T*  items    = inline_storage;  // Points at inline_storage, or at the heap.
  int count    = 0;
  int capacity = N;

  Inlined_Vector() = default;
  Inlined_Vector(const Inlined_Vector& other) { copy_from(other); }
  Inlined_Vector& operator=(const Inlined_Vector& other) {
    if (this != &other) {
      release();
      copy_from(other);
    }
    return *this;
  }
  // Assign from an Inlined_Vector of any capacity (copies the live elements).
  template <int M>
  Inlined_Vector& operator=(const Inlined_Vector<T, M>& other) {
    assign(other.begin(), other.end());
    return *this;
  }
  ~Inlined_Vector() { release(); }

  int  size() const { return count; }
  bool empty() const { return count == 0; }
  void clear() { count = 0; }

  T&       operator[](int index) { return items[index]; }
  const T& operator[](int index) const { return items[index]; }
  T&       back() { return items[count - 1]; }
  const T& back() const { return items[count - 1]; }
  T&       front() { return items[0]; }
  const T& front() const { return items[0]; }

  T*       begin() { return items; }
  T*       end() { return items + count; }
  const T* begin() const { return items; }
  const T* end() const { return items + count; }
  T*       data() { return items; }
  const T* data() const { return items; }

  void push_back(const T& value) {
    if (count == capacity) grow();
    items[count++] = value;
  }
  void pop_back() {
    assert(count > 0);
    --count;
  }

  // Replace the contents with the range [first, last).
  template <class Iterator>
  void assign(Iterator first, Iterator last) {
    count = 0;
    for (Iterator it = first; it != last; ++it) push_back(*it);
  }

  // Append the range [first, last). Only end-insertion is used by the games.
  template <class Iterator>
  void insert(T* position, Iterator first, Iterator last) {
    assert(position == end());
    for (Iterator it = first; it != last; ++it) push_back(*it);
  }

  // Erase one element, shifting the tail down. Returns the next position.
  T* erase(T* position) {
    for (T* p = position; p + 1 != end(); ++p) *p = *(p + 1);
    --count;
    return position;
  }

  // Non-owning views (spans) over the live elements.
  operator array<T>() { return array<T>(items, (size_t)count); }
  operator array<const T>() const {
    return array<const T>(items, (size_t)count);
  }

 private:
  void grow() {
    int new_capacity = capacity * 2;
    T*  heap         = (T*)std::malloc(sizeof(T) * new_capacity);
    std::memcpy(heap, items, sizeof(T) * count);
    if (items != inline_storage) std::free(items);
    items    = heap;
    capacity = new_capacity;
  }
  void release() {
    if (items != inline_storage) std::free(items);
    items    = inline_storage;
    capacity = N;
    count    = 0;
  }
  void copy_from(const Inlined_Vector& other) {
    count = other.count;
    if (other.count <= N) {
      items    = inline_storage;
      capacity = N;
    } else {
      capacity = other.count;
      items    = (T*)std::malloc(sizeof(T) * capacity);
    }
    std::memcpy(items, other.items, sizeof(T) * count);
  }
};
