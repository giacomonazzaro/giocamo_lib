from __future__ import annotations
from dataclasses import dataclass, field
from typing import Optional
from enum import Enum

from game.game import Game, Choice


class Card_Type(Enum):
    WONDER = "wonder"
    EVENT = "event"
    PEOPLE = "people"


class Card_Color(Enum):
    GREEN = "green"
    BLUE = "blue"
    RED = "red"
    YELLOW = "yellow"


@dataclass
class Card:
    name: str
    card_type: Card_Type
    power: int
    color: Card_Color
    effect: str
    destroyed: bool = False
    counters: int = 0  # +1 counters
    owner: Optional[int] = None  # player index who controls this card (for people)
    id: int = -1

    def on_draw(self, game: Game_State) -> list[Choice]: return []
    def on_draw_replacement(self, game: Game_State) -> list[Choice]: return []
    def on_played(self, game: Game_State) -> list[Choice]: return []
    def on_destroyed(self, game: Game_State) -> None: pass
    def on_play(self, game: Game_State, card_played: Card) -> None: pass
    def on_destroy(self, game: Game_State, card_destroyed: Card) -> None: pass
    def on_restore(self, game: Game_State, card_destroyed: Card) -> None: pass
    def on_discard(self, game: Game_State, card_discarded: Card) ->  list[Choice]: return []
    def on_pass(self, game: Game_State) -> list[Choice]: return []
    def on_turn_end(self, game: Game_State) -> list[Choice]: return []
    def on_turn_start(self, game: Game_State) -> list[Choice]: return []
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        """Modify another card's power. Override in subclasses."""
        return power

    def eval_points(self, game: Game_State, player_index: int) -> int:
        """Evaluate points for a people card. Override in people subclasses."""
        return 0

    def on_scoring(self, game: Game_State) -> int:
        """Points from this wonder at end of game. Override in subclasses."""
        return 0

    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int:
        """Bonus points for a people card. Override in subclasses."""
        return points

    def wins_tie(self, game: Game_State, people: Card) -> bool:
        """Whether this card breaks ties for a people. Override in subclasses."""
        return False

@dataclass
class Player:
    name: str
    deck: list[Card] = field(default_factory=list)
    hand: list[Card] = field(default_factory=list)
    discard: list[Card] = field(default_factory=list)
    wonders: list[Card] = field(default_factory=list)  # wonders in play

@dataclass(frozen=True)
class Card_Id:
    area: str  # "deck", "hand", "discard", "wonders", "people"
    card_index: int
    owner_index: Optional[int] = None  # None means neutral / no owner

    @staticmethod
    def null() -> Card_Id:
        return Card_Id(area="none", card_index=-1, owner_index=-1)

    @staticmethod
    def is_null(card_id: Card_Id) -> bool:
        return card_id.area == "none" and card_id.card_index == -1 and card_id.owner_index == -1

