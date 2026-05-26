#pragma once
#include <memory>
#include <optional>
#include <string>

#include "models.h"
#include "protocol.h"

// Start hosting a game. Returns immediately with a Connection_State whose
// room_code is already populated; the menu calls tick() each frame and
// reads ready / error / etc to drive its UI.
std::shared_ptr<Connection_State> start_hosting(bool local = false);

// Join a hosted match by room code. Same polling contract as start_hosting.
std::shared_ptr<Connection_State> join_room(
  const std::string& room_code, bool local = false
);

// Bundle that drops out of a completed handshake; mirrors Menu_Result for an
// online match.
struct Online_Connection {
  Online online;
  int    player_index = 0;
  int    seed         = 0;
};

// Loopback handshake for local testing — both peers share a fixed room
// code so two instances on the same machine meet without typing a code.
// Synchronous: blocks until the handshake completes. host==true posts the
// hello first, host==false replies.
Online_Connection setup_local(bool host);

// CLI shortcut for setup_local: scans argv for `--local-host` /
// `--local-join`. Returns nullopt if neither flag is present so the caller
// can fall through to the normal menu flow.
std::optional<Online_Connection> setup_local_from_argv(int argc, char** argv);
