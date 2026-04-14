from __future__ import annotations
from game.game import Choice, Choose_Card, Choose_Cards, Choose_Option, Choose_Options, action_options
from gods.models import Game_State, Card_Id

class Agent_Terminal:
    def __init__(self):
        pass

    def message(self, msg: str):
        print("Terminal Agent:", msg)

    def choose_action(self, state: Game_State, choice: Choice) -> int:
        action_type = choice.actions(state)
        options = action_options(action_type)
        player = state.players[choice.player_index]

        print(f"\n{player.name}, choose an action:")

        if isinstance(action_type, Choose_Option):
            for i, label in enumerate(options):
                print(f"  {i + 1}: {label}")
        elif isinstance(action_type, Choose_Card):
            done_label = "Pass" if choice.description == "main" else "Done"
            for i, card_id in enumerate(options):
                if Card_Id.is_null(card_id):
                    print(f"  {i + 1}: {done_label}")
                else:
                    print(f"  {i + 1}: {state.get_card(card_id).name}")
        elif isinstance(action_type, Choose_Cards):
            for i, combination in enumerate(options):
                if len(combination) == 0:
                    print(f"  {i + 1}: None")
                else:
                    names = [state.get_card(card_id).name for card_id in combination]
                    print(f"  {i + 1}: {', '.join(names)}")

        selected = -1
        while selected not in range(len(options)):
            try:
                selected = int(input("Enter choice: ")) - 1
            except ValueError:
                pass

        return selected
