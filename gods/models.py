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
class Card_Design:
    """Immutable card definition: display text and all hook methods.
    Shared across all game state copies; excluded from MCTS deep copies."""
    id: int
    name: str
    card_type: Card_Type
    color: Card_Color
    effect: str

    def on_draw(self, game: Game_State) -> list[Choice]: return []
    def on_draw_replacement(self, game: Game_State) -> list[Choice]: return []
    def on_played(self, game: Game_State) -> list[Choice]: return []
    def on_game_end(self, game: Game_State) -> list[Choice]: return []
    def on_destroyed(self, game: Game_State) -> None: pass
    def on_play(self, game: Game_State, card_played: Card) -> None: pass
    def on_destroy(self, game: Game_State, card_destroyed: Card) -> None: pass
    def on_restore(self, game: Game_State, card_restored: Card) -> None: pass
    def on_discard(self, game: Game_State, card_discarded: Card) -> list[Choice]: return []
    def on_pass(self, game: Game_State) -> list[Choice]: return []
    def on_turn_end(self, game: Game_State) -> list[Choice]: return []
    def on_turn_start(self, game: Game_State) -> list[Choice]: return []
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int: return power
    def is_indestructible(self, game: Game_State, card: Card) -> bool: return False
    def can_be_claimed(self, game: Game_State, player_index: int) -> int: return 0
    def on_scoring(self, game: Game_State) -> int: return 0
    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int: return points
    def wins_tie(self, game: Game_State, people: Card) -> bool: return False


# Global list of card designs — set once at game initialization, never deep-copied.
# All game state copies share this list; hook methods are looked up by card id.
card_designs: list[Card_Design] = []

@dataclass(slots=True)
class Card:
    """Runtime card state — only the mutable fields that change during a game.
    Trivially copyable; MCTS deep copies contain only these, not Card_Design."""
    id: int           # index into card_designs and Game_State.all_cards
    card_type: Card_Type
    color: Card_Color
    power: int       
    counters: int = 0
    destroyed: bool = False
    owner: int = -1

    # Methods that delegate to the card design's hooks, looked up by id. 
    # So Card_Design use dynamic dispatch, while Cards 
    def on_draw(self, game: Game_State) -> list[Choice]:
        return card_designs[self.id].on_draw(game)
    def on_draw_replacement(self, game: Game_State) -> list[Choice]:
        return card_designs[self.id].on_draw_replacement(game)
    def on_played(self, game: Game_State) -> list[Choice]:
        return card_designs[self.id].on_played(game)
    def on_game_end(self, game: Game_State) -> list[Choice]:
        return card_designs[self.id].on_game_end(game)
    def on_destroyed(self, game: Game_State) -> None:
        card_designs[self.id].on_destroyed(game)
    def on_play(self, game: Game_State, card_played: Card) -> None:
        card_designs[self.id].on_play(game, card_played)
    def on_destroy(self, game: Game_State, card_destroyed: Card) -> None:
        card_designs[self.id].on_destroy(game, card_destroyed)
    def on_restore(self, game: Game_State, card_restored: Card) -> None:
        card_designs[self.id].on_restore(game, card_restored)
    def on_discard(self, game: Game_State, card_discarded: Card) -> list[Choice]:
        return card_designs[self.id].on_discard(game, card_discarded)
    def on_pass(self, game: Game_State) -> list[Choice]:
        return card_designs[self.id].on_pass(game)
    def on_turn_end(self, game: Game_State) -> list[Choice]:
        return card_designs[self.id].on_turn_end(game)
    def on_turn_start(self, game: Game_State) -> list[Choice]:
        return card_designs[self.id].on_turn_start(game)
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        return card_designs[self.id].power_modifier(game, card, power)
    def is_indestructible(self, game: Game_State, card: Card) -> bool:
        return card_designs[self.id].is_indestructible(game, card)
    def can_be_claimed(self, game: Game_State, player_index: int) -> int:
        return card_designs[self.id].can_be_claimed(game, player_index)
    def on_scoring(self, game: Game_State) -> int:
        return card_designs[self.id].on_scoring(game)
    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int:
        return card_designs[self.id].on_scoring_people(game, people, points)
    def wins_tie(self, game: Game_State, people: Card) -> bool:
        return card_designs[self.id].wins_tie(game, people)

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
    owner_index: int

    @staticmethod
    def null() -> Card_Id:
        return Card_Id(area="none", card_index=-1, owner_index=-1)

    @staticmethod
    def is_null(card_id: Card_Id) -> bool:
        return card_id.area == "none" and card_id.card_index == -1 and card_id.owner_index == -1

@dataclass(slots=True)
class Game_State(Game):
    # Runtime state only
    all_cards: list[Card] = field(default_factory=list)
    players: list[Player] = field(default_factory=list)
    peoples: list[int] = field(default_factory=list)     # people cards in the center, indices into all_cards
    current_player: int = 0
    current_phase: str = "main"  # "start", "main", "end"
    shared_deck: list[int] = field(default_factory=list)
    game_over: bool = False
    # Callback fired whenever any card's mutable state changes (power, counters, destroyed, owner).
    on_cards_changed: object = field(default_factory=lambda: (lambda: None))  # Callable[[], None]

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
        assert not Card_Id.is_null(card_id)
        return self.all_cards[card_id.card_index]

    def card_list(self, player_id: int | None, area: str) -> list[Card_Id]:
        if player_id is None:
            combined = self.card_list(player_id=0, area=area) + self.card_list(player_id=1, area=area)
            return sorted(combined, key=lambda c: c.card_index)

        player = self.players[player_id]
        area_list = {"hand": player.hand, "wonders": player.wonders,
                     "discard": player.discard, "deck": player.deck}[area]
        return sorted([Card_Id(area, cid, player_id) for cid in area_list], key=lambda c: c.card_index)

    def effective_power(self, card_id: int) -> int:
        """Calculate effective power of a card, applying all wonder power modifiers."""
        card = self.all_cards[card_id]
        power = card.power + card.counters
        for player in self.players:
            for wid in player.wonders:
                power = self.all_cards[wid].power_modifier(self, card, power)
        if power < 0:
            power = 0
        return power

    def owner(self, card_id: int) -> int:
        return self.all_cards[card_id].owner