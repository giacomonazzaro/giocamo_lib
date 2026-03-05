from __future__ import annotations
import gods.models as models_module
from gods.models import Card, Card_Design, Card_Type, Player, Game_State
from gods.cards import create_card_design
import json
import os
import random


def load_cards_from_json(filepath: str) -> list[dict]:
    with open(filepath, 'r') as f:
        return json.load(f)


def all_card_designs(default_power: int = 3) -> list[Card_Design]:
    filepath = os.path.join(os.path.dirname(__file__), "cards.json")
    data = load_cards_from_json(filepath)
    return [create_card_design(d, i) for i, d in enumerate(data)]


def get_people_cards() -> list[int]:
    return [d.id for d in all_card_designs() if d.card_type == Card_Type.PEOPLE]


def get_playable_cards() -> list[int]:
    return [d.id for d in all_card_designs() if d.card_type != Card_Type.PEOPLE]


def create_game(player1: list[int], player2: list[int], peoples: list[int], shared: list[int]) -> Game_State:
    """Initialize a new game from card designs. Creates runtime Cards and populates the global card_designs registry."""
    
    # Create one runtime Card per design; power comes from design.default_power.
    all_cards = [Card(id=d.id, card_type=d.card_type, color=d.color, power=-1) for d in models_module.card_designs]
    for card in all_cards:
        card.power = random.randint(1, 5)


    # Assign deck owners.
    for d in player1:
        all_cards[d].owner = 0
    for d in player2:
        all_cards[d].owner = 1

    random.shuffle(player1)
    random.shuffle(player2)

    p1 = Player(name="Player 1", deck=player1)
    p2 = Player(name="Player 2", deck=player2)

    game = Game_State(
        all_cards=all_cards,
        players=[p1, p2],
        peoples=peoples,
        shared_deck=shared,
    )

    # Deal initial hands.
    for player in game.players:
        for _ in range(5):
            if player.deck:
                card_id = player.deck.pop()
                player.hand.append(card_id)
    return game


def quick_setup(seed: int | None) -> Game_State:
    """Quick setup with random decks and people cards."""
    if seed is not None:
        random.seed(seed)

    card_designs = all_card_designs()
    models_module.card_designs.clear()
    models_module.card_designs.extend(card_designs)

    all_playable = get_playable_cards()

    random.shuffle(all_playable)
    deck1 = [all_playable.pop() for _ in range(10)]
    deck2 = [all_playable.pop() for _ in range(10)]

    # 4 people cards, distributed 2 per player from the start.
    all_people = get_people_cards()
    random.shuffle(all_people)
    peoples = all_people[:4]

    game = create_game(deck1, deck2, peoples, all_playable)

    # Assign initial people ownership.
    game.all_cards[peoples[0]].owner = 0
    game.all_cards[peoples[1]].owner = 0
    game.all_cards[peoples[2]].owner = 1
    game.all_cards[peoples[3]].owner = 1

    return game
