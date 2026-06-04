#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "../struct/visit.hpp"
#include "config.h"
#include "raylib.h"

VISITABLE_STRUCT(Rectangle, x, y, width, height);
VISITABLE_STRUCT(Color, r, g, b, a);
VISITABLE_STRUCT(Vector2, x, y);

// Position and rotation of thing w.r.t. parent.
struct Transform2D {
  float x        = 0.0f;
  float y        = 0.0f;
  float rotation = 0.0f;
};
VISITABLE_STRUCT(Transform2D, x, y, rotation);

// Compose two transforms: `parent * child` is the transform that applies
// `child` first, then `parent`. The child's translation is rotated by the
// parent's rotation before being added — without that, a child anchored
// to a rotating parent would stay aligned to the parent's original axes.
// Rotation is in degrees.
inline Transform2D operator*(
  const Transform2D& parent, const Transform2D& child
) {
  float angle = parent.rotation * (float)(M_PI / 180.0);
  float cos_a = std::cos(angle);
  float sin_a = std::sin(angle);
  return Transform2D{
    parent.x + cos_a * child.x - sin_a * child.y,
    parent.y + sin_a * child.x + cos_a * child.y,
    parent.rotation + child.rotation,
  };
}

inline Transform2D inverse(const Transform2D& t) {
  float angle = -t.rotation * (float)(M_PI / 180.0);
  float cos_a = std::cos(angle);
  float sin_a = std::sin(angle);
  return Transform2D{
    -(cos_a * t.x - sin_a * t.y),
    -(sin_a * t.x + cos_a * t.y),
    -t.rotation,
  };
}

struct Counter {
  int value = 0;
  int min   = 0;
  int max   = 0;

  // Instead of making Counter optional, we just check if (counter) { ... }.
  // When min=max=0, the counter is  inactive and the value is not rendered.
  operator bool() const { return !(min == 00 && max == 0); }
};
VISITABLE_STRUCT(Counter, value, min, max);

struct Shape_Circle {
  float radius = 0.0f;
};
struct Shape_Hexagon {
  float radius = 0.0f;
};
struct Shape_Triangle {
  float radius = 0.0f;
};
struct Shape_Rectangle {
  Vector2 size = {0.0f, 0.0f};
  float   corner_radius =
    (float)tt::CARD_CORNER_RADIUS;  // formerly `rounded_corners`
};
VISITABLE_STRUCT(Shape_Circle, radius);
VISITABLE_STRUCT(Shape_Hexagon, radius);
VISITABLE_STRUCT(Shape_Triangle, radius);
VISITABLE_STRUCT(Shape_Rectangle, size, corner_radius);

enum struct Shape_Type {
  CIRCLE,
  HEXAGON,
  TRIANGLE,
  RECTANGLE,
};

struct Shape {
  Shape_Type type = Shape_Type::RECTANGLE;
  union {
    Shape_Circle    circle;
    Shape_Hexagon   hexagon;
    Shape_Triangle  triangle;
    Shape_Rectangle rectangle;
  };
  // Default to a rounded rectangle, so a default-constructed Shape is valid.
  Shape() : rectangle() {}
};

// Bounding-box size of any shape (used for layout, hit-testing, drawing).
inline Vector2 shape_size(const Shape& shape) {
  switch (shape.type) {
    case Shape_Type::RECTANGLE: return shape.rectangle.size;
    case Shape_Type::CIRCLE:
      return {shape.circle.radius * 2.0f, shape.circle.radius * 2.0f};
    case Shape_Type::HEXAGON:
      return {shape.hexagon.radius * 2.0f, shape.hexagon.radius * 2.0f};
    case Shape_Type::TRIANGLE:
      return {shape.triangle.radius * 2.0f, shape.triangle.radius * 2.0f};
  }
  return {0.0f, 0.0f};
}

// Point-in-convex-regular-polygon test, in the shape's local space (centered
// at the origin), matching how DrawPoly lays out its vertices.
inline bool point_in_regular_polygon(float x, float y, int sides, float radius) {
  bool has_positive = false;
  bool has_negative = false;
  for (int i = 0; i < sides; i++) {
    float angle_0 = (float)(2.0 * M_PI * i / sides);
    float angle_1 = (float)(2.0 * M_PI * (i + 1) / sides);
    float x_0     = std::cos(angle_0) * radius;
    float y_0     = std::sin(angle_0) * radius;
    float x_1     = std::cos(angle_1) * radius;
    float y_1     = std::sin(angle_1) * radius;
    // The sign of the cross product tells which side of the edge the point is.
    float cross = (x_1 - x_0) * (y - y_0) - (y_1 - y_0) * (x - x_0);
    if (cross > 0.0f) has_positive = true;
    if (cross < 0.0f) has_negative = true;
    if (has_positive && has_negative) return false;  // On both sides: outside.
  }
  return true;
}

