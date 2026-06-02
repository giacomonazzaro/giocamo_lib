#pragma once
#include <string>

#include "tabletop.h"

struct Input;
void draw_background(const Input& input, float turn = 0.0f);
void draw_table(Table_State& state, const Input& input);
void draw_thing_back();
void draw_zoomed_thing(const Thing& thing, bool face_up);
// Dashed outline placeholder drawn in world coords. thing_id is a thing-id.
void draw_drop_placeholder(int thing_id, const Table_State& state);
// Smooth per-thing world transforms toward the current target tree.
void animate(
  std::vector<Transform2D>& animated, const Table_State& state, float dt = 0.1f
);
void render_text(
  const std::string& text, float x, float y, int size, Color color
);
int text_width(const std::string& text, int size);

void run_tabletop(
  Table_State&       table,
  int                window_width,
  int                window_height,
  const std::string& window_name
);