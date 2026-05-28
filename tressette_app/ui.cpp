#include "ui.h"

#include <tabletop/config.h>
#include <tabletop/tabletop.h>
#include <tabletop/rendering.h>

#include <string>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

// Suit name in Italian.
static const char* suit_name(tressette::Suit s) {
  switch (s) {
    case tressette::Suit::COPPE: return "Coppe";
    case tressette::Suit::DENARI: return "Denari";
    case tressette::Suit::SPADE: return "Spade";
    case tressette::Suit::BASTONI: return "Bastoni";
  }
  return "";
}

// Suit accent color used for the rank/suit text on the card face.
static Color suit_color(tressette::Suit s) {
  switch (s) {
    case tressette::Suit::COPPE: return Color{180, 50, 70, 255};
    case tressette::Suit::DENARI: return Color{210, 170, 30, 255};
    case tressette::Suit::SPADE: return Color{70, 110, 190, 255};
    case tressette::Suit::BASTONI: return Color{80, 150, 80, 255};
  }
  return Color{0, 0, 0, 255};
}

// Rank labels: 1-7 numeric, 8-10 face-card names.
static const char* rank_label(int rank) {
  switch (rank) {
    case 1: return "Asso";
    case 2: return "II";
    case 3: return "III";
    case 4: return "4";
    case 5: return "5";
    case 6: return "6";
    case 7: return "7";
    case 8: return "Donna";
    case 9: return "Cavallo";
    case 10: return "Re";
  }
  return "?";
}

std::vector<Thing> make_tressette_stacks(
  int bottom_player, bool show_opponent_hand
) {
  const int W           = tt::WINDOW_WIDTH;
  const int w           = tt::CARD_WIDTH;
  const int h           = tt::CARD_HEIGHT;
  const int margin      = 30;
  const int spread_hand = w;
  const int spread_pile = -3;
  const int hand_width  = spread_hand * 9 + w;  // fits up to 10 cards.

  // Layout in root-local coords: root is centered on the screen, so the
  // window spans (-W/2, -H/2) to (W/2, H/2) in root-local space.
  auto window = Rectangle{
    -(float)W / 2.0f,
    -(float)tt::WINDOW_HEIGHT / 2.0f,
    (float)W,
    (float)tt::WINDOW_HEIGHT,
  };

  // Position the local seat at the bottom and the opponent at the top.
  Rectangle bottom_hand_r =
    place_inside(window, hand_width, h, "center", "bottom", margin);
  Rectangle top_hand_r =
    place_inside(window, hand_width, h, "center", "top", margin);
  Rectangle bottom_tricks_r =
    place_next(bottom_hand_r, w, h, "right", "center", margin);
  Rectangle top_tricks_r =
    place_next(top_hand_r, w, h, "left", "center", margin);
  Rectangle stock_r = place_inside(window, w, h, "center", "center", 0);
  stock_r.x -= (float)(w * 3 / 2);
  Rectangle table_r =
    place_inside(window, 2 * w + 30, h, "center", "center", 0);
  table_r.x += (float)(w * 2 / 5);

  // Seat-indexed rects: index by player id, not by screen position.
  const int top_player = 1 - bottom_player;
  Rectangle hand_r[2];
  Rectangle tricks_r[2];
  hand_r[bottom_player]   = bottom_hand_r;
  hand_r[top_player]      = top_hand_r;
  tricks_r[bottom_player] = bottom_tricks_r;
  tricks_r[top_player]    = top_tricks_r;

  auto make = [](Rectangle r, float sx, float sy, bool fu, const char* name) {
    Thing t;
    set_local_rect(t, r);
    t.spread_x = sx;
    t.spread_y = sy;
    t.face_up  = fu;
    t.name     = name;
    return t;
  };

  // Local hand is always face-up; opponent's depends on show_opponent_hand.
  bool face_up_0 = (bottom_player == 0) ? true : show_opponent_hand;
  bool face_up_1 = (bottom_player == 1) ? true : show_opponent_hand;

  return {
    make(hand_r[0], (float)spread_hand, 0.0f, face_up_0, "p0_hand"),
    make(hand_r[1], (float)spread_hand, 0.0f, face_up_1, "p1_hand"),
    make(tricks_r[0], 0.0f, (float)spread_pile, false, "p0_tricks"),
    make(tricks_r[1], 0.0f, (float)spread_pile, false, "p1_tricks"),
    make(stock_r, 0.0f, (float)spread_pile, false, "stock"),
    make(table_r, (float)(w + 30), 0.0f, true, "table"),
  };
}

std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const tressette::Game_State& state, UI_State& ui_state, int id
) {
  return
    [&state, &ui_state, id](const Table_State&, const Input&, bool face_up) {
      if (!face_up) return;
      // Drawn in card-local space where the card center is at (0, 0).
      const tressette::Card& c      = state.all_cards[id];
      const char*            rlbl   = rank_label(c.rank);
      const char*            slbl   = suit_name(c.suit);
      Color                  col    = {255, 255, 255, 255};
      int                    w      = tt::CARD_WIDTH;
      int                    h      = tt::CARD_HEIGHT;
      float                  half_h = (float)h / 2.0f;
      float                  half_w = (float)w / 2.0f;
      // Big rank number/word near the top, horizontally centered.
      int rank_size = (c.rank < 8) ? 56 : 32;
      int tw        = text_width(rlbl, rank_size);
      render_text(rlbl, (float)(-tw / 2), h * 0.18f - half_h, rank_size, col);

      // Suit name underneath.
      int suit_size = 22;
      int sw        = text_width(slbl, suit_size);
      render_text(slbl, (float)(-sw / 2), h * 0.55f - half_h, suit_size, col);

      // Highlight border for legal cards.
      if (ui_state.highlighted_things.count(id) > 0) {
        DrawRectangleRoundedLinesEx(
          Rectangle{-half_w, -half_h, (float)w, (float)h},
          0.18f,
          8,
          4.0f,
          Color{255, 215, 0, 200}
        );
      }
    };
}

void draw_tressette_player_hud(
  int player_index, int score, bool is_current, int hud_y
) {
  std::string label = "Player " + std::to_string(player_index + 1) + ": " +
                      std::to_string(score);
  Color col = is_current ? Color{200, 200, 200, 255}
                         : Color{120, 120, 120, 200};
  render_text(label, 30.0f, (float)hud_y, 28, col);
}

