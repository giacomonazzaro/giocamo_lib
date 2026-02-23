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


@dataclass(slots=True)
class Card:
    name: str
    card_type: Card_Type
    power: int
    color: Card_Color
    effect: str
    destroyed: bool = False
    counters: int = 0  # +1 counters
    owner: Optional[int] = None  # player index who controls this card (for people)
    id: int = -1  # index into Game_State.all_cards; also serves as the kitchen_table card id since both lists are aligned

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

@dataclass(slots=True)
class Player:
    name: str
    deck: list[int] = field(default_factory=list)     # indices into Game_State.all_cards
    hand: list[int] = field(default_factory=list)     # indices into Game_State.all_cards
    discard: list[int] = field(default_factory=list)  # indices into Game_State.all_cards
    wonders: list[int] = field(default_factory=list)  # wonders in play, indices into Game_State.all_cards

@dataclass(slots=True, frozen=True)
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

@dataclass(slots=True)
class Game_State(Game):
    all_cards: list[Card] = field(default_factory=list)  # master registry of all cards in the game
    players: list[Player] = field(default_factory=list)
    peoples: list[int] = field(default_factory=list)     # people cards in the center, indices into all_cards
    current_player: int = 0
    current_phase: str = "main"  # "start", "main", "end"
    shared_deck: list[int] = field(default_factory=list)
    game_over: bool = False

    def is_game_over(self) -> bool:
        return self.game_over

    def next_choice(self) -> Choice | None:
        from gods.gameplay import make_main_choice, draw_card
        while not self.game_over:
            if self.choices:
                choice = self.choices.pop(0)
                actions = choice.actions(self)
                if not actions:
                    continue
                return choice

            if self.current_phase == "start":
                for wid in self.active_player().wonders:
                    self.choices.extend(self.all_cards[wid].on_turn_start(self))
                self.current_phase = "main"

            elif self.current_phase == "main":
                self.choices.append(make_main_choice(self))

            elif self.current_phase == "post-play":
                self.current_phase = "claim"

            elif self.current_phase == "post-pass-effects":
                player = self.active_player()
                if not player.deck:
                    self.game_over = True
                    continue
                new_choices = draw_card(self, self.current_player)
                self.choices.extend(new_choices)
                self.current_phase = "post-pass-draw"

            elif self.current_phase == "post-pass-draw":
                self.current_phase = "claim"

            elif self.current_phase == "claim":
                from gods.gameplay import make_claim_choice
                claim = make_claim_choice(self)
                if claim is not None:
                    self.choices.append(claim)
                self.current_phase = "end"

            elif self.current_phase == "end":
                for wid in self.active_player().wonders:
                    self.choices.extend(self.all_cards[wid].on_turn_end(self))
                self.switch_turn()
                self.current_phase = "start"

        return None

    def active_player(self) -> Player:
        return self.players[self.current_player]

    def opponent(self) -> Player:
        return self.players[1 - self.current_player]

    def peoples_ids(self) -> list[Card_Id]:
        # card_index is the stable Card.id (index into all_cards), sorted for canonical ordering.
        return sorted([Card_Id(area="people", card_index=pid, owner_index=self.all_cards[pid].owner)
                       for pid in self.peoples], key=lambda c: c.card_index)

    def wonders(state: Game_State, player_index: int) -> list[Card_Id]:
        return sorted([Card_Id(area="wonders", card_index=wid, owner_index=player_index)
                       for wid in state.players[player_index].wonders], key=lambda c: c.card_index)

    def discard(state: Game_State, player_index: int) -> list[Card_Id]:
        return sorted([Card_Id(area="discard", card_index=did, owner_index=player_index)
                       for did in state.players[player_index].discard], key=lambda c: c.card_index)

    def hand(state: Game_State, player_index: int) -> list[Card_Id]:
        return sorted([Card_Id(area="hand", card_index=hid, owner_index=player_index)
                       for hid in state.players[player_index].hand], key=lambda c: c.card_index)

    def switch_turn(self) -> None:
        self.current_player = 1 - self.current_player

    def get_card(self, card_id: Card_Id) -> Card:
        # card_index is the stable Card.id, so lookup is a direct index into all_cards.
        assert not Card_Id.is_null(card_id)
        return self.all_cards[card_id.card_index]

    def card_list(self, player_id: int | None, area: str) -> list[Card_Id]:
        if player_id is None:
            # Merge both players' lists and sort globally for canonical ordering.
            combined = self.card_list(player_id=0, area=area) + self.card_list(player_id=1, area=area)
            return sorted(combined, key=lambda c: c.card_index)

        player = self.players[player_id]
        area_list = {"hand": player.hand, "wonders": player.wonders,
                     "discard": player.discard, "deck": player.deck}[area]
        # card_index is the stable Card.id, sorted for canonical ordering.
        return sorted([Card_Id(area, cid, player_id) for cid in area_list], key=lambda c: c.card_index)

def effective_power(game: Game_State, card: Card) -> int:
    """Calculate effective power of a card, applying all wonder power modifiers."""
    power = card.power + card.counters
    # Apply power modifiers from all wonders in play.
    for player in game.players:
        for wid in player.wonders:
            power = game.all_cards[wid].power_modifier(game, card, power)
    if power < 0:
        power = 0
    return power
