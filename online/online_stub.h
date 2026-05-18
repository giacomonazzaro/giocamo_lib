#pragma once
// Stub definitions used in Emscripten builds where the online module is excluded.
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>

struct UDP_Socket {};

struct Online {
  UDP_Socket*                 sock = nullptr;
  std::pair<std::string, int> friend_addr;
};

inline void send_message(const Online&, const nlohmann::json&) {}

inline std::optional<nlohmann::json> try_recv_message(const Online&) {
  return std::nullopt;
}

inline nlohmann::json recv_message(const Online&) {
  return {};
}
