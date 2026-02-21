from __future__ import annotations
import random
from kitchen_table.models import Card, Stack, Table_State
from kitchen_table.config import tweak


def create_card(id: str, title: str, description: str = "") -> Card:
    return Card(id=id, title=title, description=description)

def add_card_to_stack(card_id: int, stack: Stack, state: Table_State) -> None:
    stack.cards.append(card_id)
    update_card_positions(stack, state)


def remove_card_from_stack(card_id: int, stack: Stack, state: Table_State) -> int | None:
    if card_id in stack.cards:
        stack.cards.remove(card_id)
        update_card_positions(stack, state)
        return card_id
    return None


def update_card_positions(stack: Stack, state: Table_State) -> None:
    """Update x,y positions of all cards in a stack based on spread values."""
    n = len(stack.cards)
    stack.cards.sort(key = lambda card_id: state.cards[card_id].x)
    spread_x = stack.spread_x
    spread_y = stack.spread_y
    card_width = tweak["card_width"]
    if n > 1 and stack.width > 0 and spread_x != 0:
        total_width = (n - 1) * spread_x + card_width
        if total_width > stack.width:
            spread_x = (stack.width - card_width) / (n - 1)
    total_spread_x = (n - 1) * spread_x if n > 1 else 0
    total_spread_y = (n - 1) * spread_y if n > 1 else 0
    mid_x = stack.x + stack.width / 2 if stack.width > 0 else stack.x
    start_x = mid_x - (total_spread_x + card_width) / 2
    start_y = stack.y - total_spread_y / 2
    for i, card_id in enumerate(stack.cards):
        if card_id == state.drag_state.card_id:
            continue
        card = state.cards[card_id]
        card.x = start_x + i * spread_x
        card.y = start_y + i * spread_y


def move_card_to_stack(card_id: int, from_stack: Stack, to_stack: Stack, state: Table_State) -> None:
    remove_card_from_stack(card_id, from_stack, state)
    add_card_to_stack(card_id, to_stack, state)


def find_stack_containing_card(card_id: int, state: Table_State) -> int:
    """Return stack index, or -1 if not found."""
    for i, stack in enumerate(state.stacks):
        if card_id in stack.cards:
            return i
    return -1


def add_loose_card(card_id: int, state: Table_State) -> None:
    """Add a card to the table as a loose card."""
    state.loose_cards.append(card_id)


def remove_loose_card(card_id: int, state: Table_State) -> int | None:
    if card_id in state.loose_cards:
        state.loose_cards.remove(card_id)
        return card_id
    return None


def shuffle_stack(stack: Stack, state: Table_State) -> None:
    """Shuffle the cards in a stack."""
    random.shuffle(stack.cards)
    update_card_positions(stack, state)


def create_sample_cards(state: Table_State) -> list[int]:
    """Create a sample set of cards for testing. Returns list of card indices."""
    card_ids = []
    for i in range(10):
        card = create_card(f"card_{i}", f"Card {i + 1}")
        card_id = len(state.cards)
        state.cards.append(card)
        card_ids.append(card_id)
    return card_ids

