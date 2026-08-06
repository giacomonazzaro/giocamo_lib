#pragma once

#include <game/game.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "models.h"

// Put the choices an effect produced on the game's queue, and return no_choice.
// A resolve ends with this when a card effect asks further questions: it cannot
// say which choice comes next, so resolve_choice takes the first queued one
// through next_choice().
Choice queue_follow_ups(Game_State& game, std::vector<Choice> follow_ups);

// ---- Choice helpers ----
//
// In game, Choose_Card / Choose_Cards carry std::vector<int> targets.
// We use those slots to carry packed Card_Ids (see pack_card_id in models.h).

// Build a "choose one card" Choice with packed Card_Id targets.
//
// get_targets : called every time the choice is presented to enumerate options.
// on_chosen   : called after the player picks; null Card_Id means "pass".
Choice make_choose_card_choice(
  int                                                      player_index,
  std::function<std::vector<Card_Id>(Game_State&)>         get_targets,
  std::function<std::vector<Choice>(Game_State&, Card_Id)> on_chosen,
  const char*                                              text_description = ""
);

// Build a "choose multiple cards" Choice. up_to=true allows fewer than count.
Choice make_choose_cards_choice(
  int                                              player_index,
  std::function<std::vector<Card_Id>(Game_State&)> get_targets,
  std::function<int(Game_State&)>                  get_count,
  bool                                             up_to,
  std::function<std::vector<Choice>(Game_State&, std::vector<Card_Id>)>
              on_chosen,
  const char* text_description = ""
);

// Enumerate all combinations of card_ids of size <= num_cards (or == num_cards
// if up_to is false). Mirrors Python all_combinations in cards.py.
std::vector<std::vector<Card_Id>> all_combinations(
  const std::vector<Card_Id>& card_ids, int num_cards, bool up_to
);

// True if metric(game, player_index) > metric(game, opponent).
bool beats_opponent(
  Game_State&                                 game,
  int                                         player_index,
  const std::function<int(Game_State&, int)>& metric
);

// Filter helpers. f returns true to include the card. include_null appends
// Card_Id::null() so the player can opt out (used for "play a card or pass").
std::vector<Card_Id> card_selection(
  Game_State&                             state,
  int                                     player_id,
  const std::string&                      area,
  const std::function<bool(const Card&)>& f = [](const Card&) { return true; },
  bool                                    include_null = false
);
std::vector<Card_Id> people_selection(
  Game_State&                             game,
  const std::function<bool(const Card&)>& f = [](const Card&) { return true; },
  bool                                    include_null = false
);
std::vector<Card_Id> wonders_selection(
  Game_State&                             game,
  const std::function<bool(const Card&)>& f = [](const Card&) { return true; }
);

// ---- Game mechanics ----

// Draw a card from the player's deck. Returns choices produced by draw effects.
// replacement_effects=false skips on_draw_replacement (used by Stars to avoid
// recursion).
std::vector<Choice> draw_card(
  Game_State& game, int player_id, bool replacement_effects = true
);

// Discard cards from a single player's hand.
std::vector<Choice> discard_cards(
  Game_State& game, const std::vector<Card_Id>& card_ids
);

// Iteration order used by on_play, on_destroy, etc — active player first.
std::vector<int> wonders_by_priority(Game_State& game);

// Play a card from a player's hand (or shared deck for Stars). Returns choices
// from on_played and any wonder triggers.
std::vector<Choice> play_card(Game_State& game, const Card_Id& card_id);

void destroy_people(Game_State& game, const Card_Id& card_id);
void destroy_wonder(Game_State& game, const Card_Id& card_id);
void restore_people(Game_State& game, const Card_Id& card_id);
void shuffle_card_into_deck(Game_State& game, const Card_Id& card_id);

// Ask the active player to claim one opponent people card. Returns nullopt if
// nothing is claimable (no Choice should be enqueued).
std::optional<Choice> make_claim_choice(Game_State& game);

// Sum of points for player_index given current state.
int compute_player_score(Game_State& game, int player_index);

// Build the main "play a card or pass" choice for the active player.
Choice make_main_choice(Game_State& game);
