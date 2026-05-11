#include "game_state.h"

#include <algorithm>

#include "config.h"

KT_Card create_card_design(int id) {
  KT_Card card;
  card.id = id;
  return card;
}

void add_card_to_stack(int card_id, Stack& stack, Table_State& state) {
  stack.cards.push_back(card_id);
  update_card_positions(stack, state, false);
}

// Returns card_id on success, -1 if not found (matches header signature).
int remove_card_from_stack(int card_id, Stack& stack, Table_State& state) {
  auto it = std::find(stack.cards.begin(), stack.cards.end(), card_id);
  if (it != stack.cards.end()) {
    stack.cards.erase(it);
    update_card_positions(stack, state, false);
    return card_id;
  }
  return -1;
}

void update_card_positions(Stack& stack, Table_State& state, bool sort) {
  // Update x,y positions of all cards in a stack based on spread values.
  size_t n = stack.cards.size();

  if (sort && n > 0) {
    // Sort cards by their current x position.
    std::sort(stack.cards.begin(), stack.cards.end(), [&state](int a, int b) {
      return state.cards[a].rect.x < state.cards[b].rect.x;
    });
  }

  if (n == 0) return;

  float spread_x   = stack.spread_x;
  float spread_y   = stack.spread_y;
  float card_width = static_cast<float>(tt::CARD_WIDTH);

  // Adaptive spread: shrink if cards exceed stack width when spread_x is
  // non-zero.
  if (n > 1 && stack.rect.width > 0.0f && spread_x != 0.0f) {
    float total_width = static_cast<float>(n - 1) * spread_x + card_width;
    if (total_width > stack.rect.width) {
      spread_x = (stack.rect.width - card_width) / static_cast<float>(n - 1);
    }
  }

  float total_spread_x = (n > 1) ? static_cast<float>(n - 1) * spread_x : 0.0f;
  float total_spread_y = (n > 1) ? static_cast<float>(n - 1) * spread_y : 0.0f;

  float mid_x   = (stack.rect.width > 0.0f)
                    ? stack.rect.x + stack.rect.width / 2.0f
                    : stack.rect.x;
  float start_x = mid_x - (total_spread_x + card_width) / 2.0f;
  float start_y = stack.rect.y - total_spread_y / 2.0f;

  int drag_id = state.drag_state.card_id;
  for (int i = 0; i < (int)n; i++) {
    int card_id = stack.cards[i];
    if (card_id != drag_id) {
      KT_Card& card = state.cards[card_id];
      card.rect.x   = start_x + static_cast<float>(i) * spread_x;
      card.rect.y   = start_y + static_cast<float>(i) * spread_y;
    }
  }
}

void move_card_to_stack(
  int card_id, Stack& from_stack, Stack& to_stack, Table_State& state
) {
  remove_card_from_stack(card_id, from_stack, state);
  add_card_to_stack(card_id, to_stack, state);
}

int find_stack_containing_card(int card_id, const Table_State& state) {
  // Return stack index, or -1 if not found.
  for (int i = 0; i < (int)state.stacks.size(); i++) {
    const Stack& stack = state.stacks[i];
    if (std::find(stack.cards.begin(), stack.cards.end(), card_id) !=
        stack.cards.end())
      return i;
  }
  return -1;
}

void add_loose_card(int card_id, Table_State& state) {
  // Add a card to the table as a loose card.
  state.loose_cards.push_back(card_id);
}

// Returns card_id on success, -1 if not found (matches header signature).
int remove_loose_card(int card_id, Table_State& state) {
  auto it =
    std::find(state.loose_cards.begin(), state.loose_cards.end(), card_id);
  if (it != state.loose_cards.end()) {
    state.loose_cards.erase(it);
    return card_id;
  }
  return -1;
}

std::vector<int> create_sample_cards(Table_State& state) {
  // Create a sample set of cards for testing. Returns list of card indices.
  std::vector<int> card_ids;
  for (int i = 0; i < 10; i++) {
    KT_Card card    = create_card_design(i);
    int     card_id = static_cast<int>(state.cards.size());
    state.cards.push_back(card);
    card_ids.push_back(card_id);
  }
  return card_ids;
}
