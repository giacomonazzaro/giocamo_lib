// Tic-tac-toe demo for the game_cpp engine.
// Modes:
//   ./tic_tac_toe         — human vs human (1v1).
//   ./tic_tac_toe ai      — human (X) vs random AI (O).
//   ./tic_tac_toe ai-first — random AI (X) vs human (O).

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#include "agent.h"
#include "game.h"

// Cell values: 0 = empty, 1 = X (player 0), 2 = O (player 1).
struct Tic_Tac_Toe : Game {
  std::array<int, 9> board{};
  int                current_player = 0;

  // Returns the winning player index (0 or 1), or -1 if no winner yet.
  int winner() const {
    static const int lines[8][3] = {
      {0, 1, 2}, {3, 4, 5}, {6, 7, 8},  // rows.
      {0, 3, 6}, {1, 4, 7}, {2, 5, 8},  // cols.
      {0, 4, 8}, {2, 4, 6},             // diagonals.
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

  bool is_game_over() const override {
    return winner() != -1 || is_full();
  }

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
      const int cell        = empties[index];
      ttt.board[cell]       = ttt.current_player + 1;
      ttt.current_player    = 1 - ttt.current_player;
      return {};
    };

    return choice;
  }
};

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
  void message(const std::string& msg) override {
    std::cout << msg << "\n";
  }

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

int main(int argc, char** argv) {
  const std::string mode = (argc > 1) ? argv[1] : "1v1";

  Tic_Tac_Toe        game;
  Agent_Terminal_TTT human;
  Agent_Random       ai(12345);

  // Build the duel based on mode.
  std::unique_ptr<Agent> duel;
  if (mode == "1v1") {
    std::cout << "Mode: human (X) vs human (O).\n";
    duel = std::make_unique<Agent_Duel>(&human, &human, false);
  } else if (mode == "ai") {
    std::cout << "Mode: human (X) vs random AI (O).\n";
    duel = std::make_unique<Agent_Duel>(&human, &ai, false);
  } else if (mode == "ai-first") {
    std::cout << "Mode: random AI (X) vs human (O).\n";
    duel = std::make_unique<Agent_Duel>(&ai, &human, false);
  } else {
    std::cerr << "Unknown mode: " << mode << "\n";
    std::cerr << "Usage: " << argv[0] << " [1v1|ai|ai-first]\n";
    return 1;
  }

  game_loop(game, *duel, [](Game& g) {
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
