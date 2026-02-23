from __future__ import annotations

import base64
import json
import random
import socket
import struct
import threading
import time
from dataclasses import dataclass, field

from gods_online.setup import STUN_PORT, STUN_SERVER, get_ip_info


def encode_room_code(ip: str, port: int) -> str:
    """Encode an IPv4 address and port into a compact URL-safe base64 string (~8 chars)."""
    ip_bytes = socket.inet_aton(ip)       # 4 bytes.
    port_bytes = struct.pack('!H', port)  # 2 bytes, big-endian.
    return base64.urlsafe_b64encode(ip_bytes + port_bytes).decode().rstrip('=')


def decode_room_code(code: str) -> tuple[str, int]:
    """Decode a room code back into (ip, port)."""
    # Restore stripped base64 padding.
    code += '=' * (-len(code) % 4)
    raw = base64.urlsafe_b64decode(code)
    ip = socket.inet_ntoa(raw[:4])
    port = struct.unpack('!H', raw[4:6])[0]
    return ip, port


@dataclass
class Connection_State:
    """Tracks the asynchronous state of a P2P connection setup.

    All fields are written by the background setup thread and read by the UI loop.
    The UI should poll ready and error each frame until one is set.
    """
    room_code: str = ""                           # Available quickly for the host.
    ready: bool = False                           # True when both sides are connected.
    player_index: int = 0
    seed: int = 0
    sock: socket.socket | None = None
    friend_addr: tuple[str, int] | None = None
    error: str = ""                               # Non-empty if setup failed.


def start_hosting(local: bool = False) -> Connection_State:
    """Begin hosting a game.

    Binds a UDP socket, discovers the public address (STUN for internet, local IP
    for LAN), encodes it as a room code, then waits for a joiner.
    All blocking work runs in a daemon thread; the caller polls Connection_State.
    """
    state = Connection_State()

    def setup() -> None:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(('0.0.0.0', 0))

            if local:
                ip = socket.gethostbyname_ex(socket.gethostname())[-1][-1]
                port = sock.getsockname()[1]
            else:
                ip, port = get_ip_info(sock)

            state.sock = sock
            state.room_code = encode_room_code(ip, port)

            # Keep the NAT port mapping alive with periodic STUN keep-alive packets
            # while the host displays the room code and waits for a joiner.
            stop_keepalive = threading.Event()
            def keep_alive() -> None:
                while not stop_keepalive.is_set():
                    try:
                        sock.sendto(b'', (STUN_SERVER, STUN_PORT))
                    except Exception:
                        pass
                    stop_keepalive.wait(timeout=10)
            if not local:
                threading.Thread(target=keep_alive, daemon=True).start()

            # Wait for the joiner's first "join" packet.
            my_seed = random.randint(0, 2**32 - 1)
            sock.settimeout(300)  # 5-minute patience before giving up.
            while True:
                data, addr = sock.recvfrom(1024)
                if data == b"PUNCH":
                    continue
                msg = json.loads(data.decode())
                if msg.get("type") == "join":
                    friend_seed = msg["seed"]
                    # Reply with our own seed so both sides can agree on player order.
                    sock.sendto(
                        json.dumps({"type": "welcome", "seed": my_seed}).encode(), addr
                    )
                    # Lower seed becomes player 0.
                    if my_seed <= friend_seed:
                        state.player_index = 0
                        state.seed = my_seed
                    else:
                        state.player_index = 1
                        state.seed = friend_seed
                    state.friend_addr = addr
                    stop_keepalive.set()
                    sock.settimeout(None)
                    state.ready = True
                    break

        except Exception as exc:
            state.error = str(exc)

    threading.Thread(target=setup, daemon=True).start()
    return state


def join_room(room_code: str, local: bool = False) -> Connection_State:
    """Join a hosted game using a room code.

    Decodes the room code, sends a join packet (with hole-punch attempts for NAT
    traversal), and waits for the host's welcome reply.
    All blocking work runs in a daemon thread; the caller polls Connection_State.
    """
    state = Connection_State()
    state.room_code = room_code.strip()

    def setup() -> None:
        try:
            ip, port = decode_room_code(state.room_code)
            friend_addr = (ip, port)

            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(('0.0.0.0', 0))
            state.sock = sock

            my_seed = random.randint(0, 2**32 - 1)

            # Send a few PUNCH packets to open a path through the joiner's NAT,
            # then send the actual join payload.
            for _ in range(3):
                sock.sendto(b"PUNCH", friend_addr)
                time.sleep(0.1)
            sock.sendto(
                json.dumps({"type": "join", "seed": my_seed}).encode(), friend_addr
            )

            # Wait for the host's welcome with its seed.
            sock.settimeout(15)
            while True:
                data, addr = sock.recvfrom(1024)
                if data == b"PUNCH":
                    continue
                msg = json.loads(data.decode())
                if msg.get("type") == "welcome":
                    host_seed = msg["seed"]
                    # Lower seed becomes player 0.
                    if my_seed <= host_seed:
                        state.player_index = 0
                        state.seed = my_seed
                    else:
                        state.player_index = 1
                        state.seed = host_seed
                    state.friend_addr = addr
                    sock.settimeout(None)
                    state.ready = True
                    break

        except Exception as exc:
            state.error = str(exc)

    threading.Thread(target=setup, daemon=True).start()
    return state
