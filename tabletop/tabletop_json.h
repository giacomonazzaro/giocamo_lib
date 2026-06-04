#pragma once
#include "../struct/json.h"
#include "tabletop.h"

// The generic JSON serializer walks a struct's visitable members, but it can't
// walk a C++ union. So the Shape union gets hand-written JSON: the shape type
// plus the one active member. The member structs themselves are visitable, so
// we delegate them to the generic serializer.

inline std::string to_json(const Shape& shape, int indent = 0, bool pretty = true) {
  std::string result = "{" + json_newline(pretty);
  result += json_indent(indent + 1, pretty) + "\"type\":" + json_space(pretty) +
            to_json((int)shape.type, indent + 1, pretty) + "," +
            json_newline(pretty);
  result += json_indent(indent + 1, pretty);
  switch (shape.type) {
    case Shape_Type::RECTANGLE:
      result += "\"rectangle\":" + json_space(pretty) +
                to_json(shape.rectangle, indent + 1, pretty);
      break;
    case Shape_Type::CIRCLE:
      result += "\"circle\":" + json_space(pretty) +
                to_json(shape.circle, indent + 1, pretty);
      break;
    case Shape_Type::HEXAGON:
      result += "\"hexagon\":" + json_space(pretty) +
                to_json(shape.hexagon, indent + 1, pretty);
      break;
    case Shape_Type::TRIANGLE:
      result += "\"triangle\":" + json_space(pretty) +
                to_json(shape.triangle, indent + 1, pretty);
      break;
  }
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
      int type_value = 0;
      ok       = from_json_impl(p, type_value);
      out.type = (Shape_Type)type_value;
    } else if (field_name == "rectangle") {
      ok = from_json_impl(p, out.rectangle);
    } else if (field_name == "circle") {
      ok = from_json_impl(p, out.circle);
    } else if (field_name == "hexagon") {
      ok = from_json_impl(p, out.hexagon);
    } else if (field_name == "triangle") {
      ok = from_json_impl(p, out.triangle);
    } else {
      return false;
    }
    if (!ok) return false;
  }
}