// True if a point in the shape's local space (relative to its center) lies
// inside the shape.
inline bool point_in_shape(const Shape& shape, float x, float y) {
  switch (shape.type) {
    case Shape_Type::RECTANGLE: {
      float half_width  = shape.rectangle.size.x / 2.0f;
      float half_height = shape.rectangle.size.y / 2.0f;
      return -half_width <= x && x <= half_width && -half_height <= y &&
             y <= half_height;
    }
    case Shape_Type::CIRCLE: {
      float radius = shape.circle.radius;
      return x * x + y * y <= radius * radius;
    }
    case Shape_Type::HEXAGON:
      return point_in_regular_polygon(x, y, 6, shape.hexagon.radius);
    case Shape_Type::TRIANGLE:
      return point_in_regular_polygon(x, y, 3, shape.triangle.radius);
  }
  return false;
}

// Build a rounded-rectangle shape from a size, the default for most things.
inline Shape rectangle_shape(Vector2 size) {
  Shape shape;
  shape.type      = Shape_Type::RECTANGLE;
  shape.rectangle = Shape_Rectangle{size, (float)tt::CARD_CORNER_RADIUS};
  return shape;
}

inline Shape circle_shape(float size) {
  Shape shape;
  shape.type   = Shape_Type::CIRCLE;
  shape.circle = Shape_Circle{size / 2.0f};
  return shape;
}

// Base visual entity with optional draw callback.
struct Thing {
  // Info.
  std::string name = "thing";
  int         id   = 0;

  // Appearance.
  Color       color      = {255, 255, 255, 50};
  std::string image_path = "";

  Transform2D transform = {};

  // A thing was assumed to be a rectangle before, centered at (0,0).
  Shape   shape;
  Counter counter = {};
  bool    face_up = true;
  float   depth   = 0.0f;

  // Container
  int              capacity = -1;  // -1 = unlimited.
  std::vector<int> children;       // Ordered list of thing IDs.
  float            spread_x = 0.0f;
  float            spread_y = 0.0f;
};
VISITABLE_STRUCT(
  Thing,
  name,
  id,
  image_path,
  color,
  shape,
  counter,
  transform,
  face_up,
  depth,
  capacity,
  children,
  spread_x,
  spread_y
);

// Set a thing's local rectangle from a top-left rectangle expressed in the
// parent's local space. transform.x/y stores the center, so we offset by
// half the size.
inline void set_local_rect(Thing& thing, Rectangle rect) {
  thing.shape       = rectangle_shape({rect.width, rect.height});
  thing.transform.x = rect.x + rect.width / 2.0f;
  thing.transform.y = rect.y + rect.height / 2.0f;
}

// Local-space rectangle (top-left coords) of a thing.
inline Rectangle local_rect(const Thing& thing) {
  Vector2 size = shape_size(thing.shape);
  return Rectangle{
    thing.transform.x - size.x / 2.0f,
    thing.transform.y - size.y / 2.0f,
    size.x,
    size.y,
  };
}

// Build a full-window root thing centered on a width×height screen, textured
// with the given table surface and with square corners so it fills the screen.
// The caller assigns its id and children, then adds it to the table.
Thing create_table_root(int width, int height, const std::string& texture_path);

// Path of thing IDs from root to the thing.
using Thing_Location = std::vector<int>;

// Drag operation in progress.
struct Drag_State {
  // Root-to-thing path of the thing currently being dragged.
  Thing_Location dragged_thing;
  // Root-to-thing path of the candidate drop target under the cursor.
  Thing_Location hovered_thing;
  float          mouse_offset_x = 0.0f;
  float          mouse_offset_y = 0.0f;
  bool           allowed        = false;

  // Id of the thing currently being dragged, or -1 when no drag is active.
  inline int thing_id() const {
    return dragged_thing.empty() ? -1 : dragged_thing.back();
  }
  // Id of the dragged thing's parent, or -1 when there's no drag / the
  // dragged thing has no parent in the path.
  inline int parent_id() const {
    return dragged_thing.size() > 1 ? dragged_thing[dragged_thing.size() - 2]
                                    : -1;
  }
  // Id of the candidate drop target under the cursor, or -1 when nothing
  // is being hovered.
  inline int hovered_id() const {
    return hovered_thing.empty() ? -1 : hovered_thing.back();
  }
};
VISITABLE_STRUCT(
  Drag_State,
  dragged_thing,
  hovered_thing,
  mouse_offset_x,
  mouse_offset_y,
  allowed
);

struct Table_Layout {
  std::vector<Thing> things;
  int                root = -1;  // Thing id of the scene-tree root.
};
VISITABLE_STRUCT(Table_Layout, things, root);

struct Input;

// Full table state passed to every render and input function.
struct Table_State : Table_Layout {
  int width  = 0;
  int height = 0;

  Drag_State               drag_state;
  std::vector<Transform2D> world_transforms_animated;
  std::vector<Transform2D> world_transforms;

  // HUD/per-thing draw callbacks. Receive Input so they can run immediate-mode
  // buttons against the recorded/replayed input stream. The bool argument is
  // the face_up flag of the thing being decorated (true for the HUD slot).
  std::unordered_map<
    int,
    std::function<void(const Table_State&, const Input&, bool)>>
                                     draw_callbacks;
  Thing_Location                     zoomed_thing_id;
  std::function<bool(int, int, int)> is_drop_allowed;

