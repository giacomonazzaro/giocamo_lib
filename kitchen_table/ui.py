from __future__ import annotations
import copy
from dataclasses import dataclass, field
from pyray import *
from kitchen_table.config import tweak
from kitchen_table.rendering import color_from_tuple, render_text, text_width
from kitchen_table.models import Table_State


def point_in_rect(px: float, py: float, x: float, y: float, w: float, h: float) -> bool:
    """Check if point (px, py) is inside a rectangle."""
    return x <= px <= x + w and y <= py <= y + h


@dataclass(slots=True)
class Button:
    x: int
    y: int
    width: int
    height: int
    text: str = ""

    def pressed(self) -> bool:
        """Check if button was clicked."""
        if not is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT):
            return False
        mx, my = get_mouse_x(), get_mouse_y()
        return point_in_rect(mx, my, self.x, self.y, self.width, self.height)

def immediate_button(rectangle: Rectangle, label: str, color: Color=None, text_color: Color=None) -> bool:
    """Create a rectangle and return whether it was clicked."""
    # Fix size to fit label if needed.
    tw = text_width(label, 20)
    r = rectangle
    r.width = max(r.width, tw + 20)
    
    
    mx, my = get_mouse_x(), get_mouse_y()
    hovered = point_in_rect(mx, my, r.x, r.y, r.width, r.height)
    if color is None:
        color = color_from_tuple(tweak["button_color"])
    if hovered:
        color = color_from_tuple(tweak["button_hover_color"])

    if text_color is None:
        text_color = color_from_tuple(tweak["button_text_color"])
    draw_rectangle_rounded(r, 0.3, 8, color)
    text_x = int(r.x) + (r.width - tw) // 2
    text_y = int(r.y) + (r.height - 20) // 2
    render_text(label, text_x, text_y, 20, text_color)
    if not is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT):
        return False
    return hovered

def immediate_buttons(size: tuple[int, int], buttons: list[tuple[tuple[int, int], str]], color: Color=None, text_color: Color=None) -> int | None:
    for i, (pos, label) in enumerate(buttons):
        rect = Rectangle(pos[0], pos[1], size[0], size[1])
        if immediate_button(rect, label, color, text_color):
            return i
    return None

@dataclass(slots=True)
class UI_State:
    buttons: dict[str, Button] = field(default_factory=dict)
    highlighted_cards: dict[str, int] = field(default_factory=dict)
    current_choice_text: str = ""

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
            tw = text_width(button.text, 20)
            text_x = button.x + (button.width - tw) // 2
            text_y = button.y + (button.height - 20) // 2
            render_text(button.text, text_x, text_y, 20, color_from_tuple(tweak["button_text_color"]))
