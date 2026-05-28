#include "ui.h"

#include <raylib.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>
#include <tabletop/ui.h>

#include <algorithm>
#include <cctype>

// On desktop assets sit relative to the gods-app working directory; on web
// they're preloaded at the absolute path "/gods/card-images" in MEMFS, and
// the emscripten cwd isn't guaranteed to be "/".
#ifdef __EMSCRIPTEN__
static const std::string IMAGES_DIR = "/gods/card-images";
#else
static const std::string IMAGES_DIR = "gods/card-images";
#endif

std::vector<Thing> make_gods_stacks(
  int bottom_player, int window_width, int window_height
) {
  int W      = window_width;
  int H      = window_height;
  int w      = tt::CARD_WIDTH;
  int h      = tt::CARD_HEIGHT;
  int margin = 20;

  int spread_hand    = 160;
  int spread_wonders = 160;
  int spread_pile    = -3;

  // Layout in root-local coords: root is centered on the screen, so the
  // window spans (-W/2, -H/2) to (W/2, H/2) in root-local space.
  Rectangle window        = {-(float)W / 2.0f, -(float)H / 2.0f, (float)W, (float)H};
  int       hand_width    = (int)((float)w * 5.5f * (float)W / 1600.0f);
  int       peoples_width = 2 * w + spread_wonders;

  // Bottom player layout (player 0 by default).
  Rectangle p0_hand =
    place_inside(window, hand_width, h, "center", "bottom", margin);
  p0_hand.x += 100;
  Rectangle p0_wonders =
    place_next(p0_hand, hand_width, h, "center", "top", margin);
  Rectangle p0_deck    = place_next(p0_hand, w, h, "left", "center", margin);
  Rectangle p0_discard = place_next(p0_deck, w, h, "left", "center", margin);

  int opponent_shift = (int)(h * 0.65f);
  // Opponent rows mirror the bottom rows across the screen's vertical
  // midline. In root-local coords (origin at center), a mirror of y is -y;
  // a row of height h that mirrors a row whose top is at Y has its top at
  // -Y - h. opponent_shift nudges the whole top row slightly off-screen.
  int top_y          = -(int)(H / 2) + margin - opponent_shift;
  int top_wonders_y  = -(int)p0_wonders.y - h - opponent_shift;

  Rectangle shared_deck = place_next(window, w, h, "right", "center", 10);
  Rectangle p0_peoples =
    place_next(p0_wonders, peoples_width, h, "left", "center", margin);

  Rectangle p1_deck    = {p0_deck.x, (float)top_y, (float)w, (float)h};
  Rectangle p1_hand    = {p0_hand.x, (float)top_y, (float)hand_width, (float)h};
  Rectangle p1_discard = {p0_discard.x, (float)top_y, (float)w, (float)h};
  Rectangle p1_peoples = {
    p0_peoples.x, (float)top_wonders_y, (float)peoples_width, (float)h
  };
  Rectangle p1_wonders = {
    p0_wonders.x, (float)top_wonders_y, (float)hand_width, (float)h
  };

  if (bottom_player == 1) {
    std::swap(p0_deck, p1_deck);
    std::swap(p0_hand, p1_hand);
    std::swap(p0_discard, p1_discard);
    std::swap(p0_peoples, p1_peoples);
    std::swap(p0_wonders, p1_wonders);
  }

  // Each hand is visible only to the player who owns it. Without this
  // split, the joiner (bottom_player == 1) would see both hands face-down.
  bool p0_visible = (bottom_player == 0);
  bool p1_visible = (bottom_player == 1);

  auto mk = [](Rectangle r, int sx, int sy, bool face_up, std::string name) {
    Thing t;
    set_local_rect(t, r);
    t.spread_x = (float)sx;
    t.spread_y = (float)sy;
    t.face_up  = true;
    t.name     = std::move(name);
    t.color    = {255, 255, 255, 100};
    return t;
  };

  std::vector<Thing> out;
  out.push_back(mk(p0_deck, 0, spread_pile, false, "p0_deck"));
  out.push_back(mk(p0_hand, spread_hand, 0, p0_visible, "p0_hand"));
  out.push_back(mk(p0_discard, 0, spread_pile, true, "p0_discard"));
  out.push_back(mk(p0_peoples, spread_wonders, 0, true, "p0_peoples"));
  out.push_back(mk(p0_wonders, spread_wonders, 0, true, "p0_wonders"));
  out.push_back(mk(p1_deck, 0, spread_pile, false, "p1_deck"));
  out.push_back(mk(p1_hand, spread_hand, 0, p1_visible, "p1_hand"));
  out.push_back(mk(p1_discard, 0, spread_pile, true, "p1_discard"));
  out.push_back(mk(p1_peoples, spread_wonders, 0, true, "p1_peoples"));
  out.push_back(mk(p1_wonders, spread_wonders, 0, true, "p1_wonders"));
  out.push_back(mk(shared_deck, 0, spread_pile, false, "shared_deck"));
  return out;
}

std::string get_image_path(const std::string& card_name) {
  std::string name = card_name;
  if (name.size() == 1) name = "0" + name;
  std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
    return (char)std::tolower(c);
  });
  std::replace(name.begin(), name.end(), ' ', '_');
  // Don't FileExists-gate — raylib handles missing files by returning a
  // null texture, which the renderer falls back from. The check was also
  // unreliable on emscripten depending on cwd.
  return IMAGES_DIR + "/" + name + ".png";
}

void draw_card_power_badge(const std::string& power, bool destroyed) {
  // Drawn in card-local space where the card center is at (0, 0).
  int w = tt::CARD_WIDTH;
  int h = tt::CARD_HEIGHT;
  int r = tt::CARD_CORNER_RADIUS;

  // Badge sits near the top-right corner — offsets are from the center.
  int badge_cx = (int)(0.38f * (float)w);
  int badge_cy = (int)(0.12f * (float)w - 0.5f * (float)h);
  int badge_r  = (int)(0.12f * (float)w);
  DrawCircle(badge_cx, badge_cy, (float)badge_r, ::Color{0, 0, 0, 255});

  int size = (int)(0.2f * (float)w);
  int tw   = text_width(power, size);
  render_text(
    power,
    (float)(badge_cx - tw / 2),
    (float)(badge_cy - size / 2),
    size,
    Color{255, 255, 255, 255}
  );

  if (destroyed) {
    DrawRectangleRounded(
      Rectangle{-(float)w / 2.0f, -(float)h / 2.0f, (float)w, (float)h},
      (float)r / (float)std::min(w, h),
      8,
      ::Color{0, 0, 0, 100}
    );
  }
}

void draw_player_hud(
  int player_id, int score, int deck_count, bool is_current, int hud_y
) {
  (void)player_id;
  (void)deck_count;

  if (is_current) {
    DrawRectangleRounded(
      Rectangle{
        (float)(tt::WINDOW_WIDTH - 10), (float)(hud_y + 28), 6.0f, 50.0f
      },
      0.5f,
      4,
      ::Color{255, 255, 255, 255}
    );
  }

  std::string score_text = "Points: " + std::to_string(score);
  render_text(
    score_text,
    (float)(tt::WINDOW_WIDTH - 200),
    (float)(hud_y + 22),
    40,
    Color{200, 200, 200, 255}
  );
}

