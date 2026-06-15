#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <limits>
#include <optional>
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

using Clock    = std::chrono::steady_clock;
using Deadline = Clock::time_point;

// Resolve every child of `choice` once, in place in `children` (each already a
// copy of the parent position), and return their indices ordered best-first for
// the side to move: scored by the cheap static evaluation of the resulting
// position, highest score first when `maximizing`, lowest otherwise. Searching
// the likely-best child first tightens the alpha/beta window sooner, so far more
// of the remaining tree is pruned. The caller reuses `children` for the
// recursion, so each child is resolved exactly once.
template <class Game_T>
Array_Inline<int, 64> order_children(
  std::vector<Game_T>& children,
  const Choice&        choice,
  int                  player_index,
  bool                 maximizing
) {
  int  num_actions = (int)children.size();
  auto scores      = Array_Inline<float, 64>();
  auto indices     = Array_Inline<int, 64>();
  for (int i = 0; i < num_actions; ++i) {
    resolve_choice(children[i], choice, i);
    scores.push_back(evaluate_state(children[i], player_index));
    indices.push_back(i);
  }
  std::sort(indices.begin(), indices.end(), [&](int a, int b) {
    return maximizing ? scores[a] > scores[b] : scores[a] < scores[b];
  });
  return indices;
}

// Alpha-beta minimax. Templated on the concrete Game subclass so the search can
// copy state by value (no clone() / unique_ptr needed). `aborted` is a predicate
// checked at every node; when it returns true the search unwinds early with a
// partial value the caller must discard. Pass a lambda that always returns false
// for an unbounded search, or one that tests a wall-clock deadline for a
// time-bounded one.
template <class Game_T, typename Aborted>
float minimax(
  Game_T& state,
  int     depth,
  float   alpha,
  float   beta,
  int     player_index,
  Aborted aborted
) {
  if (state.is_game_over() || depth == 0) {
    return evaluate_state(state, player_index);
  }
  if (aborted()) return evaluate_state(state, player_index);

  std::optional<Choice> choice = state.next_choice();
  if (!choice) return evaluate_state(state, player_index);

  const int num_actions = action_options_count(choice->actions(state));
  if (num_actions == 0) return evaluate_state(state, player_index);

  const bool  maximizing = choice->player_index == player_index;
  const float inf        = std::numeric_limits<float>::infinity();
  float       value      = maximizing ? -inf : inf;

  auto children = std::vector<Game_T>(num_actions, state);
  auto indices  = order_children(children, *choice, player_index, maximizing);

  for (int i = 0; i < num_actions; i++) {
    int   action_index = indices[i];
    float score        = minimax(
      children[action_index], depth - 1, alpha, beta, player_index, aborted
    );
    if (aborted()) return value;  // Result is incomplete; the caller drops it.
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

// Search every root action and return their exact scores, indexed by action.
// Each action is searched with a full [-inf, +inf] window, so the scores are
// exact values (not alpha-beta bounds) — which matters when several actions tie
// for best. The root actions are resolved once and ordered best-first like every
// interior node; with full windows that does not change the scores, only the
// order they are produced in. When `num_threads != 1` the actions are split
// across threads; each score is written by exactly one thread, so the disjoint
// writes need no locking. `aborted` is forwarded to the search (see minimax).
template <class Game_T, typename Aborted>
std::vector<float> minimax_scores(
  Game_T&       state,
  const Choice& choice,
  int           num_actions,
  int           player_index,
  int           max_depth,
  int           num_threads,
  Aborted       aborted
) {
  using minimax_detail::minimax;
  using minimax_detail::order_children;
  const float        inf = std::numeric_limits<float>::infinity();
  std::vector<float> scores(num_actions, -inf);

  // Resolve and order the root children once, exactly like an interior node.
  bool maximizing = choice.player_index == player_index;
  auto children   = std::vector<Game_T>(num_actions, state);
  auto indices    = order_children(children, choice, player_index, maximizing);

  // Shared lower bound across the root moves: each move is searched with the
  // best score found so far as its alpha, so a move that cannot beat it is cut
  // early. With the best move searched first (the ordering above), this prunes
  // most of the rest. The winning move still gets its exact score; the others
  // may come back as a bound, which is fine for picking the best.
  // ponytail: threads update this float without locking; the value only ever
  // rises, so a missed update just costs a little pruning, never a wrong move.
  // Add a lock only if a data-race sanitizer must stay clean.
  float shared_alpha = -inf;

  auto search_one = [&](int action_index) {
    float score = minimax(
      children[action_index], max_depth, shared_alpha, inf, player_index, aborted
    );
    scores[action_index] = score;
    if (score > shared_alpha) shared_alpha = score;
  };

#ifdef __EMSCRIPTEN__
  for (int k = 0; k < num_actions; ++k) search_one(indices[k]);
#else
  int thread_count =
    num_threads > 0 ? num_threads
                    : (int)std::max(1u, std::thread::hardware_concurrency());
  thread_count = std::min(thread_count, num_actions);

  if (thread_count <= 1) {
    for (int k = 0; k < num_actions; ++k) search_one(indices[k]);
  } else {
    // Round-robin over the ordered actions balances the uneven subtrees.
    auto threads = std::vector<std::thread>(thread_count);
    for (int t = 0; t < thread_count; ++t) {
      threads[t] = std::thread([&, t] {
        for (int k = t; k < num_actions; k += thread_count) {
          search_one(indices[k]);
        }
      });
    }
    for (auto& thread : threads) thread.join();
  }
#endif
  return scores;
}

// Alpha-beta minimax agent. Root-parallel: minimax_scores splits the root moves
// across threads (each with a full window, so the split costs no pruning).
template <class Game_T>
struct Agent_Minimax : Agent {
  int max_depth;
  int num_threads;  // 0 = one per hardware core. Always 1 under Emscripten.

  explicit Agent_Minimax(int max_depth, int num_threads = 0)
      : max_depth(max_depth), num_threads(num_threads) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete    = static_cast<Game_T&>(state);
    const int num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;

    std::vector<float> scores = minimax_scores<Game_T>(
      concrete, choice, num_actions, choice.player_index, max_depth,
      num_threads, [] { return false; }
    );
    return argmax_randomized(scores);
  }
};

// Iterative-deepening alpha-beta with a wall-clock budget. Searches depth
// 1, 2, 3, ... and plays the best root move from the deepest search that
// finished before the budget ran out. A search that overruns mid-depth aborts
// (its partial scores discarded), so the move time stays close to the budget.
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

    const auto deadline =
      minimax_detail::Clock::now() +
      std::chrono::duration_cast<minimax_detail::Clock::duration>(
        std::chrono::duration<float>(time_budget_seconds)
      );
    auto aborted = [deadline] {
      return minimax_detail::Clock::now() >= deadline;
    };

    int best_action     = 0;
    int completed_depth = 0;
    for (int depth = 1; depth <= max_depth; ++depth) {
      std::vector<float> scores = minimax_scores<Game_T>(
        concrete, choice, num_actions, choice.player_index, depth, num_threads,
        aborted
      );
      // Keep the deepest fully completed depth; a partial one is unreliable.
      if (aborted()) break;
      best_action     = (int)argmax_randomized(scores);
      completed_depth = depth;
    }
    std::fprintf(stderr, "minimax: reached depth %d\n", completed_depth);
    return best_action;
  }
};

