#pragma once

#include <game/agent.h>
#include <online/models.h>
#include <tabletop/input_recorder.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>

#include "menu.h"

// SPACE-to-zoom: zooms the thing under the cursor while SPACE is held,
// clears the zoom otherwise. tabletop's process_input also handles this
// internally; calling both is harmless (idempotent).
void update_zoomed_thing(Table_State& table_state, const Input& input);

// Snapshot the children of every thing in `table_state` to a JSON array
// (indexed by thing id). Used by the online sync to replicate the scene
// tree across peers.
nlohmann::json serialize_table_state(const Table_State& table_state);

// Apply a previously-serialized table_state array onto `table_state`, updating
// each affected thing's children and re-laying out its slots.
void apply_table_state_message(
  Table_State& table_state, const nlohmann::json& arr
);

// Wrap serialize_table_state in a {"type": "table_state", "table_state": [...]}
// envelope and send it on `online`.
void send_table_state(const Online& online, const Table_State& table_state);

// Resolve the play mode in priority order:
//   1. `--local-host` / `--local-join` on argv → loopback handshake; no menu.
//   2. `skip_menu == true` → default Menu_Result (mode = VS_AI, no online).
//   3. Otherwise opens the menu and returns the user's choice.
// The returned Menu_Result owns its `online` field (valid only when
// mode == ONLINE). `cli_seed` is folded into the result's seed for solo
// play; online uses the matchmaker's seed instead.
Menu_Result run_menu(
  const std::string& title,
  int                window_width,
  int                window_height,
  Input_Feed&        inputs,
  int                argc,
  char**             argv,
  bool               skip_menu,
  int                cli_seed
);

// Wrap a local Agent into the right duel for the chosen mode:
//   - ONLINE: pairs `local_agent` with an Agent_Remote (via make_online_duel).
//   - Otherwise: returns Agent_Duel(local_agent, opponent, /*swap=*/seat1).
// For hot-seat callers pass `opponent == local_agent`; for vs-AI pass the
// already-built AI agent (caller decides any Agent_Async wrapping).
Agent* make_duel(
  Agent* local_agent, Agent* opponent, const Menu_Result& menu_result
);

// Parsed command-line options shared by every game app.
//   --hot-seat        → vs_ai=false, skip_menu=true (one screen, two players).
//   --skip-menu       → skip the menu, default to vs-AI.
//   --seed=N          → deterministic deal for solo play. When omitted, the
//                       parser generates a random seed so the field always has
//                       a value.
//   --record=PATH     → play live and write the input stream to PATH.
//   --playback=PATH   → replay the input stream in PATH instead of the mouse.
//   --load[=PATH]     → start from the game saved on disk, not from a deal.
//                       Without a path the game reads its usual one.
struct Play_Options {
  bool        vs_ai      = true;
  bool        skip_menu  = false;
  int         seed       = 0;
  Input_Mode  input_mode = Input_Mode::Live;
  std::string input_file_path;  // Where to write it, or where to read it.
  bool        load_from_disk = false;
  std::string load_path      = "data/debug_game_state.json";

  // --local-host / --local-join.
  std::optional<Online_Connection> local_connection;
};
Play_Options parse_play_args(int argc, char** argv);

// Wrap the AI in Agent_Async when playing vs the computer, then build the duel
// for the current play mode. For hot-seat, pass `local_agent` itself as the
// opponent (it plays both seats).
Agent* make_agent_pair(
  Agent*             local_agent,
  Agent*             ai_opponent,
  const Menu_Result& menu_result,
  bool               vs_ai
);

struct Agent_UI : Agent {
  Table_State table;
  // The frame being drawn, so choose_action can read the mouse. The loop sets
  // it before asking the agent for a move.
  const Input* input = nullptr;
};

struct Giocamo {
  Game&        game;
  Agent_UI&    agent_ui;
  Table_State& table;

  int  bottom_player;
  bool hot_seat;

  Giocamo(Game& game, Agent_UI& agent_ui)
      : game(game), agent_ui(agent_ui), table(agent_ui.table) {}

  virtual ~Giocamo()        = default;
  virtual void init_table() = 0;
  // Read the game back from disk instead of dealing one, for --load. `path` is
  // empty unless one was given, and the game reads its usual one. Returns false
  // when there is nothing to read, and play_game deals instead.
  virtual bool   load_game(const std::string& path) { return false; }
  virtual void   draw(const Input& input) {}
  virtual void   update_table_from_game() = 0;
  virtual void   update_game_from_table() {}
  virtual Agent* agent_opponent() = 0;
  virtual Agent* agent_player() { return &agent_ui; }
  virtual std::vector<int>
               player_scores() const = 0;  // TODO(giacomo): not needed
  virtual void on_message(const nlohmann::json& msg) {}
};

// Standard game loop. Initializes the game with the seat's seed, runs the
// table-top loop until the window closes or the game ends, then draws the
// result screen.
void play_game(
  Giocamo&           giocamo,
  UI_State&          ui_state,
  Agent&             agent,
  Input_Feed&        input_feed,
  const Menu_Result& menu_result,
  const std::string& window_title
);

// Standard game loop. Runs the table-top interactive loop until the window
// closes or the game ends; on game-over draws the result screen.
//
// `state`          — game state (subclass of Game).
// `table`          — fully built Table_State (cards + table_state + root).
// `ui_state`       — shared UI state; play_game clears its highlighted_things
//                    when toggling playground mode on so stale "legal move"
//                    borders don't linger over a paused game.
// `agent`          — the agent driving both seats (typically via Agent_Duel).
// `input_feed`     — input source (live, record, or playback).
// `window_title`          — used if `run_tabletop` opens the window itself.
// `update_table_from_game`— invoked after every resolved Choice; copies the
//                           game state into the matching table children.
// `compute_scores`        — per-player final score for the game-over screen.
//                           Skipped (no screen) if null.
// `update_game_from_table`— invoked when leaving playground: reads the
//                           rearranged table back into the game state so play
//                           resumes from it. If null, the table is instead
//                           restored from the game via update_table_from_game.
// `on_message`            — handles online messages play_game doesn't itself
//                           recognize (anything but
//                           "playground"/"table_state").
void play_game(
  Game&                                      state,
  Table_State&                               table,
  UI_State&                                  ui_state,
  Agent&                                     agent,
  Input_Feed&                                input_feed,
  const Menu_Result&                         menu_result,
  const std::string&                         window_title,
  std::function<void()>                      update_table_from_game,
  std::function<std::vector<int>()>          compute_scores,
  std::function<void()>                      update_game_from_table = nullptr,
  std::function<void(const nlohmann::json&)> on_message             = nullptr
);

void play_game(
  Giocamo& giocamo, Play_Options& options, const std::string& window_title
);