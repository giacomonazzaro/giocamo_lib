from __future__ import annotations
from game.game import Game, Choice
from game.agents.agent import Agent
from game.agents.minimax_search import minimax_search
import copy
import time
import random


class Agent_Minimax_Stochastic(Agent):
    def __init__(self, max_depth: int = 7, time_limit: float = 10.0, num_samples: int = 20):
        self.max_depth = max_depth
        self.time_limit = time_limit
        self.num_samples = num_samples

    def message(self, msg: str):
        pass

    def evaluate_state(self, state: Game, player_index: int) -> float:
        return 0.0


    # TODO(giacomo): This should be provided by the caller.
    def _sample_state(self, state: Game, player_index: int) -> Game:
        """Create a sampled state by shuffling hidden information.

        The agent cannot see:
        - Opponent's hand (only knows the count)
        - Opponent's deck order
        - Agent's own deck order
        """
        sampled_state = copy.deepcopy(state)

        # Shuffle opponent's hidden cards (hand + deck)
        opponent_index = 1 - player_index
        opponent = sampled_state.players[opponent_index]
        hand_size = len(opponent.hand)
        hidden_cards = opponent.hand + opponent.deck
        random.shuffle(hidden_cards)
        opponent.hand = hidden_cards[:hand_size]
        opponent.deck = hidden_cards[hand_size:]

        # Shuffle agent's own deck (hand is known, deck order is not)
        me = sampled_state.players[player_index]
        random.shuffle(me.deck)

        return sampled_state

        selected = self._search(state, choice, actions)

        print(f"choice: {choice.description}: {actions}")
        print(f"selected: {actions[selected]}")
        return selected

    def choose_action(self, state: Game, choice: Choice) -> int:
        actions = choice.actions(state)
        """Stochastic minimax with root sampling.

        Runs multiple samples to handle hidden information:
        - Opponent's hand and deck are shuffled together, then redrawn
        - Agent's own deck is shuffled

        For each sample, runs iterative deepening alpha-beta search.
        Returns the action with the highest average score across samples.
        """
        num_actions = len(actions)
        total_scores: list[float] = [0.0] * num_actions
        votes: list[int] = [0] * num_actions
        time_per_sample = self.time_limit / self.num_samples
        overall_start = time.time()
        evaluate_state = lambda state, player_index: self.evaluate_state(state, player_index)

        print(f"started: {choice.description} ({self.num_samples} samples)")
        player_index = choice.player_index

        for _ in range(self.num_samples):
            sampled_state = self._sample_state(state, player_index)  # type: ignore[arg-type]
            scores = minimax_search(
                sampled_state, evaluate_state, choice, actions,
                player_index, self.max_depth, time_per_sample,
            )
            best_action = max(range(num_actions), key=lambda a: scores[a])
            votes[best_action] += 1


            for i, score in enumerate(scores):
                total_scores[i] += score

        best_action = max(range(num_actions), key=lambda a: votes[a])
        elapsed = time.time() - overall_start
        avg_score = total_scores[best_action] / self.num_samples
        print(
            f"  result: action={actions[best_action]} "
            f"avg_score={avg_score:.2f} time={elapsed:.2f}s"
        )

        return best_action
