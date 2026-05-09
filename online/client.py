from __future__ import annotations
import socket
import sys

from online.protocol import send_message, recv_message

DEFAULT_PORT = 9999


def render_state_text(state: dict) -> str:
    lines = []
    lines.append("=" * 60)

    # People cards
    lines.append("--- People Cards ---")
    for p in state["peoples"]:
        owner = p["owner"]
        if owner is not None:
            owner_str = f" - Controlled by {state['players'][owner]['name']}"
        else:
            owner_str = " - Unclaimed"
        destroyed = " [DESTROYED]" if p["destroyed"] else ""
        lines.append(
            f"  {p['name']} [{p['color']} {p['card_type']}, power {p['power']}]{destroyed}{owner_str}"
        )
        lines.append(f"    Effect: {p['effect']}")

    # Players
    for i, player in enumerate(state["players"]):
        is_current = i == state["current_player"]
        marker = " <<< CURRENT TURN" if is_current else ""
        lines.append(f"\n--- {player['name']}{marker} ---")
        lines.append(
            f"  Deck: {player['deck_count']} cards | Discard: {player['discard_count']} cards"
        )

        if player["wonders"]:
            lines.append(f"  Wonders in play:")
            for w in player["wonders"]:
                counters = ""
                if w["counters"] > 0:
                    counters = f" (+{w['counters']})"
                elif w["counters"] < 0:
                    counters = f" ({w['counters']})"
                destroyed = " [DESTROYED]" if w["destroyed"] else ""
                lines.append(
                    f"    - {w['name']} [{w['color']} {w['card_type']}, power {w['power']}{counters}]{destroyed} - {w['effect']}"
                )
        else:
            lines.append(f"  Wonders in play: None")

        if player["hand"]:
            lines.append(f"  Hand ({player['hand_count']} cards):")
            for c in player["hand"]:
                counters = ""
                if c["counters"] > 0:
                    counters = f" (+{c['counters']})"
                elif c["counters"] < 0:
                    counters = f" ({c['counters']})"
                lines.append(
                    f"    - {c['name']} [{c['color']} {c['card_type']}, power {c['power']}{counters}] - {c['effect']}"
                )
        else:
            lines.append(f"  Hand: {player['hand_count']} cards (hidden)")

        lines.append(f"  Points: {state['scores'][i]}")

    lines.append("=" * 60)
    return "\n".join(lines)


def run_client(host: str = "localhost", port: int = DEFAULT_PORT):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    print(f"Connected to {host}:{port}")

    try:
        while True:
            msg = recv_message(sock)
            msg_type = msg["type"]

            if msg_type == "welcome":
                print(msg["message"])

            elif msg_type == "message":
                print(msg["text"])

            elif msg_type == "state_update":
                print(render_state_text(msg["game_state"]))

            elif msg_type == "choose_action":
                actions = msg["actions"]
                print(f"\nChoose an action ({msg['choice_type']}):")
                for i, action in enumerate(actions):
                    print(f"  {i + 1}: {action}")

                selected = -1
                while selected not in range(len(actions)):
                    try:
                        selected = int(input("Enter choice: ")) - 1
                    except (ValueError, EOFError):
                        pass

                send_message(sock, {"type": "action_response", "index": selected})

            elif msg_type == "game_over":
                scores = msg["scores"]
                print("\n" + "=" * 60)
                print("GAME OVER!")
                print(f"  Player 1: {scores[0]} points")
                print(f"  Player 2: {scores[1]} points")
                if scores[0] > scores[1]:
                    print("  Player 1 wins!")
                elif scores[1] > scores[0]:
                    print("  Player 2 wins!")
                else:
                    print("  It's a tie!")
                if "final_state" in msg:
                    print(render_state_text(msg["final_state"]))
                print("=" * 60)
                break

    except ConnectionError:
        print("Disconnected from server.")
    finally:
        sock.close()


if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else "localhost"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_PORT
    run_client(host, port)
