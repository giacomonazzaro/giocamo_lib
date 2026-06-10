#pragma once

#include <basic/array.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <vector>

// A vector that stores up to N elements inline and only spills to the heap if
// it grows beyond N.
// Restricted to trivially-copyable elements, so copies are plain memory moves.
//
// Implicitly converts to `array<T>` (a non-owning span), so functions can take
// `array<const T>` parameters and accept both Array_Inline and std::vector.
template <class T, int N>
struct Array_Inline {
  static_assert(
    std::is_trivially_copyable<T>::value,
    "Array_Inline requires a trivially-copyable element type"
  );

  T   inline_storage[N];
  T*  items    = inline_storage;  // Points at inline_storage, or at the heap.
  int count    = 0;
  int capacity = N;

  Array_Inline() = default;
  Array_Inline(const Array_Inline& other) { copy_from(other); }
  // Construct from an Array_Inline of any capacity (copies the live
  // elements).
  template <int M>
  Array_Inline(const Array_Inline<T, M>& other) {
    assign(other.begin(), other.end());
  }
  // Construct from a std::vector, for call sites that still produce one.
  Array_Inline(const std::vector<T>& other) {
    assign(other.begin(), other.end());
  }
  // Construct from a braced list, e.g. Array_Inline<const char*, N>{"a",
  // "b"}.
  Array_Inline(std::initializer_list<T> list) {
    assign(list.begin(), list.end());
  }
  Array_Inline& operator=(const Array_Inline& other) {
    if (this != &other) {
      release();
      copy_from(other);
    }
    return *this;
  }
  // Assign from an Array_Inline of any capacity (copies the live elements).
  template <int M>
  Array_Inline& operator=(const Array_Inline<T, M>& other) {
    assign(other.begin(), other.end());
    return *this;
  }
  // Assign from a std::vector or an array (span) view, for convenience.
  Array_Inline& operator=(const std::vector<T>& other) {
    assign(other.begin(), other.end());
    return *this;
  }
  // Assign from a braced list, e.g. targets = {"Ok"}.
  Array_Inline& operator=(std::initializer_list<T> list) {
    assign(list.begin(), list.end());
    return *this;
  }
  template <class U>
  Array_Inline& operator=(const array<U>& other) {
    assign(other.begin(), other.end());
    return *this;
  }
  ~Array_Inline() { release(); }

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

  template <class Iterator>
  void append(Iterator first, Iterator last) {
    for (Iterator it = first; it != last; ++it) push_back(*it);
  }

  // Erase one element, shifting the tail down. Returns the next position.
  T* erase(T* position) {
    for (T* p = position; p + 1 != end(); ++p) {
      *p = *(p + 1);
    }
    count -= 1;
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
  void copy_from(const Array_Inline& other) {
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
