#pragma once

#include <functional>
#include <vector>

#include <scopa/models.h>
#include <tabletop/input.h>
#include <tabletop/models.h>
#include <tabletop/ui.h>

// Stack offsets from num_cards (add num_cards to get thing_ids).
constexpr int SCOPA_HAND_0       = 0;
constexpr int SCOPA_HAND_1       = 1;
constexpr int SCOPA_CAPTURED_0   = 2;
constexpr int SCOPA_CAPTURED_1   = 3;
constexpr int SCOPA_STOCK_IDX    = 4;
constexpr int SCOPA_TABLE_IDX    = 5;

// Build the 6 stack Things for a Scopa table layout. `bottom_player` is the
// seat (0 or 1) whose hand sits at the bottom of the screen. The stack
// ordering (HAND_0, HAND_1, …) stays aligned with the player index so
// agent code can look up by seat without remapping; only the rects/face_up
// flip.
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
  const scopa::Game_State& state,
  int                      player_index,
  bool                     is_current,
  int                      hud_y
);

// Game-over screen: blocks the main loop drawing until the window is closed.
void draw_scopa_game_over_screen(
  Table_State& table_state, const std::vector<int>& scores
);
