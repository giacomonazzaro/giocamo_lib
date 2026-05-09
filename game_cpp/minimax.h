#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

#include "agent.h"
#include "game.h"

// Score the state from the perspective of player_index.
// Higher = better for that player. Convention: >= 900 means a forced win.
// Templated on the concrete Game subclass so search can copy by value.
template <class Game_T>
using Evaluate_Fn = std::function<float(Game_T&, int)>;

template <typename T>
inline size_t argmax(const std::vector<T>& v) {
  return static_cast<size_t>(
    std::distance(v.begin(), std::max_element(v.begin(), v.end()))
  );
}

namespace minimax_detail {

template <class Game_T>
float minimax(
  Game_T& state,
  // const Evaluate_Fn<Game_T>& evaluate,
  int   depth,
  float alpha,
  float beta,
  int   player_index
) {
  if (state.is_game_over()) return evaluate_state(state, player_index);
  if (depth == 0) return evaluate_state(state, player_index);

  std::optional<Choice> choice = state.next_choice();
  if (!choice) return evaluate_state(state, player_index);

  const int num_actions = action_options_count(choice->actions(state));
  if (num_actions == 0) return evaluate_state(state, player_index);

  const bool  maximizing = choice->player_index == player_index;
  const float inf        = std::numeric_limits<float>::infinity();
  float       value      = maximizing ? -inf : inf;

  for (int action_index = 0; action_index < num_actions; ++action_index) {
    Game_T new_state = state;
    resolve_choice(new_state, *choice, action_index);
    const float score =
      minimax(new_state, depth - 1, alpha, beta, player_index);
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
// [-inf, +inf] window so the returned scores are exact (not lower/upper
// bounds), which matters when several actions tie and we want to pick the best.
template <class Game_T>
std::vector<float> minimax_search(
  Game_T& state,
  // const Evaluate_Fn<Game_T>& evaluate,
  const Choice& choice,
  int           num_actions,
  int           player_index,
  int           max_depth
) {
  using minimax_detail::minimax;
  const float        inf = std::numeric_limits<float>::infinity();
  std::vector<float> scores(num_actions, -inf);

  for (int action_index = 0; action_index < num_actions; ++action_index) {
    Game_T new_state = state;
    resolve_choice(new_state, choice, action_index);
    scores[action_index] =
      minimax(new_state, max_depth, -inf, inf, player_index);
  }
  return scores;
}

// Alpha-beta minimax. Templated on the concrete Game subclass so the search can
// copy state by value (no clone() / unique_ptr needed).
template <class Game_T>
struct Agent_Minimax : Agent {
  // Evaluate_Fn<Game_T> evaluate;
  int max_depth;

  Agent_Minimax(int max_depth) : max_depth(max_depth) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete    = static_cast<Game_T&>(state);
    const int num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    std::vector<float> scores = minimax_search<Game_T>(
      concrete, choice, num_actions, choice.player_index, max_depth
    );
    return argmax(scores);
  }
};

template <class Game_T>
using Sample_State = std::function<Game_T(const Game_T&, int, std::mt19937&)>;

template <class Game_T>
struct Agent_Minimax_Stochastic : Agent_Minimax<Game_T> {
  int num_samples = 20;

  Agent_Minimax_Stochastic(int max_depth = 6, int num_samples = 20)
      : Agent_Minimax<Game_T>(max_depth), num_samples(num_samples) {}

  void message(const std::string&) override {}

  // Heuristic position score from player_index's perspective.
  // Mirrors evaluate_state + evaluate_heuristic in Python.
  // static float evaluate_state(Game_T& game, int player_index);

  int choose_action(Game& state, const Choice& choice) override {
    Game_T& concrete    = static_cast<Game_T&>(state);
    int     num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::vector<int>                 votes(num_actions, 0);
    std::vector<float>               total_scores(num_actions, 0.0f);

    for (int s = 0; s < num_samples; ++s) {
      Game_T sampled = sample_state(concrete, choice.player_index, rng);
      std::vector<float> scores = minimax_search<Game_T>(
        sampled, choice, num_actions, choice.player_index, this->max_depth
      );
      votes[argmax(scores)] += 1;
      for (int i = 0; i < num_actions; ++i) total_scores[i] += scores[i];
    }

    return argmax(votes);
    // return argmax(total_scores);
  }
};