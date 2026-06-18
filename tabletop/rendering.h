#pragma once
#include <string>

#include "tabletop.h"

struct Input;
struct Input_Feed;
void draw_background(const Input& input, float turn = 0.0f);
void draw_table(Table_State& state, const Input& input);
void draw_zoomed_thing(
  const Table_State& state, const Input& input, int thing_id, bool face_up
);
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

// Push / pop the letterbox transform (see screen_fit) so drawing done in
// logical canvas coordinates lands scaled and centered in the real window.
void begin_screen_fit();
void end_screen_fit();

void run_tabletop(
  Table_State&                                    table,
  std::function<bool(Table_State&, const Input&)> update,
  Input_Feed&                                     input_feed,
  int                                             window_width,
  int                                             window_height,
  const std::string&                              window_name
);

// Convenience overload: creates a live-input feed internally. Use this when
// the caller doesn't need to record or replay the input stream.
void run_tabletop(
  Table_State&                                    table,
  std::function<bool(Table_State&, const Input&)> update,
  int                                             window_width,
  int                                             window_height,
  const std::string&                              window_name
);