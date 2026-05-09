#include "gameplay.h"

#include <game_cpp/game.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>

#include <algorithm>

#include "models.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace tressette {

int trick_winner(const Game_State& state) {
  // Caller guarantees state.trick.size() == 2.
  const int   led_card_id      = state.trick[0];
  const int   response_card_id = state.trick[1];
  const Card& led_card         = state.all_cards[led_card_id];
  const Card& response_card    = state.all_cards[response_card_id];
  const int   leader           = state.trick_leader;
  // Different suit -> leader wins automatically (no trumps in Tressette).
  if (response_card.suit != led_card.suit) return leader;
  // Same suit -> stronger rank wins.
  return strength(response_card.rank) > strength(led_card.rank) ? 1 - leader
                                                                : leader;
}

int compute_player_score(const Game_State& state, int player_index) {
  int thirds = 0;
  for (int cid : state.players[player_index].tricks_won) {
    thirds += card_thirds(state.all_cards[cid].rank);
  }
  int score = thirds / 3;                                   // floor.
  if (state.last_trick_winner == player_index) score += 1;  // ultima bonus.
  return score;
}

// Returns the legal cards in the current player's hand for the next play.
// During the draw phase (stock non-empty) any card is legal. After the stock
// is empty, the responder must follow suit if possible.
static std::vector<int> legal_cards(const Game_State& state) {
  const Player& player        = state.players[state.current_player];
  const bool must_follow_suit = state.stock.empty() && state.trick.size() == 1;
  if (!must_follow_suit) return player.hand;

  const Suit       led_suit = state.all_cards[state.trick[0]].suit;
  std::vector<int> matches;
  for (int cid : player.hand) {
    if (state.all_cards[cid].suit == led_suit) matches.push_back(cid);
  }
  if (!matches.empty()) return matches;
  return player.hand;
}

// Helper: remove card_id from a vector<int> (single occurrence).
static void erase_card(std::vector<int>& v, int card_id) {
  auto it = std::find(v.begin(), v.end(), card_id);
  if (it != v.end()) v.erase(it);
}

void play_card(Game_State& state, int card_id) {
  Player& player = state.players[state.current_player];
  erase_card(player.hand, card_id);
  state.trick.push_back(card_id);
  // printf(
  //   "Player %d plays card rank %d suit %d\n",
  //   state.current_player,
  //   state.all_cards[card_id].rank,
  //   state.all_cards[card_id].suit
  // );

  if (state.trick.size() < 2) {
    // First card of the trick: the responder plays next.
    state.switch_turn();
    state.notify_cards_changed();
    return;
  }

  // Trick complete: resolve it.
  const int winner        = trick_winner(state);
  state.last_trick_winner = winner;
  Player& winner_p        = state.players[winner];
  for (int cid : state.trick) winner_p.tricks_won.push_back(cid);
  state.trick.clear();
  state.trick_leader   = winner;
  state.current_player = winner;

  // Draw phase: winner draws first, loser second (if cards remain).
  if (!state.stock.empty()) {
    int top = state.stock.back();
    state.stock.pop_back();
    state.players[winner].hand.push_back(top);
  }
  if (!state.stock.empty()) {
    int top = state.stock.back();
    state.stock.pop_back();
    state.players[1 - winner].hand.push_back(top);
  }

  // Game ends when both hands are empty.
  if (state.players[0].hand.empty() && state.players[1].hand.empty()) {
    state.game_over = true;
  }
  state.notify_cards_changed();
}

std::optional<Choice> Game_State::next_choice() {
  if (game_over) return std::nullopt;

  Choice choice;
  choice.player_index     = current_player;
  choice.description      = "play";
  choice.text_description = "Play a card";

  choice.actions = [](Game& g) -> Choose {
    auto&       s = static_cast<Game_State&>(g);
    Choose_Card c;
    c.targets = legal_cards(s);
    return c;
  };

  choice.resolve = [](Game& g, int index) -> std::vector<Choice> {
    auto&            s     = static_cast<Game_State&>(g);
    std::vector<int> legal = legal_cards(s);
    play_card(s, legal[index]);
    return {};
  };

  return choice;
}

}  // namespace tressette

using namespace tressette;

void bind_gameplay(nb::module_& m) {
  m.def(
    "compute_player_score", &compute_player_score, "state"_a, "player_index"_a
  );
  m.def("play_card", &play_card, "state"_a, "card_id"_a);
  m.def("trick_winner", &trick_winner, "state"_a);
  m.def("strength", &strength, "rank"_a);
  m.def("card_thirds", &card_thirds, "rank"_a);
}
