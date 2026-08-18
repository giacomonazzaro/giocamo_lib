#pragma once

#include <game/game.h>
#include <struct/visit.hpp>

#include <string>
#include <vector>

namespace scopa {

// Carte napoletane suits. The DENARI suit is the one that counts for the
// "denari" and "settebello" points.
enum class Suit {
  COPPE,
  DENARI,
  SPADE,
  BASTONI,
};

// Static card identity. 40 cards = 4 suits x 10 ranks.
// Rank meaning:
//   1 = Asso, 2..7 = number cards, 8 = Fante, 9 = Cavallo, 10 = Re.
// Capture value equals the rank itself (1..10).
struct Card {
  int  id   = 0;  // 0..39, index into Game_State.all_cards.
  int  rank = 1;  // 1..10.
  Suit suit = Suit::COPPE;
};

// Primiera value for a single rank. The Primiera is a per-suit "best card"
// contest where the highest-value card per suit contributes these points,
// summed across all four suits. The 7 is the most valuable, then 6, then Ace.
inline int primiera_value(int rank) {
  switch (rank) {
    case 7: return 21;
    case 6: return 18;
    case 1: return 16;  // Asso.
    case 5: return 15;
    case 4: return 14;
    case 3: return 13;
    case 2: return 12;
    case 8:
    case 9:
    case 10: return 10;  // Face cards.
  }
  return 0;
}

// One in-game action: play a card from hand and optionally capture a subset
// of the table cards. If captured_card_ids is empty the played card just
// stays face-up on the table.
struct Action {
  int              played_card_id = -1;
  std::vector<int> captured_card_ids;
};

struct Player {
  std::string      name;
  std::vector<int> hand;
  std::vector<int> captured;  // All cards taken in captures by this player.
  int              scope = 0;  // Number of sweeps (clears) made this round.
};

// Full Scopa Scientifica game state. The deck is fully dealt at setup: 4
// cards face-up on the table and 9 to each player; the remaining 18 cards
// sit in `stock` and are dealt out as a fresh hand once both players empty
// their first hand (so no card is ever drawn blindly from the deck).
struct Game_State : Game {
  std::vector<Card>   all_cards;  // 40 fixed cards.
  std::vector<Player> players;    // exactly 2.
  std::vector<int>    table;      // Face-up cards available for capture.
  std::vector<int>    stock;      // Cards waiting for the mid-game re-deal.
  int                 current_player    = 0;
  int                 last_capturer     = -1;  // For the end-of-round sweep.
  bool                game_over         = false;

  Game_State() {}

  bool   is_game_over() const override { return game_over; }
  Choice next_choice() override;
  void   init(int seed = 0) override;

  void switch_turn() { current_player = 1 - current_player; }
};

}  // namespace scopa

VISITABLE_STRUCT(scopa::Card, id, rank, suit);
VISITABLE_STRUCT(scopa::Player, name, hand, captured, scope);
VISITABLE_STRUCT(
  scopa::Game_State,
  all_cards,
  players,
  table,
  stock,
  current_player,
  last_capturer,
  game_over
);
