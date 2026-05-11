#include "ai.h"

#include <game/game.h>
#include <game/minimax.h>
#include <nanobind/nanobind.h>

#include <algorithm>
#include <random>

#include "gameplay.h"
#include "models.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace tressette {

float evaluate_state(Game_State& game, int player_index) {
  int   my  = compute_player_score(game, player_index);
  int   opp = compute_player_score(game, 1 - player_index);
  float s   = float(my - opp);
  if (game.is_game_over()) {
    if (my > opp) return +1000.0f;
    if (my < opp) return -1000.0f;
    return 0.0f;
  }
  return s;
}

// Sample hidden information: shuffle opponent_hand union stock, then redraw
// the opponent's hand. The current player's hand is fully observed.
Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
) {
  Game_State sampled        = state;
  const int  opponent_index = 1 - player_index;
  Player&    opponent       = sampled.players[opponent_index];
  const int  hand_size      = (int)opponent.hand.size();

  std::vector<int> hidden = opponent.hand;
  hidden.insert(hidden.end(), sampled.stock.begin(), sampled.stock.end());
  std::shuffle(hidden.begin(), hidden.end(), rng);

  opponent.hand.assign(hidden.begin(), hidden.begin() + hand_size);
  sampled.stock.assign(hidden.begin() + hand_size, hidden.end());
  return sampled;
}

}  // namespace tressette

using namespace tressette;

void bind_agent(nb::module_& m) {
  nb::class_<Tressette_Agent>(m, "Tressette_Agent")
    .def(nb::init<>())
    .def(nb::init<int, int>(), "max_depth"_a = 6, "num_samples"_a = 12)
    .def_rw("max_depth", &Tressette_Agent::max_depth)
    .def_rw("num_samples", &Tressette_Agent::num_samples)
    .def("message", [](Tressette_Agent&, const std::string&) {})
    .def(
      "choose_action",
      [](Tressette_Agent& self, Game_State& g, const Choice& c) {
        return self.choose_action(g, c);
      },
      "state"_a,
      "choice"_a
    );
}
