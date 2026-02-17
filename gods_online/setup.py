import json
import random
import struct
import time
import socket

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

def setup_online_game(sock: socket.socket, local: bool) -> tuple[int, int, socket.socket, tuple[str, int]]:
    """Send or receive init"""

    # Keep-Alive Loop (Background)
    # Because manual copy-pasting takes time, the router might close the port.
    # We send a tiny packet to the STUN server every 20s to keep the port open
    # while you are typing.
    stop_keepalive = False
    def keep_alive():
        while not stop_keepalive:
            sock.sendto(b'', (STUN_SERVER, STUN_PORT))
            time.sleep(10)
    if not local:
        threading.Thread(target=keep_alive, daemon=True).start()

    friend_ip = typer.prompt("What is your friend's public IP address")
    friend_port = typer.prompt("What is your friend's public port", type=int)

    # Stop the keep-alive to STUN server
    stop_keepalive = True
    friend_addr = (friend_ip, friend_port)

    seed = random.randint(0, 2**32 - 1)
    game_init: dict[str, int] = {}
    # Start Listener
    listener = threading.Thread(target=pick_seed, args=(sock, seed, game_init), daemon=True)
    listener.start()

    if not local:
        # Hole Punching & Chat Loop
        # Send a few punch packets to force the router to open the path
        for _ in range(5):
            typer.echo(f"[*] Sending hole punch packet to {friend_addr}...")
            sock.sendto(b"PUNCH", friend_addr)
            time.sleep(0.5)

    # send seed to the other player
    sock.sendto(json.dumps({"type": "init", "seed": seed}).encode(), friend_addr)
    typer.echo("joining")
    listener.join()
    print(f"You are Player {(player_index := game_init['player_index']) + 1}. Seed: {(seed := game_init['seed'])}")
    return player_index, seed, sock, friend_addr


def peer_to_peer(local: bool = False) -> socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    if local:
        typer.echo("You're playing in local mode.")
        your_ip = socket.gethostbyname_ex(socket.gethostname())[-1][-1]
        your_port = typer.prompt("Enter the port to use for the game", type=int)
        sock.bind(('0.0.0.0', your_port))
    else:
        sock.bind(('0.0.0.0', 0))
        your_ip, your_port = get_ip_info(sock)

    if your_ip is None or your_port is None:
        typer.echo("Could not discover your public IP and port using STUN. Please check your network configuration and try again.")
        raise typer.Exit(1)
    typer.echo("=" * 60)
    typer.echo(f"Your public IP is: {your_ip}\nYour external port is: {your_port}\nShare this with your friend to connect directly, or use it to set up port forwarding on your router if needed.")
    typer.echo("=" * 60)
    return sock