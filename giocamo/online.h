#pragma once

#include <game/agent.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

// Online duel over a Firebase Realtime Database, reached with plain HTTPS
// requests. There is no SDK and no server of our own: the two players only
// have to agree on a room code.
//
// A match is one room. Each player writes its messages into a numbered list
// under /rooms/<code>/host or /rooms/<code>/join, and reads the other list.
// The numbers are what makes the match safe:
//   - A write that fails is written again to the same slot, so a message is
//     never lost and never lands twice.
//   - A reader that finds a gap waits for it instead of reading past it, so
//     messages always arrive in the order they were sent.
// Everything is stored, so a player who reloads the page reads the whole
// match back.
//
// giocamo/firebase_setup.md says how to make the database.

// Which side of a room this player is. This is a plain value, copied into
// the agents; the message queues live in online.cpp, keyed by room code and
// seat.
struct Online {
  std::string room_code;
  // "host" for the player who created the room, "join" for the other one.
  std::string seat;
};

// Put a message in line for the other player. Returns at once; the message
// is written in the background and written again until the database takes
// it.
void send_message(const Online& online, const nlohmann::json& message);

// Take the next message from the other player, or nothing if none has
// arrived yet. `only_type == "action"` reads the queue of moves; any other
// value reads the queue of everything else. The two queues are separate
// because the game loop and Agent_Remote both read from here, and the loop
// would otherwise take the moves the agent is waiting for.
std::optional<nlohmann::json> try_recv_message(
  const Online& online, const std::string& only_type = ""
);

// A match being set up. The menu calls tick() once per frame and reads the
// fields to draw itself.
struct Connection_State {
  std::string room_code;
  bool        ready        = false;
  int         player_index = 0;
  int         seed         = 0;
  Online      online;
  // Set when nobody answers on that room code. The menu shows it and goes
  // back to the online screen.
  std::string error;

  bool                                  is_host    = false;
  bool                                  said_hello = false;
  std::chrono::steady_clock::time_point give_up_at;

  // Move the handshake forward by one step. Cheap: it limits its own
  // polling, so calling it every frame is fine.
  void tick();
};

// Create a room. The returned pointer stays valid until the next call to
// start_hosting or join_room; the caller does not own it.
Connection_State* start_hosting();

// Enter a room somebody else created. Same rules for the pointer.
Connection_State* join_room(const std::string& room_code);

// What a finished handshake gives the game.
struct Online_Connection {
  Online online;
  int    player_index = 0;
  int    seed         = 0;
};

// Two instances on one machine meet in a room called "local", so neither
// player has to type a code. Blocks until both are in.
Online_Connection setup_local(bool host);

// Command-line shortcut for setup_local: looks for `--local-host` or
// `--local-join` in argv. Returns nothing when neither is there, so the
// caller falls through to the normal menu.
std::optional<Online_Connection> setup_local_from_argv(int argc, char** argv);

// Reads the other player's move. Never blocks: returns -1 every frame until
// a move arrives, so the game loop keeps drawing.
struct Agent_Remote : Agent {
  Online online;

  explicit Agent_Remote(const Online& online) : online(online) {}

  int choose_action(Game& state, const Choice& choice) override;
};

// Wraps a local agent and sends the move it picks to the other player.
struct Agent_Local_Online : Agent {
  Agent* local_agent;
  Online online;

  Agent_Local_Online(Agent* local_agent, const Online& online)
      : local_agent(local_agent), online(online) {}

  int choose_action(Game& state, const Choice& choice) override;
};

// Build an Agent_Duel where this player's seat is `local_agent` wrapped in
// Agent_Local_Online and the other seat is an Agent_Remote. `player_index`
// is the seat this player owns, 0 or 1.
Agent* make_online_duel(
  Agent* local_agent, const Online& online, int player_index
);
