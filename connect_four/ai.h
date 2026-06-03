#pragma once

#include "models.h"

namespace connect_four {

// Value of `state` from `player`'s perspective: +1 win, -1 loss, 0 otherwise.
// Connect Four is deterministic and perfect-information, and MCTS rollouts run
// to a terminal position (rollout depth exceeds the 42-move cap), so a
// terminal-only score is enough to guide the search.
float evaluate_state(Game_State& state, int player);

}  // namespace connect_four
