#include "ui.h"

#include <scopa/gameplay.h>
#include <tabletop/config.h>
#include <tabletop/tabletop.h>
#include <tabletop/rendering.h>

#include <string>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

// Suit name in Italian.
static const char* suit_name(scopa::Suit suit) {
  switch (suit) {
    case scopa::Suit::COPPE: return "Coppe";
    case scopa::Suit::DENARI: return "Denari";
    case scopa::Suit::SPADE: return "Spade";
    case scopa::Suit::BASTONI: return "Bastoni";
  }
  return "";
}

// Suit accent color for the rank/suit label on the card face.
static Color suit_color(scopa::Suit suit) {
  switch (suit) {
    case scopa::Suit::COPPE: return Color{180, 50, 70, 255};
    case scopa::Suit::DENARI: return Color{210, 170, 30, 255};
    case scopa::Suit::SPADE: return Color{70, 110, 190, 255};
    case scopa::Suit::BASTONI: return Color{80, 150, 80, 255};
  }
  return Color{0, 0, 0, 255};
}

// Rank labels: 1-7 numeric, 8-10 face-card names.
static const char* rank_label(int rank) {
  switch (rank) {
    case 1: return "Asso";
    case 2: return "2";
    case 3: return "3";
    case 4: return "4";
    case 5: return "5";
    case 6: return "6";
    case 7: return "7";
    case 8: return "Fante";
    case 9: return "Cavallo";
    case 10: return "Re";
  }
  return "?";
}

std::vector<Thing> make_scopa_stacks(
  int bottom_player, bool show_opponent_hand
) {
  const int window_width  = tt::WINDOW_WIDTH;
  const int window_height = tt::WINDOW_HEIGHT;
  const int card_width    = tt::CARD_WIDTH;
  const int card_height   = tt::CARD_HEIGHT;
  const int margin        = 30;
  const int hand_spread   = card_width;
  const int pile_spread   = -3;
  const int table_spread  = card_width + 20;
  // Plenty of space for up to 9 cards in a hand.
  const int hand_width  = hand_spread * 8 + card_width;
  // Table can grow to a dozen-ish cards in extreme runs; size for 10.
  const int table_width = table_spread * 9 + card_width;

  // Layout in root-local coords: root is centered on the screen, so the
  // window spans (-W/2, -H/2) to (W/2, H/2) in root-local space.
  Rectangle window = Rectangle{
    -(float)window_width / 2.0f,
    -(float)window_height / 2.0f,
    (float)window_width,
    (float)window_height,
  };

  // Layout: bottom-hand, top-hand, both captured piles to the side, stock
  // off to one side and the table strip down the middle of the screen.
  Rectangle bottom_hand_rect =
    place_inside(window, hand_width, card_height, "center", "bottom", margin);
  Rectangle top_hand_rect =
    place_inside(window, hand_width, card_height, "center", "top", margin);
  Rectangle bottom_captured_rect = place_next(
    bottom_hand_rect, card_width, card_height, "right", "center", margin
  );
  Rectangle top_captured_rect = place_next(
    top_hand_rect, card_width, card_height, "left", "center", margin
  );
  Rectangle stock_rect = place_next(
    top_hand_rect, card_width, card_height, "right", "center", margin
  );
  Rectangle table_rect =
    place_inside(window, table_width, card_height, "center", "center", 0);

  // Seat-indexed rects: index by player id, not by screen position.
  const int top_player = 1 - bottom_player;
  Rectangle hand_rect[2];
  Rectangle captured_rect[2];
  hand_rect[bottom_player]     = bottom_hand_rect;
  hand_rect[top_player]        = top_hand_rect;
  captured_rect[bottom_player] = bottom_captured_rect;
  captured_rect[top_player]    = top_captured_rect;

  auto make = [](Rectangle rect, float spread_x, float spread_y, bool face_up,
                 const char* name) {
    Thing thing;
    set_local_rect(thing, rect);
    thing.spread_x = spread_x;
    thing.spread_y = spread_y;
    thing.face_up  = face_up;
    thing.name     = name;
    return thing;
  };

  // Local hand is always face-up; opponent's depends on show_opponent_hand.
  bool face_up_0 = (bottom_player == 0) ? true : show_opponent_hand;
  bool face_up_1 = (bottom_player == 1) ? true : show_opponent_hand;

  return {
    make(hand_rect[0], (float)hand_spread, 0.0f, face_up_0, "p0_hand"),
    make(hand_rect[1], (float)hand_spread, 0.0f, face_up_1, "p1_hand"),
    make(captured_rect[0], 0.0f, (float)pile_spread, false, "p0_captured"),
    make(captured_rect[1], 0.0f, (float)pile_spread, false, "p1_captured"),
    make(stock_rect, 0.0f, (float)pile_spread, false, "stock"),
    make(table_rect, (float)table_spread, 0.0f, true, "table"),
  };
}

