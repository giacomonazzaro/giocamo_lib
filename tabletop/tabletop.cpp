#include "tabletop.h"

#include <struct/print.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include "config.h"
#include "raylib.h"

// Keys we sample once per frame. Adding a new key here is all it takes to
// make a hotkey recordable/replayable.
static const int s_watched_pressed[] = {
  KEY_A,
  KEY_B,
  KEY_C,
  KEY_D,
  KEY_E,
  KEY_F,
  KEY_G,
  KEY_H,
  KEY_I,
  KEY_J,
  KEY_K,
  KEY_L,
  KEY_M,
  KEY_N,
  KEY_O,
  KEY_P,
  KEY_Q,
  KEY_R,
  KEY_S,
  KEY_T,
  KEY_U,
  KEY_V,
  KEY_W,
  KEY_X,
  KEY_Y,
  KEY_Z,
  KEY_ZERO,
  KEY_ONE,
  KEY_TWO,
  KEY_THREE,
  KEY_FOUR,
  KEY_FIVE,
  KEY_SIX,
  KEY_SEVEN,
  KEY_EIGHT,
  KEY_NINE,
  KEY_ENTER,
  KEY_BACKSPACE,
  KEY_TAB,
  KEY_ESCAPE,
  KEY_DELETE,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_UP,
  KEY_DOWN,
  KEY_MINUS,
  KEY_EQUAL,
  KEY_LEFT_BRACKET,
  KEY_RIGHT_BRACKET,
  KEY_SEMICOLON,
  KEY_APOSTROPHE,
  KEY_GRAVE,
  KEY_BACKSLASH,
  KEY_COMMA,
  KEY_PERIOD,
  KEY_SLASH,
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
  input.time       = GetTime();
  input.delta_time = GetFrameTime();
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

Thing_Location find_thing_at(float px, float py, const Table_State& state) {
  int            node_id = state.root;
  Thing_Location path;
  path.push_back(state.root);
  while (true) {
    auto found = false;
    for (size_t i = 0; i < state.things[node_id].children.size(); i++) {
      if (point_in_thing(px, py, state.things[node_id].children[i], state)) {
        node_id = state.things[node_id].children[i];
        path.push_back(node_id);
        found = true;
        break;
      }
    }
    if (!found) break;
  }
  return path;
}

void handle_mouse_press(Table_State& state, const Input& input) {
  // Mouse pressed — begin drag on the thing under the cursor.
  float       mx   = (float)input.mouse_x;
  float       my   = (float)input.mouse_y;
  Drag_State& drag = state.drag_state;

  Thing_Location path = find_thing_at(mx, my, state);
  // Need at least two elements (parent + child); clicking empty space returns
  // an empty path which would make path[size-2] undefined behavior.
  if (path.size() < 2) return;

  int thing_id  = path.back();
  int parent_id = path[path.size() - 2];

  // Hovered target starts out as the dragged thing's own parent — that's the
  // root-to-parent prefix of `path`. Build it before moving `path` away.
  Thing_Location parent_path(path.begin(), path.end() - 1);
  drag.dragged_thing = std::move(path);
  drag.hovered_thing = std::move(parent_path);

  // Drag offset in world coords so it's parent-agnostic during hover.
  float world_pos_x   = state.world_transforms[thing_id].x;
  float world_pos_y   = state.world_transforms[thing_id].y;
  drag.mouse_offset_x = mx - world_pos_x;
  drag.mouse_offset_y = my - world_pos_y;
}

void handle_mouse_release(Table_State& state) {
  // Mouse released — finalize the drop.
  Drag_State& drag     = state.drag_state;
  int         thing_id = drag.thing_id();
  if (thing_id < 0) return;
  assert(!drag.hovered_thing.empty());

  // Capture the thing.s current world position (where the user let go) so the
  // animation can lerp from that point — not from a stale rect that's about to
  // be reinterpreted in a different parent's coordinate space.
  Vector2 world_at_release = {
    state.world_transforms[thing_id].x,
    state.world_transforms[thing_id].y,
  };
  print(drag);
  bool allowed =
    state.is_drop_allowed(drag.parent_id(), drag.hovered_id(), thing_id);
  if (is_full(state.things[drag.hovered_id()])) {
    allowed = false;
  }
  //    bool allowed = true; // TODO(giacomo)

  if (!allowed) {
    // Snap-back: reset drag first so update_children_positions doesn't skip
    // the (still-dragged) card and leave it at the drop position. The card's
    // world_transforms_animated still holds the drop pose, so animate() will
    // glide it back to its slot.
    int original_parent = drag.parent_id();
    state.drag_state    = Drag_State();
    update_children_positions(original_parent, state, /*sort=*/true);
    return;
  }

  // Signal drop as (from_parent, to_parent, thing_id).
  state.dropped_thing =
    std::make_tuple(drag.parent_id(), drag.hovered_id(), thing_id);

  int original_parent = drag.parent_id();
  int current_parent  = drag.hovered_id();
  state.drag_state    = Drag_State();

  // Re-layout the affected parents. Skip root: its "layout" would smush all
  // top-level zones together with its zero spread.
  if (original_parent >= 0 && original_parent != state.root) {
    // Remove.
    auto& children = state.things[original_parent].children;
    auto  it       = std::find(children.begin(), children.end(), thing_id);
    if (it != children.end()) {
      children.erase(it);
      update_children_positions(original_parent, state, /*sort=*/true);
    }

    // Add.
    // auto old_parent_world = state.world_transforms[original_parent];
    // auto new_parent_world = state.world_transforms[current_parent];
    // auto old_local        = state.things[thing_id].transform;
    // // old_parent * old_local = new_parent * new_local
    // auto new_local = inverse(new_parent_world) * (old_parent_world *
    // old_local);
    update_local_transform_to_match_world_transform(
      state, current_parent, thing_id
    );
    state.world_transforms_animated[thing_id] =
      state.world_transforms[thing_id];
    state.things[current_parent].children.push_back(thing_id);
    update_children_positions(current_parent, state, /*sort=*/true);
  }

  //  // Re-anchor the smoothed world transform to where the user released, so
  //  // the lerp glides from there into the new parent.s slot.
  //  if (thing_id >= 0 && thing_id <
  //  (int)state.world_transforms_animated.size()) {
  //    state.world_transforms_animated[thing_id].x = world_at_release.x;
  //    state.world_transforms_animated[thing_id].y = world_at_release.y;
  //  }
}

void handle_mouse_move(Table_State& state, const Input& input) {
  // Continuously update the dragged thing.s position.
  Drag_State& drag     = state.drag_state;
  int         thing_id = drag.thing_id();
  if (thing_id < 0) return;

  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;

  // Walk the scene tree to find the topmost thing under the cursor. If that's
  // the dragged thing itself (the user hasn't moved off it yet), peel it off
  // so the hovered target is its parent.
  Thing_Location path = find_thing_at(mx, my, state);
  if (!path.empty() && path.back() == thing_id) path.pop_back();
  assert(!path.empty());
  drag.hovered_thing = std::move(path);
  //
  //  if (hovered >= 0 && drag.last_hovered_parent != hovered) {
  //    // Reparent: detach from the old parent, optionally attach to the
  //    hovered one. if (drag.current_parent >= 0) {
  //      auto& cur = state.things[drag.current_parent].children;
  //      auto  it  = std::find(cur.begin(), cur.end(), thing_id);
  //      if (it != cur.end()) {
  //        cur.erase(it);
  //        update_children_positions(drag.current_parent, state,
  //        /*sort=*/true);
  //      }
  //    }
  //
  //    Thing& hovered_thing = state.things[hovered];
  //    bool   allowed =
  //      state.is_drop_allowed(drag.original_parent, hovered, thing_id);
  //    bool full = is_full(hovered_thing);
  //
  //    if (allowed && !full) {
  //      hovered_thing.children.push_back(thing_id);
  //      update_children_positions(hovered, state, /*sort=*/true);
  //      drag.current_parent = hovered;
  //    } else {
  //      drag.current_parent = -1;
  //    }
  //  }

  //  if (hovered >= 0) {
  //    update_children_positions(hovered, state, /*sort=*/true);
  //    drag.last_hovered_parent = hovered;
  //  }

  // Target world position for the dragged thing.
  float target_world_x = mx - drag.mouse_offset_x;
  float target_world_y = my - drag.mouse_offset_y;

  // Now position the dragged thing under the cursor — convert world → local of
  // parent. Both target_world and parent_world are centers; subtracting gives
  // the thing's center in the parent's local space.

  Thing& thing        = state.things[thing_id];
  auto   parent_world = state.world_transforms[drag.parent_id()];
  auto   target_world = Transform2D{target_world_x, target_world_y, 0.0f};
  // parent_world * local = target_world
  auto local      = inverse(parent_world) * target_world;
  thing.transform = local;

  update_children_positions(drag.parent_id(), state, /*sort=*/true);
  if (drag.hovered_id() != state.root) {
    update_children_positions(drag.hovered_id(), state, /*sort=*/true);
  }
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

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include "config.h"
#include "raylib.h"

Rectangle world_rect(int thing_id, const Table_State& state) {
  float        px = state.world_transforms[thing_id].x;
  float        py = state.world_transforms[thing_id].y;
  const Thing& t  = state.things[thing_id];
  return Rectangle{
    px - t.size.x / 2.0f, py - t.size.y / 2.0f, t.size.x, t.size.y
  };
}

void update_children_positions(int parent_id, Table_State& state, bool sort) {
  if (parent_id == state.root) return;
  Thing& parent   = state.things[parent_id];
  auto   children = parent.children;
  auto&  drag     = state.drag_state;
  if (drag.thing_id() != -1) {
    if (drag.parent_id() != parent_id && drag.hovered_id() == parent_id) {
      // Dragging thing onto new parent.
      children.push_back(drag.thing_id());
    }
    if (drag.parent_id() == parent_id && drag.hovered_id() != parent_id) {
      // Moving thing away from this parent.
      auto it = std::find(children.begin(), children.end(), drag.thing_id());
      assert(it != children.end());
      children.erase(it);
    }
  }
  size_t n = children.size();

  // Cache each child's x in THIS parent's local space, keyed by thing id.
  // The dragged card's stored transform is in its old parent's local space,
  // so for it we translate its world position into this parent's space.
  auto local_x = std::unordered_map<int, float>();
  for (int child_id : children) {
    if (child_id == drag.thing_id() && drag.parent_id() != parent_id) {
      auto parent_world = state.world_transforms[parent_id];
      auto card_world   = state.world_transforms[child_id];
      auto card_in_this = inverse(parent_world) * card_world;
      local_x[child_id] = card_in_this.x;
    } else {
      local_x[child_id] = state.things[child_id].transform.x;
    }
  }

  if (sort && n > 0) {
    // Sort by the cached x so the dragged card slots in at the right index.
    std::sort(children.begin(), children.end(), [&local_x](int a, int b) {
      return local_x.at(a) < local_x.at(b);
    });
  }

  if (n == 0) return;

  float spread_x    = parent.spread_x;
  float spread_y    = parent.spread_y;
  float child_width = static_cast<float>(tt::CARD_WIDTH);

  // Adaptive spread: shrink if children would exceed the parent's width.
  if (n > 1 && parent.size.x > 0.0f && spread_x != 0.0f) {
    float total_width = static_cast<float>(n - 1) * spread_x + child_width;
    if (total_width > parent.size.x) {
      spread_x = (parent.size.x - child_width) / static_cast<float>(n - 1);
    }
  }

  float total_spread_x = (n > 1) ? static_cast<float>(n - 1) * spread_x : 0.0f;
  float total_spread_y = (n > 1) ? static_cast<float>(n - 1) * spread_y : 0.0f;

  // Children are placed by their centers around the parent's center (which
  // is at the origin in the parent's local space).
  float start_x_local = -total_spread_x / 2.0f;
  float start_y_local = -total_spread_y / 2.0f;

  int drag_id = drag.thing_id();
  for (int i = 0; i < (int)n; i++) {
    int    child_id = children[i];
    Thing& child    = state.things[child_id];
    if (child_id != drag_id) {
      child.transform.x = start_x_local + static_cast<float>(i) * spread_x;
      child.transform.y = start_y_local + static_cast<float>(i) * spread_y;
    }
  }
}

Thing make_card(int id, const std::string& image_path) {
  auto card = Thing{};
  card.id   = id;
  if (!image_path.empty()) {
    card.image_path = image_path;
  }
  card.capacity = 0;
  card.size     = {(float)tt::CARD_WIDTH, (float)tt::CARD_HEIGHT};
  return card;
}
