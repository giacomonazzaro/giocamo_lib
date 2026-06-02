#pragma once

#include <nlohmann/json.hpp>
#include <online/models.h>
#include <tabletop/input_recorder.h>
#include <tabletop/tabletop.h>

#include <functional>
#include <optional>

#include <game/agent.h>

#include "menu.h"

// SPACE-to-zoom: zooms the thing under the cursor while SPACE is held,
// clears the zoom otherwise. tabletop's process_input also handles this
// internally; calling both is harmless (idempotent).
void update_zoomed_thing(Table_State& table_state, const Input& input);

// Snapshot the children of every thing in `table_state` to a JSON array
// (indexed by thing id). Used by the online sync to replicate the scene
// tree across peers.
nlohmann::json serialize_stacks(const Table_State& table_state);

// Apply a previously-serialized stacks array onto `table_state`, updating
// each affected thing's children and re-laying out its slots.
void apply_stacks_message(Table_State& table_state, const nlohmann::json& arr);

// Wrap serialize_stacks in a {"type": "stacks", "stacks": [...]} envelope
// and send it on `online`.
void send_stacks(const Online& online, const Table_State& table_state);

// Resolve the play mode in priority order:
//   1. `--local-host` / `--local-join` on argv → loopback handshake; no menu.
//   2. `skip_menu == true` → default Menu_Result (mode = VS_AI, no online).
//   3. Otherwise opens the menu and returns the user's choice.
// The returned Menu_Result owns its `online` field (valid only when
// mode == ONLINE).
Menu_Result resolve_play_mode(
  const std::string& title,
  int                window_width,
  int                window_height,
  Input_Feed&        inputs,
  int                argc,
  char**             argv,
  bool               skip_menu
);

// Wrap a local Agent into the right duel for the chosen mode:
//   - ONLINE: pairs `local_agent` with an Agent_Remote (via make_online_duel).
//   - Otherwise: returns Agent_Duel(local_agent, opponent, /*swap=*/seat1).
// For hot-seat callers pass `opponent == local_agent`; for vs-AI pass the
// already-built AI agent (caller decides any Agent_Async wrapping).
Agent* make_duel(
  Agent*             local_agent,
  Agent*             opponent,
  const Menu_Result& menu_result
);

// Blocking game-over overlay: keeps redrawing the table dimmed and prints
// `result_text` plus the two scores. Returns when the window is closed.
void draw_game_over_screen(
  Table_State&            table_state,
  const std::string&      result_text,
  const std::vector<int>& scores
);

// Parsed command-line options shared by every game app.
//   --hot-seat   → vs_ai=false, skip_menu=true (one screen, two players).
//   --skip-menu  → skip the menu, default to vs-AI.
//   --seed=N     → deterministic deal for solo play.
struct Play_Options {
  bool               vs_ai     = true;
  bool               skip_menu = false;
  std::optional<int> seed;
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

// Standard game loop. Runs the table-top interactive loop until the window
// closes or the game ends; on game-over draws the result screen.
//
// `state`          — game state (subclass of Game).
// `table`          — fully built Table_State (cards + stacks + root).
// `agent`          — the agent driving both seats (typically via Agent_Duel).
// `input_feed`     — input source (live, record, or playback).
// `window_title`   — used if `run_tabletop` needs to open the window itself.
// `sync_table`     — invoked after every resolved Choice; the game-specific
//                    callback typically copies the game's stacks into the
//                    matching `table.things[...].children`.
// `compute_scores` — returns the per-player final score for the game-over
//                    screen. Skipped (no screen) if null.
void play_game(
  Game&                             state,
  Table_State&                      table,
  Agent&                            agent,
  Input_Feed&                       input_feed,
  const std::string&                window_title,
  std::function<void()>             sync_table,
  std::function<std::vector<int>()> compute_scores
);
