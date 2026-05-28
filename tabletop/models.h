#pragma once
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
    return location.size() > 2 ? location[location.size() - 2] : -1;
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
