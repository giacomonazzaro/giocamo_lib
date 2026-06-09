// Headless benchmark: plain MCTS (uniform-random rollouts) with two different
// determinization-sample counts, given the same per-move time budget. Tests
// whether spending the budget on more determinizations (many samples) beats
// fewer determinizations searched more deeply (few samples).

#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
#include <tressette/ai.h>
#include <tressette/gameplay.h>
#include <tressette/models.h>

#include <cstdlib>
#include <iostream>
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
  int num_games           = 20;
  int mcts_iterations     = 1000000;  // High cap so the time budget is what binds.
  int mcts_rollout_depth  = 40;
  int samples_a           = 20;
  int samples_b           = 200;
  int mcts_time_budget_ms = 1000;  // Per move; the shared budget for both sides.
  int seed                = 42;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (parse_int_flag(arg, "num-games", num_games)) continue;
    if (parse_int_flag(arg, "mcts-iterations", mcts_iterations)) continue;
    if (parse_int_flag(arg, "mcts-rollout-depth", mcts_rollout_depth)) continue;
    if (parse_int_flag(arg, "samples-a", samples_a)) continue;
    if (parse_int_flag(arg, "samples-b", samples_b)) continue;
    if (parse_int_flag(arg, "mcts-time-budget-ms", mcts_time_budget_ms))
      continue;
    if (parse_int_flag(arg, "seed", seed)) continue;
    std::cerr << "unknown argument: " << arg << "\n";
    return 1;
  }

  const float time_budget          = (float)mcts_time_budget_ms / 1000.0f;
  const float exploration_constant = 1.41421356f;

  std::cerr << "tressette_match_mcts: games=" << num_games
            << "  rollout=" << mcts_rollout_depth
            << "  time_budget_ms=" << mcts_time_budget_ms
            << "  iters_cap=" << mcts_iterations << "  seed=" << seed
            << "\n  " << samples_a << " samples  vs  " << samples_b
            << " samples (same per-move time)\n";

  // Plain MCTS on both sides; the only difference is the number of
  // determinization samples each spends its time budget across.
  auto agent_a = Agent_MCTS_Stochastic<tressette::Game_State>(
    mcts_iterations, mcts_rollout_depth, samples_a, exploration_constant,
    time_budget
  );
  auto agent_b = Agent_MCTS_Stochastic<tressette::Game_State>(
    mcts_iterations, mcts_rollout_depth, samples_b, exploration_constant,
    time_budget
  );

  auto make_state = [&](int game_index) {
    return tressette::quick_setup(seed + game_index);
  };
  auto score = [](const tressette::Game_State& game, int player_index) {
    return tressette::compute_player_score(game, player_index);
  };

  std::string name_a = std::to_string(samples_a) + "-samples";
  std::string name_b = std::to_string(samples_b) + "-samples";

  Benchmark_Result result = benchmark_agents<tressette::Game_State>(
    num_games, make_state, score, agent_a, name_a.c_str(), agent_b,
    name_b.c_str()
  );

  std::cout << "\nresult:  " << name_a << "_wins=" << result.a_wins << "  "
            << name_b << "_wins=" << result.b_wins << "  draws=" << result.draws
            << "  total_points: " << name_a << "=" << result.a_points << " "
            << name_b << "=" << result.b_points << "\n";
  std::cout << "compute (wall clock):\n";
  std::cout << "  " << name_a << ": avg_per_move=" << result.a_ms_per_move()
            << "ms\n";
  std::cout << "  " << name_b << ": avg_per_move=" << result.b_ms_per_move()
            << "ms\n";
  return 0;
}
