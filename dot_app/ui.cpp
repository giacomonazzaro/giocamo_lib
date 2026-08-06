#include "ui.h"

#include <dot/gameplay.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>

#include <string>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

// Dot colors for the three rows.
static const Color BLUE_DOT  = {70, 130, 200, 255};
static const Color BLACK_DOT = {45, 45, 45, 255};
static const Color RED_DOT   = {200, 70, 70, 255};

// Slots in the vector make_dot_stacks() returns. Only the layout code below
// uses these; everything else addresses a stack by name.
namespace {
enum Dot_Stack {
  DOT_POOL_1,
  DOT_SHARED,
  DOT_PLAY_AREA,
  DOT_HAND_0,
  DOT_POOL_0,
  DOT_HAND_1,
  DOT_DRAW_0,
  DOT_STAR_0,
  DOT_DRAW_1,
  DOT_STAR_1,
  DOT_STACK_COUNT,
};
}  // namespace

// One table stack: a rectangle parent that lays its card children out by
// spread_x / spread_y. Rectangles are in root-local coords (origin centered).
static Thing make_stack(
  Rectangle          rect,
  float              spread_x,
  float              spread_y,
  bool               face_up,
  const std::string& name
) {
  Thing stack;
  set_local_rect(stack, rect);
  stack.spread_x = spread_x;
  stack.spread_y = spread_y;
  stack.face_up  = face_up;
  stack.name     = name;
  stack.color    = {255, 255, 255, 10};  // Invisible parent; only cards show.
  return stack;
}

std::vector<Thing> make_dot_stacks(int bottom_player, bool show_opponent_hand) {
  const float card_w = (float)tt::CARD_WIDTH;
  const float card_h = (float)tt::CARD_HEIGHT;
  const float row    = card_w + 10.0f;  // Spread for a fanned-out row.
  const float pile   = -4.0f;           // Spread for a near-flat pile.

  // A fanned-out row centered at (center_x, center_y), wide enough for
  // `max_cards` (it auto-shrinks if more are added).
  auto row_rect = [&](float center_x, float center_y, int max_cards) {
    float width = row * (float)(max_cards - 1) + card_w;
    return Rectangle{
      center_x - width / 2.0f, center_y - card_h / 2.0f, width, card_h
    };
  };
  auto pile_rect = [&](float center_x, float center_y) {
    return Rectangle{
      center_x - card_w / 2.0f, center_y - card_h / 2.0f, card_w, card_h
    };
  };

  // Stacks are indexed by player; their on-screen position depends on which
  // seat the local player owns. The local player sits along the bottom.
  const int top_player  = 1 - bottom_player;
  const int pool_idx[2] = {DOT_POOL_0, DOT_POOL_1};
  const int hand_idx[2] = {DOT_HAND_0, DOT_HAND_1};
  const int draw_idx[2] = {DOT_DRAW_0, DOT_DRAW_1};
  const int star_idx[2] = {DOT_STAR_0, DOT_STAR_1};

  // Stacks are named by seat ("p0_hand", "p1_hand", ...), so game code finds
  // the seat it means regardless of which seat is sitting at the bottom.
  auto seat_name = [](int seat, const char* zone) {
    return "p" + std::to_string(seat) + "_" + zone;
  };

  // Matching the rulebook: opponent pool on top, the shared pool in the
  // middle, your pool below it, and your hand along the bottom with the play
  // area beside it.
  std::vector<Thing> stacks(DOT_STACK_COUNT);
  // The shared pool stack is face up; individual shared cards are flipped
  // face-down per-card until both players commit (see update_table_from_game).
  stacks[DOT_SHARED] =
    make_stack(row_rect(0.0f, -130.0f, 6), row, 0.0f, true, "shared");
  stacks[DOT_PLAY_AREA] =
    make_stack(row_rect(560.0f, 391.0f, 3), row, 0.0f, true, "play_area");

  // Your pool (bottom) and the opponent's pool (top), both fanned out.
  stacks[pool_idx[bottom_player]] = make_stack(
    row_rect(0.0f, 130.0f, 8), row, 0.0f, true, seat_name(bottom_player, "pool")
  );
  stacks[pool_idx[top_player]] = make_stack(
    row_rect(0.0f, -391.0f, 8), row, 0.0f, true, seat_name(top_player, "pool")
  );

  // Your hand is a fanned-out row (drag source); the opponent's hand is a
  // face-down pile, shown face up only in hot-seat.
  stacks[hand_idx[bottom_player]] = make_stack(
    row_rect(-320.0f, 391.0f, 6),
    row,
    0.0f,
    true,
    seat_name(bottom_player, "hand")
  );
  stacks[hand_idx[top_player]] = make_stack(
    pile_rect(765.0f, -391.0f),
    0.0f,
    pile,
    show_opponent_hand,
    seat_name(top_player, "hand")
  );

  // Draw and star decks tucked into the side margins (yours left, theirs
  // right).
  stacks[draw_idx[bottom_player]] = make_stack(
    pile_rect(-765.0f, -130.0f),
    0.0f,
    pile,
    false,
    seat_name(bottom_player, "draw")
  );
  stacks[star_idx[bottom_player]] = make_stack(
    pile_rect(-765.0f, 130.0f),
    0.0f,
    pile,
    false,
    seat_name(bottom_player, "star")
  );
  stacks[draw_idx[top_player]] = make_stack(
    pile_rect(765.0f, -130.0f), 0.0f, pile, false, seat_name(top_player, "draw")
  );
  stacks[star_idx[top_player]] = make_stack(
    pile_rect(765.0f, 130.0f), 0.0f, pile, false, seat_name(top_player, "star")
  );
  return stacks;
}

