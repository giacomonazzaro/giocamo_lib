#pragma once

#include <game/game.h>

#include <vector>

#include "models.h"

namespace connect_four {

// Columns (ascending) that still have room for a disc. Both next_choice and the
// UI agent call this, so a chosen index maps to the same column on each side.
std::vector<int> legal_columns(const Game_State& state);

// Lowest empty row in `col`, or -1 if the column is full.
int drop_row(const Game_State& state, int col);

// Drop the current player's disc into `col` (must be non-full), update
// winner/game_over, and pass the turn when the game continues.
void apply_move(Game_State& state, int col);

// Winning player (0/1) if four line up horizontally, vertically, or on either
// diagonal; -1 otherwise.
int check_winner(const Game_State& state);

// 1 if `player` has won, else 0. Feeds the game-over score line.
int compute_player_score(const Game_State& state, int player);

// Fresh empty board with player 0 to move. The seed is unused (Connect Four has
// no randomness); it's accepted to match the other games' setup signature.
Game_State quick_setup(int seed = 0);

}  // namespace connect_four
