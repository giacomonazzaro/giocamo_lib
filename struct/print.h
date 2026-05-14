#pragma once
#include <iostream>
#include <type_traits>
#include <vector>

#include "struct/visit.hpp"

template <typename T>
struct Custom_Visit_Struct;

// Method 1: Using void_t (C++17)
template <typename T, typename = void>
struct has_custom_visit_struct : std::false_type {};

template <typename T>
struct has_custom_visit_struct<
  T,
  std::void_t<decltype(sizeof(Custom_Visit_Struct<T>))>> : std::true_type {};

// Helper variable template
template <typename T>
inline constexpr bool has_custom_visit_struct_v =
  has_custom_visit_struct<T>::value;

template <typename T, typename = void>
struct is_printable : std::false_type {};

template <typename T>
struct is_printable<
  T,
  std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>>
    : std::true_type {};

template <typename T>
constexpr bool is_printable_v = is_printable<T>::value;

#define is_type_vistable(x) \
  visit_struct::traits::is_visitable<std::decay_t<x>>::value

#define is_visitable(x) \
  visit_struct::traits::is_visitable<std::decay_t<decltype(x)>>::value

#define struct_is_string(s) \
  std::is_same<std::decay_t<decltype(s)>, std::string>::value

#include <cxxabi.h>
inline std::string demangle(const char* name) {
  int         status;
  char*       dem    = abi::__cxa_demangle(name, nullptr, nullptr, &status);
  std::string result = (status == 0 && dem) ? dem : name;
  free(dem);
  return result;
}

template <typename>
struct is_std_vector : std::false_type {};
template <typename U, typename Alloc>
struct is_std_vector<std::vector<U, Alloc>> : std::true_type {
  using value_type = U;
};

template <typename>
struct is_custom_visit_struct : std::false_type {};
template <typename T>
struct is_custom_visit_struct<Custom_Visit_Struct<T>> : std::true_type {
  using value_type = T;
};

template <typename T>
inline std::string get_type_name(const T& value) {
  using DT = std::decay_t<T>;

  if constexpr (struct_is_string(value)) {
    return "std::string";
  } else if constexpr (is_std_vector<DT>::value) {
    using Vector_Type = typename is_std_vector<DT>::value_type;
    return std::string("std::vector<") + get_type_name(Vector_Type()) + ">";
  } else if constexpr (is_custom_visit_struct<DT>::value) {
    using Custom_Type = typename is_custom_visit_struct<DT>::value_type;
    return get_type_name(Custom_Type());
  } else {
    return demangle(typeid(T).name());
  }
}

#include <sstream>

namespace std {
inline std::string to_string(void* ptr) {
  std::ostringstream oss;
  oss << ptr;  // prints the pointer in implementation-defined format
  return oss.str();
}

}  // namespace std

template <typename T>
inline std::string to_string(const T& t, bool print_type, int indent) {
  auto ind    = std::string(indent, ' ');
  auto result = std::string();
  if constexpr (has_custom_visit_struct_v<T>) {
    return Custom_Visit_Struct<T>::to_string(t, print_type, indent);
  } else if constexpr (is_visitable(t)) {
    if (print_type) {
      std::string type_name = get_type_name(t);
      result += type_name + " ";
    }
    result += "{\n";
    visit_struct::for_each(t, [&](const char* name, const auto& value) {
      result += ind + name + ": ";
      result += to_string(value, print_type, indent + 2);
    });
    result += std::string(indent - 2, ' ') + "}\n";
    return result;
  } else if constexpr (std::is_enum_v<T>) {
    result += std::to_string(static_cast<std::underlying_type_t<T>>(t));
    std::string type_name = get_type_name(t);
    if (print_type) result += " (" + type_name + ")";
    result += "\n";
    return result;
  } else if constexpr (is_printable_v<T>) {
    if constexpr (struct_is_string(t)) {
      result += "\"" + t + "\"";
    } else {
      result += std::to_string(t);
    }
    std::string type_name = get_type_name(t);
    if (print_type) result += " (" + type_name + ")";
    result += "\n";
    return result;
  } else {
    result += "<unprintable type>";
    std::string type_name = get_type_name(t);
    if (print_type) result += " (" + type_name + ")";
    result += "\n";
    return result;
  }
}

#define print(x)           \
  std::cout << #x << ": "; \
  printf("%s", to_string(x, true, 2).c_str());

#include "basic/array.h"

template <typename Array>
inline std::string array_to_string(
  const Array& t, bool print_type, int indent
) {
  //
  auto        result    = std::string();
  auto        ind       = std::string(indent, ' ');
  std::string type_name = get_type_name(t);
  if (t.size() == 0) {
    result += type_name + " { size: " + std::to_string(t.size()) + " }\n";
    return result;
  }
  result += type_name + " { size: " + std::to_string(t.size()) + "\n";
  auto s = t.size();
  if (s >= 2) s = 2;
  for (size_t i = 0; i < s; i++) {
    auto id = "[" + std::to_string(i) + "]";
    result += ind + id.c_str() + ": ";
    result += ::to_string(t[i], false, indent + 2);
  }

  if (t.size() < 5) s = 2;
  if (t.size() == 5) s = t.size() - 3;
  if (t.size() > 5) {
    s = t.size() - 2;
    result += ind + "..." + "\n";
  }

  for (size_t i = s; i < t.size(); i++) {
    auto id = "[" + std::to_string(i) + "]";
    result += ind + id.c_str() + ": ";
    result += ::to_string(t[i], false, indent + 2);
  }

  result += std::string(indent - 2, ' ') + "}\n";
  return result;
}

template <typename T>
struct Custom_Visit_Struct<array<T>> {
  static std::string to_string(const array<T>& t, bool print_type, int indent) {
    return array_to_string(t, print_type, indent);
  }
};

template <typename T>
struct Custom_Visit_Struct<std::vector<T>> {
  static std::string to_string(
    const std::vector<T>& t, bool print_type, int indent
  ) {
    return array_to_string(t, print_type, indent);
  }
};