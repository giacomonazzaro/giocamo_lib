// Headless battle: the current MCTS (uniform-random rollouts) vs the softmax
// MCTS (guided, softmax-weighted rollouts). Both run Agent_MCTS_Stochastic with
// the same search budget, so the only difference is the rollout policy. Plays N
// games of Tressette, alternating seats, and prints per-game scores plus the
// running win count.

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
  int mcts_iterations     = 1000;
  int mcts_rollout_depth  = 40;
  int mcts_samples        = 20;
  int mcts_time_budget_ms = 0;  // 0 disables the time bound.
  int seed                = 42;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (parse_int_flag(arg, "num-games", num_games)) continue;
    if (parse_int_flag(arg, "mcts-iterations", mcts_iterations)) continue;
    if (parse_int_flag(arg, "mcts-rollout-depth", mcts_rollout_depth)) continue;
    if (parse_int_flag(arg, "mcts-samples", mcts_samples)) continue;
    if (parse_int_flag(arg, "mcts-time-budget-ms", mcts_time_budget_ms))
      continue;
    if (parse_int_flag(arg, "seed", seed)) continue;
    std::cerr << "unknown argument: " << arg << "\n";
    return 1;
  }

  const float time_budget = (float)mcts_time_budget_ms / 1000.0f;

  std::cerr << "tressette_match_mcts: games=" << num_games
            << "  mcts(iters=" << mcts_iterations
            << ", rollout=" << mcts_rollout_depth
            << ", samples=" << mcts_samples
            << ", time_budget_ms=" << mcts_time_budget_ms << ")  seed=" << seed
            << "\n  current (random rollouts)  vs  softmax (guided rollouts)\n";

  const float exploration_constant = 1.41421356f;

  // Same search budget on both sides; only the rollout policy differs.
  auto current_agent = Agent_MCTS_Stochastic<tressette::Game_State>(
    mcts_iterations, mcts_rollout_depth, mcts_samples, exploration_constant,
    time_budget
  );
  auto softmax_agent = Agent_MCTS_Stochastic<
    tressette::Game_State, Agent_Softmax_Rollout<tressette::Game_State>>(
    mcts_iterations, mcts_rollout_depth, mcts_samples, exploration_constant,
    time_budget
  );
  // Each search thread builds its own rollout agent; hand it the temperature.
  softmax_agent.rollout_agent_factory = []() {
    return Agent_Softmax_Rollout<tressette::Game_State>(/* temperature */ 0.5f);
  };

  auto make_state = [&](int game_index) {
    return tressette::quick_setup(seed + game_index);
  };
  auto score = [](const tressette::Game_State& game, int player_index) {
    return tressette::compute_player_score(game, player_index);
  };

  Benchmark_Result result = benchmark_agents<tressette::Game_State>(
    num_games, make_state, score, softmax_agent, "softmax", current_agent,
    "current"
  );

  std::cout << "\nresult:  softmax_wins=" << result.a_wins
            << "  current_wins=" << result.b_wins << "  draws=" << result.draws
            << "  total_points: softmax=" << result.a_points
            << " current=" << result.b_points << "\n";
  std::cout << "compute (wall clock):\n";
  std::cout << "  current: total=" << result.b_seconds
            << "s  avg_per_move=" << result.b_ms_per_move() << "ms\n";
  std::cout << "  softmax: total=" << result.a_seconds
            << "s  avg_per_move=" << result.a_ms_per_move() << "ms\n";
  return 0;
}
