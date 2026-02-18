from gods.models import Game_State
from gods.agents.agent import Choice
import random

class Agent_Random:
    def __init__(self):
        pass

    def message(self, msg: str):
        pass  # Silent agent

    def choose_action(self, state: Game_State, choice: Choice, actions: list) -> int:
        return random.randint(0, len(actions) - 1)
