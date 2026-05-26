#include "game_state.h"

#include <algorithm>

#include "config.h"
#include "raylib.h"

int find_parent(int thing_id, const Table_State& state) {
  // Linear scan: tree depth is small and total things is in the hundreds.
  for (int i = 0; i < (int)state.things.size(); ++i) {
    const auto& children = state.things[i].children;
    if (std::find(children.begin(), children.end(), thing_id) != children.end())
      return i;
  }
  return -1;
}

Vector2 local_to_world(int thing_id, const Table_State& state) {
  // Walk up the parent chain summing local rects; stop at root or detached.
  float x = 0.0f, y = 0.0f;
  int   cur = thing_id;
  while (cur >= 0) {
    const Thing& t = state.things[cur];
    x += t.rect.x;
    y += t.rect.y;
    if (cur == state.root) break;
    cur = find_parent(cur, state);
  }
  return Vector2{x, y};
}

Rectangle world_rect(int thing_id, const Table_State& state) {
  Vector2      p = local_to_world(thing_id, state);
  const Thing& t = state.things[thing_id];
  return Rectangle{p.x, p.y, t.rect.width, t.rect.height};
}

void update_children_positions(int parent_id, Table_State& state, bool sort) {
  Thing& parent = state.things[parent_id];
  size_t n      = parent.children.size();

  if (sort && n > 0) {
    // Sort children by their current local x position.
    std::sort(
      parent.children.begin(), parent.children.end(), [&state](int a, int b) {
        return state.things[a].rect.x < state.things[b].rect.x;
      }
    );
  }

  if (n == 0) return;

  float spread_x   = parent.spread_x;
  float spread_y   = parent.spread_y;
  float child_width = static_cast<float>(tt::CARD_WIDTH);

  // Adaptive spread: shrink if children would exceed the parent's width.
  if (n > 1 && parent.rect.width > 0.0f && spread_x != 0.0f) {
    float total_width = static_cast<float>(n - 1) * spread_x + child_width;
    if (total_width > parent.rect.width) {
      spread_x = (parent.rect.width - child_width) / static_cast<float>(n - 1);
    }
  }

  float total_spread_x = (n > 1) ? static_cast<float>(n - 1) * spread_x : 0.0f;
  float total_spread_y = (n > 1) ? static_cast<float>(n - 1) * spread_y : 0.0f;

  // In local space: center horizontally inside the parent's rect.
  float mid_x_local   = (parent.rect.width > 0.0f) ? parent.rect.width / 2.0f
                                                   : 0.0f;
  float start_x_local = mid_x_local - (total_spread_x + child_width) / 2.0f;
  float start_y_local = -total_spread_y / 2.0f;

  int drag_id = dragged_thing_id(state.drag_state);
  for (int i = 0; i < (int)n; i++) {
    int child_id = parent.children[i];
    if (child_id != drag_id) {
      Thing& child = state.things[child_id];
      child.rect.x = start_x_local + static_cast<float>(i) * spread_x;
      child.rect.y = start_y_local + static_cast<float>(i) * spread_y;
    }
  }
}
