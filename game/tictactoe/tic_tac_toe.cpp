// Tic-tac-toe demo for the game engine.
// Human (X) plays first against minimax AI (O).

#include "tic_tac_toe.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "agent.h"
#include "game.h"
#include "minimax.h"

// Prints the board with cell numbers (1-9) shown in empty cells as a hint.
static void print_board(const Tic_Tac_Toe& ttt) {
  static const char marks[3] = {' ', 'X', 'O'};
  std::cout << "\n";
  for (int r = 0; r < 3; ++r) {
    std::cout << "  ";
    for (int c = 0; c < 3; ++c) {
      const int idx = r * 3 + c;
      const int v   = ttt.board[idx];
      if (v == 0) {
        // Show the cell number as a hint for input.
        std::cout << (idx + 1);
      } else {
        std::cout << marks[v];
      }
      if (c < 2) std::cout << " | ";
    }
    std::cout << "\n";
    if (r < 2) std::cout << "  ---------\n";
  }
  std::cout << "\n";
}

struct Agent_Terminal_TTT : Agent {
  void message(const std::string& msg) override { std::cout << msg << "\n"; }

  int choose_action(Game& state, const Choice& choice) override {
    auto& ttt = static_cast<Tic_Tac_Toe&>(state);
    print_board(ttt);

    const Choose       options = choice.actions(state);
    const Choose_Card& card    = std::get<Choose_Card>(options);

    const char mark = (choice.player_index == 0) ? 'X' : 'O';
    while (true) {
      std::cout << "Player " << mark << ", choose a cell (1-9): ";
      int input = 0;
      if (!(std::cin >> input)) {
        if (std::cin.eof()) std::exit(0);
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "  not a number, try again.\n";
        continue;
      }
      const int cell = input - 1;
      for (std::size_t i = 0; i < card.targets.size(); ++i) {
        if (card.targets[i] == cell) return static_cast<int>(i);
      }
      std::cout << "  cell " << input << " is not available, try again.\n";
    }
  }
};

int main() {
  Tic_Tac_Toe                game;
  Agent_Terminal_TTT         human;
  Agent_Minimax<Tic_Tac_Toe> minimax_ai(9);
  Agent_Duel                 duel(&human, &minimax_ai, false);

  std::cout << "Tic-tac-toe: you are X (first), minimax AI is O.\n";

  game.begin_game(game.next_choice());  // The opening decision to present.
  game_loop(game, duel, [](Game& g) {
    auto& ttt = static_cast<Tic_Tac_Toe&>(g);
    print_board(ttt);
    const int w = ttt.winner();
    if (w == -1) {
      std::cout << "Draw!\n";
    } else {
      const char mark = (w == 0) ? 'X' : 'O';
      std::cout << "Player " << mark << " wins!\n";
    }
  });

  return 0;
}
