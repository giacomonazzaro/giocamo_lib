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

// Base visual entity with optional draw callback.
struct Thing {
  // Info.
  std::string name;
  int         id = 0;

  // Appearance.
  Color       color = {255, 255, 255, 50};
  std::string image_path;

  // Geometry. A thing is assumed to be a rectangle for now, centered at (0,0).
  Transform2D transform;
  Vector2     size = {(float)tt::CARD_WIDTH, (float)tt::CARD_HEIGHT};

  bool  face_up = true;
  float depth   = 0.0f;

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
  size,
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
  thing.size        = {rect.width, rect.height};
  thing.transform.x = rect.x + rect.width / 2.0f;
  thing.transform.y = rect.y + rect.height / 2.0f;
}

// Local-space rectangle (top-left coords) of a thing.
inline Rectangle local_rect(const Thing& thing) {
  return Rectangle{
    thing.transform.x - thing.size.x / 2.0f,
    thing.transform.y - thing.size.y / 2.0f,
    thing.size.x,
    thing.size.y,
  };
}

// Path of thing IDs from root to the thing.
using Thing_Location = std::vector<int>;

// Drag operation in progress.
struct Drag_State {
  // Root-to-thing path captured when the drag started. Empty when no drag
  // is in progress.
  Thing_Location location;
  int            hovered_thing = -1;
  // int            current_parent      = -1;
  // int            last_hovered_parent = -1;
  // int            original_parent     = -1;
  float mouse_offset_x = 0.0f;
  float mouse_offset_y = 0.0f;

  // Id of the thing currently being dragged, or -1 when no drag is active.
  inline int thing_id() const {
    return location.empty() ? -1 : location.back();
  }
  inline int parent_id() const {
    return location.size() > 1 ? location[location.size() - 2] : -1;
  }
};
VISITABLE_STRUCT(
  Drag_State, location, hovered_thing, mouse_offset_x, mouse_offset_y
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
  std::vector<Transform2D> animated_transforms;
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
};
VISITABLE_STRUCT(
  Input,
  mouse_x,
  mouse_y,
  left_pressed,
  left_released,
  keys_pressed,
  keys_down,
  chars_typed
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
void handle_mouse_press(Table_State& state, const Input& input);
void handle_mouse_release(Table_State& state);
void handle_mouse_move(Table_State& state, const Input& input);
void handle_rotate_thing(
  Table_State& state, const Input& input, bool clockwise = true
);
void shuffle_thing(Table_State& state, int thing_id);
void process_input(Table_State& state, const Input& input);

Rectangle world_rect(int thing_id, const Table_State& state);

// Reflow a thing's children into their slot positions based on the parent's
// spread_x / spread_y.
void update_children_positions(int parent_id, Table_State& state, bool sort);

Thing make_card(int id, const std::string& image_path = "");