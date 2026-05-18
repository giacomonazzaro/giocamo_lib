#include "menu.h"

#include <raylib.h>
#include <tabletop/config.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

Menu_Result run_menu(Input_Feed&) {
  InitWindow(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "Gods");
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
// gods_graphical only sees raw pointers.
static std::shared_ptr<UDP_Socket> s_socket_keep_alive;

enum class Screen { MAIN, ONLINE, CREATING, JOINING, CONNECTING };

struct Menu_State {
  Screen                            screen = Screen::MAIN;
  std::string                       text_input;
  std::shared_ptr<Connection_State> connection;
  std::string                       error_message;
};

static int centered_x(int width) { return (tt::WINDOW_WIDTH - width) / 2; }

static void draw_centered_text(
  const std::string& text, int y, int font_size, Color color
) {
  int w = text_width(text, font_size);
  render_text(text, (float)centered_x(w), (float)y, font_size, color);
}

static bool draw_button(
  const Input&       input,
  const std::string& text,
  int                y,
  int                width  = 320,
  int                height = 58
) {
  int  mx      = input.mouse_x;
  int  my      = input.mouse_y;
  int  x       = centered_x(width);
  bool hovered = (x <= mx && mx <= x + width && y <= my && my <= y + height);

  if (hovered) {
    DrawRectangleRounded(
      Rectangle{(float)x, (float)y, (float)width, (float)height},
      0.3f,
      8,
      ::Color{20, 20, 20, 100}
    );
  }

  int size = 30;
  int tw   = text_width(text, size);
  render_text(
    text,
    (float)(x + (width - tw) / 2),
    (float)(y + (height - size) / 2),
    size,
    Color{255, 255, 255, 255}
  );

  return hovered && input.left_pressed;
}

static void draw_text_input(
  const std::string& label,
  const std::string& text,
  int                y,
  int                width  = 380,
  int                height = 52
) {
  int x       = centered_x(width);
  int label_w = text_width(label, 18);
  render_text(
    label,
    (float)centered_x(label_w),
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
    (float)(y + (height - 24) / 2),
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

static std::string dots() {
  int n = (int)(GetTime() * 2) % 4;
  return std::string(n, '.');
}

static bool is_super_down(const Input& input) {
#ifdef __APPLE__
  return key_down(input, KEY_LEFT_SUPER) || key_down(input, KEY_RIGHT_SUPER);
#else
  return key_down(input, KEY_LEFT_CONTROL) ||
         key_down(input, KEY_RIGHT_CONTROL);
#endif
}

Menu_Result run_menu(int window_width, int window_height, Input_Feed& inputs) {
  int W = window_width;
  int H = window_height;

  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  InitWindow(W, H, "Gods");
  SetTargetFPS(tt::TARGET_FPS);

  Menu_State state;
  int        center_y = H / 2;

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

    BeginDrawing();
    draw_background(input);

    if (state.screen == Screen::MAIN) {
      draw_centered_text("GODS", center_y - 180, 90, Color{255, 255, 255, 255});
      if (draw_button(input, "Play vs AI", center_y - 20)) {
        Menu_Result r;
        r.mode = Menu_Result::VS_AI;
        // Note: window stays open; main() continues using it.
        EndDrawing();
        return r;
      }
      if (draw_button(input, "Play Online", center_y + 60))
        state.screen = Screen::ONLINE;

    } else if (state.screen == Screen::ONLINE) {
      draw_centered_text("PLAY ONLINE", 150, 54, Color{255, 255, 255, 255});
      if (!state.error_message.empty()) {
        draw_centered_text(
          state.error_message, center_y - 120, 18, Color{255, 100, 100, 255}
        );
      }
      if (draw_button(input, "Create Game", center_y - 60)) {
        state.error_message.clear();
        state.connection = start_hosting();
        state.screen     = Screen::CREATING;
      }
      if (draw_button(input, "Join Game", center_y + 20)) {
        state.error_message.clear();
        state.text_input.clear();
        state.screen = Screen::JOINING;
      }
      if (draw_button(input, "Back", center_y + 120, 180, 46)) {
        state.error_message.clear();
        state.screen = Screen::MAIN;
      }

    } else if (state.screen == Screen::CREATING) {
      draw_centered_text("CREATE GAME", 150, 54, Color{255, 255, 255, 255});
      std::string code;
      if (state.connection) {
        std::lock_guard<std::mutex> lg(state.connection->state_lock);
        code = state.connection->room_code;
      }
      if (!code.empty()) {
        draw_centered_text(
          "Share this code with your friend:",
          center_y - 80,
          20,
          Color{200, 200, 200, 255}
        );
        draw_centered_text(code, center_y - 30, 50, Color{255, 215, 0, 255});
        if (draw_button(input, "Copy Code", center_y + 30, 200, 44)) {
          SetClipboardText(code.c_str());
        }
        draw_centered_text(
          "Waiting for opponent" + dots(),
          center_y + 90,
          22,
          Color{180, 180, 180, 255}
        );
      } else {
        draw_centered_text(
          "Getting your room code" + dots(),
          center_y,
          26,
          Color{200, 200, 200, 255}
        );
      }
      if (draw_button(input, "Back", center_y + 170, 180, 46)) {
        state.connection.reset();
        state.screen = Screen::ONLINE;
      }

    } else if (state.screen == Screen::JOINING) {
      draw_centered_text("JOIN GAME", 150, 54, Color{255, 255, 255, 255});
      draw_text_input("Enter room code:", state.text_input, center_y - 40);
      if (draw_button(input, "Paste", center_y + 30, 160, 44)) {
        const char* clip = GetClipboardText();
        if (clip) state.text_input = (state.text_input + clip).substr(0, 16);
      }
      if (draw_button(input, "Connect", center_y + 90) &&
          !state.text_input.empty()) {
        state.connection = join_room(state.text_input);
        state.screen     = Screen::CONNECTING;
      }
      if (draw_button(input, "Back", center_y + 160, 180, 46)) {
        state.text_input.clear();
        state.screen = Screen::ONLINE;
      }

    } else if (state.screen == Screen::CONNECTING) {
      draw_centered_text("JOIN GAME", 150, 54, Color{255, 255, 255, 255});
      draw_centered_text(
        "Connecting" + dots(), center_y - 20, 30, Color{200, 200, 200, 255}
      );
      if (draw_button(input, "Back", center_y + 100, 180, 46)) {
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
