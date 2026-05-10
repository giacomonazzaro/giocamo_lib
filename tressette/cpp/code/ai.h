#pragma once

#include <game_cpp/agent.h>
#include <game_cpp/minimax.h>
#include <nanobind/nanobind.h>

#include "models.h"

namespace nb = nanobind;

namespace tressette {

float evaluate_state(Game_State& game, int player_index);

Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
);

using Tressette_Agent = Agent_Minimax_Stochastic<Game_State>;

}  // namespace tressette

void bind_agent(nb::module_& m);
