#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "models.h"

// Publish a JSON message to the peer's recv topic via ntfy.sh. Blocking;
// happens at most once per turn so the cost is negligible.
void send_message(const Online& online, const nlohmann::json& data);

// Non-blocking receive. If `only_type` is "action", returns the next queued
// action message; otherwise returns the next non-action message ("hello",
// "all_cards", "stacks", "playground", ...). The split is necessary because
// the gods_app main loop and Agent_Remote both drain the inbox — without
// separate queues, the main loop would steal action messages.
std::optional<nlohmann::json> try_recv_message(
  const Online& online, const std::string& only_type = ""
);

// Blocking version of try_recv_message — used by the handshake. Polls in
// the background while yielding to the OS / JS event loop.
nlohmann::json recv_message(const Online& online);
