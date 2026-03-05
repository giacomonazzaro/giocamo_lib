from __future__ import annotations

from game.agents.agent import Agent
from game.game import Choice, Choose_Card, Choose_Cards, Choose_Option, action_options
from gods.models import Card_Type, Game_State, Card_Id
from kitchen_table.input import card_pressed
from kitchen_table.models import Table_State
from kitchen_table.game_state import update_card_positions
from kitchen_table.config import tweak
from pyray import *

from kitchen_table.ui import immediate_button, UI_State

def stack_indices(player_index: int):
    """Return the global stack indices for a given player's zones.

    Usage:
        s = stack_indices(0)
        s.hand   # → 1
        s.wonders # → 4

        s = stack_indices(1)
        s.hand   # → 6
        s.wonders # → 9
    """
    from types import SimpleNamespace
    base = player_index * 5
    return SimpleNamespace(deck=base, hand=base+1, discard=base+2, peoples=base+3, wonders=base+4)


def update_stacks(table_state: Table_State, gods_state: Game_State):
    def update_stack(stack_id: int, card_indices: list[int]):
        # table_state.cards is aligned with game.all_cards, so card.id == kt card id.
        table_state.stacks[stack_id].cards = list(card_indices)
        update_card_positions(table_state.stacks[stack_id], table_state, sort=False)

    for i in range(2):
        s = stack_indices(i)
        update_stack(s.deck,    gods_state.players[i].deck)
        update_stack(s.hand,    gods_state.players[i].hand)
        update_stack(s.discard, gods_state.players[i].discard)
        update_stack(s.peoples, [pid for pid in gods_state.peoples if gods_state.all_cards[pid].owner == i])
        update_stack(s.wonders, gods_state.players[i].wonders)

class Agent_UI(Agent):
    def __init__(self, table_state: Table_State, ui_state: UI_State, bottom_player: int = 0):
        self.table_state = table_state
        self.ui_state = ui_state
        self.bottom_player = bottom_player
        self.card_multiselection = set()

    def message(self, msg: str):
        pass

    def choose_action(self, state: Game_State, choice: Choice) -> int:
        """
        Handle user input to select an action during gameplay.
        
        Processes player input through drag-and-drop card mechanics and button clicks
        to determine which action to execute from the available options.
        
        Args:
            state (Game_State): The current game state containing card and player information.
            choice (Choice): The choice object defining available actions and their type.
        
        Returns:
            int: The index of the selected action within the options list, or -1 if no action
                 was selected in this frame.
        
        Behavior:
            - For single-option choices (non-main), automatically returns 0.
            - Handles drag-and-drop from hand to play area for card selection.
            - For Choose_Option: Displays buttons for each option label.
            - For Choose_Card: Displays selectable cards and a Pass/Done button.
            - For Choose_Cards: Allows multi-selection of cards with auto-confirmation
              when a maximal valid combination is reached, or shows Done button for
              non-maximal selections.
            - Highlights available cards in the UI based on current selection state.
            - Clears highlights when transitioning to non-Agent_UI players.
        """
        action_type = choice.actions(state)
        options = action_options(action_type)
        if len(options) == 1 and choice.description != "main":
            return 0

        # Stacks are globally ordered: player i's hand is i*5+1, wonders is i*5+4.
        hand_stack = choice.player_index * 5 + 1
        play_stack = choice.player_index * 5 + 4

        # Set drag-and-drop permission (safe to call every frame).
        def is_drop_card_allowed(source_stack: int, target_stack: int, _card_id: int) -> bool:
            if source_stack == target_stack:
                return True
            return source_stack == hand_stack and target_stack == play_stack
        self.table_state.is_drop_card_allowed = is_drop_card_allowed

        # Clear highlights - repopulated below for this frame.
        self.ui_state.highlighted_cards = {}

        # Handle dropped card (drag-and-drop to play from hand).
        dropped_card = self.table_state.poll_dropped_card()
        if dropped_card:
            original_stack, target_stack, dropped_card_id = dropped_card
            if choice.description == "main" and original_stack == hand_stack and target_stack == play_stack:
                action_index = next(i for i, cid in enumerate(options) if not Card_Id.is_null(cid) and cid.card_index == dropped_card_id)
                return action_index

        # Button layout: anchor just above the bottom player's name/score block.
        count = len(options) - sum(1 for cid in options if isinstance(cid, Card_Id) and not Card_Id.is_null(cid))
        gap = 20
        button_height = 40
        button_width = 140
        all_buttons_width = count * button_width + (count) * gap
        start_x = (get_screen_width() - all_buttons_width)
        all_buttons = self.ui_state.place(all_buttons_width, button_height, x="right", y="center", padding=gap)
        button = Rectangle(all_buttons.x, all_buttons.y, button_width, button_height)

        mouse_clicked = is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT)

        if isinstance(action_type, Choose_Option):
            for i, label in enumerate(options):
                if immediate_button(button, label):
                    return i
                button.x += (button.width + gap)

        elif isinstance(action_type, Choose_Card):
            done_label = "Pass" if choice.description == "main" else "Done"
            for i, card_id in enumerate(options):
                if Card_Id.is_null(card_id):
                    if immediate_button(button, done_label):
                        self.ui_state.highlighted_cards = {}
                        return i
                    button.x += (button.width + gap)
                else:
                    kt_card_id = state.get_card(card_id).id
                    self.ui_state.highlighted_cards[i] = kt_card_id
                    if mouse_clicked and choice.description != "main":
                        card = self.table_state.cards[kt_card_id]
                        if card_pressed(card):
                            # Clear state if next player is not Agent_UI, to avoid stale highlights.
                            self.ui_state.highlighted_cards = {}
                            return i

        elif isinstance(action_type, Choose_Cards):
            card_combinations = [set(combination) for combination in options]
            # Collect all unique card ids across all combinations.
            all_card_ids = {card_id for combination in options for card_id in combination}
            for card_id in all_card_ids:
                kt_card_id = state.get_card(card_id).id
                if card_id not in self.card_multiselection:
                    self.ui_state.highlighted_cards[card_id] = kt_card_id
                    if mouse_clicked:
                        card = self.table_state.cards[kt_card_id]
                        if card_pressed(card):
                            self.card_multiselection.add(card_id)
                            mouse_clicked = False  # Consume the click.

            if self.card_multiselection in card_combinations:
                m = max(len(c) for c in card_combinations)
                if m == len(self.card_multiselection):
                    # Multiselection is maximal - auto-confirm.
                    i = card_combinations.index(self.card_multiselection)
                    self.card_multiselection = set()
                    # Clear state if next player is not Agent_UI, to avoid stale highlights.
                    self.ui_state.highlighted_cards = {}
                    return i
                # Show "Done" button to confirm a non-maximal valid selection.
                # button.x = self.ui_state.place(button.width, button.height, x="right", y="center").x
                if immediate_button(button, "Done"):
                    i = card_combinations.index(self.card_multiselection)
                    self.card_multiselection = set()
                    self.ui_state.highlighted_cards = {}
                    return i

        return -1
