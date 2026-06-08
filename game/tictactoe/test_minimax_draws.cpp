// Verifies that two perfect minimax agents always draw at tic-tac-toe.
// Plain minimax is deterministic, so we get variety by pre-placing X's first
// move in each of the 9 cells, then letting minimax finish the game from there.

#include <cstdlib>
#include <iostream>

#include "agent.h"
#include "game.h"
#include "minimax.h"
#include "tic_tac_toe.h"

// Plays a single game from `start` to terminal between minimax_x and minimax_o.
// Returns the winner index (0 = X, 1 = O), or -1 for a draw.
static int play_game(Tic_Tac_Toe start, Agent& minimax_x, Agent& minimax_o) {
  Agent_Duel duel(&minimax_x, &minimax_o, false);
  start.begin_game(start.next_choice());  // The opening decision to present.
  game_loop(start, duel);
  return start.winner();
}

int main() {
  Agent_Minimax<Tic_Tac_Toe> minimax_x(9);
  Agent_Minimax<Tic_Tac_Toe> minimax_o(9);

  int draws  = 0;
  int x_wins = 0;
  int o_wins = 0;

  // Iterate the 9 possible X opening moves to force game variety.
  for (int first_cell = 0; first_cell < 9; ++first_cell) {
    Tic_Tac_Toe game;
    // Pre-place X's first move and pass the turn to O.
    game.board[first_cell] = 1;
    game.current_player    = 1;

    const int w = play_game(game, minimax_x, minimax_o);
    if (w == -1)
      ++draws;
    else if (w == 0)
      ++x_wins;
    else
      ++o_wins;

    std::cout << "X opens at cell " << first_cell << ": "
              << (w == -1  ? "draw"
                  : w == 0 ? "X wins"
                           : "O wins")
              << "\n";
  }

  std::cout << "\nResults: " << draws << " draws, " << x_wins << " X wins, "
            << o_wins << " O wins (out of 9 games).\n";

  if (x_wins != 0 || o_wins != 0) {
    std::cerr << "FAIL: minimax-vs-minimax should always draw.\n";
    return 1;
  }
  std::cout << "PASS\n";
  return 0;
}
