from __future__ import annotations
from typing import Callable
from game.game import Game, Choice, resolve_choice
import copy
import time


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
    deadline = time.time() + time_limit
    num_actions = len(actions)
    action_order = list(range(num_actions))
    scores: list[float] = [-float("inf")] * num_actions

    for depth in range(1, max_depth + 1):
        if time.time() >= deadline:
            break
        depth_scores = minimax_root(state, evaluate, choice, actions, depth, action_order, player_index, deadline)
        if time.time() < deadline:
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
    player_index: int,
    deadline: float,
) -> list[float]:
    """Try every action at the root and return scores."""
    alpha = -float("inf")
    beta = float("inf")
    scores: list[float] = [-float("inf")] * len(actions)

    for action_index in action_order:
        if time.time() >= deadline:
            break
        new_state = copy.deepcopy(state)
        resolve_choice(new_state, choice, action_index)
        score = minimax(new_state, evaluate, depth, alpha, beta, player_index)
        scores[action_index] = score
        alpha = max(alpha, score)

    return scores


def minimax(
    state: Game,
    evaluate: Callable[[Game, int], float],
    depth: int,
    alpha: float,
    beta: float,
    player_index: int
) -> float:
    """Recursive minimax with alpha-beta pruning."""
    if state.is_game_over():
        return evaluate(state, player_index)

    choice = state.next_choice()
    if choice is None:
        return evaluate(state, player_index)

    actions = choice.actions(state)
    maximizing = choice.player_index == player_index
    next_depth = depth - 1
    if next_depth < 0:
        return evaluate(state, player_index)

    if not actions:
        return evaluate(state, player_index)

    value = -float("inf") if maximizing else float("inf")

    for action_index in range(len(actions)):
        new_state = copy.deepcopy(state)
        resolve_choice(new_state, choice, action_index)
        score = minimax(new_state, evaluate, next_depth, alpha, beta, player_index)
        if maximizing:
            value = max(value, score)
            alpha = max(alpha, value)
        else:
            value = min(value, score)
            beta = min(beta, value)
        if alpha >= beta:
            break
    return value
