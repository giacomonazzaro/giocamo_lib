#include "input.h"

#include <algorithm>
#include <cstdio>
#include <random>

#include "config.h"
#include "game_state.h"
#include "raylib.h"

bool stack_is_full(const Stack& stack) {
  // Returns true if the stack has a capacity limit and has reached it.
  return stack.capacity >= 0 && (int)stack.cards.size() >= stack.capacity;
}

bool point_in_card(float px, float py, const Thing& card) {
  // Check if point (px, py) is inside the card bounds.
  float w = (float)kt::CARD_WIDTH;
  float h = (float)kt::CARD_HEIGHT;
  return (card.x <= px && px <= card.x + w && card.y <= py && py <= card.y + h);
}

bool card_pressed(const Thing& card) {
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();
  return point_in_card(mx, my, card);
}

bool point_in_stack_area(float px, float py, const Stack& stack) {
  // Check if point is in the stack's general area (for drop targets).
  float h = (float)kt::CARD_HEIGHT;
  float x = stack.rect.x;
  float y = stack.rect.y;
  float w = stack.rect.width;
  return (x <= px && px <= x + w && y <= py && py <= y + h);
}

std::optional<std::pair<int, int>>
find_card_at(float px, float py, Table_State& state) {
  // Find the topmost card at position. stack_index is -1 for loose cards.

  // Check loose cards first (on top of everything).
  for (int i = (int)state.loose_cards.size() - 1; i >= 0; i--) {
    int card_id = state.loose_cards[i];
    if (point_in_card(px, py, state.cards[card_id]))
      return std::make_pair(card_id, -1);
  }

  // Check stacks (reverse order so we check top cards first).
  for (int stack_idx = (int)state.stacks.size() - 1; stack_idx >= 0;
       stack_idx--) {
    const Stack& stack = state.stacks[stack_idx];
    for (int j = (int)stack.cards.size() - 1; j >= 0; j--) {
      int card_id = stack.cards[j];
      if (point_in_card(px, py, state.cards[card_id]))
        return std::make_pair(card_id, stack_idx);
    }
  }
  return std::nullopt;
}

int find_stack_at(float px, float py, const Table_State& state) {
  // Find a stack at the given position. Returns stack index or -1.
  for (int i = 0; i < (int)state.stacks.size(); i++) {
    if (point_in_stack_area(px, py, state.stacks[i])) return i;
  }
  return -1;
}

void handle_mouse_press(Table_State& state) {
  // Handle mouse button press — start drag.
  float       mx   = (float)GetMouseX();
  float       my   = (float)GetMouseY();
  Drag_State& drag = state.drag_state;

  auto result = find_card_at(mx, my, state);
  if (!result) return;

  int   card_id   = result->first;
  int   stack_idx = result->second;
  KT_Card& card      = state.cards[card_id];

  drag.card_id        = card_id;
  drag.current_stack  = stack_idx;
  drag.original_stack = stack_idx;
  drag.offset_x       = mx - card.x;
  drag.offset_y       = my - card.y;
}

void handle_mouse_release(Table_State& state) {
  // Handle mouse button release — drop card.
  Drag_State& drag = state.drag_state;
  if (drag.card_id < 0) return;

  bool allowed = state.is_drop_card_allowed(
    drag.original_stack, drag.current_stack, drag.card_id
  );

  if (!allowed) {
    if (drag.current_stack == -1) {
      state.stacks[drag.original_stack].cards.push_back(drag.card_id);
    }
  }

  // Signal the drop event as (from_stack, to_stack, card_id) — matches Python original.
  state.dropped_card =
    std::make_tuple(drag.original_stack, drag.current_stack, drag.card_id);

  int original_stack = drag.original_stack;
  int current_stack  = drag.current_stack;
  state.drag_state   = Drag_State();

  update_card_positions(state.stacks[original_stack], state, /*sort=*/true);

  if (current_stack >= 0) {
    update_card_positions(state.stacks[current_stack], state, /*sort=*/true);
  }
}

