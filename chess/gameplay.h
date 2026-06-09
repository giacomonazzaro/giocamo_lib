#pragma once

#include <game/game.h>

#include "models.h"

namespace chess {

// A single move. `from` and `to` are squares (row*8 + col). `promotion` is the
// Piece type a pawn turns into on reaching the last rank, or 0 for any other
// move.
struct Move {
  int from;
  int to;
  int promotion = 0;
};

// Move lists stay inline (no heap allocation) for any reasonable position: a
// chess position has at most 218 legal moves but rarely more than ~40, and
// legal_moves runs on every simulated ply during search.
using Move_List = Inlined_Vector<Move, 128>;

// All fully legal moves for the player to move: pseudo-legal moves with any
// that would leave the mover's own king in check removed. Both next_choice and
// the UI agent call this, so a chosen index maps to the same move on each side.
Move_List legal_moves(const Game_State& state);

// True if `square` is attacked by any piece of `by_player`.
bool is_square_attacked(const Game_State& state, int square, int by_player);

// True if `player`'s king is currently attacked.
bool in_check(const Game_State& state, int player);

// Apply `move` (assumed legal): move the piece, handle castling / en passant /
// promotion, update castling rights, en-passant target and the 50-move clock,
// then either end the game (checkmate, stalemate, or draw) or pass the turn.
void apply_move(Game_State& state, const Move& move);

// 1 if `player` has won, else 0. Feeds the game-over score line.
int compute_player_score(const Game_State& state, int player);

// Standard starting position with white to move. The seed is unused (chess has
// no randomness); it matches the other games' setup signature.
Game_State quick_setup(int seed = 0);

}  // namespace chess
