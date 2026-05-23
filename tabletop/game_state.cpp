#include "game_state.h"

#include <algorithm>

#include "config.h"
#include "raylib.h"

int find_parent(int thing_id, const Table_State& state) {
  // Linear scan: tree depth is small (2) and total things is in the hundreds.
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

void update_card_positions(int stack_id, Table_State& state, bool sort) {
  // Set each child's local rect (x, y) within the stack based on spread.
  Thing& stack = state.things[stack_id];
  size_t n     = stack.children.size();

  if (sort && n > 0) {
    // Sort children by their current local x position.
    std::sort(
      stack.children.begin(), stack.children.end(), [&state](int a, int b) {
        return state.things[a].rect.x < state.things[b].rect.x;
      }
    );
  }

  if (n == 0) return;

  float spread_x   = stack.spread_x;
  float spread_y   = stack.spread_y;
  float card_width = static_cast<float>(tt::CARD_WIDTH);

  // Adaptive spread: shrink if cards would exceed stack width.
  if (n > 1 && stack.rect.width > 0.0f && spread_x != 0.0f) {
    float total_width = static_cast<float>(n - 1) * spread_x + card_width;
    if (total_width > stack.rect.width) {
      spread_x = (stack.rect.width - card_width) / static_cast<float>(n - 1);
    }
  }

  float total_spread_x = (n > 1) ? static_cast<float>(n - 1) * spread_x : 0.0f;
  float total_spread_y = (n > 1) ? static_cast<float>(n - 1) * spread_y : 0.0f;

  // In local space: center horizontally inside the stack's rect.
  float mid_x_local   = (stack.rect.width > 0.0f) ? stack.rect.width / 2.0f
                                                  : 0.0f;
  float start_x_local = mid_x_local - (total_spread_x + card_width) / 2.0f;
  // Vertical: cards float around the stack's top edge as in the original.
  float start_y_local = -total_spread_y / 2.0f;

  int drag_id = dragged_thing_id(state.drag_state);
  for (int i = 0; i < (int)n; i++) {
    int card_id = stack.children[i];
    if (card_id != drag_id) {
      Thing& card = state.things[card_id];
      card.rect.x = start_x_local + static_cast<float>(i) * spread_x;
      card.rect.y = start_y_local + static_cast<float>(i) * spread_y;
    }
  }
}
