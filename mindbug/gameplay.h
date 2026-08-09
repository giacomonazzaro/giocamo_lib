#pragma once

#include <mindbug/models.h>

#include <algorithm>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace mindbug {

// Fill card_designs from a cards.json file. Must be called once before any
// game is set up. Returns false if the file is missing or malformed.
bool load_card_designs(const std::string& path = "mindbug/cards.json");

// Deal a game: shuffle the 48-card deck, give each player 5 cards in hand and
// 5 face down, and ask for the first decision.
Game_State quick_setup(int seed);

// The whole 48-card deck as a list of designs, each repeated as many times as
// the card is printed. Setup deals out of it, and a search samples the cards it
// cannot see out of what is left of it.
inline Array_Inline<int, 48> full_deck_designs() {
  auto deck = Array_Inline<int, 48>();
  for (int design = 0; design < (int)card_designs.size(); ++design) {
    for (int copy = 0; copy < card_designs[design].copies; ++copy) {
      deck.push_back(design);
    }
  }
  return deck;
}

// Power after the auras and self conditions that change it.
int effective_power(const Game_State& state, int creature_index);

// Keywords the creature has right now, printed ones plus granted ones.
int effective_keywords(const Game_State& state, int creature_index);

// Creatures a player controls, in play order.
std::vector<int> creatures_of(const Game_State& state, int player);

// True if `blocker` is allowed to block `attacker`.
bool can_block(const Game_State& state, int attacker, int blocker);

// The actions the active player may take. The pending choice offers these in
// order, so an action index means the same thing here and in the UI.
std::vector<Turn_Action> turn_actions(const Game_State& state);

// Take a creature out of play: exhaust it instead if Tough has not been used
// yet, otherwise discard it and trigger its Defeated ability.
void defeat_creature(Game_State& state, int creature_index);

// 1 for the winner, 0 otherwise. Feeds the game-over score line.
int compute_player_score(const Game_State& state, int player);

// ---- Mechanics the card effects are written with ----

// Put a card into play as a creature under `controller` and trigger its Play
// ability. `owner` is the player whose discard pile it returns to when it is
// defeated. Returns the creature's index.
int enter_play(Game_State& state, int card, int owner, int controller);

void lose_life(Game_State& state, int player, int amount);

// Alive creatures a player controls (-1 for either player) whose power is
// between min_power and max_power.
std::vector<int> creature_targets(
  const Game_State& state, int controller, int min_power, int max_power
);

// ---- Choice helpers ----
//
// get_targets runs again when the choice resolves, so an option index always
// means the same target. Targets are creature indices, hand positions or
// discard positions, depending on the effect; `description` says which.

Choice make_choice(
  int                                          player,
  const char*                                  description,
  std::function<std::vector<int>(Game_State&)> get_targets,
  std::function<void(Game_State&, int)>        on_chosen
);

// Every selection of `count` targets (or of up to `count`, if up_to), in the
// order a multi-choice indexes them: option i picks combination i.
std::vector<std::vector<int>> target_combinations(
  const std::vector<int>& targets, int count, bool up_to
);

Choice make_multi_choice(
  int                                                       player,
  const char*                                               description,
  std::function<std::vector<int>(Game_State&)>              get_targets,
  int                                                       count,
  bool                                                      up_to,
  std::function<void(Game_State&, const std::vector<int>&)> on_chosen
);

// The int a turn action is carried as in the pending choice: the hand position
// or creature index, with bit 8 set for an attack.
inline int pack_turn_action(const Turn_Action& action) {
  return action.index | (action.is_attack ? 256 : 0);
}

// Rates the position for `player`. A win beats every unfinished position and a
// loss loses to every unfinished position. Life is what the game is about;
// board power and cards left break the ties. (inline: the search templates need
// to see it, without a duplicate definition across translation units.)
inline float evaluate_state(const Game_State& state, int player) {
  const int opponent = 1 - player;
  if (state.game_over) return state.winner == player ? 2.0f : 0.0f;

  // float life     = state.players[player].life;
  // float life_opp = state.players[opponent].life;
  float mindbugs = std::min(
    state.players[player].mindbugs, state.players[opponent].hand.size()
  );
  float mindbugs_opp = std::min(
    state.players[opponent].mindbugs, state.players[player].hand.size()
  );

  // for (int i = 0; i < state.creatures.size(); ++i) {
  //   if (!state.creatures[i].alive) continue;
  //   const float power = (float)effective_power(state,
  //   i); score += state.creatures[i].controller == player
  //   ? power : -power;
  // }
  float cards_left = state.players[player].hand.size() +
                     state.players[player].draw_pile.size();

  float cards_left_opp = state.players[opponent].hand.size() +
                         state.players[opponent].draw_pile.size();

  for (size_t i = 0; i < state.creatures.size(); i++) {
    state.creatures[i].controller == player ? cards_left += 1
                                            : cards_left_opp += 1;
  }
  if (cards_left == 0 && state.current_player == player) return 0.0f;
  if (cards_left_opp == 0 && state.current_player == opponent) return 2.0f;

  float score = cards_left + 2 * mindbugs;
  float den   = (cards_left + cards_left_opp) + 2 * (mindbugs + mindbugs_opp);
  if (den > 0.0f)
    score /= den;
  else
    score = 0.5f;

  assert(score >= 0.0f && score <= 1.0f);
  return score;
}

// One position `player` cannot tell apart from `state`.
//
// A player has seen their own hand, both discard piles and everything in play.
// Every other card is the same unknown to them: the opponent's hand, both draw
// piles and the 28 cards the deal set aside are one pool, and a card in a
// hidden zone could be any card of the deck that has not been shown. So take
// the deck, drop what has been seen, shuffle the rest, and deal the hidden
// cards again out of it. The zones keep their sizes; only what the cards are
// changes.
inline Game_State sample_state(
  const Game_State& concrete, int player, std::mt19937& rng
) {
  auto sampled = Game_State(concrete);

  auto is_hidden = std::vector<bool>(sampled.all_cards.size(), false);
  for (int card : sampled.players[player].draw_pile) is_hidden[card] = true;
  for (int card : sampled.players[1 - player].hand) is_hidden[card] = true;
  for (int card : sampled.players[1 - player].draw_pile) is_hidden[card] = true;

  auto unseen = full_deck_designs();
  for (int card = 0; card < (int)is_hidden.size(); ++card) {
    if (is_hidden[card]) continue;
    auto shown =
      std::find(unseen.begin(), unseen.end(), design_of(sampled, card));
    if (shown != unseen.end()) unseen.erase(shown);
  }
  std::shuffle(unseen.begin(), unseen.end(), rng);

  int next = 0;
  for (int card = 0; card < (int)is_hidden.size(); ++card) {
    if (is_hidden[card]) sampled.all_cards[card] = unseen[next++];
  }
  return sampled;
}

}  // namespace mindbug
