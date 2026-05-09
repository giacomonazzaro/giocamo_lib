#pragma once

#include <algorithm>
#include <iostream>
#include <iterator>
#include <random>
#include <string>

#include "game.h"
#include "minimax.h"

struct Agent {
  virtual ~Agent() = default;

  virtual void message(const std::string& msg) {
    std::cout << "Agent: " << msg << "\n";
  }

  // Pick an action index. Does NOT call resolve. Return -1 to indicate "not ready yet".
  virtual int choose_action(Game& game, const Choice& choice) = 0;
};

struct Agent_Random : Agent {
  std::mt19937 rng;

  Agent_Random() : rng(std::random_device{}()) {}
  explicit Agent_Random(std::uint32_t seed) : rng(seed) {}

  void message(const std::string&) override {}  // Silent agent.

  int choose_action(Game& state, const Choice& choice) override;
};

// Alpha-beta minimax. Templated on the concrete Game subclass so the search can
// copy state by value (no clone() / unique_ptr needed).
template <class Game_T>
struct Agent_Minimax : Agent {
  Evaluate_Fn<Game_T> evaluate;
  int                 max_depth;

  Agent_Minimax(Evaluate_Fn<Game_T> evaluate, int max_depth)
      : evaluate(std::move(evaluate)), max_depth(max_depth) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete    = static_cast<Game_T&>(state);
    const int num_actions = action_options_count(choice.actions(state));
    if (num_actions <= 0) return 0;
    std::vector<float> scores = minimax_search<Game_T>(
      concrete, evaluate, choice, num_actions, choice.player_index, max_depth
    );
    return static_cast<int>(
      std::distance(scores.begin(), std::max_element(scores.begin(), scores.end()))
    );
  }
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
