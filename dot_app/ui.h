#pragma once

#include <dot/models.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <functional>
#include <vector>

// Build the table stacks (rows and side piles). The local player
// (`bottom_player`) is laid out along the bottom; the opponent at the top. The
// opponent's hand is shown only in hot-seat (`show_opponent_hand`). Stacks are
// named "shared", "play_area", and "p<seat>_hand" / "_pool" / "_draw" /
// "_star"; game code finds them with find_thing().
std::vector<Thing> make_dot_stacks(int bottom_player, bool show_opponent_hand);

// Face renderer for the card with the given id: three rows of dots (blue,
// black, red) plus a marker on star cards, and a highlight border when the
// card is selectable. Drawn in card-local space (center at 0, 0).
std::function<void(const Table_State&, const Input&, bool)>
make_dot_card_draw_callback(
  const std::vector<dot::Card>& cards, UI_State& ui_state, int id
);

// Heads-up display: round number, tokens available, and each player's pool
// dot totals and tokens won. `local_seat` is shown as "You".
void draw_dot_hud(const dot::Game_State& state, int local_seat);
