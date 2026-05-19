#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <vector>

#include "agent.h"
#include "game.h"
#include "minimax.h"  // For argmax / argmax_randomized.

#ifndef __EMSCRIPTEN__
#include <thread>
#endif

namespace mcts_detail {

// A single node in the MCTS tree. The `choice` field caches the
// pending Choice once `next_choice()` has been called on the node's state, so
// expansion of N children only pays for `next_choice()` once per node — this
// matters for games whose `next_choice()` mutates state.
struct Node {
  int              parent;  // Parent node index, -1 for root.
  std::vector<int> children;
  int              visits;
  float  value_sum;  // Cumulative reward, from root_player's perspective.
  Choice choice;     // Pending choice to resolve from this node.
};

// Rollout from `state` using `rollout_agent` to pick actions. Plays until the
// game ends or `max_depth` plies have been taken, then evaluates the resulting
// position from `root_player`'s perspective. Passing an Agent here means the
// rollout policy is swappable (random by default, but a heuristic agent works
// just as well).
template <class Game_T>
float rollout(
  Game_T state, int root_player, Agent& rollout_agent, int max_depth
) {
  for (int depth = 0; depth < max_depth; ++depth) {
    if (state.is_game_over()) break;
    std::optional<Choice> choice = state.next_choice();
    if (!choice) break;
    const int num_actions = action_options_count(choice->actions(state));
    if (num_actions == 0) break;
    const int action_index = rollout_agent.choose_action(state, *choice);
    resolve_choice(state, *choice, action_index);
  }
  return evaluate_state(state, root_player);
}

// Picks the child with the highest UCB1 score. When the current node belongs
// to `root_player`, larger average reward is better (maximizing); otherwise
// the opponent is assumed to minimize root_player's reward.
inline int best_ucb1_child(
  const std::vector<Node>& nodes,
  int                      node_index,
  int                      root_player,
  float                    exploration_constant
) {
  const Node& parent            = nodes[node_index];
  const bool  maximizing        = (parent.choice.player_index == root_player);
  const float log_parent_visits = std::log((float)std::max(1, parent.visits));
  int         best_action       = 0;
  float       best_score        = -std::numeric_limits<float>::infinity();
  for (int action_index = 0; action_index < (int)parent.children.size();
       ++action_index) {
    const Node& child   = nodes[parent.children[action_index]];
    const float average = child.value_sum / (float)child.visits;
    const float exploit = maximizing ? average : -average;
    const float explore = exploration_constant *
                          std::sqrt(log_parent_visits / (float)child.visits);
    const float score = exploit + explore;
    if (score > best_score) {
      best_score  = score;
      best_action = action_index;
    }
  }
  return best_action;
}

// Initializes a freshly created node from its state. Calls state.next_choice()
// to discover the next pending choice, leaving the node terminal when the game
// is over or no actions are available.
template <class Game_T>
void initialize_node(Node& node, Game_T& state, int parent) {
  node.parent    = parent;
  node.visits    = 0;
  node.value_sum = 0.0f;
  node.children.clear();
  node.choice = Choice();
  if (state.is_game_over()) return;
  std::optional<Choice> choice = state.next_choice();
  if (!choice) return;
  const int num_actions = action_options_count(choice->actions(state));
  if (num_actions == 0) return;
  node.children.assign(num_actions, -1);
  node.choice = std::move(*choice);
}

}  // namespace mcts_detail

// Runs MCTS for `num_iterations` and returns one score per root action — the
// visit count of the corresponding root child. Visit counts are the standard
// final selection criterion for MCTS and are more robust to outliers than the
// raw value estimates.
template <class Game_T>
std::vector<float> mcts_scores(
  Game_T&       state,
  const Choice& root_choice,
  int           num_root_actions,
  int           root_player,
  int           num_iterations,
  int           rollout_depth,
  float         exploration_constant,
  Agent&        rollout_agent,
  float         time_budget_seconds = 0.0f
) {
  using mcts_detail::best_ucb1_child;
  using mcts_detail::initialize_node;
  using mcts_detail::Node;
  using mcts_detail::rollout;

  // When `time_budget_seconds` is positive, iterations stop as soon as the
  // wall-clock budget is exhausted (whichever happens first). Zero disables
  // the time bound and the call runs the full `num_iterations`.
  const auto start_time = std::chrono::steady_clock::now();

  std::vector<Node>   nodes;
  std::vector<Game_T> states;
  // Reserve so push_back never reallocates, keeping references and the parallel
  // index relationship between `nodes` and `states` stable across iterations.
  nodes.reserve(num_iterations + 1);
  states.reserve(num_iterations + 1);

  // The caller has already popped `root_choice` from the game's choices queue,
  // so we use it directly rather than calling state.next_choice() (which would
  // return a later pending choice).
  Node root_node;
  root_node.parent = -1;
  root_node.children.assign(num_root_actions, -1);
  root_node.visits    = 0;
  root_node.value_sum = 0.0f;
  root_node.choice    = root_choice;
  nodes.push_back(std::move(root_node));
  states.push_back(state);

  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    // 1) Selection: descend the tree until we either find a node with an
    //    unexpanded action or reach a terminal node.
    int node_index = 0;
    while (!nodes[node_index].children.empty()) {
      int unexpanded_action = -1;
      for (int i = 0; i < (int)nodes[node_index].children.size(); ++i) {
        if (nodes[node_index].children[i] < 0) {
          unexpanded_action = i;
          break;
        }
      }

      if (unexpanded_action >= 0) {
        // 2) Expansion: create a new child node for the chosen action.
        Game_T child_state = states[node_index];
        resolve_choice(
          child_state, nodes[node_index].choice, unexpanded_action
        );
        Node child_node;
        initialize_node(child_node, child_state, node_index);
        const int child_index = (int)nodes.size();
        nodes.push_back(std::move(child_node));
        states.push_back(std::move(child_state));
        nodes[node_index].children[unexpanded_action] = child_index;
        node_index                                    = child_index;
        break;
      }

      // All children expanded: descend by UCB1.
      const int best_action =
        best_ucb1_child(nodes, node_index, root_player, exploration_constant);
      node_index = nodes[node_index].children[best_action];
    }

    // 3) Simulation: roll out from the leaf we just reached.
    const float reward = rollout<Game_T>(
      states[node_index], root_player, rollout_agent, rollout_depth
    );

    // 4) Backpropagation: update visit counts and value sums up to the root.
    int current = node_index;
    while (current >= 0) {
      nodes[current].visits += 1;
      nodes[current].value_sum += reward;
      current = nodes[current].parent;
    }

    if (time_budget_seconds > 0.0f) {
      const float elapsed = std::chrono::duration<float>(
                              std::chrono::steady_clock::now() - start_time
      )
                              .count();
      if (elapsed >= time_budget_seconds) break;
    }
  }

  std::vector<float> scores(num_root_actions, 0.0f);
  for (int i = 0; i < num_root_actions; ++i) {
    const int child_index = nodes[0].children[i];
    if (child_index < 0) continue;
    scores[i] = (float)nodes[child_index].visits;
  }
  return scores;
}

