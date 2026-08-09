#pragma once

#include <basic/array_inline.h>
#include <game/game.h>

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

// A creature that has been played. Defeated creatures keep their slot with
// alive=false so an index stays valid for as long as a game runs.
struct Creature {
  int  card       = 0;  // Index into Game_State.all_cards.
  int  owner      = 0;  // Whose discard pile it returns to when defeated.
  int  controller = 0;  // Who attacks and blocks with it; a Mindbug flips this.
  bool exhausted  = false;  // Tough has already saved it once.
  bool alive      = true;
};

// Card collections hold indices into Game_State.all_cards, so the two copies
// of a card stay apart — which is what lets the app give each one its own
// place on the table.
struct Player {
  Array_Inline<int, 24> hand;
  Array_Inline<int, 8>  draw_pile;  // Face-down, in draw order (back = top).
  Array_Inline<int, 24> discard;
  int                   life     = STARTING_LIFE;
  int                   mindbugs = STARTING_MINDBUGS;
};

// What the game does next once the pending effects are done.
enum class Phase {
  TURN,      // The active player plays a creature or attacks with one.
  MINDBUG,   // The opponent decides whether to steal the creature being played.
  ATTACK,    // The attacker's Attack ability triggers.
  BLOCK,     // A blocker is picked, by the defender or by a hunter's controller.
  COMBAT,    // The block (or lack of one) is resolved.
  TURN_END,  // Refill the hand, then pass the turn.
};

// A turn action: play hand[index], or attack with creatures[index].
struct Turn_Action {
  bool is_attack = false;
  int  index     = 0;
};

struct Game_State : Game {
  // The 20 cards dealt this game, each holding the design it shows. Fixed at
  // setup; every other list refers to a card by its index here.
  Array_Inline<int, 24>      all_cards;
  Player                     players[2];
  Array_Inline<Creature, 32> creatures;

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

  Player& active_player() { return players[current_player]; }
  Player& opponent() { return players[1 - current_player]; }
};

// The design a dealt card shows.
inline int design_of(const Game_State& state, int card) {
  return state.all_cards[card];
}

// The design of the creature in play at `creature_index`.
inline int creature_design(const Game_State& state, int creature_index) {
  return design_of(state, state.creatures[creature_index].card);
}

}  // namespace mindbug
