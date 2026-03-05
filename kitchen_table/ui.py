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


def place_next(rect: Rectangle, width: int, height: int, x: str, y: str, padding: int = 0) -> Rectangle:
    """Return a new Rectangle of (width, height) placed adjacent to rect.
    x: "left" = to the left of rect, "right" = to the right, "center" = horizontally centered with rect.
    y: "top" = above rect, "bottom" = below rect, "center" = vertically centered with rect.
    padding: gap between the two rectangles for non-center alignments.
    """
    if x == "left":
        new_x = int(rect.x) - width - padding
    elif x == "right":
        new_x = int(rect.x) + int(rect.width) + padding
    else:  # center
        assert x == "center"
        new_x = int(rect.x) + int(rect.width) // 2 - width // 2

    if y == "top":
        new_y = int(rect.y) - height - padding
    elif y == "bottom":
        new_y = int(rect.y) + int(rect.height) + padding
    else:  # center
        assert y == "center"
        new_y = int(rect.y) + int(rect.height) // 2 - height // 2

    return Rectangle(new_x, new_y, width, height)


def place_inside(rect: Rectangle, width: int, height: int, x: str, y: str, padding: int = 0) -> Rectangle:
    """Return a new Rectangle of (width, height) placed inside rect.
    x: "left" = flush to the left edge, "right" = flush to the right edge, "center" = horizontally centered.
    y: "top" = flush to the top edge, "bottom" = flush to the bottom edge, "center" = vertically centered.
    padding: gap from the edges for non-center alignments.
    """
    if x == "left":
        new_x = int(rect.x) + padding
    elif x == "right":
        new_x = int(rect.x) + int(rect.width) - width - padding
    else:  # center
        assert x == "center"
        new_x = int(rect.x) + int(rect.width) // 2 - width // 2

    if y == "top":
        new_y = int(rect.y) + padding
    elif y == "bottom":
        new_y = int(rect.y) + int(rect.height) - height - padding
    else:  # center
        assert y == "center"
        new_y = int(rect.y) + int(rect.height) // 2 - height // 2

    return Rectangle(new_x, new_y, width, height)

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
    tr = place_inside(r, tw, 20, x="center", y="center")
    render_text(label, tr.x, tr.y, 20, text_color)
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
    window_size: tuple[int, int] = (tweak["window_width"], tweak["window_height"])
    playground: bool = False

    
    def place(self, width: int, height: int, x: str = "left", y: str = "top", padding: int = 0) -> Rectangle:
        """Return a Rectangle of (width, height) placed inside the window."""
        window = Rectangle(0, 0, self.window_size[0], self.window_size[1])
        return place_inside(window, width, height, x=x, y=y, padding=padding)

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
            br = Rectangle(button.x, button.y, button.width, button.height)
            draw_rectangle_rounded(br, 0.3, 8, color)
            tw = text_width(button.text, 20)
            tr = place_inside(br, tw, 20, x="center", y="center")
            render_text(button.text, tr.x, tr.y, 20, color_from_tuple(tweak["button_text_color"]))
