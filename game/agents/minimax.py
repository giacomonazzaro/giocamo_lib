from __future__ import annotations
from game.game import Game, Choice
from game.agents.agent import Agent
from game.agents.minimax_search import Search_Context, minimax_search
import time


class Agent_Minimax(Agent):
    def __init__(self, max_depth: int = 5, time_limit: float = 10.0):
        self.max_depth = max_depth
        self.time_limit = time_limit

    def message(self, msg: str):
        pass
    
    def evaluate_state(state: Game, player_index: int) -> float:
        return 0.0

    def choose_action(self, state: Game, choice: Choice, actions: list) -> int:
        ctx = Search_Context(
            player_index=choice.player_index,  # type: ignore[arg-type]
            start_time=time.time(),
            time_limit=self.time_limit,
        )

        print("started:", choice.description)
        scores = minimax_search(state, choice, actions, self.max_depth, ctx)
        best_action = max(range(len(scores)), key=lambda a: scores[a])
        elapsed = time.time() - ctx.start_time
        print(
            f"  result: action={actions[best_action]} "
            f"score={scores[best_action]:.2f} nodes={ctx.nodes_searched} "
            f"time={elapsed:.2f}s"
        )

        return best_action
