#pragma once

#include <array>

#include "game.h"

// Cell values: 0 = empty, 1 = X (player 0), 2 = O (player 1).
struct Tic_Tac_Toe : Game {
  std::array<int, 9> board{};
  int                current_player = 0;

  // Returns the winning player index (0 or 1), or -1 if no winner yet.
  int winner() const {
    static const int lines[8][3] = {
      {0, 1, 2},
      {3, 4, 5},
      {6, 7, 8},  // rows.
      {0, 3, 6},
      {1, 4, 7},
      {2, 5, 8},  // cols.
      {0, 4, 8},
      {2, 4, 6},  // diagonals.
    };
    for (const auto& line : lines) {
      const int a = board[line[0]];
      if (a != 0 && a == board[line[1]] && a == board[line[2]]) {
        return a - 1;
      }
    }
    return -1;
  }

  bool is_full() const {
    for (int v : board) {
      if (v == 0) return false;
    }
    return true;
  }

  bool is_game_over() const override { return winner() != -1 || is_full(); }

  std::optional<Choice> next_choice() override {
    if (is_game_over()) return std::nullopt;

    Choice choice;
    choice.player_index     = current_player;
    choice.description      = "place";
    choice.text_description = "Place your mark";

    // Targets are the empty cell indices, recomputed from current state.
    choice.actions = [](Game& g) -> Choose {
      auto&       ttt = static_cast<Tic_Tac_Toe&>(g);
      Choose_Card c;
      for (int i = 0; i < 9; ++i) {
        if (ttt.board[i] == 0) c.targets.push_back(i);
      }
      return c;
    };

    choice.resolve = [](Game& g, int index) -> std::vector<Choice> {
      auto&            ttt = static_cast<Tic_Tac_Toe&>(g);
      std::vector<int> empties;
      for (int i = 0; i < 9; ++i) {
        if (ttt.board[i] == 0) empties.push_back(i);
      }
      const int cell     = empties[index];
      ttt.board[cell]    = ttt.current_player + 1;
      ttt.current_player = 1 - ttt.current_player;
      return {};
    };

    return choice;
  }
};

// Win/loss evaluation for tic-tac-toe from `player_index`'s perspective.
float evaluate_state(const Tic_Tac_Toe& ttt, int player_index) {
  const int w = ttt.winner();
  if (w == player_index) return 1000.0f;
  if (w != -1) return -1000.0f;
  return 0.0f;
}
