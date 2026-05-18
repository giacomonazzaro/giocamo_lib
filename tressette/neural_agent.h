#pragma once

// Stochastic minimax agent using a neural value network at leaf nodes.
// Samples num_samples hidden states, runs alpha-beta on each, majority-votes.
// Requires LibTorch. Only active when TORCH_AVAILABLE is defined (set by CMake
// when find_package(Torch) succeeds). Safe to include unconditionally.

#ifdef TORCH_AVAILABLE
#include <game/agent.h>
#include <game/game.h>

#include <string>

#include "models.h"
#include "value_net.h"

namespace tressette {

// Alpha-beta minimax with neural leaf evaluation.
// Terminal states use the true score; depth-0 states use the value network.
float minimax_neural(
  Game_State& state,
  int         depth,
  float       alpha,
  float       beta,
  int         player_index,
  Value_Net&  net
);

struct Agent_Minimax_Neural : Agent {
  Value_Net net;
  int       max_depth;
  int       num_samples;

  Agent_Minimax_Neural(
    const std::string& model_path, int max_depth = 3, int num_samples = 20
  );

  void message(const std::string&) override {}

  int choose_action(Game& game, const Choice& choice) override;
};

}  // namespace tressette
#endif  // TORCH_AVAILABLE
