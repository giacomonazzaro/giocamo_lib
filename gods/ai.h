#pragma once

#include <game/agent.h>
#include <game/minimax.h>

#include "models.h"

float evaluate_state(Game_State& game, int player_index);

Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
);

// Stochastic minimax agent for Gods.
// Mirrors gods/gameplay.py:Agent_Minimax_Stochastic_Gods.
//
// Each call to choose_action:
//   1. Generates num_samples sampled states (shuffling hidden information).
//   2. Runs alpha-beta minimax_search on each sample to fixed depth.
//   3. Aggregates votes — each sample votes for its argmax action.
//   4. Returns the action with the most votes.
//
// Stays on the C++ side so the search can copy Game_State by value without
// crossing the Python boundary.
using Agent_Minimax_Stochastic_Gods = Agent_Minimax_Stochastic<Game_State>;
