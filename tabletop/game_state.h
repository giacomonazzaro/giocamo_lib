#pragma once
#include "models.h"

Rectangle world_rect(int thing_id, const Table_State& state);

// Reflow a thing's children into their slot positions based on the parent's
// spread_x / spread_y.
void update_children_positions(int parent_id, Table_State& state, bool sort);

Thing make_card(int id, const std::string& image_path = "");