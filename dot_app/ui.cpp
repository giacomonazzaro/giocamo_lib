#include "ui.h"

#include <tabletop/config.h>
#include <tabletop/rendering.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

// Dot colors for the three rows.
static const Color BLUE_DOT  = {70, 130, 200, 255};
static const Color BLACK_DOT = {45, 45, 45, 255};
static const Color RED_DOT   = {200, 70, 70, 255};

// Draw `count` dots in a horizontal row centered on the card, at the given
// fraction of the card height above/below center (negative is up).
static void draw_dot_row(int count, float height_fraction, Color color) {
  const float radius  = 11.0f;
  const float spacing = 30.0f;
  float       y       = height_fraction * (float)tt::CARD_HEIGHT;
  float       start_x = -(float)(count - 1) * spacing / 2.0f;
  for (int i = 0; i < count; i++) {
    DrawCircle((int)(start_x + (float)i * spacing), (int)y, radius, color);
  }
}

std::function<void(const Table_State&, const Input&, bool)>
make_dot_card_draw_callback(const std::vector<dot::Card>& deck, int id) {
  return [&deck, id](const Table_State&, const Input&, bool face_up) {
    if (!face_up) return;
    const dot::Card& card = deck[id];

    // Three rows of dots: blue on top, black in the middle, red at the bottom.
    draw_dot_row(card.blue_dots, -0.28f, BLUE_DOT);
    draw_dot_row(card.black_dots, 0.0f, BLACK_DOT);
    draw_dot_row(card.red_dots, 0.28f, RED_DOT);

    // A small star marker in the top-left corner distinguishes star cards.
    if (card.is_star) {
      float half_w = (float)tt::CARD_WIDTH / 2.0f;
      float half_h = (float)tt::CARD_HEIGHT / 2.0f;
      render_text("*", 8.0f - half_w, 4.0f - half_h, 40, Color{255, 215, 0, 255});
    }
  };
}
