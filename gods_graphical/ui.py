from __future__ import annotations
import os
from dataclasses import dataclass, field

from pyray import *

from kitchen_table.config import tweak
from kitchen_table.rendering import draw_table, draw_background, color_from_tuple
import kitchen_table.models as kt

IMAGES_DIR = os.path.join(os.path.dirname(__file__), "..", "gods", "cards", "card-images")


def get_table_layout(bottom_player: int = 0):
    """Return stack layout definitions for the card table.

    Returns list of (zone_name, x, y, width, spread_x, spread_y, face_up) tuples.
    Zone names: p{i}_deck, p{i}_hand, p{i}_discard, p{i}_wonders, peoples.
    bottom_player determines which player's cards appear at the bottom.
    Positions are computed adaptively from window dimensions.
    """
    W = tweak["window_width"]
    H = tweak["window_height"]
    w = tweak["card_width"]
    h = tweak["card_height"]
    margin = 20

    spread_hand = tweak["hand_spread_x"]
    spread_wonders = tweak["wonders_spread_x"]
    spread_pile = tweak["pile_spread_y"]

    # Vertical: bottom player from the bottom edge
    bottom_hand_y = H - h - margin
    bottom_deck_y = bottom_hand_y
    bottom_wonders_y = bottom_hand_y - h - margin

    # Vertical: top player mirrored and pushed partially offscreen
    opponent_shift = int(h * 0.65)
    top_hand_y = margin - opponent_shift
    top_deck_y = top_hand_y
    top_wonders_y = H - bottom_wonders_y - h - opponent_shift

    # Peoples centered vertically
    peoples_y = H // 2 - h // 2

    # Horizontal: piles on the left, spread cards fill the rest
    discard_x = margin
    deck_x = margin + w + margin
    right_start = deck_x + w + margin * 2
    right_width = W - right_start - margin
    peoples_x = margin
    peoples_width = 3 * (w + 10)

    shared_deck_x = -w
    shared_deck_y = peoples_y

    bp = f"p{bottom_player}"
    tp = f"p{1 - bottom_player}"

    return [
        (f"{bp}_deck",    deck_x,        bottom_deck_y,    w,             0,              spread_pile, False),
        (f"{bp}_hand",    right_start,   bottom_hand_y,    right_width,   spread_hand,    0,           True),
        (f"{bp}_discard", discard_x,     bottom_deck_y,    w,             0,              spread_pile, True),
        (f"{bp}_wonders", right_start,   bottom_wonders_y, right_width,   spread_wonders, 0,           True),
        (f"{tp}_deck",    deck_x,        top_deck_y,       w,             0,              spread_pile, False),
        (f"{tp}_hand",    right_start,   top_hand_y,       right_width,   spread_hand,    0,           not False),
        (f"{tp}_discard", discard_x,     top_deck_y,       w,             0,              spread_pile, True),
        (f"{tp}_wonders", right_start,   top_wonders_y,    right_width,   spread_wonders, 0,           True),
        ("peoples",       peoples_x,     peoples_y,        peoples_width, spread_wonders, 0,           True),
        ("shared_deck",   shared_deck_x, shared_deck_y,    w,             0,              0,           True),
    ]

OWNER_COLORS = [
    (100, 200, 100, 200),  # Player 0 - green
    (200, 100, 100, 200),  # Player 1 - red
]


def get_image_path(card_name: str) -> str | None:
    filename = card_name.lower().replace(" ", "_") + ".jpg"
    path = os.path.join(IMAGES_DIR, filename)
    if os.path.exists(path):
        return path
    return None


def point_in_rect(mx: float, my: float, x: float, y: float, w: float, h: float) -> bool:
    return x <= mx <= x + w and y <= my <= y + h


@dataclass
class Button:
    x: int
    y: int
    width: int
    height: int
    text: str = ""

    def pressed(self, mx, my, click) -> bool:
        if not click:
            return False
        return point_in_rect(mx, my, self.x, self.y, self.width, self.height)



# --- Card rendering ---

def draw_card_power_badge(power: str, destroyed: bool):
    w = tweak["card_width"]
    h = tweak["card_height"]
    r = tweak["card_corner_radius"]

    draw_circle(
        int(0.88 * w), int(0.12 * w), int(0.12 * w), Color(255, 255, 255, 255)
    )
    draw_text(
        power, int(0.83 * w), int(0.03 * w), int(0.2 * w), Color(0, 0, 0, 255)
    )

    if destroyed:
        draw_rectangle_rounded(
            Rectangle(0, 0, w, h), r / min(w, h), 8, (0, 0, 0, 100)
        )


