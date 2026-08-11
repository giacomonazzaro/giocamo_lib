#pragma once

#include <mindbug/models.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <functional>
#include <string>
#include <vector>

// Face decoration for one card: the power it has right now while it is in play
// (auras change it), a mark when it is exhausted, and the border of a card the
// pending choice can take.
std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const Game_State& state, int card, bool highlighted = false
);

// Whose turn it is. `local_seat` is "You".
void draw_mindbug_hud(const Game_State& state, int local_seat);
