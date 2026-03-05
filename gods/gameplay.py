from __future__ import annotations
import random
from typing import Optional
from gods.models import Card, Card_Id, Card_Type, Game_State, card_designs, effective_power
from game.game import Choice, Choose_Card
from game.agents.minimax_stochastic import Agent_Minimax_Stochastic

def draw_card(game: Game_State, player_id: int, replacement_effects=True) -> list[Choice]:
    """Draw a card from the player's deck. Returns list of choices produced by draw effects."""
    player = game.players[player_id]
    if len(player.deck) == 0:
        return []

    if replacement_effects:
        for wid in player.wonders:
            choices = card_designs[wid].on_draw_replacement(game)
            if choices:
                return choices

    if len(player.deck) == 0:
        return []

    card_id = player.deck.pop()
    player.hand.append(card_id)

    for wid in player.wonders:
        choices = card_designs[wid].on_draw(game)
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
        player.hand.remove(card.id)
        player.discard.append(card.id)

    choices = []
    player_id = card_ids[0].owner_index
    for wonder_id in game.wonders(player_id):
        wonder = game.get_card(wonder_id)
        for card in cards:
            choices += card_designs[wonder.id].on_discard(game, card)

    return choices


def wonders_by_priority(state: Game_State) -> list[Card]:
    all_wonder_ids = state.active_player().wonders + state.opponent().wonders
    return [state.all_cards[wid] for wid in all_wonder_ids]

def play_card(state: Game_State, card_id: Card_Id) -> list[Choice]:
    """Play a card from a player's hand. Returns list of choices from the card's on_played."""
    player = state.players[card_id.owner_index]
    card = state.get_card(card_id)
    if card_id.area == "hand":
        # card_index is the stable Card.id, so remove by value rather than position.
        player.hand.remove(card_id.card_index)

    choices = card_designs[card.id].on_played(state)
    if card.card_type == Card_Type.WONDER:
        card.owner = state.current_player
        player.wonders.append(card.id)
    elif card.card_type == Card_Type.EVENT:
        player.discard.append(card.id)

    card.counters = 0

    for w in wonders_by_priority(state):
        card_designs[w.id].on_play(state, card)

    return choices

def destroy_people(game: Game_State, card_id: Card_Id) -> None:
    people = game.get_card(card_id)
    for w in wonders_by_priority(game):
        if card_designs[w.id].is_indestructible(game, people):
            return

    people.destroyed = True
    card_designs[people.id].on_destroyed(game)
    for card in wonders_by_priority(game):
        card_designs[card.id].on_destroy(game, people)

def destroy_wonder(game: Game_State, card_id: Card_Id) -> None:
    card = game.get_card(card_id)
    assert card.card_type == Card_Type.WONDER, card.card_type
    owner_idx = card_id.owner_index
    if owner_idx is not None:
        player = game.players[owner_idx]
        player.wonders.remove(card.id)
        player.discard.append(card.id)

    card_designs[card.id].on_destroyed(game)
    for w in wonders_by_priority(game):
        card_designs[w.id].on_destroy(game, card)

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
        player.wonders.remove(card.id)
        card.counters = 0
        player.deck.append(card.id)
        random.shuffle(player.deck)

def make_claim_choice(state: Game_State) -> Optional[Choice]:
    """
    At the end of the active player's turn, offer a chance to claim one people
    card from the opponent. Claiming is only allowed when the active player's
    eval_points strictly exceeds the opponent's (ties do not allow claiming).
    Returns None if there is nothing claimable.
    """
    player_index = state.current_player
    opponent_index = 1 - player_index


    def actions(state: Game_State) -> list:
        return [
            Card_Id(area="people", card_index=pid, owner_index=opponent_index)
            for pid in state.peoples
            if state.all_cards[pid].owner == opponent_index
            and card_designs[pid].can_be_claimed(state, player_index)
        ] + [Card_Id.null()]

    if(len(actions(state)) == 1):
        return None

    def resolve(state: Game_State, option_index: int):
        card_id = actions(state)[option_index]
        if not Card_Id.is_null(card_id):
            # Transfer ownership from opponent to the active player.
            people = state.get_card(card_id)
            people.owner = player_index
        return []

    return Choice(player_index=player_index, description="choose-card", text_description="Claim a people card from your opponent",
                  actions=lambda state: Choose_Card(targets=actions(state)), resolve=resolve)


