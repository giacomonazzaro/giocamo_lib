from game.game import Game, Choice, action_options
import random

class Agent_Random:
    def __init__(self):
        pass

    def message(self, msg: str):
        pass  # Silent agent

    def choose_action(self, state: Game, choice: Choice) -> int:
        actions = action_options(choice.actions(state))
        return random.randint(0, len(actions) - 1)
