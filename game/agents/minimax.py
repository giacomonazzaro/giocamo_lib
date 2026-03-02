from __future__ import annotations
from game.game import Game, Choice, action_options
from game.agents.agent import Agent
from game.agents.minimax_search import minimax_search
import time


class Agent_Minimax(Agent):
    def __init__(self, max_depth: int = 5, time_limit: float = 10.0):
        self.max_depth = max_depth
        self.time_limit = time_limit

    def message(self, msg: str):
        pass
    
    def evaluate_state(self, state: Game, player_index: int) -> float:
        return 0.0

    def choose_action(self, state: Game, choice: Choice) -> int:
        actions = action_options(choice.actions(state))

        print("started:", choice.description)
        start_time = time.time()
        scores = minimax_search(
            state, self.evaluate_state, choice, actions,
            choice.player_index, self.max_depth, self.time_limit,
        )
        best_action = max(range(len(scores)), key=lambda a: scores[a])
        elapsed = time.time() - start_time
        print(
            f"  result: action={actions[best_action]} "
            f"score={scores[best_action]:.2f} time={elapsed:.2f}s"
        )

        return best_action
