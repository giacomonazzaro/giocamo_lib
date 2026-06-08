#include "gameplay.h"

#include <string>

namespace connect_four {

std::vector<int> legal_columns(const Game_State& state) {
  std::vector<int> columns;
  for (int col = 0; col < COLS; ++col) {
    // A column has room if its top row is still empty.
    if (state.board[ROWS - 1][col] == EMPTY) columns.push_back(col);
  }
  return columns;
}

int drop_row(const Game_State& state, int col) {
  for (int row = 0; row < ROWS; ++row) {
    if (state.board[row][col] == EMPTY) return row;
  }
  return -1;
}

// Consecutive slots owned by `player` starting one step past (row,col) along
// (d_row,d_col).
static int count_direction(
  const Game_State& state, int row, int col, int d_row, int d_col, int player
) {
  int count = 0;
  int r     = row + d_row;
  int c     = col + d_col;
  while (r >= 0 && r < ROWS && c >= 0 && c < COLS &&
         state.board[r][c] == player) {
    ++count;
    r += d_row;
    c += d_col;
  }
  return count;
}

int check_winner(const Game_State& state) {
  // Horizontal, vertical, and both diagonals. For each filled slot, count the
  // run reaching both ways along the axis.
  static const int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  for (int row = 0; row < ROWS; ++row) {
    for (int col = 0; col < COLS; ++col) {
      int player = state.board[row][col];
      if (player == EMPTY) continue;
      for (const auto& d : directions) {
        int line = 1 + count_direction(state, row, col, d[0], d[1], player) +
                   count_direction(state, row, col, -d[0], -d[1], player);
        if (line >= WIN) return player;
      }
    }
  }
  return -1;
}

void apply_move(Game_State& state, int col) {
  int row = drop_row(state, col);
  if (row < 0) return;  // Caller guarantees a legal (non-full) column.
  state.board[row][col] = state.current_player;

  state.winner = check_winner(state);
  if (state.winner != -1) {
    state.game_over = true;  // Someone connected four.
  } else if (legal_columns(state).empty()) {
    state.game_over = true;  // Full board, no winner: a draw.
  } else {
    state.switch_turn();
  }
}

int compute_player_score(const Game_State& state, int player) {
  return state.winner == player ? 1 : 0;
}

Game_State quick_setup(int /*seed*/) {
  return Game_State();  // Empty board, player 0 to move.
}

Choice Game_State::next_choice() {
  if (game_over) return Choice{};

  Choice choice;
  choice.player_index     = current_player;
  choice.description      = "drop";
  choice.text_description = "Drop a disc";

  // Offer the legal columns; option index i corresponds to legal_columns()[i].
  choice.actions = [](Game& game) -> Choose {
    Game_State&   state = static_cast<Game_State&>(game);
    Choose_Option option;
    for (int col : legal_columns(state)) {
      option.targets.push_back(std::to_string(col));
    }
    return option;
  };

  // Drop into the chosen column, then return the next decision.
  choice.resolve = [](Game& game, int index) -> Choice {
    Game_State&      state   = static_cast<Game_State&>(game);
    std::vector<int> columns = legal_columns(state);
    apply_move(state, columns[index]);
    return state.next_choice();
  };

  return choice;
}

}  // namespace connect_four
