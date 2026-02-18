from __future__ import annotations
from dataclasses import dataclass, field
from gods.models import Game_State
from game.agents.agent import Choice, game_loop
from game.agents.randomized import Agent_Random
from gods.gameplay import compute_player_score
import copy
import math
import random
import time


@dataclass
class MCTS_Node:
    parent: int = -1  # index into tree, -1 means no parent (root)
    action_index: int = -1  # action that led to this node
    children: list[int] = field(default_factory=list)  # indices into tree
    visits: int = 0
    wins: float = 0.0
    untried_actions: list[int] = field(default_factory=list)


class Agent_MCTS:
    def __init__(self, exploration: float = 1.41, time_limit: float = 10.0):
        self.exploration = exploration
        self.time_limit = time_limit  # max seconds for search
        self.random_agent = Agent_Random()
        self.player_index = None  # set during choose_action
        self.tree: list[MCTS_Node] = []  # nodes stored by index

    def message(self, msg: str):
        pass  # silent agent

    def create_node(self, parent: int = -1, action_index: int = -1) -> int:
        node_index = len(self.tree)
        self.tree.append(MCTS_Node(parent=parent, action_index=action_index))
        return node_index

    def ucb1(self, node_index: int) -> float:
        node = self.tree[node_index]
        if node.visits == 0:
            return float('inf')
        parent = self.tree[node.parent]
        exploitation = node.wins / node.visits
        exploration_term = self.exploration * math.sqrt(math.log(parent.visits) / node.visits)
        return exploitation + exploration_term

    def best_child(self, node_index: int) -> int:
        node = self.tree[node_index]
        return max(node.children, key=lambda c: self.ucb1(c))

    def best_action(self, node_index: int) -> int:
        node = self.tree[node_index]
        best_child_index = max(node.children, key=lambda c: self.tree[c].visits)
        return self.tree[best_child_index].action_index

    def choose_action(self, state: Game_State, choice: Choice, actions: list) -> int:
        self.player_index = choice.player_index
        selected = self.mcts_search(state, choice, actions)
        return selected

    def mcts_search(self, state: Game_State, choice: Choice, actions: list) -> int:
        self.tree = []
        root = self.create_node()
        self.tree[root].untried_actions = list(range(len(actions)))

        print("started:", choice.description)
        start_time = time.time()
        iteration = 0
        while (time.time() - start_time) < self.time_limit:
            iteration += 1
            # clone state for simulation
            sim_state = copy.deepcopy(state)
            sim_choice = copy.deepcopy(choice)
            sim_state.choices = []
            node_index = root

            # selection: traverse tree using UCB1
            node = self.tree[node_index]
            while node.untried_actions == [] and node.children:
                node_index = self.best_child(node_index)
                node = self.tree[node_index]
                new_choices = sim_choice.resolve(sim_state, node.action_index) or []
                sim_state.choices.extend(new_choices)
                sim_choice = sim_state.next_choice()
                if sim_choice is None:
                    break

            # expansion: add a new child node
            if node.untried_actions and sim_choice is not None:
                action = random.choice(node.untried_actions)
                node.untried_actions.remove(action)
                child_index = self.create_node(parent=node_index, action_index=action)
                node.children.append(child_index)
                node_index = child_index
                node = self.tree[node_index]
                new_choices = sim_choice.resolve(sim_state, action) or []
                sim_state.choices.extend(new_choices)
                # initialize child's untried actions for the next choice
                sim_choice = sim_state.next_choice()
                if sim_choice is not None:
                    sim_actions = sim_choice.actions(sim_state)
                    node.untried_actions = list(range(len(sim_actions)))

            # simulation: play randomly until game ends
            result = self.simulate(sim_state)

            # backpropagation: update statistics
            while node_index != -1:
                node = self.tree[node_index]
                node.visits += 1
                node.wins += result
                node_index = node.parent

        print(choice.description, "iterations:", iteration)
        root_node = self.tree[root]
        for child_index in root_node.children:
            child = self.tree[child_index]
            print("child:", actions[child.action_index], child.visits, child.wins/child.visits)

        return self.best_action(root)

    def simulate(self, state: Game_State) -> float:
        # play random moves until game ends using the actual game loop
        game_loop(state, self.random_agent)
        return self.evaluate(state)

    def evaluate(self, state: Game_State) -> float:
        # evaluate the final state from perspective of self.player_index
        # returns value in [-1, 1] range for proper UCB1 exploration
        my_score = compute_player_score(state, self.player_index)
        opp_score = compute_player_score(state, 1 - self.player_index)
        diff = my_score - opp_score
        if diff == 0:
            # tiebreaker: ending player loses
            if state.ending_player == self.player_index:
                return -1.0
            else:
                return 1.0
        elif diff > 0:
            return 1.0
        else:
            return -1.0
