from gods.models import Game_State
from game.agents.agent import Choice
from game.agents.agent import Agent

class Agent_Duel(Agent):
    def __init__(self, agent_0, agent_1, swap: bool):
        if swap:
            self.agents = [agent_1, agent_0]
        else:
            self.agents = [agent_0, agent_1]

    def message(self, msg: str):
        print("Duel:", msg)

    def choose_action(self, state: Game_State, choice: Choice, actions: list):
        return self.agents[choice.player_index].choose_action(state, choice, actions)
