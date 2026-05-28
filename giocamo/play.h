#pragma once

#include <nlohmann/json.hpp>
#include <online/models.h>
#include <tabletop/tabletop.h>

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
