"""Initial game state construction for Tressette."""

from __future__ import annotations

import random

from tressette.game.models import Card, Game_State, Player, Suit


def quick_setup(seed: int | None = None) -> Game_State:
    """Deal a single hand: 10 cards each, 20 in stock, player 0 leads."""
    if seed is not None:
        random.seed(seed)

    # Card id encoding: id // 10 -> suit (0..3), id % 10 -> rank-1.
    suits = [Suit.COPPE, Suit.DENARI, Suit.SPADE, Suit.BASTONI]
    all_cards = [
        Card(id=i, rank=(i % 10) + 1, suit=suits[i // 10])
        for i in range(40)
    ]

    deck_ids = list(range(40))
    random.shuffle(deck_ids)

    p0_hand = deck_ids[:10]
    p1_hand = deck_ids[10:20]
    stock   = deck_ids[20:]

    game = Game_State()
    game.all_cards = all_cards
    game.players = [
        Player(name="Player 1", hand=p0_hand),
        Player(name="Player 2", hand=p1_hand),
    ]
    game.stock = stock
    game.current_player = 0
    game.trick_leader = 0
    return game
