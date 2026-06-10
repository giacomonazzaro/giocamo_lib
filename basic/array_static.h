#pragma once

#include <basic/array.h>
#include <basic/array_inline.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <vector>

template <class T, int N>
struct Array_Static {
  static_assert(
    std::is_trivially_copyable<T>::value,
    "Array_Static requires a trivially-copyable element type"
  );

  T   items[N];
  int count = 0;

  Array_Static() = default;
  Array_Static(const Array_Static& other) { copy_from(other); }

  // Construct from a std::vector, for call sites that still produce one.
  Array_Static(const std::vector<T>& other) {
    assign(other.begin(), other.end());
  }
  // Construct from a braced list, e.g. Array_Static<const char*, N>{"a",
  // "b"}.
  Array_Static(std::initializer_list<T> list) {
    assign(list.begin(), list.end());
  }
  Array_Static& operator=(const Array_Static& other) {
    if (this != &other) {
      copy_from(other);
    }
    return *this;
  }
  // Assign from an Array_Static of any capacity (copies the live elements).
  template <int M>
  Array_Static& operator=(const Array_Static<T, M>& other) {
    assign(other.begin(), other.end());
    return *this;
  }
  // Assign from a std::vector or an array (span) view, for convenience.
  Array_Static& operator=(const std::vector<T>& other) {
    assign(other.begin(), other.end());
    return *this;
  }
  // Assign from a braced list, e.g. targets = {"Ok"}.
  Array_Static& operator=(std::initializer_list<T> list) {
    assign(list.begin(), list.end());
    return *this;
  }
  template <class U>
  Array_Static& operator=(const array<U>& other) {
    assign(other.begin(), other.end());
    return *this;
  }

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
    assert(count != N);
    items[count++] = value;
  }
  void pop_back() {
    assert(count > 0);
    count -= 1;
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

  // Convert to an Array_Inline of any capacity (copies the live elements).
  template <int M>
  operator Array_Inline<T, M>() const {
    auto result = Array_Inline<T, M>();
    result.assign(begin(), end());
    return result;
  }

 private:
  void copy_from(const Array_Static& other) {
    count = other.count;
    std::memcpy(items, other.items, sizeof(T) * count);
  }
};
