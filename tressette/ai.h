#pragma once

#include <game/agent.h>
#include <game/minimax.h>

#include <random>

#include "models.h"

namespace tressette {

// Heuristic state evaluation. Terminal states return +/-1000.
float evaluate_state(Game_State& game, int player_index);

// Sample hidden information: shuffle opponent_hand union stock, then redraw
// the opponent's hand. The current player's hand is fully observed.
Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
);

using Tressette_Agent = Agent_Minimax_Stochastic<Game_State>;

}  // namespace tressette
