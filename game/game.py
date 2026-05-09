"""Choice / Choose / Game abstractions.

After the gods C++ port, the choice machinery is implemented in gods_cpp and
re-exported here so existing imports (`from game.game import Choice, ...`)
keep working unchanged. The thin Python wrappers below (resolve_choice,
game_loop, game_frame) call into the bound C++ Game_State methods."""

from __future__ import annotations

from gods._gods_cpp import (  # noqa: F401
    Choice,
    Choose_Card,
    Choose_Cards,
    Choose_Option,
    Choose_Options,
    action_options,
)
from gods._gods_cpp import Game_State as Game


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
