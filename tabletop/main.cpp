// Demo of the tabletop library.
// Three containers (a deck pile, a hand spread, an empty discard zone) hold
// 16 blank cards in four colors. Use the mouse to drag cards between
// containers. Press S to shuffle a container. Hold SPACE to zoom a card. Press
// R to rotate.

#include "config.h"
#include "raylib.h"
#include "rendering.h"
#include "tabletop.h"

// Convert a desired world position to a transform in root-local space.
// Root is centered at (WINDOW_WIDTH/2, WINDOW_HEIGHT/2), so every container
// position is expressed as an offset from that center.
static Transform2D root_local_from_world(float world_x, float world_y) {
  return Transform2D{
    world_x - tt::WINDOW_WIDTH / 2.0f,
    world_y - tt::WINDOW_HEIGHT / 2.0f,
    0.0f,
  };
}

// Build the demo scene: 16 colored cards distributed across three containers.
static Table_State make_demo_table() {
  auto table            = Table_State();
  table.is_drop_allowed = [](int, int, int) { return true; };

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

  // 16 blank cards with sequential ids (0-15).
  for (int index = 0; index < 16; index++) {
    Thing card = make_card(index);
    card.color = card_colors[index % 4];
    table.things.push_back(card);
    table.draw_callbacks[index] = draw_card_border;
  }

  // Deck: a stacked pile of 8 cards on the left.
  const int deck_id = 16;
  {
    Thing deck;
    deck.id        = deck_id;
    deck.name      = "Deck";
    deck.capacity  = -1;
    deck.size      = {(float)tt::CARD_WIDTH + 20, (float)tt::CARD_HEIGHT + 20};
    deck.spread_x  = 0.0f;
    deck.spread_y  = (float)tt::PILE_SPREAD_Y;
    deck.color     = {80, 80, 80, 80};
    deck.transform = root_local_from_world(220, 400);
    for (int index = 0; index < 8; index++) deck.children.push_back(index);
    table.things.push_back(deck);
  }

  // Hand: 8 cards fanned out horizontally at the bottom.
  const int hand_id = 17;
  {
    Thing hand;
    hand.id        = hand_id;
    hand.name      = "Hand";
    hand.capacity  = -1;
    hand.size      = {800.0f, (float)tt::CARD_HEIGHT + 20};
    hand.spread_x  = (float)tt::HAND_SPREAD_X;
    hand.spread_y  = 0.0f;
    hand.color     = {60, 100, 60, 80};
    hand.transform = root_local_from_world(850, 800);
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
    discard.size = {(float)tt::CARD_WIDTH + 20, (float)tt::CARD_HEIGHT + 20};
    discard.spread_x  = 0.0f;
    discard.spread_y  = (float)tt::PILE_SPREAD_Y;
    discard.color     = {100, 60, 60, 80};
    discard.transform = root_local_from_world(1480, 400);
    table.things.push_back(discard);
  }

  // Root: full-screen invisible container that owns the three zones.
  const int root_id = 19;
  {
    Thing root;
    root.id        = root_id;
    root.name      = "root";
    root.size      = {(float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT};
    root.transform = {tt::WINDOW_WIDTH / 2.0f, tt::WINDOW_HEIGHT / 2.0f, 0.0f};
    root.children  = {deck_id, hand_id, discard_id};
    table.things.push_back(root);
    table.root = root_id;
  }

  // Lay out cards into their initial slot positions inside each container.
  update_children_positions(deck_id, table, false);
  update_children_positions(hand_id, table, false);

  return table;
}

int main() {
  // Request 4x multisampling so rotated card edges aren't jaggy. Must be set
  // before InitWindow — the flag is read when the GL context is created.
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "Tabletop Demo");
  SetTargetFPS(tt::TARGET_FPS);

  auto table = make_demo_table();

  while (!WindowShouldClose()) {
    auto input = capture_input();
    process_input(table, input);

    BeginDrawing();
    draw_background(input);
    draw_table(table, input);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
