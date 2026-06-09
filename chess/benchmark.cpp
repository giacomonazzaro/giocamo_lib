// Headless match: time-bounded iterative-deepening alpha-beta (Agent_Minimax_Timed)
// vs MCTS whose leaves are scored by a shallow alpha-beta (Agent_MCTS with a
// minimax leaf_evaluator), for chess, through the shared benchmark_agents()
// helper. Both agents are root-parallel and get the same per-move time budget,
// so the result says which search spends a fixed thinking budget better.

#include <chess/ai.h>
#include <chess/gameplay.h>
#include <chess/models.h>
#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
#include <game/minimax.h>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

static bool parse_int_flag(
  const std::string& arg, const std::string& name, int& out
) {
  std::string prefix = "--" + name + "=";
  if (arg.rfind(prefix, 0) != 0) return false;
  out = std::atoi(arg.c_str() + prefix.size());
  return true;
}

int main(int argc, char** argv) {
  int num_games         = 10;
  int mcts_iterations   = 1000000;  // High cap; the time budget is the real bound.
  int rollout_depth     = 40;       // Unused while the MCTS leaf evaluator is set.
  int mcts_leaf_depth   = 2;        // Alpha-beta depth at each MCTS leaf.
  int minimax_max_depth = 64;       // Deepening cap for the timed minimax.
  int time_budget_ms    = 1000;
  int num_threads       = 0;        // 0 = one per hardware core (both agents).
  int seed              = 42;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (parse_int_flag(arg, "num-games", num_games)) continue;
    if (parse_int_flag(arg, "mcts-iterations", mcts_iterations)) continue;
    if (parse_int_flag(arg, "rollout-depth", rollout_depth)) continue;
    if (parse_int_flag(arg, "mcts-leaf-depth", mcts_leaf_depth)) continue;
    if (parse_int_flag(arg, "minimax-max-depth", minimax_max_depth)) continue;
    if (parse_int_flag(arg, "time-budget-ms", time_budget_ms)) continue;
    if (parse_int_flag(arg, "threads", num_threads)) continue;
    if (parse_int_flag(arg, "seed", seed)) continue;
    std::cerr << "unknown argument: " << arg << "\n";
    return 1;
  }

  const float time_budget_seconds = (float)time_budget_ms / 1000.0f;

  std::cerr << "chess_benchmark: games=" << num_games
            << "  time_budget_ms=" << time_budget_ms
            << "  mcts_leaf_depth=" << mcts_leaf_depth
            << "  minimax_max_depth=" << minimax_max_depth
            << "  threads=" << (num_threads > 0 ? std::to_string(num_threads)
                                                : std::string("auto"))
            << "  seed=" << seed << "\n";

  // Time-bounded iterative-deepening alpha-beta (root-parallel).
  auto minimax_agent = Agent_Minimax_Timed<chess::Game_State>(
    time_budget_seconds, minimax_max_depth, num_threads
  );

  // MCTS with a depth-N alpha-beta leaf evaluator (root-parallel).
  auto mcts_agent = Agent_MCTS<chess::Game_State>(
    mcts_iterations,
    rollout_depth,
    /*exploration_constant=*/1.41421356f,
    time_budget_seconds,
    num_threads
  );
  mcts_agent.leaf_evaluator =
    [mcts_leaf_depth](const chess::Game_State& state, int player) {
      const float       infinity = std::numeric_limits<float>::infinity();
      chess::Game_State copy     = state;  // minimax needs a mutable copy.
      return minimax_detail::minimax(copy, mcts_leaf_depth, -infinity, infinity, player);
    };

  auto make_state = [seed](int game_index) {
    return chess::quick_setup(seed + game_index);
  };
  auto score = [](const chess::Game_State& game, int player) {
    return chess::compute_player_score(game, player);
  };

  Benchmark_Result result = benchmark_agents<chess::Game_State>(
    num_games, make_state, score, minimax_agent, "minimax", mcts_agent, "mcts"
  );

  std::cout << "\nresult (from minimax's perspective):"
            << "  minimax_wins=" << result.a_wins
            << "  mcts_wins=" << result.b_wins
            << "  draws=" << result.draws << "\n";
  std::cout << "points:  minimax=" << result.a_points
            << "  mcts=" << result.b_points << "\n";
  std::cout << "compute:  minimax=" << result.a_ms_per_move() << "ms/move"
            << "  mcts=" << result.b_ms_per_move() << "ms/move\n";
  return 0;
}
