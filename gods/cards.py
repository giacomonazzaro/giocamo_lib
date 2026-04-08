from __future__ import annotations
from collections.abc import Callable
from dataclasses import dataclass
import itertools
from gods.models import Card, Card_Design, Card_Id, Card_Type, Card_Color, Game_State, card_designs
from game.game import Choice, Choose_Card, Choose_Cards, Choose_Option
from gods.gameplay import *

def create_card_design(data: dict, id: int) -> Card_Design:
    card_type = Card_Type(data["type"])
    color = Card_Color(data["color"])
    name = data["name"]

    # Use specialized class if available (defined at bottom of file).
    design_class = _get_card_class(name)
    return design_class(
        id=id,
        name=name,
        card_type=card_type,
        color=color,
        effect=data["effect"],
    )


def _get_card_class(name: str) -> type[Card_Design]:
    """Get the Card_Design class for a given card name. Returns base Card_Design if no specialized class."""
    # CARD_CLASSES is defined at the bottom of the file after all classes.
    if name in CARD_CLASSES:
        return CARD_CLASSES[name]
    return Card_Design


def all_combinations(card_ids: list[Card_Id], num_cards: int, up_to: bool) -> list[tuple]:
    num_cards = min(num_cards, len(card_ids))
    if up_to:
        combinations = []
        for k in range(0, num_cards + 1):
            combinations += itertools.combinations(card_ids, k)
        return combinations
    else:
        if len(card_ids) <= num_cards:
            return [tuple(card_ids)]
        return list(itertools.combinations(card_ids, num_cards))


def make_choose_card_choice(player_index: int, get_targets: Callable[[Game_State], list[Card_Id]], on_chosen: Callable[[Game_State, Card_Id], list[Choice]], text_description: str = "") -> Choice:
    def resolve(state, option_index):
        card_id = get_targets(state)[option_index]
        if Card_Id.is_null(card_id):
            return []
        return on_chosen(state, card_id)
    return Choice(player_index=player_index, description="choose-card", text_description=text_description,
                  actions=lambda state: Choose_Card(targets=get_targets(state)), resolve=resolve)

def make_choose_cards_choice(player_index: int, get_targets: Callable[[Game_State], list[Card_Id]], get_count: Callable[[Game_State], int], up_to: bool, on_chosen: Callable[[Game_State, tuple[Card_Id, ...]], list[Choice]], text_description: str = "") -> Choice:
    def resolve(state, option_index):
        combination = all_combinations(get_targets(state), get_count(state), up_to)[option_index]
        return on_chosen(state, combination)
    return Choice(player_index=player_index, description="choose-cards", text_description=text_description,
                  actions=lambda state: Choose_Cards(targets=get_targets(state), count=get_count(state), up_to=up_to), resolve=resolve)

def beats_opponent(game: Game_State, player_index: int, metric) -> int:
    scores = [metric(game, i) for i in range(len(game.players))]
    if scores[player_index] > scores[1 - player_index]:
        return True
    return False

def return_true(card: Card): return True

def card_selection(state: Game_State, player_id: int, area: str, f=return_true, include_null=False) -> list[Card_Id]:
    result = []
    card_list = state.card_list(player_id, area)
    for card_id in card_list:
        if f(state.get_card(card_id)):
            result.append(card_id)
    # Sorted by stable card_index so the list is canonical regardless of display order.
    result.sort(key=lambda c: c.card_index)
    if include_null:
        result.append(Card_Id.null())
    return result

def people_selection(game: Game_State, f=return_true, include_null: bool = False) -> list[Card_Id]:
    """Select people cards matching filter f, sorted by stable card_index."""
    result = [Card_Id(area="people", card_index=pid, owner_index=game.owner(pid))
              for pid in game.peoples if f(game.all_cards[pid])]
    result.sort(key=lambda c: c.card_index)
    if include_null:
        result.append(Card_Id.null())
    return result

def wonders_selection(game: Game_State, f=return_true) -> list[Card_Id]:
    """Select wonders from both players matching filter f, sorted by stable card_index."""
    result = [Card_Id(area="wonders", card_index=wid, owner_index=player_id)
              for player_id, p in enumerate(game.players)
              for wid in p.wonders if f(game.all_cards[wid])]
    result.sort(key=lambda c: c.card_index)
    return result


# Card classes with specialized effects

