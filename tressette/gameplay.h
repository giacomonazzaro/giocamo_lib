#pragma once

#include <game/game.h>

#include <optional>
#include <vector>

#include "models.h"

namespace tressette {

// Determine the winner of the current 2-card trick.
// state.trick must contain exactly 2 card ids; the first one is the led card
// played by state.trick_leader, the second by 1 - state.trick_leader.
// Returns the winning player index (0 or 1).
int trick_winner(const Game_State& state);

// Sum of captured card values in thirds-of-a-point (un-floored). Used both for
// the final score and as a smooth mid-game heuristic.
int compute_player_thirds(const Game_State& state, int player_index);

// Total points scored by player_index, integer (floored thirds + ultima bonus).
int compute_player_score(const Game_State& state, int player_index);

// Returns the legal cards in the current player's hand for the next play.
// The responder must follow suit if possible.
Inlined_Vector<int, 16> legal_cards(const Game_State& state);

// Sort a player's hand by suit then rank.
void sort_hand(Game_State& state, int player_index);

// Apply the play of card_id by the current player.
//   - Removes card_id from current player's hand and pushes onto trick.
//   - If trick is now complete, resolves it: assigns winner, draws from stock
//     (winner first, loser second), checks for game-over.
//   - Otherwise switches to the other player.
//   - Fires on_cards_changed once at the end.
// void play_card(Game_State& state, int card_id);

// Deal a fresh hand: 10 cards each, 20 in the stock, player 0 leads.
Game_State quick_setup(std::optional<int> seed = std::nullopt);

}  // namespace tressette
