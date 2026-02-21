from __future__ import annotations
from dataclasses import dataclass, field
from pyray import *
from kitchen_table.config import tweak
from kitchen_table.rendering import color_from_tuple
from kitchen_table.models import Table_State


def point_in_rect(px: float, py: float, x: float, y: float, w: float, h: float) -> bool:
    """Check if point (px, py) is inside a rectangle."""
    return x <= px <= x + w and y <= py <= y + h


@dataclass
class Button:
    x: int
    y: int
    width: int
    height: int
    text: str = ""

    def pressed(self, mx, my, click) -> bool:
        """Check if button was clicked."""
        if not click:
            return False
        return point_in_rect(mx, my, self.x, self.y, self.width, self.height)

@dataclass
class UI_State:
    buttons: dict[str, Button] = field(default_factory=dict)
    highlighted_cards: dict[str, int] = field(default_factory=dict)

    def clicked(self, mouse_x: float, mouse_y: float) -> str | None:
        """Return the name of the clicked button, or None."""
        if not is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT):
            return None
        for name, button in self.buttons.items():
            if point_in_rect(mouse_x, mouse_y, button.x, button.y, button.width, button.height):
                return name
        return None

    def draw_buttons(self):
        """Draw all buttons with hover highlighting."""
        mx, my = get_mouse_x(), get_mouse_y()
        for name, button in self.buttons.items():
            hovered = point_in_rect(mx, my, button.x, button.y, button.width, button.height)
            color_key = "button_hover_color" if hovered else "button_color"
            color = color_from_tuple(tweak[color_key])
            draw_rectangle_rounded(
                Rectangle(button.x, button.y, button.width, button.height), 0.3, 8, color
            )
            text_width = measure_text(button.text, 20)
            text_x = button.x + (button.width - text_width) // 2
            text_y = button.y + (button.height - 20) // 2
            draw_text(button.text, text_x, text_y, 20, color_from_tuple(tweak["button_text_color"]))

    def draw_card_highlights(self, table_state: Table_State):
        """Draw highlight borders around cards in the highlighted_cards list."""
        if table_state is None or not table_state.animated_cards:
            return
        w = tweak["card_width"]
        h = tweak["card_height"]
        highlight_color = color_from_tuple(tweak["highlight_color"])
        for i, card_id in self.highlighted_cards.items():
            kt_card = table_state.animated_cards[card_id]
            draw_rectangle_rounded_lines_ex(
                Rectangle(kt_card.x, kt_card.y, w, h), 0.25, 8, 4, highlight_color
            )
