from __future__ import annotations

import json
import random
import string
import struct
import time
import socket
import os
import urllib.request
import typer
import threading
from dataclasses import dataclass

STUN_SERVER = os.getenv('STUN_SERVER', 'stun.l.google.com')
STUN_PORT = int(os.getenv('STUN_PORT', '19302'))
NTFY_URL = "https://ntfy.sh"


def generate_room_code(length=4):
    chars = string.ascii_lowercase + string.digits
    return ''.join(random.choice(chars) for _ in range(length))


def get_local_ip():
    """Get the LAN IP address of this machine."""
    try:
        # Connect to a public address to determine which local interface is used.
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return socket.gethostbyname_ex(socket.gethostname())[-1][-1]


def publish_address(room_code, ip, port, local_ip, local_port, suffix=""):
    topic = f"gods-{room_code}{suffix}"
    url = f"{NTFY_URL}/{topic}"
    data = json.dumps({
        "ip": ip, "port": port,
        "local_ip": local_ip, "local_port": local_port,
    }).encode()
    req = urllib.request.Request(url, data=data)
    urllib.request.urlopen(req, timeout=10)


def fetch_address(room_code, suffix="", timeout=120):
    """Returns (public_ip, public_port, local_ip, local_port)."""
    topic = f"gods-{room_code}{suffix}"
    url = f"{NTFY_URL}/{topic}/json?poll=1&since=all"
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            response = urllib.request.urlopen(url, timeout=10)
            for line in response.read().decode().strip().split('\n'):
                if not line:
                    continue
                msg = json.loads(line)
                if msg.get("event") == "message":
                    payload = json.loads(msg["message"])
                    return (
                        payload["ip"], payload["port"],
                        payload.get("local_ip"), payload.get("local_port"),
                    )
        except Exception:
            pass
        time.sleep(2)
    return None, None, None, None


def get_ip_info(sock):
    """
    Sends a raw STUN Binding Request to Google to find out our Public IP/Port.
    We use the SAME socket that we will later use for chatting.
    """
    sock.settimeout(2)
    try:
        print(struct)
        # STUN Binding Request (Header: 0x0001, Length: 0)
        # We don't need a full STUN library for a simple binding request
        data = struct.pack('!H', 0x0001) + struct.pack('!H', 0) + b'\x00'*16

        print(f"[*] Querying STUN server ({STUN_SERVER})...")
        sock.sendto(data, (STUN_SERVER, STUN_PORT))

        response, _ = sock.recvfrom(2048)

        # Parse STUN Response (This is a simplified parser for IPv4)
        # Skip header (20 bytes) -> look for MAPPED-ADDRESS attribute (0x0001)
        offset = 20
        while offset < len(response):
            attr_type, attr_len = struct.unpack('!HH', response[offset:offset+4])
            if attr_type == 0x0001: # MAPPED-ADDRESS
                # Skip family(1) and port(2)
                port = struct.unpack('!H', response[offset+6:offset+8])[0]
                ip_octets = struct.unpack('!BBBB', response[offset+8:offset+12])
                ip = ".".join(map(str, ip_octets))
                return ip, port
            offset += 4 + attr_len

    except Exception as e:
        raise
        print(f"[!] STUN failed: {e}")
        return None, None
    finally:
        sock.settimeout(None) # Remove timeout for chat mode


def pick_seed(sock: socket.socket, seed: int, game_init: dict):
    typer.echo("[*] Waiting to receive seed from friend...")
    while True:
        try:
            data, _ = sock.recvfrom(1024)
            msg = data.decode().strip()
            # If we receive "PUNCH", just print a notification, don't clutter chat
            if msg == "PUNCH":
                typer.echo("[*] Received hole punch packet from friend, router should have opened the path.")
                continue
            elif "init" in json.loads(msg)["type"]:
                typer.echo("[*] Received seed from friend, determining player order...")
                # player with lower seed is player 0
                if seed < (friend_seed := int(json.loads(msg)["seed"])):
                    game_init.update({"seed": seed, "player_index": 0})
                    break
                else:
                    game_init.update({"seed": friend_seed, "player_index": 1})
                    break
        except Exception as e:
            typer.echo(f"[!] Error receiving seed: {e}")
            break


def _exchange_seeds(sock: socket.socket, local: bool, friend_addr: tuple[str, int]) -> tuple[int, int]:
    """Hole-punch to friend and exchange seeds to agree on player order and game seed.

    Returns (player_index, seed).
    """
    seed = random.randint(0, 2**32 - 1)
    game_init: dict[str, int] = {}
    listener = threading.Thread(target=pick_seed, args=(sock, seed, game_init), daemon=True)
    listener.start()

    if not local:
        for _ in range(5):
            typer.echo(f"[*] Sending hole punch packet to {friend_addr}...")
            sock.sendto(b"PUNCH", friend_addr)
            time.sleep(0.5)

    sock.sendto(json.dumps({"type": "init", "seed": seed}).encode(), friend_addr)
    typer.echo("[*] Exchanging seeds...")
    listener.join()
    player_index = game_init['player_index']
    seed = game_init['seed']
    print(f"You are Player {player_index + 1}. Seed: {seed}")
    return player_index, seed


