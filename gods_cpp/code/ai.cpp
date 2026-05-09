#include "ai.h"

#include <game_cpp/game.h>
#include <game_cpp/minimax.h>
#include <nanobind/nanobind.h>

#include <algorithm>
#include <chrono>
#include <random>

#include "gameplay.h"
#include "models.h"

namespace nb = nanobind;
using namespace nb::literals;

float evaluate_state(Game_State& game, int player_index) {
  if (!game.is_game_over()) {
    // Heuristic for non-terminal positions.
    int   my_score  = compute_player_score(game, player_index);
    int   opp_score = compute_player_score(game, 1 - player_index);
    float score     = float(my_score - opp_score);
    int   my_hand   = (int)game.players[player_index].hand.size();
    int   opp_hand  = (int)game.players[1 - player_index].hand.size();
    score += 0.1f * (my_hand - opp_hand);
    int my_wonders  = (int)game.players[player_index].wonders.size();
    int opp_wonders = (int)game.players[1 - player_index].wonders.size();
    score += 0.2f * (my_wonders - opp_wonders);
    int my_deck  = (int)game.players[player_index].deck.size();
    int opp_deck = (int)game.players[1 - player_index].deck.size();
    score += 0.05f * (my_deck - opp_deck);
    return score;
  }
  // Terminal: large reward for win/loss; tie favors the inactive player
  // (whoever didn't move into the tied state).
  int my_score  = compute_player_score(game, player_index);
  int opp_score = compute_player_score(game, 1 - player_index);
  int diff      = my_score - opp_score;
  if (diff > 0) return +1000.0f;
  if (diff < 0) return -1000.0f;
  return (player_index == game.current_player) ? -1000.0f : +1000.0f;
}

// Sample hidden information: shuffle opponent's (hand + deck) and our deck.
// Mirrors Agent_Minimax_Stochastic._sample_state.
Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
) {
  Game_State       sampled        = state;
  int              opponent_index = 1 - player_index;
  Player&          opponent       = sampled.players[opponent_index];
  int              hand_size      = (int)opponent.hand.size();
  std::vector<int> hidden         = opponent.hand;
  hidden.insert(hidden.end(), opponent.deck.begin(), opponent.deck.end());
  std::shuffle(hidden.begin(), hidden.end(), rng);
  opponent.hand.assign(hidden.begin(), hidden.begin() + hand_size);
  opponent.deck.assign(hidden.begin() + hand_size, hidden.end());
  Player& me = sampled.players[player_index];
  std::shuffle(me.deck.begin(), me.deck.end(), rng);
  return sampled;
}

void bind_agent(nb::module_& m) {
  nb::class_<Agent_Minimax_Stochastic_Gods>(m, "Agent_Minimax_Stochastic_Gods")
    .def(nb::init<>())
    .def(nb::init<int, int>(), "max_depth"_a = 6, "num_samples"_a = 20)
    .def_rw("max_depth", &Agent_Minimax_Stochastic_Gods::max_depth)
    .def_rw("num_samples", &Agent_Minimax_Stochastic_Gods::num_samples)
    .def("message", [](Agent_Minimax_Stochastic_Gods&, const std::string&) {})
    .def(
      "choose_action",
      [](Agent_Minimax_Stochastic_Gods& self, Game_State& g, const Choice& c) {
        return self.choose_action(g, c);
      },
      "state"_a,
      "choice"_a
    );
}
