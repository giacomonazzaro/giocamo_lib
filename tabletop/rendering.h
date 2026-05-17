#pragma once
#include <string>

#include "models.h"

struct Input;
void draw_background(const Input& input, float turn = 0.0f);
void draw_table(Table_State& state, const Input& input);
void draw_card_back();
void draw_card_content(const Thing& card, bool face_up);
void draw_zoomed_card(const Thing& card, bool face_up);
// Dashed outline placeholder drawn in world coords. stack_id is a thing-id.
void draw_stack_placeholder(int stack_id, const Table_State& state);
// Smooth animated_cards (a parallel std::vector<Thing>) toward state.things.
void animate(
  std::vector<Thing>& things, const Table_State& state, float dt = 0.1f
);
void render_text(
  const std::string& text, float x, float y, int size, Color color
);
int text_width(const std::string& text, int size);
