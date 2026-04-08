from __future__ import annotations
import os

from pyray import *

from kitchen_table.config import tweak
from kitchen_table.rendering import draw_table, draw_background, color_from_tuple, render_text, text_width
from kitchen_table.ui import place_inside, place_next
import kitchen_table.models as kt

IMAGES_DIR = os.path.join(os.path.dirname(__file__), "..", "gods", "cards", "fronts")

def make_gods_stacks(bottom_player: int = 0) -> list[kt.Stack]:
    """Return stack layout definitions for the card table.

    Returns a list of Stack objects (cards/name/depth left at defaults).
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
    p0_hand    = place_inside(window, hand_width, h, x="center", y="bottom", padding=margin)
    p0_hand.x += 100
    p0_wonders = place_next(p0_hand, hand_width, h, x="center", y="top", padding=margin)
    p0_deck    = place_next(p0_hand, w, h, x="left", y="center", padding=margin)
    p0_discard = place_next(p0_deck, w, h, x="left", y="center", padding=margin)

    # Top player: mirror bottom y positions, pushed partially offscreen.
    opponent_shift = int(h * 0.65)
    top_y = margin - opponent_shift
    top_wonders_y = H - int(p0_wonders.y) - h - opponent_shift

    # Shared deck: vertically centered, off-screen to the left.
    shared_deck = place_next(window, w, h, x="right", y="center", padding=10)
    # shared_deck.x = -w
    
    # Peoples
    p0_peoples = place_next(p0_wonders, peoples_width, h, x="left", y="center", padding=margin)

    # Pre-build top-player rects by reusing bottom positions with mirrored y.
    p1_deck    = Rectangle(p0_deck.x,    top_y,         w,             h)
    p1_hand    = Rectangle(p0_hand.x,    top_y,         hand_width,    h)
    p1_discard = Rectangle(p0_discard.x,        top_y,         w,             h)
    p1_peoples = Rectangle(p0_peoples.x,     top_wonders_y, peoples_width, h)
    p1_wonders = Rectangle(p0_wonders.x, top_wonders_y, hand_width,    h)

    # If the bottom player is player 1, swap all stack positions. The stack indices will remain
    # the same and this is important for online game, where the state must be the same but it should
    # just be rendered flipped for player 1.
    if bottom_player == 1:
        p0_deck, p1_deck = p1_deck, p0_deck
        p0_hand, p1_hand = p1_hand, p0_hand
        p0_discard, p1_discard = p1_discard, p0_discard
        p0_peoples, p1_peoples = p1_peoples, p0_peoples
        p0_wonders, p1_wonders = p1_wonders, p0_wonders

    visible = bottom_player == 0
    result = [
        kt.Stack(p0_deck, spread_x=0, spread_y=spread_pile, face_up=False, name=f"p0_deck"),
        kt.Stack(p0_hand, spread_x=spread_hand, spread_y=0, face_up=visible, name=f"p0_hand"),
        kt.Stack(p0_discard, spread_x=0, spread_y=spread_pile, face_up=True, name=f"p0_discard" ),
        kt.Stack(p0_peoples, spread_x=spread_wonders, spread_y=0, face_up=True, name=f"p0_peoples" ),
        kt.Stack(p0_wonders, spread_x=spread_wonders, spread_y=0, face_up=True, name=f"p0_wonders" ),
        kt.Stack(p1_deck, spread_x=0, spread_y=spread_pile, face_up=False, name=f"p1_deck"    ),
        kt.Stack(p1_hand, spread_x=spread_hand, spread_y=0, face_up=visible, name=f"p1_hand"    ),
        kt.Stack(p1_discard, spread_x=0, spread_y=spread_pile, face_up=True, name=f"p1_discard" ),
        kt.Stack(p1_peoples, spread_x=spread_wonders, spread_y=0, face_up=True, name=f"p1_peoples" ),
        kt.Stack(p1_wonders, spread_x=spread_wonders, spread_y=0, face_up=True, name=f"p1_wonders" ),
        kt.Stack(shared_deck, spread_x=0, spread_y=spread_pile, face_up=False, name="shared_deck"   ),
    ]
    return result

def get_image_path(card_name: str) -> str | None:
    if len(card_name) == 1: card_name = "0" + card_name
    filename = card_name.lower().replace(" ", "_") + ".png"
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
    draw_circle(badge_cx, badge_cy, badge_r, Color(0, 0, 0, 255))

    # Center the power number on the badge circle.
    size = int(0.2 * w)
    tw = text_width(power, size)
    render_text(power, badge_cx - tw // 2, badge_cy - size // 2, size, Color(255, 255, 255, 255))

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

def draw_player_hud(player_id: int, score: int, deck_count: int, is_current: bool, hud_y: int):
    w = tweak["card_width"]
    margin = 20

    # Current player indicator bar.
    if is_current:
        draw_rectangle_rounded(
            Rectangle(tweak["window_width"] - 10, hud_y + 28, 6, 50), 0.5, 4, (255, 255, 255, 255)
        )

    # Player name above score.
    # render_text(name, tweak["window_width"] - 200, hud_y, 18, Color(160, 160, 160, 180))

    # Score.
    render_text(f"Points: {score}", tweak["window_width"] - 200, hud_y + 22, 40, Color(200, 200, 200, 255))

    # Deck count near deck stack.
    # rect = place_next()
    # render_text(
    #     str(deck_count),
    #     deck_x + w // 2 - 10,
    #     hud_y + 18 if hud_y < 400 else hud_y - 30,
    #     18,
    #     Color(180, 180, 180, 255),
    # )



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
