#include "ui.h"

#include <mindbug/gameplay.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>

#include <string>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

// On desktop the art sits relative to the working directory; on web it is
// preloaded at an absolute path in MEMFS, and the emscripten working directory
// isn't guaranteed to be "/".
#ifdef __EMSCRIPTEN__
static const std::string IMAGES_DIR = "/mindbug/card-images";
#else
static const std::string IMAGES_DIR = "mindbug/card-images";
#endif

std::string get_image_path(const std::string& image_file) {
  if (image_file.empty()) return "";
  return IMAGES_DIR + "/" + image_file;
}

// One zone: a rectangle parent that spreads its cards out by spread_x /
// spread_y. Invisible itself; only the cards in it are drawn.
static Thing make_stack(
  Rectangle rect, float spread_x, float spread_y, bool face_up, std::string name
) {
  auto stack = Thing();
  set_local_rect(stack, rect);
  stack.spread_x = spread_x;
  stack.spread_y = spread_y;
  stack.face_up  = face_up;
  stack.name     = std::move(name);
  stack.color    = {255, 255, 255, 0};
  return stack;
}

std::vector<Thing> make_mindbug_stacks(
  int bottom_player, int window_width, int window_height
) {
  const int card_width  = tt::CARD_WIDTH;
  const int card_height = tt::CARD_HEIGHT;
  const int margin      = 24;
  const int fan         = 130;  // Spread of a row of cards.
  const int pile        = -3;   // Spread of a near-flat pile.
  const int row_width   = 6 * fan + card_width;

  // The root is centered on the screen, so the window spans
  // (-width/2, -height/2) to (width/2, height/2) in root-local coordinates.
  Rectangle window = {
    -(float)window_width / 2.0f,
    -(float)window_height / 2.0f,
    (float)window_width,
    (float)window_height
  };

  // Bottom seat: hand along the bottom edge, creatures in the half above it,
  // draw pile and discard pile out on the flanks.
  Rectangle hand =
    place_inside(window, row_width, card_height, "center", "bottom", margin);
  Rectangle creatures =
    place_next(hand, row_width, card_height, "center", "top", margin);
  Rectangle draw_pile =
    place_next(hand, card_width, card_height, "left", "center", margin);
  Rectangle discard =
    place_next(hand, card_width, card_height, "right", "center", margin);

  // The opponent's zones mirror them across the middle of the screen.
  auto mirrored = [](Rectangle rect) {
    return Rectangle{
      -rect.x - rect.width, -rect.y - rect.height, rect.width, rect.height
    };
  };

  // The creature waiting on a Mindbug decision sits in the middle, between the
  // two rows of creatures.
  Rectangle played =
    place_inside(window, card_width, card_height, "center", "center", 0);

  const int top_player = 1 - bottom_player;
  auto      zone_name  = [](int seat, const char* zone) {
    return "p" + std::to_string(seat) + "_" + zone;
  };

  // A draw pile is face down for both players; everything else is open.
  std::vector<Thing> stacks;
  stacks.push_back(
    make_stack(hand, fan, 0, true, zone_name(bottom_player, "hand"))
  );
  stacks.push_back(
    make_stack(creatures, fan, 0, true, zone_name(bottom_player, "creatures"))
  );
  stacks.push_back(
    make_stack(draw_pile, 0, pile, false, zone_name(bottom_player, "draw"))
  );
  stacks.push_back(
    make_stack(discard, 10, pile, true, zone_name(bottom_player, "discard"))
  );
  stacks.push_back(
    make_stack(mirrored(hand), fan, 0, true, zone_name(top_player, "hand"))
  );
  stacks.push_back(make_stack(
    mirrored(creatures), fan, 0, true, zone_name(top_player, "creatures")
  ));
  stacks.push_back(make_stack(
    mirrored(draw_pile), 0, pile, false, zone_name(top_player, "draw")
  ));
  stacks.push_back(make_stack(
    mirrored(discard), 10, pile, true, zone_name(top_player, "discard")
  ));
  stacks.push_back(make_stack(played, 0, pile, true, "played"));
  return stacks;
}

std::function<void(const Table_State&, const Input&, bool)>
make_card_draw_callback(
  const mindbug::Game_State&     state,
  const std::unordered_set<int>& highlighted_things,
  int                            card
) {
  return [&state, &highlighted_things, card](
           const Table_State&, const Input&, bool face_up
         ) {
      if (!face_up) return;
      const float half_width  = (float)tt::CARD_WIDTH / 2.0f;
      const float half_height = (float)tt::CARD_HEIGHT / 2.0f;

      // Power is only worth showing while the card is in play, where auras and
      // the turn can push it away from the printed number.
      if (mindbug::is_in_play(state, card)) {
        const auto power =
          std::to_string(mindbug::effective_power(state, card));
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

      if (highlighted_things.count(card) > 0) {
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

// One player's line: life points and Mindbugs left.
static std::string player_line(const mindbug::Game_State& state, int player) {
  return "life " + std::to_string(state.players[player].life) +
         "    mindbugs " + std::to_string(state.players[player].mindbugs);
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
