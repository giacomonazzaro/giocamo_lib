#pragma once

#include <iostream>
#include <random>
#include <string>

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
