from __future__ import annotations
from dataclasses import dataclass
from game.game import Game, Choice
from gods.gameplay import compute_player_score
import copy
import time


@dataclass
class Search_Context:
    """Mutable state shared across minimax search functions."""
    player_index: int
    start_time: float
    time_limit: float
    time_up: bool = False
    nodes_searched: int = 0


def check_time(ctx: Search_Context) -> None:
    """Set time_up flag if we exceeded the time budget."""
    if time.time() - ctx.start_time >= ctx.time_limit:
        ctx.time_up = True


def minimax_search(
    state: Game,
    choice: Choice,
    actions: list,
    max_depth: int,
    ctx: Search_Context,
) -> list[float]:
    """Run iterative deepening minimax search.

    Returns scores where scores[i] is the score for action i.
    """
    num_actions = len(actions)
    action_order = list(range(num_actions))
    scores: list[float] = [-float("inf")] * num_actions

    for depth in range(1, max_depth + 1):
        if ctx.time_up:
            break
        ctx.nodes_searched = 0
        depth_scores = minimax_root(state, choice, actions, depth, action_order, ctx)
        if not ctx.time_up:
            scores = depth_scores
            action_order.sort(key=lambda a: depth_scores[a], reverse=True)
        if max(scores) >= 900:
            break

    return scores


def minimax_root(
    state: Game,
    choice: Choice,
    actions: list,
    depth: int,
    action_order: list[int],
    ctx: Search_Context,
) -> list[float]:
    """Try every action at the root and return scores."""
    num_actions = len(actions)
    alpha = -float("inf")
    beta = float("inf")
    scores: list[float] = [-float("inf")] * num_actions

    for action in action_order:
        if ctx.time_up:
            break
        sim = copy.deepcopy(state)
        sim_choice = copy.deepcopy(choice)
        new_choices = sim_choice.resolve(sim, action) or []
        sim.choices = list(new_choices)
        score = minimax(sim, depth, alpha, beta, ctx)
        scores[action] = score
        alpha = max(alpha, score)

    return scores


def minimax(
    state: Game,
    depth: int,
    alpha: float,
    beta: float,
    ctx: Search_Context,
) -> float:
    """Recursive minimax with alpha-beta pruning."""
    ctx.nodes_searched += 1
    if ctx.nodes_searched & 1023 == 0:
        check_time(ctx)
    if ctx.time_up:
        return 0.0

    if state.is_game_over():
        return evaluate_state(state, ctx.player_index)

    choice = state.next_choice()
    if choice is None:
        return evaluate_state(state, ctx.player_index)

    actions = choice.actions(state)
    maximizing = choice.player_index == ctx.player_index
    next_depth = depth - 1
    if next_depth < 0:
        return evaluate_state(state, ctx.player_index)

    if not actions:
        return evaluate_state(state, ctx.player_index)

    if maximizing:
        value = -float("inf")
        for action in range(len(actions)):
            sim = copy.deepcopy(state)
            sim_choice = copy.deepcopy(choice)
            new_choices = sim_choice.resolve(sim, action) or []
            sim.choices = list(new_choices) + list(sim.choices)
            score = minimax(sim, next_depth, alpha, beta, ctx)
            value = max(value, score)
            alpha = max(alpha, value)
            if alpha >= beta:
                break
        return value
    else:
        value = float("inf")
        for action in range(len(actions)):
            sim = copy.deepcopy(state)
            sim_choice = copy.deepcopy(choice)
            new_choices = sim_choice.resolve(sim, action) or []
            sim.choices = list(new_choices) + list(sim.choices)
            score = minimax(sim, next_depth, alpha, beta, ctx)
            value = min(value, score)
            beta = min(beta, value)
            if alpha >= beta:
                break
        return value


def evaluate_heuristic(state: Game, player_index: int) -> float:
    """Estimate how good a non-finished position is."""
    my_score = compute_player_score(state, player_index)
    opp_score = compute_player_score(state, 1 - player_index)

    score = float(my_score - opp_score)

    my_hand = len(state.players[player_index].hand)
    opp_hand = len(state.players[1 - player_index].hand)
    score += 0.1 * (my_hand - opp_hand)

    my_wonders = len(state.players[player_index].wonders)
    opp_wonders = len(state.players[1 - player_index].wonders)
    score += 0.2 * (my_wonders - opp_wonders)

    my_deck = len(state.players[player_index].deck)
    opp_deck = len(state.players[1 - player_index].deck)
    score += 0.05 * (my_deck - opp_deck)

    return score

def evaluate_state(state: Game, player_index: int) -> float:
    """Evaluate a finished game. Returns +1000 for win, -1000 for loss."""
    if not state.is_game_over():
        return evaluate_heuristic(state, player_index)

    my_score = compute_player_score(state, player_index)
    opp_score = compute_player_score(state, 1 - player_index)
    diff = my_score - opp_score
    if diff > 0:
        score = 1000.0
    elif diff < 0:
        score = -1000.0
    else:
        if state.ending_player == player_index:
            score = -1000.0
        else:
            score = 1000.0

    # score += evaluate_heuristic(state, player_index)
    return score
