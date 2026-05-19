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
  int num_games           = 20;
  int minimax_depth       = 6;
  int minimax_samples     = 20;
  int mcts_iterations     = 1000;
  int mcts_rollout_depth  = 40;
  int mcts_samples        = 20;
  int mcts_time_budget_ms = 0;  // 0 disables the time bound.
  int seed                = 42;

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
            << ", time_budget_ms=" << mcts_time_budget_ms << ")"
            << "  seed=" << seed << "\n";

  // tressette::Tressette_Agent minimax_agent(minimax_depth, minimax_samples);
  Agent_Random minimax_agent;
  // Agent_Random                                 mcts_agent;
  Agent_MCTS_Stochastic<tressette::Game_State> mcts_agent(
    mcts_iterations,
    mcts_rollout_depth,
    mcts_samples,
    /*exploration_constant=*/1.41421356f,
    mcts_time_budget_seconds
  );

  Timing_Agent timed_minimax(&minimax_agent, "minimax");
  Timing_Agent timed_mcts(&mcts_agent, "mcts");

  // Scores tracked from MCTS's perspective.
  int mcts_wins            = 0;
  int minimax_wins         = 0;
  int draws                = 0;
  int mcts_total_points    = 0;
  int minimax_total_points = 0;

  for (int game_index = 0; game_index < num_games; ++game_index) {
    // Alternate which seat MCTS plays so neither side benefits from leading.
    const bool mcts_is_player_0 = (game_index % 2 == 0);
    // Agent_Duel.swap reorders the [agent_0, agent_1] array: when swap=true the
    // first argument lands in seat 1 and the second in seat 0.
    Agent_Duel duel(&timed_mcts, &timed_minimax, /*swap=*/!mcts_is_player_0);

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
    int mcts_score     = mcts_is_player_0 ? score_player_0 : score_player_1;
    int minimax_score  = mcts_is_player_0 ? score_player_1 : score_player_0;
    mcts_total_points += mcts_score;
    minimax_total_points += minimax_score;
    if (mcts_score > minimax_score)
      mcts_wins += 1;
    else if (mcts_score < minimax_score)
      minimax_wins += 1;
    else
      draws += 1;

    std::cerr << "game " << (game_index + 1) << "/" << num_games
              << "  mcts=" << mcts_score << "  minimax=" << minimax_score
              << "  (mcts_is_p0=" << mcts_is_player_0 << ")\n";
  }

  std::cout << "\nresult:"
            << "  mcts_wins=" << mcts_wins << "  minimax_wins=" << minimax_wins
            << "  draws=" << draws
            << "  total_points: mcts=" << mcts_total_points
            << " minimax=" << minimax_total_points << "\n";

  std::cout << "compute (wall clock):\n";
  std::cout << "  minimax: total=" << timed_minimax.total_seconds << "s"
            << "  calls=" << timed_minimax.num_calls << "  avg_per_move="
            << (timed_minimax.average_seconds_per_move() * 1000.0) << "ms\n";
  std::cout << "  mcts:    total=" << timed_mcts.total_seconds << "s"
            << "  calls=" << timed_mcts.num_calls << "  avg_per_move="
            << (timed_mcts.average_seconds_per_move() * 1000.0) << "ms\n";
  const double ratio = (timed_mcts.total_seconds > 0.0)
                         ? timed_minimax.total_seconds /
                             timed_mcts.total_seconds
                         : 0.0;
  std::cout << "  minimax/mcts time ratio = " << ratio << "\n";
  return 0;
}
