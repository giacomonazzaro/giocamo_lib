// Headless match: Agent_Minimax vs Agent_MCTS for chess. Plays N games,
// alternating which seat minimax holds, and prints per-game results plus an
// aggregate win count and per-agent wall-clock totals.
//
// Chess is perfect-information, so both agents are the non-stochastic variants
// and share the same evaluate_state.

#include <chess/ai.h>
#include <chess/gameplay.h>
#include <chess/models.h>
#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
#include <game/minimax.h>

#include <cstdlib>
#include <iostream>
#include <optional>
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
  int num_games           = 6;
  int minimax_depth       = 3;
  int mcts_iterations     = 2000;
  int mcts_rollout_depth  = 40;
  int mcts_time_budget_ms = 0;  // 0 disables the time bound.
  int max_plies           = 200;  // Long games count as a draw for the summary.
  int seed                = 42;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (parse_int_flag(arg, "num-games", num_games)) continue;
    if (parse_int_flag(arg, "minimax-depth", minimax_depth)) continue;
    if (parse_int_flag(arg, "mcts-iterations", mcts_iterations)) continue;
    if (parse_int_flag(arg, "mcts-rollout-depth", mcts_rollout_depth)) continue;
    if (parse_int_flag(arg, "mcts-time-budget-ms", mcts_time_budget_ms)) continue;
    if (parse_int_flag(arg, "max-plies", max_plies)) continue;
    if (parse_int_flag(arg, "seed", seed)) continue;
    std::cerr << "unknown argument: " << arg << "\n";
    return 1;
  }

  const float mcts_time_budget_seconds = (float)mcts_time_budget_ms / 1000.0f;

  std::cerr << "chess_match_mcts: games=" << num_games
            << "  minimax(depth=" << minimax_depth << ")"
            << "  mcts(iters=" << mcts_iterations
            << ", rollout=" << mcts_rollout_depth
            << ", time_budget_ms=" << mcts_time_budget_ms << ")"
            << "  seed=" << seed << "\n";

  Agent_Minimax<chess::Game_State> minimax_agent(minimax_depth);
  Agent_MCTS<chess::Game_State>    mcts_agent(
    mcts_iterations,
    mcts_rollout_depth,
    /*exploration_constant=*/1.41421356f,
    mcts_time_budget_seconds
  );

  Timing_Agent timed_minimax(&minimax_agent, "minimax");
  Timing_Agent timed_mcts(&mcts_agent, "mcts");

  int minimax_wins = 0;
  int mcts_wins    = 0;
  int draws        = 0;

  for (int game_index = 0; game_index < num_games; ++game_index) {
    // Alternate which seat minimax plays so neither side benefits from leading.
    const bool minimax_is_player_0 = (game_index % 2 == 0);
    Agent_Duel duel(&timed_minimax, &timed_mcts, /*swap=*/!minimax_is_player_0);

    chess::Game_State state = chess::quick_setup(seed + game_index);
    int               plies = 0;
    while (!state.game_over && plies < max_plies) {
      std::optional<Choice> choice = state.next_choice();
      if (!choice) break;
      int action_index = duel.choose_action(state, *choice);
      if (action_index < 0) break;
      resolve_choice(state, *choice, action_index);
      plies += 1;
    }

    int score_player_0 = chess::compute_player_score(state, 0);
    int score_player_1 = chess::compute_player_score(state, 1);
    int minimax_score  = minimax_is_player_0 ? score_player_0 : score_player_1;
    int mcts_score     = minimax_is_player_0 ? score_player_1 : score_player_0;
    if (minimax_score > mcts_score)
      minimax_wins += 1;
    else if (minimax_score < mcts_score)
      mcts_wins += 1;
    else
      draws += 1;

    std::cerr << "game " << (game_index + 1) << "/" << num_games
              << "  minimax=" << minimax_score << "  mcts=" << mcts_score
              << "  plies=" << plies
              << "  (minimax_is_p0=" << minimax_is_player_0 << ")\n";
  }

  std::cout << "\nresult:"
            << "  minimax_wins=" << minimax_wins << "  mcts_wins=" << mcts_wins
            << "  draws=" << draws << "\n";

  std::cout << "compute (wall clock):\n";
  std::cout << "  minimax: total=" << timed_minimax.total_seconds << "s"
            << "  calls=" << timed_minimax.num_calls << "  avg_per_move="
            << (timed_minimax.average_seconds_per_move() * 1000.0) << "ms\n";
  std::cout << "  mcts:    total=" << timed_mcts.total_seconds << "s"
            << "  calls=" << timed_mcts.num_calls << "  avg_per_move="
            << (timed_mcts.average_seconds_per_move() * 1000.0) << "ms\n";
  return 0;
}
