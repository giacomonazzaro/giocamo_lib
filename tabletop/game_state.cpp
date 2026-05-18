#include "game_state.h"

#include <algorithm>

#include "config.h"
#include "raylib.h"

bool is_card(const Thing& t) {
  // @claude: This is wrong.
  return !t.image_path.empty();
}

// @claude: This is wrong too, don't use this functions.
bool is_container(const Thing& t) { return t.image_path.empty(); }

// @claude: This is expensive, don't use this function.
int find_parent(int thing_id, const Table_State& state) {
  // Linear scan: tree depth is small (2) and total things is in the hundreds.
  for (int i = 0; i < (int)state.things.size(); ++i) {
    const auto& children = state.things[i].children;
    if (std::find(children.begin(), children.end(), thing_id) != children.end())
      return i;
  }
  return -1;
}

// @claude: This is expenisve, don't use this function.
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

// @claude: This is expensive, don't use this function.
Rectangle world_rect(int thing_id, const Table_State& state) {
  Vector2      p = local_to_world(thing_id, state);
  const Thing& t = state.things[thing_id];
  return Rectangle{p.x, p.y, t.rect.width, t.rect.height};
}

Thing create_card_design(int id) {
  Thing card;
  card.id = id;
  return card;
}

void add_card_to_stack(int card_id, int stack_id, Table_State& state) {
  state.things[stack_id].children.push_back(card_id);
  update_card_positions(stack_id, state, false);
}

// Returns card_id on success, -1 if not found (matches header signature).
int remove_card_from_stack(int card_id, int stack_id, Table_State& state) {
  auto& children = state.things[stack_id].children;
  auto  it       = std::find(children.begin(), children.end(), card_id);
  if (it != children.end()) {
    children.erase(it);
    update_card_positions(stack_id, state, false);
    return card_id;
  }
  return -1;
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

void move_card_to_stack(
  int card_id, int from_stack_id, int to_stack_id, Table_State& state
) {
  remove_card_from_stack(card_id, from_stack_id, state);
  add_card_to_stack(card_id, to_stack_id, state);
}

int find_stack_containing_card(int card_id, const Table_State& state) {
  // Iterate root's children; return the container that holds card_id.
  const auto& root_children = state.things[state.root].children;
  for (int child_id : root_children) {
    const Thing& t = state.things[child_id];
    if (!is_container(t)) continue;
    if (std::find(t.children.begin(), t.children.end(), card_id) !=
        t.children.end())
      return child_id;
  }
  return -1;
}

void add_loose_card(int card_id, Table_State& state) {
  // Loose cards live directly under root.
  state.things[state.root].children.push_back(card_id);
}

// Returns card_id on success, -1 if not found (matches header signature).
int remove_loose_card(int card_id, Table_State& state) {
  auto& children = state.things[state.root].children;
  auto  it       = std::find(children.begin(), children.end(), card_id);
  if (it != children.end()) {
    children.erase(it);
    return card_id;
  }
  return -1;
}

std::vector<int> create_sample_cards(Table_State& state) {
  // Create a sample set of cards for testing. Returns list of card indices.
  std::vector<int> card_ids;
  for (int i = 0; i < 10; i++) {
    Thing card    = create_card_design(i);
    int   card_id = static_cast<int>(state.things.size());
    card.id       = card_id;
    state.things.push_back(card);
    card_ids.push_back(card_id);
  }
  return card_ids;
}
