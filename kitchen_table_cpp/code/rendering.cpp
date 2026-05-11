#include "rendering.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "config.h"
#include "raylib.h"
#include "rlgl.h"  // for rlPushMatrix, rlPopMatrix, rlTranslatef, rlRotatef, rlScalef

// --- Static globals (lazy initialized) ---

static Shader s_background_shader = {0};
static int    s_bg_time_loc       = -1;
static int    s_bg_resolution_loc = -1;
static int    s_bg_turn_loc       = -1;
static float  s_bg_turn_value     = 0.0f;

static Font s_font        = {0};
static bool s_font_loaded = false;

static std::unordered_map<std::string, Texture2D> s_texture_cache;
static std::unordered_map<std::string, Texture2D> s_rounded_texture_cache;


// --- Background shader loading ---

static void load_background_shader() {
  // Load the fragment shader source from disk.
  char* fs_code       = LoadFileText("kitchen_table_cpp/code/background.frag");
  s_background_shader = LoadShaderFromMemory(nullptr, fs_code);
  s_bg_time_loc       = GetShaderLocation(s_background_shader, "u_time");
  s_bg_resolution_loc = GetShaderLocation(s_background_shader, "u_resolution");
  s_bg_turn_loc       = GetShaderLocation(s_background_shader, "u_turn");
  UnloadFileText(fs_code);
}

// --- Font loading ---

static Font& get_font() {
  if (!s_font_loaded) {
    const char* font_path = kt::FONT_PATH;
    if (FileExists(font_path)) {
      s_font = LoadFontEx(font_path, kt::FONT_LOAD_SIZE, nullptr, 0);
    } else {
      s_font = GetFontDefault();
    }
    // Bilinear filtering prevents jagged edges when scaling to any size.
    SetTextureFilter(s_font.texture, TEXTURE_FILTER_BILINEAR);
    s_font_loaded = true;
  }
  return s_font;
}

// --- Texture cache helpers ---

static Texture2D* get_texture(const std::string& image_path) {
  auto it = s_texture_cache.find(image_path);
  if (it != s_texture_cache.end()) {
    return &it->second;
  }

  if (!FileExists(image_path.c_str())) {
    return nullptr;
  }

  Texture2D tex               = LoadTexture(image_path.c_str());
  s_texture_cache[image_path] = tex;
  return &s_texture_cache[image_path];
}

static Texture2D* get_rounded_texture(const std::string& image_path) {
  auto it = s_rounded_texture_cache.find(image_path);
  if (it != s_rounded_texture_cache.end()) {
    return &it->second;
  }

  if (!FileExists(image_path.c_str())) {
    return nullptr;
  }

  float w = (float)kt::CARD_WIDTH;
  float h = (float)kt::CARD_HEIGHT;
  float r = (float)kt::CARD_CORNER_RADIUS;

  // Load image at original resolution (GPU scales when drawing).
  Image image = LoadImage(image_path.c_str());
  int   iw    = image.width;
  int   ih    = image.height;

  // Scale corner radius to match image resolution.
  int sr = (int)(r * std::min((float)iw / w, (float)ih / h));

  // Create rounded rectangle mask at image resolution.
  Image mask = GenImageColor(iw, ih, Color{0, 0, 0, 0});
  ImageDrawRectangle(&mask, sr, 0, iw - 2 * sr, ih, WHITE);
  ImageDrawRectangle(&mask, 0, sr, iw, ih - 2 * sr, WHITE);
  ImageDrawCircle(&mask, sr, sr, sr, WHITE);
  ImageDrawCircle(&mask, iw - sr, sr, sr, WHITE);
  ImageDrawCircle(&mask, sr, ih - sr, sr, WHITE);
  ImageDrawCircle(&mask, iw - sr, ih - sr, sr, WHITE);

  // Apply mask and convert to GPU texture.
  ImageAlphaMask(&image, mask);
  Texture2D tex = LoadTextureFromImage(image);

  UnloadImage(image);
  UnloadImage(mask);

  s_rounded_texture_cache[image_path] = tex;
  return &s_rounded_texture_cache[image_path];
}

// --- Text rendering ---

void render_text(
  const std::string& text, float x, float y, int size, Color color
) {
  DrawTextEx(
    get_font(),
    text.c_str(),
    {(float)x, (float)y},
    (float)size,
    kt::FONT_SPACING,
    color
  );
}

int text_width(const std::string& text, int size) {
  Vector2 measured =
    MeasureTextEx(get_font(), text.c_str(), (float)size, kt::FONT_SPACING);
  return (int)measured.x;
}

// --- draw_background ---

