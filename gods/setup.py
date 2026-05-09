"""Python shim for game setup.

The C++ side knows nothing about cards.json. This module reads it, populates
the C++ card_designs registry via set_card_designs, and constructs the initial
Game_State / Cards / Players using the bound C++ types."""

from __future__ import annotations

import json
import os
import random

from gods._gods_cpp import set_card_designs
from gods.models import Card, Card_Type, Game_State, Player, card_designs


def _cards_json_path() -> str:
    return os.path.join(os.path.dirname(__file__), "cards.json")


def load_cards_from_json(filepath: str) -> list[dict]:
    with open(filepath, "r") as f:
        return json.load(f)


def all_card_designs(default_power: int = 3):
    """Load card definitions from cards.json and refresh the C++ registry.

    Card names are set to their stable index ("0", "1", ...) — gods_graphical
    looks up card art by name (e.g. "00.png") and the asset directory is
    indexed numerically. The factory only activates a specialized Card_Design
    subclass when the name matches one in CARD_CLASSES; numeric names fall
    back to the no-op base. Returns the live `card_designs` proxy."""
    data = load_cards_from_json(_cards_json_path())
    entries = [
        (str(i), d["type"], d["color"], d.get("effect", ""))
        for i, d in enumerate(data)
    ]
    set_card_designs(entries)
    return card_designs


def get_people_cards() -> list[int]:
    return [d.id for d in all_card_designs() if d.card_type == Card_Type.PEOPLE]


def get_playable_cards() -> list[int]:
    return [d.id for d in all_card_designs() if d.card_type != Card_Type.PEOPLE]


def create_game(
    player1: list[int],
    player2: list[int],
    peoples: list[int],
    shared: list[int],
) -> Game_State:
    """Initialize a new game from existing card designs.
    Power is randomized per card for now (mirrors the Python original)."""
    all_cards = [
        Card(id=d.id, card_type=d.card_type, color=d.color, power=-1)
        for d in card_designs
    ]
    for card in all_cards:
        card.power = random.randint(1, 5)

    for d in player1:
        all_cards[d].owner = 0
    for d in player2:
        all_cards[d].owner = 1

    random.shuffle(player1)
    random.shuffle(player2)

    p1 = Player(name="Player 1", deck=player1)
    p2 = Player(name="Player 2", deck=player2)

    game = Game_State()
    game.all_cards = all_cards
    game.players = [p1, p2]
    game.peoples = peoples
    game.shared_deck = shared

    # Deal 5-card opening hands.
    for player in game.players:
        for _ in range(5):
            if player.deck:
                card_id = player.deck.pop()
                player.hand.append(card_id)
    return game


def create_draft() -> Game_State:
    cards = [
        Card(id=d.id, card_type=d.card_type, color=d.color, power=0)
        for d in card_designs
    ]
    shared_deck = [d.id for d in cards]
    random.shuffle(shared_deck)

    game = Game_State()
    game.all_cards = cards
    game.players = [Player(name="Player 1"), Player(name="Player 2")]
    game.peoples = []
    game.shared_deck = shared_deck

    for player in game.players:
        for _ in range(5):
            if game.shared_deck:
                card_id = game.shared_deck.pop()
                player.hand.append(card_id)
    return game


def quick_setup(seed: int | None) -> Game_State:
    """Quick setup with random decks and people cards (draft mode)."""
    if seed is not None:
        random.seed(seed)
    all_card_designs()
    return create_draft()
