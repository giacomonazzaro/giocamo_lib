// Demo of the tabletop library.
// Three containers (a deck pile, a hand spread, an empty discard zone) hold
// 16 blank cards in four colors. Use the mouse to drag cards between
// containers. Press S to shuffle a container. Hold SPACE to zoom a card. Press
// R to rotate.

#include "config.h"
#include "raylib.h"
#include "rendering.h"
#include "rlgl.h"
#include "tabletop.h"
#include "ui.h"

// Build the demo scene: 16 colored cards distributed across three containers.
static Table_State make_demo_table() {
  auto table            = Table_State();
  table.is_drop_allowed = [](int, int, int) { return true; };

  // Root's rectangle expressed in its own local space: origin sits at the
  // root's center, so the rect spans -W/2..W/2 horizontally and -H/2..H/2
  // vertically. This is the parent rect passed to place_inside() when
  // anchoring containers against the window edges.
  Rectangle root_rect = {
    -(float)tt::WINDOW_WIDTH / 2.0f,
    -(float)tt::WINDOW_HEIGHT / 2.0f,
    (float)tt::WINDOW_WIDTH,
    (float)tt::WINDOW_HEIGHT,
  };

  const int card_slot_width  = tt::CARD_WIDTH + 20;
  const int card_slot_height = tt::CARD_HEIGHT + 20;
  const int edge_padding     = 100;

  // Four distinct colors for the blank cards.
  Color card_colors[] = {
    {180, 60, 60, 255},   // Red.
    {60, 80, 180, 255},   // Blue.
    {50, 150, 80, 255},   // Green.
    {180, 160, 40, 255},  // Yellow.
  };

  // Draw a dark rounded border centered at origin (called after draw_thing,
  // with the world transform already pushed onto the matrix stack).
  auto draw_card_border = [](const Table_State&, const Input&, bool) {
    float width            = (float)tt::CARD_WIDTH;
    float height           = (float)tt::CARD_HEIGHT;
    float radius           = (float)tt::CARD_CORNER_RADIUS;
    Color border_color     = {20, 20, 20, 255};
    float border_thickness = 5.0f;
    DrawRectangleRoundedLinesEx(
      Rectangle{-width / 2.0f, -height / 2.0f, width, height},
      radius / std::min(width, height),
      8,
      border_thickness,
      border_color
    );
  };

  // 16 blank cards with sequential ids (0-15). Each card carries a counter
  // (value 1..16, range 0..16) so its number is drawn centered inside it.
  for (int index = 0; index < 16; index++) {
    Thing card    = make_card(index);
    card.color    = card_colors[index % 4];
    card.counter  = {index + 1, 0, 16};
    table.things.push_back(card);
    table.draw_callbacks[index] = draw_card_border;
  }

  // Deck: a stacked pile of 8 cards on the left.
  const int deck_id = 16;
  {
    Thing deck;
    deck.id       = deck_id;
    deck.name     = "Deck";
    deck.capacity = -1;
    deck.spread_x = 0.0f;
    deck.spread_y = (float)tt::PILE_SPREAD_Y;
    deck.color    = {80, 80, 80, 80};
    // Anchored to the left edge of the window, vertically centered. Sets
    // both deck.size and deck.transform from the resulting rect.
    set_local_rect(
      deck,
      place_inside(
        root_rect,
        card_slot_width,
        card_slot_height,
        "left",
        "center",
        edge_padding
      )
    );
    for (int index = 0; index < 8; index++) deck.children.push_back(index);
    table.things.push_back(deck);
  }

  // Hand: 8 cards fanned out horizontally at the bottom.
  const int hand_id = 17;
  {
    Thing hand;
    hand.id       = hand_id;
    hand.name     = "Hand";
    hand.capacity = -1;
    hand.spread_x = (float)tt::HAND_SPREAD_X;
    hand.spread_y = 0.0f;
    hand.color    = {60, 100, 60, 80};
    // Centered horizontally, anchored to the bottom edge of the window.
    set_local_rect(
      hand,
      place_inside(
        root_rect, 800, card_slot_height, "center", "bottom", edge_padding
      )
    );
    for (int index = 8; index < 16; index++) hand.children.push_back(index);
    table.things.push_back(hand);
  }

  // Discard pile: an empty drop zone on the right.
  const int discard_id = 18;
  {
    Thing discard;
    discard.id       = discard_id;
    discard.name     = "Discard";
    discard.capacity = -1;
    discard.spread_x = 0.0f;
    discard.spread_y = (float)tt::PILE_SPREAD_Y;
    discard.color    = {100, 60, 60, 80};
    // Mirrors the deck: anchored to the right edge, vertically centered.
    set_local_rect(
      discard,
      place_inside(
        root_rect,
        card_slot_width,
        card_slot_height,
        "right",
        "center",
        edge_padding
      )
    );
    table.things.push_back(discard);
  }

  // Root: full-screen invisible container that owns the three zones.
  const int root_id = 19;
  {
    Thing root;
    root.id        = root_id;
    root.name      = "root";
    root.shape     = rectangle_shape({(float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT});
    root.transform = {tt::WINDOW_WIDTH / 2.0f, tt::WINDOW_HEIGHT / 2.0f, 0.0f};
    root.children  = {deck_id, hand_id, discard_id};
    // Wooden table surface filling the whole window (no rounded corners).
    root.shape.rectangle.corner_radius = 0.0f;
    root.image_path                    = "tabletop/data/wood.png";
    table.things.push_back(root);
    table.root = root_id;
  }

  // Lay out cards into their initial slot positions inside each container.
  update_children_positions(deck_id, table, false);
  update_children_positions(hand_id, table, false);

  return table;
}

int main() {
  auto table = make_demo_table();
  run_tabletop(
    table,
    // Per-frame update. UP / DOWN change the value of the counter on the thing
    // under the cursor, clamped to its [min, max] range. Returns false: the
    // demo never ends itself; closing the window stops the loop.
    [](Table_State& table, const Input& input) {
      int delta = 0;
      if (key_pressed(input, KEY_UP)) delta = 1;
      if (key_pressed(input, KEY_DOWN)) delta = -1;
      if (delta != 0) {
        Thing_Location hovered =
          find_thing_at((float)input.mouse_x, (float)input.mouse_y, table);
        if (!hovered.empty()) {
          Thing& thing = table.things[hovered.back()];
          if (thing.counter) {
            int value = thing.counter.value + delta;
            thing.counter.value =
              std::max(thing.counter.min, std::min(thing.counter.max, value));
          }
        }
      }
      return false;
    },
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    "Tabletop Demo"
  );
}
