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

// True if `thing` has a capacity limit and has reached it.
bool is_full(const Thing& thing);
// Hit-test `thing` against world point (px, py) using its accumulated world
// rect.
bool point_in_thing(float px, float py, int thing_id, const Table_State& state);
bool thing_pressed(int thing_id, const Table_State& state, const Input& input);
// Returns the scene-tree path from root down to the topmost thing whose world
// rect contains (px, py). Topmost is determined by reverse-DFS (the
// last-drawn / visually frontmost thing wins). Empty when nothing matched.
Thing_Location find_thing_at(float px, float py, const Table_State& state);
void handle_mouse_press(Table_State& state, const Input& input);
void handle_mouse_release(Table_State& state);
void handle_mouse_move(Table_State& state, const Input& input);
void handle_rotate_thing(
  Table_State& state, const Input& input, bool clockwise = true
);
void shuffle_thing(Table_State& state, int thing_id);
void process_input(Table_State& state, const Input& input);
