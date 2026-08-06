#pragma once

#include <tabletop/tabletop.h>
#include <tabletop/ui.h>
#include <tressette/models.h>

#include <functional>
#include <vector>

// Build the 6 stack Things for a Tressette table layout. `bottom_player` is
// the seat (0 or 1) whose hand sits at the bottom of the screen — i.e. the
// local player. The stacks are named "p0_hand", "p1_hand", "p0_tricks",
// "p1_tricks", "stock" and "table"; game code finds them with find_thing().
// Only the rects/face_up depend on which seat is at the bottom.
std::vector<Thing> make_tressette_stacks(
  int bottom_player, bool show_opponent_hand
);

// Draw callback that renders rank/suit text on each card face.
std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const tressette::Game_State& state, UI_State& ui_state, int id
);

// HUD: per-player score panel at hud_y.
void draw_tressette_player_hud(
  int player_index, int score, bool is_current, int hud_y
);
