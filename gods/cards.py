from __future__ import annotations
from dataclasses import dataclass
import itertools
from gods.models import Card, Card_Id, Card_Type, Card_Color, Game_State, effective_power
from game.game import Choice
from gods.gameplay import *

def create_card(data: dict, default_power: int = 3) -> Card:
    card_type = Card_Type(data["type"])
    color = Card_Color(data["color"])
    # Use the power from data if it's a people card (they have fixed power), otherwise use default
    power = data["power"] if card_type == Card_Type.PEOPLE else default_power
    name = data["name"]

    # Use specialized card class if available (defined at bottom of file)
    # Import here to avoid forward reference issues
    card_class = _get_card_class(name)
    return card_class(
        name=name,
        card_type=card_type,
        power=power,
        color=color,
        effect=data["effect"]
    )


def _get_card_class(name: str) -> type:
    """Get the card class for a given card name. Returns base Card if no specialized class."""
    # CARD_CLASSES is defined at the bottom of the file after all classes
    if name in CARD_CLASSES:
        return CARD_CLASSES[name]
    return Card


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


def make_choose_card_choice(player_index, get_targets, on_chosen) -> Choice:
    def resolve(state, option_index):
        card_id = get_targets(state)[option_index]
        if Card_Id.is_null(card_id):
            return []
        return on_chosen(state, card_id) or []
    return Choice(player_index=player_index, description="choose-card",
                  actions=lambda state: get_targets(state), resolve=resolve)

def make_choose_cards_choice(player_index, get_combinations, on_chosen) -> Choice:
    def resolve(state, option_index):
        combination = get_combinations(state)[option_index]
        return on_chosen(state, combination) or []
    return Choice(player_index=player_index, description="choose-cards",
                  actions=lambda state: get_combinations(state), resolve=resolve)

