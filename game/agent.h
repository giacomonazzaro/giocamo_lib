#pragma once

#include <algorithm>
#include <iostream>
#include <random>
#include <string>

#ifndef __EMSCRIPTEN__
#include <chrono>
#include <future>
#endif

#include "game.h"

// Index of the largest value, and the same with ties broken at random. Used by
// every agent that scores its options and then picks one.
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
  for (int i = 0; i < (int)v.size(); ++i) {
    if (v[i] == max) argmaxes.push_back(i);
  }
  if (argmaxes.size() == 1) return argmaxes[0];
  return argmaxes[rand() % argmaxes.size()];
}

struct Agent {
  virtual ~Agent() = default;

  virtual void message(const std::string& msg) {
    std::cout << "Agent: " << msg << "\n";
  }

  // Pick an action index. Does NOT call resolve. Return -1 to indicate "not
  // ready yet".
  virtual int choose_action(Game& game, const Choice& choice) = 0;
};

struct Agent_Random : Agent {
  std::mt19937 rng;

  Agent_Random() : rng(std::random_device{}()) {}
  explicit Agent_Random(std::uint32_t seed) : rng(seed) {}

  void message(const std::string&) override {}  // Silent agent.

  int choose_action(Game& state, const Choice& choice) override;
};

struct Agent_Duel : Agent {
  Agent* agents[2];

  Agent_Duel(Agent* agent_0, Agent* agent_1, bool swap) {
    if (swap) {
      agents[0] = agent_1;
      agents[1] = agent_0;
    } else {
      agents[0] = agent_0;
      agents[1] = agent_1;
    }
  }

  void message(const std::string& msg) override {
    std::cout << "Duel: " << msg << "\n";
  }

  int choose_action(Game& state, const Choice& choice) override {
    return agents[choice.player_index]->choose_action(state, choice);
  }
};

inline auto   time_now() { return std::chrono::steady_clock::now(); }
inline double time_elapsed_seconds(
  std::chrono::steady_clock::time_point start
) {
  return std::chrono::duration<double>(time_now() - start).count();
}

// Wraps another Agent and records wall-clock time spent inside its
// choose_action calls. Used to measure the per-agent compute budget the match
// is actually spending.
struct Timing_Agent : Agent {
  Agent*      inner;
  std::string name;
  double      total_seconds = 0.0;
  int         num_calls     = 0;

  Timing_Agent(Agent* inner, std::string name)
      : inner(inner), name(std::move(name)) {}

  void message(const std::string&) override {}

  int choose_action(Game& game, const Choice& choice) override {
    const auto start_time   = time_now();
    const int  action_index = inner->choose_action(game, choice);
    total_seconds += time_elapsed_seconds(start_time);
    num_calls += 1;
    return action_index;
  }

  double average_seconds_per_move() const {
    if (num_calls == 0) return 0.0;
    return total_seconds / (double)num_calls;
  }
};

// Aggregated outcome of benchmark_agents, from agent_a's perspective.
struct Benchmark_Result {
  int    a_wins   = 0;
  int    b_wins   = 0;
  int    draws    = 0;
  int    a_points = 0;
  int    b_points = 0;
  double a_seconds =
    0.0;  // Total wall-clock time agent_a spent choosing moves.
  double b_seconds = 0.0;
  int    a_calls   = 0;  // Number of moves each agent chose.
  int    b_calls   = 0;

  double a_ms_per_move() const {
    return a_calls ? a_seconds / a_calls * 1000.0 : 0.0;
  }
  double b_ms_per_move() const {
    return b_calls ? b_seconds / b_calls * 1000.0 : 0.0;
  }
};

