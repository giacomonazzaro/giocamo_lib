from __future__ import annotations
import random
from typing import Optional
from gods.models import Card, Card_Id, Card_Type, Game_State
from game.game import Choice

def draw_card(game: Game_State, player_id: int, replacement_effects=True) -> list[Choice]:
    """Draw a card from the player's deck. Returns list of choices produced by draw effects."""
    player = game.players[player_id]
    if len(player.deck) == 0:
        return []

    if replacement_effects:
        for w in player.wonders:
            choices = w.on_draw_replacement(game)
            if choices:
                return choices

    if len(player.deck) == 0:
        return []

    card = player.deck.pop()
    player.hand.append(card)

    for w in player.wonders:
        choices = w.on_draw(game)
        if choices:
            return choices

    return []


def discard_cards(game: Game_State, card_ids: list[Card_Id]) -> list[Choice]:
    """Discard cards from players' hands."""
    if not card_ids:
        return
    assert all([card_id.area == "hand" for card_id in card_ids])
    assert all([card_id.owner_index == card_ids[0].owner_index for card_id in card_ids])

    cards = [game.get_card(card_id) for card_id in card_ids]
    for i, card in enumerate(cards):
        player = game.players[card_ids[i].owner_index]
        player.hand.remove(card)
        player.discard.append(card)

    choices = []
    player_id = card_ids[0].owner_index
    for wonder_id in game.wonders(player_id):
        wonder = game.get_card(wonder_id)
        for card in cards:
            choices += wonder.on_discard(game, card)

    return choices


def check_people_conditions(game: Game_State) -> None:
    """Check and update ownership of people cards based on their conditions."""
    for people in game.peoples:
        old_owner = people.owner
        new_owner = evaluate_people_condition(game, people)

        if new_owner != old_owner:
            people.owner = new_owner


def evaluate_people_condition(game: Game_State, people: Card) -> Optional[int]:
    """
    Evaluate who should control a people card.
    Returns player index (0 or 1) or None if tied/no one qualifies.
    """
    scores = [
        people.eval_points(game, 0),
        people.eval_points(game, 1)
    ]

    if scores[0] > scores[1]:
        return 0
    elif scores[1] > scores[0]:
        return 1
    else:
        # Tie or no one qualifies - check for wonders that break ties
        if scores[0] == scores[1]:
            for i, player in enumerate(game.players):
                for w in player.wonders:
                    if w.wins_tie(game, people):
                        return i
            return people.owner
        return None


def play_card(state: Game_State, card_id: Card_Id) -> list[Choice]:
    """Play a card from a player's hand. Returns list of choices from the card's on_played."""
    player = state.players[card_id.owner_index]
    card = state.get_card(card_id)
    if card_id.area == "hand":
        del player.hand[card_id.card_index]

    if card.card_type == Card_Type.WONDER:
        card.owner = state.current_player
        player.wonders.append(card)
    elif card.card_type == Card_Type.EVENT:
        player.discard.append(card)
    return card.on_played(state)


def wonders_by_priority(state: Game_State) -> list[Card]:
    active_player = state.active_player()
    opponent = state.opponent()
    all_wonders = active_player.wonders + opponent.wonders
    return all_wonders

def destroy_people(game: Game_State, card_id: Card_Id) -> None:
    people = game.get_card(card_id)
    people.destroyed = True
    people.on_destroyed(game)
    for card in wonders_by_priority(game):
        card.on_destroy(game, people)

def destroy_wonder(game: Game_State, card_id: Card_Id) -> None:
    card = game.get_card(card_id)
    assert card.card_type == Card_Type.WONDER, card.card_type
    owner_idx = card_id.owner_index
    if owner_idx is not None:
        player = game.players[owner_idx]
        player.wonders.remove(card)
        player.discard.append(card)

    card.on_destroyed(game)
    for w in wonders_by_priority(game):
        w.on_destroy(game, card)

def restore_people(game: Game_State, card_id: Card_Id) -> None:
    people = game.get_card(card_id)
    people.destroyed = False

