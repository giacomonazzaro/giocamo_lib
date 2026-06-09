#pragma once

#include "models.h"

namespace chess {

// Value of `state` from `player`'s perspective. Terminal: +1 win, -1 loss, 0
// draw. Otherwise a material-plus-light-position heuristic squashed into
// (-1, 1). Chess rollouts rarely reach a natural terminal within the rollout
// depth, so this estimate (not a terminal-only score) is what guides the search
// at the cutoff.
float evaluate_state(Game_State& state, int player);

}  // namespace chess
