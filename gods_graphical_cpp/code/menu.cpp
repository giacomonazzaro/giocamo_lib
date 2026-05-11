#include "menu.h"

#include <algorithm>
#include <cstdlib>

#include <raylib.h>

#include <kitchen_table_cpp/code/config.h>
#include <kitchen_table_cpp/code/rendering.h>
#include <kitchen_table_cpp/code/ui.h>

enum class Screen { MAIN, ONLINE, CREATING, JOINING, CONNECTING };

struct Menu_State {
  Screen                            screen = Screen::MAIN;
  std::string                       text_input;
  std::shared_ptr<Connection_State> connection;
  std::string                       error_message;
};

static int centered_x(int width) {
  return (kt::WINDOW_WIDTH - width) / 2;
}

static void draw_centered_text(const std::string& text, int y, int font_size, KT_Color color) {
  int w = text_width(text, font_size);
  render_text(text, (float)centered_x(w), (float)y, font_size, color);
}

static bool draw_button(const std::string& text, int y, int width = 320, int height = 58) {
  int mx = GetMouseX();
  int my = GetMouseY();
  int x  = centered_x(width);
  bool hovered = (x <= mx && mx <= x + width && y <= my && my <= y + height);

  if (hovered) {
    DrawRectangleRounded(
      Rectangle{(float)x, (float)y, (float)width, (float)height},
      0.3f, 8, ::Color{20, 20, 20, 100}
    );
  }

  int size = 30;
  int tw   = text_width(text, size);
  render_text(text, (float)(x + (width - tw) / 2), (float)(y + (height - size) / 2),
              size, KT_Color{255, 255, 255, 255});

  return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_text_input(const std::string& label, const std::string& text, int y,
                            int width = 380, int height = 52) {
  int x = centered_x(width);
  int label_w = text_width(label, 18);
  render_text(label, (float)centered_x(label_w), (float)(y - 30), 18,
              KT_Color{200, 200, 200, 255});
  DrawRectangleRounded(
    Rectangle{(float)x, (float)y, (float)width, (float)height},
    0.2f, 8, ::Color{30, 30, 50, 220}
  );
  DrawRectangleRoundedLinesEx(
    Rectangle{(float)x, (float)y, (float)width, (float)height},
    0.2f, 8, 2.0f, ::Color{140, 140, 200, 255}
  );
  bool blink_on = ((int)(GetTime() * 2) % 2) == 0;
  std::string display = text + (blink_on ? "_" : " ");
  render_text(display, (float)(x + 12), (float)(y + (height - 24) / 2), 24,
              KT_Color{255, 255, 255, 255});
}

static void update_text_input(std::string& text, size_t max_length = 16) {
  int c = GetCharPressed();
  while (c) {
    if (text.size() < max_length && c >= 32 && c < 127) {
      text += (char)c;
    }
    c = GetCharPressed();
  }
  if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) text.pop_back();
}

static std::string dots() {
  int n = (int)(GetTime() * 2) % 4;
  return std::string(n, '.');
}

static bool is_super_down() {
#ifdef __APPLE__
  return IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
#else
  return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
#endif
}

