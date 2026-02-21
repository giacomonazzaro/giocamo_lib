from pyray import *
from kitchen_table.config import tweak
from kitchen_table.game_state import add_card_to_stack, create_sample_cards, update_card_positions
from kitchen_table.models import Table_State, Stack
from kitchen_table.rendering import draw_table, draw_background
from kitchen_table.input import update_input
from kitchen_table.ui import UI_State, Button
import random

def create_example_table_state() -> Table_State:
    """Initialize a new table state with some stacks."""
    state = Table_State()

    # Create a few stacks at different positions
    stack1 = Stack(100, 300, width = 300, spread_y=tweak["pile_spread_y"], spread_x=0, face_up=False)
    stack2 = Stack(400, 550, width = 500, spread_x=tweak["hand_spread_x"])
    stack3 = Stack(1000, 300, width = 600, spread_y=tweak["pile_spread_y"])

    state.stacks = [stack1, stack2, stack3]

    # Add sample cards to first stack
    card_ids = create_sample_cards(state)
    random.shuffle(card_ids)
    for card_id in card_ids:
        add_card_to_stack(card_id, stack1, state)

    return state

def main():
    # Initialize window
    init_window(
        tweak["window_width"],
        tweak["window_height"],
        tweak["window_title"]
    )
    set_target_fps(tweak["target_fps"])

    # Create initial table state
    state = create_example_table_state()
    ui_state = UI_State()
    ui_state.buttons[0] = Button(100, 600, 200, 100, "Draw")

    # Main loop
    while not window_should_close():
        # Update
        update_input(state)

        # Draw card by pressing button.
        if ui_state.buttons[0].pressed():
            state.stacks[1].cards.append(state.stacks[0].cards.pop())
            update_card_positions(state.stacks[0], state)
            update_card_positions(state.stacks[1], state)

        # Render.
        begin_drawing()
        draw_background()
        draw_table(state)
        ui_state.draw_buttons()
        end_drawing()

    # Cleanup
    close_window()


if __name__ == "__main__":
    main()
