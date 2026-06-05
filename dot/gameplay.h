#pragma once

#include <dot/models.h>

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

}  // namespace dot
