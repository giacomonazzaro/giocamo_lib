from __future__ import annotations
import socket

from game.game import Game, Choice
from game.agents.agent import Agent
from gods_online.protocol import send_message, recv_message


class Agent_Remote(Agent):
    """Receives opponent's action indices from the server."""
    def __init__(self, sock: socket.socket):
        self.sock = sock

    def choose_action(self, state: Game, choice: Choice) -> int:
        msg = recv_message(self.sock)
        return msg["index"]


class Agent_Local_Online(Agent):
    """Wraps a local agent and sends chosen action indices to the server."""
    def __init__(self, local_agent: Agent, sock: socket.socket, friend_addr: tuple[str, int]):
        self.local_agent = local_agent
        self.sock = sock
        self.friend_addr = friend_addr

    def choose_action(self, state: Game, choice: Choice) -> int:
        index = self.local_agent.choose_action(state, choice)
        send_message(self.sock, {"type": "action", "index": index}, self.friend_addr)
        return index
