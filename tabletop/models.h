#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "../struct/visit.hpp"
#include "raylib.h"

VISITABLE_STRUCT(Rectangle, x, y, width, height);
VISITABLE_STRUCT(Color, r, g, b, a);

// Base visual entity with optional draw callback.
struct Thing {
  std::string name;
  int         id = 0;
  std::string image_path;
  Color       color;
  Rectangle   rect     = {255.0f, 255.0f, 255.0f, 100.0f};
  float       rotation = 0.0f;
  bool        face_up  = true;
  float       depth    = 0.0f;

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
  rect,
  rotation,
  face_up,
  depth,
  capacity,
  children,
  spread_x,
  spread_y
);

// Path of thing IDs from root to the thing.
using Thing_Location = std::vector<int>;

// A thing's pose in the global frame of the table. The animation pipeline
// works in this space so that re-parenting (e.g. a card moving from the
// table stack into a player's captured pile) is just a target swap rather
// than a coord-system swap. The renderer consumes these directly.
struct World_Transform {
  float x        = 0.0f;
  float y        = 0.0f;
  float rotation = 0.0f;
};

// Drag operation in progress.
struct Drag_State {
  // Root-to-thing path captured when the drag started. Empty when no drag
  // is in progress.
  Thing_Location location;
  int            current_stack      = -1;
  int            last_hovered_stack = -1;
  int            original_stack     = -1;
  float          offset_x = 0.0f, offset_y = 0.0f;
};

// Id of the thing currently being dragged, or -1 when no drag is active.
inline int dragged_thing_id(const Drag_State& drag) {
  return drag.location.empty() ? -1 : drag.location.back();
}

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

  // TODO: Remove, here we don't have assumptions about the fact that there are
  // cards.
  int num_cards = 0;  // Cards occupy ids [0, num_cards).

  //
  Drag_State                   drag_state;
  std::vector<World_Transform> animated_world;  // Smoothed world transform
                                                // per thing, same indexing as
                                                // `things`. The renderer
                                                // reads these directly.
  // HUD/per-thing draw callbacks. Receive Input so they can run immediate-mode
  // buttons against the recorded/replayed input stream. The bool argument is
  // the face_up flag of the thing being decorated (true for the HUD slot).
  std::unordered_map<
    int, std::function<void(const Table_State&, const Input&, bool)>>
                                     draw_callbacks;
  Thing_Location                     zoomed_card_id;
  std::function<bool(int, int, int)> is_drop_card_allowed;

  // (src_stack, dst_stack, card_id) after a drop.
  std::optional<std::tuple<int, int, int>> dropped_card;

  // Returns dropped_card and resets it to nullopt (consume-once event poll).
  inline std::optional<std::tuple<int, int, int>> poll_dropped_card() {
    auto result  = dropped_card;
    dropped_card = std::nullopt;
    return result;
  }

  Table_State() {};
  Table_State(int width, int height, const Table_Layout& layout)
      : Table_Layout(layout)
      , width(width)
      , height(height)
      , is_drop_card_allowed([](int, int, int) { return true; }) {}
};