  // (from_parent, to_parent, thing_id) after a drop.
  std::optional<std::tuple<int, int, int>> dropped_thing;

  // Returns dropped_thing and resets it to nullopt (consume-once event poll).
  inline std::optional<std::tuple<int, int, int>> poll_dropped_thing() {
    auto result   = dropped_thing;
    dropped_thing = std::nullopt;
    return result;
  }

  Table_State() {};
  Table_State(int width, int height, const Table_Layout& layout)
      : Table_Layout(layout)
      , width(width)
      , height(height)
      , is_drop_allowed([](int, int, int) { return true; }) {}
};

// Per-frame snapshot of every input that `tabletop/` code consumes.
// Built once at the top of each frame either by capture_input() (live mode)
// or pulled from a recorded array (playback mode). Every tabletop function
// that needs to know about input takes a `const Input&` instead of calling
// raylib directly, so the entire interaction stream can be recorded/replayed.
//
// Keys are stored as raylib KEY_* codes. capture_input() only watches a fixed
// set of keys (see input.cpp). To make a new key recordable, add it to the
// watched lists in capture_input() — call sites then just use key_pressed()
// or key_down() with the new code.
struct Input {
  int  mouse_x       = 0;
  int  mouse_y       = 0;
  bool left_pressed  = false;  // IsMouseButtonPressed(MOUSE_BUTTON_LEFT).
  bool left_released = false;  // IsMouseButtonReleased(MOUSE_BUTTON_LEFT).
  // Raylib KEY_* codes triggered this frame (IsKeyPressed).
  std::vector<int> keys_pressed;
  // Raylib KEY_* codes held this frame (IsKeyDown).
  std::vector<int> keys_down;
  // Characters produced this frame (GetCharPressed loop result).
  std::string chars_typed;

  float time       = 0.0f;  // Seconds since the start of the app.
  float delta_time = 0.0f;  // Seconds since the previous frame.
};
VISITABLE_STRUCT(
  Input,
  mouse_x,
  mouse_y,
  left_pressed,
  left_released,
  keys_pressed,
  keys_down,
  chars_typed,
  time,
  delta_time
);

// Reads the current frame's input from raylib. This is the ONLY place in
// `tabletop/` that calls raylib input functions directly.
Input capture_input();

inline bool key_pressed(const Input& input, int key) {
  return std::find(input.keys_pressed.begin(), input.keys_pressed.end(), key) !=
         input.keys_pressed.end();
}
inline bool key_down(const Input& input, int key) {
  return std::find(input.keys_down.begin(), input.keys_down.end(), key) !=
         input.keys_down.end();
}

// True if `thing` has a capacity limit and has reached it.
bool is_full(const Thing& thing);
// Hit-test `thing` against world point (px, py) using its accumulated world
// rect.
bool point_in_thing(float px, float py, int thing_id, const Table_State& state);
bool thing_pressed(int thing_id, const Table_State& state, const Input& input);
// Returns the scene-tree path from root down to the topmost thing whose world
// rect contains (px, py). Topmost is determined by reverse-DFS (the
// last-drawn / visually frontmost thing wins). Empty when nothing matched.
Thing_Location find_thing_at(float px, float py, const Table_State& state);
void           handle_mouse_press(Table_State& state, const Input& input);
void           handle_mouse_release(Table_State& state);
void           handle_mouse_move(Table_State& state, const Input& input);
void           handle_rotate_thing(
            Table_State& state, const Input& input, bool clockwise = true
          );
void shuffle_thing(Table_State& state, int thing_id);
void process_input(Table_State& state, const Input& input);

Rectangle world_rect(int thing_id, const Table_State& state);

// Reflow a thing's children into their slot positions based on the parent's
// spread_x / spread_y.
void update_children_positions(int parent_id, Table_State& state, bool sort);

Thing make_card(int id, const std::string& image_path = "");

template <typename F>
void visit_things_recursive(
  Table_State& table, int parent_id, int thing_id, F&& f
) {
  f(table, parent_id, thing_id);
  for (int child_id : table.things[thing_id].children)
    visit_things_recursive(table, thing_id, child_id, f);
}

template <typename F>
void visit_things(Table_State& table, F&& f) {
  visit_things_recursive(table, -1, table.root, f);
}

inline void update_local_transform_to_match_world_transform(
  Table_State& table, int parent_id, int thing_id
) {
  // world_transform = new_parent_world * new_local
  // new_local = inverse(new_parent_world) * world_transform
  table.things[thing_id].transform =
    inverse(table.world_transforms[parent_id]) *
    table.world_transforms[thing_id];
}

inline void update_local_transforms_to_match_world_transforms(
  Table_State& table
) {
  auto f = [&](Table_State& table, int parent_id, int thing_id) {
    if (parent_id == -1) return;
    update_local_transform_to_match_world_transform(table, parent_id, thing_id);
  };
  visit_things(table, f);
}