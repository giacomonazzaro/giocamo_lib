#include "ui.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <algorithm>

#include "config.h"
#include "raylib.h"
#include "rendering.h"

namespace nb = nanobind;
using namespace nb::literals;

// Default button colors (kept in sync with the tweak dict in config.cpp).
static const Color s_button_color       = {70, 130, 180, 255};
static const Color s_button_hover_color = {90, 150, 200, 255};
static const Color s_button_text_color  = {255, 255, 255, 255};

// Extract a C Raylib Color from a pyray Color cffi object or a (r,g,b,a) tuple.
static Color extract_color(nb::object pycolor) {
  if (nb::isinstance<nb::tuple>(pycolor)) {
    nb::tuple t = nb::cast<nb::tuple>(pycolor);
    return Color{
      (unsigned char)nb::cast<int>(t[0]),
      (unsigned char)nb::cast<int>(t[1]),
      (unsigned char)nb::cast<int>(t[2]),
      (unsigned char)nb::cast<int>(t[3])
    };
  }
  return Color{
    (unsigned char)nb::cast<int>(pycolor.attr("r")),
    (unsigned char)nb::cast<int>(pycolor.attr("g")),
    (unsigned char)nb::cast<int>(pycolor.attr("b")),
    (unsigned char)nb::cast<int>(pycolor.attr("a"))
  };
}

// Build a pyray.Color from a C Raylib Color.
static nb::object to_pycolor(Color c) {
  return nb::module_::import_("pyray").attr("Color")(c.r, c.g, c.b, c.a);
}

bool point_in_rect(float px, float py, float x, float y, float w, float h) {
  return x <= px && px <= x + w && y <= py && py <= y + h;
}

KT_Rectangle place_next(
  const KT_Rectangle& rect,
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

  return KT_Rectangle{nx, ny, (float)width, (float)height};
}

KT_Rectangle place_inside(
  const KT_Rectangle& rect,
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

  return KT_Rectangle{nx, ny, (float)width, (float)height};
}

bool Button::pressed() const {
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();
  return point_in_rect(mx, my, (float)x, (float)y, (float)width, (float)height);
}

bool immediate_button(
  KT_Rectangle       rect,
  const std::string& label,
  nb::object         color,
  nb::object         text_color
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
  else if (color.is_none())
    c = s_button_color;
  else
    c = extract_color(color);

  Color tc = text_color.is_none() ? s_button_text_color : extract_color(text_color);

  Rectangle rl_rect = {rect.x, rect.y, rect.width, rect.height};
  DrawRectangleRounded(rl_rect, 0.3f, 8, c);

  KT_Rectangle tr = place_inside(rect, tw, 20, "center", "center");
  render_text(label, tr.x, tr.y, 20, to_pycolor(tc));

  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
  return hovered;
}

nb::object immediate_buttons(
  nb::object size,
  nb::list   buttons,
  nb::object color,
  nb::object text_color
) {
  int sw = nb::cast<int>(nb::cast<nb::tuple>(size)[0]);
  int sh = nb::cast<int>(nb::cast<nb::tuple>(size)[1]);

  for (int i = 0; i < (int)buttons.size(); i++) {
    nb::tuple  item  = nb::cast<nb::tuple>(buttons[i]);
    nb::tuple  pos   = nb::cast<nb::tuple>(item[0]);
    std::string label = nb::cast<std::string>(item[1]);
    float px = (float)nb::cast<int>(pos[0]);
    float py = (float)nb::cast<int>(pos[1]);
    KT_Rectangle rect = {px, py, (float)sw, (float)sh};
    if (immediate_button(rect, label, color, text_color))
      return nb::int_(i);
  }
  return nb::none();
}

UI_State::UI_State()
    : buttons(nb::dict())
    , highlighted_cards(nb::dict())
    , window_width(kt::WINDOW_WIDTH)
    , window_height(kt::WINDOW_HEIGHT) {}

KT_Rectangle UI_State::place(
  int                width,
  int                height,
  const std::string& x,
  const std::string& y,
  int                padding
) const {
  KT_Rectangle window = {0.0f, 0.0f, (float)window_width, (float)window_height};
  return place_inside(window, width, height, x, y, padding);
}

nb::object UI_State::clicked(float mouse_x, float mouse_y) const {
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return nb::none();
  for (auto item : buttons.attr("items")()) {
    nb::tuple  kv  = nb::cast<nb::tuple>(item);
    nb::object key = kv[0];
    Button&    btn = nb::cast<Button&>(kv[1]);
    if (point_in_rect(
          mouse_x, mouse_y, (float)btn.x, (float)btn.y, (float)btn.width, (float)btn.height
        ))
      return key;
  }
  return nb::none();
}

