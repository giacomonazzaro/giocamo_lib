#pragma once

#include <game/agent.h>

#include <string>
#include <utility>

#ifdef __EMSCRIPTEN__
#include "online_stub.h"
#else
#include <online/protocol.h>
#endif

// The two things needed to talk to the peer over the network. Bundled so
// they can be passed as a single parameter through the play_gods/main call
// stack. A nullptr `const Online*` is the convention for "local-only";
// where Online itself is held by value, `sock == nullptr` means the same.
// Lifetime of `sock` is managed at the producer (menu.cpp keeps the socket
// alive for the process lifetime).
struct Online {
  UDP_Socket*                 sock = nullptr;
  std::pair<std::string, int> friend_addr;
};

#ifdef __EMSCRIPTEN__

// Stub agents for Emscripten builds; online play is disabled.
struct Agent_Remote : Agent {
  explicit Agent_Remote(const Online&) {}
  int choose_action(Game& state, const Choice& choice) override { return 0; }
};

struct Agent_Local_Online : Agent {
  Agent_Local_Online(Agent*, const Online&) {}
  int choose_action(Game& state, const Choice& choice) override { return 0; }
};

#else

// Receives opponent's action indices from the server.
struct Agent_Remote : Agent {
  Online online;

  explicit Agent_Remote(const Online& o) : online(o) {}

  int choose_action(Game& state, const Choice& choice) override;
};

// Wraps a local agent and sends chosen action indices to the peer.
struct Agent_Local_Online : Agent {
  Agent* local_agent;
  Online online;

  Agent_Local_Online(Agent* a, const Online& o) : local_agent(a), online(o) {}

  int choose_action(Game& state, const Choice& choice) override;
};
#endif
