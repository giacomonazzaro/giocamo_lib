from __future__ import annotations
from dataclasses import dataclass, field
from typing import Callable

@dataclass(slots=True)
class Game:
    choices: list[Choice] = field(default_factory=list)

    def is_game_over(self) -> bool:
        pass

    def next_choice(self) -> Choice | None:
        pass


@dataclass(slots=True)
class Choice:
    player_index: int = 0
    description: str = ""
    actions: Callable[[Game], list] = lambda state: []
    resolve: Callable[[Game, int], list[Choice]] = lambda state, index: []

def resolve_choice(game: Game, choice: Choice, index: int):
    new_choices = choice.resolve(game, index) or []
    game.choices.extend(new_choices)

def game_loop(game: Game, agent: Agent, callback: any = None) -> None:
    while not game.is_game_over():
        choice = game.next_choice()
        if choice is None:
            break

        index = agent.choose_action(game, choice)
        resolve_choice(game, choice, index)
    
    if callback is not None:
        callback(game)


def game_frame(game: Game, agent: Agent, choice: Choice | None) -> Choice | None:
    # Only fetch a new choice when the previous one has been resolved.
    if choice is None:
        choice = game.next_choice()

    if choice is not None:
        if len(choice.actions(game)) == 0:
            return None
        else:
            action_index = agent.choose_action(game, choice)
            if action_index != -1:
                resolve_choice(game, choice, action_index)
                choice = None

    return choice
