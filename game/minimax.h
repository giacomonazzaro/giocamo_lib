#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <vector>

#include "agent.h"
#include "game.h"

#ifndef __EMSCRIPTEN__
#include <thread>
#endif

template <typename T>
inline size_t argmax(const std::vector<T>& v) {
  return static_cast<size_t>(
    std::distance(v.begin(), std::max_element(v.begin(), v.end()))
  );
}
template <typename T>
inline size_t argmax_randomized(const std::vector<T>& v) {
  float            max = *std::max_element(v.begin(), v.end());
  std::vector<int> argmaxes;
  for (int i = 0; i < v.size(); ++i) {
    if (v[i] == max) argmaxes.push_back(i);
  }
  if (argmaxes.size() == 1) return argmaxes[0];
  return argmaxes[rand() % argmaxes.size()];
}

namespace minimax_detail {

template <class Game_T>
float minimax(
  Game_T& state, int depth, float alpha, float beta, int player_index
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

using Clock    = std::chrono::steady_clock;
using Deadline = Clock::time_point;

// Alpha-beta like minimax() above, but abandoned as soon as `deadline` passes.
// On abort it sets `aborted` and unwinds with whatever value it has so far —
// the caller discards that incomplete search and keeps the previous depth's
// result. The clock is checked at every node; steady_clock::now() is cheap
// enough next to the per-node work.
template <class Game_T>
float minimax_timed(
  Game_T&         state,
  int             depth,
  float           alpha,
  float           beta,
  int             player_index,
  const Deadline& deadline,
  bool&           aborted
) {
  if (aborted) return 0.0f;
  if (Clock::now() >= deadline) {
    aborted = true;
    return 0.0f;
  }
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
    const float score = minimax_timed(
      new_state, depth - 1, alpha, beta, player_index, deadline, aborted
    );
    if (aborted) return value;  // Result is incomplete; the caller drops it.
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
std::vector<float> minimax_scores(
  Game_T&       state,
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
  int max_depth;

  Agent_Minimax(int max_depth) : max_depth(max_depth) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete    = static_cast<Game_T&>(state);
    const int num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    std::vector<float> scores = minimax_scores<Game_T>(
      concrete, choice, num_actions, choice.player_index, max_depth
    );
    return argmax_randomized(scores);
  }
};

// Iterative-deepening alpha-beta with a wall-clock budget. Searches depth
// 1, 2, 3, ... and plays the best root move from the deepest search that
// finished before the budget ran out. A search that overruns mid-depth is
// abandoned (its partial scores discarded), so the move time stays close to the
// budget regardless of how the position branches.
//
// Root-parallel: each depth's root moves are split across threads. Sibling root
// moves are already searched with a full [-inf, inf] window (no alpha sharing
// between them), so spreading them over cores costs no pruning — it just lets
// the agent reach a deeper completed depth in the same budget.
template <class Game_T>
struct Agent_Minimax_Timed : Agent {
  float time_budget_seconds;
  int   max_depth;    // Safety cap on the deepening loop.
  int   num_threads;  // 0 = one per hardware core. Always 1 under Emscripten.

  explicit Agent_Minimax_Timed(
    float time_budget_seconds, int max_depth = 64, int num_threads = 0
  )
      : time_budget_seconds(time_budget_seconds)
      , max_depth(max_depth)
      , num_threads(num_threads) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete    = static_cast<Game_T&>(state);
    const int num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;

    const float inf      = std::numeric_limits<float>::infinity();
    const auto  deadline = minimax_detail::Clock::now() +
                          std::chrono::duration_cast<minimax_detail::Clock::duration>(
                            std::chrono::duration<float>(time_budget_seconds)
                          );

    int best_action = 0;
    for (int depth = 1; depth <= max_depth; ++depth) {
      std::vector<float> scores(num_actions, -inf);
      // Keep the deepest fully completed depth; a partial one is unreliable.
      if (search_root(concrete, choice, depth, deadline, scores)) break;
      best_action = (int)argmax_randomized(scores);
      if (minimax_detail::Clock::now() >= deadline) break;
    }
    return best_action;
  }

  // Score every root action at `depth`, writing into `scores`. Returns true if
  // the budget ran out before the depth finished (scores are then incomplete
  // and must be dropped). Each action's score is written by exactly one thread,
  // so the disjoint writes need no synchronization.
  bool search_root(
    Game_T&                         concrete,
    const Choice&                   choice,
    int                             depth,
    const minimax_detail::Deadline& deadline,
    std::vector<float>&             scores
  ) {
    using minimax_detail::minimax_timed;
    const float inf         = std::numeric_limits<float>::infinity();
    const int   num_actions = (int)scores.size();
    const int   player      = choice.player_index;

#ifdef __EMSCRIPTEN__
    bool aborted = false;
    for (int action_index = 0; action_index < num_actions; ++action_index) {
      Game_T new_state = concrete;
      resolve_choice(new_state, choice, action_index);
      scores[action_index] =
        minimax_timed(new_state, depth - 1, -inf, inf, player, deadline, aborted);
      if (aborted) return true;
    }
    return false;
#else
    int thread_count = num_threads > 0
      ? num_threads
      : (int)std::max(1u, std::thread::hardware_concurrency());
    thread_count = std::min(thread_count, num_actions);

    auto thread_aborted = std::vector<char>(thread_count, 0);
    auto threads        = std::vector<std::thread>(thread_count);
    for (int t = 0; t < thread_count; ++t) {
      threads[t] = std::thread([&, t] {
        bool aborted = false;
        // Round-robin assignment spreads the variable-size root subtrees evenly.
        for (int action_index = t; action_index < num_actions;
             action_index += thread_count) {
          Game_T new_state = concrete;
          resolve_choice(new_state, choice, action_index);
          scores[action_index] = minimax_timed(
            new_state, depth - 1, -inf, inf, player, deadline, aborted
          );
          if (aborted) {
            thread_aborted[t] = 1;
            return;
          }
        }
      });
    }
    for (auto& thread : threads) thread.join();
    for (char aborted : thread_aborted) {
      if (aborted) return true;
    }
    return false;
#endif
  }
};

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
    // std::vector<float>               total_scores(num_actions, 0.0f);

#ifdef __EMSCRIPTEN__
    for (int s = 0; s < num_samples; ++s) {
      Game_T sampled = sample_state(concrete, choice.player_index, rng);
      std::vector<float> scores = minimax_scores<Game_T>(
        sampled, choice, num_actions, choice.player_index, this->max_depth
      );
      votes[argmax(scores)] += 1;
      // for (int i = 0; i < num_actions; ++i) total_scores[i] += scores[i];
    }
#else
    // Parallelize over samples: each is independent, so threads never share
    // state. scoress[s] is written by exactly one thread (index s), so no
    // synchronisation is needed when reading the results after joining.
    auto scoress = std::vector<std::vector<float>>(num_samples);
    auto threads = std::vector<std::thread>(num_samples);
    for (int s = 0; s < num_samples; ++s) {
      threads[s] = std::thread([&, s] {
        // Local rng per thread: avoids contention and gives distinct sequences.
        std::mt19937 local_rng{std::random_device{}()};
        Game_T sampled = sample_state(concrete, choice.player_index, local_rng);
        scoress[s]     = minimax_scores<Game_T>(
          sampled, choice, num_actions, choice.player_index, this->max_depth
        );
      });
    }
    for (auto& t : threads) t.join();
    for (const auto& scores : scoress) {
      votes[argmax_randomized(scores)] += 1;
      // for (int i = 0; i < num_actions; ++i) total_scores[i] += scores[i];
    }
#endif

    return argmax_randomized(votes);
    // return argmax(total_scores);
  }
};