// MCTS agent. Templated on the concrete Game subclass so the search can copy
// state by value, in the same spirit as Agent_Minimax.
template <class Game_T>
struct Agent_MCTS : Agent {
  int   num_iterations;
  int   rollout_depth;
  float exploration_constant;
  // Soft wall-clock budget per choose_action call. 0 disables the bound and
  // the agent runs the full `num_iterations`.
  float time_budget_seconds;

  Agent_MCTS(
    int   num_iterations       = 1000,
    int   rollout_depth        = 64,
    float exploration_constant = 1.41421356f,
    float time_budget_seconds  = 0.0f
  )
      : num_iterations(num_iterations)
      , rollout_depth(rollout_depth)
      , exploration_constant(exploration_constant)
      , time_budget_seconds(time_budget_seconds) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete    = static_cast<Game_T&>(state);
    const int num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;
    static thread_local Agent_Random rollout_agent;

    std::vector<float> scores = mcts_scores<Game_T>(
      concrete,
      choice,
      num_actions,
      choice.player_index,
      num_iterations,
      rollout_depth,
      exploration_constant,
      rollout_agent,
      time_budget_seconds
    );
    return argmax_randomized(scores);
  }
};

// Stochastic MCTS: shuffles hidden information each sample, runs MCTS on the
// resulting determinization, and aggregates votes — mirrors the structure of
// Agent_Minimax_Stochastic. Requires sample_state(state, player_index, rng) to
// be defined for Game_T, just like the stochastic minimax agent.
template <class Game_T>
struct Agent_MCTS_Stochastic : Agent_MCTS<Game_T> {
  int num_samples;

  Agent_MCTS_Stochastic(
    int   num_iterations       = 1000,
    int   rollout_depth        = 64,
    int   num_samples          = 20,
    float exploration_constant = 1.41421356f,
    float time_budget_seconds  = 0.0f
  )
      : Agent_MCTS<Game_T>(
          num_iterations,
          rollout_depth,
          exploration_constant,
          time_budget_seconds
        )
      , num_samples(num_samples) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete    = static_cast<Game_T&>(state);
    const int num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;

    std::vector<int> votes(num_actions, 0);

#ifdef __EMSCRIPTEN__
    static thread_local std::mt19937 sampling_rng{std::random_device{}()};
    static thread_local Agent_Random rollout_agent;
    // Sequential path: each sample runs in turn, so the per-call budget has to
    // be split across samples to keep the total per-move time on target.
    const float per_sample_budget = (this->time_budget_seconds > 0.0f)
                                      ? this->time_budget_seconds /
                                          (float)num_samples
                                      : 0.0f;
    for (int sample_index = 0; sample_index < num_samples; ++sample_index) {
      Game_T sampled =
        sample_state(concrete, choice.player_index, sampling_rng);
      std::vector<float> scores = mcts_scores<Game_T>(
        sampled,
        choice,
        num_actions,
        choice.player_index,
        this->num_iterations,
        this->rollout_depth,
        this->exploration_constant,
        rollout_agent,
        per_sample_budget
      );
      votes[argmax(scores)] += 1;
    }
#else
    // Each sample is independent — run them on separate threads. Each thread
    // gets its own sampling rng and its own Agent_Random so the two never
    // share state. scoress[s] is written by exactly one thread (index s), so
    // no synchronisation is needed after joining.
    auto scoress = std::vector<std::vector<float>>(num_samples);
    auto threads = std::vector<std::thread>(num_samples);
    for (int sample_index = 0; sample_index < num_samples; ++sample_index) {
      threads[sample_index] = std::thread([&, sample_index] {
        std::mt19937 local_sampling_rng{std::random_device{}()};
        Agent_Random local_rollout_agent;
        Game_T       sampled =
          sample_state(concrete, choice.player_index, local_sampling_rng);
        // Parallel path: samples run in parallel, so each thread gets the
        // full per-move budget and the total wall time stays ~budget.
        scoress[sample_index] = mcts_scores<Game_T>(
          sampled,
          choice,
          num_actions,
          choice.player_index,
          this->num_iterations,
          this->rollout_depth,
          this->exploration_constant,
          local_rollout_agent,
          this->time_budget_seconds
        );
      });
    }
    for (auto& thread : threads) thread.join();
    for (const auto& scores : scoress) votes[argmax_randomized(scores)] += 1;
#endif

    return argmax_randomized(votes);
  }
};