void UI_State::draw_buttons() const {
  float mx = (float)GetMouseX();
  float my = (float)GetMouseY();
  // Build text color once — it's constant across all buttons.
  nb::object tc_obj = to_pycolor(s_button_text_color);

  for (auto item : buttons.attr("items")()) {
    nb::tuple kv  = nb::cast<nb::tuple>(item);
    Button&   btn = nb::cast<Button&>(kv[1]);

    bool  hovered = point_in_rect(mx, my, (float)btn.x, (float)btn.y, (float)btn.width, (float)btn.height);
    Color c       = hovered ? s_button_hover_color : s_button_color;

    Rectangle    rl_rect = {(float)btn.x, (float)btn.y, (float)btn.width, (float)btn.height};
    DrawRectangleRounded(rl_rect, 0.3f, 8, c);

    int          tw = text_width(btn.text, 20);
    KT_Rectangle br = {(float)btn.x, (float)btn.y, (float)btn.width, (float)btn.height};
    KT_Rectangle tr = place_inside(br, tw, 20, "center", "center");
    render_text(btn.text, tr.x, tr.y, 20, tc_obj);
  }
}

// Extract a KT_Rectangle from any object with .x .y .width .height (duck typing).
// Accepts both our KT_Rectangle and pyray's cffi Rectangle.
static KT_Rectangle rect_from_obj(nb::object r) {
  return KT_Rectangle{
    nb::cast<float>(r.attr("x")),
    nb::cast<float>(r.attr("y")),
    nb::cast<float>(r.attr("width")),
    nb::cast<float>(r.attr("height"))
  };
}

void bind_ui(nb::module_& m) {
  m.def("point_in_rect", &point_in_rect, "px"_a, "py"_a, "x"_a, "y"_a, "w"_a, "h"_a);
  m.def(
    "place_next",
    [](nb::object rect, int width, int height, const std::string& x, const std::string& y, int padding) {
      return place_next(rect_from_obj(rect), width, height, x, y, padding);
    },
    "rect"_a,
    "width"_a,
    "height"_a,
    "x"_a,
    "y"_a,
    "padding"_a = 0
  );
  m.def(
    "place_inside",
    [](nb::object rect, int width, int height, const std::string& x, const std::string& y, int padding) {
      return place_inside(rect_from_obj(rect), width, height, x, y, padding);
    },
    "rect"_a,
    "width"_a,
    "height"_a,
    "x"_a,
    "y"_a,
    "padding"_a = 0
  );

  nb::class_<Button>(m, "Button")
    .def(nb::init<>())
    .def(
      "__init__",
      [](Button* b, int x, int y, int width, int height, std::string text) {
        new (b) Button();
        b->x     = x;
        b->y     = y;
        b->width  = width;
        b->height = height;
        b->text   = std::move(text);
      },
      "x"_a,
      "y"_a,
      "width"_a,
      "height"_a,
      "text"_a = ""
    )
    .def_rw("x", &Button::x)
    .def_rw("y", &Button::y)
    .def_rw("width", &Button::width)
    .def_rw("height", &Button::height)
    .def_rw("text", &Button::text)
    .def("pressed", &Button::pressed);

  m.def(
    "immediate_button",
    [](nb::object rect, const std::string& label, nb::object color, nb::object text_color) {
      return immediate_button(rect_from_obj(rect), label, color, text_color);
    },
    "rectangle"_a,
    "label"_a,
    "color"_a      = nb::none(),
    "text_color"_a = nb::none()
  );
  m.def(
    "immediate_buttons",
    &immediate_buttons,
    "size"_a,
    "buttons"_a,
    "color"_a      = nb::none(),
    "text_color"_a = nb::none()
  );

  nb::class_<UI_State>(m, "UI_State")
    .def(nb::init<>())
    .def_rw("buttons", &UI_State::buttons)
    .def_rw("highlighted_cards", &UI_State::highlighted_cards)
    .def_prop_rw(
      "window_size",
      [](const UI_State& s) { return nb::make_tuple(s.window_width, s.window_height); },
      [](UI_State& s, nb::tuple t) {
        s.window_width  = nb::cast<int>(t[0]);
        s.window_height = nb::cast<int>(t[1]);
      }
    )
    .def_rw("playground", &UI_State::playground)
    .def_rw("power_edit_card_id", &UI_State::power_edit_card_id)
    .def(
      "place",
      &UI_State::place,
      "width"_a,
      "height"_a,
      "x"_a       = "left",
      "y"_a       = "top",
      "padding"_a = 0
    )
    .def("clicked", &UI_State::clicked, "mouse_x"_a, "mouse_y"_a)
    .def("draw_buttons", &UI_State::draw_buttons);
}
