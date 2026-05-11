#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "raylib.h"

// Base visual entity with optional draw callback.
struct Thing {
  int id = 0;
  std::string image_path;
  float x = 0.0f, y = 0.0f;
  float rotation = 0.0f;
  std::function<void(Thing&)> draw_callback;
};

// A visual card — inherits all Thing fields.
struct KT_Card : Thing {
  using Thing::Thing;
};

// An ordered pile of cards with layout parameters.
struct Stack {
  Rectangle rect;
  std::vector<int> cards; // Ordered list of card IDs.
  float spread_x = 0.0f, spread_y = 0.0f;
  bool face_up = true;
  std::string name;
  float depth = 0.0f;
  int capacity = -1; // -1 = unlimited.
};

// Drag operation in progress.
struct Drag_State {
  int card_id = -1;
  int current_stack = -1;
  int last_hovered_stack = -1;
  int original_stack = -1;
  float offset_x = 0.0f, offset_y = 0.0f;
};

// Full table state passed to every render and input function.
struct Table_State {
  std::vector<KT_Card> cards;
  std::vector<Stack> stacks;
  std::vector<int> loose_cards; // KT_Card IDs of cards not in any stack.
  Drag_State drag_state;
  std::vector<KT_Card> animated_cards;
  std::function<void(Table_State*)> draw_callback;
  int zoomed_card_id = -1;
  std::function<bool(int, int, int)> is_drop_card_allowed;
  std::optional<std::tuple<int,int,int>> dropped_card; // (src_stack, dst_stack, card_id) after a drop.

  Table_State();
  // Returns dropped_card and resets it to nullopt (consume-once event poll).
  std::optional<std::tuple<int,int,int>> poll_dropped_card();
};
