#pragma once
#include <optional>
#include <string>
#include <unordered_map>

#include "models.h"

bool         point_in_rect(float px, float py, float x, float y, float w, float h);
KT_Rectangle place_next(const KT_Rectangle& rect, int width, int height, const std::string& x, const std::string& y, int padding = 0);
KT_Rectangle place_inside(const KT_Rectangle& rect, int width, int height, const std::string& x, const std::string& y, int padding = 0);

struct Button {
  int         x = 0, y = 0, width = 0, height = 0;
  std::string text;
  bool pressed() const;
};

// Draws an immediate-mode button. Returns true if it was clicked this frame.
// If color/text_color is empty, defaults to the styled button look.
bool immediate_button(
  KT_Rectangle rect, const std::string& label,
  std::optional<KT_Color> color = std::nullopt,
  std::optional<KT_Color> text_color = std::nullopt
);

struct UI_State {
  // Persistent buttons drawn each frame by draw_buttons().
  std::unordered_map<int, Button> buttons;
  // Highlight overlay keyed by choice index → kt card id.
  std::unordered_map<int, int> highlighted_cards;
  int      window_width       = 0;
  int      window_height      = 0;
  bool     playground         = false;
  int      power_edit_card_id = -1; // KT_Card whose power is being edited; -1 = none.

  UI_State();
  KT_Rectangle      place(int width, int height, const std::string& x = "left", const std::string& y = "top", int padding = 0) const;
  std::optional<int> clicked(float mouse_x, float mouse_y) const;
  void              draw_buttons() const;
};
