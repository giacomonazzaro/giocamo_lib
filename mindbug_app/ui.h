#pragma once

#include <mindbug/models.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <functional>
#include <string>
#include <vector>

// The table zones, laid out with the local player (`bottom_player`) along the
// bottom and the opponent at the top. Zones are named by seat — "p0_hand",
// "p1_creatures", "p0_draw", "p1_discard", "p0_mindbugs" — plus "played" for
// the creature waiting on a Mindbug decision. Game code finds them with
// find_thing().
std::vector<Thing> make_mindbug_stacks(
  int bottom_player, int window_width, int window_height
);

// Full path of a card's art, given the file name cards.json records for it.
std::string get_image_path(const std::string& image_file);

// Face decoration for one card: the power it has right now while it is in play
// (auras change it), a mark when it is exhausted, and the border of a card the
// pending choice can take.
std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const mindbug::Game_State& state, int card, bool highlighted = false
);

// Ask for a card to be drawn with the border that says the pending choice can
// take it. The border is part of the card's own face, so a card in front of it
// covers it like the rest of the card.
void highlight_card(
  Table_State& table, const mindbug::Game_State& state, int card
);

// Take that border off every card.
void clear_highlights(Table_State& table, const mindbug::Game_State& state);

// Life points and whose turn it is. `local_seat` is "You".
void draw_mindbug_hud(const mindbug::Game_State& state, int local_seat);

