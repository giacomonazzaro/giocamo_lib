#pragma once

// Network-glue agents that adapt a local Agent to a peer-to-peer match. Live
// here in online/ so any app (gods, tressette, …) can opt in to online play
// without re-implementing the same wrapper classes. Safe to include
// unconditionally — Emscripten builds get no-op stubs.

#include <game/agent.h>

#ifdef __EMSCRIPTEN__
#include "online_stub.h"
#else
#include "protocol.h"
#endif

#ifdef __EMSCRIPTEN__

// Stub agents for Emscripten builds; online play is disabled.
struct Agent_Remote : Agent {
  explicit Agent_Remote(const Online&) {}
  int choose_action(Game&, const Choice&) override { return 0; }
};

struct Agent_Local_Online : Agent {
  Agent_Local_Online(Agent*, const Online&) {}
  int choose_action(Game&, const Choice&) override { return 0; }
};

inline Agent* make_online_duel(Agent* local_agent, const Online&, int) {
  return local_agent;
}

#else

// Receives the opponent's action index from the peer.
struct Agent_Remote : Agent {
  Online online;

  explicit Agent_Remote(const Online& o) : online(o) {}

  int choose_action(Game& state, const Choice& choice) override;
};

// Wraps a local agent and forwards its chosen action index to the peer.
struct Agent_Local_Online : Agent {
  Agent* local_agent;
  Online online;

  Agent_Local_Online(Agent* a, const Online& o) : local_agent(a), online(o) {}

  int choose_action(Game& state, const Choice& choice) override;
};

// Build an Agent_Duel where the local seat is `local_agent` wrapped in
// Agent_Local_Online, and the opponent seat is an Agent_Remote. `player_index`
// is which seat (0 or 1) the local player owns — used to set the duel's swap
// so each peer drives its own seat. Caller owns the returned Agent_Duel*; the
// inner Agent_Local_Online and Agent_Remote are leaked deliberately (process
// exits when the game ends — see gods_app for the same convention).
Agent* make_online_duel(
  Agent* local_agent, const Online& online, int player_index
);

#endif
