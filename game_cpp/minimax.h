#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

#include "game.h"

// Score the state from the perspective of player_index.
// Higher = better for that player. Convention: >= 900 means a forced win.
// Templated on the concrete Game subclass so search can copy by value.
template <class Game_T>
using Evaluate_Fn = std::function<float(Game_T&, int)>;

namespace minimax_detail {

template <class Game_T>
float minimax(
  Game_T&                    state,
  const Evaluate_Fn<Game_T>& evaluate,
  int                        depth,
  float                      alpha,
  float                      beta,
  int                        player_index
) {
  if (state.is_game_over()) return evaluate(state, player_index);
  if (depth == 0) return evaluate(state, player_index);

  std::optional<Choice> choice = state.next_choice();
  if (!choice) return evaluate(state, player_index);

  const int num_actions = action_options_count(choice->actions(state));
  if (num_actions == 0) return evaluate(state, player_index);

  const bool  maximizing = choice->player_index == player_index;
  const float inf        = std::numeric_limits<float>::infinity();
  float       value      = maximizing ? -inf : inf;

  for (int action_index = 0; action_index < num_actions; ++action_index) {
    Game_T new_state = state;
    resolve_choice(new_state, *choice, action_index);
    const float score = minimax(new_state, evaluate, depth - 1, alpha, beta, player_index);
    if (maximizing) {
      value = std::max(value, score);
      alpha = std::max(alpha, value);
    } else {
      value = std::min(value, score);
      beta  = std::min(beta, value);
    }
    if (alpha >= beta) break;
  }
  return value;
}

}  // namespace minimax_detail

// Plain alpha-beta search at the root. Each root action is searched with a full
// [-inf, +inf] window so the returned scores are exact (not lower/upper bounds),
// which matters when several actions tie and we want to pick the best.
template <class Game_T>
std::vector<float> minimax_search(
  Game_T&                    state,
  const Evaluate_Fn<Game_T>& evaluate,
  const Choice&              choice,
  int                        num_actions,
  int                        player_index,
  int                        max_depth
) {
  using minimax_detail::minimax;
  const float        inf = std::numeric_limits<float>::infinity();
  std::vector<float> scores(num_actions, -inf);

  for (int action_index = 0; action_index < num_actions; ++action_index) {
    Game_T new_state = state;
    resolve_choice(new_state, choice, action_index);
    scores[action_index] =
      minimax(new_state, evaluate, max_depth, -inf, inf, player_index);
  }
  return scores;
}
