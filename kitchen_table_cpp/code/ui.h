#pragma once
#include "models.h"
#include <string>

bool         point_in_rect(float px, float py, float x, float y, float w, float h);
KT_Rectangle place_next(const KT_Rectangle& rect, int width, int height, const std::string& x, const std::string& y, int padding = 0);
KT_Rectangle place_inside(const KT_Rectangle& rect, int width, int height, const std::string& x, const std::string& y, int padding = 0);

struct Button {
  int         x = 0, y = 0, width = 0, height = 0;
  std::string text;
  bool pressed() const;
};

bool       immediate_button(KT_Rectangle rect, const std::string& label, nb::object color, nb::object text_color);
nb::object immediate_buttons(nb::object size, nb::list buttons, nb::object color, nb::object text_color);

struct UI_State {
  nb::dict buttons;
  nb::dict highlighted_cards;
  int      window_width       = 0;
  int      window_height      = 0;
  bool     playground         = false;
  int      power_edit_card_id = -1; // Card whose power is being edited; -1 = none.

  UI_State();
  KT_Rectangle place(int width, int height, const std::string& x = "left", const std::string& y = "top", int padding = 0) const;
  nb::object   clicked(float mouse_x, float mouse_y) const;
  void         draw_buttons() const;
};

void bind_ui(nb::module_& m);
