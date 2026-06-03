#include "ui.h"

#include <string>

#include <connect_four/gameplay.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

std::vector<Thing> make_connect_four_columns() {
  std::vector<Thing> columns;
  const float        cell = (float)CONNECT_FOUR_CELL;
  // Columns live in root-local coords; the root is centered on the screen, so
  // the board is centered around the origin.
  for (int col = 0; col < connect_four::COLS; ++col) {
    Thing column;
    column.name        = "col" + std::to_string(col);
    column.size        = {cell, cell * (float)connect_four::ROWS};
    column.transform.x =
      ((float)col - (float)(connect_four::COLS - 1) / 2.0f) * cell;
    column.transform.y = 0.0f;
    column.color       = Color{40, 80, 160, 220};  // Translucent blue board.
    column.capacity    = connect_four::ROWS;
    columns.push_back(column);
  }
  return columns;
}

Color connect_four_disc_color(int cell) {
  if (cell == connect_four::P0) return Color{210, 70, 70, 255};  // Red.
  return Color{225, 200, 70, 255};                               // Yellow.
}

void draw_connect_four_hud(const connect_four::Game_State& state) {
  std::string label;
  Color       color;
  if (state.game_over) {
    if (state.winner == -1) {
      label = "Draw";
    } else {
      label = std::string(state.winner == connect_four::P0 ? "Red" : "Yellow") +
              " wins!";
    }
    color = Color{230, 230, 230, 255};
  } else {
    label =
      std::string(state.current_player == connect_four::P0 ? "Red" : "Yellow") +
      " to move";
    color = connect_four_disc_color(state.current_player);
  }
  render_text(label, 30.0f, 24.0f, 28, color);
}
