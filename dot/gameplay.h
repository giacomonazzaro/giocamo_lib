#pragma once

#include <dot/models.h>

#include <algorithm>
#include <random>
#include <vector>

namespace dot {

// How many cards each player plays into the shared pool every round.
constexpr int SHARED_COUNT = 3;

// Deal both decks and start Round 1. Both players get an identical deck
// (same seed) but shuffle their draw piles independently.
Game_State quick_setup(int seed);

// Total scoring tokens a player has won (blue + black + red).
int total_tokens(const Game_State& state, int player);

// Final score for the game-over screen: total tokens won.
int compute_player_score(const Game_State& state, int player);

// How many cards the acting player must remove from the opponent's pool this
// round: 1 at the end of Round 1, 2 at the end of Round 2.
int discard_count(const Game_State& state);

// The index-th k-combination of [0, n), in lexicographic order, as a sorted
// list of positions. Used to turn an action index into a concrete card
// selection; matches the ordering counted by action_options_count.
std::vector<int> combination_at(int n, int k, long long index);

// Inverse of combination_at: the lexicographic index of a sorted selection of
// positions out of [0, n). Used by the UI to turn dragged cards into an index.
long long combination_rank(int n, const std::vector<int>& positions);

// Heuristic value of a state for `player`: how many more tokens they hold
// than the opponent. (inline: defined in the header so the MCTS template can
// see it, without a duplicate definition across translation units.)
inline float evaluate_state(const Game_State& state, int player) {
  return (float)(compute_player_score(state, player) -
                 compute_player_score(state, 1 - player));
}

// Determinize the hidden information for an MCTS rollout from `player_index`'s
// view: their own hand, both pools, the shared pool and every star card are
// known, but which draw cards sit in the opponent's hand versus their
// face-down draw deck is not. Reshuffle those, and the searching player's own
// face-down draw deck, while leaving all face-up cards untouched.
inline Game_State sample_state(
  const Game_State& concrete, int player_index, std::mt19937& rng
) {
  Game_State sampled        = concrete;
  int        opponent_index = 1 - player_index;
  Player&    opponent       = sampled.players[opponent_index];

  // The opponent's hand keeps its (known) star cards; its draw cards are
  // pooled with the draw deck and re-dealt.
  std::vector<int> hand_stars;
  std::vector<int> hidden_draws;
  for (int id : opponent.hand) {
    if (sampled.all_cards[id].is_star) hand_stars.push_back(id);
    else hidden_draws.push_back(id);
  }
  int draws_in_hand = (int)hidden_draws.size();
  hidden_draws.insert(
    hidden_draws.end(), opponent.draw_deck.begin(), opponent.draw_deck.end()
  );
  std::shuffle(hidden_draws.begin(), hidden_draws.end(), rng);

  opponent.hand = hand_stars;
  opponent.hand.insert(
    opponent.hand.end(), hidden_draws.begin(), hidden_draws.begin() + draws_in_hand
  );
  opponent.draw_deck.assign(hidden_draws.begin() + draws_in_hand, hidden_draws.end());

  // The searching player doesn't know the order of their own face-down draw
  // deck either, so randomize it for the rollout.
  std::shuffle(
    sampled.players[player_index].draw_deck.begin(),
    sampled.players[player_index].draw_deck.end(),
    rng
  );
  return sampled;
}

}  // namespace dot
