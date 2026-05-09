from __future__ import annotations
import socket
import random
import threading
import sys

from online.protocol import send_message, recv_message

DEFAULT_PORT = 9999


def relay(src, dst):
    """Forward messages from src to dst until disconnection."""
    try:
        while True:
            msg = recv_message(src)
            send_message(dst, msg)
    except ConnectionError:
        pass


def run_server(host: str = "0.0.0.0", port: int = DEFAULT_PORT):
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind((host, port))
    server_sock.listen(2)
    print(f"Server listening on {host}:{port}")

    print("Waiting for Player 1...")
    conn0, addr0 = server_sock.accept()
    print(f"Player 1 connected from {addr0}")

    print("Waiting for Player 2...")
    conn1, addr1 = server_sock.accept()
    print(f"Player 2 connected from {addr1}")

    # Generate and send seed so both clients create identical game states
    seed = random.randint(0, 2**32 - 1)
    send_message(conn0, {"type": "init", "seed": seed, "player_index": 0})
    send_message(conn1, {"type": "init", "seed": seed, "player_index": 1})
    print(f"Game started with seed {seed}")

    # Relay action messages between clients
    t0 = threading.Thread(target=relay, args=(conn0, conn1), daemon=True)
    t1 = threading.Thread(target=relay, args=(conn1, conn0), daemon=True)
    t0.start()
    t1.start()

    t0.join()
    t1.join()

    conn0.close()
    conn1.close()
    server_sock.close()
    print("Server shut down.")


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_PORT
    run_server(port=port)