@dataclass(slots=True)
class Light(Card_Design):
    """When you end the game, you may play a card with power <= X"""
    def on_game_end(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        my_owner = game.owner(self.id)
        action = lambda state, card_id: play_card(state, card_id)
        def cards(state: Game_State) -> list[Card_Id]:
            return card_selection(state, state.owner(my_id), "hand",
                                  lambda c: state.effective_power(c.id) <= state.effective_power(my_id),
                                  include_null=True)
        return [make_choose_card_choice(my_owner, cards, action, "Play a card from your hand")]

@dataclass(slots=True)
class Moon(Card_Design):
    def draw_back_up(self, game: Game_State) -> list[Choice]:
        my_owner = game.owner(self.id)
        player = game.players[my_owner]
        if not player.deck or len(player.hand) >= game.effective_power(self.id):
            return []
        return draw_card(game, my_owner)

    def on_turn_start(self, game: Game_State) -> list[Choice]:
        return self.draw_back_up(game)

    def on_turn_end(self, game: Game_State) -> list[Choice]:
        return self.draw_back_up(game)

    def on_draw(self, game: Game_State) -> list[Choice]:
        return self.draw_back_up(game)

    def on_play(self, game: Game_State, card_played: Card) -> None:
        return self.draw_back_up(game)

    def on_discard(self, game: Game_State, card_discarded: Card) -> list[Choice]:
        return self.draw_back_up(game)

@dataclass(slots=True)
class War(Card_Design):
    def on_pass(self, game: Game_State) -> list[Choice]:
        my_owner = game.owner(self.id)
        if game.current_player != my_owner:
            return []
        my_id = self.id
        def get_targets(state: Game_State) -> list[Card_Id]:
            power = state.effective_power(my_id)
            return people_selection(state, lambda p: not p.destroyed and state.effective_power(p.id) <= power, include_null=True)
        def action(state, card_id):
            destroy_people(state, card_id)
            return []
        return [make_choose_card_choice(game.current_player, get_targets, action, "Destroy a people")]

@dataclass(slots=True)
class Rivers(Card_Design):
    def wins_tie(self, game: Game_State, people: Card) -> bool:
        # Allow the owner to claim tied people with power <= Rivers' power.
        return game.effective_power(people.id) <= game.effective_power(self.id)

@dataclass(slots=True)
class Earthquake(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        power = game.effective_power(self.id)
        for people_id in people_selection(game, lambda p: game.effective_power(p.id) <= power):
            destroy_people(game, people_id)
        return []

@dataclass(slots=True)
class Eruption(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def get_targets(state):
            return card_selection(state, player_id=None, area="wonders", f=lambda card: card.color == Card_Color.BLUE)
        def on_chosen(state, combination):
            for card_id in combination:
                shuffle_card_into_deck(state, card_id)
            return []
        return [make_choose_cards_choice(game.current_player, get_targets, lambda state: state.effective_power(my_id), True, on_chosen, "Shuffle blue wonders back into decks")]


@dataclass(slots=True)
class Meteorite(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        power = game.effective_power(self.id)
        opponent = 1 - game.current_player
        for target in people_selection(game, lambda p: p.owner == opponent and not p.destroyed and game.effective_power(p.id) <= power):
            destroy_people(game, target)
        return []


@dataclass(slots=True)
class Miracle(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def get_targets(state: Game_State) -> list[Card_Id]:
            return card_selection(state, state.current_player, "hand", lambda c: c.card_type == Card_Type.EVENT)
        def resolve(state: Game_State, option_index: int) -> list[Choice]:
            card_id = get_targets(state)[option_index]
            card = state.get_card(card_id)
            card.counters += state.effective_power(my_id)
            return play_card(state, card_id)
        return [Choice(player_index=game.current_player, description="choose-card", text_description="Play a card with extra power",
                       actions=lambda state: Choose_Card(targets=get_targets(state)), resolve=resolve)]


@dataclass(slots=True)
class Flashback(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        my_owner = game.owner(self.id)
        def get_targets(state):
            # Exclude flashback itself by id, not by object identity — safe after deepcopy.
            return card_selection(state, my_owner, "discard",
                                  lambda c: c.card_type == Card_Type.EVENT and c.id != my_id)
        def on_chosen(state, combination):
            player = state.players[my_owner]
            cards = [state.get_card(card_id) for card_id in combination]
            for card in cards:
                player.discard.remove(card.id)
                player.hand.append(card.id)
            return []
        return [make_choose_cards_choice(my_owner, get_targets, lambda state: state.effective_power(my_id), False, on_chosen, "Return event cards from discard to hand")]


@dataclass(slots=True)
class Prophecy(Card_Design):
    """ Play up to X extra cards """
    def on_played(self, game: Game_State) -> list[Choice]:
        return self._make_nth_choice(game, 0)

    def _make_nth_choice(self, game: Game_State, n: int) -> list[Choice]:
        my_id = self.id
        power = game.effective_power(my_id)
        if n >= power:
            return []
        my_owner = game.owner(my_id)
        def get_targets(state: Game_State) -> list[Card_Id]:
            return card_selection(state, my_owner, "hand", include_null=True)
        def make_resolve(iteration):
            def resolve(state: Game_State, option_index: int) -> list[Choice]:
                card_id = get_targets(state)[option_index]
                result: list[Choice] = []
                if not Card_Id.is_null(card_id):
                    result.extend(play_card(state, card_id))
                    # Look up design in the global registry so minimax clones use the same design.
                    result.extend(card_designs[my_id]._make_nth_choice(state, iteration + 1))
                return result
            return resolve
        return [Choice(player_index=my_owner, description="choose-card", text_description="Play an extra card",
                       actions=lambda state: Choose_Card(targets=get_targets(state)), resolve=make_resolve(n))]


@dataclass(slots=True)
class Time_Warp(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def on_chosen(state, combination):
            cards = [state.get_card(card_id) for card_id in combination]
            for card in cards:
                state.players[card.owner].wonders.remove(card.id)
                card.counters = 0
                state.players[card.owner].hand.append(card.id)
            return []
        return [make_choose_cards_choice(game.current_player, wonders_selection, lambda state: state.effective_power(my_id), True, on_chosen, "Return wonders to hand")]


@dataclass(slots=True)
class Aurora(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        power = game.effective_power(self.id)
        result = []
        for _ in range(power):
            result.extend(draw_card(game, game.current_player))
        return result


@dataclass(slots=True)
class Darkness(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        my_owner = game.owner(self.id)
        def get_targets(state):
            return card_selection(state, 1 - my_owner, "hand")
        def on_chosen(state, combination):
            return discard_cards(state, list(combination))
        return [make_choose_cards_choice(1 - my_owner, get_targets, lambda state: state.effective_power(my_id), False, on_chosen, "Discard cards from opponent's hand")]


@dataclass(slots=True)
class Spring(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def add_counters(state, card_id):
            state.get_card(card_id).counters += state.effective_power(my_id)
            return []
        get_targets = lambda state: people_selection(state, lambda p: not p.destroyed)
        return [make_choose_card_choice(game.current_player, get_targets, add_counters, "Add counters to a people")]


@dataclass(slots=True)
class Regrowth(Card_Design):
    """Restore a people with power <= X"""
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def get_targets(state: Game_State) -> list[Card_Id]:
            power = state.effective_power(my_id)
            return people_selection(state, lambda p: p.destroyed and state.effective_power(p.id) <= power)
        def restore(state, card_id):
            state.get_card(card_id).destroyed = False
            return []
        return [make_choose_card_choice(game.current_player, get_targets, restore, "Restore a destroyed people")]


@dataclass(slots=True)
class Flood(Card_Design):
    """Put X -1 counters on all people"""
    def on_played(self, game: Game_State) -> list[Choice]:
        power = game.effective_power(self.id)
        for people in game.peoples:
            game.all_cards[people].counters -= power
        return []


@dataclass(slots=True)
class Forgive(Card_Design):
    """Add X +1 counters on a people"""
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def add_counters(state, card_id):
            state.get_card(card_id).counters += state.effective_power(my_id)
            return []
        return [make_choose_card_choice(game.current_player, people_selection, add_counters, "Add counters to a people")]


@dataclass(slots=True)
class Unmaking(Card_Design):
    """Destroy a wonder with power <= X"""
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def get_targets(state: Game_State) -> list[Card_Id]:
            power = state.effective_power(my_id)
            return wonders_selection(state, f=lambda w: state.effective_power(w.id) <= power)
        def action(state, card_id):
            destroy_wonder(state, card_id)
            return []
        return [make_choose_card_choice(game.current_player, get_targets, action, "Destroy a wonder")]


@dataclass(slots=True)
class Revolt(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def get_targets(state: Game_State) -> list[Card_Id]:
            power = state.effective_power(my_id)
            return people_selection(state, lambda p: not p.destroyed and state.effective_power(p.id) <= power)
        def action(state, card_id):
            destroy_people(state, card_id)
            return []
        return [make_choose_card_choice(game.current_player, get_targets, action, "Destroy a people")]


@dataclass(slots=True)
class Blessing(Card_Design):
    def on_played(self, game: Game_State) -> list[Choice]:
        my_id = self.id
        def add_counters(state, card_id):
            state.get_card(card_id).counters += state.effective_power(my_id)
            return []
        return [make_choose_card_choice(game.current_player, wonders_selection, add_counters, "Add counters to a wonder")]


# Passive wonders - these use hooks rather than on_played

@dataclass(slots=True)
class Wisdom(Card_Design):
    """When you pass, you may play a card with power <= X"""
    def on_pass(self, game: Game_State) -> list[Choice]:
        my_owner = game.owner(self.id)
        if game.current_player != my_owner:
            return []
        my_id = self.id
        def get_targets(state: Game_State) -> list[Card_Id]:
            power = state.effective_power(my_id)
            return card_selection(state, my_owner, "hand",
                                  lambda c: c.power <= power and c.card_type != Card_Type.PEOPLE,
                                  include_null=True)
        action = lambda state, card_id: play_card(state, card_id)
        return [make_choose_card_choice(my_owner, get_targets, action, "Play a card from your hand")]


@dataclass(slots=True)
class Knowledge(Card_Design):
    """Opponent events get -X, down to a minimum of 1 power"""
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        my_owner = game.owner(self.id)
        if card.card_type == Card_Type.EVENT and card.owner == 1 - my_owner:
            reduction = game.effective_power(self.id)
            return max(1, power - reduction)
        return power


@dataclass(slots=True)
class Sky(Card_Design):
    """Your other blue wonders get +X"""
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        my_owner = game.owner(self.id)
        if card.color == Card_Color.BLUE and card.id != self.id:
            if card.id in game.players[my_owner].wonders:
                return power + game.effective_power(self.id)
        return power


@dataclass(slots=True)
class Deserts(Card_Design):
    """You can score destroyed peoples with power X or less"""
    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int:
        my_owner = game.owner(self.id)
        if people.destroyed and people.owner == my_owner:
            if game.effective_power(people.id) <= game.effective_power(self.id):
                return card_designs[people.id].can_be_claimed(game, my_owner)
        return points


@dataclass(slots=True)
class Forests(Card_Design):
    """When you pass, you may restore a people with power <= X"""
    def on_pass(self, game: Game_State) -> list[Choice]:
        my_owner = game.owner(self.id)
        if game.current_player != my_owner:
            return []
        my_id = self.id
        def get_targets(state: Game_State) -> list[Card_Id]:
            power = state.effective_power(my_id)
            return people_selection(state, lambda p: p.destroyed and state.effective_power(p.id) <= power, include_null=True)
        def restore(state, card_id):
            state.get_card(card_id).destroyed = False
            return []
        return [make_choose_card_choice(my_owner, get_targets, restore, "Restore a destroyed people")]


@dataclass(slots=True)
class Mountains(Card_Design):
    """Your peoples with power X or less are indestructible"""
    def is_indestructible(self, game: Game_State, people: Card) -> bool:
        my_owner = game.owner(self.id)
        if people.owner == my_owner:
            if game.effective_power(people.id) <= game.effective_power(self.id):
                return True
        return False


@dataclass(slots=True)
class Animals(Card_Design):
    """This is worth X points at the end of the game"""
    def on_scoring(self, game: Game_State) -> int:
        return game.effective_power(self.id)


@dataclass(slots=True)
class Love(Card_Design):
    """Your peoples are worth X points extra"""
    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int:
        if people.owner == game.owner(self.id) and not people.destroyed:
            return points + game.effective_power(self.id)
        return points

@dataclass(slots=True)
class Seas(Card_Design):
    """Your alive peoples with power X or less are worth +1 points"""
    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int:
        if people.owner == game.owner(self.id) and not people.destroyed:
            if game.effective_power(people.id) <= game.effective_power(self.id):
                return points + 1
        return points


@dataclass(slots=True)
class Fire(Card_Design):
    """Your red events get +X"""
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        if card.card_type == Card_Type.EVENT and card.color == Card_Color.RED and card.owner == game.owner(self.id):
            return power + game.effective_power(self.id)
        return power


@dataclass(slots=True)
class Sun(Card_Design):
    """Your green wonders get +X"""
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        my_owner = game.owner(self.id)
        if card.color == Card_Color.GREEN and card.card_type == Card_Type.WONDER and card.id != self.id:
            if card.id in game.players[my_owner].wonders:
                return power + game.effective_power(self.id)
        return power


@dataclass(slots=True)
class Stars(Card_Design):
    """When you draw cards, you may draw from the shared deck."""
    def on_draw_replacement(self, game: Game_State) -> list[Choice]:
        my_owner = game.owner(self.id)
        if game.current_player != my_owner:
            return []
        if not game.shared_deck:
            return []

        my_id = self.id
        def resolve(state: Game_State, option_index: int) -> list[Choice]:
            if option_index == 0:
                power = state.effective_power(my_id)
                player = state.players[my_owner]
                cid = state.shared_deck.pop()
                card = state.all_cards[cid]
                card.power = power
                card.owner = my_owner
                player.hand.append(cid)
                return []
            else:
                return draw_card(state, my_owner, replacement_effects=False)
        return [Choice(player_index=my_owner, description="choose-binary", text_description="Choose how to draw a card",
                       actions=lambda _: Choose_Option(targets=["Draw from shared deck", "Draw normally"]), resolve=resolve)]


# People card classes - each implements their own condition for ownership

@dataclass(slots=True)
class Egyptians(Card_Design):
    """You have the most total power among green wonders"""
    def can_be_claimed(self, game: Game_State, player_index: int) -> int:
        metric = lambda g, i: sum(g.effective_power(wid) for wid in g.players[i].wonders if g.all_cards[wid].color == Card_Color.GREEN)
        return beats_opponent(game, player_index, metric)

@dataclass(slots=True)
class Greeks(Card_Design):
    """You have twice or more cards in hand than the opponent"""
    def can_be_claimed(self, game: Game_State, player_index: int) -> int:
        player = game.players[player_index]
        opponent = game.players[1 - player_index]
        if len(player.hand) >= 2 * len(opponent.hand) and len(opponent.hand) > 0:
            return True
        return False

@dataclass(slots=True)
class Vikings(Card_Design):
    """You have the most cards in your deck"""
    def can_be_claimed(self, game: Game_State, player_index: int) -> int:
        return beats_opponent(game, player_index, lambda g, i: len(g.players[i].deck))

@dataclass(slots=True)
class Minoans(Card_Design):
    """You have the most wonders"""
    def can_be_claimed(self, game: Game_State, player_index: int) -> int:
        return beats_opponent(game, player_index, lambda g, i: len(g.players[i].wonders))

@dataclass(slots=True)
class Babylonians(Card_Design):
    """You have the most total power among wonders"""
    def can_be_claimed(self, game: Game_State, player_index: int) -> int:
        metric = lambda g, i: sum(g.effective_power(wid) for wid in g.players[i].wonders)
        return beats_opponent(game, player_index, metric)

@dataclass(slots=True)
class Romans(Card_Design):
    """You have the most total power among red wonders"""
    def can_be_claimed(self, game: Game_State, player_index: int) -> int:
        metric = lambda g, i: sum(g.effective_power(wid) for wid in g.players[i].wonders if g.all_cards[wid].color == Card_Color.RED)
        return beats_opponent(game, player_index, metric)

@dataclass(slots=True)
class Judeans(Card_Design):
    """You have the most total power among blue wonders"""
    def can_be_claimed(self, game: Game_State, player_index: int) -> int:
        metric = lambda g, i: sum(g.effective_power(wid) for wid in g.players[i].wonders if g.all_cards[wid].color == Card_Color.BLUE)
        return beats_opponent(game, player_index, metric)


# Registry mapping card names to their specialized classes
CARD_CLASSES: dict[str, type] = {
    "Light": Light,
    "Moon": Moon,
    "War": War,
    "Rivers": Rivers,
    "Earthquake": Earthquake,
    "Eruption": Eruption,
    "Meteorite": Meteorite,
    "Miracle": Miracle,
    "Flashback": Flashback,
    "Prophecy": Prophecy,
    "Time Warp": Time_Warp,
    "Aurora": Aurora,
    "Darkness": Darkness,
    "Spring": Spring,
    "Regrowth": Regrowth,
    "Flood": Flood,
    "Forgive": Forgive,
    "Unmaking": Unmaking,
    "Revolt": Revolt,
    "Blessing": Blessing,
    "Wisdom": Wisdom,
    "Knowledge": Knowledge,
    "Sky": Sky,
    "Deserts": Deserts,
    "Forests": Forests,
    "Mountains": Mountains,
    "Animals": Animals,
    "Love": Love,
    "Seas": Seas,
    "Fire": Fire,
    "Sun": Sun,
    "Stars": Stars,
    # People cards
    "Egyptians": Egyptians,
    "Greeks": Greeks,
    "Vikings": Vikings,
    "Minoans": Minoans,
    "Babylonians": Babylonians,
    "Romans": Romans,
    "Judeans": Judeans,
}
