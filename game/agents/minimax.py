from __future__ import annotations
from typing import Optional
from gods.models import Game_State
from game.agents.agent import Choice
from game.agents.minimax_search import Search_Context, minimax_search
import time


class Agent_Minimax:
    def __init__(self, max_depth: int = 5, time_limit: float = 10.0):
        self.max_depth = max_depth
        self.time_limit = time_limit
        self.player_index: Optional[int] = None

    def message(self, msg: str):
        pass

    def choose_action(self, state: Game_State, choice: Choice, actions: list) -> int:
        is_root = self.player_index is None
        if is_root:
            self.player_index = choice.player_index

        ctx = Search_Context(
            player_index=self.player_index,  # type: ignore[arg-type]
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

        if is_root:
            self.player_index = None
        return best_action
