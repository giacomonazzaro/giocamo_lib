#pragma once
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "struct/visit.hpp"

// Detect std::vector (used by parsing code)
template <typename>
struct is_std_vector_json : std::false_type {};
template <typename U, typename Alloc>
struct is_std_vector_json<std::vector<U, Alloc>> : std::true_type {
  using value_type = U;
};

// Escape string for JSON
inline std::string json_escape_string(const std::string& s) {
  std::string result;
  result.reserve(s.size() + 2);
  for (char c : s) {
    switch (c) {
      case '"': result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\b': result += "\\b"; break;
      case '\f': result += "\\f"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
          result += buf;
        } else {
          result += c;
        }
    }
  }
  return result;
}

// Helper for indentation
inline std::string json_indent(int level, bool pretty) {
  return pretty ? std::string(level * 2, ' ') : "";
}

inline std::string json_newline(bool pretty) { return pretty ? "\n" : ""; }

inline std::string json_space(bool pretty) { return pretty ? " " : ""; }

// Forward declarations
template <typename T, typename Alloc>
std::string to_json(
  const std::vector<T, Alloc>& arr, int indent = 0, bool pretty = true
);

// Inlined_Vector (inline fixed-capacity vector, defined in game/inlined_vector.h).
template <class T, int Capacity>
struct Inlined_Vector;
template <class T, int Capacity>
std::string to_json(
  const Inlined_Vector<T, Capacity>& arr, int indent = 0, bool pretty = true
);

template <typename T, std::enable_if_t<!std::is_enum_v<T>, int> = 0>
std::string to_json(const T& t, int indent = 0, bool pretty = true);

// Boolean
inline std::string to_json(bool t, int = 0, bool = true) {
  return t ? "true" : "false";
}

// Nullptr
inline std::string to_json(std::nullptr_t, int = 0, bool = true) {
  return "null";
}

// String
inline std::string to_json(const std::string& t, int = 0, bool = true) {
  return "\"" + json_escape_string(t) + "\"";
}

// C-string
inline std::string to_json(const char* t, int = 0, bool = true) {
  if (t == nullptr) return "null";
  return "\"" + json_escape_string(t) + "\"";
}

