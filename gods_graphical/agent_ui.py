from game.agents.agent import Agent
from game.game import Choice
from gods.models import Game_State, Card_Id
from kitchen_table.models import Table_State
from kitchen_table.game_state import update_card_positions
from kitchen_table.config import tweak
from pyray import *
import time

from gods_graphical.ui import point_in_rect, Button, UI_State


def update_stacks(table_state: Table_State, gods_state: Game_State, bottom_player: int = 0):
    def update_stack(stack_id: int, card_list):
        table_state.stacks[stack_id].cards = [card.id for card in card_list]
        update_card_positions(table_state.stacks[stack_id], table_state)

    bp = bottom_player
    tp = 1 - bottom_player

    # Bottom player areas (stacks 0-3)
    update_stack(0, gods_state.players[bp].deck)
    update_stack(1, gods_state.players[bp].hand)
    update_stack(2, gods_state.players[bp].discard)
    update_stack(3, gods_state.players[bp].wonders)

    # Top player areas (stacks 4-7)
    update_stack(4, gods_state.players[tp].deck)
    update_stack(5, gods_state.players[tp].hand)
    update_stack(6, gods_state.players[tp].discard)
    update_stack(7, gods_state.players[tp].wonders)

    # People cards (center)
    update_stack(8, gods_state.peoples)




class Agent_UI(Agent):
    def __init__(self, table_state: Table_State, ui_state: UI_State, bottom_player: int = 0):
        self.table_state = table_state
        self.ui_state = ui_state
        self.bottom_player = bottom_player

    def message(self, msg: str):
        pass

    def _handle_choose_cards(self, state: Game_State, actions: list, done_label="Done") -> int:
        """Sequential card-picking UI for combinatorial choices.
        Actions are list[tuple[Card_Id, ...]], each tuple is one valid combination.
        User picks cards one at a time; remaining valid combinations are narrowed down."""
        button_w = 140
        button_h = 45
        button_x = (get_screen_width() - button_w) // 2
        button_y = get_screen_height() - 50

        remaining = list(range(len(actions)))
        picked_cards = []

        while len(remaining) != 1:
            # Find selectable card_ids across remaining combinations, excluding already picked
            selectable = []
            for idx in remaining:
                for card_id in actions[idx]:
                    if card_id not in picked_cards and card_id not in selectable:
                        selectable.append(card_id)

            # Check if a combination matching exactly the picked cards exists
            has_done = any(
                len(actions[idx]) == len(picked_cards) and all(c in picked_cards for c in actions[idx])
                for idx in remaining
            )

            self.ui_state.highlighted_cards = list(selectable)
            self.ui_state.buttons = []
            if has_done:
                self.ui_state.buttons.append(Button(button_x, button_y, button_w, button_h, text=done_label))

            clicked_card = None
            done_clicked = False
            while clicked_card is None and not done_clicked:
                time.sleep(1/60)
                mx, my = get_mouse_x(), get_mouse_y()
                click = is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT)

                if has_done and self.ui_state.buttons[0].pressed(mx, my, click):
                    done_clicked = True
                    break

                for card_id in selectable:
                    card = state.get_card(card_id)
                    kt_card = self.table_state.cards[card.id]
                    w = tweak["card_width"]
                    h = tweak["card_height"]
                    if click and point_in_rect(mx, my, kt_card.x, kt_card.y, w, h):
                        clicked_card = card_id
                        break

            if done_clicked:
                for idx in remaining:
                    if len(actions[idx]) == len(picked_cards) and all(c in picked_cards for c in actions[idx]):
                        remaining = [idx]
                        break
                break

            picked_cards.append(clicked_card)
            remaining = [idx for idx in remaining if clicked_card in actions[idx]]

        selected = remaining[0]
        update_stacks(self.table_state, state, self.bottom_player)
        self.ui_state.highlighted_cards = []
        self.ui_state.buttons = []
        return selected

    def choose_action(self, state: Game_State, choice: Choice, actions: list) -> int:
        if len(actions) == 0:
            return 0
        elif len(actions) == 1:
            return 0

        if choice.description == "choose-cards":
            return self._handle_choose_cards(state, actions)

        mx, my = get_mouse_x(), get_mouse_y()
        click = is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT)

        # Display options based on action type
        count = len(actions)
        button_w = 140
        button_h = 45
        gap = 20
        total_width = count * button_w + (count - 1) * gap
        start_x = (get_screen_width() - total_width) // 2
        button_y = get_screen_height() - 50

        if choice.description == "main":
            # Same as "choose-card" but with "Pass" button for the null entry.
            self.ui_state.highlighted_cards = []
            self.ui_state.buttons = []
            for i, card_id in enumerate(actions):
                if Card_Id.is_null(card_id):
                    x = start_x
                    button = Button(x, button_y, button_w, button_h, text="Pass")
                    self.ui_state.buttons.append(button)
                else:
                    self.ui_state.highlighted_cards.append(card_id)

        elif choice.description == "choose-binary":
            self.ui_state.buttons = []
            labels = ["Yes", "No"]
            for i in range(2):
                x = start_x + i * (button_w + gap)
                button = Button(x, button_y, button_w, button_h, text=labels[i])
                self.ui_state.buttons.append(button)
        elif choice.description == "choose-card":
            self.ui_state.highlighted_cards = []
            self.ui_state.buttons = []
            for i, card_id in enumerate(actions):
                if Card_Id.is_null(card_id):
                    x = start_x
                    button = Button(x, button_y, button_w, button_h, text="Done")
                    self.ui_state.buttons.append(button)
                else:
                    card = state.get_card(card_id)
                    kt_card = self.table_state.animated_cards[card.id]
                    self.ui_state.highlighted_cards.append(card_id)

        selected = -1
        while selected == -1:
            time.sleep(1/60)  # Yield the GIL so the render thread can run
            mx, my = get_mouse_x(), get_mouse_y()
            click = is_mouse_button_pressed(MouseButton.MOUSE_BUTTON_LEFT)
            if choice.description == "main":
                for i, card_id in enumerate(actions):
                    if Card_Id.is_null(card_id):
                        if self.ui_state.buttons[0].pressed(mx, my, click):
                            selected = i
                            break
                    else:
                        card = state.get_card(card_id)
                        kt_card = self.table_state.cards[card.id]
                        w = tweak["card_width"]
                        h = tweak["card_height"]
                        if click and point_in_rect(mx, my, kt_card.x, kt_card.y, w, h):
                            selected = i
                            break
            elif choice.description == "choose-binary":
                for i in range(2):
                    if self.ui_state.buttons[i].pressed(mx, my, click):
                        selected = i
                        break
            elif choice.description == "choose-card":
                for i, card_id in enumerate(actions):
                    if Card_Id.is_null(card_id):
                        if self.ui_state.buttons[0].pressed(mx, my, click):
                            selected = i
                            break
                    else:
                        card = state.get_card(card_id)
                        kt_card = self.table_state.cards[card.id]
                        w = tweak["card_width"]
                        h = tweak["card_height"]
                        if click and point_in_rect(mx, my, kt_card.x, kt_card.y, w, h):
                            selected = i
                            break

        update_stacks(self.table_state, state, self.bottom_player)
        self.ui_state.highlighted_cards = []
        self.ui_state.buttons = []
        return selected