@dataclass
class Game_State(Game):
    players: list[Player] = field(default_factory=list)
    peoples: list[Card] = field(default_factory=list)  # people cards in the center
    current_player: int = 0
    current_phase: str = "main"  # "start", "main", "end"
    game_ending: bool = False  # someone declared end
    ending_player: Optional[int] = None  # who triggered the end
    final_turn: bool = False  # is this the final turn?
    game_over: bool = False
    extra_turns: int = 0  # for Prophecy card
    shared_deck: list[Card] = field(default_factory=list)  # for Stars card

    def is_game_over(self) -> bool:
        return self.game_over

    def next_choice(self) -> Choice | None:
        from gods.gameplay import make_main_choice, check_people_conditions, draw_card
        while not self.game_over:
            if self.choices:
                choice = self.choices.pop(0)
                actions = choice.actions(self)
                if not actions:
                    continue
                return choice

            if self.current_phase == "start":
                for w in self.active_player().wonders:
                    self.choices.extend(w.on_turn_start(self))
                self.current_phase = "main"

            elif self.current_phase == "main":
                self.choices.append(make_main_choice(self))

            elif self.current_phase == "post-play":
                check_people_conditions(self)
                self.current_phase = "end"

            elif self.current_phase == "post-pass-effects":
                player = self.active_player()
                if not player.deck:
                    self.game_over = True
                    self.ending_player = self.current_player
                    continue
                new_choices = draw_card(self, self.current_player)
                self.choices.extend(new_choices)
                check_people_conditions(self)
                self.current_phase = "post-pass-draw"

            elif self.current_phase == "post-pass-draw":
                check_people_conditions(self)
                self.current_phase = "end"

            elif self.current_phase == "end":
                for w in self.active_player().wonders:
                    self.choices.extend(w.on_turn_end(self))
                self.switch_turn()
                self.current_phase = "start"

        return None

    def active_player(self) -> Player:
        return self.players[self.current_player]

    def opponent(self) -> Player:
        return self.players[1 - self.current_player]

    def peoples_ids(self) -> list[Card_Id]:
        return [Card_Id(area="people", card_index=i, owner_index=card.owner) for (i, card) in enumerate(self.peoples)]

    def wonders(state: Game_State, player_index: int) -> list[Card_Id]:
        return [Card_Id(area="wonders", card_index=i, owner_index=player_index) for i in range(len(state.players[player_index].wonders))]

    def discard(state: Game_State, player_index: int) -> list[Card_Id]:
        return [Card_Id(area="discard", card_index=i, owner_index=player_index) for i in range(len(state.players[player_index].discard))]

    def hand(state: Game_State, player_index: int) -> list[Card_Id]:
        return [Card_Id(area="hand", card_index=i, owner_index=player_index) for i in range(len(state.players[player_index].hand))]

    def switch_turn(self) -> None:
        if self.extra_turns > 0:
            self.extra_turns -= 1
            return

        if self.final_turn:
            self.game_over = True
            return

        self.current_player = 1 - self.current_player

        if self.game_ending and self.current_player != self.ending_player:
            self.final_turn = True

    def get_card(self, card_id: Card_Id) -> Card:
        assert not Card_Id.is_null(card_id)

        if card_id.area == "people":
            assert self.peoples[card_id.card_index].owner == card_id.owner_index
            return self.peoples[card_id.card_index]
        assert card_id.owner_index is not None

        if card_id.area == "deck":
            return self.players[card_id.owner_index].deck[card_id.card_index]
        elif card_id.area == "hand":
            return self.players[card_id.owner_index].hand[card_id.card_index]
        elif card_id.area == "discard":
            return self.players[card_id.owner_index].discard[card_id.card_index]
        elif card_id.area == "wonders":
            return self.players[card_id.owner_index].wonders[card_id.card_index]
        else:
            raise ValueError(f"Invalid card area: {card_id.area}")

    def card_list(self, player_id: int | None, area: str) -> list[Card_Id]:
        if player_id is None:
            return self.card_list(player_id=0, area=area) + self.card_list(player_id=1, area=area)

        if area == "hand":
            return [Card_Id(area, i, player_id) for i in range(len(self.players[player_id].hand))]
        if area == "wonders":
            return [Card_Id(area, i, player_id) for i in range(len(self.players[player_id].wonders))]
        if area == "discard":
            return [Card_Id(area, i, player_id) for i in range(len(self.players[player_id].discard))]
        if area == "deck":
            return [Card_Id(area, i, player_id) for i in range(len(self.players[player_id].deck))]
        assert False
        return []

def effective_power(game: Game_State, card: Card) -> int:
    """Calculate effective power of a card, applying all wonder power modifiers."""
    power = card.power + card.counters
    # Apply power modifiers from all wonders in play
    for player in game.players:
        for wonder in player.wonders:
            power = wonder.power_modifier(game, card, power)
    if power < 0:
        power = 0
    return power
