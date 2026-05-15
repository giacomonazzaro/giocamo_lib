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

// Base visual entity with optional draw callback.
struct Thing {
  std::string name;
  int         id = 0;
  std::string image_path;
  Rectangle   rect     = {0.0f, 0.0f, 0.0f, 0.0f};
  float       rotation = 0.0f;
  // std::function<void(Thing&)> draw_callback;
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
  rect,
  rotation,
  face_up,
  depth,
  capacity,
  children,
  spread_x,
  spread_y
);

// A visual card — inherits all Thing fields.
struct KT_Card : Thing {
  using Thing::Thing;
};

// Drag operation in progress.
struct Drag_State {
  int   card_id            = -1;
  int   current_stack      = -1;
  int   last_hovered_stack = -1;
  int   original_stack     = -1;
  float offset_x = 0.0f, offset_y = 0.0f;
};

struct Table_Layout {
  std::vector<Thing> things;
  int                root = -1;  // Thing id of the scene-tree root.
};
VISITABLE_STRUCT(Table_Layout, things, root);

// Full table state passed to every render and input function.
struct Table_State : Table_Layout {
  // TODO: Remove, here we don't have assumptions about the fact that there are
  // cards.
  int num_cards = 0;  // Cards occupy ids [0, num_cards).

  //
  Drag_State         drag_state;
  std::vector<Thing> animated_cards;  // Smoothed mirror of `things`, used for
                                      // animation. Same indexing.
  std::unordered_map<int, std::function<void(Table_State*)>> draw_callbacks;
  int                                zoomed_card_id = -1;
  std::function<bool(int, int, int)> is_drop_card_allowed;
  std::optional<std::tuple<int, int, int>>
    dropped_card;  // (src_stack, dst_stack, card_id) after a drop.

  Table_State();
  Table_State(const Table_Layout& layout)
      : Table_Layout(layout),
        is_drop_card_allowed([](int, int, int) { return true; }) {}
  // Returns dropped_card and resets it to nullopt (consume-once event poll).
  std::optional<std::tuple<int, int, int>> poll_dropped_card();
};
