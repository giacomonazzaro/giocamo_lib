#pragma once
#include "models.h"

// Tree navigation helpers (scene tree is rooted at state.things[state.root]).
// These walk the parent chain — prefer state.animated_world for cached
// world-space lookups in hot paths.
int       find_parent(int thing_id, const Table_State& state);
Vector2   local_to_world(int thing_id, const Table_State& state);
Rectangle world_rect(int thing_id, const Table_State& state);

// Reflow a thing's children into their slot positions based on the parent's
// spread_x / spread_y.
void update_children_positions(int parent_id, Table_State& state, bool sort);

Thing make_card(int id, const std::string& image_path = "");