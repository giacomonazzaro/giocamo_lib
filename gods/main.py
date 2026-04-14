from __future__ import annotations

from gods.gameplay import Agent_Minimax_Stochastic_Gods
from gods.agent_terminal import Agent_Terminal
from game.agents.duel import Agent_Duel

from game.game import game_loop
from gods.gameplay import (
    display_game_state, compute_player_score
)
from gods.setup import quick_setup


def main():
    print("=" * 60)
    print("        GODS - A Card Game")
    print("=" * 60)

    game = quick_setup(seed=None)

    print("\nGame started! Each player has drawn 5 cards.")

    agent = Agent_Duel(Agent_Terminal(), Agent_Minimax_Stochastic_Gods())
    game_loop(game, agent, display_game_state)

    # Game over - calculate scores.
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


if __name__ == "__main__":
    main()
