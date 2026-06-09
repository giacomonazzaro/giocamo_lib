#pragma once

#include <vector>

#include <chess/models.h>
#include <tabletop/tabletop.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

// Square pitch in pixels; the board is 8 squares wide.
constexpr int CHESS_CELL = 90;

// Build the 64 square Things: a centered 8x8 grid of alternating light/dark
// squares. Square `row*8 + col` ends up at thing-id `row*8 + col` (the squares
// are appended first), so a square's id equals its board index.
std::vector<Thing> make_chess_squares();

// Single-letter glyph for a board value (K Q R B N P), or "" when empty.
const char* chess_piece_glyph(int piece);

// Text color for a board value: light for white pieces, dark for black.
Color chess_piece_color(int piece);

// HUD: whose turn it is / check / the result once the game ends.
void draw_chess_hud(const chess::Game_State& state);
