#pragma once

// Network-glue agents that adapt a local Agent to a peer-to-peer match. Live
// here in online/ so any app (gods, tressette, ...) can opt in to online
// play without re-implementing the same wrapper classes. Compiles on both
// native and wasm because the underlying ntfy.sh transport is portable.

#include <game/agent.h>

#include "protocol.h"

// Receives the opponent's action index from the peer. Non-blocking: returns
// -1 each frame until a message arrives, so the game loop stays responsive.
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
// Agent_Local_Online, and the opponent seat is an Agent_Remote.
// `player_index` is which seat (0 or 1) the local player owns.
Agent* make_online_duel(
  Agent* local_agent, const Online& online, int player_index
);
