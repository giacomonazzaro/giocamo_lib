#pragma once
#include "../struct/json.h"
#include "tabletop.h"

// The generic JSON serializer walks a struct's visitable members, and a variant
// is not a struct, so Shape gets written by hand: the index of the live
// alternative plus that alternative under its own name. The alternatives are
// visitable, so the generic serializer still does the actual work. The shape of
// the output has not changed, so files written before Shape became a variant
// still load.

inline std::string to_json(const Shape& shape, int indent = 0, bool pretty = true) {
  std::string result = "{" + json_newline(pretty);
  result += json_indent(indent + 1, pretty) + "\"type\":" + json_space(pretty) +
            to_json((int)shape.index(), indent + 1, pretty) + "," +
            json_newline(pretty);
  result += json_indent(indent + 1, pretty);
  result += std::visit(
    [&](const auto& alternative) {
      using S         = std::decay_t<decltype(alternative)>;
      const char* key = std::is_same_v<S, Shape_Rectangle> ? "rectangle"
                        : std::is_same_v<S, Shape_Circle>  ? "circle"
                        : std::is_same_v<S, Shape_Hexagon> ? "hexagon"
                                                           : "triangle";
      return std::string("\"") + key + "\":" + json_space(pretty) +
             to_json(alternative, indent + 1, pretty);
    },
    shape
  );
  result += json_newline(pretty) + json_indent(indent, pretty) + "}";
  return result;
}

inline bool from_json_impl(JsonParser& p, Shape& out) {
  p.skip_whitespace();
  if (!p.expect('{')) return false;
  bool first = true;
  while (true) {
    p.skip_whitespace();
    if (p.peek() == '}') {
      p.get();
      return true;
    }
    if (!first && !p.expect(',')) return false;
    first = false;

    std::string field_name;
    if (!parse_json_string(p, field_name)) return false;
    p.skip_whitespace();
    if (!p.expect(':')) return false;

    bool ok = true;
    if (field_name == "type") {
      // Read and dropped: the member name below already says which shape this
      // is. Still parsed so files that carry it stay readable.
      int type_value = 0;
      ok             = from_json_impl(p, type_value);
    } else if (field_name == "rectangle") {
      Shape_Rectangle value;
      ok  = from_json_impl(p, value);
      out = value;
    } else if (field_name == "circle") {
      Shape_Circle value;
      ok  = from_json_impl(p, value);
      out = value;
    } else if (field_name == "hexagon") {
      Shape_Hexagon value;
      ok  = from_json_impl(p, value);
      out = value;
    } else if (field_name == "triangle") {
      Shape_Triangle value;
      ok  = from_json_impl(p, value);
      out = value;
    } else {
      return false;
    }
    if (!ok) return false;
  }
}
