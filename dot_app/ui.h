#pragma once

#include <functional>
#include <vector>

#include <dot/models.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// The stacks on the table, in the order make_dot_stacks() creates them. Their
// thing-ids are this index plus the number of card Things (state.all_cards).
enum Dot_Stack {
  DOT_POOL_1,     // Opponent's pool (spread out, so you can pick from it).
  DOT_SHARED,     // The shared pool for the current round.
  DOT_PLAY_AREA,  // Where the local player drags cards before committing.
  DOT_HAND_0,     // Local player's hand (drag source for the split).
  DOT_POOL_0,     // Local player's pool (a side pile; totals shown in the HUD).
  DOT_HAND_1,     // Opponent's hand (face-down pile).
  DOT_DRAW_0,
  DOT_STAR_0,
  DOT_DRAW_1,
  DOT_STAR_1,
  DOT_STACK_COUNT,
};

// Build the table stacks (rows and side piles) in Dot_Stack order. The local
// player (`bottom_player`) is laid out along the bottom; the opponent at the
// top. The opponent's hand is shown only in hot-seat (`show_opponent_hand`).
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
