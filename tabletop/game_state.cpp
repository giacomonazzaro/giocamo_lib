#include "game_state.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

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
    if (drag.parent_id() != parent_id && drag.hovered_thing == parent_id) {
      // Dragging thing onto new parent.
      children.push_back(drag.thing_id());
    }
    if (drag.parent_id() == parent_id && drag.hovered_thing != parent_id) {
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