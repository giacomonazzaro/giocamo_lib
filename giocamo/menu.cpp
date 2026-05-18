#include "menu.h"

#include <raylib.h>
#include <tabletop/config.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

Menu_Result run_menu(
  const char* title, int window_width, int window_height, Input_Feed&
) {
  InitWindow(window_width, window_height, title);
  // FLAG_WINDOW_HIGHDPI is not implemented on PLATFORM_WEB (GetWindowScaleDPI
  // returns {1,1}).  Resize the canvas pixel buffer to physical resolution and
  // pin the CSS size to logical dimensions so the game fills the viewport at
  // full Retina sharpness.  The per-frame projection fix in draw_background
  // maps the logical 1700×1000 coordinate space to the physical canvas.
  double dpr = emscripten_get_device_pixel_ratio();
  if (dpr > 1.0) {
    emscripten_set_canvas_element_size(
      "#canvas", (int)(tt::WINDOW_WIDTH * dpr), (int)(tt::WINDOW_HEIGHT * dpr)
    );
    EM_ASM(
      {
        var c          = document.getElementById('canvas');
        c.style.width  = $0 + 'px';
        c.style.height = $1 + 'px';
      },
      tt::WINDOW_WIDTH,
      tt::WINDOW_HEIGHT
    );
  }
  SetTargetFPS(tt::TARGET_FPS);
  return Menu_Result{};  // Default mode is VS_AI.
}

#else

#include <tabletop/rendering.h>
#include <tabletop/ui.h>

#include <algorithm>
#include <cstdlib>
#include <memory>

// Keeps the UDP socket alive for the process lifetime. online_lib hands us a
// shared_ptr; we park one copy here so main()/Online can hold a raw pointer
// without worrying about ownership. Confined to this file so the rest of
// gods_app only sees raw pointers.
static std::shared_ptr<UDP_Socket> s_socket_keep_alive;

enum class Screen { MAIN, ONLINE, CREATING, JOINING, CONNECTING };

struct Menu_State {
  Screen                            screen = Screen::MAIN;
  std::string                       text_input;
  std::shared_ptr<Connection_State> connection;
  std::string                       error_message;
};

static void draw_text_input(
  const std::string& label,
  const std::string& text,
  int                y,
  int                window_width = 1700,
  int                width        = 380,
  int                height       = 52
) {
  int x       = (window_width - width) / 2;
  int label_w = text_width(label, 18);
  render_text(
    label,
    (float)(window_width - label_w) / 2.0f,
    (float)(y - 30),
    18,
    Color{200, 200, 200, 255}
  );
  DrawRectangleRounded(
    Rectangle{(float)x, (float)y, (float)width, (float)height},
    0.2f,
    8,
    ::Color{30, 30, 50, 220}
  );
  DrawRectangleRoundedLinesEx(
    Rectangle{(float)x, (float)y, (float)width, (float)height},
    0.2f,
    8,
    2.0f,
    ::Color{140, 140, 200, 255}
  );
  bool        blink_on = ((int)(GetTime() * 2) % 2) == 0;
  std::string display  = text + (blink_on ? "_" : " ");
  render_text(
    display,
    (float)(x + 12),
    (float)y + (float)(height - 24) / 2.0f,
    24,
    Color{255, 255, 255, 255}
  );
}

static void update_text_input(
  const Input& input, std::string& text, size_t max_length = 16
) {
  for (char c : input.chars_typed) {
    if (text.size() < max_length) text.push_back(c);
  }
  if (key_pressed(input, KEY_BACKSPACE) && !text.empty()) text.pop_back();
}

static bool is_super_down(const Input& input) {
#ifdef __APPLE__
  return key_down(input, KEY_LEFT_SUPER) || key_down(input, KEY_RIGHT_SUPER);
#else
  return key_down(input, KEY_LEFT_CONTROL) ||
         key_down(input, KEY_RIGHT_CONTROL);
#endif
}

inline bool menu_button(
  Rectangle rect, const std::string& label, const Input& input
) {
  return immediate_button(
    rect,
    label,
    input,
    {0, 0, 0, 0},
    {20, 20, 20, 200},
    {255, 255, 255, 255},
    30
  );
}

