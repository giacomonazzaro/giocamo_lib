from __future__ import annotations
import os

from pyray import *

from kitchen_table.config import tweak
from kitchen_table.rendering import draw_table, draw_background, color_from_tuple, render_text, text_width
from kitchen_table.ui import place_inside, place_next
import kitchen_table.models as kt

IMAGES_DIR = os.path.join(os.path.dirname(__file__), "..", "gods", "cards", "card-images")


def get_table_layout(bottom_player: int = 0) -> dict[str, kt.Stack]:
    """Return stack layout definitions for the card table.

    Returns a dict mapping zone name to Stack (cards/name/depth left at defaults).
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

    window = Rectangle(0, 0, W, H)
    hand_width = int(w * 5.5 * W / 1600)
    peoples_width = 2 * w + spread_wonders

    # Bottom player: anchor zones to the bottom of the window, then chain leftward.
    bottom_hand    = place_inside(window, hand_width, h, x="center", y="bottom", padding=margin)
    bottom_wonders = place_next(bottom_hand, hand_width, h, x="center", y="top", padding=margin)
    bottom_deck    = place_next(bottom_hand, w, h, x="left", y="center", padding=margin)
    discard        = place_next(bottom_deck, w, h, x="left", y="center", padding=margin)

    # Top player: mirror bottom y positions, pushed partially offscreen.
    opponent_shift = int(h * 0.65)
    top_y = margin - opponent_shift
    top_wonders_y = H - int(bottom_wonders.y) - h - opponent_shift

    # Shared deck: vertically centered, off-screen to the left.
    shared_deck = place_inside(window, w, h, x="left", y="center")
    shared_deck.x = -w
    
    # Peoples
    bp_peoples = Rectangle(discard.x,        bottom_wonders.y, peoples_width, h)

    # Pre-build top-player rects by reusing bottom positions with mirrored y.
    tp_deck    = Rectangle(bottom_deck.x,    top_y,         w,             h)
    tp_hand    = Rectangle(bottom_hand.x,    top_y,         hand_width,    h)
    tp_discard = Rectangle(discard.x,        top_y,         w,             h)
    tp_peoples = Rectangle(bp_peoples.x,     top_wonders_y, peoples_width, h)
    tp_wonders = Rectangle(bottom_wonders.x, top_wonders_y, hand_width,    h)

    bp = f"p{bottom_player}"
    tp = f"p{1 - bottom_player}"

    S = kt.Stack
    return {
        f"{bp}_deck":    S(bottom_deck,  spread_x=0,           spread_y=spread_pile, face_up=False),
        f"{bp}_hand":    S(bottom_hand,  spread_x=spread_hand, spread_y=0,           face_up=True),
        f"{bp}_discard": S(discard,      spread_x=0,           spread_y=spread_pile, face_up=True),
        f"{bp}_peoples": S(bp_peoples,   spread_x=spread_wonders, spread_y=0,        face_up=True),
        f"{bp}_wonders": S(bottom_wonders, spread_x=spread_wonders, spread_y=0,      face_up=True),
        f"{tp}_deck":    S(tp_deck,      spread_x=0,           spread_y=spread_pile, face_up=False),
        f"{tp}_hand":    S(tp_hand,      spread_x=spread_hand, spread_y=0,           face_up=False),
        f"{tp}_discard": S(tp_discard,   spread_x=0,           spread_y=spread_pile, face_up=True),
        f"{tp}_peoples": S(tp_peoples,   spread_x=spread_wonders, spread_y=0,        face_up=True),
        f"{tp}_wonders": S(tp_wonders,   spread_x=spread_wonders, spread_y=0,        face_up=True),
        "shared_deck":   S(shared_deck,  spread_x=0,           spread_y=0,           face_up=True),
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

        screen = Rectangle(0, 0, w_width, w_height)
        score_text = f"{scores[0]}     |     {scores[1]}"
        render_text("GAME OVER", place_inside(screen, text_width("GAME OVER", 60), 60, x="center", y="top").x, 350, 60, Color(255, 255, 255, 255))
        render_text(result_text,  place_inside(screen, text_width(result_text,  40), 40, x="center", y="top").x, 430, 40, Color(255, 215, 0, 255))
        render_text(score_text,   place_inside(screen, text_width(score_text,   30), 30, x="center", y="top").x, 490, 30, Color(200, 200, 200, 255))

        end_drawing()
