#pragma once

#include <game/agent.h>
#include <game/minimax.h>

#include <random>

#include "models.h"

namespace tressette {

// When true, evaluate_state returns a reward in [0,1] (0.5 = even) — the range
// UCB1's exploration constant assumes. When false (default) it uses the raw
// scale. Switchable so the two can be compared; raw tested no worse here.
extern bool use_normalized_evaluation;

// Heuristic state evaluation from player_index's perspective.
float evaluate_state(Game_State& game, int player_index);

// Sample hidden information: shuffle opponent_hand union stock, then redraw
// the opponent's hand. The current player's hand is fully observed.
Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
);

using Tressette_Agent = Agent_Minimax_Stochastic<Game_State>;

}  // namespace tressette
