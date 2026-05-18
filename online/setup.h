#pragma once
#include <memory>
#include <string>

#include "models.h"

// Start hosting a game; returns a Connection_State that the UI polls each
// frame. When ready == true the game can start.
std::shared_ptr<Connection_State> start_hosting(bool local = false);

// Join a hosted game by room code. Returns a Connection_State polled the same
// way as start_hosting.
std::shared_ptr<Connection_State> join_room(
  const std::string& room_code, bool local = false
);
