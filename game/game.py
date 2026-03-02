from __future__ import annotations
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Callable
import itertools

if TYPE_CHECKING:
    from gods.models import Card_Id

@dataclass(slots=True)
class Game:
    choices: list[Choice] = field(default_factory=list)

    def is_game_over(self) -> bool:
        pass

    def next_choice(self) -> Choice | None:
        pass


@dataclass(slots=True)
class Choose_Card:
    targets: list[Card_Id]
    up_to: bool = True

@dataclass(slots=True)
class Choose_Cards:
    targets: list[Card_Id]
    count: int
    up_to: bool = True    

@dataclass(slots=True)
class Choose_Option:
    targets: list[str]

@dataclass(slots=True)
class Choose_Options:
    targets: list[str]
    count: int
    up_to: bool = True


@dataclass(slots=True)
class Choice:
    player_index: int
    description: str
    text_description: str
    actions: Callable[[Game], Choose_Card | Choose_Cards | Choose_Option | Choose_Options]
    resolve: Callable[[Game, int], list[Choice]]

def action_options(actions: Choose_Card | Choose_Cards | Choose_Option | Choose_Options) -> list:
    """Returns an indexable list of all valid options from an action type."""
    if isinstance(actions, (Choose_Card, Choose_Option)):
        return actions.targets
    # Choose_Cards / Choose_Options: enumerate all valid combinations.
    targets, count, up_to = actions.targets, actions.count, actions.up_to
    if up_to:
        result = []
        for i in range(count + 1):
            result.extend(itertools.combinations(targets, i))
        return result
    else:
        if len(targets) <= count:
            return [tuple(targets)]
        return list(itertools.combinations(targets, count))

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
        if len(action_options(choice.actions(game))) == 0:
            return None
        else:
            action_index = agent.choose_action(game, choice)
            if action_index != -1:
                resolve_choice(game, choice, action_index)
                choice = None

    return choice
