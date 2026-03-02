from game.agents.agent import Agent
from game.game import Choice, Choose_Card, Choose_Cards, Choose_Option, action_options
from gods.models import Card_Type, Game_State, Card_Id
from kitchen_table.input import card_pressed
from kitchen_table.models import Table_State
from kitchen_table.game_state import update_card_positions
from kitchen_table.config import tweak
from pyray import *
import time

from kitchen_table.ui import point_in_rect, Button, UI_State


def update_stacks(table_state: Table_State, gods_state: Game_State, bottom_player: int = 0):
    def update_stack(stack_id: int, card_indices: list[int]):
        # table_state.cards is aligned with game.all_cards, so card.id == kt card id.
        table_state.stacks[stack_id].cards = list(card_indices)
        update_card_positions(table_state.stacks[stack_id], table_state)

    bp = bottom_player
    tp = 1 - bottom_player

    # Bottom player areas (stacks 0-4): deck, hand, discard, peoples, wonders.
    update_stack(0, gods_state.players[bp].deck)
    update_stack(1, gods_state.players[bp].hand)
    update_stack(2, gods_state.players[bp].discard)
    update_stack(3, [pid for pid in gods_state.peoples if gods_state.all_cards[pid].owner == bp])
    update_stack(4, gods_state.players[bp].wonders)

    # Top player areas (stacks 5-9): deck, hand, discard, peoples, wonders.
    update_stack(5, gods_state.players[tp].deck)
    update_stack(6, gods_state.players[tp].hand)
    update_stack(7, gods_state.players[tp].discard)
    update_stack(8, [pid for pid in gods_state.peoples if gods_state.all_cards[pid].owner == tp])
    update_stack(9, gods_state.players[tp].wonders)




