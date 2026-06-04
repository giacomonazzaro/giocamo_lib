#include <dot/cards.h>
#include <dot/models.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "ui.h"

// Make a row parent centered horizontally at the given y, laying its cards
// out left to right. The caller fills in `children` and lays them out.
static Thing make_row(float center_y, int card_count) {
  const float spread   = (float)tt::CARD_WIDTH + 20.0f;
  float       width    = (float)(card_count - 1) * spread + (float)tt::CARD_WIDTH;
  Rectangle   rect     = {
    -width / 2.0f,
    center_y - (float)tt::CARD_HEIGHT / 2.0f,
    width,
    (float)tt::CARD_HEIGHT,
  };
  Thing row;
  set_local_rect(row, rect);
  row.spread_x = spread;
  row.face_up  = true;
  return row;
}

static Table_State init_table_state(const std::vector<dot::Card>& deck) {
  Table_State table;
  table.is_drop_allowed = [](int, int, int) { return false; };

  // One Thing per card; ids 0..17 match the deck indices. Give each a cream
  // face so the colored dots stand out.
  for (const dot::Card& card : deck) {
    Thing thing = make_card(card.id);
    thing.color = {235, 225, 205, 255};
    table.things.push_back(thing);
    table.draw_callbacks[card.id] = make_dot_card_draw_callback(deck, card.id);
  }

  // Lay the 15 draw cards in three rows of five, and the 3 star cards in a
  // fourth row below them.
  const float row_pitch = (float)tt::CARD_HEIGHT + 30.0f;
  const float first_y   = -1.5f * row_pitch;
  std::vector<int> row_ids;
  for (int row = 0; row < 4; row++) {
    int   card_count = (row < 3) ? 5 : 3;
    Thing row_thing  = make_row(first_y + (float)row * row_pitch, card_count);
    row_thing.id     = (int)table.things.size();
    for (int i = 0; i < card_count; i++) {
      row_thing.children.push_back(row * 5 + i);
    }
    row_ids.push_back(row_thing.id);
    table.things.push_back(row_thing);
  }

  auto root = create_table_root(
    tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
  );
  root.id       = (int)table.things.size();
  root.children = row_ids;
  table.things.push_back(root);
  table.root = root.id;

  for (int row_id : row_ids) update_children_positions(row_id, table, false);
  return table;
}

int main(int argc, char** argv) {
  // Optional --seed=N; both players would share this same deck.
  int seed = 0;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg.rfind("--seed=", 0) == 0) seed = std::atoi(arg.c_str() + 7);
  }

  std::vector<dot::Card> deck  = dot::make_deck(seed);
  Table_State            table = init_table_state(deck);

  // No game logic yet: just show the cards until the window is closed.
  run_tabletop(
    table,
    [](Table_State&, const Input&) { return false; },
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    "D.O.T"
  );
  return 0;
}
