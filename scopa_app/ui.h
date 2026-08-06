#pragma once

#include <scopa/models.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <functional>
#include <vector>

// Build the 6 stack Things for a Scopa table layout. `bottom_player` is the
// seat (0 or 1) whose hand sits at the bottom of the screen. The stacks are
// named "p0_hand", "p1_hand", "p0_captured", "p1_captured", "stock" and
// "table"; game code finds them with find_thing(). Only the rects/face_up
// depend on which seat is at the bottom.
std::vector<Thing> make_scopa_stacks(
  int bottom_player, bool show_opponent_hand
);

// Draw callback that renders rank/suit text on each card face.
std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const scopa::Game_State& state, UI_State& ui_state, int id
);

// HUD: per-player score panel at hud_y, including a small breakdown of the
// four end-of-round point categories plus scope.
void draw_scopa_player_hud(
  const scopa::Game_State& state, int player_index, bool is_current, int hud_y
);