// Integer types
inline std::string to_json(int t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(long t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(long long t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(unsigned int t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(unsigned long t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(unsigned long long t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(short t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(unsigned short t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(signed char t, int = 0, bool = true) {
  return std::to_string(t);
}
inline std::string to_json(unsigned char t, int = 0, bool = true) {
  return std::to_string(t);
}

// Floating point types
inline std::string to_json(float t, int = 0, bool = true) {
  std::ostringstream oss;
  oss << std::setprecision(17) << t;
  return oss.str();
}
inline std::string to_json(double t, int = 0, bool = true) {
  std::ostringstream oss;
  oss << std::setprecision(17) << t;
  return oss.str();
}
inline std::string to_json(long double t, int = 0, bool = true) {
  std::ostringstream oss;
  oss << std::setprecision(17) << t;
  return oss.str();
}

// Pointer (as null or hex string)
template <typename T>
auto to_json(T* ptr, int = 0, bool = true)
  -> std::enable_if_t<!std::is_same_v<std::decay_t<T>, char>, std::string> {
  if (ptr == nullptr) return "null";
  std::ostringstream oss;
  oss << "\"" << static_cast<const void*>(ptr) << "\"";
  return oss.str();
}

// Array/vector to JSON helper
template <typename Array>
static std::string array_to_json(const Array& arr, int indent, bool pretty) {
  if (arr.size() == 0) {
    return "[]";
  }

  std::string result = "[" + json_newline(pretty);
  for (size_t i = 0; i < arr.size(); i++) {
    result += json_indent(indent + 1, pretty);
    result += to_json(arr[i], indent + 1, pretty);
    if (i < arr.size() - 1) {
      result += ",";
    }
    result += json_newline(pretty);
  }
  result += json_indent(indent, pretty) + "]";
  return result;
}

// std::vector
template <typename T, typename Alloc>
std::string to_json(const std::vector<T, Alloc>& arr, int indent, bool pretty) {
  return array_to_json(arr, indent, pretty);
}

// Inlined_Vector — serialized as a JSON array, same as a vector.
template <class T, int Capacity>
std::string to_json(
  const Inlined_Vector<T, Capacity>& arr, int indent, bool pretty
) {
  return array_to_json(arr, indent, pretty);
}

// Enum (scoped or unscoped) — serialize as underlying integer.
template <typename T>
auto to_json(const T& t, int = 0, bool = true)
  -> std::enable_if_t<std::is_enum_v<T>, std::string> {
  return std::to_string(static_cast<std::underlying_type_t<T>>(t));
}

// Visitable struct (excluded for enums, which are handled above).
template <typename T, std::enable_if_t<!std::is_enum_v<T>, int>>
std::string to_json(const T& t, int indent, bool pretty) {
  std::string result = "{" + json_newline(pretty);
  bool        first  = true;
  visit_struct::for_each(t, [&](const char* name, const auto& value) {
    if (!first) {
      result += "," + json_newline(pretty);
    }
    first = false;
    result += json_indent(indent + 1, pretty);
    result += "\"" + std::string(name) + "\":" + json_space(pretty);
    result += to_json(value, indent + 1, pretty);
  });
  result += json_newline(pretty) + json_indent(indent, pretty) + "}";
  return result;
}

// ============================================================================
// JSON PARSING
// ============================================================================

// Parser state
struct JsonParser {
  const std::string& json;
  size_t             pos = 0;

  JsonParser(const std::string& s) : json(s) {}

  char peek() const { return pos < json.size() ? json[pos] : '\0'; }
  char get() { return pos < json.size() ? json[pos++] : '\0'; }
  bool eof() const { return pos >= json.size(); }

  void skip_whitespace() {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                 json[pos] == '\n' || json[pos] == '\r')) {
      pos++;
    }
  }

  bool expect(char c) {
    skip_whitespace();
    if (peek() == c) {
      pos++;
      return true;
    }
    return false;
  }

  bool match(const char* s) {
    skip_whitespace();
    size_t len = strlen(s);
    if (pos + len <= json.size() && json.compare(pos, len, s) == 0) {
      pos += len;
      return true;
    }
    return false;
  }
};

// Forward declarations for parsing
inline bool from_json_impl(JsonParser& p, bool& out);
inline bool from_json_impl(JsonParser& p, std::string& out);
inline bool from_json_impl(JsonParser& p, int& out);
inline bool from_json_impl(JsonParser& p, long& out);
inline bool from_json_impl(JsonParser& p, long long& out);
inline bool from_json_impl(JsonParser& p, unsigned int& out);
inline bool from_json_impl(JsonParser& p, unsigned long& out);
inline bool from_json_impl(JsonParser& p, unsigned long long& out);
inline bool from_json_impl(JsonParser& p, short& out);
inline bool from_json_impl(JsonParser& p, unsigned short& out);
inline bool from_json_impl(JsonParser& p, signed char& out);
inline bool from_json_impl(JsonParser& p, unsigned char& out);
inline bool from_json_impl(JsonParser& p, float& out);
inline bool from_json_impl(JsonParser& p, double& out);
inline bool from_json_impl(JsonParser& p, long double& out);

template <typename T, typename Alloc>
bool from_json_impl(JsonParser& p, std::vector<T, Alloc>& out);

template <class T, int Capacity>
bool from_json_impl(JsonParser& p, Inlined_Vector<T, Capacity>& out);

template <typename T>
auto from_json_impl(JsonParser& p, T& out) -> std::
  enable_if_t<visit_struct::traits::is_visitable<std::decay_t<T>>::value, bool>;

// Parse a JSON string (handles escape sequences)
inline bool parse_json_string(JsonParser& p, std::string& out) {
  p.skip_whitespace();
  if (p.peek() != '"') return false;
  p.get();  // consume opening quote

  out.clear();
  while (!p.eof() && p.peek() != '"') {
    char c = p.get();
    if (c == '\\') {
      char escaped = p.get();
      switch (escaped) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          // Parse 4 hex digits
          if (p.pos + 4 > p.json.size()) return false;
          std::string hex = p.json.substr(p.pos, 4);
          p.pos += 4;
          int codepoint = std::stoi(hex, nullptr, 16);
          if (codepoint < 0x80) {
            out += static_cast<char>(codepoint);
          } else if (codepoint < 0x800) {
            out += static_cast<char>(0xC0 | (codepoint >> 6));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
          } else {
            out += static_cast<char>(0xE0 | (codepoint >> 12));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codepoint & 0x3F));
          }
          break;
        }
        default: out += escaped; break;
      }
    } else {
      out += c;
    }
  }

  if (p.peek() != '"') return false;
  p.get();  // consume closing quote
  return true;
}

// Parse a JSON number
inline bool parse_json_number(JsonParser& p, double& out) {
  p.skip_whitespace();
  size_t start = p.pos;

  // Optional minus
  if (p.peek() == '-') p.get();

  // Integer part
  if (p.peek() == '0') {
    p.get();
  } else if (p.peek() >= '1' && p.peek() <= '9') {
    while (p.peek() >= '0' && p.peek() <= '9') p.get();
  } else {
    return false;
  }

  // Fractional part
  if (p.peek() == '.') {
    p.get();
    if (!(p.peek() >= '0' && p.peek() <= '9')) return false;
    while (p.peek() >= '0' && p.peek() <= '9') p.get();
  }

  // Exponent
  if (p.peek() == 'e' || p.peek() == 'E') {
    p.get();
    if (p.peek() == '+' || p.peek() == '-') p.get();
    if (!(p.peek() >= '0' && p.peek() <= '9')) return false;
    while (p.peek() >= '0' && p.peek() <= '9') p.get();
  }

  std::string num_str = p.json.substr(start, p.pos - start);
  out                 = std::stod(num_str);
  return true;
}

// Boolean
inline bool from_json_impl(JsonParser& p, bool& out) {
  p.skip_whitespace();
  if (p.match("true")) {
    out = true;
    return true;
  } else if (p.match("false")) {
    out = false;
    return true;
  }
  return false;
}

// String
inline bool from_json_impl(JsonParser& p, std::string& out) {
  return parse_json_string(p, out);
}

// Integer types
inline bool from_json_impl(JsonParser& p, int& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<int>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, long& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<long>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, long long& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<long long>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, unsigned int& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<unsigned int>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, unsigned long& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<unsigned long>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, unsigned long long& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<unsigned long long>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, short& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<short>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, unsigned short& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<unsigned short>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, signed char& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<signed char>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, unsigned char& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<unsigned char>(d);
  return true;
}

// Floating point types
inline bool from_json_impl(JsonParser& p, float& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<float>(d);
  return true;
}
inline bool from_json_impl(JsonParser& p, double& out) {
  return parse_json_number(p, out);
}
inline bool from_json_impl(JsonParser& p, long double& out) {
  double d;
  if (!parse_json_number(p, d)) return false;
  out = static_cast<long double>(d);
  return true;
}

// Enum (scoped or unscoped) — parse as underlying integer.
template <typename T>
inline auto from_json_impl(JsonParser& p, T& out)
  -> std::enable_if_t<std::is_enum_v<T>, bool> {
  std::underlying_type_t<T> n;
  if (!from_json_impl(p, n)) return false;
  out = static_cast<T>(n);
  return true;
}

// std::vector
template <typename T, typename Alloc>
inline bool from_json_impl(JsonParser& p, std::vector<T, Alloc>& out) {
  p.skip_whitespace();
  if (!p.expect('[')) return false;

  out.clear();
  bool first = true;
  while (true) {
    p.skip_whitespace();
    if (p.peek() == ']') {
      p.get();
      return true;
    }

    if (!first && !p.expect(',')) return false;
    first = false;

    T elem;
    if (!from_json_impl(p, elem)) return false;
    out.push_back(std::move(elem));
  }
}

// Inlined_Vector — parsed from a JSON array, same as a vector.
template <class T, int Capacity>
inline bool from_json_impl(JsonParser& p, Inlined_Vector<T, Capacity>& out) {
  p.skip_whitespace();
  if (!p.expect('[')) return false;

  out.clear();
  bool first = true;
  while (true) {
    p.skip_whitespace();
    if (p.peek() == ']') {
      p.get();
      return true;
    }

    if (!first && !p.expect(',')) return false;
    first = false;

    T elem;
    if (!from_json_impl(p, elem)) return false;
    out.push_back(std::move(elem));
  }
}

// Visitable struct
template <typename T>
inline auto from_json_impl(JsonParser& p, T& out) -> std::enable_if_t<
  visit_struct::traits::is_visitable<std::decay_t<T>>::value,
  bool> {
  p.skip_whitespace();
  if (!p.expect('{')) return false;

  // Parse object fields
  bool first = true;
  while (true) {
    p.skip_whitespace();
    if (p.peek() == '}') {
      p.get();
      return true;
    }

    if (!first && !p.expect(',')) return false;
    first = false;

    // Parse field name
    std::string field_name;
    if (!parse_json_string(p, field_name)) return false;

    p.skip_whitespace();
    if (!p.expect(':')) return false;

    // Find and parse the matching field
    bool found = false;
    visit_struct::for_each(out, [&](const char* name, auto& value) {
      if (!found && field_name == name) {
        found = from_json_impl(p, value);
      }
    });

    // If field not found in struct, skip the value
    if (!found) {
      // Skip unknown value (simple skip - handles nested objects/arrays)
      int  depth     = 0;
      bool in_string = false;
      while (!p.eof()) {
        char c = p.peek();
        if (in_string) {
          p.get();
          if (c == '\\')
            p.get();  // skip escaped char
          else if (c == '"')
            in_string = false;
        } else {
          if (c == '"') {
            in_string = true;
            p.get();
          } else if (c == '{' || c == '[') {
            depth++;
            p.get();
          } else if (c == '}' || c == ']') {
            if (depth == 0) break;
            depth--;
            p.get();
          } else if (c == ',' && depth == 0) {
            break;
          } else {
            p.get();
          }
        }
      }
    }
  }
}

// Main from_json function
template <typename T>
inline bool from_json(const std::string& json, T& out) {
  JsonParser p(json);
  return from_json_impl(p, out);
}

// Convenience: returns a new object (throws on error)
template <typename T>
inline T from_json(const std::string& json) {
  T result{};
  if (!from_json(json, result)) {
    throw std::runtime_error("JSON parse error");
  }
  return result;
}

// Include array.h for custom array support
#include "basic/array.h"

// array<T> overload
template <typename T>
inline std::string to_json(
  const array<T>& arr, int indent = 0, bool pretty = true
) {
  return array_to_json(arr, indent, pretty);
}

template <typename T>
inline void save_to_json(const T& obj, const std::string& filename) {
  std::ofstream ofs(filename);
  if (!ofs) {
    throw std::runtime_error("Failed to open file for writing: " + filename);
  }
  ofs << to_json(obj, 0, true);
}

template <typename T>
inline T load_from_json(const std::string& filename) {
  std::ifstream ifs(filename);
  if (!ifs) {
    throw std::runtime_error("Failed to open file for reading: " + filename);
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  return from_json<T>(buffer.str());
}