// Plays one whole chess game, minimax against minimax (same agent drives both
// sides), to confirm the search runs a full game end to end without crashing or
// producing an illegal move. Reports the winner, the game length, and the time
// per move. Usage: selfplay_test [depth] [max_plies].

#include <chess/ai.h>
#include <chess/gameplay.h>
#include <chess/models.h>
#include <game/game.h>
#include <game/minimax.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
  int depth     = argc > 1 ? std::atoi(argv[1]) : 3;
  int max_plies = argc > 2 ? std::atoi(argv[2]) : 200;
  int threads   = argc > 3 ? std::atoi(argv[3]) : 1;  // 0 = one per core.

  chess::Game_State game  = chess::quick_setup(0);
  auto              agent = Agent_Minimax<chess::Game_State>(depth, threads);

  int  plies = 0;
  auto start = std::chrono::steady_clock::now();
  while (!game.is_game_over() && plies < max_plies) {
    if (game_frame(game, agent)) ++plies;
  }
  double seconds =
    std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
      .count();

  // winner: -1 none, 0/1 the winner, 2 a draw.
  std::printf(
    "game over: winner=%d  plies=%d  %.2fs  (%.0f ms/move)\n",
    game.winner,
    plies,
    seconds,
    plies ? 1000.0 * seconds / plies : 0.0
  );
  return 0;
}
