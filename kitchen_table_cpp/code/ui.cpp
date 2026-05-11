#include "ui.h"

#include <algorithm>

#include "config.h"
#include "raylib.h"
#include "rendering.h"

// Default button colors (kept in sync with the tweak dict in config.cpp).
static const Color s_button_color       = {70, 130, 180, 255};
static const Color s_button_hover_color = {90, 150, 200, 255};
static const Color s_button_text_color  = {255, 255, 255, 255};


bool point_in_rect(float px, float py, float x, float y, float w, float h) {
  return x <= px && px <= x + w && y <= py && py <= y + h;
}

Rectangle place_next(
  const Rectangle& rect,
  int                  width,
  int                  height,
  const std::string&   x,
  const std::string&   y,
  int                  padding
) {
  float nx, ny;

  if (x == "left")
    nx = rect.x - (float)width - (float)padding;
  else if (x == "right")
    nx = rect.x + rect.width + (float)padding;
  else // center
    nx = rect.x + rect.width / 2.0f - (float)width / 2.0f;

  if (y == "top")
    ny = rect.y - (float)height - (float)padding;
  else if (y == "bottom")
    ny = rect.y + rect.height + (float)padding;
  else // center
    ny = rect.y + rect.height / 2.0f - (float)height / 2.0f;

  return Rectangle{nx, ny, (float)width, (float)height};
}

Rectangle place_inside(
  const Rectangle& rect,
  int                  width,
  int                  height,
  const std::string&   x,
  const std::string&   y,
  int                  padding
) {
  float nx, ny;

  if (x == "left")
    nx = rect.x + (float)padding;
  else if (x == "right")
    nx = rect.x + rect.width - (float)width - (float)padding;
  else // center
    nx = rect.x + rect.width / 2.0f - (float)width / 2.0f;

  if (y == "top")
    ny = rect.y + (float)padding;
  else if (y == "bottom")
    ny = rect.y + rect.height - (float)height - (float)padding;
  else // center
    ny = rect.y + rect.height / 2.0f - (float)height / 2.0f;

  return Rectangle{nx, ny, (float)width, (float)height};
}

bool Button::pressed() const {
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();
  return point_in_rect(mx, my, (float)x, (float)y, (float)width, (float)height);
}

bool immediate_button(
  Rectangle              rect,
  const std::string&        label,
  std::optional<Color>   color,
  std::optional<Color>   text_color
) {
  // Expand width to fit label text if necessary.
  int tw     = text_width(label, 20);
  rect.width = std::max(rect.width, (float)(tw + 20));

  float mx      = (float)GetMouseX();
  float my      = (float)GetMouseY();
  bool  hovered = point_in_rect(mx, my, rect.x, rect.y, rect.width, rect.height);

  // Resolve button background color: hover always wins.
  Color c;
  if (hovered)
    c = s_button_hover_color;
  else if (!color)
    c = s_button_color;
  else
    c = *color;

  Color tc = text_color ? *text_color : s_button_text_color;

  Rectangle rl_rect = {rect.x, rect.y, rect.width, rect.height};
  DrawRectangleRounded(rl_rect, 0.3f, 8, c);

  Rectangle tr = place_inside(rect, tw, 20, "center", "center");
  render_text(label, tr.x, tr.y, 20, tc);

  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
  return hovered;
}

UI_State::UI_State()
    : window_width(kt::WINDOW_WIDTH), window_height(kt::WINDOW_HEIGHT) {}

Rectangle UI_State::place(
  int                width,
  int                height,
  const std::string& x,
  const std::string& y,
  int                padding
) const {
  Rectangle window = {0.0f, 0.0f, (float)window_width, (float)window_height};
  return place_inside(window, width, height, x, y, padding);
}

std::optional<int> UI_State::clicked(float mouse_x, float mouse_y) const {
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return std::nullopt;
  for (const auto& [key, btn] : buttons) {
    if (point_in_rect(
          mouse_x, mouse_y, (float)btn.x, (float)btn.y, (float)btn.width, (float)btn.height
        ))
      return key;
  }
  return std::nullopt;
}

void UI_State::draw_buttons() const {
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();

  for (const auto& [key, btn] : buttons) {
    bool     hovered = point_in_rect(mx, my, (float)btn.x, (float)btn.y, (float)btn.width, (float)btn.height);
    Color c       = hovered ? s_button_hover_color : s_button_color;

    Rectangle    rl_rect = {(float)btn.x, (float)btn.y, (float)btn.width, (float)btn.height};
    DrawRectangleRounded(rl_rect, 0.3f, 8, c);

    int          tw = text_width(btn.text, 20);
    Rectangle br = {(float)btn.x, (float)btn.y, (float)btn.width, (float)btn.height};
    Rectangle tr = place_inside(br, tw, 20, "center", "center");
    render_text(btn.text, tr.x, tr.y, 20, s_button_text_color);
  }
}
