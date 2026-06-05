#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <random>
#include <type_traits>
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
  std::vector<int> children;
  int              visits;
  float  value_sum;  // Cumulative reward, from root_player's perspective.
  Choice choice;     // Pending choice to resolve from this node.
  // Number of children this node will have once expanded. 0 means terminal
  // (game over or no actions available). A node is a leaf while `children`
  // is empty; once expanded, children.size() == num_actions.
  int num_actions;
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
// the opponent is assumed to minimize root_player's reward. Children that
// haven't been visited yet have an infinite UCB1 score, so they're picked
// before any standard scoring kicks in.
inline int best_ucb1_child(
  const std::vector<Node>& nodes,
  int                      node_index,
  int                      root_player,
  float                    exploration_constant
) {
  const Node& parent = nodes[node_index];
  // Prefer any never-visited child: UCB1 is infinite for visits == 0, and
  // avoids a division-by-zero in the score formula below.
  for (int action_index = 0; action_index < (int)parent.children.size();
       ++action_index) {
    if (nodes[parent.children[action_index]].visits == 0) return action_index;
  }
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
// to cache the next pending choice and the number of actions it offers. The
// children vector stays empty — children are materialized later by the first
// expansion of this node.
template <class Game_T>
void initialize_node(Node& node, Game_T& state, int parent) {
  node.visits    = 0;
  node.value_sum = 0.0f;
  node.children.clear();
  node.choice      = Choice();
  node.num_actions = 0;
  if (state.is_game_over()) return;
  std::optional<Choice> choice = state.next_choice();
  if (!choice) return;
  const int num_actions = action_options_count(choice->actions(state));
  if (num_actions == 0) return;
  node.choice      = std::move(*choice);
  node.num_actions = num_actions;
}

// Walks down the tree from the root by UCB1 until it reaches a leaf — a node
// with no children allocated yet. If the leaf has never been visited (or is
// terminal) it's returned as-is for simulation. Otherwise the leaf is expanded
// (all its children are materialized) and the first child is returned.
template <class Game_T>
std::vector<int> traverse_to_leaf_node(
  std::vector<Node>&   nodes,
  std::vector<Game_T>& states,
  int                  root_player,
  float                exploration_constant,
  std::mt19937&        rng
) {
  // Descend through expanded nodes until we reach a leaf.
  int                      node_index = 0;
  static thread_local auto path       = std::vector<int>();
  path.clear();
  path.push_back(node_index);
  while (!nodes[node_index].children.empty()) {
    const int best_action =
      best_ucb1_child(nodes, node_index, root_player, exploration_constant);
    node_index = nodes[node_index].children[best_action];
    path.push_back(node_index);
  }

  // Fresh or terminal leaf: simulate from here.
  if (nodes[node_index].visits == 0) return path;
  if (nodes[node_index].num_actions == 0) return path;

  // Visited leaf: expand all children and return the first one.
  const int parent_index = node_index;
  const int num_children = nodes[parent_index].num_actions;
  nodes[parent_index].children.resize(num_children);
  for (int action_index = 0; action_index < num_children; ++action_index) {
    Game_T child_state = states[parent_index];
    resolve_choice(child_state, nodes[parent_index].choice, action_index);
    Node child_node;
    initialize_node(child_node, child_state, parent_index);
    const int child_index = (int)nodes.size();
    nodes.push_back(std::move(child_node));
    states.push_back(std::move(child_state));
    nodes[parent_index].children[action_index] = child_index;
  }
  path.push_back(nodes[parent_index].children[rng() % num_children]);
  return path;
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
  std::mt19937  rng,
  float         time_budget_seconds = 0.0f,
  // Optional: when set, the simulation step replaces the random rollout with a
  // direct value lookup at the leaf — AlphaZero-style "neural value at leaf"
  // instead of Monte Carlo. Use a callable like
  // [&net](const Game_T& s, int p) { return net.predict(s, p); }.
  // When set, rollout_agent and rollout_depth are unused.
  std::function<float(const Game_T&, int)> leaf_evaluator = nullptr
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
  // return a later pending choice). Children stay empty until the root is
  // expanded by the first traversal that revisits it.
  Node root_node;
  root_node.visits      = 0;
  root_node.value_sum   = 0.0f;
  root_node.choice      = root_choice;
  root_node.num_actions = num_root_actions;
  nodes.push_back(std::move(root_node));
  states.push_back(state);

  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    const auto path = mcts_detail::traverse_to_leaf_node(
      nodes, states, root_player, exploration_constant, rng
    );
    const int node_index = path.back();

    // 3) Simulation: either evaluate the leaf with the supplied value
    // function, or fall back to a random rollout.
    const float reward =
      leaf_evaluator
        ? leaf_evaluator(states[node_index], root_player)
        : rollout<Game_T>(
            states[node_index], root_player, rollout_agent, rollout_depth
          );

    // 4) Backpropagation: update visit counts and value sums up to the root.
    for (int i = (int)path.size() - 1; i >= 0; --i) {
      nodes[path[i]].visits += 1;
      nodes[path[i]].value_sum += reward;
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
  // If the root never got expanded (e.g., when num_iterations is tiny) every
  // score stays at zero and the agent falls back to picking uniformly.
  if ((int)nodes[0].children.size() < num_root_actions) return scores;
  for (int i = 0; i < num_root_actions; ++i) {
    scores[i] = (float)nodes[nodes[0].children[i]].visits;
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
    static thread_local std::mt19937 rng{std::random_device{}()};

    std::vector<float> scores = mcts_scores<Game_T>(
      concrete,
      choice,
      num_actions,
      choice.player_index,
      num_iterations,
      rollout_depth,
      exploration_constant,
      rollout_agent,
      rng,
      time_budget_seconds
    );
    return argmax_randomized(scores);
  }
};

// Stochastic MCTS: shuffles hidden information each sample, runs MCTS on the
// resulting determinization, and aggregates votes — mirrors the structure of
// Agent_Minimax_Stochastic. Requires sample_state(state, player_index, rng) to
// be defined for Game_T, just like the stochastic minimax agent.
//
// The rollout policy is configurable via the Rollout_Agent_T template
// parameter (default Agent_Random). To use a different policy, pass the type
// and set `rollout_agent_factory` to a callable that constructs one with the
// desired arguments. The factory is called once per thread per choose_action
// call, so each thread gets its own freshly constructed instance.
template <class Game_T, class Rollout_Agent_T = Agent_Random>
struct Agent_MCTS_Stochastic : Agent_MCTS<Game_T> {
  int num_samples;
  // Factory called per-thread to allocate a rollout agent. When the rollout
  // type is default-constructible (e.g. Agent_Random) the constructor sets a
  // default factory that calls `Rollout_Agent_T()`; otherwise the caller must
  // set it before the first `choose_action` call.
  std::function<Rollout_Agent_T()> rollout_agent_factory;

#ifdef __EMSCRIPTEN__
  // Web has no worker threads, so the search runs cooperatively: one
  // determinization sample per frame, with the running vote tally kept here
  // between frames. See the Emscripten branch of choose_action.
  bool             web_search_active = false;
  int              web_samples_done  = 0;
  std::vector<int> web_votes;
#endif

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
      , num_samples(num_samples) {
    if constexpr (std::is_default_constructible_v<Rollout_Agent_T>) {
      rollout_agent_factory = []() -> Rollout_Agent_T {
        return Rollout_Agent_T();
      };
    }
  }

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete    = static_cast<Game_T&>(state);
    const int num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;

#ifdef __EMSCRIPTEN__
    // Web has no worker threads, so a blocking search would freeze the whole
    // page until it finishes. Instead the search is spread across frames: each
    // call runs a single determinization sample and returns -1, which tells the
    // game loop to render this frame and ask again next frame. The vote tally
    // is kept on the agent between frames; once enough samples have been
    // gathered we return the voted-best action.
    static thread_local std::mt19937 rng{std::random_device{}()};
    if (!web_search_active) {
      web_search_active = true;
      web_samples_done  = 0;
      web_votes.assign(num_actions, 0);
    }
    Rollout_Agent_T rollout_agent = rollout_agent_factory();
    // Cap each frame's work by time so a single sample never stalls the loop on
    // a slow device; num_iterations still bounds the tree (and its allocation).
    const float        per_frame_budget = 0.010f;
    Game_T             sampled = sample_state(concrete, choice.player_index, rng);
    std::vector<float> scores  = mcts_scores<Game_T>(
      sampled,
      choice,
      num_actions,
      choice.player_index,
      this->num_iterations,
      this->rollout_depth,
      this->exploration_constant,
      rollout_agent,
      rng,
      per_frame_budget
    );
    web_votes[argmax(scores)] += 1;
    web_samples_done += 1;
    if (web_samples_done < num_samples) return -1;  // Keep thinking next frame.
    web_search_active = false;
    return argmax_randomized(web_votes);
#else
    std::vector<int> votes(num_actions, 0);

    // Each sample is independent — run them on separate threads. Each thread
    // gets its own sampling rng and its own Agent_Random so the two never
    // share state. scoress[s] is written by exactly one thread (index s), so
    // no synchronisation is needed after joining.
    auto scoress = std::vector<std::vector<float>>(num_samples);
    auto threads = std::vector<std::thread>(num_samples);
    for (int sample_index = 0; sample_index < num_samples; ++sample_index) {
      threads[sample_index] = std::thread([&, sample_index] {
        std::mt19937    rng{std::random_device{}()};
        Rollout_Agent_T local_rollout_agent = this->rollout_agent_factory();
        Game_T sampled = sample_state(concrete, choice.player_index, rng);
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
          rng,
          this->time_budget_seconds
        );
      });
    }
    for (auto& thread : threads) thread.join();
    for (const auto& scores : scoress) votes[argmax_randomized(scores)] += 1;
    return argmax_randomized(votes);
#endif
  }
};
