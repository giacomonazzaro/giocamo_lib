
from gods.models import Card, Card_Type, Card_Color, Player, Game_State
from gods.cards import create_card
import json
import copy
import random

def load_cards_from_json(filepath: str) -> list[dict]:
    with open(filepath, 'r') as f:
        return json.load(f)


def get_all_cards() -> list[Card]:
    import os
    filepath = os.path.join(os.path.dirname(__file__), "cards.json")
    data = load_cards_from_json(filepath)
    return [create_card(d, 3) for d in data]


def get_people_cards() -> list[Card]:
    return [c for c in get_all_cards() if c.card_type == Card_Type.PEOPLE]


def get_playable_cards() -> list[Card]:
    return [c for c in get_all_cards() if c.card_type != Card_Type.PEOPLE]


def create_game(player1_deck: list[Card], player2_deck: list[Card], people_cards: list[Card], shared_deck: list[Card]) -> Game_State:
    """Initialize a new game with the given decks and people cards."""
    # Assign owners before building the card registry.
    for card in player1_deck:
        card.owner = 0
    for card in player2_deck:
        card.owner = 1

    # Build the master card registry and assign stable integer IDs.
    all_cards = player1_deck + player2_deck + people_cards + shared_deck
    for i, card in enumerate(all_cards):
        card.id = i

    random.shuffle(player1_deck)
    random.shuffle(player2_deck)

    p1 = Player(name="Player 1", deck=[c.id for c in player1_deck])
    p2 = Player(name="Player 2", deck=[c.id for c in player2_deck])

    game = Game_State(
        all_cards=all_cards,
        players=[p1, p2],
        peoples=[c.id for c in people_cards],
    )
    game.shared_deck = [c.id for c in shared_deck]

    for player in game.players:
        for _ in range(5):
            if player.deck:
                card_id = player.deck.pop()
                player.hand.append(card_id)
    return game

def quick_setup(seed: int | None) -> Game_State:
    if seed is not None:
        random.seed(seed)

    """Quick setup with random decks and people cards."""
    all_playable = get_playable_cards()
    for i, card in enumerate(all_playable):
        all_playable[i].power = random.randint(1, 5)
    

    # Random decks for both players
    random.shuffle(all_playable)
    deck1 = []
    deck2 = []
    for i in range(10):
        deck1.append(all_playable.pop())
        deck2.append(all_playable.pop())

    # Random 3 people cards
    all_people = get_people_cards()
    random.shuffle(all_people)
    peoples = []
    for i in range(3):
        peoples.append(all_people.pop())

    return create_game(deck1, deck2, peoples, all_playable)

