#include "ai.h"

#include <cmath>

namespace connect_four {

// Score a single length-WIN window from `player`'s perspective. A window with
// discs from both players is dead (neither can complete it) and scores 0; only
// windows still open for one side count. Opponent threats are weighted a touch
// heavier so the search leans defensive.
static float score_window(int player_count, int opponent_count) {
  float score = 0.0f;
  if (opponent_count == 0) {  // Still completable by `player`.
    if (player_count == 3) score += 6.0f;       // One move from four.
    else if (player_count == 2) score += 2.0f;
    else if (player_count == 1) score += 0.5f;
  }
  if (player_count == 0) {  // Still completable by the opponent.
    if (opponent_count == 3) score -= 8.0f;     // Must be blocked.
    else if (opponent_count == 2) score -= 2.0f;
    else if (opponent_count == 1) score -= 0.5f;
  }
  return score;
}

float evaluate_state(Game_State& state, int player) {
  // Terminal positions dominate any heuristic estimate below.
  if (state.winner == player) return 1.0f;
  if (state.winner == 1 - player) return -1.0f;
  if (state.game_over) return 0.0f;  // Full board, drawn.

  const int opponent = 1 - player;
  float     raw      = 0.0f;

  // Center-column control: central discs sit in the most lines, so owning them
  // is worth a steady bonus.
  const int center = COLS / 2;
  for (int row = 0; row < ROWS; ++row) {
    if (state.board[row][center] == player) raw += 3.0f;
    else if (state.board[row][center] == opponent) raw -= 3.0f;
  }

  // Every length-WIN window in the four directions (horizontal, vertical, both
  // diagonals). Anchor at (row,col) and only keep windows that fit on the board.
  static const int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  for (int row = 0; row < ROWS; ++row) {
    for (int col = 0; col < COLS; ++col) {
      for (const auto& d : directions) {
        int end_row = row + (WIN - 1) * d[0];
        int end_col = col + (WIN - 1) * d[1];
        if (end_row < 0 || end_row >= ROWS) continue;
        if (end_col < 0 || end_col >= COLS) continue;

        int player_count   = 0;
        int opponent_count = 0;
        for (int k = 0; k < WIN; ++k) {
          int value = state.board[row + k * d[0]][col + k * d[1]];
          if (value == player) ++player_count;
          else if (value == opponent) ++opponent_count;
        }
        raw += score_window(player_count, opponent_count);
      }
    }
  }

  // Squash into (-1, 1): the heuristic asymptotes but never reaches a true
  // win/loss, so the search always prefers an actual four-in-a-row.
  return std::tanh(raw * 0.08f);
}

}  // namespace connect_four
