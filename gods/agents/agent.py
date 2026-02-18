from __future__ import annotations
from dataclasses import dataclass
from typing import Callable

@dataclass
class Game:
    def is_game_over(self) -> bool:
        pass

    def next_choice(self) -> Choice | None:
        pass


@dataclass
class Choice:
    player_index: int = 0
    description: str = ""
    actions: Callable[[Game], list] = lambda state: []
    resolve: Callable[[Game, int], list[Choice]] = lambda state, index: []


class Agent:
    def message(self, msg: str):
        print("Agent:", msg)

    def choose_action(self, game: Game, choice: Choice, actions: list) -> int:
        """Pick an action index. Does NOT call resolve."""
        return 0


def game_loop(game: Game, agent: Agent, callback: any = None) -> None:
    game.choices = []
    while not game.is_game_over():
        choice = game.next_choice()
        if choice is None:
            break

        if callback is not None:
            callback(game)

        actions = choice.actions(game)
        if len(actions) == 1:
            index = 0
        else:
            index = agent.choose_action(game, choice, actions)

        new_choices = choice.resolve(game, index) or []
        game.choices.extend(new_choices)

    if callback is not None:
        callback(game)
