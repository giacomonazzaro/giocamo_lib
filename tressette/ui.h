#pragma once
#include <functional>
#include <string>
#include <vector>

#include <tabletop/config.h>
#include <tabletop/input.h>
#include <tabletop/models.h>
#include <tabletop/rendering.h>
#include <tabletop/ui.h>
#include <tressette/models.h>

// Stack offsets from num_cards (add num_cards to get thing_ids).
constexpr int TRESSETTE_HAND_0    = 0;
constexpr int TRESSETTE_HAND_1    = 1;
constexpr int TRESSETTE_TRICKS_0  = 2;
constexpr int TRESSETTE_TRICKS_1  = 3;
constexpr int TRESSETTE_STOCK_IDX = 4;
constexpr int TRESSETTE_TABLE_IDX = 5;

// Suit display helpers.

inline static const char* suit_name(tressette::Suit s) {
  switch (s) {
    case tressette::Suit::COPPE:   return "Coppe";
    case tressette::Suit::DENARI:  return "Denari";
    case tressette::Suit::SPADE:   return "Spade";
    case tressette::Suit::BASTONI: return "Bastoni";
  }
  return "";
}

inline static Color suit_color(tressette::Suit s) {
  switch (s) {
    case tressette::Suit::COPPE:   return Color{180, 50,  70,  255};
    case tressette::Suit::DENARI:  return Color{210, 170, 30,  255};
    case tressette::Suit::SPADE:   return Color{70,  110, 190, 255};
    case tressette::Suit::BASTONI: return Color{80,  150, 80,  255};
  }
  return Color{0, 0, 0, 255};
}

// Rank labels: 1-7 numeric, 8-10 face-card names.
inline static const char* rank_label(int rank) {
  switch (rank) {
    case 1:  return "1";
    case 2:  return "2";
    case 3:  return "3";
    case 4:  return "4";
    case 5:  return "5";
    case 6:  return "6";
    case 7:  return "7";
    case 8:  return "Donna";
    case 9:  return "Cavallo";
    case 10: return "Re";
  }
  return "?";
}

// Build the 6 stack Things for a Tressette table layout.
inline std::vector<Thing> make_tressette_stacks(bool both_hands_visible) {
  const int W          = tt::WINDOW_WIDTH;
  const int H          = tt::WINDOW_HEIGHT;
  const int w          = tt::CARD_WIDTH;
  const int h          = tt::CARD_HEIGHT;
  const int margin     = 30;
  const int spread_hand = w;
  const int spread_pile = -3;
  const int hand_width = spread_hand * 9 + w;  // fits up to 10 cards.

  auto window = Rectangle{0.0f, 0.0f, (float)W, (float)H};

  Rectangle p0_hand_r   = place_inside(window, hand_width, h, "center", "bottom", margin);
  Rectangle p1_hand_r   = place_inside(window, hand_width, h, "center", "top",    margin);
  Rectangle p0_tricks_r = place_next(p0_hand_r, w, h, "right", "center", margin);
  Rectangle p1_tricks_r = place_next(p1_hand_r, w, h, "left",  "center", margin);
  Rectangle stock_r     = place_inside(window, w, h, "center", "center", 0);
  stock_r.x -= (float)(w * 3 / 2);
  Rectangle table_r     = place_inside(window, 2 * w + 30, h, "center", "center", 0);
  table_r.x += (float)(w * 2 / 5);

  auto make = [](Rectangle r, float sx, float sy, bool fu, const char* name) {
    Thing t;
    t.rect     = r;
    t.spread_x = sx;
    t.spread_y = sy;
    t.face_up  = fu;
    t.name     = name;
    return t;
  };

  return {
    make(p0_hand_r,   (float)spread_hand, 0.0f,            true,               "p0_hand"),
    make(p1_hand_r,   (float)spread_hand, 0.0f,            both_hands_visible, "p1_hand"),
    make(p0_tricks_r, 0.0f,               (float)spread_pile, false,           "p0_tricks"),
    make(p1_tricks_r, 0.0f,               (float)spread_pile, false,           "p1_tricks"),
    make(stock_r,     0.0f,               (float)spread_pile, false,           "stock"),
    make(table_r,     (float)(w + 30),    0.0f,            true,               "table"),
  };
}

// Draw callback that renders rank/suit text on each card face.
inline std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const tressette::Game_State& state, UI_State& ui_state, int id
) {
  return [&state, &ui_state, id](const Table_State&, const Input&, bool) {
    const tressette::Card& c    = state.all_cards[id];
    const char*            rlbl = rank_label(c.rank);
    const char*            slbl = suit_name(c.suit);
    Color                  col  = suit_color(c.suit);
    int                    w    = tt::CARD_WIDTH;
    int                    h    = tt::CARD_HEIGHT;

    // Big rank number/word near the top, horizontally centered.
    int rank_size = ((int)std::string(rlbl).size() == 1) ? 56 : 32;
    int tw        = text_width(rlbl, rank_size);
    render_text(rlbl, (float)(w / 2 - tw / 2), h * 0.18f, rank_size, col);

    // Suit name underneath.
    int suit_size = 22;
    int sw        = text_width(slbl, suit_size);
    render_text(slbl, (float)(w / 2 - sw / 2), h * 0.55f, suit_size, col);

    // Tiny rank in the bottom-right corner so fanned cards still show value.
    int small_size = 22;
    int rw         = text_width(rlbl, small_size);
    render_text(rlbl, (float)(w - rw - 10), (float)(h - small_size - 10), small_size, col);

    // Highlight border for legal cards.
    if (ui_state.highlighted_cards.count(id) > 0) {
      DrawRectangleRoundedLinesEx(
        Rectangle{0.0f, 0.0f, (float)w, (float)h},
        0.18f, 8, 4.0f,
        Color{255, 215, 0, 200}
      );
    }
  };
}

inline void draw_tressette_player_hud(
  int player_index, int score, bool is_current, int hud_y
) {
  std::string label =
    "Player " + std::to_string(player_index + 1) + ": " + std::to_string(score);
  Color col = is_current ? Color{200, 200, 200, 255} : Color{120, 120, 120, 200};
  render_text(label, 30.0f, (float)hud_y, 28, col);
}

inline void draw_tressette_game_over_screen(
  Table_State& table_state, const std::vector<int>& scores
) {
  const int   W          = tt::WINDOW_WIDTH;
  const int   H          = tt::WINDOW_HEIGHT;
  const char* title      = "GAME OVER";
  const char* msg =
    (scores[0] > scores[1]) ? "Player 1 wins!" :
    (scores[1] > scores[0]) ? "Player 2 wins!" : "It's a tie.";
  std::string score_line =
    std::to_string(scores[0]) + " - " + std::to_string(scores[1]);

  while (!WindowShouldClose()) {
    BeginDrawing();
    draw_background(0.0f);
    draw_table(table_state);
    DrawRectangle(0, 0, W, H, Color{0, 0, 0, 160});
    render_text(title,      (float)(W / 2 - text_width(title,      60) / 2), 320.0f, 60, Color{255, 255, 255, 255});
    render_text(msg,        (float)(W / 2 - text_width(msg,        36) / 2), 410.0f, 36, Color{255, 215, 0,   255});
    render_text(score_line, (float)(W / 2 - text_width(score_line, 30) / 2), 470.0f, 30, Color{200, 200, 200, 255});
    EndDrawing();
  }
}