Menu_Result run_menu() {
  int W = kt::WINDOW_WIDTH;
  int H = kt::WINDOW_HEIGHT;

  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  InitWindow(W, H, "Gods");
  SetTargetFPS(kt::TARGET_FPS);

  Menu_State state;
  int center_y = H / 2;

  while (!WindowShouldClose()) {
    // Text input for the JOINING screen.
    if (state.screen == Screen::JOINING) {
      if (is_super_down() && IsKeyPressed(KEY_V)) {
        const char* clip = GetClipboardText();
        if (clip) state.text_input = (state.text_input + clip).substr(0, 16);
      } else {
        update_text_input(state.text_input);
      }
      if (IsKeyPressed(KEY_ENTER) && !state.text_input.empty()) {
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
      if (!code.empty() && is_super_down() && IsKeyPressed(KEY_C)) {
        SetClipboardText(code.c_str());
      }
    }

    // Poll async connection result.
    if (state.connection) {
      if (state.connection->ready.load()) {
        Menu_Result r;
        r.mode = Menu_Result::ONLINE;
        std::lock_guard<std::mutex> lg(state.connection->state_lock);
        r.player_index = state.connection->player_index;
        r.seed         = state.connection->seed;
        r.sock         = state.connection->sock;
        r.friend_addr  = {state.connection->friend_ip, state.connection->friend_port};
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
    draw_background();

    if (state.screen == Screen::MAIN) {
      draw_centered_text("GODS", center_y - 180, 90, KT_Color{255, 255, 255, 255});
      if (draw_button("Play vs AI",  center_y - 20)) {
        Menu_Result r;
        r.mode = Menu_Result::VS_AI;
        // Note: window stays open; main() continues using it.
        EndDrawing();
        return r;
      }
      if (draw_button("Play Online", center_y + 60)) state.screen = Screen::ONLINE;

    } else if (state.screen == Screen::ONLINE) {
      draw_centered_text("PLAY ONLINE", 150, 54, KT_Color{255, 255, 255, 255});
      if (!state.error_message.empty()) {
        draw_centered_text(state.error_message, center_y - 120, 18, KT_Color{255, 100, 100, 255});
      }
      if (draw_button("Create Game", center_y - 60)) {
        state.error_message.clear();
        state.connection = start_hosting();
        state.screen     = Screen::CREATING;
      }
      if (draw_button("Join Game", center_y + 20)) {
        state.error_message.clear();
        state.text_input.clear();
        state.screen = Screen::JOINING;
      }
      if (draw_button("Back", center_y + 120, 180, 46)) {
        state.error_message.clear();
        state.screen = Screen::MAIN;
      }

    } else if (state.screen == Screen::CREATING) {
      draw_centered_text("CREATE GAME", 150, 54, KT_Color{255, 255, 255, 255});
      std::string code;
      if (state.connection) {
        std::lock_guard<std::mutex> lg(state.connection->state_lock);
        code = state.connection->room_code;
      }
      if (!code.empty()) {
        draw_centered_text("Share this code with your friend:", center_y - 80, 20,
                           KT_Color{200, 200, 200, 255});
        draw_centered_text(code, center_y - 30, 50, KT_Color{255, 215, 0, 255});
        if (draw_button("Copy Code", center_y + 30, 200, 44)) {
          SetClipboardText(code.c_str());
        }
        draw_centered_text("Waiting for opponent" + dots(), center_y + 90, 22,
                           KT_Color{180, 180, 180, 255});
      } else {
        draw_centered_text("Getting your room code" + dots(), center_y, 26,
                           KT_Color{200, 200, 200, 255});
      }
      if (draw_button("Back", center_y + 170, 180, 46)) {
        state.connection.reset();
        state.screen = Screen::ONLINE;
      }

    } else if (state.screen == Screen::JOINING) {
      draw_centered_text("JOIN GAME", 150, 54, KT_Color{255, 255, 255, 255});
      draw_text_input("Enter room code:", state.text_input, center_y - 40);
      if (draw_button("Paste", center_y + 30, 160, 44)) {
        const char* clip = GetClipboardText();
        if (clip) state.text_input = (state.text_input + clip).substr(0, 16);
      }
      if (draw_button("Connect", center_y + 90) && !state.text_input.empty()) {
        state.connection = join_room(state.text_input);
        state.screen     = Screen::CONNECTING;
      }
      if (draw_button("Back", center_y + 160, 180, 46)) {
        state.text_input.clear();
        state.screen = Screen::ONLINE;
      }

    } else if (state.screen == Screen::CONNECTING) {
      draw_centered_text("JOIN GAME", 150, 54, KT_Color{255, 255, 255, 255});
      draw_centered_text("Connecting" + dots(), center_y - 20, 30, KT_Color{200, 200, 200, 255});
      if (draw_button("Back", center_y + 100, 180, 46)) {
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
