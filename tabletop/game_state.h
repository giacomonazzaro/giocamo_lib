#pragma once
#include "models.h"

// Tree navigation helpers (scene tree is rooted at state.things[state.root]).
// These walk the parent chain — prefer state.animated_world for cached
// world-space lookups in hot paths.
int       find_parent(int thing_id, const Table_State& state);
Vector2   local_to_world(int thing_id, const Table_State& state);
Rectangle world_rect(int thing_id, const Table_State& state);

// Reflow a stack's children into their slot positions based on spread.
void update_card_positions(int stack_id, Table_State& state, bool sort);
