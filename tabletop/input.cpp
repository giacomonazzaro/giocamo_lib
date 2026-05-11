#include "input.h"

#include <algorithm>
#include <cstdio>
#include <random>

#include "config.h"
#include "game_state.h"
#include "raylib.h"

bool stack_is_full(const Thing& stack) {
  // True if the container has a capacity limit and has reached it.
  return stack.capacity >= 0 && (int)stack.children.size() >= stack.capacity;
}

bool point_in_card(float px, float py, int card_id, const Table_State& state) {
  // Resolve world rect: card.rect is local; CARD_WIDTH/HEIGHT defines bounds.
  Vector2 world = local_to_world(card_id, state);
  float   w     = (float)tt::CARD_WIDTH;
  float   h     = (float)tt::CARD_HEIGHT;
  return (world.x <= px && px <= world.x + w && world.y <= py &&
          py <= world.y + h);
}

bool card_pressed(int card_id, const Table_State& state) {
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();
  return point_in_card(mx, my, card_id, state);
}

bool point_in_stack_area(
  float px, float py, int stack_id, const Table_State& state
) {
  // Use the stack's world rect (width/height are stored locally; world rect
  // computes their world-space position).
  Rectangle r = world_rect(stack_id, state);
  float     h = (float)tt::CARD_HEIGHT;
  // Match the original semantics: vertical span is one card height.
  return (r.x <= px && px <= r.x + r.width && r.y <= py && py <= r.y + h);
}

std::optional<std::pair<int, int>> find_card_at(
  float px, float py, Table_State& state
) {
  // Topmost card under (px, py). Iterate root's children in reverse so the
  // last-drawn (visually topmost) hits first.
  const auto& root_children = state.things[state.root].children;
  for (int i = (int)root_children.size() - 1; i >= 0; i--) {
    int          child_id = root_children[i];
    const Thing& child    = state.things[child_id];
    if (is_container(child)) {
      // Stack: walk its cards in reverse for top-of-pile hit testing.
      for (int j = (int)child.children.size() - 1; j >= 0; j--) {
        int card_id = child.children[j];
        if (point_in_card(px, py, card_id, state))
          return std::make_pair(card_id, child_id);
      }
    } else {
      // Loose card directly under root.
      if (point_in_card(px, py, child_id, state))
        return std::make_pair(child_id, state.root);
    }
  }
  return std::nullopt;
}

int find_stack_at(float px, float py, const Table_State& state) {
  // Topmost container child of root whose world rect contains the point.
  const auto& root_children = state.things[state.root].children;
  for (int i = (int)root_children.size() - 1; i >= 0; i--) {
    int child_id = root_children[i];
    if (!is_container(state.things[child_id])) continue;
    if (point_in_stack_area(px, py, child_id, state)) return child_id;
  }
  return -1;
}

void handle_mouse_press(Table_State& state) {
  // Mouse pressed — begin drag if a card is under the cursor.
  float       mx   = (float)GetMouseX();
  float       my   = (float)GetMouseY();
  Drag_State& drag = state.drag_state;

  auto result = find_card_at(mx, my, state);
  if (!result) return;

  int card_id  = result->first;
  int stack_id = result->second;

  drag.card_id        = card_id;
  drag.current_stack  = stack_id;
  drag.original_stack = stack_id;
  // Drag offset in world coords so it's parent-agnostic during hover.
  Vector2 card_world = local_to_world(card_id, state);
  drag.offset_x      = mx - card_world.x;
  drag.offset_y      = my - card_world.y;
}

void handle_mouse_release(Table_State& state) {
  // Mouse released — finalize the drop.
  Drag_State& drag = state.drag_state;
  if (drag.card_id < 0) return;

  bool allowed = state.is_drop_card_allowed(
    drag.original_stack, drag.current_stack, drag.card_id
  );

  if (!allowed) {
    // Snap back: re-attach to original parent if we're floating without one.
    if (drag.current_stack == -1) {
      state.things[drag.original_stack].children.push_back(drag.card_id);
    }
  }

  // Signal drop as (from_stack, to_stack, card_id).
  state.dropped_card =
    std::make_tuple(drag.original_stack, drag.current_stack, drag.card_id);

  int original_stack = drag.original_stack;
  int current_stack  = drag.current_stack;
  state.drag_state   = Drag_State();

  if (original_stack >= 0)
    update_card_positions(original_stack, state, /*sort=*/true);
  if (current_stack >= 0 && current_stack != original_stack)
    update_card_positions(current_stack, state, /*sort=*/true);
}

