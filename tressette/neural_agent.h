#pragma once

// Stochastic minimax agent using a neural value network at leaf nodes.
// Samples num_samples hidden states, runs alpha-beta on each, majority-votes.
// Requires LibTorch. Only active when TORCH_AVAILABLE is defined (set by CMake
// when find_package(Torch) succeeds). Safe to include unconditionally.

#ifdef TORCH_AVAILABLE
#include <game/agent.h>
#include <game/game.h>
#include <game/minimax.h>  // argmax

#include <limits>
#include <optional>
#include <random>
#include <vector>

#include "ai.h"        // sample_state
#include "gameplay.h"  // compute_player_score
#include "models.h"
#include "value_net.h"

namespace tressette {

// Alpha-beta minimax with neural leaf evaluation.
// Terminal states use the true score; depth-0 states use the value network.
inline float minimax_neural(
  Game_State& state,
  int         depth,
  float       alpha,
  float       beta,
  int         player_index,
  Value_Net&  net
) {
  if (state.is_game_over())
    return static_cast<float>(compute_player_score(state, player_index));
  if (depth == 0) return net.predict(state, player_index);

  std::optional<Choice> choice = state.next_choice();
  if (!choice) return net.predict(state, player_index);
  const int n = action_options_count(choice->actions(state));
  if (n == 0) return net.predict(state, player_index);

  const bool  maximizing = choice->player_index == player_index;
  const float inf        = std::numeric_limits<float>::infinity();
  float       value      = maximizing ? -inf : inf;
  for (int a = 0; a < n; ++a) {
    Game_State child = state;
    resolve_choice(child, *choice, a);
    const float score =
      minimax_neural(child, depth - 1, alpha, beta, player_index, net);
    if (maximizing) {
      value = std::max(value, score);
      alpha = std::max(alpha, value);
    } else {
      value = std::min(value, score);
      beta  = std::min(beta, value);
    }
    if (alpha >= beta) break;
  }
  return value;
}

struct Agent_Minimax_Neural : Agent {
  Value_Net net;
  int       max_depth;
  int       num_samples;

  Agent_Minimax_Neural(
    const std::string& model_path, int max_depth = 3, int num_samples = 20
  )
      : net(model_path), max_depth(max_depth), num_samples(num_samples) {}

  void message(const std::string&) override {}

  int choose_action(Game& game, const Choice& choice) override {
    Game_State& concrete    = static_cast<Game_State&>(game);
    const int   num_actions = action_options_count(choice.actions(game));
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::vector<int>                 votes(num_actions, 0);

    for (int s = 0; s < num_samples; ++s) {
      Game_State  sampled = sample_state(concrete, choice.player_index, rng);
      const float inf     = std::numeric_limits<float>::infinity();
      std::vector<float> scores(num_actions, -inf);
      for (int a = 0; a < num_actions; ++a) {
        Game_State child = sampled;
        resolve_choice(child, choice, a);
        scores[a] =
          minimax_neural(child, max_depth, -inf, inf, choice.player_index, net);
      }
      votes[argmax(scores)] += 1;
    }
    return static_cast<int>(argmax(votes));
  }
};

}  // namespace tressette
#endif  // TORCH_AVAILABLE
