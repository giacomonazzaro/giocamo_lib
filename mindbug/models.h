#pragma once

#include <basic/array_inline.h>
#include <game/game.h>
#include <struct/print.h>

#include <string>
#include <vector>

namespace mindbug {

// Mindbug: First Contact. Two players, 3 life points and 2 Mindbugs each.
// On your turn you either play a creature or attack with one.

constexpr int STARTING_LIFE     = 3;
constexpr int STARTING_MINDBUGS = 2;
constexpr int HAND_SIZE         = 5;
constexpr int DRAW_PILE_SIZE    = 5;

enum Keyword {
  SNEAKY    = 1 << 0,  // Can only be blocked by sneaky creatures.
  HUNTER    = 1 << 1,  // Its controller picks the blocker.
  FRENZY    = 1 << 2,  // Attacks a second time if it survives.
  POISONOUS = 1 << 3,  // Defeats any creature it fights.
  TOUGH     = 1 << 4,  // The first defeat only exhausts it.
};

// Card designs, in the order of cards.json, which is the numbering printed on
// the cards (1..32) minus one. Card effects are written against these names.
enum Design {
  AXOLOTL_HEALER,
  BEE_BEAR,
  BRAIN_FLY,
  CHAMELEON_SNIPER,
  COMPOST_DRAGON,
  DEATHWEAVER,
  ELEPHANTOPUS,
  EXPLOSIVE_TOAD,
  FERRET_BOMBER,
  GIRAFFODILE,
  GOBLIN_WEREWOLF,
  GORILLION,
  GRAVE_ROBBER,
  HARPY_MOTHER,
  KANGASAURUS_REX,
  KILLER_BEE,
  LONE_YETI,
  LUCHATAUR,
  MYSTERIOUS_MERMAID,
  PLATED_SCORPION,
  RHINO_TURTLE,
  SHARK_DOG,
  SHARKY_CRAB_DOG_MUMMYPUS,
  SHIELD_BUGS,
  SNAIL_HYDRA,
  SNAIL_THROWER,
  SPIDER_OWL,
  STRANGE_BARREL,
  TIGER_SQUIRREL,
  TURBO_BUG,
  TUSKED_EXTORTER,
  URCHIN_HURLER,
  DESIGN_COUNT,
};

// The printed card. Fixed at setup and never modified during play, so it lives
// outside Game_State: copying a state (as a search does per node) shouldn't
// copy the card list. Cards are looked up by design index.
struct Card_Design {
  std::string name;
  std::string text;   // Rules text below the keyword line, if any.
  std::string image;  // File name of the card art.
  int         power    = 0;
  int         keywords = 0;
  int         copies   = 1;  // How many of this card the 48-card deck holds.
};

extern std::vector<Card_Design> card_designs;

// Card collections hold indices into Game_State.all_cards, so the two copies
// of a card stay apart — which is what lets the app give each one its own
// place on the table. A card is in exactly one of them, and which player holds
// it in `creatures` is what "controls that creature" means.
struct Player {
  // Capacities are what a zone can hold at its fullest, not what it usually
  // holds: past it an Array_Inline goes to the heap, and a state is copied on
  // every node a search looks at. A hand refills to 5 but Giraffodile empties a
  // discard pile into it, and every card a player owns can end up discarded.
  Array_Inline<int, 16> hand;
  Array_Inline<int, 5>  draw_pile;  // Face-down, in draw order (back = top).
  Array_Inline<int, 12> creatures;  // In play, in the order they were played.
  Array_Inline<int, 20> discard;
  int                   life     = STARTING_LIFE;
  int                   mindbugs = STARTING_MINDBUGS;
};

// What the game does next once the pending effects are done.
enum class Phase {
  TURN,     // The active player plays a creature or attacks with one.
  MINDBUG,  // The opponent decides whether to steal the creature being played.
  ATTACK,   // The attacker's Attack ability triggers.
  BLOCK,    // A blocker is picked, by the defender or by a hunter's controller.
  COMBAT,   // The block (or lack of one) is resolved.
  FRENZY,   // A frenzy creature that survived may attack a second time.
  TURN_END,  // Refill the hand, then pass the turn.
};

// A turn action: play `card` from hand, or attack with it.
struct Turn_Action {
  bool is_attack = false;
  int  card      = 0;
};

struct Game_State : Game {
  // The 20 cards dealt this game, each holding the design it shows. Fixed at
  // setup; every other list refers to a card by its index here.
  Array_Inline<int, 24> all_cards;
  Player                players[2];
  // Creatures in play whose Tough keyword has already saved them once.
  Array_Inline<int, 8> exhausted_cards;

  int   current_player = 0;
  Phase phase          = Phase::TURN;
  bool  game_over      = false;
  int   winner         = -1;

  // Set while a creature is played, until the Mindbug decision resolves.
  int played_card = -1;
  // Set while an attack resolves.
  int attacker     = -1;
  int blocker      = -1;
  int attack_count = 0;  // Attacks this creature has made this turn (frenzy).
  // A hunter's controller gave the block decision back to the defender.
  bool hunter_declined = false;
  // The opponent spent a Mindbug, so the active player takes another turn.
  bool extra_turn = false;

  // Effects that still owe the players a decision, oldest first.
  std::vector<Choice> queue;

  // Only Strange Barrel needs randomness during play. Kept as a plain seed so
  // copying a state during search stays cheap.
  unsigned int random_seed = 1;

  bool   is_game_over() const override { return game_over; }
  Choice next_choice() override;
  void   init(int seed = 0) override;

  Player& active_player() { return players[current_player]; }
  Player& opponent() { return players[1 - current_player]; }
};

// The design a dealt card shows.
inline int design_of(const Game_State& state, int card) {
  return state.all_cards[card];
}

// The player who has `card` in play, or -1 if it is not in play.
inline int controller_of(const Game_State& state, int card) {
  for (int player = 0; player < 2; ++player) {
    for (int in_play : state.players[player].creatures) {
      if (in_play == card) return player;
    }
  }
  return -1;
}

inline bool is_in_play(const Game_State& state, int card) {
  return controller_of(state, card) != -1;
}

// True once Tough has saved this creature, so the next defeat takes it.
inline bool is_exhausted(const Game_State& state, int card) {
  for (int exhausted : state.exhausted_cards) {
    if (exhausted == card) return true;
  }
  return false;
}

// Cards a player still has: in hand, face down, and in play.
inline int cards_left(const Game_State& state, int player) {
  return state.players[player].hand.size() +
         state.players[player].draw_pile.size() +
         state.players[player].creatures.size();
}

}  // namespace mindbug

// visit_struct opens its own namespace, so these belong at global scope.
VISITABLE_STRUCT(
  mindbug::Player, hand, draw_pile, creatures, discard, life, mindbugs);

VISITABLE_STRUCT(
  mindbug::Turn_Action, is_attack, card);

VISITABLE_STRUCT(
  mindbug::Game_State,
  all_cards,
  players,
  exhausted_cards,
  current_player,
  phase,
  game_over,
  winner,
  played_card,
  attacker,
  blocker,
  attack_count,
  hunter_declined,
  extra_turn,
  random_seed
);

