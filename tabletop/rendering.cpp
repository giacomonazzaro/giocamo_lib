#include "rendering.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "config.h"
#include "input_recorder.h"
#include "raylib.h"
#include "rlgl.h"  // for rlPushMatrix, rlPopMatrix, rlTranslatef, rlRotatef, rlScalef
#include "tabletop.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// --- Static globals (lazy initialized) ---

static Shader s_background_shader = {0};
static int    s_bg_time_loc       = -1;
static int    s_bg_resolution_loc = -1;
static int    s_bg_turn_loc       = -1;
static int    s_bg_mouse_loc      = -1;
static float  s_bg_turn_value     = 0.0f;

static Font s_font        = {0};
static bool s_font_loaded = false;

static std::unordered_map<std::string, Texture2D> s_texture_cache;

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
  s_bg_mouse_loc      = GetShaderLocation(s_background_shader, "u_mouse");
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

static Texture2D* get_texture(
  const std::string& image_path, bool rounded_corners
) {
  // Rounded and plain versions of the same file are cached under distinct keys.
  std::string key = rounded_corners ? image_path : image_path + "#plain";
  auto        it  = s_texture_cache.find(key);
  if (it != s_texture_cache.end()) {
    return &it->second;
  }

  // Load image at original resolution (GPU scales when drawing). raylib
  // returns an image with width==0 when the file is missing — bail in that
  // case so the renderer can fall back to a solid-color rect.
  Image image = LoadImage(image_path.c_str());
  int   iw    = image.width;
  int   ih    = image.height;
  if (iw == 0 || ih == 0) {
    UnloadImage(image);
    return nullptr;
  }

  if (rounded_corners) {
    float w = (float)tt::CARD_WIDTH;
    float h = (float)tt::CARD_HEIGHT;
    float r = (float)tt::CARD_CORNER_RADIUS;

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

    // Apply mask so the corners become transparent.
    ImageAlphaMask(&image, mask);
    UnloadImage(mask);
  }

  Texture2D tex = LoadTextureFromImage(image);
  UnloadImage(image);

  s_texture_cache[key] = tex;
  return &s_texture_cache[key];
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

void draw_background(const Input& input, float turn) {
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

  // Pass logical pixel dimensions to match desktop behavior: on retina
  // displays gl_FragCoord is in physical pixels so uv = fragCoord/resolution
  // naturally ranges 0..dpr, which the shader is tuned for.
  float res[2] = {(float)GetScreenWidth(), (float)GetScreenHeight()};
  SetShaderValue(
    s_background_shader, s_bg_resolution_loc, res, SHADER_UNIFORM_VEC2
  );

  SetShaderValue(
    s_background_shader, s_bg_turn_loc, &s_bg_turn_value, SHADER_UNIFORM_FLOAT
  );
  float        mouse[2]          = {(float)input.mouse_x, (float)input.mouse_y};
  static float mouse_animated[2] = {0, 0};
  if (mouse_animated[0] == 0 && mouse_animated[1] == 0) {
    mouse_animated[0] = mouse[0];
    mouse_animated[1] = mouse[1];
  }
  mouse_animated[0] = (1.0 - dt) * mouse_animated[0] + dt * mouse[0];
  mouse_animated[1] = (1.0 - dt) * mouse_animated[1] + dt * mouse[1];

  SetShaderValue(
    s_background_shader, s_bg_mouse_loc, mouse_animated, SHADER_UNIFORM_VEC2
  );
  BeginShaderMode(s_background_shader);
  DrawRectangle(0, 0, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, WHITE);
  EndShaderMode();
}

// --- draw_thing_back ---

void draw_thing_back() {
  // Drawn centered at origin. The world transform places this center.
  float w = (float)tt::CARD_WIDTH;
  float h = (float)tt::CARD_HEIGHT;
  float r = (float)tt::CARD_CORNER_RADIUS;
  float x = -w / 2.0f;
  float y = -h / 2.0f;

  // Thing colors from kt namespace (matching config.py defaults).
  Color back_color    = {60, 80, 120, 255};
  Color pattern_color = {80, 100, 140, 255};
  Color border_color  = {80, 80, 80, 255};

  // Thing background.
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

// --- draw_thing ---

void draw_thing(const Thing& thing, bool face_up) {
  if (!face_up) {
    draw_thing_back();
    return;
  }

  // Drawn centered at origin. The world transform places this center.
  Vector2 size = shape_size(thing.shape);
  float   w    = size.x;
  float   h    = size.y;
  float   x    = -w / 2.0f;
  float   y    = -h / 2.0f;

  // A rectangle's texture corners are rounded only when it has a corner radius.
  bool rounded = thing.shape.type == Shape_Type::RECTANGLE &&
                 thing.shape.rectangle.corner_radius > 0.0f;

  // Thing background: image with rounded corners, or solid color fallback.
  Texture2D* texture = nullptr;
  if (!thing.image_path.empty()) {
    texture = get_texture(thing.image_path, rounded);
  }

  if (texture) {
    // Draw image scaled to fill the thing.
    Rectangle source_rect = {
      0.0f, 0.0f, (float)texture->width, (float)texture->height
    };
    Rectangle dest_rect = {x, y, w, h};
    DrawTexturePro(
      *texture, source_rect, dest_rect, Vector2{0.0f, 0.0f}, 0.0f, WHITE
    );
  } else if (w > 0.0f && h > 0.0f) {
    // Fallback: solid color background matching the thing's shape.
    switch (thing.shape.type) {
      case Shape_Type::RECTANGLE: {
        float r = thing.shape.rectangle.corner_radius;
        DrawRectangleRounded(
          Rectangle{x, y, w, h}, r / std::min(w, h), 8, thing.color
        );
        break;
      }
      case Shape_Type::CIRCLE:
        DrawCircleV({0.0f, 0.0f}, thing.shape.circle.radius, thing.color);
        break;
      case Shape_Type::HEXAGON:
        DrawPoly(
          {0.0f, 0.0f}, 6, thing.shape.hexagon.radius, 0.0f, thing.color
        );
        break;
      case Shape_Type::TRIANGLE:
        DrawPoly(
          {0.0f, 0.0f}, 3, thing.shape.triangle.radius, 0.0f, thing.color
        );
        break;
    }
  }

  // Counter: a number centered in the thing, sized relative to the thing.
  if (thing.counter) {
    std::string text      = std::to_string(thing.counter.value);
    int         font_size = (int)(std::min(w, h) * 0.45f);
    int         label_w   = text_width(text, font_size);
    render_text(text, -label_w / 2.0f, -font_size / 2.0f, font_size, WHITE);
  }
}

// --- draw_drop_placeholder ---

void draw_drop_placeholder(int thing_id, const Table_State& state) {
  // Drawn in world coords (overlay, not part of the DFS tree walk).
  Rectangle r_world = world_rect(thing_id, state);
  float     w       = (float)tt::CARD_WIDTH;
  float     h       = (float)tt::CARD_HEIGHT;
  float     r       = (float)tt::CARD_CORNER_RADIUS;

  DrawRectangleRoundedLinesEx(
    r_world, r / std::min(w, h), 8, 1.0f, Color{100, 100, 100, 100}
  );

  const std::string& label   = state.things[thing_id].name;
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

static void update_world_transforms(
  int                             id,
  const Transform2D&              parent_transform,
  const std::vector<Thing>&       things,
  const std::vector<Transform2D>& local_transforms,
  std::vector<Transform2D>&       world_transforms
) {
  world_transforms[id] = parent_transform * local_transforms[id];
  for (int child_id : things[id].children)
    update_world_transforms(
      child_id, world_transforms[id], things, local_transforms, world_transforms
    );
}

void animate(
  int                             i,
  std::vector<Transform2D>&       animated,
  const std::vector<Transform2D>& target,
  const Table_State&              table,
  float                           dt,
  bool                            smoothout = true
) {
  if (!smoothout) {
    printf("smoothout: %d\n", smoothout);
  }
  // dt *= 10.0f;  // Speed multiplier: higher = snappier but more CPU.
  // for (int i = 0; i < table.things.size(); ++i) {
  if (i == table.drag_state.thing_id() || !smoothout) {
    animated[i] = target[i];  // Snap to cursor.
    // return;
  } else {
    float vx = (target[i].x - animated[i].x) * dt;
    float vy = (target[i].y - animated[i].y) * dt;
    // Cap per-frame travel so big jumps (e.g. trick → captured pile) glide
    // instead of teleporting.
    // const float speed     = std::sqrt(vx * vx + vy * vy);
    // const float max_speed = 10.0f;
    // if (speed > max_speed) {
    //   vx *= max_speed / speed;
    //   vy *= max_speed / speed;
    // }
    animated[i].x += vx;
    animated[i].y += vy;
    animated[i].rotation = animated[i].rotation * (1.0f - dt) +
                           target[i].rotation * dt;
    // Rotation eases toward target with a small swing that tilts the thing.
    animated[i].rotation += vx * 0.1f;
  }

  if (smoothout && table.drag_state.thing_id() == i) {
    smoothout = false;
  }
  for (size_t k = 0; k < table.things[i].children.size(); k++) {
    animate(
      table.things[i].children[k], animated, target, table, dt, smoothout
    );
  }
}

void animate(
  std::vector<Transform2D>&       animated,
  const std::vector<Transform2D>& target,
  const Table_State&              table,
  float                           dt
) {
  animate(table.root, animated, target, table, dt * 10);
}

// Translate to (wt.x, wt.y) — which is the thing's center — and rotate
// around it.
static void apply_world_transform(const Transform2D& wt) {
  rlTranslatef(wt.x, wt.y, 0.0f);
  if (wt.rotation != 0.0f) {
    rlRotatef(wt.rotation, 0.0f, 0.0f, 1.0f);
  }
}

// Walk the tree to draw each thing at its absolute world transform. No
// matrix nesting — transforms are already fully resolved by animate().
static void draw_thing_world(
  int id, const Table_State& state, bool parent_face_up, const Input& input
) {
  const Thing& t       = state.things[id];
  const bool   face_up = parent_face_up && t.face_up;
  if (id != state.drag_state.thing_id()) {
    rlPushMatrix();
    apply_world_transform(state.world_transforms_animated[id]);
    draw_thing(t, face_up);
    auto cb = state.draw_callbacks.find(id);
    if (cb != state.draw_callbacks.end()) cb->second(state, input, face_up);
    rlPopMatrix();
  }
  for (int child_id : t.children) {
    draw_thing_world(child_id, state, face_up, input);
  }
}

// --- draw_zoomed_thing ---

void draw_zoomed_thing(
  const Table_State& state, const Input& input, int thing_id, bool face_up
) {
  int screen_w = GetScreenWidth();
  int screen_h = GetScreenHeight();

  // Dim background.
  DrawRectangle(0, 0, screen_w, screen_h, Color{0, 0, 0, 160});

  float thing_w = (float)tt::CARD_WIDTH;
  float thing_h = (float)tt::CARD_HEIGHT;
  float margin  = 40.0f;

  // Scale to fill the screen with some margin.
  float scale = std::min(
    ((float)screen_w - 2.0f * margin) / thing_w,
    ((float)screen_h - 2.0f * margin) / thing_h
  );

  // draw_thing draws centered at origin; translate to the screen center.
  float cx = (float)screen_w / 2.0f;
  float cy = (float)screen_h / 2.0f;

  const Thing& thing = state.things[thing_id];
  rlPushMatrix();
  rlTranslatef(cx, cy, 0.0f);
  rlScalef(scale, scale, 1.0f);
  draw_thing(thing, face_up);
  auto cb = state.draw_callbacks.find(thing_id);
  if (cb != state.draw_callbacks.end()) cb->second(state, input, face_up);
  rlPopMatrix();
}

// --- draw_table ---

void draw_table(Table_State& state, const Input& input) {
  static auto target_transforms = std::vector<Transform2D>();
  target_transforms.resize(state.things.size());
  for (size_t i = 0; i < target_transforms.size(); i++) {
    target_transforms[i] = state.things[i].transform;
  }
  if (state.world_transforms.size() != state.things.size()) {
    state.world_transforms.resize(state.things.size());
  }
  update_world_transforms(
    state.root,
    Transform2D{},
    state.things,
    target_transforms,
    state.world_transforms
  );

  if (state.world_transforms_animated.size() != state.things.size()) {
    state.world_transforms_animated = state.world_transforms;
  }
#if 0
  state.world_transforms_animated = state.world_transforms;
#else
  animate(
    state.world_transforms_animated,
    state.world_transforms,
    state,
    input.delta_time
  );
#endif

  // Highlight the hovered drop target while dragging.
  if (!state.drag_state.hovered_thing.empty()) {
    draw_drop_placeholder(state.drag_state.hovered_id(), state);
  }

  // Depth-sort root's children so layered draw order is preserved.
  // const Thing&     root_thing = state.things[state.root];
  // std::vector<int> draw_order = root_thing.children;
  // std::sort(draw_order.begin(), draw_order.end(), [&state](int a, int b) {
  //   return state.things[a].depth < state.things[b].depth;
  // });

  // Optional root draw callback — runs before any child, so it paints behind
  // everything (useful as a table background / playmat). face_up is reported
  // as true since the root is the scene container, not a card.
  // auto root_cb = state.draw_callbacks.find(state.root);
  // if (root_cb != state.draw_callbacks.end()) {
  //   root_cb->second(state, input, true);
  // }

  draw_thing_world(state.root, state, state.things[state.root].face_up, input);

  // Dragged thing overlay: drawn last so it sits above everything else.
  int dragged = state.drag_state.thing_id();
  if (dragged >= 0) {
    bool face_up = true;
    int  orig    = state.drag_state.parent_id();
    if (orig >= 0 && orig != state.root) face_up = state.things[orig].face_up;
    rlPushMatrix();
    apply_world_transform(state.world_transforms[dragged]);
    draw_thing(state.things[dragged], face_up);
    auto cb = state.draw_callbacks.find(dragged);
    if (cb != state.draw_callbacks.end()) cb->second(state, input, face_up);
    rlPopMatrix();
  }

  // Optional table-level draw callback (custom HUD overlays), keyed by -1.
  // No specific thing here, so face_up is reported as true.
  auto hud_it = state.draw_callbacks.find(-1);
  if (hud_it != state.draw_callbacks.end()) {
    hud_it->second(state, input, true);
  }

  // Zoomed thing on top of everything. zoomed_thing_id is a path [root, ...,
  // thing] when set; its direct parent (path[size-2]) is the owning parent.
  if (!state.zoomed_thing_id.empty()) {
    int  thing_id = state.zoomed_thing_id.back();
    bool face_up  = true;
    if (state.zoomed_thing_id.size() >= 2) {
      int owner = state.zoomed_thing_id[state.zoomed_thing_id.size() - 2];
      face_up   = state.things[owner].face_up;
    }
    draw_zoomed_thing(state, input, thing_id, face_up);
  }
}

void run_tabletop(
  Table_State&                                    table,
  std::function<bool(Table_State&, const Input&)> update,
  Input_Feed&                                     input_feed,
  int                                             window_width,
  int                                             window_height,
  const std::string&                              window_name
) {
  bool owns_window = !IsWindowReady();
  if (owns_window) {
    // Request 4x multisampling and high-DPI so on Retina displays the GL
    // framebuffer is created at physical pixel resolution (2x logical) —
    // effectively free supersampling on top of MSAA.
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(window_width, window_height, window_name.c_str());
    SetTargetFPS(tt::TARGET_FPS);
  }

  while (!WindowShouldClose()) {
    auto input = next_input(input_feed);
    process_input(table, input);

    BeginDrawing();
    draw_background(input);
    draw_table(table, input);

    // Game logic runs after rendering so that world_transforms (refreshed
    // inside draw_table) are current when the update needs them — e.g. to
    // recompute local transforms after re-parenting a card.
    bool end = update(table, input);
    if (end) break;
    EndDrawing();
  }

  if (owns_window) CloseWindow();
}

void run_tabletop(
  Table_State&                                    table,
  std::function<bool(Table_State&, const Input&)> update,
  int                                             window_width,
  int                                             window_height,
  const std::string&                              window_name
) {
  auto live_feed = Input_Feed{Input_Mode::Live, ""};
  run_tabletop(
    table, update, live_feed, window_width, window_height, window_name
  );
}