// Plays `num_games` of a two-player game between agent_a and agent_b, swapping
// which seat each holds every game so neither benefits from leading.
//   make_state(game_index) -> Game_T   : a fresh game to play (e.g. a new
//   deal). score(game, player_index) -> int   : that player's final score.
// Each game runs through game_loop; a per-game line with the running win count
// is printed to stderr. The agents are timed internally (move time goes into
// the result). Tallies are returned from agent_a's perspective.
template <class Game_T, class Make_State, class Score_Fn>
Benchmark_Result benchmark_agents(
  int         num_games,
  Make_State  make_state,
  Score_Fn    score,
  Agent&      agent_a,
  const char* name_a,
  Agent&      agent_b,
  const char* name_b
) {
  Timing_Agent timed_a(&agent_a, name_a);
  Timing_Agent timed_b(&agent_b, name_b);

  Benchmark_Result result;
  for (int game_index = 0; game_index < num_games; ++game_index) {
    // Alternate seats: agent_a leads on even games, agent_b on odd ones.
    const bool a_is_player_0 = (game_index % 2 == 0);
    Agent_Duel duel(&timed_a, &timed_b, /*swap=*/!a_is_player_0);

    Game_T game = make_state(game_index);
    game_loop(game, duel);

    const int a_score = score(game, a_is_player_0 ? 0 : 1);
    const int b_score = score(game, a_is_player_0 ? 1 : 0);
    result.a_points += a_score;
    result.b_points += b_score;
    if (a_score > b_score)
      result.a_wins += 1;
    else if (a_score < b_score)
      result.b_wins += 1;
    else
      result.draws += 1;

    std::cerr << "game " << (game_index + 1) << "/" << num_games << "  "
              << name_a << "=" << a_score << "  " << name_b << "=" << b_score
              << "  score=" << result.a_wins << "-" << result.b_wins << "\n";
  }
  result.a_seconds = timed_a.total_seconds;
  result.b_seconds = timed_b.total_seconds;
  result.a_calls   = timed_a.num_calls;
  result.b_calls   = timed_b.num_calls;
  return result;
}

// Runs an inner agent's choose_action on a background thread so the caller
// can keep drawing frames while it thinks. The first call for a given choice
// kicks off the worker and returns -1 immediately; subsequent calls return
// -1 until the worker is done, then return its result and arm the agent
// for the next choice.
//
// Contract: the caller must keep `game` and `choice` alive and unmutated
// across the calls that share a pending computation. game_frame() already
// satisfies this — it holds onto the same Choice until the agent returns a
// non-negative index.
#ifndef __EMSCRIPTEN__
struct Agent_Async : Agent {
  Agent*           agent;
  std::future<int> result;
  bool             is_thinking = false;

  explicit Agent_Async(Agent* agent) : agent(agent) {}

  ~Agent_Async() override {
    // Make sure the worker has finished before our state goes away,
    // otherwise it would be left holding dangling references.
    if (is_thinking && result.valid()) result.wait();
  }

  void message(const std::string&) override {}

  int choose_action(Game& game, const Choice& choice) override {
    if (!is_thinking) {
      // First frame for this choice: spawn the worker. Game is captured by
      // reference because the caller keeps it alive on the main thread and
      // promises not to mutate it. Choice is captured by value as a copy keeps
      // the worker safe for the whole duration of the async computation.
      Agent* worker_agent = agent;
      Choice choice_copy  = choice;
      result =
        std::async(std::launch::async, [worker_agent, &game, choice_copy]() {
          return worker_agent->choose_action(game, choice_copy);
        });
      is_thinking = true;
      return -1;
    }
    // Not done yet -> tell the game loop to come back next frame.
    if (result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      return -1;
    }
    // Worker finished: deliver its action and re-arm for the next choice.
    int action_index = result.get();
    is_thinking      = false;
    return action_index;
  }
};
#else
struct Agent_Async : Agent {
  Agent* inner;

  explicit Agent_Async(Agent* inner) : inner(inner) {}

  void message(const std::string&) override {}

  int choose_action(Game& game, const Choice& choice) override {
    // Async not supported in Emscripten build: just call the inner agent
    // directly and block until it returns. The game loop will hitch while
    // it's thinking, but that's unavoidable without threads.
    return inner->choose_action(game, choice);
  }
};
#endif