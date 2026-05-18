#pragma once

#include <tabletop/models.h>
#include <tabletop/ui.h>

#include <string>
#include <vector>

// Build the 11 standard stack things for a Gods table (deck/hand/discard/
// wonders/peoples per player + shared_deck). Each returned Thing has its
// layout fields set (rect, spread_x, spread_y, face_up, name); ids and
// children are filled in by the caller when appending to state.things.
std::vector<Thing> make_gods_stacks(int bottom_player = 0);

// Resolve a card name to its image path under gods/cards/fronts/, or empty
// string if no matching image exists on disk.
std::string get_image_path(const std::string& card_name);

// Draw the power badge in the top-right corner of a card. If `destroyed` is
// true, overlays a darkening rounded rectangle. Coordinates are relative — the
// caller is responsible for translating to the card origin first.
void draw_card_power_badge(const std::string& power, bool destroyed);

// HUD: per-player score panel anchored at hud_y on the right side of the
// screen.
void draw_player_hud(
  int player_id, int score, int deck_count, bool is_current, int hud_y
);

// Game-over screen: blocks the main loop drawing until the window is closed.
void draw_game_over_screen(
  Table_State&                    table_state,
  const std::string&              result_text,
  const std::vector<std::string>& names,
  const std::vector<int>&         scores
);

struct Gods_UI : UI_State {
  bool playground         = false;
  int  power_edit_card_id = -1;  // Card whose power is being edited.

  Gods_UI(int width, int height) : UI_State(width, height) {}
};