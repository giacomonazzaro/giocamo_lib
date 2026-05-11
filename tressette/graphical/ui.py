from __future__ import annotations

from typing import Callable

import tabletop.models as kt
from tabletop.config import tweak
from tabletop.rendering import (
    color_from_tuple,
    render_text,
    text_width,
)
from tabletop.ui import place_inside, place_next
from pyray import (
    Color,
    Rectangle,
    draw_rectangle_rounded,
    draw_rectangle_rounded_lines_ex,
)

from tressette.game.models import Game_State, Suit

# Italian suit short labels and tints — matches carte napoletane.
SUIT_LETTER = {
    Suit.COPPE: "Coppe",
    Suit.DENARI: "Denari",
    Suit.SPADE: "Spade",
    Suit.BASTONI: "Bastoni",
}

SUIT_COLOR = {
    Suit.COPPE: Color(180, 50, 70, 255),
    Suit.DENARI: Color(210, 170, 30, 255),
    Suit.SPADE: Color(70, 110, 190, 255),
    Suit.BASTONI: Color(80, 150, 80, 255),
}

# Rank labels: 1..7 numeric, then Donna / Cavallo / Re for face cards.
RANK_LABEL = {
    1: "1",
    2: "2",
    3: "3",
    4: "4",
    5: "5",
    6: "6",
    7: "7",
    8: "Donna",
    9: "Cavallo",
    10: "Re",
}


def make_tressette_stacks(both_hands_visible: bool) -> list[kt.Stack]:
    """Layout for the 6 zones on a Tressette table.

    Zones (in order, indices below match what agent_ui uses):
      0: p0_hand     — bottom player hand
      1: p1_hand     — top player hand
      2: p0_tricks   — bottom player's trick pile (face-down stack)
      3: p1_tricks   — top player's trick pile
      4: stock       — face-down draw pile, centered
      5: table       — current trick (0-2 cards face-up in the middle).
    """
    W = tweak["window_width"]
    H = tweak["window_height"]
    w = tweak["card_width"]
    h = tweak["card_height"]
    margin = 30

    spread_hand = int(w)
    spread_pile = -3

    window = Rectangle(0, 0, W, H)
    hand_width = spread_hand * 9 + w  # fits up to 10 cards.

    p0_hand = place_inside(
        window, hand_width, h, x="center", y="bottom", padding=margin
    )
    p1_hand = place_inside(window, hand_width, h, x="center", y="top", padding=margin)

    p0_tricks = place_next(p0_hand, w, h, x="right", y="center", padding=margin)
    p1_tricks = place_next(p1_hand, w, h, x="left", y="center", padding=margin)

    stock = place_inside(window, w, h, x="center", y="center", padding=0)
    stock.x -= int(w * 1.5)

    # Two card-slots side by side for the played trick (leader on the left).
    table = place_inside(window, 2 * w + 30, h, x="center", y="center", padding=0)
    table.x += int(w * 0.4)

    return [
        kt.Stack(
            p0_hand, spread_x=spread_hand, spread_y=0, face_up=True, name="p0_hand"
        ),
        kt.Stack(
            p1_hand,
            spread_x=spread_hand,
            spread_y=0,
            face_up=both_hands_visible,
            name="p1_hand",
        ),
        kt.Stack(
            p0_tricks, spread_x=0, spread_y=spread_pile, face_up=False, name="p0_tricks"
        ),
        kt.Stack(
            p1_tricks, spread_x=0, spread_y=spread_pile, face_up=False, name="p1_tricks"
        ),
        kt.Stack(stock, spread_x=0, spread_y=spread_pile, face_up=False, name="stock"),
        kt.Stack(table, spread_x=int(w + 30), spread_y=0, face_up=True, name="table"),
    ]


# Stack indices in the list returned above.
HAND = (0, 1)
TRICKS = (2, 3)
STOCK_IDX = 4
TABLE_IDX = 5


def make_card_draw_callback(state: Game_State, ui_state) -> Callable[[kt.Card], None]:
    """Render rank + suit on the card face. No card art."""

    def draw(card: kt.Card):
        c = state.all_cards[card.id]
        w = tweak["card_width"]
        h = tweak["card_height"]
        rank_text = RANK_LABEL[c.rank]
        suit_text = SUIT_LETTER[c.suit]
        col = SUIT_COLOR[c.suit]

        # Big rank number/word, centered horizontally near the top.
        rank_size = 56 if len(rank_text) == 1 else 32
        tw = text_width(rank_text, rank_size)
        render_text(rank_text, w // 2 - tw // 2, int(h * 0.18), rank_size, col)

        # Suit name underneath.
        suit_size = 22
        sw = text_width(suit_text, suit_size)
        render_text(suit_text, w // 2 - sw // 2, int(h * 0.55), suit_size, col)

        # Tiny rank in the bottom-right corner so overlapped fanned cards still
        # show their value.
        small_size = 22
        render_text(
            rank_text,
            w - text_width(rank_text, small_size) - 10,
            h - small_size - 10,
            small_size,
            col,
        )

        # Highlight border for legal cards (driven by ui_state.highlighted_cards).
        if card.id in ui_state.highlighted_cards.values():
            draw_rectangle_rounded_lines_ex(
                Rectangle(0, 0, w, h),
                0.18,
                8,
                4,
                color_from_tuple(tweak["highlight_color"]),
            )

    return draw


def draw_player_hud(player_index: int, score: int, is_current: bool, hud_y: int):
    label = f"Player {player_index + 1}: {score}"
    color = Color(200, 200, 200, 255) if is_current else Color(120, 120, 120, 200)
    render_text(label, 30, hud_y, 28, color)


def draw_game_over_overlay(scores: list[int]):
    from pyray import draw_rectangle, get_screen_height, get_screen_width

    W = get_screen_width()
    H = get_screen_height()
    draw_rectangle(0, 0, W, H, color_from_tuple(tweak["modal_overlay"]))
    title = "GAME OVER"
    if scores[0] > scores[1]:
        msg = "Player 1 wins!"
    elif scores[1] > scores[0]:
        msg = "Player 2 wins!"
    else:
        msg = "It's a tie."
    score_line = f"{scores[0]} - {scores[1]}"
    render_text(
        title, W // 2 - text_width(title, 60) // 2, 320, 60, Color(255, 255, 255, 255)
    )
    render_text(
        msg, W // 2 - text_width(msg, 36) // 2, 410, 36, Color(255, 215, 0, 255)
    )
    render_text(
        score_line,
        W // 2 - text_width(score_line, 30) // 2,
        470,
        30,
        Color(200, 200, 200, 255),
    )
