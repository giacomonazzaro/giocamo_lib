#pragma once

#include <game/agent.h>

#include <utility>

#ifdef __EMSCRIPTEN__
#include "online_stub.h"

// Stub agents for Emscripten builds; online play is disabled.
struct Agent_Remote : Agent {
  explicit Agent_Remote(UDP_Socket*) {}
  int choose_action(Game& state, const Choice& choice) override { return 0; }
};

struct Agent_Local_Online : Agent {
  Agent_Local_Online(Agent*, UDP_Socket*, std::pair<std::string, int>) {}
  int choose_action(Game& state, const Choice& choice) override { return 0; }
};

#else
#include <online/protocol.h>

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
#endif
