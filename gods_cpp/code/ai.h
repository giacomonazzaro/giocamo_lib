#pragma once

#include <nanobind/nanobind.h>

#include <game_cpp/agent.h>
#include <game_cpp/minimax.h>

#include "models.h"

namespace nb = nanobind;

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
struct Agent_Minimax_Stochastic_Gods : Agent {
  int max_depth   = 6;
  int num_samples = 20;

  Agent_Minimax_Stochastic_Gods() = default;
  Agent_Minimax_Stochastic_Gods(int max_depth, int num_samples)
      : max_depth(max_depth), num_samples(num_samples) {}

  void message(const std::string&) override {}
  int  choose_action(Game& state, const Choice& choice) override;

  // Heuristic position score from player_index's perspective.
  // Mirrors evaluate_state + evaluate_heuristic in Python.
  static float evaluate_state(Game_State& game, int player_index);
};

void bind_agent(nb::module_& m);