template <class Game_T>
struct Agent_Minimax_Stochastic : Agent_Minimax<Game_T> {
  int num_samples = 20;

  Agent_Minimax_Stochastic(int max_depth = 6, int num_samples = 20)
      : Agent_Minimax<Game_T>(max_depth), num_samples(num_samples) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T& concrete    = static_cast<Game_T&>(state);
    int     num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::vector<int>                 votes(num_actions, 0);

    // Each sample searches serially (num_threads = 1); the parallelism is over
    // the samples instead, since they are independent and never share state.
#ifdef __EMSCRIPTEN__
    for (int s = 0; s < num_samples; ++s) {
      Game_T             sampled = sample_state(concrete, choice.player_index, rng);
      std::vector<float> scores  = minimax_scores<Game_T>(
        sampled, choice, num_actions, choice.player_index, this->max_depth, 1,
        [] { return false; }
      );
      votes[argmax(scores)] += 1;
    }
#else
    // scoress[s] is written by exactly one thread (index s), so no
    // synchronisation is needed when reading the results after joining.
    auto scoress = std::vector<std::vector<float>>(num_samples);
    auto threads = std::vector<std::thread>(num_samples);
    for (int s = 0; s < num_samples; ++s) {
      threads[s] = std::thread([&, s] {
        // Local rng per thread: avoids contention and gives distinct sequences.
        std::mt19937 local_rng{std::random_device{}()};
        Game_T sampled = sample_state(concrete, choice.player_index, local_rng);
        scoress[s]     = minimax_scores<Game_T>(
          sampled, choice, num_actions, choice.player_index, this->max_depth, 1,
          [] { return false; }
        );
      });
    }
    for (auto& t : threads) t.join();
    for (const auto& scores : scoress) votes[argmax_randomized(scores)] += 1;
#endif

    return argmax_randomized(votes);
  }
};
