#pragma once
#include <atomic>
#include <mutex>
#include <string>

// Everything online_lib needs at runtime to talk to a peer. We use ntfy.sh
// as a public HTTP relay; each match owns two topic names (one per
// direction). Same struct on native and wasm — only the underlying HTTP
// client (libcurl vs emscripten_fetch) differs.
struct Online {
  std::string topic_send;
  std::string topic_recv;
};

// Async matchmaking state, polled by the menu each frame via tick(). Holds
// everything the menu needs to render and what the game will eventually
// consume to start.
struct Connection_State {
  std::string       room_code;
  std::atomic<bool> ready{false};
  int               player_index = 0;
  int               seed         = 0;
  std::string       topic_send;
  std::string       topic_recv;
  std::string       error;
  std::mutex        state_lock;

  // Handshake bookkeeping.
  bool is_host    = false;
  long my_seed    = 0;
  bool sent_hello = false;

  // Advance the handshake by one step. Must be called from the menu loop
  // each frame; cheap and rate-limits its own HTTP polling internally.
  void tick();

  Connection_State()                                   = default;
  Connection_State(const Connection_State&)            = delete;
  Connection_State& operator=(const Connection_State&) = delete;
};
