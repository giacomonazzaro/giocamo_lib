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

// Put a creature into play under `controller` and trigger its Play ability.
// `owner` is the player whose discard pile it returns to. Returns its index.
int enter_play(Game_State& state, int design, int owner, int controller);

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

Choice make_multi_choice(
  int                                                      player,
  const char*                                              description,
  std::function<std::vector<int>(Game_State&)>             get_targets,
  int                                                      count,
  bool                                                     up_to,
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
  if (state.game_over) return state.winner == player ? 1000.0f : -1000.0f;

  float score =
    10.0f * (float)(state.players[player].life - state.players[opponent].life);
  score += 2.0f * (float)(state.players[player].mindbugs -
                          state.players[opponent].mindbugs);
  for (int i = 0; i < state.creatures.size(); ++i) {
    if (!state.creatures[i].alive) continue;
    const float power = (float)effective_power(state, i);
    score += state.creatures[i].controller == player ? power : -power;
  }
  score += (float)(state.players[player].hand.size() +
                   state.players[player].draw_pile.size());
  score -= (float)(state.players[opponent].hand.size() +
                   state.players[opponent].draw_pile.size());
  return score;
}

// One position `player` cannot tell apart from `state`: what they cannot see is
// the opponent's hand and the order of both draw piles. Pool the opponent's
// hand with their draw pile, reshuffle, and deal it back the same way.
inline Game_State sample_state(
  const Game_State& concrete, int player, std::mt19937& rng
) {
  Game_State sampled = concrete;
  Player&    them    = sampled.players[1 - player];

  Array_Inline<int, 24> hidden;
  hidden.append(them.hand.begin(), them.hand.end());
  hidden.append(them.draw_pile.begin(), them.draw_pile.end());
  std::shuffle(hidden.begin(), hidden.end(), rng);

  const int hand_size = them.hand.size();
  them.hand.assign(hidden.begin(), hidden.begin() + hand_size);
  them.draw_pile.assign(hidden.begin() + hand_size, hidden.end());

  Player& us = sampled.players[player];
  std::shuffle(us.draw_pile.begin(), us.draw_pile.end(), rng);
  return sampled;
}

}  // namespace mindbug
