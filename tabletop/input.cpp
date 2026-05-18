#include "input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include "config.h"
#include "game_state.h"
#include "raylib.h"

// Keys we sample once per frame. Adding a new key here is all it takes to
// make a hotkey recordable/replayable.
static const int s_watched_pressed[] = {
  KEY_R,
  KEY_S,
  KEY_P,
  KEY_V,
  KEY_C,
  KEY_ENTER,
  KEY_BACKSPACE,
  KEY_ONE,
  KEY_TWO,
  KEY_THREE,
  KEY_FOUR,
  KEY_FIVE,
  KEY_SIX,
  KEY_SEVEN,
  KEY_EIGHT,
  KEY_NINE,
  KEY_ZERO,
};
static const int s_watched_down[] = {
  KEY_SPACE,
  KEY_LEFT_SHIFT,
  KEY_RIGHT_SHIFT,
  KEY_LEFT_SUPER,
  KEY_RIGHT_SUPER,
  KEY_LEFT_CONTROL,
  KEY_RIGHT_CONTROL,
};

Input capture_input() {
  Input in;
  in.mouse_x       = GetMouseX();
  in.mouse_y       = GetMouseY();
  in.left_pressed  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  in.left_released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  for (int k : s_watched_pressed) {
    if (IsKeyPressed(k)) in.keys_pressed.push_back(k);
  }
  for (int k : s_watched_down) {
    if (IsKeyDown(k)) in.keys_down.push_back(k);
  }
  // Drain the typed-character queue. Menus need this for room-code input.
  int c;
  while ((c = GetCharPressed()) != 0) {
    if (c >= 32 && c < 127) in.chars_typed.push_back((char)c);
  }
  return in;
}

bool stack_is_full(const Thing& stack) {
  // True if the container has a capacity limit and has reached it.
  return stack.capacity >= 0 && (int)stack.children.size() >= stack.capacity;
}

bool point_in_thing(
  float px, float py, int thing_id, const Table_State& state
) {
  // Cards store only x/y in rect (size implicit via CARD_WIDTH/HEIGHT);
  // other things use their rect's width/height.
  Rectangle    r = world_rect(thing_id, state);
  const Thing& t = state.things[thing_id];
  float        w = is_card(t) ? (float)tt::CARD_WIDTH : r.width;
  float        h = is_card(t) ? (float)tt::CARD_HEIGHT : r.height;
  return (r.x <= px && px <= r.x + w && r.y <= py && py <= r.y + h);
}

bool point_in_card(float px, float py, int card_id, const Table_State& state) {
  return point_in_thing(px, py, card_id, state);
}

