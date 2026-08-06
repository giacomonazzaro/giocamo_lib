#pragma once

#include "tabletop.h"

// Side panel showing the properties of the thing the cursor was last over, and
// letting you edit them. The panel is built by walking Thing's visitable
// fields, so a field added to VISITABLE_STRUCT(Thing, ...) shows up on its own,
// with no change here.
//
// Call this from the per-frame update, inside run_tabletop's draw pass.
void draw_ui(Table_State& table, const Input& input);
