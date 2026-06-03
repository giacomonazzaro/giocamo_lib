#pragma once

#include <vector>

#include <connect_four/models.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// Cell pitch and disc size (a bit smaller than the cell, for spacing).
constexpr int CONNECT_FOUR_CELL = 130;
constexpr int CONNECT_FOUR_DISC = 110;

// Build the 7 column Things: a centered grid of translucent blue columns, one
// per board column. Column `col` ends up at thing-id columns_offset + col once
// the columns are appended to the table. Each column owns the discs dropped
// into it as children.
std::vector<Thing> make_connect_four_columns();

// Fill color for a board cell value (connect_four::P0 -> red, P1 -> yellow).
Color connect_four_disc_color(int cell);

// HUD: whose turn it is, or the winner once the game ends.
void draw_connect_four_hud(const connect_four::Game_State& state);
