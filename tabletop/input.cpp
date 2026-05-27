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
  KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
  KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
  KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
  KEY_ZERO, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR,
  KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE,
  KEY_ENTER, KEY_BACKSPACE, KEY_TAB, KEY_ESCAPE, KEY_DELETE,
  KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
  KEY_MINUS, KEY_EQUAL, KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET,
  KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_BACKSLASH,
  KEY_COMMA, KEY_PERIOD, KEY_SLASH,
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
  Input input;
  input.mouse_x       = GetMouseX();
  input.mouse_y       = GetMouseY();
  input.left_pressed  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  input.left_released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  for (int k : s_watched_pressed) {
    if (IsKeyPressed(k)) input.keys_pressed.push_back(k);
  }
  for (int k : s_watched_down) {
    if (IsKeyDown(k)) input.keys_down.push_back(k);
  }
  // Drain the typed-character queue. Menus need this for room-code input.
  int c;
  while ((c = GetCharPressed()) != 0) {
    if (c >= 32 && c < 127) input.chars_typed.push_back((char)c);
  }
  return input;
}

bool is_full(const Thing& thing) {
  return thing.capacity >= 0 && (int)thing.children.size() >= thing.capacity;
}

bool point_in_thing(
  float px, float py, int thing_id, const Table_State& state
) {
  Rectangle r = world_rect(thing_id, state);
  return (
    r.x <= px && px <= r.x + r.width && r.y <= py && py <= r.y + r.height
  );
}

