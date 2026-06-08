#pragma once

#include <game/game.h>

#include <array>
#include <optional>

namespace connect_four {

// Standard Connect Four: 7 columns, 6 rows, four-in-a-row wins.
constexpr int COLS = 7;
constexpr int ROWS = 6;
constexpr int WIN  = 4;

// Value stored in a board slot.
enum Cell { EMPTY = -1, P0 = 0, P1 = 1 };

struct Game_State : Game {
  // board[row][col]; row 0 is the bottom row. Each slot holds EMPTY / P0 / P1.
  std::array<std::array<int, COLS>, ROWS> board;
  int  current_player = 0;
  int  winner         = -1;  // -1 = none yet, else the winning player (0/1).
  bool game_over      = false;

  Game_State() {
    for (auto& row : board) row.fill(EMPTY);
  }

  bool                  is_game_over() const override { return game_over; }
  Choice next_choice();

  void switch_turn() { current_player = 1 - current_player; }
};

}  // namespace connect_four
