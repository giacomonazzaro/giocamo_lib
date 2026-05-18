#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>

#include "models.h"

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

// Sends data reliably to the peer (non-blocking). Retried in the background
// until ACK'd.
void send_message(const Online& online, const nlohmann::json& data);

// Blocks until a message arrives from the peer; returns the JSON payload.
nlohmann::json recv_message(const Online& online);

// Non-blocking receive; returns nullopt if no message is available.
std::optional<nlohmann::json> try_recv_message(const Online& online);
