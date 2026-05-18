#pragma once

// Inference wrapper around the TorchScript-traced value network produced by
// `python tressette/train_value.py --export`.
//
// Requires LibTorch. Only active when TORCH_AVAILABLE is defined (set by
// CMake when find_package(Torch) succeeds). Safe to include unconditionally.
// Usage:
//   Value_Net net("tressette_value_traced.pt");
//   float score = net.predict(state, player_index);

#ifdef TORCH_AVAILABLE
#include <torch/script.h>

#include <string>
#include <vector>

#include "models.h"

namespace tressette {

struct Value_Net {
  explicit Value_Net(const std::string& model_path);

  // Returns the predicted final score for player_index given the current state.
  float predict(const Game_State& state, int player_index);

  // Batch inference on a list of states. Returns one score per state.
  // Much faster than calling predict() in a loop when n > ~8.
  std::vector<float> predict_batch(
    const std::vector<Game_State>& states, int player_index
  );

 private:
  torch::jit::script::Module module_;
};

}  // namespace tressette
#endif  // TORCH_AVAILABLE
