#pragma once

#include <functional>
#include <vector>

#include <tabletop/input.h>
#include <tabletop/models.h>
#include <tabletop/ui.h>
#include <tressette/models.h>

// Stack offsets from num_cards (add num_cards to get thing_ids).
constexpr int TRESSETTE_HAND_0    = 0;
constexpr int TRESSETTE_HAND_1    = 1;
constexpr int TRESSETTE_TRICKS_0  = 2;
constexpr int TRESSETTE_TRICKS_1  = 3;
constexpr int TRESSETTE_STOCK_IDX = 4;
constexpr int TRESSETTE_TABLE_IDX = 5;

// Build the 6 stack Things for a Tressette table layout. `bottom_player` is
// the seat (0 or 1) whose hand sits at the bottom of the screen — i.e. the
// local player. The stack ordering (HAND_0, HAND_1, …) stays aligned with the
// player index so agent code can look up by seat without remapping; only the
// rects/face_up flip.
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

// Game-over screen: blocks the main loop drawing until the window is closed.
void draw_tressette_game_over_screen(
  Table_State& table_state, const std::vector<int>& scores
);
