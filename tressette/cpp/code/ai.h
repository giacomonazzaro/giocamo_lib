#pragma once

#include <game_cpp/agent.h>
#include <game_cpp/minimax.h>
#include <nanobind/nanobind.h>

#include "models.h"

namespace nb = nanobind;

namespace tressette {

// Stochastic minimax agent for Tressette.
//
// Each call to choose_action:
//   1. Generates num_samples sampled states (shuffling opponent's hand and the
//      stock together, then redrawing the opponent's hand).
//   2. Runs alpha-beta minimax_search on each sample to fixed depth.
//   3. Returns the action with the most votes across samples.
// struct Agent_Minimax_Stochastic_Tressette : Agent {
//   int max_depth   = 6;
//   int num_samples = 12;

//   Agent_Minimax_Stochastic_Tressette() = default;
//   Agent_Minimax_Stochastic_Tressette(int max_depth, int num_samples)
//       : max_depth(max_depth), num_samples(num_samples) {}

//   void message(const std::string&) override {}
//   int  choose_action(Game& state, const Choice& choice) override;

//   // Position score from player_index's perspective. Mirrors the gods
//   version:
//   // current point delta, with terminal positions clamped to +-1000.
//   static float evaluate_state(Game_State& game, int player_index);
// };

float evaluate_state(Game_State& game, int player_index);

Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
);

using Tressette_Agent = Agent_Minimax_Stochastic<Game_State>;

}  // namespace tressette

void bind_agent(nb::module_& m);