def shuffle_card_into_deck(game: Game_State, card_id: Card_Id) -> None:
    card = game.get_card(card_id)
    assert card.card_type == Card_Type.WONDER, card.card_type
    assert card_id.area in ["wonders", "discard"], f"{card_id.area}"
    assert card_id.owner_index is not None
    owner_idx = card_id.owner_index
    if owner_idx is not None:
        player = game.players[owner_idx]
        player.wonders.remove(card)
        card.counters = 0
        player.deck.append(card)
        random.shuffle(player.deck)

def declare_end_game(game: Game_State) -> None:
    """Active player declares end of game."""
    game.game_ending = True
    game.ending_player = game.current_player


def compute_player_score(game: Game_State, player_index: int) -> int:
    """Compute the total score for a player."""
    score = 0
    player = game.players[player_index]

    # Points from peoples where this player meets the condition
    for people in game.peoples:
        points = people.eval_points(game, player_index)
        if people.destroyed:
            points = 0
        for wonder in player.wonders:
            points = wonder.on_scoring_people(game, people, points)
        score += points

    # Points from wonders (Animals, Love)
    for wonder in player.wonders:
        score += wonder.on_scoring(game)

    return score


def make_play_choice(state: Game_State) -> Choice:
    player_index = state.current_player
    def actions(state: Game_State) -> list:
        return [Card_Id(area="hand", card_index=i, owner_index=state.current_player)
                for i, card in enumerate(state.players[state.current_player].hand)]
    def resolve(state: Game_State, option_index: int) -> list[Choice]:
        card_id = actions(state)[option_index]
        new_choices = play_card(state, card_id)
        state.current_phase = "post-play"
        return new_choices
    return Choice(player_index=player_index, description="choose-card",
                  actions=actions, resolve=resolve)

def make_main_choice(state: Game_State) -> Choice:
    player_index = state.current_player
    def actions(state: Game_State) -> list:
        player = state.active_player()
        options = []
        if player.hand:
            options.append("play")
        options.append("pass")
        return options
    def resolve(state: Game_State, option_index: int) -> list[Choice]:
        action = actions(state)[option_index]
        if action == "play":
            return [make_play_choice(state)]
        elif action == "pass":
            result: list[Choice] = []
            player = state.active_player()
            for w in player.wonders:
                result.extend(w.on_pass(state))
            state.current_phase = "post-pass-effects"
            return result
        return []
    return Choice(player_index=player_index, description="main",
                  actions=actions, resolve=resolve)


def detailed_str(card: Card) -> str:
    counters_str = f" (+{card.counters})" if card.counters > 0 else (f" ({card.counters})" if card.counters < 0 else "")
    return f"{card.name} [{card.color.value} {card.card_type.value}, power {card.power}{counters_str}] - {card.effect}"

def display_game_state(game: Game_State, current_player_view: bool = True) -> None:
    """Display the current game state."""
    print("\n" + "=" * 60)
    print("GAME STATE")
    print("=" * 60)

    # People cards
    print("\n--- PEOPLE CARDS (Center) ---")
    for people in game.peoples:
        owner_str = f" - Controlled by {game.players[people.owner].name}" if people.owner is not None else " - Unclaimed"
        status_str = " [DESTROYED]" if people.destroyed else ""
        effect_text = people.effect
        print(f"  {people}{status_str}{owner_str}")
        print(f"    Effect: {effect_text}")

    # Both players' info
    for i, player in enumerate(game.players):
        is_current = (i == game.current_player)
        marker = " <<< CURRENT TURN" if is_current else ""
        print(f"\n--- {player.name}{marker} ---")
        print(f"  Deck: {len(player.deck)} cards | Discard: {len(player.discard)} cards")

        if player.wonders:
            print(f"  Wonders in play:")
            for w in player.wonders:
                print(f"    - {detailed_str(w)}")
        else:
            print(f"  Wonders in play: None")

        # Show hand for current player (or both in hot-seat mode)
        print(f"  Hand ({len(player.hand)} cards):")
        for card in player.hand:
            print(f"    - {detailed_str(card)}")
        print("  points:", compute_player_score(game, i))
    print("\n" + "=" * 60)
