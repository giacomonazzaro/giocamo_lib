// Headless match: Agent_MCTS_Stochastic vs Tressette_Agent
// (minimax-stochastic). Plays N games of Tressette, alternating which seat MCTS
// holds, prints per- game scores plus an aggregate win count and per-agent
// wall-clock totals so the comparison can be made equal-time by tuning each
// side's knobs.

#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
#include <game/minimax.h>
#include <tressette/ai.h>
#include <tressette/gameplay.h>
#include <tressette/models.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

static bool parse_int_flag(
  const std::string& arg, const std::string& name, int& out
) {
  std::string prefix = "--" + name + "=";
  if (arg.rfind(prefix, 0) != 0) return false;
  out = std::atoi(arg.c_str() + prefix.size());
  return true;
}

int main(int argc, char** argv) {
  int num_games             = 20;
  int minimax_depth         = 6;
  int minimax_samples       = 20;
  int mcts_iterations       = 1000;
  int mcts_rollout_depth    = 40;
  int mcts_samples          = 20;
  int mcts_time_budget_ms   = 0;  // 0 disables the time bound.
  int rollout_minimax_depth = 2;  // Depth used by the minimax rollout policy.
  int seed                  = 42;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (parse_int_flag(arg, "num-games", num_games)) continue;
    if (parse_int_flag(arg, "minimax-depth", minimax_depth)) continue;
    if (parse_int_flag(arg, "minimax-samples", minimax_samples)) continue;
    if (parse_int_flag(arg, "mcts-iterations", mcts_iterations)) continue;
    if (parse_int_flag(arg, "mcts-rollout-depth", mcts_rollout_depth)) continue;
    if (parse_int_flag(arg, "mcts-samples", mcts_samples)) continue;
    if (parse_int_flag(arg, "mcts-time-budget-ms", mcts_time_budget_ms))
      continue;
    if (parse_int_flag(arg, "rollout-minimax-depth", rollout_minimax_depth))
      continue;
    if (parse_int_flag(arg, "seed", seed)) continue;
    std::cerr << "unknown argument: " << arg << "\n";
    return 1;
  }

  const float mcts_time_budget_seconds = (float)mcts_time_budget_ms / 1000.0f;

  std::cerr << "tressette_match_mcts: games=" << num_games
            << "  minimax(depth=" << minimax_depth
            << ", samples=" << minimax_samples << ")"
            << "  mcts(iters=" << mcts_iterations
            << ", rollout=" << mcts_rollout_depth
            << ", samples=" << mcts_samples
            << ", time_budget_ms=" << mcts_time_budget_ms
            << ", rollout_minimax_depth=" << rollout_minimax_depth << ")"
            << "  seed=" << seed << "\n";

  // Both agents use Agent_Minimax<tressette::Game_State> as their rollout
  // policy. Plain Agent_Minimax (not Stochastic) because:
  //   - the parent Stochastic MCTS has already determinized hidden info via
  //     sample_state(...) before calling mcts_scores, so re-sampling inside
  //     the rollout would scramble information we already committed to;
  //   - Agent_Minimax_Stochastic spawns num_samples threads per call, which
  //     would explode into thousands of short-lived threads inside the
  //     rollout loop.
  using Mcts_Agent_T = Agent_MCTS_Stochastic<
    tressette::Game_State,
    Agent_Minimax<tressette::Game_State>>;

  Agent_MCTS_Stochastic<tressette::Game_State> weak_agent(
    mcts_iterations,
    mcts_rollout_depth,
    mcts_samples,
    /*exploration_constant=*/1.41421356f
    // mcts_time_budget_seconds * 0.01f
  );
  // weak_agent.rollout_agent_factory = []() { return Agent_Random(); };
  // Agent_Random weak_agent;

  Mcts_Agent_T strong_agent(
    mcts_iterations,
    mcts_rollout_depth,
    mcts_samples,
    /*exploration_constant=*/1.41421356f
    // mcts_time_budget_seconds
  );
  strong_agent.rollout_agent_factory = [=]() {
    return Agent_Minimax<tressette::Game_State>(rollout_minimax_depth);
  };

  Timing_Agent timed_weak(&weak_agent, "weak");
  Timing_Agent timed_strong(&strong_agent, "strong");

  // Scores tracked from MCTS's perspective.
  int strong_wins         = 0;
  int weak_wins           = 0;
  int draws               = 0;
  int strong_total_points = 0;
  int weak_total_points   = 0;

  for (int game_index = 0; game_index < num_games; ++game_index) {
    // Alternate which seat MCTS plays so neither side benefits from leading.
    const bool strong_is_player_0 = (game_index % 2 == 0);
    // Agent_Duel.swap reorders the [agent_0, agent_1] array: when swap=true the
    // first argument lands in seat 1 and the second in seat 0.
    Agent_Duel duel(&timed_strong, &timed_weak, /*swap=*/!strong_is_player_0);

    tressette::Game_State state = tressette::quick_setup(seed + game_index);
    while (!state.game_over) {
      std::optional<Choice> choice = state.next_choice();
      if (!choice) break;
      int action_index = duel.choose_action(state, *choice);
      if (action_index < 0) break;
      resolve_choice(state, *choice, action_index);
    }

    int score_player_0 = tressette::compute_player_score(state, 0);
    int score_player_1 = tressette::compute_player_score(state, 1);
    int strong_score   = strong_is_player_0 ? score_player_0 : score_player_1;
    int weak_score     = strong_is_player_0 ? score_player_1 : score_player_0;
    strong_total_points += strong_score;
    weak_total_points += weak_score;
    if (strong_score > weak_score)
      strong_wins += 1;
    else if (strong_score < weak_score)
      weak_wins += 1;
    else
      draws += 1;

    std::cerr << "game " << (game_index + 1) << "/" << num_games
              << "  strong=" << strong_score << "  weak=" << weak_score
              << "  (strong_is_p0=" << strong_is_player_0 << ")\n";
  }

  std::cout << "\nresult:"
            << "  strong_wins=" << strong_wins << "  weak_wins=" << weak_wins
            << "  draws=" << draws
            << "  total_points: strong=" << strong_total_points
            << " weak=" << weak_total_points << "\n";

  std::cout << "compute (wall clock):\n";
  std::cout << "  weak: total=" << timed_weak.total_seconds << "s"
            << "  calls=" << timed_weak.num_calls << "  avg_per_move="
            << (timed_weak.average_seconds_per_move() * 1000.0) << "ms\n";
  std::cout << "  strong:    total=" << timed_strong.total_seconds << "s"
            << "  calls=" << timed_strong.num_calls << "  avg_per_move="
            << (timed_strong.average_seconds_per_move() * 1000.0) << "ms\n";
  const double ratio = (timed_strong.total_seconds > 0.0)
                         ? timed_weak.total_seconds / timed_strong.total_seconds
                         : 0.0;
  std::cout << "  weak/strong time ratio = " << ratio << "\n";
  return 0;
}
