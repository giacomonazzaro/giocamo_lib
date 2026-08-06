#pragma once

#include <basic/array_inline.h>
#include <game/game.h>

#include <memory>
#include <string>
#include <vector>

#include "../struct/print.h"

// Card kind (matches Python Card_Type enum string values).
enum class Card_Type {
  WONDER,
  EVENT,
  PEOPLE,
};

// Card color (matches Python Card_Color enum string values).
enum class Card_Color {
  GREEN,
  BLUE,
  RED,
  YELLOW,
};

// Forward declarations.
struct Card;
struct Game_State;
struct Card_Design;

// Stable reference to a card in some zone of the game.
// Mirrors gods/models.py:Card_Id (slots=True, frozen=True).
// area is one of: "deck", "hand", "discard", "wonders", "people", "none".
struct Card_Id {
  std::string area;
  int         card_index  = -1;
  int         owner_index = -1;

  static Card_Id null() { return Card_Id{"none", -1, -1}; }
  static bool    is_null(const Card_Id& c) {
    return c.area == "none" && c.card_index == -1 && c.owner_index == -1;
  }

  bool operator==(const Card_Id& o) const {
    return area == o.area && card_index == o.card_index &&
           owner_index == o.owner_index;
  }
  bool operator!=(const Card_Id& o) const { return !(*this == o); }
};

// Pack/unpack Card_Id into the std::vector<int> targets used by game's
// Choose_Card / Choose_Cards.
//
// Bit layout in int32:
//   bits  0..23 : card_index (16M cards)
//   bits 24..27 : area enum  (4 bits)
//   bits 28..31 : owner+1    (so -1 => 0, 0 => 1, 1 => 2, fits 4 bits)
//
// Area encoding mirrors Card_Id.area string values.
namespace area_code {
constexpr int NONE    = 0;
constexpr int DECK    = 1;
constexpr int HAND    = 2;
constexpr int DISCARD = 3;
constexpr int WONDERS = 4;
constexpr int PEOPLE  = 5;
}  // namespace area_code

int     pack_card_id(const Card_Id& cid);
Card_Id unpack_card_id(int packed);

// Runtime card state — the only fields that change during a game.
// Trivially copyable; value-copied during minimax search.
// Mirrors gods/models.py:Card.
struct Card {
  int        id = 0;  // index into Game_State.all_cards and card_designs.
  Card_Type  card_type = Card_Type::EVENT;
  Card_Color color     = Card_Color::RED;
  int        power     = 0;
  int        counters  = 0;
  bool       destroyed = false;
  int        owner     = -1;

  // Hooks delegate to card_designs[id] — defined in cards.cpp to avoid a
  // circular include with cards.h.
  std::vector<Choice> on_draw(Game_State& g);
  std::vector<Choice> on_draw_replacement(Game_State& g);
  std::vector<Choice> on_played(Game_State& g);
  std::vector<Choice> on_game_end(Game_State& g);
  void                on_destroyed(Game_State& g);
  void                on_play(Game_State& g, Card& card_played);
  void                on_destroy(Game_State& g, Card& card_destroyed);
  void                on_restore(Game_State& g, Card& card_restored);
  std::vector<Choice> on_discard(Game_State& g, Card& card_discarded);
  std::vector<Choice> on_pass(Game_State& g);
  std::vector<Choice> on_turn_end(Game_State& g);
  std::vector<Choice> on_turn_start(Game_State& g);
  int                 power_modifier(Game_State& g, const Card& c, int p);
  bool                is_indestructible(Game_State& g, const Card& c);
  int                 can_be_claimed(Game_State& g, int player_index);
  int                 on_scoring(Game_State& g);
  int  on_scoring_people(Game_State& g, const Card& people, int points);
  bool wins_tie(Game_State& g, const Card& people);
};
VISITABLE_STRUCT(Card, id, card_type, color, power, counters, destroyed, owner);

// Player state — owns lists of card ids referencing Game_State.all_cards.
struct Player {
  std::string           name;
  Array_Inline<int, 32> deck;     // Face-down draw pile.
  Array_Inline<int, 12> hand;     // Cards in hand.
  Array_Inline<int, 16> discard;  // Played/discarded cards.
  Array_Inline<int, 8>  wonders;  // Wonders in play.
};
VISITABLE_STRUCT(Player, name, deck, hand, discard, wonders);