Menu_Result run_menu(
  const std::string& title,
  int                window_width,
  int                window_height,
  Input_Feed&        inputs
) {
  int W = window_width;
  int H = window_height;

  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  InitWindow(W, H, title.c_str());
  SetTargetFPS(tt::TARGET_FPS);

  Menu_State state;

  while (!WindowShouldClose()) {
    Input input = next_input(inputs);
    if (inputs.exhausted) {
      // Playback ran out of frames before the user chose a mode; bail out so
      // main() can act as if the menu was skipped (defaults to VS_AI).
      EndDrawing();
      return Menu_Result{};
    }

    // Text input for the JOINING screen.
    if (state.screen == Screen::JOINING) {
      if (is_super_down(input) && key_pressed(input, KEY_V)) {
        const char* clip = GetClipboardText();
        if (clip) state.text_input = (state.text_input + clip).substr(0, 16);
      } else {
        update_text_input(input, state.text_input);
      }
      if (key_pressed(input, KEY_ENTER) && !state.text_input.empty()) {
        state.connection = join_room(state.text_input);
        state.screen     = Screen::CONNECTING;
      }
    }

    if (state.screen == Screen::CREATING && state.connection) {
      std::string code;
      {
        std::lock_guard<std::mutex> lg(state.connection->state_lock);
        code = state.connection->room_code;
      }
      if (!code.empty() && is_super_down(input) && key_pressed(input, KEY_C)) {
        SetClipboardText(code.c_str());
      }
    }

    // Poll async connection result.
    if (state.connection) {
      if (state.connection->ready.load()) {
        Menu_Result r;
        r.mode = Menu_Result::ONLINE;
        std::lock_guard<std::mutex> lg(state.connection->state_lock);
        r.player_index      = state.connection->player_index;
        r.seed              = state.connection->seed;
        s_socket_keep_alive = state.connection->sock;
        r.online            = {
          s_socket_keep_alive.get(),
          {state.connection->friend_ip, state.connection->friend_port},
        };
        return r;
      }
      std::string err;
      {
        std::lock_guard<std::mutex> lg(state.connection->state_lock);
        err = state.connection->error;
      }
      if (!err.empty()) {
        state.error_message = err;
        state.connection.reset();
        state.screen = Screen::ONLINE;
      }
    }

    auto window_rect = Rectangle{0.0f, 0.0f, (float)W, (float)H};
    BeginDrawing();
    draw_background(input);

    if (state.screen == Screen::MAIN) {
      auto container =
        place_inside(window_rect, 600, 400, "center", "center", 0);

      // Title.
      int  title_width = text_width(title, 90);
      auto title_rect =
        place_inside(container, title_width, 100, "center", "top", 40);
      render_text(
        title, title_rect.x, title_rect.y, 90, Color{255, 255, 255, 255}
      );

      // Play vs AI button.
      auto play_vs_ai_rect =
        place_next(title_rect, 280, 50, "center", "bottom", 50);
      if (menu_button(play_vs_ai_rect, "Play vs AI", input)) {
        Menu_Result r;
        r.mode = Menu_Result::VS_AI;
        // Note: window stays open; main() continues using it.
        EndDrawing();
        return r;
      }

      // Play Online button.
      auto play_online_rect =
        place_next(play_vs_ai_rect, 280, 50, "center", "bottom", 20);
      if (menu_button(play_online_rect, "Play Online", input))
        state.screen = Screen::ONLINE;
    } else if (state.screen == Screen::ONLINE) {
      auto container =
        place_inside(window_rect, 600, 500, "center", "center", 0);

      // Title.
      std::string title       = "PLAY ONLINE";
      int         title_width = text_width(title, 54);
      auto        title_rect =
        place_inside(container, title_width, 60, "center", "top", 20);
      render_text(
        title, title_rect.x, title_rect.y, 54, Color{255, 255, 255, 255}
      );

      // Error message.
      auto button_start_rect = title_rect;
      if (!state.error_message.empty()) {
        int  error_width = text_width(state.error_message, 18);
        auto error_rect =
          place_next(title_rect, error_width, 24, "center", "bottom", 40);
        render_text(
          state.error_message,
          error_rect.x,
          error_rect.y,
          18,
          Color{255, 100, 100, 255}
        );
        button_start_rect = error_rect;
      }

      // Create Game button.
      auto create_game_rect =
        place_next(button_start_rect, 280, 50, "center", "bottom", 60);
      if (menu_button(create_game_rect, "Create Game", input)) {
        state.error_message.clear();
        state.connection = start_hosting();
        state.screen     = Screen::CREATING;
      }

      // Join Game button.
      auto join_game_rect =
        place_next(create_game_rect, 280, 50, "center", "bottom", 20);
      if (menu_button(join_game_rect, "Join Game", input)) {
        state.error_message.clear();
        state.text_input.clear();
        state.screen = Screen::JOINING;
      }

      // Back button.
      auto back_rect =
        place_next(join_game_rect, 180, 46, "center", "bottom", 20);
      if (menu_button(back_rect, "Back", input)) {
        state.error_message.clear();
        state.screen = Screen::MAIN;
      }
    } else if (state.screen == Screen::CREATING) {
      auto container =
        place_inside(window_rect, 700, 500, "center", "center", 0);

      // Title.
      std::string title       = "CREATE GAME";
      int         title_width = text_width(title, 54);
      auto        title_rect =
        place_inside(container, title_width, 60, "center", "top", 20);
      render_text(
        title, title_rect.x, title_rect.y, 54, Color{255, 255, 255, 255}
      );

      std::string code;
      if (state.connection) {
        std::lock_guard<std::mutex> lg(state.connection->state_lock);
        code = state.connection->room_code;
      }

      // Instruction text.
      std::string instruction       = "Share this code with your friend:";
      int         instruction_width = text_width(instruction, 20);
      auto        instruction_rect =
        place_next(title_rect, instruction_width, 24, "center", "bottom", 40);
      render_text(
        instruction,
        instruction_rect.x,
        instruction_rect.y,
        20,
        Color{200, 200, 200, 255}
      );

      // Room code (or placeholder dots while waiting for it).
      std::string code_display = code.empty() ? "...." : code;
      int         code_width   = text_width(code_display, 50);
      auto        code_rect =
        place_next(instruction_rect, code_width, 60, "center", "bottom", 30);
      render_text(
        code_display, code_rect.x, code_rect.y, 50, Color{255, 215, 0, 255}
      );

      // Copy Code button.
      auto copy_button_rect =
        place_next(code_rect, 200, 44, "center", "bottom", 30);
      if (menu_button(copy_button_rect, "Copy Code", input) && !code.empty()) {
        SetClipboardText(code.c_str());
      }

      // Back button.
      auto back_rect =
        place_next(copy_button_rect, 180, 46, "center", "bottom", 60);
      if (menu_button(back_rect, "Back", input)) {
        state.connection.reset();
        state.screen = Screen::ONLINE;
      }
    } else if (state.screen == Screen::JOINING) {
      auto container =
        place_inside(window_rect, 600, 500, "center", "center", 0);

      // Title.
      std::string title       = "JOIN GAME";
      int         title_width = text_width(title, 54);
      auto        title_rect =
        place_inside(container, title_width, 60, "center", "top", 20);
      render_text(
        title, title_rect.x, title_rect.y, 54, Color{255, 255, 255, 255}
      );

      // Text input for room code.
      draw_text_input(
        "Enter room code:", state.text_input, (int)(title_rect.y + 100), W
      );

      // Paste button.
      auto paste_rect =
        place_next(title_rect, 160, 44, "center", "bottom", 130);
      if (menu_button(paste_rect, "Paste", input)) {
        const char* clip = GetClipboardText();
        if (clip) state.text_input = (state.text_input + clip).substr(0, 16);
      }

      // Connect button.
      auto connect_rect =
        place_next(paste_rect, 280, 50, "center", "bottom", 20);
      if (menu_button(connect_rect, "Connect", input) &&
          !state.text_input.empty()) {
        state.connection = join_room(state.text_input);
        state.screen     = Screen::CONNECTING;
      }

      // Back button.
      auto back_rect =
        place_next(connect_rect, 180, 46, "center", "bottom", 20);
      if (menu_button(back_rect, "Back", input)) {
        state.text_input.clear();
        state.screen = Screen::ONLINE;
      }
    } else if (state.screen == Screen::CONNECTING) {
      auto container =
        place_inside(window_rect, 600, 400, "center", "center", 0);

      // Title.
      std::string title       = "JOIN GAME";
      int         title_width = text_width(title, 54);
      auto        title_rect =
        place_inside(container, title_width, 60, "center", "top", 20);
      render_text(
        title, title_rect.x, title_rect.y, 54, Color{255, 255, 255, 255}
      );

      // Connecting message.
      std::string connecting       = "Connecting...";
      int         connecting_width = text_width(connecting, 30);
      auto        connecting_rect =
        place_next(title_rect, connecting_width, 35, "center", "bottom", 80);
      render_text(
        connecting,
        connecting_rect.x,
        connecting_rect.y,
        30,
        Color{200, 200, 200, 255}
      );

      // Back button.
      auto back_rect =
        place_next(connecting_rect, 180, 46, "center", "bottom", 80);
      if (menu_button(back_rect, "Back", input)) {
        state.connection.reset();
        state.screen = Screen::JOINING;
      }
    }

    EndDrawing();
  }

  // User closed window during the menu.
  CloseWindow();
  std::exit(0);
}

#endif  // !__EMSCRIPTEN__