# --- Button and highlight rendering ---

def draw_buttons(buttons: list):
    mx, my = get_mouse_x(), get_mouse_y()
    for button in buttons:
        hovered = (button.x <= mx <= button.x + button.width
                   and button.y <= my <= button.y + button.height)
        color_key = "button_hover_color" if hovered else "button_color"
        color = color_from_tuple(tweak[color_key])
        draw_rectangle_rounded(
            Rectangle(button.x, button.y, button.width, button.height), 0.3, 8, color
        )
        text_width = measure_text(button.text, 20)
        text_x = button.x + (button.width - text_width) // 2
        text_y = button.y + (button.height - 20) // 2
        draw_text(button.text, text_x, text_y, 20, color_from_tuple(tweak["button_text_color"]))


def draw_card_highlights(kt_card_ids: list[int], table_state: kt.Table_State):
    if table_state is None or not table_state.animated_cards:
        return
    w = tweak["card_width"]
    h = tweak["card_height"]
    highlight_color = color_from_tuple(tweak["highlight_color"])
    for card_id in kt_card_ids:
        kt_card = table_state.animated_cards[card_id]
        draw_rectangle_rounded_lines_ex(
            Rectangle(kt_card.x, kt_card.y, w, h), 0.25, 8, 4, highlight_color
        )


# --- HUD rendering ---

def draw_player_hud(name: str, score: int, deck_count: int, is_current: bool, hud_y: int):
    w = tweak["card_width"]
    margin = 20

    # Current player indicator bar
    if is_current:
        indicator_color = color_from_tuple(tweak["current_player_color"])
        draw_rectangle_rounded(
            Rectangle(10, hud_y - 5, 6, 50), 0.5, 4, indicator_color
        )

    # Player name
    name_color = color_from_tuple(tweak["current_player_color"]) if is_current else Color(255, 255, 255, 255)
    draw_text(name, 25, hud_y, 24, name_color)

    # Score
    draw_text(f"Score: {score}", 25, hud_y + 28, 20, Color(200, 200, 200, 255))

    # Deck count near deck stack
    deck_x = margin + w + margin
    draw_text(
        str(deck_count),
        deck_x + w // 2 - 10,
        hud_y + 18 if hud_y < 400 else hud_y - 30,
        18,
        Color(180, 180, 180, 255),
    )


def draw_people_ownership_bars(people_info: list[tuple[int, int]], table_state: kt.Table_State):
    """Draw colored ownership bars on people cards.
    people_info: list of (kt_card_id, owner_index) for owned, non-destroyed people.
    """
    if not table_state.animated_cards:
        return
    w = tweak["card_width"]
    h = tweak["card_height"]
    for kt_card_id, owner_index in people_info:
        kt_card = table_state.animated_cards[kt_card_id]
        draw_rectangle_rounded(
            Rectangle(kt_card.x + 4, kt_card.y + h - 12, w - 8, 8),
            0.5, 4, OWNER_COLORS[owner_index],
        )


def draw_final_round_indicator():
    text = "FINAL ROUND"
    text_w = measure_text(text, 24)
    W = get_screen_width()
    H = get_screen_height()
    h = tweak["card_height"]
    peoples_y = H // 2 - h // 2
    draw_text(
        text, (W - text_w) // 2, peoples_y - 30, 24,
        Color(255, 180, 0, 200),
    )


# --- Game over screen ---

def draw_game_over_screen(table_state: kt.Table_State, result_text: str,
                          player_names: list[str], scores: list[int]):
    w_width = get_screen_width()
    w_height = get_screen_height()

    while not window_should_close():
        begin_drawing()
        draw_background()
        draw_table(table_state)

        # Semi-transparent overlay
        draw_rectangle(0, 0, w_width, w_height, color_from_tuple(tweak["modal_overlay"]))

        title_w = measure_text("GAME OVER", 60)
        draw_text("GAME OVER", (w_width - title_w) // 2, 350, 60, Color(255, 255, 255, 255))

        result_w = measure_text(result_text, 40)
        draw_text(result_text, (w_width - result_w) // 2, 430, 40, Color(255, 215, 0, 255))

        score_text = f"{player_names[0]}: {scores[0]}  |  {player_names[1]}: {scores[1]}"
        score_w = measure_text(score_text, 30)
        draw_text(score_text, (w_width - score_w) // 2, 490, 30, Color(200, 200, 200, 255))

        end_drawing()
