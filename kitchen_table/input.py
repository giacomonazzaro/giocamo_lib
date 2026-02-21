from __future__ import annotations
from pyray import *
from kitchen_table.models import Card, Stack, Table_State
from kitchen_table.config import tweak
import kitchen_table.game_state as gs


def point_in_card(px: float, py: float, card: Card) -> bool:
    """Check if point (px, py) is inside the card bounds."""
    w = tweak["card_width"]
    h = tweak["card_height"]
    return (card.x <= px <= card.x + w and card.y <= py <= card.y + h)

def card_pressed(card: Card) -> bool:
    if not is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT):
        return False
    mx, my = get_mouse_x(), get_mouse_y()
    return point_in_card(mx, my, card)


def point_in_stack_area(px: float, py: float, stack: Stack, state: Table_State) -> bool:
    """Check if point is in the stack's general area (for drop targets)."""
    # w = tweak["card_width"]
    h = tweak["card_height"]

    def point_in_rect(mx: float, my: float, x: float, y: float, w: float, h: float) -> bool:
        return x <= mx <= x + w and y <= my <= y + h

    return point_in_rect(px, py, stack.x, stack.y, stack.width, h)
    # if stack.cards:
    #     # Calculate bounds including all cards in stack
    #     last_card_id = stack.cards[-1]
    #     last_card = state.cards[last_card_id]
    #     max_x = last_card.x + w
    #     max_y = last_card.y + h
    #     return (stack.x <= px <= max_x and stack.y <= py <= max_y)
    # else:
        # Empty stack - just check base position
    # return (stack.x <= px <= stack.x + w and stack.y <= py <= stack.y + h)


def find_card_at(px: float, py: float, state: Table_State) -> tuple[int, int] | None:
    """Find the topmost card at position. Returns (card_id, stack_index) or None.
    For loose cards, stack_index is -1."""
    # Check loose cards first (on top of everything)
    for card_id in reversed(state.loose_cards):
        card = state.cards[card_id]
        if point_in_card(px, py, card):
            return (card_id, -1)
    # Check stacks (reverse order so we check top cards first)
    for stack_idx in range(len(state.stacks) - 1, -1, -1):
        stack = state.stacks[stack_idx]
        for card_id in reversed(stack.cards):
            card = state.cards[card_id]
            if point_in_card(px, py, card):
                return (card_id, stack_idx)
    return None


def find_stack_at(px: float, py: float, state: Table_State) -> int:
    """Find a stack at the given position. Returns stack index or -1."""
    for i, stack in enumerate(state.stacks):
        if point_in_stack_area(px, py, stack, state):
            return i
    return -1


def handle_mouse_press(state: Table_State) -> None:
    """Handle mouse button press - start drag."""
    mx = get_mouse_x()
    my = get_mouse_y()
    drag = state.drag_state
    
    # Check if clicking a card
    result = find_card_at(mx, my, state)
    if result:
        card_id, stack_idx = result
        card = state.cards[card_id]
        drag.card_id = card_id
        drag.source_stack = stack_idx
        drag.offset_x = mx - card.x
        drag.offset_y = my - card.y
        # # Remove from source
        # if stack_idx >= 0:
        #     gs.remove_card_from_stack(card_id, state.stacks[stack_idx], state)
        # else:
        #     gs.remove_loose_card(card_id, state)


def handle_mouse_release(state: Table_State) -> None:
    """Handle mouse button release - drop card."""
    drag = state.drag_state
    if drag.card_id < 0:
        return

    # Clear drag state
    stack_id = drag.source_stack
    drag.card_id = -1
    drag.source_stack = -1
    drag.offset_x = 0
    drag.offset_y = 0
    gs.update_card_positions(state.stacks[stack_id], state)



def handle_mouse_move(state: Table_State) -> None:
    """Update dragged card position."""
    drag = state.drag_state

    if drag.card_id < 0:
        return
    
    mx = get_mouse_x()
    my = get_mouse_y()
    card = state.cards[drag.card_id]
    card.x = mx - drag.offset_x
    card.y = my - drag.offset_y

    target_stack_idx = find_stack_at(mx, my, state)
    if target_stack_idx >= 0 and drag.source_stack != target_stack_idx:
        state.stacks[drag.source_stack].cards.remove(drag.card_id)
        gs.update_card_positions(state.stacks[drag.source_stack], state)
        state.stacks[target_stack_idx].cards.append(drag.card_id)
        drag.source_stack = target_stack_idx

    if target_stack_idx >= 0:
        gs.update_card_positions(state.stacks[target_stack_idx], state)


def handle_rotate_card(state: Table_State, clockwise: bool = True) -> None:
    """Rotate the card under the cursor by 90 degrees."""
    mx = get_mouse_x()
    my = get_mouse_y()

    result = find_card_at(mx, my, state)
    if result:
        card_id, _ = result
        card = state.cards[card_id]
        if clockwise:
            card.rotation = (card.rotation + 90)
        else:
            card.rotation = (card.rotation - 90)


def update_input(state: Table_State) -> None:
    """Main input processing - call each frame."""
    # Handle mouse
    if is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT):
        handle_mouse_press(state)
    elif is_mouse_button_released(MouseButton.MOUSE_BUTTON_LEFT):
        handle_mouse_release(state)

    # Update drag position continuously
    handle_mouse_move(state)

    # Handle card rotation
    if is_key_pressed(KeyboardKey.KEY_R):
        if is_key_down(KeyboardKey.KEY_LEFT_SHIFT) or is_key_down(KeyboardKey.KEY_RIGHT_SHIFT):
            handle_rotate_card(state, clockwise=False)
        else:
            handle_rotate_card(state, clockwise=True)

    # Handle card zoom
    if is_key_down(KeyboardKey.KEY_SPACE):
        mx = get_mouse_x()
        my = get_mouse_y()
        result = find_card_at(mx, my, state)
        if result:
            state.zoomed_card_id = result[0]
        else:
            state.zoomed_card_id = -1
    else:
        state.zoomed_card_id = -1
