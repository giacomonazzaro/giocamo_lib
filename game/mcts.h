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
#include "stochastic.h"

#ifndef __EMSCRIPTEN__
#include <thread>
#endif

namespace mcts_detail {

// A single node in the MCTS tree.
struct Node {
  std::vector<int> children;
  int              visits;
  float value_sum;  // Cumulative reward, from root_player's perspective.
  // Seat to move at this node, taken from the state's pending choice. -1 when
  // the node is terminal.
  int player_index;
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
    if (pending_action_count(state) == 0) break;
    const int action_index =
      rollout_agent.choose_action(state, pending_choice(state));
    if (action_index < 0) break;
    resolve_choice(state, action_index);
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
  const bool  maximizing        = (parent.player_index == root_player);
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

// Initializes a freshly created node from its state: records the seat to move
// and how many actions the state's pending choice offers. The children vector
// stays empty — children are materialized later by the first expansion of this
// node.
template <class Game_T>
void initialize_node(Node& node, Game_T& state, int parent) {
  node.visits    = 0;
  node.value_sum = 0.0f;
  node.children.clear();
  node.player_index = -1;
  node.num_actions  = 0;
  if (state.is_game_over()) return;
  const int num_actions = pending_action_count(state);
  if (num_actions == 0) return;
  node.player_index = pending_choice(state).player_index;
  node.num_actions  = num_actions;
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
    // A child starts as a copy of the parent, so it carries the same pending
    // choice and `action_index` means the same thing in both.
    Game_T child_state = states[parent_index];
    resolve_choice(child_state, action_index);
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

// A rollout policy that biases play toward stronger moves instead of choosing
// uniformly at random. For each legal action it applies the action to a copy of
// the state and scores the resulting position with evaluate_state from the
// acting player's perspective, then samples an action with probability
// proportional to softmax(score / temperature). Low temperature is greedy; high
// temperature approaches uniform random. Plugs into Agent_MCTS_Stochastic as
// the Rollout_Agent_T. Game_T must provide evaluate_state(Game_T&, int) — the
// same hook the rollout's terminal evaluation already uses.
template <class Game_T>
struct Agent_Softmax_Rollout : Agent {
  float        temperature;
  std::mt19937 rng;

  explicit Agent_Softmax_Rollout(float temperature = 1.0f)
      : temperature(temperature), rng(std::random_device{}()) {}

  void message(const std::string&) override {}

  int choose_action(Game& game, const Choice&) override {
    const int num_actions = pending_action_count(game);
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;
    const int player = pending_choice(game).player_index;

    // Score each action by the heuristic value of the position it leads to.
    Array_Inline<float, 16> weights;
    float                   max_score = -std::numeric_limits<float>::infinity();
    for (int action_index = 0; action_index < num_actions; ++action_index) {
      Game_T next = static_cast<Game_T&>(game);
      resolve_choice(next, action_index);
      const float score = evaluate_state(next, player);
      weights.push_back(score);
      if (score > max_score) max_score = score;
    }

    // Softmax over the scores (max-shifted for numerical stability), then draw
    // one action from the resulting distribution.
    float sum = 0.0f;
    for (int i = 0; i < num_actions; ++i) {
      weights[i] = std::exp((weights[i] - max_score) / temperature);
      sum += weights[i];
    }
    std::uniform_real_distribution<float> dist(0.0f, sum);
    const float                           threshold = dist(rng);
    float                                 running   = 0.0f;
    for (int i = 0; i < num_actions; ++i) {
      running += weights[i];
      if (running >= threshold) return i;
    }
    return num_actions - 1;
  }
};

// Runs MCTS for `num_iterations` and returns one score per root action — the
// visit count of the corresponding root child. Visit counts are the standard
// final selection criterion for MCTS and are more robust to outliers than the
// raw value estimates.
template <class Game_T>
std::vector<float> mcts_scores(
  Game_T&      state,
  int          num_root_actions,
  int          root_player,
  int          num_iterations,
  int          rollout_depth,
  float        exploration_constant,
  Agent&       rollout_agent,
  std::mt19937 rng,
  float        time_budget_seconds = 0.0f,
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

  // Children stay empty until the root is expanded by the first traversal that
  // revisits it.
  Node root_node;
  root_node.visits       = 0;
  root_node.value_sum    = 0.0f;
  root_node.player_index = pending_choice(state).player_index;
  root_node.num_actions  = num_root_actions;
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
//
// Root-parallel: choose_action grows `num_threads` independent trees at once
// (each running the full iteration / time budget) and sums their root visit
// counts. At a fixed wall-clock budget this multiplies the total simulations by
// the core count, which is the cheapest way to make the agent stronger.
template <class Game_T, class Rollout_Agent_T = Agent_Random>
struct Agent_MCTS : Agent {
  int   num_iterations;
  int   rollout_depth;
  float exploration_constant;
  // Soft wall-clock budget per choose_action call. 0 disables the bound and
  // the agent runs the full `num_iterations`.
  float time_budget_seconds;
  // Number of independent search trees to grow in parallel. 0 means "one per
  // hardware core". Always 1 under Emscripten, which has no worker threads.
  int num_threads;
  // Optional leaf value function. When set, the simulation step replaces the
  // random rollout with a direct value lookup at each leaf — e.g. a shallow
  // minimax — which is far stronger than random playouts in tactical games.
  // Shared (read-only) across the search threads.
  std::function<float(const Game_T&, int)> leaf_evaluator = nullptr;
  // Builds the agent that plays the rollouts, once per tree. Set for you when
  // the policy is default-constructible (Agent_Random is); a policy that needs
  // arguments is handed in by the caller.
  std::function<Rollout_Agent_T()> rollout_agent_factory;

  Agent_MCTS(
    int   num_iterations       = 1000,
    int   rollout_depth        = 64,
    float exploration_constant = 1.41421356f,
    float time_budget_seconds  = 0.0f,
    int   num_threads          = 0
  )
      : num_iterations(num_iterations)
      , rollout_depth(rollout_depth)
      , exploration_constant(exploration_constant)
      , time_budget_seconds(time_budget_seconds)
      , num_threads(num_threads) {
    if constexpr (std::is_default_constructible_v<Rollout_Agent_T>) {
      rollout_agent_factory = []() -> Rollout_Agent_T {
        return Rollout_Agent_T();
      };
    }
  }

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice&) override {
    Game_T&   concrete     = static_cast<Game_T&>(state);
    const int num_actions  = pending_action_count(state);
    const int player_index = pending_choice(state).player_index;
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;

#ifdef __EMSCRIPTEN__
    // No worker threads on the web: grow a single tree.
    static thread_local std::mt19937 rng{std::random_device{}()};
    Rollout_Agent_T    rollout_agent = rollout_agent_factory();
    std::vector<float> scores        = mcts_scores<Game_T>(
      concrete,
      num_actions,
      player_index,
      num_iterations,
      rollout_depth,
      exploration_constant,
      rollout_agent,
      rng,
      time_budget_seconds,
      leaf_evaluator
    );
    return argmax_randomized(scores);
#else
    int thread_count =
      num_threads > 0 ? num_threads
                      : (int)std::max(1u, std::thread::hardware_concurrency());

    // Each thread grows its own tree with its own rollout agent and rng — no
    // shared mutable state, so no locking. concrete is only read (mcts_scores
    // copies the state into its own tree), so sharing it is safe.
    auto per_thread_scores = std::vector<std::vector<float>>(thread_count);
    auto threads           = std::vector<std::thread>(thread_count);
    for (int t = 0; t < thread_count; ++t) {
      threads[t] = std::thread([&, t] {
        Rollout_Agent_T rollout_agent = rollout_agent_factory();
        std::mt19937    rng{std::random_device{}()};
        per_thread_scores[t] = mcts_scores<Game_T>(
          concrete,
          num_actions,
          player_index,
          num_iterations,
          rollout_depth,
          exploration_constant,
          rollout_agent,
          rng,
          time_budget_seconds,
          leaf_evaluator
        );
      });
    }
    for (auto& thread : threads) thread.join();

    // Sum visit counts across the independent trees, then pick the best.
    auto scores = std::vector<float>(num_actions, 0.0f);
    for (const auto& tree_scores : per_thread_scores) {
      for (int i = 0; i < num_actions; ++i) scores[i] += tree_scores[i];
    }
    return argmax_randomized(scores);
#endif
  }
};

// MCTS on sampled deals, for a game with hidden information. The sampling
// itself is Agent_Stochastic's; this is only the shape the callers ask for.
// The rollout policy is the second parameter, and a policy that needs
// arguments is handed in through `rollout_agent_factory`.
template <class Game_T, class Rollout_Agent_T = Agent_Random>
struct Agent_MCTS_Stochastic
    : Agent_Stochastic<Game_T, Agent_MCTS<Game_T, Rollout_Agent_T>> {
  int   num_iterations;
  int   rollout_depth;
  float exploration_constant;
  float time_budget_seconds;

  std::function<Rollout_Agent_T()>         rollout_agent_factory;
  std::function<float(const Game_T&, int)> leaf_evaluator = nullptr;

  Agent_MCTS_Stochastic(
    int   num_iterations       = 1000,
    int   rollout_depth        = 64,
    int   num_samples          = 20,
    float exploration_constant = 1.41421356f,
    float time_budget_seconds  = 0.0f
  )
      : Agent_Stochastic<Game_T, Agent_MCTS<Game_T, Rollout_Agent_T>>(
          nullptr, num_samples
        )
      , num_iterations(num_iterations)
      , rollout_depth(rollout_depth)
      , exploration_constant(exploration_constant)
      , time_budget_seconds(time_budget_seconds) {
    if constexpr (std::is_default_constructible_v<Rollout_Agent_T>) {
      rollout_agent_factory = []() -> Rollout_Agent_T {
        return Rollout_Agent_T();
      };
    }
    // Read when a sample starts, not now, so a caller can still set the
    // rollout policy or the leaf evaluator after building this.
    this->make_inner = [this] {
      auto agent = Agent_MCTS<Game_T, Rollout_Agent_T>(
        this->num_iterations,
        this->rollout_depth,
        this->exploration_constant,
        this->time_budget_seconds,
        1  // The sampling owns the threads.
      );
      agent.rollout_agent_factory = this->rollout_agent_factory;
      agent.leaf_evaluator        = this->leaf_evaluator;
      return agent;
    };
  }
};