void draw_background(float turn) {
  if (s_bg_time_loc == -1) {
    load_background_shader();
  }

  // Smoothly interpolate toward the target turn value.
  float dt    = GetFrameTime();
  float speed = 3.0f;
  s_bg_turn_value += (turn - s_bg_turn_value) * std::min(dt * speed, 1.0f);

  float t = (float)GetTime();
  SetShaderValue(s_background_shader, s_bg_time_loc, &t, SHADER_UNIFORM_FLOAT);

  float res[2] = {(float)GetScreenWidth(), (float)GetScreenHeight()};
  SetShaderValue(
    s_background_shader, s_bg_resolution_loc, res, SHADER_UNIFORM_VEC2
  );

  SetShaderValue(
    s_background_shader, s_bg_turn_loc, &s_bg_turn_value, SHADER_UNIFORM_FLOAT
  );

  BeginShaderMode(s_background_shader);
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
  EndShaderMode();
}

// --- draw_card_back ---

void draw_card_back() {
  float x = 0.0f;
  float y = 0.0f;
  float w = (float)kt::CARD_WIDTH;
  float h = (float)kt::CARD_HEIGHT;
  float r = (float)kt::CARD_CORNER_RADIUS;

  // KT_Card colors from kt namespace (matching config.py defaults).
  Color back_color    = {60, 80, 120, 255};
  Color pattern_color = {80, 100, 140, 255};
  Color border_color  = {80, 80, 80, 255};

  // KT_Card background.
  DrawRectangleRounded(
    Rectangle{x, y, w, h}, r / std::min(w, h), 8, back_color
  );

  // Simple pattern on back.
  float margin = 15.0f;
  DrawRectangleRounded(
    Rectangle{x + margin, y + margin, w - 2.0f * margin, h - 2.0f * margin},
    r / std::min(w, h),
    8,
    pattern_color
  );

  // Border.
  DrawRectangleRoundedLinesEx(
    Rectangle{x, y, w, h}, r / std::min(w, h), 8, 2.0f, border_color
  );
}

// --- draw_card_content ---

void draw_card_content(const Thing& card, bool face_up) {
  if (!face_up) {
    draw_card_back();
    return;
  }

  float x = 0.0f;
  float y = 0.0f;
  float w = (float)kt::CARD_WIDTH;
  float h = (float)kt::CARD_HEIGHT;
  float r = (float)kt::CARD_CORNER_RADIUS;

  // KT_Card background: image with rounded corners, or solid color fallback.
  Texture2D* texture = nullptr;
  if (!card.image_path.empty()) {
    texture = get_rounded_texture(card.image_path);
  }

  if (texture) {
    // Draw image scaled to fill the card.
    Rectangle source_rect = {
      0.0f, 0.0f, (float)texture->width, (float)texture->height
    };
    Rectangle dest_rect = {x, y, w, h};
    DrawTexturePro(
      *texture, source_rect, dest_rect, Vector2{0.0f, 0.0f}, 0.0f, WHITE
    );
  } else {
    // Fallback: solid color background.
    Color bg = {255, 255, 255, 255};
    DrawRectangleRounded(Rectangle{x, y, w, h}, r / std::min(w, h), 8, bg);
  }

  // Invoke the draw callback if set.
  if (card.draw_callback) {
    card.draw_callback(const_cast<Thing&>(card));
  }
}

// --- draw_card ---

void draw_card(const Thing& card, bool face_up) {
  float w = (float)kt::CARD_WIDTH;
  float h = (float)kt::CARD_HEIGHT;

  if (card.rotation == 0) {
    // No rotation: translate to card position and draw at origin.
    rlPushMatrix();
    rlTranslatef(card.x, card.y, 0.0f);
    draw_card_content(card, face_up);
    rlPopMatrix();
  } else {
    // Rotate around the center of the card.
    float cx = card.x + w / 2.0f;
    float cy = card.y + h / 2.0f;
    rlPushMatrix();
    rlTranslatef(cx, cy, 0.0f);
    rlRotatef((float)card.rotation, 0.0f, 0.0f, 1.0f);
    rlTranslatef(-w / 2.0f, -h / 2.0f, 0.0f);
    draw_card_content(card, face_up);
    rlPopMatrix();
  }
}

// --- draw_stack ---

void draw_stack(const Stack& stack, const Table_State& state) {
  // Draw each card that belongs to this stack.
  for (int card_id : stack.cards) {
    draw_card(state.cards[card_id], stack.face_up);
  }
}

// --- draw_stack_placeholder ---

