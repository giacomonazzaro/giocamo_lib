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