def compute_player_score(game: Game_State, player_index: int) -> int:
    """Compute the total score for a player."""
    score = 0
    player = game.players[player_index]

    # Points equal to the power of each people card this player owns.
    for people_id in game.peoples:
        people = game.all_cards[people_id]
        if people.owner != player_index or people.destroyed:
            continue
        points = effective_power(game, people)
        for wid in player.wonders:
            points = card_designs[wid].on_scoring_people(game, people, points)
        score += points

    # Points from wonders (Animals, Love).
    for wid in player.wonders:
        score += card_designs[wid].on_scoring(game)

    return score


def make_main_choice(state: Game_State) -> Choice:
    player_index = state.current_player
    def actions(state: Game_State) -> list:
        # Sorted by stable card_index so the list is canonical regardless of display order.
        cards = sorted([Card_Id(area="hand", card_index=cid, owner_index=state.current_player)
                        for cid in state.players[state.current_player].hand], key=lambda c: c.card_index)
        cards.append(Card_Id.null())
        return cards

    def resolve(state: Game_State, option_index: int):
        card_id = actions(state)[option_index]
        if card_id != Card_Id.null():
            new_choices = play_card(state, card_id)
            state.current_phase = "post-play"
            return new_choices
        else:
            result: list[Choice] = []
            player = state.active_player()
            for wid in player.wonders:
                result.extend(card_designs[wid].on_pass(state))
            state.current_phase = "post-pass-effects"
            return result

    return Choice(player_index=player_index, description="main", text_description="Play a card or pass",
                  actions=lambda state: Choose_Card(targets=actions(state)), resolve=resolve)


def detailed_str(card: Card) -> str:
    design = card_designs[card.id]
    counters_str = f" (+{card.counters})" if card.counters > 0 else (f" ({card.counters})" if card.counters < 0 else "")
    return f"{design.name} [{card.color.value} {card.card_type.value}, power {card.power}{counters_str}] - {design.effect}"

def display_game_state(game: Game_State, current_player_view: bool = True) -> None:
    """Display the current game state."""
    print("\n" + "=" * 60)
    print("GAME STATE")
    print("=" * 60)

    # People cards, grouped by owning player.
    for i, player in enumerate(game.players):
        owned = [pid for pid in game.peoples if game.all_cards[pid].owner == i]
        print(f"\n--- PEOPLE ({player.name}) ---")
        for people_id in owned:
            people = game.all_cards[people_id]
            design = card_designs[people_id]
            status_str = " [DESTROYED]" if people.destroyed else ""
            print(f"  {design.name}{status_str}")
            print(f"    Effect: {design.effect}")

    # Both players' info.
    for i, player in enumerate(game.players):
        is_current = (i == game.current_player)
        marker = " <<< CURRENT TURN" if is_current else ""
        print(f"\n--- {player.name}{marker} ---")
        print(f"  Deck: {len(player.deck)} cards | Discard: {len(player.discard)} cards")

        if player.wonders:
            print(f"  Wonders in play:")
            for wid in player.wonders:
                print(f"    - {detailed_str(game.all_cards[wid])}")
        else:
            print(f"  Wonders in play: None")

        # Show hand for current player (or both in hot-seat mode).
        print(f"  Hand ({len(player.hand)} cards):")
        for card_id in player.hand:
            print(f"    - {detailed_str(game.all_cards[card_id])}")
        print("  points:", compute_player_score(game, i))
    print("\n" + "=" * 60)


class Agent_Minimax_Stochastic_Gods(Agent_Minimax_Stochastic):
    """Minimax agent with gods-specific evaluation."""

    def evaluate_state(self, state: Game_State, player_index: int) -> float:
        if not state.is_game_over():
            return self.evaluate_heuristic(state, player_index)

        my_score = compute_player_score(state, player_index)
        opp_score = compute_player_score(state, 1 - player_index)
        diff = my_score - opp_score
        if diff > 0:
            return +1000.0
        elif diff < 0:
            return -1000.0
        else:
            if player_index == state.current_player:
                return -1000.0
            else:
                return +1000.0

    def evaluate_heuristic(self, state: Game_State, player_index: int) -> float:
        """Estimate how good a non-finished position is."""
        my_score = compute_player_score(state, player_index)
        opp_score = compute_player_score(state, 1 - player_index)

        score = float(my_score - opp_score)

        my_hand = len(state.players[player_index].hand)
        opp_hand = len(state.players[1 - player_index].hand)
        score += 0.1 * (my_hand - opp_hand)

        my_wonders = len(state.players[player_index].wonders)
        opp_wonders = len(state.players[1 - player_index].wonders)
        score += 0.2 * (my_wonders - opp_wonders)

        my_deck = len(state.players[player_index].deck)
        opp_deck = len(state.players[1 - player_index].deck)
        score += 0.05 * (my_deck - opp_deck)

        return score