def eval_most(game: Game_State, card: Card, player_index: int, metric) -> int:
    scores = [metric(game, i) for i in range(len(game.players))]
    if scores[player_index] > scores[1 - player_index]:
        return effective_power(game, card)
    return 0

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
    result = [Card_Id(area="people", card_index=pid, owner_index=game.all_cards[pid].owner)
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
class Light(Card):
    """When you end the game, you may play a card with power <= X"""
    def get_card_selection(self, state: Game_State) -> list[Card_Id]:
        return result

    def on_game_end(self, game: Game_State) -> list[Choice]:
        action = lambda state, card_id: play_card(state, card_id)
        def select(card: Card) -> bool:
            return effective_power(game, card) <= effective_power(game, self)
        def cards(state: Game_State) -> list[Card_Id]:
            return card_selection(state, self.owner, "hand", select, include_null=True) 
        return [make_choose_card_choice(self.owner, cards, action)]

@dataclass(slots=True)
class Moon(Card):
    def draw_back_up(self, game: Game_State) -> list[Choice]:
        player = game.players[self.owner]
        if not player.deck or len(player.hand) >= effective_power(game, self):
            return []
        return draw_card(game, self.owner)

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
class War(Card):
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        power = effective_power(game, self)
        return people_selection(game, lambda p: not p.destroyed and effective_power(game, p) <= power, include_null=True)

    def on_pass(self, game: Game_State) -> list[Choice]:
        if game.current_player != self.owner:
            return []
        action = lambda state, card_id: destroy_people(state, card_id)
        return [make_choose_card_choice(game.current_player, self.get_card_selection, action)]

@dataclass(slots=True)
class Rivers(Card):
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        return people_selection(game, lambda p: p.destroyed, include_null=True)

    def on_pass(self, game: Game_State) -> list[Choice]:
        action = lambda state, card_id: restore_people(state, card_id)
        return [make_choose_card_choice(game.current_player, self.get_card_selection, action)]

@dataclass(slots=True)
class Earthquake(Card):
    def on_played(self, game: Game_State) -> list[Choice]:
        power = effective_power(game, self)
        for people_id in people_selection(game, lambda p: effective_power(game, p) <= power):
            destroy_people(game, people_id)
        return []

@dataclass(slots=True)
class Eruption(Card):
    def on_played(self, game: Game_State) -> list[Choice]:
        def get_targets(state):
            card_ids = card_selection(state, player_id=None, area="wonders", f=lambda card: card.color == Card_Color.BLUE)
            power = effective_power(state, self)
            return all_combinations(card_ids, power, up_to=True)
        def on_chosen(state, combination):
            for card_id in combination:
                shuffle_card_into_deck(state, card_id)
        return [make_choose_cards_choice(game.current_player, get_targets, on_chosen)]


@dataclass(slots=True)
class Meteorite(Card):
    def on_played(self, game: Game_State) -> list[Choice]:
        power = effective_power(game, self)
        opponent = 1 - game.current_player
        for target in people_selection(game, lambda p: p.owner == opponent and not p.destroyed and effective_power(game, p) <= power):
            destroy_people(game, target)
        return []


@dataclass(slots=True)
class Miracle(Card):
    def on_played(self, game: Game_State) -> list[Choice]:
        miracle_card = self
        def actions(state: Game_State) -> list:
            return card_selection(state, state.current_player, "hand", lambda c: c.card_type == Card_Type.EVENT)
        def resolve(state: Game_State, option_index: int) -> list[Choice]:
            card_id = actions(state)[option_index]
            card = state.get_card(card_id)
            card.counters += effective_power(state, miracle_card)
            return play_card(state, card_id)
        return [Choice(player_index=game.current_player, description="choose-card",
                       actions=actions, resolve=resolve)]


@dataclass(slots=True)
class Flashback(Card):
    def on_played(self, game: Game_State) -> list[Choice]:
        flashback = self
        def get_cards(state):
            return card_selection(state, state.current_player, "discard",
                                  lambda c: c.card_type == Card_Type.EVENT and c != flashback)
        def get_combos(state):
            return all_combinations(get_cards(state), effective_power(state, flashback), up_to=True)
        def on_chosen(state, combination):
            player = state.players[state.current_player]
            cards = [state.get_card(card_id) for card_id in combination]
            for card in cards:
                player.discard.remove(card.id)
                player.hand.append(card.id)
        return [make_choose_cards_choice(game.current_player, get_combos, on_chosen)]


@dataclass(slots=True)
class Prophecy(Card):
    """ Play up to X extra cards """
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        return card_selection(game, self.owner, "hand", include_null=True)

    def on_played(self, game: Game_State) -> list[Choice]:
        return self._make_nth_choice(game, 0)

    def _make_nth_choice(self, game: Game_State, n: int) -> list[Choice]:
        power = effective_power(game, self)
        if n >= power:
            return []
        prophecy = self
        def actions(state: Game_State) -> list:
            return self.get_card_selection(state)
        def make_resolve(iteration):
            def resolve(state: Game_State, option_index: int) -> list[Choice]:
                card_id = actions(state)[option_index]
                result: list[Choice] = []
                if not Card_Id.is_null(card_id):
                    result.extend(play_card(state, card_id))
                    result.extend(prophecy._make_nth_choice(state, iteration + 1))
                return result
            return resolve
        return [Choice(player_index=self.owner, description="choose-card",
                       actions=actions, resolve=make_resolve(n))]


@dataclass(slots=True)
class Time_Warp(Card):
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        return wonders_selection(game)

    def on_played(self, game: Game_State) -> list[Choice]:
        time_warp = self
        def get_combos(state):
            return all_combinations(time_warp.get_card_selection(state), effective_power(state, time_warp), up_to=True)
        def on_chosen(state, combination):
            cards = [state.get_card(card_id) for card_id in combination]
            for card in cards:
                state.players[card.owner].wonders.remove(card.id)
                card.counters = 0
                state.players[card.owner].hand.append(card.id)
        return [make_choose_cards_choice(game.current_player, get_combos, on_chosen)]


@dataclass(slots=True)
class Aurora(Card):
    def on_played(self, game: Game_State) -> list[Choice]:
        power = effective_power(game, self)
        result = []
        for _ in range(power):
            result.extend(draw_card(game, game.current_player))
        return result


@dataclass(slots=True)
class Darkness(Card):
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        return card_selection(game, 1 - self.owner, "hand")

    def on_played(self, game: Game_State) -> list[Choice]:
        darkness = self
        def get_combos(state):
            return all_combinations(darkness.get_card_selection(state), effective_power(state, darkness), up_to=False)
        def on_chosen(state, combination):
            discard_cards(state, list(combination))
            
        return [make_choose_cards_choice(1 - self.owner, get_combos, on_chosen)]


@dataclass(slots=True)
class Spring(Card):
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        return people_selection(game, lambda p: not p.destroyed)

    def on_played(self, game: Game_State) -> list[Choice]:
        spring_card = self
        def add_counters(state, card_id):
            state.get_card(card_id).counters += effective_power(state, spring_card)
        return [make_choose_card_choice(game.current_player, self.get_card_selection, add_counters)]


@dataclass(slots=True)
class Regrowth(Card):
    """Restore a people with power <= X"""
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        power = effective_power(game, self)
        return people_selection(game, lambda p: p.destroyed and effective_power(game, p) <= power)

    def on_played(self, game: Game_State) -> list[Choice]:
        def restore(state, card_id):
            state.get_card(card_id).destroyed = False
        return [make_choose_card_choice(game.current_player, self.get_card_selection, restore)]


@dataclass(slots=True)
class Flood(Card):
    """Put X -1 counters on all people"""
    def on_played(self, game: Game_State) -> list[Choice]:
        power = effective_power(game, self)
        for people in game.peoples:
            game.all_cards[people].counters -= power
        return []


@dataclass(slots=True)
class Forgive(Card):
    """Add X +1 counters on a people"""
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        return people_selection(game)

    def on_played(self, game: Game_State) -> list[Choice]:
        forgive_card = self
        def add_counters(state, card_id):
            state.get_card(card_id).counters += effective_power(state, forgive_card)
        return [make_choose_card_choice(game.current_player, self.get_card_selection, add_counters)]


@dataclass(slots=True)
class Unmaking(Card):
    """Destroy a wonder with power <= X"""
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        power = effective_power(game, self)
        return wonders_selection(game, f=lambda w: effective_power(game, w) <= power)

    def on_played(self, game: Game_State) -> list[Choice]:
        action = lambda state, card_id: destroy_wonder(state, card_id)
        return [make_choose_card_choice(game.current_player, self.get_card_selection, action)]


@dataclass(slots=True)
class Revolt(Card):
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        power = effective_power(game, self)
        return people_selection(game, lambda p: not p.destroyed and effective_power(game, p) <= power)

    def on_played(self, game: Game_State) -> list[Choice]:
        action = lambda state, card_id: destroy_people(state, card_id)
        return [make_choose_card_choice(game.current_player, self.get_card_selection, action)]


@dataclass(slots=True)
class Blessing(Card):
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        return wonders_selection(game)

    def on_played(self, game: Game_State) -> list[Choice]:
        blessing_card = self
        def add_counters(state, card_id):
            state.get_card(card_id).counters += effective_power(state, blessing_card)
        return [make_choose_card_choice(game.current_player, self.get_card_selection, add_counters)]


# Passive wonders - these use hooks rather than on_played

@dataclass(slots=True)
class Wisdom(Card):
    """When you pass, you may play a card with power <= X"""
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        power = effective_power(game, self)
        return card_selection(game, self.owner, "hand",
                              lambda c: c.power <= power and c.card_type != Card_Type.PEOPLE,
                              include_null=True)

    def on_pass(self, game: Game_State) -> list[Choice]:
        if game.current_player != self.owner:
            return []
        action = lambda state, card_id: play_card(state, card_id)
        return [make_choose_card_choice(self.owner, self.get_card_selection, action)]


@dataclass(slots=True)
class Knowledge(Card):
    """Opponent events get -X, down to a minimum of 1 power"""
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        if card.card_type == Card_Type.EVENT:
            # Check if card belongs to opponent.
            opponent_idx = 1 - self.owner
            if card.id in game.players[opponent_idx].hand:
                reduction = effective_power(game, self)
                return max(1, power - reduction)
        return power


@dataclass(slots=True)
class Sky(Card):
    """Your other blue wonders get +X"""
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        if card.color == Card_Color.BLUE and card != self:
            if card.id in game.players[self.owner].wonders:
                return power + effective_power(game, self)
        return power


@dataclass(slots=True)
class Deserts(Card):
    """You can score destroyed peoples with power X or less"""
    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int:
        if people.destroyed and people.owner == self.owner:
            if effective_power(game, people) <= effective_power(game, self):
                return people.eval_points(game, self.owner)
        return points


@dataclass(slots=True)
class Forests(Card):
    """When you pass, you may restore a people with power <= X"""
    def get_card_selection(self, game: Game_State) -> list[Card_Id]:
        power = effective_power(game, self)
        return people_selection(game, lambda p: p.destroyed and effective_power(game, p) <= power, include_null=True)

    def on_pass(self, game: Game_State) -> list[Choice]:
        if game.current_player != self.owner:
            return []
        def restore(state, card_id):
            state.get_card(card_id).destroyed = False
        return [make_choose_card_choice(self.owner, self.get_card_selection, restore)]


@dataclass(slots=True)
class Mountains(Card):
    """Your peoples with power X or less are indestructible"""
    def is_indestructible(self, game: Game_State, people: Card) -> bool:
        if people.owner == self.owner:
            if effective_power(game, people) <= effective_power(game, self):
                return True
        return False


@dataclass(slots=True)
class Animals(Card):
    """This is worth X points at the end of the game"""
    def on_scoring(self, game: Game_State) -> int:
        return effective_power(game, self)


@dataclass(slots=True)
class Love(Card):
    """Your peoples are worth X points extra"""
    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int:
        if people.owner == self.owner and not people.destroyed:
            return points + effective_power(game, self)
        return points

@dataclass(slots=True)
class Seas(Card):
    """Your alive peoples with power X or less are worth +1 points"""
    def on_scoring_people(self, game: Game_State, people: Card, points: int) -> int:
        if people.owner == self.owner and not people.destroyed:
            if effective_power(game, people) <= effective_power(game, self):
                return points + 1
        return points


@dataclass(slots=True)
class Fire(Card):
    """Your red events get +X"""
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        if card.card_type == Card_Type.EVENT and card.color == Card_Color.RED and card.owner == self.owner:
            return power + effective_power(game, self)
        return power


@dataclass(slots=True)
class Sun(Card):
    """Your green wonders get +X"""
    def power_modifier(self, game: Game_State, card: Card, power: int) -> int:
        if card.color == Card_Color.GREEN and card != self:
            if card.id in game.players[self.owner].wonders:
                return power + effective_power(game, self)
        return power


@dataclass(slots=True)
class Stars(Card):
    """When you draw cards, you may draw from the shared deck."""
    def on_draw_replacement(self, game: Game_State) -> list[Choice]:
        if game.current_player != self.owner:
            return []
        if not game.shared_deck:
            return []

        stars_card = self
        def actions(state: Game_State) -> list:
            return ["Draw from shared deck", "Draw normally"]
        def resolve(state: Game_State, option_index: int) -> list[Choice]:
            player_id = stars_card.owner
            if option_index == 0:
                power = effective_power(state, stars_card)
                player = state.players[player_id]
                cid = state.shared_deck.pop()
                card = state.all_cards[cid]
                card.power = power
                card.owner = stars_card.owner
                player.hand.append(cid)
                return []
            else:
                return draw_card(state, player_id, replacement_effects=False)
        return [Choice(player_index=self.owner, description="choose-binary",
                       actions=actions, resolve=resolve)]


# People card classes - each implements their own condition for ownership

@dataclass(slots=True)
class Egyptians(Card):
    """You have the most total power among green wonders"""
    def eval_points(self, game: Game_State, player_index: int) -> int:
        metric = lambda g, i: sum(effective_power(g, g.all_cards[wid]) for wid in g.players[i].wonders if g.all_cards[wid].color == Card_Color.GREEN)
        return eval_most(game, self, player_index, metric)

@dataclass(slots=True)
class Greeks(Card):
    """You have twice or more cards in hand than the opponent"""
    def eval_points(self, game: Game_State, player_index: int) -> int:
        player = game.players[player_index]
        opponent = game.players[1 - player_index]
        if len(player.hand) >= 2 * len(opponent.hand) and len(opponent.hand) > 0:
            return effective_power(game, self)
        return 0

@dataclass(slots=True)
class Vikings(Card):
    """You have the most cards in your deck"""
    def eval_points(self, game: Game_State, player_index: int) -> int:
        return eval_most(game, self, player_index, lambda g, i: len(g.players[i].deck))

@dataclass(slots=True)
class Minoans(Card):
    """You have the most wonders"""
    def eval_points(self, game: Game_State, player_index: int) -> int:
        return eval_most(game, self, player_index, lambda g, i: len(g.players[i].wonders))

@dataclass(slots=True)
class Babylonians(Card):
    """You have the most total power among wonders"""
    def eval_points(self, game: Game_State, player_index: int) -> int:
        metric = lambda g, i: sum(effective_power(g, g.all_cards[wid]) for wid in g.players[i].wonders)
        return eval_most(game, self, player_index, metric)

@dataclass(slots=True)
class Romans(Card):
    """You have the most total power among red wonders"""
    def eval_points(self, game: Game_State, player_index: int) -> int:
        metric = lambda g, i: sum(effective_power(g, g.all_cards[wid]) for wid in g.players[i].wonders if g.all_cards[wid].color == Card_Color.RED)
        return eval_most(game, self, player_index, metric)

@dataclass(slots=True)
class Judeans(Card):
    """You have the most total power among blue wonders"""
    def eval_points(self, game: Game_State, player_index: int) -> int:
        metric = lambda g, i: sum(effective_power(g, g.all_cards[wid]) for wid in g.players[i].wonders if g.all_cards[wid].color == Card_Color.BLUE)
        return eval_most(game, self, player_index, metric)


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
