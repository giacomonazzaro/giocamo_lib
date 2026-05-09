from __future__ import annotations

from game.agents.agent import Agent
from kitchen_table.models import Table_State
from kitchen_table.ui import UI_State

from tressette.game.game import Choice, Choose_Card, action_options
from tressette.game.models import Game_State
from tressette.graphical.ui import HAND, TABLE_IDX


class Agent_UI(Agent):
    """Mouse-driven agent for Tressette.

    The only choice the game ever issues is a Choose_Card from the current
    player's hand to the table — so this class just listens for a card drop
    from `hand[player]` onto the table stack and maps it to an action index.
    """

    def __init__(self, table_state: Table_State, ui_state: UI_State):
        self.table_state = table_state
        self.ui_state = ui_state

    def message(self, msg: str):
        pass

    def choose_action(self, state: Game_State, choice: Choice) -> int:
        opts = action_options(choice.actions(state))  # list of int card ids.
        if not opts:
            return -1

        hand_stack = HAND[choice.player_index]

        # Permit dragging any legal card from the active player's hand onto the table.
        legal_set = set(opts)

        def is_drop_card_allowed(source_stack: int, target_stack: int, card_id: int) -> bool:
            if source_stack == target_stack:
                return True
            return (
                source_stack == hand_stack
                and target_stack == TABLE_IDX
                and card_id in legal_set
            )

        self.table_state.is_drop_card_allowed = is_drop_card_allowed

        # Highlight legal cards every frame.
        self.ui_state.highlighted_cards = {cid: cid for cid in legal_set}

        dropped = self.table_state.poll_dropped_card()
        if dropped is None:
            return -1

        source_stack, target_stack, dropped_card_id = dropped
        if (
            source_stack == hand_stack
            and target_stack == TABLE_IDX
            and dropped_card_id in legal_set
        ):
            self.ui_state.highlighted_cards = {}
            return opts.index(dropped_card_id)
        return -1
