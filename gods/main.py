from __future__ import annotations
import random
import copy

from gods.agents.mcts import Agent_MCTS
from gods.models import Card, Card_Type
from gods.agents.terminal import Agent_Terminal
from gods.agents.randomized import Agent_Random
from gods.agents.duel import Agent_Duel

# from gods.cards import get_playable_cards, get_people_cards
from gods.agents.agent import game_loop
from gods.game import (
    check_people_conditions,
    display_game_state, compute_player_score, detailed_str
)
from gods.setup import *


def select_deck(available_cards: list[Card], player_name: str) -> list[Card]:
    """Let a player select 10 cards for their deck."""
    print(f"\n{player_name}, select 10 cards for your deck:")

    # Group cards by type
    wonders = [c for c in available_cards if c.card_type == Card_Type.WONDER]
    events = [c for c in available_cards if c.card_type == Card_Type.EVENT]

    print("\nAvailable Wonders:")
    for i, card in enumerate(wonders):
        print(f"  W{i + 1}. {detailed_str(card)}")

    print("\nAvailable Events:")
    for i, card in enumerate(events):
        print(f"  E{i + 1}. {detailed_str(card)}")

    deck = []
    while len(deck) < 10:
        remaining = 10 - len(deck)
        print(f"\nSelected: {len(deck)}/10. Select {remaining} more cards.")
        print("Enter 'W#' for wonder or 'E#' for event (e.g., W1, E3)")
        print("Enter 'auto' to randomly fill remaining slots")

        choice = input("Enter choice: ").strip().upper()

        if choice == 'AUTO':
            remaining_cards = [c for c in available_cards if c not in deck]
            random.shuffle(remaining_cards)
            deck.extend(remaining_cards[:remaining])
            break

        try:
            if choice.startswith('W'):
                idx = int(choice[1:]) - 1
                if 0 <= idx < len(wonders):
                    card = wonders[idx]
                    if card not in deck:
                        deck.append(copy.deepcopy(card))
                        print(f"Added {card.name} to deck.")
                    else:
                        print("Card already in deck!")
                else:
                    print("Invalid wonder number.")
            elif choice.startswith('E'):
                idx = int(choice[1:]) - 1
                if 0 <= idx < len(events):
                    card = events[idx]
                    if card not in deck:
                        deck.append(copy.deepcopy(card))
                        print(f"Added {card.name} to deck.")
                    else:
                        print("Card already in deck!")
                else:
                    print("Invalid event number.")
            else:
                print("Invalid format. Use W# or E#.")
        except (ValueError, IndexError):
            print("Invalid input.")

    return deck


def select_people_cards(all_people: list[Card]) -> list[Card]:
    """Select 3 people cards for the center."""
    print("\nSelect 3 people cards for the center:")
    for i, card in enumerate(all_people):
        print(f"  {i + 1}. {detailed_str(card)}")
    print("Enter 'auto' for random selection")

    while True:
        choice = input("Enter 3 numbers (comma-separated) or 'auto': ").strip()

        if choice.lower() == 'auto':
            selected = random.sample(all_people, 3)
            print(f"Selected: {', '.join(p.name for p in selected)}")
            return [copy.deepcopy(p) for p in selected]

        try:
            indices = [int(x.strip()) - 1 for x in choice.split(',')]
            if len(indices) == 3 and all(0 <= i < len(all_people) for i in indices) and len(set(indices)) == 3:
                selected = [all_people[i] for i in indices]
                return [copy.deepcopy(p) for p in selected]
        except ValueError:
            pass
        print("Please enter exactly 3 different numbers.")





def main():
    print("=" * 60)
    print("        GODS - A Card Game")
    print("=" * 60)

    print("\nSetup options:")
    print("  1. Quick game (random decks)")
    print("  2. Draft decks")

    
    game = quick_setup()

    print("\nGame started! Each player has drawn 5 cards.")

    # Initial people condition check
    check_people_conditions(game)

    agent = Agent_Duel(Agent_Terminal(), Agent_MCTS())
    game_loop(game, agent, display_game_state)

    # Game over - calculate scores
    print("\n" + "=" * 60)
    print("GAME OVER!")
    print("=" * 60)

    display_game_state(game)

    print("\n--- FINAL SCORING ---")
    score0 = compute_player_score(game, 0)
    score1 = compute_player_score(game, 1)

    print(f"\nFinal Scores:")
    print(f"  Player 1: {score0} points")
    print(f"  Player 2: {score1} points")   

    # winner = determine_winner(game, (score1, score2), ui)
    # if winner is not None:
        # print(f"\n{game.players[winner].name} WINS!")
    # else:
        # print("\nIt's a tie!")


if __name__ == "__main__":
    main()
