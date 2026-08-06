// Times a single fixed-depth alpha-beta search of the starting position,
// single-threaded. Move ordering does not change the result of the search, only
// how many nodes alpha-beta has to visit, so a faster time at the same depth is
// the whole point. Usage: ordering_test [depth].

#include <chess/ai.h>
#include <chess/gameplay.h>
#include <chess/models.h>
#include <game/minimax.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
  int depth   = argc > 1 ? std::atoi(argv[1]) : 5;
  int plies   = argc > 2 ? std::atoi(argv[2]) : 0;
  int threads = argc > 3 ? std::atoi(argv[3]) : 1;  // 0 = one per core.

  chess::Game_State state = chess::quick_setup(0);
  auto              agent = Agent_Minimax<chess::Game_State>(depth, threads);

  // Walk a few fixed pseudo-random legal moves to reach a real middlegame (with
  // captures and material imbalance), where move ordering actually matters.
  std::srand(12345);
  for (int i = 0; i < plies && !state.is_game_over(); ++i) {
    chess::Move_List moves = chess::legal_moves(state);
    if (moves.empty()) break;
    chess::apply_move(state, moves[std::rand() % moves.size()]);
  }

  state.begin_game();  // The position is built by hand here.
  auto start   = std::chrono::steady_clock::now();
  int  action  = agent.choose_action(state, pending_choice(state));
  auto elapsed = std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - start
  )
                   .count();

  std::printf("depth %d: chose %d in %.1f ms\n", depth, action, elapsed);
  return 0;
}