bool card_pressed(int card_id, const Table_State& state, const Input& input) {
  if (!input.left_pressed) return false;
  return point_in_card(
    (float)input.mouse_x, (float)input.mouse_y, card_id, state
  );
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

// Reverse-DFS so the visually topmost (last-drawn) thing under (px, py) wins.
// (px, py) enters in the parent's local space; we invert this thing's render
// transform to express the point in its own local space, then test
// containment and recurse with that local point. Children are contained in
// their parent so a containment miss prunes the whole subtree. Root spans
// the whole window and is only used as the recursion entry, never as a hit
// target. On success, `path` ends with the hit thing; root is path[0].
static bool find_thing_at_rec(
  float              px,
  float              py,
  int                thing_id,
  const Table_State& state,
  Thing_Location&    path
) {
  const Thing& t = state.things[thing_id];

  // Inverse of apply_local_transform: parent-local → thing-local.
  float w = is_card(t) ? (float)tt::CARD_WIDTH : t.rect.width;
  float h = is_card(t) ? (float)tt::CARD_HEIGHT : t.rect.height;
  float local_px, local_py;
  if (t.rotation == 0.0f) {
    local_px = px - t.rect.x;
    local_py = py - t.rect.y;
  } else {
    // Renderer rotates around (cx, cy) by t.rotation degrees. To go from
    // parent-local back to thing-local: translate to center, rotate by
    // -t.rotation, shift to top-left origin.
    float cx       = t.rect.x + w / 2.0f;
    float cy       = t.rect.y + h / 2.0f;
    float dx       = px - cx;
    float dy       = py - cy;
    float angle_rad = t.rotation * (float)(M_PI / 180.0);
    float cos_a    = std::cos(angle_rad);
    float sin_a    = std::sin(angle_rad);
    local_px       = cos_a * dx + sin_a * dy + w / 2.0f;
    local_py       = -sin_a * dx + cos_a * dy + h / 2.0f;
  }

  // Prune: containment in thing-local space. Root is exempt — it covers the
  // entire window, so the check would always pass.
  if (thing_id != state.root) {
    if (local_px < 0.0f || local_px > w || local_py < 0.0f || local_py > h)
      return false;
  }

  path.push_back(thing_id);

  // Descend with the point expressed in this thing's local space.
  const auto& children = t.children;
  for (int i = (int)children.size() - 1; i >= 0; --i) {
    if (find_thing_at_rec(local_px, local_py, children[i], state, path))
      return true;
  }

  // No descendant matched. Self is the hit — unless we're the root, which
  // isn't a meaningful hit target on its own.
  if (thing_id == state.root) {
    path.pop_back();
    return false;
  }
  return true;
}

Thing_Location find_thing_at(float px, float py, const Table_State& state) {
  Thing_Location path;
  // Root's local space coincides with world space (rect at origin).
  find_thing_at_rec(px, py, state.root, state, path);
  return path;
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

void handle_mouse_press(Table_State& state, const Input& input) {
  // Mouse pressed — begin drag if a card is under the cursor.
  float       mx   = (float)input.mouse_x;
  float       my   = (float)input.mouse_y;
  Drag_State& drag = state.drag_state;

  Thing_Location path = find_thing_at(mx, my, state);
  // if (path.size() < 2 || !is_card(state.things[path.back()])) return;

  int card_id  = path.back();
  int stack_id = path[path.size() - 2];

  drag.location       = std::move(path);
  drag.current_stack  = stack_id;
  drag.original_stack = stack_id;
  // Drag offset in world coords so it's parent-agnostic during hover.
  Vector2 card_world = local_to_world(card_id, state);
  drag.offset_x      = mx - card_world.x;
  drag.offset_y      = my - card_world.y;
}

void handle_mouse_release(Table_State& state) {
  // Mouse released — finalize the drop.
  Drag_State& drag    = state.drag_state;
  int         card_id = dragged_thing_id(drag);
  if (card_id < 0) return;

  bool allowed = state.is_drop_card_allowed(
    drag.original_stack, drag.current_stack, card_id
  );

  if (!allowed) {
    // Snap back: re-attach to original parent if we're floating without one.
    if (drag.current_stack == -1) {
      state.things[drag.original_stack].children.push_back(card_id);
    }
  }

  // Signal drop as (from_stack, to_stack, card_id).
  state.dropped_card =
    std::make_tuple(drag.original_stack, drag.current_stack, card_id);

  int  original_stack  = drag.original_stack;
  int  current_stack   = drag.current_stack;
  bool dragged_is_card = is_card(state.things[card_id]);
  state.drag_state     = Drag_State();

  // Re-layout cards in the affected stacks. Only when the dragged thing is a
  // card — dragging a stack must not re-flow root's children.
  if (dragged_is_card) {
    if (original_stack >= 0)
      update_card_positions(original_stack, state, /*sort=*/true);
    if (current_stack >= 0 && current_stack != original_stack)
      update_card_positions(current_stack, state, /*sort=*/true);
  }
}

void handle_mouse_move(Table_State& state, const Input& input) {
  // Continuously update dragged card position.
  Drag_State& drag    = state.drag_state;
  int         card_id = dragged_thing_id(drag);
  if (card_id < 0) return;

  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;

  // Target world position for the dragged card.
  float target_world_x = mx - drag.offset_x;
  float target_world_y = my - drag.offset_y;

  // Stacks-within-stacks layout only applies when the dragged thing is a card.
  // Dragging a whole stack must not trigger re-flows of root's children.
  bool dragged_is_card = is_card(state.things[card_id]);

  int hovered_stack = find_stack_at(mx, my, state);
  // Don't reparent the dragged thing into itself — would create a cycle in
  // the scene tree.
  if (hovered_stack == card_id) hovered_stack = -1;

  if (hovered_stack >= 0 && drag.last_hovered_stack != hovered_stack) {
    // Reparent: remove from current stack, optionally add to hovered stack.
    if (drag.current_stack >= 0) {
      auto& cur = state.things[drag.current_stack].children;
      auto  it  = std::find(cur.begin(), cur.end(), card_id);
      if (it != cur.end()) {
        cur.erase(it);
        if (dragged_is_card)
          update_card_positions(drag.current_stack, state, /*sort=*/true);
      }
    }

    Thing& hovered = state.things[hovered_stack];
    bool   allowed =
      state.is_drop_card_allowed(drag.original_stack, hovered_stack, card_id);
    bool full = stack_is_full(hovered);
    fprintf(
      stderr,
      "[drag] orig=%d hover=%d card=%d allowed=%d full=%d\n",
      drag.original_stack,
      hovered_stack,
      card_id,
      (int)allowed,
      (int)full
    );

    if (allowed && !full) {
      hovered.children.push_back(card_id);
      if (dragged_is_card)
        update_card_positions(hovered_stack, state, /*sort=*/true);
      drag.current_stack = hovered_stack;
    } else {
      drag.current_stack = -1;
    }
  }

  if (hovered_stack >= 0 && dragged_is_card) {
    update_card_positions(hovered_stack, state, /*sort=*/true);
    drag.last_hovered_stack = hovered_stack;
  } else if (hovered_stack >= 0) {
    drag.last_hovered_stack = hovered_stack;
  }

  // Now position the card under the cursor — convert world → local of parent.
  Thing& card   = state.things[card_id];
  int parent_id = (drag.current_stack >= 0) ? drag.current_stack : state.root;
  Vector2 parent_world = local_to_world(parent_id, state);
  card.rect.x          = target_world_x - parent_world.x;
  card.rect.y          = target_world_y - parent_world.y;
}

void handle_rotate_card(
  Table_State& state, const Input& input, bool clockwise
) {
  // Rotate the card under the cursor by 90 degrees.
  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;

  auto path = find_thing_at(mx, my, state);
  if (path.empty()) return;

  int    card_id = path.back();
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

void process_input(Table_State& state, const Input& input) {
  // Per-frame input processing.
  state.dropped_card = std::nullopt;

  if (input.left_pressed) {
    handle_mouse_press(state, input);
  } else if (input.left_released) {
    handle_mouse_release(state);
  }

  handle_mouse_move(state, input);

  bool shift = key_down(input, KEY_LEFT_SHIFT) ||
               key_down(input, KEY_RIGHT_SHIFT);
  if (key_pressed(input, KEY_R)) {
    handle_rotate_card(state, input, /*clockwise=*/!shift);
  }

  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;

  if (key_down(input, KEY_SPACE)) {
    auto path            = find_thing_at(mx, my, state);
    state.zoomed_card_id = (!path.empty() && is_card(state.things[path.back()]))
                             ? std::move(path)
                             : Thing_Location{};
  } else {
    state.zoomed_card_id.clear();
  }

  if (key_pressed(input, KEY_S)) {
    int stack_id = find_stack_at(mx, my, state);
    shuffle_stack(state, stack_id);
  }
}
