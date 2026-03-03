from __future__ import annotations
import os
from dataclasses import dataclass, field

from pyray import *

from kitchen_table.config import tweak
from kitchen_table.rendering import draw_table, draw_background, color_from_tuple, render_text, text_width
import kitchen_table.models as kt

IMAGES_DIR = os.path.join(os.path.dirname(__file__), "..", "gods", "cards", "card-images")


@dataclass(slots=True)
class Zone_Layout:
    x: int
    y: int
    width: int
    spread_x: int
    spread_y: int
    face_up: bool


def get_table_layout(bottom_player: int = 0) -> dict[str, Zone_Layout]:
    """Return stack layout definitions for the card table.

    Returns a dict mapping zone name to Zone_Layout.
    Zone names: p{i}_deck, p{i}_hand, p{i}_discard, p{i}_wonders, peoples.
    bottom_player determines which player's cards appear at the bottom.
    Positions are computed adaptively from window dimensions.
    """
    W = tweak["window_width"]
    H = tweak["window_height"]
    w = tweak["card_width"]
    h = tweak["card_height"]
    margin = 20

    spread_hand = 160
    spread_wonders = 160
    spread_pile = -3

    # Vertical: bottom player from the bottom edge
    hand_width = w * 5.5 * W / 1600
    hand_x = W // 2 - hand_width // 2 + 170
    bottom_hand_y = H - h - margin
    bottom_deck_y = bottom_hand_y
    bottom_wonders_y = bottom_hand_y - h - margin

    # Vertical: top player mirrored and pushed partially offscreen
    opponent_shift = int(h * 0.65)
    top_hand_y = margin - opponent_shift
    top_deck_y = top_hand_y
    top_wonders_y = H - bottom_wonders_y - h - opponent_shift

    # Horizontal: piles on the left, peoples then wonders on the right.
    discard_x = margin
    deck_x = margin + w + margin
    right_start = margin # + w + margin * 2
    # Peoples area sits to the left of wonders; wide enough for up to 2 cards at full spread.
    peoples_width = 2 * w + spread_wonders
    wonders_start = hand_x
    wonders_width = hand_width

    shared_deck_x = -w
    shared_deck_y = H // 2 - h // 2

    bp = f"p{bottom_player}"
    tp = f"p{1 - bottom_player}"

    Z = Zone_Layout
    return {
        f"{bp}_deck":    Z(deck_x,        bottom_deck_y,    w,                        0,              spread_pile, False),
        f"{bp}_hand":    Z(hand_x,   bottom_hand_y,         hand_width, spread_hand,    0,           True),
        f"{bp}_discard": Z(discard_x,     bottom_deck_y,    w,                        0,              spread_pile, True),
        f"{bp}_peoples": Z(right_start,   bottom_wonders_y, peoples_width,            spread_wonders, 0,           True),
        f"{bp}_wonders": Z(wonders_start, bottom_wonders_y, wonders_width,            spread_wonders, 0,           True),
        f"{tp}_deck":    Z(deck_x,        top_deck_y,       w,                        0,              spread_pile, False),
        f"{tp}_hand":    Z(hand_x,   top_hand_y,            hand_width, spread_hand,    0,           False),
        f"{tp}_discard": Z(discard_x,     top_deck_y,       w,                        0,              spread_pile, True),
        f"{tp}_peoples": Z(right_start,   top_wonders_y,    peoples_width,            spread_wonders, 0,           True),
        f"{tp}_wonders": Z(wonders_start, top_wonders_y,    wonders_width,            spread_wonders, 0,           True),
        "shared_deck":   Z(shared_deck_x, shared_deck_y,    w,                        0,              0,           True),
    }


def get_image_path(card_name: str) -> str | None:
    filename = card_name.lower().replace(" ", "_") + ".jpg"
    path = os.path.join(IMAGES_DIR, filename)
    if os.path.exists(path):
        return path
    return None


def point_in_rect(mx: float, my: float, x: float, y: float, w: float, h: float) -> bool:
    return x <= mx <= x + w and y <= my <= y + h



# --- Card rendering ---

def draw_card_power_badge(power: str, destroyed: bool):
    w = tweak["card_width"]
    h = tweak["card_height"]
    r = tweak["card_corner_radius"]

    # Badge circle in the top-right corner of the card.
    badge_cx = int(0.88 * w)
    badge_cy = int(0.12 * w)
    badge_r  = int(0.12 * w)
    draw_circle(badge_cx, badge_cy, badge_r, Color(255, 255, 255, 255))

    # Center the power number on the badge circle.
    size = int(0.2 * w)
    tw = text_width(power, size)
    render_text(power, badge_cx - tw // 2, badge_cy - size // 2, size, Color(0, 0, 0, 255))

    if destroyed:
        draw_rectangle_rounded(
            Rectangle(0, 0, w, h), r / min(w, h), 8, (0, 0, 0, 100)
        )


# --- Choice description rendering ---

# def draw_choice_description(text: str):
#     """Draw the current choice description on the right side of the screen."""
#     if not text:
#         return
#     W = tweak["window_width"]
#     H = tweak["window_height"]
#     font_size = 22
#     tw = text_width(text, font_size)
#     x = W - tw - 20
#     y = H // 2 - font_size // 2
#     render_text(text, x, y, font_size, Color(200, 200, 200, 200))


# --- HUD rendering ---

def draw_player_hud(name: str, score: int, deck_count: int, is_current: bool, hud_y: int):
    w = tweak["card_width"]
    margin = 20

    # Current player indicator bar.
    if is_current:
        draw_rectangle_rounded(
            Rectangle(tweak["window_width"] - 10, hud_y + 28, 6, 50), 0.5, 4, (255, 255, 255, 255)
        )

    # Player name above score.
    render_text(name, tweak["window_width"] - 200, hud_y, 18, Color(160, 160, 160, 180))

    # Score.
    render_text(f"Points: {score}", tweak["window_width"] - 200, hud_y + 22, 40, Color(200, 200, 200, 255))

    # Deck count near deck stack.
    deck_x = margin + w + margin
    render_text(
        str(deck_count),
        deck_x + w // 2 - 10,
        hud_y + 18 if hud_y < 400 else hud_y - 30,
        18,
        Color(180, 180, 180, 255),
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

        title_w = text_width("GAME OVER", 60)
        render_text("GAME OVER", (w_width - title_w) // 2, 350, 60, Color(255, 255, 255, 255))

        result_w = text_width(result_text, 40)
        render_text(result_text, (w_width - result_w) // 2, 430, 40, Color(255, 215, 0, 255))

        score_text = f"{scores[0]}     |     {scores[1]}"
        score_w = text_width(score_text, 30)
        render_text(score_text, (w_width - score_w) // 2, 490, 30, Color(200, 200, 200, 255))

        end_drawing()