// Draw `count` dots in a horizontal row centered on the card, at the given
// fraction of the card height above/below center (negative is up).
static void draw_dot_row(int count, float height_fraction, Color color) {
  const float radius  = 11.0f;
  const float spacing = 30.0f;
  float       y       = height_fraction * (float)tt::CARD_HEIGHT;
  float       start_x = -(float)(count - 1) * spacing / 2.0f;
  for (int i = 0; i < count; i++) {
    DrawCircle((int)(start_x + (float)i * spacing), (int)y, radius, color);
  }
}

std::function<void(const Table_State&, const Input&, bool)>
make_dot_card_draw_callback(
  const std::vector<dot::Card>& cards, UI_State& ui_state, int id
) {
  return
    [&cards, &ui_state, id](const Table_State&, const Input&, bool face_up) {
      if (!face_up) return;
      const dot::Card& card = cards[id];

      // Three rows of dots: blue on top, black in the middle, red at the
      // bottom.
      draw_dot_row(card.blue_dots, -0.28f, BLUE_DOT);
      draw_dot_row(card.black_dots, 0.0f, BLACK_DOT);
      draw_dot_row(card.red_dots, 0.28f, RED_DOT);

      float half_w = (float)tt::CARD_WIDTH / 2.0f;
      float half_h = (float)tt::CARD_HEIGHT / 2.0f;

      // A small star marker in the top-left corner distinguishes star cards.
      if (card.is_star) {
        render_text(
          "*", 8.0f - half_w, 4.0f - half_h, 40, Color{255, 215, 0, 255}
        );
      }

      // Highlight border on selectable cards.
      if (ui_state.highlighted_things.count(id) > 0) {
        DrawRectangleRoundedLinesEx(
          Rectangle{
            -half_w, -half_h, (float)tt::CARD_WIDTH, (float)tt::CARD_HEIGHT
          },
          0.18f,
          8,
          4.0f,
          Color{255, 215, 0, 220}
        );
      }
    };
}

// Sum of one color's dots over a set of cards. color: 0 blue, 1 black, 2 red.
static int pool_color_total(
  const dot::Game_State& state, array<const int> cards, int color
) {
  int total = 0;
  for (int id : cards) {
    const dot::Card& card = dot::all_cards[id];
    if (color == 0)
      total += card.blue_dots;
    else if (color == 1)
      total += card.black_dots;
    else
      total += card.red_dots;
  }
  return total;
}

// One player's line in the HUD: pool dot totals and tokens won.
static std::string player_line(const dot::Game_State& state, int player) {
  const dot::Player& p = state.players[player];
  return "pool b" + std::to_string(pool_color_total(state, p.pool, 0)) + " k" +
         std::to_string(pool_color_total(state, p.pool, 1)) + " r" +
         std::to_string(pool_color_total(state, p.pool, 2)) + "    tokens b" +
         std::to_string(p.tokens_blue) + " k" + std::to_string(p.tokens_black) +
         " r" + std::to_string(p.tokens_red) + " (" +
         std::to_string(dot::total_tokens(state, player)) + ")";
}

void draw_dot_hud(const dot::Game_State& state, int local_seat) {
  Color white = {235, 235, 235, 255};
  Color dim   = {160, 160, 160, 255};
  float x     = 16.0f;
  float y     = 16.0f;

  render_text(
    "D.O.T   Round " + std::to_string(state.round + 1) + "/3", x, y, 28, white
  );
  y += 34.0f;
  render_text(
    "Tokens up for grabs:  blue " + std::to_string(state.pending_blue) +
      "   black " + std::to_string(state.pending_black) + "   red " +
      std::to_string(state.pending_red),
    x,
    y,
    20,
    dim
  );
  y += 34.0f;
  render_text("You: " + player_line(state, local_seat), x, y, 20, white);
  y += 28.0f;
  render_text(
    "Opponent: " + player_line(state, 1 - local_seat), x, y, 20, white
  );
}
