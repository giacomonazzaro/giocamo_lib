#pragma once

#include <game/game.h>
#include <game/inlined_vector.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace tressette {

// Tressette strength order, from highest to lowest:
//   3 > 2 > 1(Asso) > 10(Re) > 9(Cavallo) > 8(Donna) > 7 > 6 > 5 > 4.
// Returns 0..9 (higher = stronger).
inline int strength(int rank) {
  // Tressette ordering: 3 > 2 > 1 > 10 > 9 > 8 > 7 > 6 > 5 > 4.
  switch (rank) {
    case 3: return 9;
    case 2: return 8;
    case 1: return 7;
    case 10: return 6;
    case 9: return 5;
    case 8: return 4;
    case 7: return 3;
    case 6: return 2;
    case 5: return 1;
    case 4: return 0;
  }
  return 0;
}

// Card value in thirds-of-a-point (so the score function can sum integers
// then floor-divide by 3 at the end):
//   Asso        -> 3 thirds (= 1 point)
//   2,3,8,9,10  -> 1 third  (= 1/3 point)
//   4,5,6,7     -> 0 thirds.
inline int card_thirds(int rank) {
  if (rank == 1) return 3;
  if (rank == 2 || rank == 3 || rank == 8 || rank == 9 || rank == 10) return 1;
  return 0;
}

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

// Player state: hand of card ids and the cards won in tricks. Both card lists
// live inline so copying a player (per MCTS node) does no heap allocation.
struct Player {
  std::string             name;
  Inlined_Vector<int, 10> hand;        // Up to 10 cards held.
  Inlined_Vector<int, 40> tricks_won;  // Up to 40 cards if one player wins all.
};

// Full Tressette game state. Subclasses game's abstract Game so the
// templated minimax / game_loop work directly on it.
struct Game_State : Game {
  std::array<Player, 2>   players;  // Exactly 2.
  Inlined_Vector<int, 20> stock;    // Face-down draw pile (up to 20).
  Inlined_Vector<int, 2>  trick;    // 0..2 cards on the table.
  int                     trick_leader      = 0;
  int                 current_player    = 0;
  int                 last_trick_winner = -1;  // for the +1 ultima bonus.
  bool                game_over         = false;
  int                 human_player      = -1;

  Game_State() {}

  bool   is_game_over() const override { return game_over; }
  Choice next_choice();

  void switch_turn() { current_player = 1 - current_player; }
};

// The 40 fixed cards. Set once at setup and never modified during play, so they
// live outside Game_State: copying a state (as MCTS does per node) shouldn't
// copy the whole deck. Cards are looked up by id, which indexes into this.
extern std::vector<Card> all_cards;

}  // namespace tressette