class Agent_UI(Agent):
    def __init__(self, table_state: Table_State, ui_state: UI_State, bottom_player: int = 0):
        self.table_state = table_state
        self.ui_state = ui_state
        self.bottom_player = bottom_player
        # Persistent state for multi-step card picking in choose-cards.
        # self._choose_cards_remaining = None
        # self._choose_cards_picked = None
        self.is_ui_ready = False
        self.card_combinations = []
        self.card_multiselection = set()

    def message(self, msg: str):
        pass

    def _handle_choose_cards(self, state: Game_State, actions: list, done_label="Done") -> int:
        """Sequential card-picking UI for combinatorial choices.
        Actions are list[tuple[Card_Id, ...]], each tuple is one valid combination.
        User picks cards one at a time; remaining valid combinations are narrowed down.
        Returns -1 until the user has fully selected a combination."""
        # Initialize persistent state on first call.
        if self._choose_cards_remaining is None:
            self._choose_cards_remaining = list(range(len(actions)))
            self._choose_cards_picked = []

        remaining = self._choose_cards_remaining
        picked_cards = self._choose_cards_picked

        # Narrowed down to one combination — done.
        if len(remaining) == 1:
            selected = remaining[0]
            self._choose_cards_remaining = None
            self._choose_cards_picked = None
            self.ui_state.highlighted_cards = []
            self.ui_state.buttons = []
            return selected

        # Find selectable card_ids across remaining combinations, excluding already picked.
        selectable = []
        for idx in remaining:
            for card_id in actions[idx]:
                if card_id not in picked_cards and card_id not in selectable:
                    selectable.append(card_id)

        # Check if a combination matching exactly the picked cards exists.
        has_done = any(
            len(actions[idx]) == len(picked_cards) and all(c in picked_cards for c in actions[idx])
            for idx in remaining
        )

        # Set up UI.
        button_w = 140
        button_h = 45
        button_x = (get_screen_width() - button_w) // 2
        button_y = get_screen_height() - 50

        self.ui_state.highlighted_cards = list(selectable)
        self.ui_state.buttons = []
        if has_done:
            self.ui_state.buttons.append(Button(button_x, button_y, button_w, button_h, text=done_label))

        # Poll for input.
        mx, my = get_mouse_x(), get_mouse_y()
        click = is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT)

        # Check "Done" button.
        if has_done and self.ui_state.buttons[0].pressed():
            for idx in remaining:
                if len(actions[idx]) == len(picked_cards) and all(c in picked_cards for c in actions[idx]):
                    selected = idx
                    self._choose_cards_remaining = None
                    self._choose_cards_picked = None
                    self.ui_state.highlighted_cards = []
                    self.ui_state.buttons = []
                    return selected

        # Check card clicks.
        for card_id in selectable:
            card = state.get_card(card_id)
            kt_card = self.table_state.cards[card.id]
            w = tweak["card_width"]
            h = tweak["card_height"]
            if click and point_in_rect(mx, my, kt_card.x, kt_card.y, w, h):
                picked_cards.append(card_id)
                self._choose_cards_remaining = [idx for idx in remaining if card_id in actions[idx]]
                break



        return -1

    def build_ui(self, state: Game_State, choice: Choice, action_type):
        action_type_obj = action_type
        options = action_options(action_type_obj)
        count = len(options)
        button_w = 140
        button_h = 45
        gap = 20
        total_width = count * button_w + (count - 1) * gap
        start_x = (get_screen_width() - total_width) // 2
        button_y = get_screen_height() - 50

        self.ui_state.highlighted_cards = {}
        self.ui_state.buttons = {}
        self.card_combinations = []
        self.card_multiselection = set()

        # Fixed centered position for standalone buttons (Pass, Done).
        button_x = (get_screen_width() - button_w) // 2

        if isinstance(action_type_obj, Choose_Option):
            for i, label in enumerate(options):
                x = start_x + i * (button_w + gap)
                button = Button(x, button_y, button_w, button_h, text=label)
                self.ui_state.buttons[i] = button
        elif isinstance(action_type_obj, Choose_Card):
            done_label = "Pass" if choice.description == "main" else "Done"
            for i, card_id in enumerate(options):
                if Card_Id.is_null(card_id):
                    button = Button(button_x, button_y, button_w, button_h, text=done_label)
                    self.ui_state.buttons[i] = button
                else:
                    self.ui_state.highlighted_cards[i] = state.get_card(card_id).id
        elif isinstance(action_type_obj, Choose_Cards):
            self.card_combinations = []
            for combination in options:
                self.card_combinations.append(set(combination))
                if len(combination) == 0:
                    button = Button(button_x, button_y, button_w, button_h, text="Done")
                    self.ui_state.buttons[0] = button
                for card_id in combination:
                    self.ui_state.highlighted_cards[card_id] = state.get_card(card_id).id

        self.is_ui_ready = True

    def clear_ui(self):
        self.is_ui_ready = False
        self.ui_state.buttons = {}
        self.ui_state.highlighted_cards = {}
        self.table_state.is_drop_card_allowed = lambda sa,sb,c: False

    def choose_action(self, state: Game_State, choice: Choice) -> int:
        action_type = choice.actions(state)
        options = action_options(action_type)

        play_stack = 4 if choice.player_index == self.bottom_player else 9
        hand_stack = 1 if choice.player_index == self.bottom_player else 6
        if not self.is_ui_ready:
            self.build_ui(state, choice, action_type)
            def is_drop_card_allowed(source_stack: int, target_stack: int, card_id: int) -> bool:
                if source_stack == target_stack:
                    return True
                return source_stack == hand_stack and target_stack == play_stack
                
            self.table_state.is_drop_card_allowed = is_drop_card_allowed


        dropped_card = self.table_state.poll_dropped_card()
        if dropped_card:
            original_stack, target_stack, dropped_card_id = dropped_card

            if choice.description == "main" and original_stack == hand_stack and target_stack == play_stack:
                # options is sorted by stable card_index (= Card.id), so find by matching card_index.
                action_index = next(i for i, cid in enumerate(options) if not Card_Id.is_null(cid) and cid.card_index == dropped_card_id)
                self.clear_ui()
                return action_index


        # return -1
        selected = -1
        if not is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT):
            return -1
        

        if isinstance(action_type, Choose_Cards):
            for gods_card_id, card_index in self.ui_state.highlighted_cards.items():
                card = self.table_state.cards[card_index]
                if card_pressed(card):
                    self.card_multiselection.add(gods_card_id)
                    del self.ui_state.highlighted_cards[gods_card_id]
                    break
            
            if self.card_multiselection in self.card_combinations:
                m = max([len(c) for c in self.card_combinations])
                if m == len(self.card_multiselection):
                    selected = self.card_combinations.index(self.card_multiselection)
                    self.clear_ui()

                if len(self.ui_state.buttons) and self.ui_state.buttons[0].pressed():
                    i = self.card_combinations.index(self.card_multiselection)
                    selected = i
                    self.clear_ui()

        for i, button in self.ui_state.buttons.items():
            if button.pressed():
                selected = i
        
        if choice.description != "main":
            for i, card_id in self.ui_state.highlighted_cards.items():
                card = self.table_state.cards[card_id]
                if card_pressed(card):
                    selected = i

        print("Selected", selected)
        if selected != -1:
            self.is_ui_ready = False
            self.ui_state.buttons = {}
            self.ui_state.highlighted_cards = {}
        return selected
