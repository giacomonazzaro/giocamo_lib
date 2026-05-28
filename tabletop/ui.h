#pragma once
#include <optional>
#include <string>
#include <unordered_map>

#include "tabletop.h"

bool      point_in_rect(float px, float py, const Rectangle& rect);
Rectangle place_next(
  const Rectangle&   rect,
  int                width,
  int                height,
  const std::string& x,
  const std::string& y,
  int                padding = 0
);
Rectangle place_inside(
  const Rectangle&   rect,
  int                width,
  int                height,
  const std::string& x,
  const std::string& y,
  int                padding = 0
);

struct Button {
  Rectangle   rect;
  std::string text;
  bool        pressed(const Input& input) const;
};

static const Color s_button_color       = {70, 130, 180, 255};
static const Color s_button_hover_color = {90, 150, 200, 255};
static const Color s_button_text_color  = {255, 255, 255, 255};

// Draws an immediate-mode button. Returns true if it was clicked this frame.
bool immediate_button(
  Rectangle          rect,
  const std::string& label,
  const Input&       input,
  Color              color             = s_button_color,
  Color              highlighted_color = s_button_hover_color,
  Color              text_color        = s_button_text_color,
  int                text_size         = 20
);

struct UI_State {
  // Persistent buttons drawn each frame by draw_buttons().
  std::unordered_map<int, Button> buttons;
  // Highlight overlay keyed by choice index → thing id.
  std::unordered_map<int, int> highlighted_things;
  int                          window_width;
  int                          window_height;
  const Input*                 input = nullptr;

  UI_State(int width, int height);
  Rectangle place(
    int                width,
    int                height,
    const std::string& x       = "left",
    const std::string& y       = "top",
    int                padding = 0
  ) const;
  std::optional<int> clicked(const Input& input) const;
  void               draw_buttons(const Input& input) const;
};