void draw_stack_placeholder(const Stack& stack) {
  float w = (float)kt::CARD_WIDTH;
  float h = (float)kt::CARD_HEIGHT;
  float r = (float)kt::CARD_CORNER_RADIUS;

  // Dashed outline placeholder.
  DrawRectangleRoundedLinesEx(
    Rectangle{stack.rect.x, stack.rect.y, stack.rect.width, stack.rect.height},
    r / std::min(w, h),
    8,
    1.0f,
    Color{100, 100, 100, 100}
  );

  // Label centered in the placeholder.
  const std::string& label   = stack.name;
  int                label_w = text_width(label, 14);

  render_text(
    label,
    (int)(stack.rect.x + (w - (float)label_w) / 2.0f),
    (int)(stack.rect.y + h / 2.0f - 7.0f),
    14,
    Color{100, 100, 100, 150}
  );
}

// --- animate ---

void animate(std::vector<KT_Card>& cards, const Table_State& state, float dt) {
  int selected_card_id = state.drag_state.card_id;
  int n                = (int)cards.size();
  for (int i = 0; i < n; ++i) {
    KT_Card&       acard  = cards[i];
    const KT_Card& target = state.cards[i];

    float old_x = acard.x;
    acard.x     = acard.x * (1.0f - dt) + target.x * dt;
    acard.y     = acard.y * (1.0f - dt) + target.y * dt;
    float vx    = acard.x - old_x;
    acard.rotation =
      acard.rotation * (1.0f - dt) + target.rotation * dt + vx * 0.1f;
  }

  // Dragged card snaps immediately to the target position.
  if (selected_card_id >= 0 && selected_card_id < n) {
    KT_Card&       acard  = cards[selected_card_id];
    const KT_Card& target = state.cards[selected_card_id];
    acard.x            = target.x;
    acard.y            = target.y;
  }
}

// --- draw_zoomed_card ---

void draw_zoomed_card(const Thing& card, bool face_up) {
  int screen_w = GetScreenWidth();
  int screen_h = GetScreenHeight();

  // Dim background.
  DrawRectangle(0, 0, screen_w, screen_h, Color{0, 0, 0, 160});

  float card_w = (float)kt::CARD_WIDTH;
  float card_h = (float)kt::CARD_HEIGHT;
  float margin = 40.0f;

  // Scale card to fill screen with some margin.
  float scale = std::min(
    ((float)screen_w - 2.0f * margin) / card_w,
    ((float)screen_h - 2.0f * margin) / card_h
  );

  // Center on screen.
  float cx = ((float)screen_w - card_w * scale) / 2.0f;
  float cy = ((float)screen_h - card_h * scale) / 2.0f;

  rlPushMatrix();
  rlTranslatef(cx, cy, 0.0f);
  rlScalef(scale, scale, 1.0f);
  draw_card_content(card, face_up);
  rlPopMatrix();
}

// --- draw_table ---

void draw_table(Table_State& state) {
  // Initialize animated_cards on first call (copy of cards list).
  if (state.animated_cards.empty()) {
    state.animated_cards = state.cards;
  }

  // Animate card positions toward their targets.
  // dt is hardcoded to match the original Python behavior (frame-rate
  // independent lerp was not intended).
  animate(state.animated_cards, state, 0.1f);

  // Draw stack placeholder when a drag is in progress.
  if (state.drag_state.current_stack != -1) {
    draw_stack_placeholder(state.stacks[state.drag_state.current_stack]);
  }

  // Collect stacks and sort by depth (ascending) before drawing.
  std::vector<Stack> stacks_sorted = state.stacks;
  std::sort(
    stacks_sorted.begin(),
    stacks_sorted.end(),
    [](const Stack& a, const Stack& b) { return a.depth < b.depth; }
  );

  // Draw stacks (skip the card that is currently being dragged).
  for (const Stack& stack : stacks_sorted) {
    for (int card_id : stack.cards) {
      if (state.drag_state.card_id == card_id) continue;
      draw_card(state.animated_cards[card_id], stack.face_up);
    }
  }

  // Draw loose cards (always face up).
  for (int card_id : state.loose_cards) {
    draw_card(state.animated_cards[card_id], true);
  }

  // Draw the dragged card on top of everything else.
  if (state.drag_state.card_id >= 0) {
    const KT_Card& card    = state.animated_cards[state.drag_state.card_id];
    bool        face_up = state.stacks[state.drag_state.original_stack].face_up;
    draw_card(card, face_up);
  }

  // Call the optional draw callback for custom overlays.
  if (state.draw_callback) {
    state.draw_callback(&state);
  }

  // Draw zoomed card on top of everything.
  if (state.zoomed_card_id >= 0) {
    // Determine face_up by checking which stack owns this card.
    bool face_up = true;
    for (const Stack& stack : state.stacks) {
      for (int cid : stack.cards) {
        if (cid == state.zoomed_card_id) {
          face_up = stack.face_up;
        }
      }
    }
    draw_zoomed_card(state.cards[state.zoomed_card_id], face_up);
  }
}
