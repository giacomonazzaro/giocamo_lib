from __future__ import annotations

import threading
from dataclasses import dataclass

from gods_online.setup import (
    fetch_address,
    generate_room_code,
    peer_to_peer,
    pick_seed,
    publish_address,
    setup_online_game,
)
import random
import json
import time


@dataclass
class Connection_State:
    """Tracks the asynchronous state of a P2P connection setup.

    Written by the background setup thread, polled by the UI render loop each frame.
    """
    room_code: str = ""
    ready: bool = False
    player_index: int = 0
    seed: int = 0
    sock = None
    friend_addr = None
    error: str = ""


def start_hosting(local: bool = False) -> Connection_State:
    """Host a game: discover address, publish room code, wait for joiner.

    Sets state.room_code as soon as the code is published (fast), then blocks
    waiting for the joiner's address so the UI can display the code early.
    """
    state = Connection_State()

    def setup() -> None:
        try:
            sock, your_ip, your_port = peer_to_peer(local)
            state.sock = sock

            # Generate code and publish our address — fast HTTP, done before blocking.
            state.room_code = generate_room_code()
            publish_address(state.room_code, your_ip, your_port)

            # Block until the joiner publishes their address.
            friend_ip, friend_port = fetch_address(state.room_code, suffix="-join")
            if friend_ip is None:
                state.error = "Timed out waiting for a joiner."
                return
            friend_addr = (friend_ip, friend_port)

            # Seed exchange — same logic as setup_online_game.
            seed = random.randint(0, 2**32 - 1)
            game_init: dict = {}
            listener = threading.Thread(
                target=pick_seed, args=(sock, seed, game_init), daemon=True
            )
            listener.start()
            if not local:
                for _ in range(5):
                    sock.sendto(b"PUNCH", friend_addr)
                    time.sleep(0.5)
            sock.sendto(json.dumps({"type": "init", "seed": seed}).encode(), friend_addr)
            listener.join()

            state.player_index = game_init["player_index"]
            state.seed = game_init["seed"]
            state.friend_addr = friend_addr
            state.ready = True

        except Exception as exc:
            state.error = str(exc)

    threading.Thread(target=setup, daemon=True).start()
    return state


def join_room(room_code: str, local: bool = False) -> Connection_State:
    """Join a game by room code. Delegates entirely to setup_online_game."""
    state = Connection_State()
    state.room_code = room_code.strip()

    def setup() -> None:
        try:
            sock, your_ip, your_port = peer_to_peer(local)
            player_index, seed, sock, friend_addr = setup_online_game(
                sock, local, your_ip, your_port, room_code=state.room_code
            )
            state.sock = sock
            state.player_index = player_index
            state.seed = seed
            state.friend_addr = friend_addr
            state.ready = True
        except Exception as exc:
            state.error = str(exc)

    threading.Thread(target=setup, daemon=True).start()
    return state
