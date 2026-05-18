#ifdef TORCH_AVAILABLE
#include "value_net.h"

namespace tressette {

// Converts a list of card ids (0..39) to a length-40 float tensor (indicator).
static torch::Tensor card_ids_to_tensor(const std::vector<int>& ids) {
  auto t = torch::zeros({40});
  for (int id : ids) t[id] = 1.0f;
  return t;
}

Value_Net::Value_Net(const std::string& model_path) {
  module_ = torch::jit::load(model_path);
  module_.eval();
}

float Value_Net::predict(const Game_State& state, int player_index) {
  const int opponent = 1 - player_index;
  auto my_hand  = card_ids_to_tensor(state.players[player_index].hand).unsqueeze(0);
  auto my_capt  = card_ids_to_tensor(state.players[player_index].tricks_won).unsqueeze(0);
  auto opp_capt = card_ids_to_tensor(state.players[opponent].tricks_won).unsqueeze(0);

  torch::NoGradGuard no_grad;
  return module_.forward({my_hand, my_capt, opp_capt}).toTensor().item<float>();
}

std::vector<float> Value_Net::predict_batch(
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

}  // namespace tressette
#endif  // TORCH_AVAILABLE
