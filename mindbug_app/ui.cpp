#include "ui.h"

#include <mindbug/gameplay.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>

#include <string>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const mindbug::Game_State& state, int card, bool highlighted
) {
  return [&state,
          card,
          highlighted](const Table_State&, const Input&, bool face_up) {
    if (!face_up) return;
    const float half_width  = (float)tt::CARD_WIDTH / 2.0f;
    const float half_height = (float)tt::CARD_HEIGHT / 2.0f;

    // Power is only worth showing while the card is in play, where auras and
    // the turn can push it away from the printed number.
    if (mindbug::is_in_play(state, card)) {
      const auto  power = std::to_string(mindbug::effective_power(state, card));
      const float badge_x = -half_width + 22.0f;
      const float badge_y = -half_height + 22.0f;
      DrawCircle((int)badge_x, (int)badge_y, 21.0f, ::Color{20, 20, 20, 235});
      const int size = 28;
      render_text(
        power,
        badge_x - (float)text_width(power, size) / 2.0f,
        badge_y - (float)size / 2.0f,
        size,
        Color{255, 255, 255, 255}
      );
      // The creature that is attacking right now, so the defender sees what
      // they are being asked to block.
      if (card == state.attacker) {
        DrawRectangleRoundedLinesEx(
          Rectangle{
            -half_width - 7.0f,
            -half_height - 7.0f,
            (float)tt::CARD_WIDTH + 14.0f,
            (float)tt::CARD_HEIGHT + 14.0f
          },
          0.18f,
          8,
          6.0f,
          Color{225, 60, 60, 255}
        );
      }

      // An exhausted creature has used up the save its Tough keyword gives
      // it.
      if (mindbug::is_exhausted(state, card)) {
        DrawRectangleRounded(
          Rectangle{
            -half_width,
            -half_height,
            (float)tt::CARD_WIDTH,
            (float)tt::CARD_HEIGHT
          },
          0.18f,
          8,
          ::Color{0, 0, 0, 110}
        );
      }
    }

    // The border of a card the pending choice can take. Part of the card's
    // face, so a card in front of it covers it like the rest of the card.
    if (highlighted) {
      DrawRectangleRoundedLinesEx(
        Rectangle{
          -half_width,
          -half_height,
          (float)tt::CARD_WIDTH,
          (float)tt::CARD_HEIGHT
        },
        0.18f,
        8,
        5.0f,
        Color{255, 215, 0, 230}
      );
    }
  };
}

// One player's line: life points. The Mindbugs are on the table.
static std::string player_line(const mindbug::Game_State& state, int player) {
  return "life " + std::to_string(state.players[player].life);
}

void draw_mindbug_hud(const mindbug::Game_State& state, int local_seat) {
  const Color white = {235, 235, 235, 255};
  const Color dim   = {160, 160, 160, 255};
  float       y     = 16.0f;

  render_text("Mindbug", 16.0f, y, 28, white);
  y += 36.0f;
  render_text("You: " + player_line(state, local_seat), 16.0f, y, 20, white);
  y += 26.0f;
  render_text(
    "Opponent: " + player_line(state, 1 - local_seat), 16.0f, y, 20, white
  );
  y += 26.0f;
  render_text(
    state.current_player == local_seat ? "Your turn" : "Opponent's turn",
    16.0f,
    y,
    20,
    dim
  );
}
