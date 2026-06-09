#include "ai.h"

#include <cmath>

namespace chess {

// Centipawn-style worth of each piece type, indexed by Piece.
static const float piece_value[7] = {0.0f, 1.0f, 3.0f, 3.0f, 5.0f, 9.0f, 0.0f};

float evaluate_state(Game_State& state, int player) {
  // Terminal positions dominate any heuristic estimate below.
  if (state.winner == player) return 1.0f;
  if (state.winner == 1 - player) return -1.0f;
  if (state.winner == 2) return 0.0f;  // Draw.

  float raw = 0.0f;
  for (int row = 0; row < 8; ++row) {
    for (int col = 0; col < 8; ++col) {
      int value = state.board[row][col];
      if (value == EMPTY) continue;
      int   color = piece_color(value);
      float worth = piece_value[piece_type(value)];

      // A light central bonus: pieces near the middle four files/ranks see more
      // of the board.
      float center_distance = std::abs(row - 3.5f) + std::abs(col - 3.5f);
      worth += (7.0f - center_distance) * 0.05f;

      raw += color == player ? worth : -worth;
    }
  }

  // Squash into (-1, 1): the heuristic asymptotes but never reaches a true
  // win/loss, so the search always prefers an actual checkmate.
  return std::tanh(raw * 0.1f);
}

}  // namespace chess
