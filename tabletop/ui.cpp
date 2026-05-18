#include "ui.h"

#include <algorithm>

#include "config.h"
#include "raylib.h"
#include "rendering.h"

bool point_in_rect(float px, float py, const Rectangle& rect) {
  return rect.x <= px && px <= rect.x + rect.width && rect.y <= py &&
         py <= rect.y + rect.height;
}

Rectangle place_next(
  const Rectangle&   rect,
  int                width,
  int                height,
  const std::string& x,
  const std::string& y,
  int                padding
) {
  float nx, ny;

  if (x == "left")
    nx = rect.x - (float)width - (float)padding;
  else if (x == "right")
    nx = rect.x + rect.width + (float)padding;
  else  // center
    nx = rect.x + rect.width / 2.0f - (float)width / 2.0f;

  if (y == "top")
    ny = rect.y - (float)height - (float)padding;
  else if (y == "bottom")
    ny = rect.y + rect.height + (float)padding;
  else  // center
    ny = rect.y + rect.height / 2.0f - (float)height / 2.0f;

  return Rectangle{nx, ny, (float)width, (float)height};
}

Rectangle place_inside(
  const Rectangle&   rect,
  int                width,
  int                height,
  const std::string& x,
  const std::string& y,
  int                padding
) {
  float nx, ny;

  if (x == "left")
    nx = rect.x + (float)padding;
  else if (x == "right")
    nx = rect.x + rect.width - (float)width - (float)padding;
  else  // center
    nx = rect.x + rect.width / 2.0f - (float)width / 2.0f;

  if (y == "top")
    ny = rect.y + (float)padding;
  else if (y == "bottom")
    ny = rect.y + rect.height - (float)height - (float)padding;
  else  // center
    ny = rect.y + rect.height / 2.0f - (float)height / 2.0f;

  return Rectangle{nx, ny, (float)width, (float)height};
}

bool Button::pressed(const Input& input) const {
  if (!input.left_pressed) return false;
  return point_in_rect((float)input.mouse_x, (float)input.mouse_y, rect);
}

bool immediate_button(
  Rectangle          rect,
  const std::string& label,
  const Input&       input,
  Color              color,
  Color              highlighted_color,
  Color              text_color,
  int                text_size
) {
  // Expand width to fit label text if necessary.
  int tw     = text_width(label, text_size);
  rect.width = std::max(rect.width, (float)(tw + text_size));

  float mx      = (float)input.mouse_x;
  float my      = (float)input.mouse_y;
  bool  hovered = point_in_rect(mx, my, rect);

  // Resolve button background color: hover always wins.
  Color c = hovered ? highlighted_color : color;

  Rectangle rl_rect = {rect.x, rect.y, rect.width, rect.height};
  DrawRectangleRounded(rl_rect, 0.3f, 8, c);

  Rectangle tr = place_inside(rect, tw, text_size, "center", "center");
  render_text(label, tr.x, tr.y, text_size, text_color);

  if (!input.left_pressed) return false;
  return hovered;
}

UI_State::UI_State(int width, int height)
    : window_width(width), window_height(height) {}

Rectangle UI_State::place(
  int width, int height, const std::string& x, const std::string& y, int padding
) const {
  Rectangle window = {0.0f, 0.0f, (float)window_width, (float)window_height};
  return place_inside(window, width, height, x, y, padding);
}

std::optional<int> UI_State::clicked(const Input& input) const {
  if (!input.left_pressed) return std::nullopt;
  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;
  for (const auto& [key, btn] : buttons) {
    if (point_in_rect(mx, my, btn.rect)) return key;
  }
  return std::nullopt;
}

void UI_State::draw_buttons(const Input& input) const {
  float mx = (float)input.mouse_x;
  float my = (float)input.mouse_y;

  for (const auto& [key, btn] : buttons) {
    bool  hovered = point_in_rect(mx, my, btn.rect);
    Color c       = hovered ? s_button_hover_color : s_button_color;

    Rectangle rl_rect = {
      btn.rect.x, btn.rect.y, btn.rect.width, btn.rect.height
    };
    DrawRectangleRounded(rl_rect, 0.3f, 8, c);

    int       tw = text_width(btn.text, 20);
    Rectangle br = {btn.rect.x, btn.rect.y, btn.rect.width, btn.rect.height};
    Rectangle tr = place_inside(br, tw, 20, "center", "center");
    render_text(btn.text, tr.x, tr.y, 20, s_button_text_color);
  }
}
