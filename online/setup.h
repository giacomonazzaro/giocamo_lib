#pragma once
#include <memory>
#include <optional>
#include <string>

#include "models.h"
#include "protocol.h"

// Start hosting a game; returns a Connection_State that the UI polls each
// frame. When ready == true the game can start.
std::shared_ptr<Connection_State> start_hosting(bool local = false);

// Join a hosted game by room code. Returns a Connection_State polled the same
// way as start_hosting.
std::shared_ptr<Connection_State> join_room(
  const std::string& room_code, bool local = false
);

// Park a UDP socket's shared_ptr in a process-lifetime list. Callers that hand
// an `Online` (which holds a raw `UDP_Socket*`) up the stack should call this
// so the socket stays alive without the caller having to track ownership.
void retain_socket(std::shared_ptr<UDP_Socket> sock);

// Ready-to-play connection plus the seat info each peer needs for `Game_State`
// setup. Mirrors what `Menu_Result` carries for an online match.
struct Online_Connection {
  Online online;
  int    player_index = 0;
  int    seed         = 0;
};

// Loopback handshake for local testing. host==true binds to 127.0.0.1:port and
// waits for the joiner; host==false connects to 127.0.0.1:port. Both peers must
// use the same port. Synchronous — blocks until the handshake completes.
// Bypasses STUN, ntfy, and hole-punching; no internet required.
Online_Connection setup_local(bool host, int port);

// CLI shortcut for setup_local: scans argv for `--local-host[=PORT]` or
// `--local-join[=PORT]` (default port 38800). Returns the resulting connection
// if a flag matched, or nullopt — the caller falls back to its normal flow.
// Argument parsing lives here so apps don't have to touch argv themselves.
std::optional<Online_Connection> setup_local_from_argv(int argc, char** argv);
