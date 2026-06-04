#pragma once

#include <functional>
#include <vector>

#include <dot/models.h>
#include <tabletop/tabletop.h>

// Face renderer for the card with the given id: draws three horizontal rows
// of dots (blue, black, red, top to bottom) and a marker on star cards.
// Drawn in card-local space where the card center is at (0, 0).
std::function<void(const Table_State&, const Input&, bool)>
make_dot_card_draw_callback(const std::vector<dot::Card>& deck, int id);
