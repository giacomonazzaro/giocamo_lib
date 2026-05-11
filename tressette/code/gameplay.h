#pragma once

#include <nanobind/nanobind.h>

#include "models.h"

namespace nb = nanobind;

namespace tressette {

// Determine the winner of the current 2-card trick.
// state.trick must contain exactly 2 card ids; the first one is the led card
// played by state.trick_leader, the second by 1 - state.trick_leader.
// Returns the winning player index (0 or 1).
int trick_winner(const Game_State& state);

// Total points scored by player_index, integer (floored thirds + ultima bonus).
int compute_player_score(const Game_State& state, int player_index);

// Apply the play of card_id by the current player. Mirrors what Choice.resolve
// does and is exposed to Python so the UI can drive moves directly.
//   - Removes card_id from current player's hand and pushes onto trick.
//   - If trick is now complete, resolves it: assigns winner, draws from stock
//     (winner first, loser second), checks for game-over.
//   - Otherwise switches to the other player.
//   - Fires on_cards_changed once at the end.
void play_card(Game_State& state, int card_id);

// Finalize a trick that's been held on the table after both cards were
// played: hand cards to the winner's pile, draw from stock, advance the
// turn. No-op if pending_trick_resolve is false.
void resolve_pending_trick(Game_State& state);

}  // namespace tressette

void bind_gameplay(nb::module_& m);