bool thing_pressed(int thing_id, const Table_State& state, const Input& input) {
  if (!input.left_pressed) return false;
  return point_in_thing(
    (float)input.mouse_x, (float)input.mouse_y, thing_id, state
  );
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
  float        w = t.size.x;
  float        h = t.size.y;
  // (px, py) is in the parent's local space, whose origin is the parent's
  // center. Express it in this thing's local space, whose origin is this
  // thing's center.
  float dx = px - t.transform.x;
  float dy = py - t.transform.y;
  float local_px, local_py;
  if (t.transform.rotation == 0.0f) {
    local_px = dx;
    local_py = dy;
  } else {
    // Renderer rotates around the thing's center by t.transform.rotation
    // degrees. Inverse: rotate by -t.transform.rotation.
    float angle_rad = t.transform.rotation * (float)(M_PI / 180.0);
    float cos_a     = std::cos(angle_rad);
    float sin_a     = std::sin(angle_rad);
    local_px        = cos_a * dx + sin_a * dy;
    local_py        = -sin_a * dx + cos_a * dy;
  }

  // Prune: containment in thing-local space (centered at origin). Root is
  // exempt — it covers the entire window, so the check would always pass.
  if (thing_id != state.root) {
    if (std::abs(local_px) > w / 2.0f || std::abs(local_py) > h / 2.0f)
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

void handle_mouse_press(Table_State& state, const Input& input) {
  // Mouse pressed — begin drag on the thing under the cursor.
  float       mx   = (float)input.mouse_x;
  float       my   = (float)input.mouse_y;
  Drag_State& drag = state.drag_state;

  Thing_Location path = find_thing_at(mx, my, state);
  // Need at least two elements (parent + child); clicking empty space returns an
  // empty path which would make path[size-2] undefined behavior.
  if (path.size() < 2) return;

  int thing_id  = path.back();
  int parent_id = path[path.size() - 2];

  drag.location       = std::move(path);
  drag.current_parent  = parent_id;
  drag.original_parent = parent_id;
  // Drag offset in world coords so it's parent-agnostic during hover.
  Vector2 world_pos = local_to_world(thing_id, state);
  drag.offset_x      = mx - world_pos.x;
  drag.offset_y      = my - world_pos.y;
}

void handle_mouse_release(Table_State& state) {
  // Mouse released — finalize the drop.
  Drag_State& drag    = state.drag_state;
  int         thing_id = dragged_thing_id(drag);
  if (thing_id < 0) return;

  // Capture the thing.s current world position (where the user let go) so the
  // animation can lerp from that point — not from a stale rect that's about to
  // be reinterpreted in a different parent's coordinate space.
  Vector2 world_at_release = local_to_world(thing_id, state);

  bool allowed = state.is_drop_allowed(
    drag.original_parent, drag.current_parent, thing_id
  );

  if (!allowed) {
    // Snap back: re-attach to original parent if we're floating without one.
    if (drag.current_parent == -1) {
      state.things[drag.original_parent].children.push_back(thing_id);
    }
  }

  // Signal drop as (from_parent, to_parent, thing_id).
  state.dropped_thing =
    std::make_tuple(drag.original_parent, drag.current_parent, thing_id);

  int original_parent = drag.original_parent;
  int current_parent  = drag.current_parent;
  state.drag_state   = Drag_State();

  // Re-layout the affected parents. Skip root: its "layout" would smush all
  // top-level zones together with its zero spread.
  if (original_parent >= 0 && original_parent != state.root)
    update_children_positions(original_parent, state, /*sort=*/true);
  if (current_parent >= 0 && current_parent != original_parent &&
      current_parent != state.root)
    update_children_positions(current_parent, state, /*sort=*/true);

  // Re-anchor the smoothed world transform to where the user released, so
  // the lerp glides from there into the new parent.s slot.
  if (thing_id >= 0 && thing_id < (int)state.animated_world.size()) {
    state.animated_world[thing_id].x = world_at_release.x;
    state.animated_world[thing_id].y = world_at_release.y;
  }
}

void handle_mouse_move(Table_State& state, const Input& input) {
  // Continuously update the dragged thing.s position.
  Drag_State& drag    = state.drag_state;
  int         thing_id = dragged_thing_id(drag);
  if (thing_id < 0) return;

  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;

  // Target world position for the dragged thing.
  float target_world_x = mx - drag.offset_x;
  float target_world_y = my - drag.offset_y;

  auto location      = find_thing_at(mx, my, state);
  int  hovered = location.empty() ? -1 : location.back();
  // Don't reparent the dragged thing into itself — would create a cycle in
  // the scene tree.
  if (hovered == thing_id) hovered = -1;

  if (hovered >= 0 && drag.last_hovered_parent != hovered) {
    // Reparent: detach from the old parent, optionally attach to the hovered one.
    if (drag.current_parent >= 0) {
      auto& cur = state.things[drag.current_parent].children;
      auto  it  = std::find(cur.begin(), cur.end(), thing_id);
      if (it != cur.end()) {
        cur.erase(it);
        update_children_positions(drag.current_parent, state, /*sort=*/true);
      }
    }

    Thing& hovered_thing = state.things[hovered];
    bool   allowed =
      state.is_drop_allowed(drag.original_parent, hovered, thing_id);
    bool full = is_full(hovered_thing);

    if (allowed && !full) {
      hovered_thing.children.push_back(thing_id);
      update_children_positions(hovered, state, /*sort=*/true);
      drag.current_parent = hovered;
    } else {
      drag.current_parent = -1;
    }
  }

  if (hovered >= 0) {
    update_children_positions(hovered, state, /*sort=*/true);
    drag.last_hovered_parent = hovered;
  }

  // Now position the dragged thing under the cursor — convert world → local of parent.
  // Both target_world and parent_world are centers; subtracting gives the
  // thing's center in the parent's local space.
  Thing& thing      = state.things[thing_id];
  int parent_id     = (drag.current_parent >= 0) ? drag.current_parent : state.root;
  Vector2 parent_world = local_to_world(parent_id, state);
  thing.transform.x = target_world_x - parent_world.x;
  thing.transform.y = target_world_y - parent_world.y;
}

void handle_rotate_thing(
  Table_State& state, const Input& input, bool clockwise
) {
  // Rotate the thing under the cursor by 90 degrees.
  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;

  auto path = find_thing_at(mx, my, state);
  if (path.empty()) return;

  int    thing_id = path.back();
  Thing& thing    = state.things[thing_id];
  if (clockwise)
    thing.transform.rotation = thing.transform.rotation + 90.0f;
  else
    thing.transform.rotation = thing.transform.rotation - 90.0f;
}

void shuffle_thing(Table_State& state, int parent_id) {
  if (parent_id < 0) return;

  Thing&              thing = state.things[parent_id];
  static std::mt19937 rng{std::random_device{}()};
  std::shuffle(thing.children.begin(), thing.children.end(), rng);
  update_children_positions(parent_id, state, /*sort=*/false);
}

void process_input(Table_State& state, const Input& input) {
  // Per-frame input processing.
  state.dropped_thing = std::nullopt;

  if (input.left_pressed) {
    handle_mouse_press(state, input);
  } else if (input.left_released) {
    handle_mouse_release(state);
  }

  handle_mouse_move(state, input);

  bool shift = key_down(input, KEY_LEFT_SHIFT) ||
               key_down(input, KEY_RIGHT_SHIFT);
  if (key_pressed(input, KEY_R)) {
    handle_rotate_thing(state, input, /*clockwise=*/!shift);
  }

  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;

  if (key_down(input, KEY_SPACE)) {
    state.zoomed_thing_id = find_thing_at(mx, my, state);
  } else {
    state.zoomed_thing_id.clear();
  }

  if (key_pressed(input, KEY_S)) {
    auto location = find_thing_at(mx, my, state);
    if (!location.empty()) {
      shuffle_thing(state, location.back());
    }
  }
}
