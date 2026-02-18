from __future__ import annotations
from game.game import Game, Choice

class Agent:
    def message(self, msg: str):
        print("Agent:", msg)

    def choose_action(self, game: Game, choice: Choice) -> int:
        """Pick an action index. Does NOT call resolve."""
        return 0
