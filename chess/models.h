#pragma once

#include <game/game.h>

#include <array>

namespace chess {

// Piece type stored in a board slot, ignoring color. A slot holds 0 when empty,
// a positive value for a white piece, and the negative of the same value for a
// black piece. So board values run -6..6, and the magnitude is the Piece type.
enum Piece {
  EMPTY  = 0,
  PAWN   = 1,
  KNIGHT = 2,
  BISHOP = 3,
  ROOK   = 4,
  QUEEN  = 5,
  KING   = 6
};

// Player of a board value: 0 white (value > 0), 1 black (value < 0), -1 empty.
inline int piece_color(int value) {
  if (value > 0) return 0;
  if (value < 0) return 1;
  return -1;
}

// Type (PAWN..KING) of a board value, regardless of color.
inline int piece_type(int value) { return value < 0 ? -value : value; }

// Board value for a piece of `type` owned by `player` (0 white, 1 black).
inline int make_piece(int type, int player) {
  return player == 0 ? type : -type;
}

// board[row][col]; row 0 is white's back rank, row 7 is black's. Column 0 is
// file a. Each slot holds a signed Piece value (see above). One byte per cell,
// so a whole board is 64 bytes and trivially copyable — cheap to copy on its
// own during move generation, separate from the rest of the game state.
using Board = std::array<std::array<signed char, 8>, 8>;

struct Game_State : Game {
  Board board;

  int current_player = 0;   // 0 white, 1 black.
  int winner         = -1;  // -1 none yet, 0/1 the winner, 2 a draw.

  // Square (row*8 + col) a pawn may capture en passant this turn, or -1.
  int en_passant_target = -1;

  // Half-moves since the last pawn move or capture, for the 50-move draw.
  int halfmove_clock = 0;

  // Castling rights: cleared once the king or the matching rook moves (or the
  // rook is captured).
  bool white_can_castle_kingside  = true;
  bool white_can_castle_queenside = true;
  bool black_can_castle_kingside  = true;
  bool black_can_castle_queenside = true;

  bool game_over = false;

  Game_State() {
    for (auto& row : board) row.fill(EMPTY);
  }

  bool   is_game_over() const override { return game_over; }
  Choice next_choice() override;

  void switch_turn() { current_player = 1 - current_player; }
};

}  // namespace chess
