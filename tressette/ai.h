#pragma once

#include <game/agent.h>
#include <game/minimax.h>

#include "models.h"

namespace tressette {

float evaluate_state(Game_State& game, int player_index);

Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
);

using Tressette_Agent = Agent_Minimax_Stochastic<Game_State>;

}  // namespace tressette
