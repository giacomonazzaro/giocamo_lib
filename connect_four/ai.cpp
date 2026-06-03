#include "ai.h"

namespace connect_four {

float evaluate_state(Game_State& state, int player) {
  if (state.winner == player) return 1.0f;
  if (state.winner == 1 - player) return -1.0f;
  return 0.0f;  // Non-terminal or draw.
}

}  // namespace connect_four