std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const scopa::Game_State& state, UI_State& ui_state, int id
) {
  return
    [&state, &ui_state, id](const Table_State&, const Input&, bool face_up) {
      if (!face_up) return;
      // Drawn in card-local space where the card center is at (0, 0).
      const scopa::Card& card        = state.all_cards[id];
      const char*        rank_str    = rank_label(card.rank);
      const char*        suit_str    = suit_name(card.suit);
      Color              text_color  = {255, 255, 255, 255};
      int                card_width  = tt::CARD_WIDTH;
      int                card_height = tt::CARD_HEIGHT;
      float              half_h      = (float)card_height / 2.0f;
      float              half_w      = (float)card_width / 2.0f;
      int                rank_size   = (card.rank < 8) ? 56 : 32;
      int                rank_w      = text_width(rank_str, rank_size);
      render_text(
        rank_str,
        (float)(-rank_w / 2),
        card_height * 0.18f - half_h,
        rank_size,
        text_color
      );
      int suit_size = 22;
      int suit_w    = text_width(suit_str, suit_size);
      render_text(
        suit_str,
        (float)(-suit_w / 2),
        card_height * 0.55f - half_h,
        suit_size,
        text_color
      );

      // The 7 of Denari (Settebello) gets a small star marker near the
      // top-left corner.
      if (card.rank == 7 && card.suit == scopa::Suit::DENARI) {
        render_text("*", 8.0f - half_w, 8.0f - half_h, 28, Color{255, 215, 0, 255});
      }

      // Highlight border for legal-to-play / selectable cards.
      if (ui_state.highlighted_things.count(id) > 0) {
        DrawRectangleRoundedLinesEx(
          Rectangle{-half_w, -half_h, (float)card_width, (float)card_height},
          0.18f,
          8,
          4.0f,
          Color{255, 215, 0, 200}
        );
      }
    };
}

void draw_scopa_player_hud(
  const scopa::Game_State& state,
  int                      player_index,
  bool                     is_current,
  int                      hud_y
) {
  int total    = scopa::compute_player_score(state, player_index);
  int primiera = scopa::compute_primiera(state, player_index);
  int denari   = 0;
  for (int card_id : state.players[player_index].captured) {
    if (state.all_cards[card_id].suit == scopa::Suit::DENARI) ++denari;
  }
  int cards = (int)state.players[player_index].captured.size();
  int scope = state.players[player_index].scope;

  std::string label = "Player " + std::to_string(player_index + 1) + ": " +
                      std::to_string(total) + "   cards " +
                      std::to_string(cards) + "  denari " +
                      std::to_string(denari) + "  primiera " +
                      std::to_string(primiera) + "  scope " +
                      std::to_string(scope);
  Color text_color = is_current ? Color{200, 200, 200, 255}
                                : Color{120, 120, 120, 200};
  render_text(label, 30.0f, (float)hud_y, 24, text_color);
}