def setup_online_game(sock: socket.socket, local: bool, your_ip: str, your_port: int, local_ip: str, local_port: int, room_code: str | None = None) -> tuple[int, int, socket.socket, tuple[str, int]]:
    """Set up connection with friend and exchange seeds."""

    # Keep-alive to prevent NAT port mapping from expiring.
    stop_keepalive = False
    def keep_alive():
        while not stop_keepalive:
            sock.sendto(b'', (STUN_SERVER, STUN_PORT))
            time.sleep(10)
    if not local:
        threading.Thread(target=keep_alive, daemon=True).start()

    same_network = False
    if local:
        # Local mode: manual IP/port exchange.
        friend_ip = typer.prompt("What is your friend's IP address")
        friend_port = typer.prompt("What is your friend's port", type=int)
    elif room_code is None:
        # Hosting: publish our address, wait for joiner.
        room_code = generate_room_code()
        publish_address(room_code, your_ip, your_port, local_ip, local_port)
        typer.echo(f"[*] Room code: {room_code}")
        typer.echo("[*] Waiting for friend to join...")
        friend_ip, friend_port, friend_local_ip, friend_local_port = fetch_address(room_code, suffix="-join")
        if friend_ip is None:
            typer.echo("[!] Timed out waiting for friend to join.")
            raise typer.Exit(1)
        # If both peers share the same public IP, use LAN addresses.
        if friend_ip == your_ip and friend_local_ip:
            typer.echo("[*] Same network detected, using LAN addresses.")
            friend_ip = friend_local_ip
            friend_port = friend_local_port
            same_network = True
        typer.echo("[*] Friend joined!")
    else:
        # Joining: fetch host's address, publish ours.
        typer.echo(f"[*] Joining room {room_code}...")
        friend_ip, friend_port, friend_local_ip, friend_local_port = fetch_address(room_code)
        if friend_ip is None:
            typer.echo("[!] Could not find room. Check the code and try again.")
            raise typer.Exit(1)
        publish_address(room_code, your_ip, your_port, local_ip, local_port, suffix="-join")
        # If both peers share the same public IP, use LAN addresses.
        if friend_ip == your_ip and friend_local_ip:
            typer.echo("[*] Same network detected, using LAN addresses.")
            friend_ip = friend_local_ip
            friend_port = friend_local_port
            same_network = True
        typer.echo("[*] Connected to host!")

    stop_keepalive = True
    friend_addr = (friend_ip, friend_port)

    # Skip hole punching on same network since LAN traffic doesn't need it.
    player_index, seed = _exchange_seeds(sock, local or same_network, friend_addr)
    return player_index, seed, sock, friend_addr


def peer_to_peer(local: bool = False) -> tuple[socket.socket, str, int, str, int]:
    """Returns (sock, public_ip, public_port, local_ip, local_port)."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    if local:
        typer.echo("You're playing in local mode.")
        local_ip = get_local_ip()
        local_port = typer.prompt("Enter the port to use for the game", type=int)
        sock.bind(('0.0.0.0', local_port))
        return sock, local_ip, local_port, local_ip, local_port
    else:
        sock.bind(('0.0.0.0', 0))
        local_ip = get_local_ip()
        local_port = sock.getsockname()[1]
        public_ip, public_port = get_ip_info(sock)

    if public_ip is None or public_port is None:
        typer.echo("Could not discover your public IP and port using STUN.")
        raise typer.Exit(1)

    typer.echo(f"[*] Public address: {public_ip}:{public_port}")
    typer.echo(f"[*] Local address: {local_ip}:{local_port}")
    return sock, public_ip, public_port, local_ip, local_port


# --- Async wrappers for the graphical menu ---

@dataclass
class Connection_State:
    """Tracks the async state of a P2P connection setup, polled by the UI render loop each frame."""
    room_code: str = ""
    ready: bool = False
    player_index: int = 0
    seed: int = 0
    sock: socket.socket | None = None
    friend_addr: tuple[str, int] | None = None
    error: str = ""


def start_hosting(local: bool = False) -> Connection_State:
    """Host a game. Sets state.room_code as soon as it's published (fast),
    then waits for the joiner in the background. The UI can display the
    room code immediately while the connection is pending.
    """
    state = Connection_State()

    def setup() -> None:
        try:
            sock, your_ip, your_port, local_ip, local_port = peer_to_peer(local)
            state.sock = sock

            # Publish before blocking so the UI has the code to display.
            state.room_code = generate_room_code()
            publish_address(state.room_code, your_ip, your_port, local_ip, local_port)

            friend_ip, friend_port, friend_local_ip, friend_local_port = fetch_address(state.room_code, suffix="-join")
            if friend_ip is None:
                state.error = "Timed out waiting for a joiner."
                return

            # If both peers share the same public IP, use LAN addresses.
            same_network = False
            if friend_ip == your_ip and friend_local_ip:
                typer.echo("[*] Same network detected, using LAN addresses.")
                friend_ip = friend_local_ip
                friend_port = friend_local_port
                same_network = True

            player_index, seed = _exchange_seeds(sock, local or same_network, (friend_ip, friend_port))
            state.player_index = player_index
            state.seed = seed
            state.friend_addr = (friend_ip, friend_port)
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
            sock, your_ip, your_port, local_ip, local_port = peer_to_peer(local)
            player_index, seed, sock, friend_addr = setup_online_game(
                sock, local, your_ip, your_port, local_ip, local_port, room_code=state.room_code
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