void handle_mouse_move(Table_State& state) {
  // Continuously update dragged card position.
  Drag_State& drag = state.drag_state;
  if (drag.card_id < 0) return;

  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();

  // Target world position for the dragged card.
  float target_world_x = mx - drag.offset_x;
  float target_world_y = my - drag.offset_y;

  int hovered_stack = find_stack_at(mx, my, state);

  if (hovered_stack >= 0 && drag.last_hovered_stack != hovered_stack) {
    // Reparent: remove from current stack, optionally add to hovered stack.
    if (drag.current_stack >= 0) {
      auto& cur = state.things[drag.current_stack].children;
      auto  it  = std::find(cur.begin(), cur.end(), drag.card_id);
      if (it != cur.end()) {
        cur.erase(it);
        update_card_positions(drag.current_stack, state, /*sort=*/true);
      }
    }

    Thing& hovered = state.things[hovered_stack];
    bool   allowed = state.is_drop_card_allowed(
      drag.original_stack, hovered_stack, drag.card_id
    );
    bool full = stack_is_full(hovered);
    fprintf(
      stderr,
      "[drag] orig=%d hover=%d card=%d allowed=%d full=%d\n",
      drag.original_stack,
      hovered_stack,
      drag.card_id,
      (int)allowed,
      (int)full
    );

    if (allowed && !full) {
      hovered.children.push_back(drag.card_id);
      update_card_positions(hovered_stack, state, /*sort=*/true);
      drag.current_stack = hovered_stack;
    } else {
      drag.current_stack = -1;
    }
  }

  if (hovered_stack >= 0) {
    update_card_positions(hovered_stack, state, /*sort=*/true);
    drag.last_hovered_stack = hovered_stack;
  }

  // Now position the card under the cursor — convert world → local of parent.
  Thing&  card = state.things[drag.card_id];
  int     parent_id = (drag.current_stack >= 0) ? drag.current_stack : state.root;
  Vector2 parent_world = local_to_world(parent_id, state);
  card.rect.x          = target_world_x - parent_world.x;
  card.rect.y          = target_world_y - parent_world.y;
}

void handle_rotate_card(Table_State& state, bool clockwise) {
  // Rotate the card under the cursor by 90 degrees.
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();

  auto result = find_card_at(mx, my, state);
  if (!result) return;

  int    card_id = result->first;
  Thing& card    = state.things[card_id];
  if (clockwise)
    card.rotation = card.rotation + 90;
  else
    card.rotation = card.rotation - 90;
}

void shuffle_stack(Table_State& state, int stack_id) {
  if (stack_id < 0) return;

  Thing&              stack = state.things[stack_id];
  static std::mt19937 rng{std::random_device{}()};
  std::shuffle(stack.children.begin(), stack.children.end(), rng);
  update_card_positions(stack_id, state, /*sort=*/false);
}

void update_input(Table_State& state) {
  // Per-frame input processing.
  state.dropped_card = std::nullopt;

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    handle_mouse_press(state);
  } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    handle_mouse_release(state);
  }

  handle_mouse_move(state);

  if (IsKeyPressed(KEY_R)) {
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    handle_rotate_card(state, /*clockwise=*/!shift);
  }

  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();

  if (IsKeyDown(KEY_SPACE)) {
    auto result          = find_card_at(mx, my, state);
    state.zoomed_card_id = result ? result->first : -1;
  } else {
    state.zoomed_card_id = -1;
  }

  if (IsKeyPressed(KEY_S)) {
    int stack_id = find_stack_at(mx, my, state);
    shuffle_stack(state, stack_id);
  }
}
