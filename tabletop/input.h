#pragma once
#include <optional>
#include <utility>

#include "models.h"

bool stack_is_full(const Thing& stack);
// Hit-tests `card` against world point (px, py) using its accumulated world
// rect — card.rect is in local space now.
bool point_in_card(float px, float py, int card_id, const Table_State& state);
bool card_pressed(int card_id, const Table_State& state);
bool point_in_stack_area(float px, float py, int stack_id, const Table_State& state);
// Returns (card_id, stack_thing_id) for the topmost card under (px, py).
// stack_thing_id == state.root for cards sitting directly under root (loose).
std::optional<std::pair<int, int>> find_card_at(
  float px, float py, Table_State& state
);
// Returns the thing-id of the topmost container under (px, py), or -1.
int  find_stack_at(float px, float py, const Table_State& state);
void handle_mouse_press(Table_State& state);
void handle_mouse_release(Table_State& state);
void handle_mouse_move(Table_State& state);
void handle_rotate_card(Table_State& state, bool clockwise = true);
void shuffle_stack(Table_State& state, int stack_id);
void update_input(Table_State& state);
