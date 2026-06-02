#pragma once

#include <iostream>
#include <random>
#include <string>

#ifndef __EMSCRIPTEN__
#include <chrono>
#include <future>
#endif

#include "game.h"

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
    const auto start_time   = std::chrono::steady_clock::now();
    const int  action_index = inner->choose_action(game, choice);
    const auto end_time     = std::chrono::steady_clock::now();
    total_seconds +=
      std::chrono::duration<double>(end_time - start_time).count();
    num_calls += 1;
    return action_index;
  }

  double average_seconds_per_move() const {
    if (num_calls == 0) return 0.0;
    return total_seconds / (double)num_calls;
  }
};

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