#pragma once

#include <basic/array_inline.h>
#include <game/game.h>

#include <array>
#include <optional>
#include <vector>

namespace dot {

// A D.O.T card. Every card shows some blue, black and red dots.
// Regular draw cards have a random 0..3 of each color. A star card has
// exactly 4 dots of a single color (the other two colors are 0).
struct Card {
  int  id         = 0;  // Index into Game_State.all_cards.
  int  blue_dots  = 0;  // 0..3, or 4 on the blue star card.
  int  black_dots = 0;  // 0..3, or 4 on the black star card.
  int  red_dots   = 0;  // 0..3, or 4 on the red star card.
  bool is_star    = false;
};

// One player. Card collections hold ids into Game_State.all_cards.
struct Player {
  Array_Inline<int, 16> draw_deck;  // Up to 15 face-down draw cards.
  Array_Inline<int, 4>  star_deck;  // Up to 3 star cards (viewable by owner).
  Array_Inline<int, 8>  hand;   // 6 cards held this round (5 draw + 1 star).
  Array_Inline<int, 12> pool;   // Personal pool; carries over between rounds.
  int revealed_pool_count = 0;  // Pool cards from earlier rounds
                                // (already shown); the rest stay
                                // hidden until both players commit.
  int tokens_blue  = 0;         // Scoring tokens won, by color.
  int tokens_black = 0;
  int tokens_red   = 0;
};

// The phases a player makes a decision in. The star card is drawn
// automatically into the hand.
//   SPLIT       - secretly pick which 3 of the 6 hand cards go to the shared
//                 pool.
//   ACKNOWLEDGE - both players have committed; pause so the player can see the
//                 revealed shared pool before it is scored.
//   DISCARD     - remove cards from the opponent's pool (end of Rounds 1 & 2).
enum class Phase { SPLIT, ACKNOWLEDGE, DISCARD };

// Full D.O.T game state. Played over 3 rounds; each round both players draw,
// secretly split 6 cards into their own pool (3) and the shared pool (3), then
// tokens are awarded on the dot-count difference, and opponents' pools are
// thinned in the discard phase.
struct Game_State : Game {
  std::array<Player, 2> players;     // Exactly 2.
  Array_Inline<int, 8> shared_pool;  // Cards played to the shared pool (max 6).

  // Tokens of each color currently up for grabs (1 per round, plus any that
  // carried over from a tied color in an earlier round).
  int pending_blue  = 0;
  int pending_black = 0;
  int pending_red   = 0;

  int   round         = 0;  // 0..2 (Rounds 1..3 on the rules sheet).
  Phase phase         = Phase::SPLIT;
  int   acting_player = 0;   // Whose decision the next choice is.
  int   discard_first = 0;   // Who discards first this round.
  int   human_player  = -1;  // Seat that owns the acknowledge pause.
  bool  game_over     = false;

  bool   is_game_over() const override { return game_over; }
  Choice next_choice() override;
};

// The deck of cards. Fixed at setup and never modified during play, so it lives
// outside Game_State: copying a state (as MCTS does per node) shouldn't copy
// the whole deck. Cards are looked up by id, which indexes into this.
extern std::vector<Card> all_cards;

}  // namespace dot
