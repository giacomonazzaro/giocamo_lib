#pragma once
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "models.h"

// Returns (or lazily creates) the Reliable_UDP_State for the given UDP_Socket.
Reliable_UDP_State& get_state(UDP_Socket& sock);

// Sends data reliably (non-blocking). Retried in the background until ACK'd.
void send_message(UDP_Socket& sock, const nlohmann::json& data, const std::pair<std::string, int>& addr);

// Blocks until a message arrives; returns the JSON payload.
nlohmann::json recv_message(UDP_Socket& sock);

// Non-blocking receive; returns nullopt if no message is available.
std::optional<nlohmann::json> try_recv_message(UDP_Socket& sock);

#ifdef ONLINE_BUILD_PYTHON
void bind_protocol(nb::module_& m);
#endif
