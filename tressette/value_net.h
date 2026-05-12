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

#include "gameplay.h"
#include "models.h"

namespace tressette {

// Converts a list of card ids (0..39) to a length-40 float tensor (indicator).
inline torch::Tensor card_ids_to_tensor(const std::vector<int>& ids) {
  auto t = torch::zeros({40});
  for (int id : ids) t[id] = 1.0f;
  return t;
}

struct Value_Net {
  explicit Value_Net(const std::string& model_path) {
    module_ = torch::jit::load(model_path);
    module_.eval();
  }

  // Returns the predicted final score for player_index given the current state.
  float predict(const Game_State& state, int player_index) {
    const int opponent = 1 - player_index;
    auto my_hand  = card_ids_to_tensor(state.players[player_index].hand).unsqueeze(0);
    auto my_capt  = card_ids_to_tensor(state.players[player_index].tricks_won).unsqueeze(0);
    auto opp_capt = card_ids_to_tensor(state.players[opponent].tricks_won).unsqueeze(0);

    torch::NoGradGuard no_grad;
    return module_.forward({my_hand, my_capt, opp_capt}).toTensor().item<float>();
  }

  // Batch inference on a list of states. Returns one score per state.
  // Much faster than calling predict() in a loop when n > ~8.
  std::vector<float> predict_batch(
    const std::vector<Game_State>& states, int player_index
  ) {
    const int n = static_cast<int>(states.size());
    if (n == 0) return {};

    auto my_hand_t  = torch::zeros({n, 40});
    auto my_capt_t  = torch::zeros({n, 40});
    auto opp_capt_t = torch::zeros({n, 40});
    float* hand_ptr = my_hand_t.data_ptr<float>();
    float* capt_ptr = my_capt_t.data_ptr<float>();
    float* opp_ptr  = opp_capt_t.data_ptr<float>();

    const int opp = 1 - player_index;
    for (int i = 0; i < n; ++i) {
      for (int id : states[i].players[player_index].hand)       hand_ptr[i * 40 + id] = 1.0f;
      for (int id : states[i].players[player_index].tricks_won) capt_ptr[i * 40 + id] = 1.0f;
      for (int id : states[i].players[opp].tricks_won)          opp_ptr[i * 40 + id]  = 1.0f;
    }

    torch::NoGradGuard no_grad;
    auto output   = module_.forward({my_hand_t, my_capt_t, opp_capt_t}).toTensor();
    auto accessor = output.accessor<float, 1>();
    std::vector<float> result(n);
    for (int i = 0; i < n; ++i) result[i] = accessor[i];
    return result;
  }

 private:
  torch::jit::script::Module module_;
};

}  // namespace tressette
#endif  // TORCH_AVAILABLE
