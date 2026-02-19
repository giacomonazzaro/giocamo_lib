from __future__ import annotations
from dataclasses import dataclass
from typing import Callable
from game.game import Game, Choice, resolve_choice
import copy
import time


@dataclass
class Search_Context:
    """Mutable state shared across minimax search."""
    player_index: int
    deadline: float
    time_up: bool = False
    _calls_since_check: int = 0

    def check_time(self) -> None:
        self._calls_since_check += 1
        if self._calls_since_check & 1023 == 0:
            if time.time() >= self.deadline:
                self.time_up = True


def minimax_search(
    state: Game,
    evaluate: Callable[[Game, int], float],
    choice: Choice,
    actions: list,
    player_index: int,
    max_depth: int,
    time_limit: float,
) -> list[float]:
    """Run iterative deepening minimax search.

    Returns scores where scores[i] is the score for action i.
    """
    ctx = Search_Context(
        player_index=player_index,
        deadline=time.time() + time_limit,
    )
    num_actions = len(actions)
    action_order = list(range(num_actions))
    scores: list[float] = [-float("inf")] * num_actions

    for depth in range(1, max_depth + 1):
        if ctx.time_up:
            break
        depth_scores = minimax_root(state, evaluate, choice, actions, depth, action_order, ctx)
        if not ctx.time_up:
            scores = depth_scores
            action_order.sort(key=lambda a: depth_scores[a], reverse=True)
        if max(scores) >= 900:
            break

    return scores


def minimax_root(
    state: Game,
    evaluate: Callable[[Game, int], float],
    choice: Choice,
    actions: list,
    depth: int,
    action_order: list[int],
    ctx: Search_Context,
) -> list[float]:
    """Try every action at the root and return scores."""
    alpha = -float("inf")
    beta = float("inf")
    scores: list[float] = [-float("inf")] * len(actions)

    for action_index in action_order:
        if ctx.time_up:
            break
        new_state = copy.deepcopy(state)
        resolve_choice(new_state, choice, action_index)
        score = minimax(new_state, evaluate, depth, alpha, beta, ctx)
        scores[action_index] = score
        alpha = max(alpha, score)

    return scores


def minimax(
    state: Game,
    evaluate: Callable[[Game, int], float],
    depth: int,
    alpha: float,
    beta: float,
    ctx: Search_Context,
) -> float:
    """Recursive minimax with alpha-beta pruning."""
    ctx.check_time()
    if ctx.time_up:
        return 0.0

    if state.is_game_over():
        return evaluate(state, ctx.player_index)

    choice = state.next_choice()
    if choice is None:
        return evaluate(state, ctx.player_index)

    actions = choice.actions(state)
    maximizing = choice.player_index == ctx.player_index
    next_depth = depth - 1
    if next_depth < 0:
        return evaluate(state, ctx.player_index)

    if not actions:
        return evaluate(state, ctx.player_index)

    value = -float("inf") if maximizing else float("inf")

    for action_index in range(len(actions)):
        new_state = copy.deepcopy(state)
        resolve_choice(new_state, choice, action_index)
        score = minimax(new_state, evaluate, next_depth, alpha, beta, ctx)
        if maximizing:
            value = max(value, score)
            alpha = max(alpha, value)
        else:
            value = min(value, score)
            beta = min(beta, value)
        if alpha >= beta:
            break
    return value
