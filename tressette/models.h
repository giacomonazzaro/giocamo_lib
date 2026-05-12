#pragma once

#include <game/game.h>

#include <functional>
#include <string>
#include <vector>

namespace tressette {

// Tressette strength order, from highest to lowest:
//   3 > 2 > 1(Asso) > 10(Re) > 9(Cavallo) > 8(Donna) > 7 > 6 > 5 > 4.
// Returns 0..9 (higher = stronger).
int strength(int rank);

// Card value in thirds-of-a-point (so the score function can sum integers
// then floor-divide by 3 at the end):
//   Asso        -> 3 thirds (= 1 point)
//   2,3,8,9,10  -> 1 third  (= 1/3 point)
//   4,5,6,7     -> 0 thirds.
int card_thirds(int rank);

// Carte napoletane suits.
enum class Suit {
  COPPE,
  DENARI,
  SPADE,
  BASTONI,
};

// Static card identity. 40 cards = 4 suits x 10 ranks.
// Rank meaning (carte napoletane):
//   1 = Asso, 2..7 = number cards, 8 = Donna, 9 = Cavallo, 10 = Re.
// Lives in the tressette:: namespace so its typeinfo is distinct from the
// tabletop `::Card` type — otherwise nanobind treats both as the
// same type and the second registration is dropped.
struct Card {
  int  id   = 0;  // 0..39, index into Game_State.all_cards.
  int  rank = 1;  // 1..10.
  Suit suit = Suit::COPPE;
};

// Player state: hand of card ids and the cards won in tricks.
struct Player {
  std::string      name;
  std::vector<int> hand;
  std::vector<int> tricks_won;
};

// Full Tressette game state. Subclasses game's abstract Game so the
// templated minimax / game_loop work directly on it.
struct Game_State : Game {
  std::vector<Card>   all_cards;  // 40 fixed cards.
  std::vector<Player> players;    // exactly 2.
  std::vector<int>    stock;      // face-down draw pile.
  std::vector<int>    trick;      // 0..2 cards on the table.
  int                 trick_leader      = 0;
  int                 current_player    = 0;
  int                 last_trick_winner = -1;  // for the +1 ultima bonus.
  bool                game_over         = false;
  // True after both cards have been played but before the trick is moved to
  // the winner's pile and the next draw happens. Lets the UI hold both cards
  // on the table until the player clicks to advance.
  bool                  pending_trick_resolve = false;
  std::function<void()> on_cards_changed;

  Game_State() : on_cards_changed([] {}) {}

  bool                  is_game_over() const override { return game_over; }
  std::optional<Choice> next_choice() override;

  void switch_turn() { current_player = 1 - current_player; }

  // Fire on_cards_changed if set (used by the UI to refresh stack contents).
  void notify_cards_changed() {
    if (on_cards_changed) on_cards_changed();
  }
};

}  // namespace tressette