void handle_mouse_move(Table_State& state) {
  // Update dragged card position.
  Drag_State& drag = state.drag_state;
  if (drag.card_id < 0) return;

  float mx   = (float)GetMouseX();
  float my   = (float)GetMouseY();
  KT_Card& card = state.cards[drag.card_id];
  card.x     = mx - drag.offset_x;
  card.y     = my - drag.offset_y;

  int hovered_stack = find_stack_at(mx, my, state);
  if (hovered_stack < 0) return;

  if (drag.last_hovered_stack != hovered_stack) {
    // Remove card from current stack if it's there.
    if (drag.current_stack >= 0) {
      Stack& cur = state.stacks[drag.current_stack];
      auto   it  = std::find(cur.cards.begin(), cur.cards.end(), drag.card_id);
      if (it != cur.cards.end()) {
        cur.cards.erase(it);
        update_card_positions(cur, state, /*sort=*/true);
      }
    }

    Stack& hovered = state.stacks[hovered_stack];
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
      hovered.cards.push_back(drag.card_id);
      update_card_positions(hovered, state, /*sort=*/true);
      drag.current_stack = hovered_stack;
    } else {
      drag.current_stack = -1;
    }
  }

  Stack& hovered = state.stacks[hovered_stack];
  update_card_positions(hovered, state, /*sort=*/true);
  drag.last_hovered_stack = hovered_stack;
}

void handle_rotate_card(Table_State& state, bool clockwise) {
  // Rotate the card under the cursor by 90 degrees.
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();

  auto result = find_card_at(mx, my, state);
  if (!result) return;

  int   card_id = result->first;
  KT_Card& card    = state.cards[card_id];
  if (clockwise)
    card.rotation = card.rotation + 90;
  else
    card.rotation = card.rotation - 90;
}

void shuffle_stack(Table_State& state, int stack_id) {
  if (stack_id == -1) return;

  Stack&              stack = state.stacks[stack_id];
  static std::mt19937 rng{std::random_device{}()};
  std::shuffle(stack.cards.begin(), stack.cards.end(), rng);
  update_card_positions(stack, state, /*sort=*/false);
}

void update_input(Table_State& state) {
  // Main input processing — call each frame.
  state.dropped_card = std::nullopt;

  // Handle mouse.
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    handle_mouse_press(state);
  } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    handle_mouse_release(state);
  }

  // Update drag position continuously.
  handle_mouse_move(state);

  // Handle card rotation.
  if (IsKeyPressed(KEY_R)) {
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    handle_rotate_card(state, /*clockwise=*/!shift);
  }

  // Handle card zoom.
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();

  if (IsKeyDown(KEY_SPACE)) {
    auto result = find_card_at(mx, my, state);
    state.zoomed_card_id = result ? result->first : -1;
  } else {
    state.zoomed_card_id = -1;
  }

  if (IsKeyPressed(KEY_S)) {
    int stack_id = find_stack_at(mx, my, state);
    shuffle_stack(state, stack_id);
  }
}

#ifdef KT_BUILD_PYTHON

#include <nanobind/stl/optional.h>
using namespace nb::literals;

void bind_input(nb::module_& m) {
  m.def("stack_is_full", &stack_is_full, "stack"_a);
  m.def("point_in_card", &point_in_card, "px"_a, "py"_a, "card"_a);
  m.def("card_pressed", &card_pressed, "card"_a);
  m.def("point_in_stack_area", &point_in_stack_area, "px"_a, "py"_a, "stack"_a);
  m.def(
    "find_card_at",
    [](float px, float py, Table_State& state) -> nb::object {
      auto r = find_card_at(px, py, state);
      if (!r) return nb::none();
      return nb::make_tuple(r->first, r->second);
    },
    "px"_a,
    "py"_a,
    "state"_a
  );
  m.def("find_stack_at", &find_stack_at, "px"_a, "py"_a, "state"_a);
  m.def("handle_mouse_press", &handle_mouse_press, "state"_a);
  m.def("handle_mouse_release", &handle_mouse_release, "state"_a);
  m.def("handle_mouse_move", &handle_mouse_move, "state"_a);
  m.def(
    "handle_rotate_card", &handle_rotate_card, "state"_a, "clockwise"_a = true
  );
  m.def("shuffle_stack", &shuffle_stack, "state"_a, "stack_id"_a);
  m.def("update_input", &update_input, "state"_a);
}

#endif // KT_BUILD_PYTHON
