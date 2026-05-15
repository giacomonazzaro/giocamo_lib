#pragma once
#include <vector>

#include "models.h"

bool stack_is_full(const Thing& stack);
// Hit-tests `thing` against world point (px, py) using its accumulated world
// rect — cards use tt::CARD_WIDTH/CARD_HEIGHT since card.rect only stores
// position, other things use rect.width/height.
bool point_in_thing(float px, float py, int thing_id, const Table_State& state);
bool point_in_card(float px, float py, int card_id, const Table_State& state);
bool card_pressed(int card_id, const Table_State& state);
bool point_in_stack_area(float px, float py, int stack_id, const Table_State& state);
// Returns the scene-tree path from root down to the topmost thing whose world
// rect contains (px, py), inclusive of both endpoints. Topmost is determined
// by reverse-DFS: the last-drawn (visually frontmost) thing wins. The path is
// empty when nothing under root matches. Works for any thing — cards,
// containers, loose things — callers decide what kind of hit they want by
// inspecting path.back().
std::vector<int> find_thing_at(
  float px, float py, const Table_State& state
);
// Returns the thing-id of the topmost container under (px, py), or -1.
int  find_stack_at(float px, float py, const Table_State& state);
void handle_mouse_press(Table_State& state);
void handle_mouse_release(Table_State& state);
void handle_mouse_move(Table_State& state);
void handle_rotate_card(Table_State& state, bool clockwise = true);
void shuffle_stack(Table_State& state, int stack_id);
void update_input(Table_State& state);
