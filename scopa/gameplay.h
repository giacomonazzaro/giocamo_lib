#pragma once

#include <game/game.h>

#include <optional>
#include <vector>

#include "models.h"

namespace scopa {

// Enumerate every legal (card_from_hand, capture_subset) action for the
// current player. The exact-match rule is enforced: if the played card has
// any same-rank match on the table, only those single-card matches are
// returned; sum captures are forbidden in that case. A played card with no
// possible capture produces a single action with an empty capture list
// (the card will be laid on the table).
std::vector<Action> enumerate_actions(const Game_State& state);

// Sort a player's hand by suit then rank for stable rendering.
void sort_hand(Game_State& state, int player_index);

// Apply `action` for the current player:
//   - Remove the played card from the player's hand.
//   - If the action has captures, remove those cards from the table and
//     append played + captured to the player's captured pile. If the table
//     is left empty by a non-final play, the player scores a Scopa.
//   - If the action has no captures, place the played card on the table.
//   - Advance current_player. When both hands are empty and stock has 18
//     cards left, deal 9 cards to each player as the second hand. When the
//     stock is also empty and hands are empty, give any remaining table
//     cards to the last player who captured and end the round.
void apply_action(Game_State& state, const Action& action);

// Round-end score, integer:
//   +1 if you took more cards than the opponent (carte).
//   +1 if you took more denari than the opponent (denari).
//   +1 for capturing the 7 of denari (settebello).
//   +1 if your primiera total beats the opponent's (primiera).
//   +1 per scopa scored during the round.
// Ties on carte / denari / primiera award no point to either side.
int compute_player_score(const Game_State& state, int player_index);

// Total primiera value summed across the 4 suits, used internally by
// compute_player_score and exposed for testing.
int compute_primiera(const Game_State& state, int player_index);

// Deal the starting position: 4 cards face-up on the table, 9 to each
// player, 18 in the stock. Player 0 leads.
Game_State quick_setup(std::optional<int> seed = std::nullopt);

}  // namespace scopa
