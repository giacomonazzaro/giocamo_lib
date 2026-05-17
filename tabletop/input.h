#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "../struct/visit.hpp"
#include "models.h"

// Per-frame snapshot of every input that `tabletop/` code consumes.
// Built once at the top of each frame either by capture_input() (live mode)
// or pulled from a recorded array (playback mode). Every tabletop function
// that needs to know about input takes a `const Input&` instead of calling
// raylib directly, so the entire interaction stream can be recorded/replayed.
//
// Keys are stored as raylib KEY_* codes. capture_input() only watches a fixed
// set of keys (see input.cpp). To make a new key recordable, add it to the
// watched lists in capture_input() — call sites then just use key_pressed()
// or key_down() with the new code.
struct Input {
  int  mouse_x       = 0;
  int  mouse_y       = 0;
  bool left_pressed  = false;  // IsMouseButtonPressed(MOUSE_BUTTON_LEFT).
  bool left_released = false;  // IsMouseButtonReleased(MOUSE_BUTTON_LEFT).
  // Raylib KEY_* codes triggered this frame (IsKeyPressed).
  std::vector<int> keys_pressed;
  // Raylib KEY_* codes held this frame (IsKeyDown).
  std::vector<int> keys_down;
  // Characters produced this frame (GetCharPressed loop result).
  std::string chars_typed;
};
VISITABLE_STRUCT(
  Input,
  mouse_x,
  mouse_y,
  left_pressed,
  left_released,
  keys_pressed,
  keys_down,
  chars_typed
);

// Reads the current frame's input from raylib. This is the ONLY place in
// `tabletop/` that calls raylib input functions directly.
Input capture_input();

inline bool key_pressed(const Input& input, int key) {
  return std::find(input.keys_pressed.begin(), input.keys_pressed.end(), key) !=
         input.keys_pressed.end();
}
inline bool key_down(const Input& input, int key) {
  return std::find(input.keys_down.begin(), input.keys_down.end(), key) !=
         input.keys_down.end();
}

bool stack_is_full(const Thing& stack);
// Hit-tests `thing` against world point (px, py) using its accumulated world
// rect — cards use tt::CARD_WIDTH/CARD_HEIGHT since card.rect only stores
// position, other things use rect.width/height.
bool point_in_thing(float px, float py, int thing_id, const Table_State& state);
bool point_in_card(float px, float py, int card_id, const Table_State& state);
bool card_pressed(int card_id, const Table_State& state, const Input& input);
bool point_in_stack_area(
  float px, float py, int stack_id, const Table_State& state
);
// Returns the scene-tree path from root down to the topmost thing whose world
// rect contains (px, py), inclusive of both endpoints. Topmost is determined
// by reverse-DFS: the last-drawn (visually frontmost) thing wins. The path is
// empty when nothing under root matches. Works for any thing — cards,
// containers, loose things — callers decide what kind of hit they want by
// inspecting path.back().
std::vector<int> find_thing_at(float px, float py, const Table_State& state);
// Returns the thing-id of the topmost container under (px, py), or -1.
int  find_stack_at(float px, float py, const Table_State& state);
void handle_mouse_press(Table_State& state, const Input& input);
void handle_mouse_release(Table_State& state);
void handle_mouse_move(Table_State& state, const Input& input);
void handle_rotate_card(
  Table_State& state, const Input& input, bool clockwise = true
);
void shuffle_stack(Table_State& state, int stack_id);
void process_input(Table_State& state, const Input& input);
