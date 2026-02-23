from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto

import pyray

from gods_online.room_code import Connection_State, join_room, start_hosting
from kitchen_table.config import tweak
from kitchen_table.rendering import color_from_tuple, draw_background


class Screen(Enum):
    MAIN = auto()
    ONLINE = auto()
    CREATING = auto()    # Host: displaying room code, waiting for joiner.
    JOINING = auto()     # Joiner: text input for room code.
    CONNECTING = auto()  # Joiner: connecting in progress.


@dataclass
class Menu_State:
    screen: Screen = Screen.MAIN
    text_input: str = ""
    connection: Connection_State | None = None
    error_message: str = ""


# --- Helpers ---

def _centered_x(width: int) -> int:
    return (tweak["window_width"] - width) // 2


def _draw_centered_text(text: str, y: int, font_size: int,
                        color: tuple = (255, 255, 255, 255)) -> None:
    w = pyray.measure_text(text, font_size)
    pyray.draw_text(text, _centered_x(w), y, font_size, pyray.Color(*color))


def _draw_button(text: str, y: int, width: int = 320, height: int = 58) -> bool:
    """Draw a horizontally-centered button and return True if clicked this frame."""
    x = _centered_x(width)
    mx, my = pyray.get_mouse_x(), pyray.get_mouse_y()
    hovered = x <= mx <= x + width and y <= my <= y + height
    color_key = "button_hover_color" if hovered else "button_color"
    bg = color_from_tuple(tweak[color_key])
    fg = color_from_tuple(tweak["button_text_color"])
    pyray.draw_rectangle_rounded(pyray.Rectangle(x, y, width, height), 0.3, 8, bg)
    tw = pyray.measure_text(text, 22)
    pyray.draw_text(text, x + (width - tw) // 2, y + (height - 22) // 2, 22, fg)
    return hovered and pyray.is_mouse_button_pressed(pyray.MouseButton.MOUSE_BUTTON_LEFT)


def _draw_text_input(label: str, text: str, y: int,
                     width: int = 380, height: int = 52) -> None:
    """Draw a centered, labeled text input box with a blinking cursor."""
    x = _centered_x(width)
    label_w = pyray.measure_text(label, 18)
    pyray.draw_text(label, _centered_x(label_w), y - 30, 18, pyray.Color(200, 200, 200, 255))
    pyray.draw_rectangle_rounded(
        pyray.Rectangle(x, y, width, height), 0.2, 8, pyray.Color(30, 30, 50, 220)
    )
    pyray.draw_rectangle_rounded_lines_ex(
        pyray.Rectangle(x, y, width, height), 0.2, 8, 2, pyray.Color(140, 140, 200, 255)
    )
    # Cursor blinks at 1 Hz.
    cursor = "_" if int(pyray.get_time() * 2) % 2 == 0 else " "
    pyray.draw_text(text + cursor, x + 12, y + (height - 24) // 2, 24,
                    pyray.Color(255, 255, 255, 255))


def _update_text_input(text: str, max_length: int = 16) -> str:
    """Consume all pending key presses and return the updated text."""
    char = pyray.get_char_pressed()
    while char:
        if len(text) < max_length and 32 <= char < 127:
            text += chr(char)
        char = pyray.get_char_pressed()
    if pyray.is_key_pressed(pyray.KeyboardKey.KEY_BACKSPACE) and text:
        text = text[:-1]
    return text


def _dots() -> str:
    """Animated ellipsis string cycling through 0-3 dots."""
    return "." * (int(pyray.get_time() * 2) % 4)


# --- Main menu entry point ---

def run_menu() -> tuple[str, dict]:
    """Display the game menu and return the user's game choice.

    Opens a Raylib window that is kept open after this function returns so that
    the game can continue rendering in the same window (play() skips init_window
    when the window is already ready).

    Returns (mode, params) where:
      - mode == "vs_ai"    → params == {}
      - mode == "online"   → params == {player_index, seed, sock, friend_addr}
    """
    W = tweak["window_width"]
    H = tweak["window_height"]

    pyray.set_config_flags(pyray.ConfigFlags.FLAG_WINDOW_HIGHDPI)
    pyray.init_window(W, H, "Gods")
    pyray.set_target_fps(tweak["target_fps"])

    state = Menu_State()
    center_y = H // 2

    while not pyray.window_should_close():
        # --- Per-frame text input ---
        if state.screen == Screen.JOINING:
            state.text_input = _update_text_input(state.text_input)
            if pyray.is_key_pressed(pyray.KeyboardKey.KEY_ENTER) and state.text_input:
                state.connection = join_room(state.text_input)
                state.screen = Screen.CONNECTING

        # --- Poll async connection result ---
        if state.connection is not None:
            if state.connection.ready:
                c = state.connection
                return "online", {
                    "player_index": c.player_index,
                    "seed":         c.seed,
                    "sock":         c.sock,
                    "friend_addr":  c.friend_addr,
                }
            if state.connection.error:
                state.error_message = state.connection.error
                state.connection = None
                state.screen = Screen.ONLINE

        # --- Draw ---
        pyray.begin_drawing()
        draw_background()

        if state.screen == Screen.MAIN:
            _draw_centered_text("GODS", center_y - 180, 90)

            if _draw_button("Play vs AI", center_y - 20):
                return "vs_ai", {}

            if _draw_button("Play Online", center_y + 60):
                state.screen = Screen.ONLINE

        elif state.screen == Screen.ONLINE:
            _draw_centered_text("PLAY ONLINE", 150, 54)

            if state.error_message:
                _draw_centered_text(
                    state.error_message, center_y - 120, 18, (255, 100, 100, 255)
                )

            if _draw_button("Create Game", center_y - 60):
                state.error_message = ""
                state.connection = start_hosting()
                state.screen = Screen.CREATING

            if _draw_button("Join Game", center_y + 20):
                state.error_message = ""
                state.text_input = ""
                state.screen = Screen.JOINING

            if _draw_button("Back", center_y + 120, width=180, height=46):
                state.error_message = ""
                state.screen = Screen.MAIN

        elif state.screen == Screen.CREATING:
            _draw_centered_text("CREATE GAME", 150, 54)

            if state.connection and state.connection.room_code:
                _draw_centered_text(
                    "Share this code with your friend:", center_y - 80, 20,
                    (200, 200, 200, 255)
                )
                # Room code displayed prominently in gold.
                _draw_centered_text(
                    state.connection.room_code, center_y - 30, 50, (255, 215, 0, 255)
                )
                _draw_centered_text(
                    f"Waiting for opponent{_dots()}", center_y + 50, 22,
                    (180, 180, 180, 255)
                )
            else:
                _draw_centered_text(
                    f"Getting your room code{_dots()}", center_y, 26,
                    (200, 200, 200, 255)
                )

            if _draw_button("Back", center_y + 150, width=180, height=46):
                state.connection = None
                state.screen = Screen.ONLINE

        elif state.screen == Screen.JOINING:
            _draw_centered_text("JOIN GAME", 150, 54)
            _draw_text_input("Enter room code:", state.text_input, center_y - 40)

            if _draw_button("Connect", center_y + 50) and state.text_input:
                state.connection = join_room(state.text_input)
                state.screen = Screen.CONNECTING

            if _draw_button("Back", center_y + 130, width=180, height=46):
                state.text_input = ""
                state.screen = Screen.ONLINE

        elif state.screen == Screen.CONNECTING:
            _draw_centered_text("JOIN GAME", 150, 54)
            _draw_centered_text(
                f"Connecting{_dots()}", center_y - 20, 30, (200, 200, 200, 255)
            )

            if _draw_button("Back", center_y + 100, width=180, height=46):
                state.connection = None
                state.screen = Screen.JOINING

        pyray.end_drawing()

    # User closed the window during the menu.
    pyray.close_window()
    raise SystemExit(0)
