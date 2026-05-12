#include "rendering.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "config.h"
#include "game_state.h"
#include "raylib.h"
#include "rlgl.h"  // for rlPushMatrix, rlPopMatrix, rlTranslatef, rlRotatef, rlScalef

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
  char* fs_code = LoadFileText("tabletop/background.frag");
#ifdef __EMSCRIPTEN__
  // WebGL2 needs "#version 300 es" with a precision qualifier instead of the
  // desktop "#version 330".
  std::string code = fs_code;
  size_t      pos  = code.find("#version 330");
  if (pos != std::string::npos) {
    code.replace(pos, 12, "#version 300 es\nprecision mediump float;");
  }
  s_background_shader = LoadShaderFromMemory(nullptr, code.c_str());
#else
  s_background_shader = LoadShaderFromMemory(nullptr, fs_code);
#endif
  s_bg_time_loc       = GetShaderLocation(s_background_shader, "u_time");
  s_bg_resolution_loc = GetShaderLocation(s_background_shader, "u_resolution");
  s_bg_turn_loc       = GetShaderLocation(s_background_shader, "u_turn");
  UnloadFileText(fs_code);
}

// --- Font loading ---

static Font& get_font() {
  if (!s_font_loaded) {
    const char* font_path = tt::FONT_PATH;
    if (FileExists(font_path)) {
      s_font = LoadFontEx(font_path, tt::FONT_LOAD_SIZE, nullptr, 0);
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

  float w = (float)tt::CARD_WIDTH;
  float h = (float)tt::CARD_HEIGHT;
  float r = (float)tt::CARD_CORNER_RADIUS;

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
    tt::FONT_SPACING,
    color
  );
}

int text_width(const std::string& text, int size) {
  Vector2 measured =
    MeasureTextEx(get_font(), text.c_str(), (float)size, tt::FONT_SPACING);
  return (int)measured.x;
}

// --- draw_background ---

void draw_background(float turn) {
  if (s_bg_time_loc == -1) {
    load_background_shader();
  }

#ifdef __EMSCRIPTEN__
  // PLATFORM_WEB does not implement GetWindowScaleDPI (returns {1,1}).  After
  // we resize the canvas pixel buffer to dpr × logical in menu.cpp, raylib's
  // viewport is still clamped to the logical 1700×1000 window, leaving the
  // rest of the physical canvas black.  Override the GL viewport and projection
  // every frame to cover the full physical canvas.
  {
    double dpr = emscripten_get_device_pixel_ratio();
    if (dpr > 1.0) {
      int pw = (int)(tt::WINDOW_WIDTH * dpr);
      int ph = (int)(tt::WINDOW_HEIGHT * dpr);
      rlViewport(0, 0, pw, ph);
      rlMatrixMode(RL_PROJECTION);
      rlLoadIdentity();
      rlOrtho(0, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, 0, 0, 1);
      rlMatrixMode(RL_MODELVIEW);
      rlLoadIdentity();
    }
  }
#endif

  // Smoothly interpolate toward the target turn value.
  float dt    = GetFrameTime();
  float speed = 3.0f;
  s_bg_turn_value += (turn - s_bg_turn_value) * std::min(dt * speed, 1.0f);

  float t = (float)GetTime();
  SetShaderValue(s_background_shader, s_bg_time_loc, &t, SHADER_UNIFORM_FLOAT);

  // Pass physical pixel dimensions so the shader's UV (fragCoord/resolution)
  // stays in [0,1] on HiDPI displays.
#ifdef __EMSCRIPTEN__
  double dpr     = emscripten_get_device_pixel_ratio();
  float  res[2]  = {
    (float)(tt::WINDOW_WIDTH  * dpr),
    (float)(tt::WINDOW_HEIGHT * dpr)
  };
#else
  float res[2] = {(float)GetScreenWidth(), (float)GetScreenHeight()};
#endif
  SetShaderValue(
    s_background_shader, s_bg_resolution_loc, res, SHADER_UNIFORM_VEC2
  );

  SetShaderValue(
    s_background_shader, s_bg_turn_loc, &s_bg_turn_value, SHADER_UNIFORM_FLOAT
  );

  BeginShaderMode(s_background_shader);
  DrawRectangle(0, 0, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, WHITE);
  EndShaderMode();
}

// --- draw_card_back ---

void draw_card_back() {
  float x = 0.0f;
  float y = 0.0f;
  float w = (float)tt::CARD_WIDTH;
  float h = (float)tt::CARD_HEIGHT;
  float r = (float)tt::CARD_CORNER_RADIUS;

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
  float w = (float)tt::CARD_WIDTH;
  float h = (float)tt::CARD_HEIGHT;
  float r = (float)tt::CARD_CORNER_RADIUS;

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

// --- draw_stack_placeholder ---

void draw_stack_placeholder(int stack_id, const Table_State& state) {
  // Drawn in world coords (overlay, not part of the DFS tree walk).
  Rectangle r_world = world_rect(stack_id, state);
  float     w       = (float)tt::CARD_WIDTH;
  float     h       = (float)tt::CARD_HEIGHT;
  float     r       = (float)tt::CARD_CORNER_RADIUS;

  DrawRectangleRoundedLinesEx(
    r_world, r / std::min(w, h), 8, 1.0f, Color{100, 100, 100, 100}
  );

  const std::string& label   = state.things[stack_id].name;
  int                label_w = text_width(label, 14);
  render_text(
    label,
    (int)(r_world.x + (w - (float)label_w) / 2.0f),
    (int)(r_world.y + h / 2.0f - 7.0f),
    14,
    Color{100, 100, 100, 150}
  );
}

// --- animate ---

void animate(std::vector<Thing>& things, const Table_State& state, float dt) {
  // Resize mirror to match state.things; copy verbatim on first call so cards
  // don't fly in from (0,0).
  if (things.size() != state.things.size()) {
    things = state.things;
    return;
  }
  int selected = state.drag_state.card_id;
  for (int i = 0; i < (int)state.things.size(); ++i) {
    Thing&       a      = things[i];
    const Thing& target = state.things[i];

    // Preserve the smoothed pose, refresh everything else from target.
    float ax   = a.rect.x;
    float ay   = a.rect.y;
    float arot = a.rotation;
    a          = target;
    a.rect.x   = ax;
    a.rect.y   = ay;
    a.rotation = arot;

    if (i == selected) {
      // Dragged card snaps to its current local position.
      a.rect.x   = target.rect.x;
      a.rect.y   = target.rect.y;
      a.rotation = target.rotation;
      continue;
    }
    float old_x = a.rect.x;
    a.rect.x    = a.rect.x * (1.0f - dt) + target.rect.x * dt;
    a.rect.y    = a.rect.y * (1.0f - dt) + target.rect.y * dt;
    float vx    = a.rect.x - old_x;
    a.rotation =
      a.rotation * (1.0f - dt) + target.rotation * dt + vx * 0.1f;
  }
}

// --- DFS render walker ---

static void apply_local_transform(const Thing& t) {
  if (t.rotation == 0.0f) {
    rlTranslatef(t.rect.x, t.rect.y, 0.0f);
    return;
  }
  // Rotate around the thing's center.
  float w = is_card(t) ? (float)tt::CARD_WIDTH : t.rect.width;
  float h = is_card(t) ? (float)tt::CARD_HEIGHT : t.rect.height;
  float cx = t.rect.x + w / 2.0f;
  float cy = t.rect.y + h / 2.0f;
  rlTranslatef(cx, cy, 0.0f);
  rlRotatef(t.rotation, 0.0f, 0.0f, 1.0f);
  rlTranslatef(-w / 2.0f, -h / 2.0f, 0.0f);
}

static void draw_thing_recursive(
  int                       thing_id,
  const Table_State&        state,
  const std::vector<Thing>& source,
  bool                      parent_face_up
) {
  const Thing& t = source[thing_id];
  rlPushMatrix();
  apply_local_transform(t);

  // Draw the card body (skip the dragged card — it's drawn on top later).
  if (is_card(t) && thing_id != state.drag_state.card_id) {
    draw_card_content(t, parent_face_up);
  }

  // Per-thing draw callback fires after self-draw, before children.
  if (t.draw_callback) {
    t.draw_callback(const_cast<Thing&>(t));
  }

  // Children inherit this thing's face_up.
  for (int child_id : t.children) {
    draw_thing_recursive(child_id, state, source, t.face_up);
  }
  rlPopMatrix();
}

// --- draw_zoomed_card ---

void draw_zoomed_card(const Thing& card, bool face_up) {
  int screen_w = GetScreenWidth();
  int screen_h = GetScreenHeight();

  // Dim background.
  DrawRectangle(0, 0, screen_w, screen_h, Color{0, 0, 0, 160});

  float card_w = (float)tt::CARD_WIDTH;
  float card_h = (float)tt::CARD_HEIGHT;
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
  // Smoothed mirror of state.things; lerps every frame toward target.
  animate(state.animated_cards, state, 0.1f);

  // Highlight the hovered stack while dragging.
  if (state.drag_state.current_stack != -1 &&
      state.drag_state.current_stack != state.root) {
    draw_stack_placeholder(state.drag_state.current_stack, state);
  }

  // Render the scene tree DFS from root. Root's direct children are sorted by
  // depth so existing layered draw order is preserved.
  const Thing&     root_target = state.things[state.root];
  const Thing&     root_anim   = state.animated_cards[state.root];
  std::vector<int> draw_order  = root_target.children;
  std::sort(
    draw_order.begin(),
    draw_order.end(),
    [&state](int a, int b) {
      return state.animated_cards[a].depth < state.animated_cards[b].depth;
    }
  );

  rlPushMatrix();
  apply_local_transform(root_anim);
  for (int child_id : draw_order) {
    draw_thing_recursive(child_id, state, state.animated_cards, root_anim.face_up);
  }
  rlPopMatrix();

  // Draw the dragged card last so it sits above everything else.
  int dragged = state.drag_state.card_id;
  if (dragged >= 0) {
    // Use the parent's world transform as the base; the card's own local
    // transform is applied on top.
    int     parent_id    = find_parent(dragged, state);
    Vector2 parent_world = (parent_id >= 0) ? local_to_world(parent_id, state)
                                            : Vector2{0.0f, 0.0f};
    rlPushMatrix();
    rlTranslatef(parent_world.x, parent_world.y, 0.0f);
    const Thing& c = state.animated_cards[dragged];
    apply_local_transform(c);
    bool face_up = true;
    int  orig    = state.drag_state.original_stack;
    if (orig >= 0 && orig != state.root) face_up = state.things[orig].face_up;
    draw_card_content(c, face_up);
    if (c.draw_callback) c.draw_callback(const_cast<Thing&>(c));
    rlPopMatrix();
  }

  // Optional table-level draw callback (custom HUD overlays).
  if (state.draw_callback) {
    state.draw_callback(&state);
  }

  // Zoomed card on top of everything.
  if (state.zoomed_card_id >= 0) {
    bool face_up = true;
    int  owner   = find_stack_containing_card(state.zoomed_card_id, state);
    if (owner >= 0) face_up = state.things[owner].face_up;
    draw_zoomed_card(state.things[state.zoomed_card_id], face_up);
  }
}
