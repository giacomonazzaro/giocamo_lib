#include "ui.h"

#include <string>

#include <chess/gameplay.h>
#include <tabletop/rendering.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

std::vector<Thing> make_chess_squares() {
  std::vector<Thing> squares;
  const float        cell = (float)CHESS_CELL;
  // Squares live in root-local coords; the root is centered on the screen, so
  // the board is centered around the origin. Row 0 (white's back rank) sits at
  // the bottom of the screen.
  for (int row = 0; row < 8; ++row) {
    for (int col = 0; col < 8; ++col) {
      Thing square;
      square.name        = "sq" + std::to_string(row * 8 + col);
      square.shape       = rectangle_shape({cell, cell});
      square.transform.x = ((float)col - 3.5f) * cell;
      square.transform.y = (3.5f - (float)row) * cell;
      // a1 (row 0, col 0) is a dark square, matching a real board.
      bool dark    = (row + col) % 2 == 0;
      square.color = dark ? Color{150, 110, 70, 255} : Color{235, 210, 170, 255};
      squares.push_back(square);
    }
  }
  return squares;
}

const char* chess_piece_glyph(int piece) {
  switch (chess::piece_type(piece)) {
    case chess::PAWN:   return "P";
    case chess::KNIGHT: return "N";
    case chess::BISHOP: return "B";
    case chess::ROOK:   return "R";
    case chess::QUEEN:  return "Q";
    case chess::KING:   return "K";
    default:            return "";
  }
}

Color chess_piece_color(int piece) {
  if (chess::piece_color(piece) == 0) return Color{245, 245, 245, 255};  // White.
  return Color{25, 25, 25, 255};                                         // Black.
}

void draw_chess_hud(const chess::Game_State& state) {
  std::string label;
  Color       color = Color{230, 230, 230, 255};
  if (state.game_over) {
    if (state.winner == 2) {
      label = "Draw";
    } else {
      label = std::string(state.winner == 0 ? "White" : "Black") + " wins!";
    }
  } else {
    label = std::string(state.current_player == 0 ? "White" : "Black") + " to move";
    if (chess::in_check(state, state.current_player)) label += " (check)";
  }
  render_text(label, 30.0f, 24.0f, 28, color);
}
