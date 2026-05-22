#pragma once

#include <random>

#include "models.h"

namespace scopa {

// Heuristic state evaluation. Terminal states return +/-1000.
float evaluate_state(Game_State& game, int player_index);

// Sample hidden information for an MCTS rollout: shuffle the opponent's
// hand together with the stock and re-deal both. The calling player's hand
// and the table are fully observed and left untouched.
Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
);

}  // namespace scopa
