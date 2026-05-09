"""Choice / Game wrappers backed by the C++ Tressette module.

Mirrors game/game.py but routes through tressette.game._tressette_cpp so the
agents (Agent_UI, Agent_Duel, Agent_Process) can do isinstance() checks against
the same Choose_Card class that the C++ side returns from choice.actions()."""

from __future__ import annotations

from tressette.game._tressette_cpp import (  # noqa: F401
    Choice,
    Choose_Card,
    Choose_Cards,
    Choose_Option,
    Choose_Options,
    action_options,
)
from tressette.game._tressette_cpp import Game_State as Game


def resolve_choice(game: Game, choice: Choice, index: int):
    new_choices = choice.resolve(game, index) or []
    game.choices.extend(new_choices)


def game_loop(game: Game, agent, callback=None) -> None:
    while not game.is_game_over():
        choice = game.next_choice()
        if choice is None:
            break
        index = agent.choose_action(game, choice)
        resolve_choice(game, choice, index)
    if callback is not None:
        callback(game)


def game_frame(game: Game, agent, choice):
    # Only fetch a new choice when the previous one has been resolved.
    if choice is None:
        choice = game.next_choice()
    if choice is not None:
        if len(action_options(choice.actions(game))) == 0:
            return None
        action_index = agent.choose_action(game, choice)
        if action_index != -1:
            resolve_choice(game, choice, action_index)
            choice = None
    return choice
