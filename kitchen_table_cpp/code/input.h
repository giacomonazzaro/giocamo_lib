#pragma once
#include <optional>
#include <utility>

#include "models.h"

bool stack_is_full(const Stack& stack);
bool point_in_card(float px, float py, const Thing& card);
bool card_pressed(const Thing& card);
bool point_in_stack_area(float px, float py, const Stack& stack);
// Returns (card_id, stack_index), or nullopt if no card under the point.
// stack_index is -1 for loose cards.
std::optional<std::pair<int, int>> find_card_at(float px, float py, Table_State& state);
int  find_stack_at(float px, float py, const Table_State& state);
void handle_mouse_press(Table_State& state);
void handle_mouse_release(Table_State& state);
void handle_mouse_move(Table_State& state);
void handle_rotate_card(Table_State& state, bool clockwise = true);
void shuffle_stack(Table_State& state, int stack_id);
void update_input(Table_State& state);

#ifdef KT_BUILD_PYTHON
void bind_input(nb::module_& m);
#endif
