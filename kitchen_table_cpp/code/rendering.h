#pragma once
#include "models.h"

void draw_background(float turn = 0.0f);
void draw_table(Table_State& state);
void draw_card(const Thing& card, bool face_up = true);
void draw_stack(const Stack& stack, const Table_State& state);
void draw_card_back();
void draw_card_content(const Thing& card, bool face_up);
void draw_zoomed_card(const Thing& card, bool face_up);
void draw_stack_placeholder(const Stack& stack);
void animate(
  std::vector<Card>& cards, const Table_State& state, float dt = 0.1f
);
// color is a pyray.Color cffi object; constructed via color_from_tuple or
// direct pyray call.
void render_text(
  const std::string& text, float x, float y, int size, nb::object color
);
int        text_width(const std::string& text, int size);
nb::object color_from_tuple(nb::object color_tuple);

void bind_rendering(nb::module_& m);