enum class Game_Phase {
  START,
  MAIN,
  POST_PLAY,
  POST_PASS_EFFECTS,
  POST_PASS_DRAW,
  CLAIM,
  END,
};

struct Game_State : Game {
  Array_Inline<Card, 40> all_cards;    // The full deck; fixed at setup.
  std::vector<Player>    players;      // Exactly 2 (kept a vector for JSON).
  Array_Inline<int, 12>  peoples;      // People in play.
  Array_Inline<int, 32>  shared_deck;  // Shared draw pile (Stars).

  int        current_player = 0;
  Game_Phase current_phase  = Game_Phase::MAIN;
  bool       game_over      = false;

  // FIFO of pending choices produced by the phase machine and card effects.
  // Kept separate from the base `choices`, which the game loop manages.
  std::vector<Choice> queue;

  Game_State() = default;

  // Game interface.
  bool   is_game_over() const override { return game_over; }
  Choice next_choice() override;

  // Helpers (mirror gods/models.py methods).
  Player&              active_player() { return players[current_player]; }
  Player&              opponent() { return players[1 - current_player]; }
  std::vector<Card_Id> peoples_ids() const;
  std::vector<Card_Id> wonders_of(int player_index) const;
  std::vector<Card_Id> discard_of(int player_index) const;
  std::vector<Card_Id> hand_of(int player_index) const;
  void                 switch_turn() { current_player = 1 - current_player; }
  Card& get_card(const Card_Id& cid) { return all_cards[cid.card_index]; }
  // player_id == -1 means "both players".
  std::vector<Card_Id> card_list(int player_id, const std::string& area) const;
  int                  effective_power(int card_id) const;
  int owner(int card_id) const { return all_cards[card_id].owner; }
};
VISITABLE_STRUCT(
  Game_State,
  all_cards,
  players,
  peoples,
  shared_deck,
  current_player,
  current_phase,
  game_over
);

// ---- Card_Design base class ----
// All cards derive from this; subclasses override the hooks they implement.
// Mirrors gods/models.py:Card_Design.
struct Card_Design {
  int         id = 0;
  std::string name;
  Card_Type   card_type = Card_Type::EVENT;
  Card_Color  color     = Card_Color::RED;
  std::string effect;

  virtual ~Card_Design() = default;

  // Defaults match the no-op base methods in Python.
  virtual std::vector<Choice> on_draw(Game_State&) { return {}; }
  virtual std::vector<Choice> on_draw_replacement(Game_State&) { return {}; }
  virtual std::vector<Choice> on_played(Game_State&) { return {}; }
  virtual std::vector<Choice> on_game_end(Game_State&) { return {}; }
  virtual void                on_destroyed(Game_State&) {}
  virtual void                on_play(Game_State&, Card&) {}
  virtual void                on_destroy(Game_State&, Card&) {}
  virtual void                on_restore(Game_State&, Card&) {}
  virtual std::vector<Choice> on_discard(Game_State&, Card&) { return {}; }
  virtual std::vector<Choice> on_pass(Game_State&) { return {}; }
  virtual std::vector<Choice> on_turn_end(Game_State&) { return {}; }
  virtual std::vector<Choice> on_turn_start(Game_State&) { return {}; }
  virtual int  power_modifier(Game_State&, const Card&, int p) { return p; }
  virtual bool is_indestructible(Game_State&, const Card&) { return false; }
  virtual int  can_be_claimed(Game_State&, int) { return 0; }
  virtual int  on_scoring(Game_State&) { return 0; }
  virtual int  on_scoring_people(Game_State&, const Card&, int points) {
    return points;
  }
  virtual bool wins_tie(Game_State&, const Card&) { return false; }
};
VISITABLE_STRUCT(Card_Design, id, name, card_type, color, effect);

// Global registry of card designs — populated once by setup, read by Card
// hooks. Not deep-copied (designs are stateless); Cards just look up by id.
extern std::vector<std::unique_ptr<Card_Design>> card_designs;
