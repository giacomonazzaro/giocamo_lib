#pragma once

#include <game_cpp/agent.h>
#include <online_cpp/code/protocol.h>

#include <utility>

// Receives opponent's action indices from the server.
struct Agent_Remote : Agent {
  UDP_Socket* sock;

  explicit Agent_Remote(UDP_Socket* s) : sock(s) {}

  int choose_action(Game& state, const Choice& choice) override;
};

// Wraps a local agent and sends chosen action indices to the peer.
struct Agent_Local_Online : Agent {
  Agent*                      local_agent;
  UDP_Socket*                 sock;
  std::pair<std::string, int> friend_addr;

  Agent_Local_Online(Agent* a, UDP_Socket* s, std::pair<std::string, int> addr)
      : local_agent(a), sock(s), friend_addr(std::move(addr)) {}

  int choose_action(Game& state, const Choice& choice) override;
};
