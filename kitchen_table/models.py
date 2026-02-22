from __future__ import annotations
from dataclasses import dataclass, field
from typing import Callable


@dataclass(slots=True)
class Card:
    id: int
    title: str
    description: str = ""
    image_path: str | None = None
    x: float = 0.0
    y: float = 0.0
    rotation: int = 0  # Rotation angle in degrees (0, 90, 180, 270)
    draw_callback: callable | None = None  # Optional custom draw function

@dataclass(slots=True)
class Stack:
    x: float
    y: float
    width: float
    cards: list[int] = field(default_factory=list)
    spread_x: float = 0.0  # Horizontal offset between cards
    spread_y: float = 0.0  # Vertical offset between cards
    face_up: bool = True   # Whether cards are visible
    name: str = ""


@dataclass(slots=True)
class Drag_State:
    card_id: int = -1  # -1 means no card being dragged
    current_stack: int = -1
    last_hovered_stack: int = -1
    original_stack: int = -1
    offset_x: float = 0.0
    offset_y: float = 0.0


@dataclass(slots=True)
class Table_State:
    cards: list[Card] = field(default_factory=list)
    stacks: list[Stack] = field(default_factory=list)
    loose_cards: list[int] = field(default_factory=list)  # Card indices not in any stack
    drag_state: Drag_State = field(default_factory=Drag_State)

    animated_cards: list[Card] = None
    draw_callback: callable | None = None
    zoomed_card_id: int = -1

    is_drop_card_allowed: Callable[[Table_State, int, int], bool] = lambda a, b, c: True
    drop_card: Callable[[Table_State, int, int], None] = lambda a,b,c: None