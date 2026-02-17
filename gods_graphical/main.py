from __future__ import annotations

import json
import os
import random
import socket
import struct
import threading
import time
from typing import Annotated

import pyray
import typer

import kitchen_table.models as kt
from gods.agents.duel import Agent_Duel
from gods.agents.minimax_stochastic import Agent_Minimax_Stochastic
from gods.game import compute_player_score, game_loop
from gods.models import Game_State, effective_power
from gods.setup import quick_setup
from gods_graphical.agent_ui import Agent_UI, update_stacks
from gods_graphical.ui import (
    UI_State,
    draw_buttons,
    draw_card_highlights,
    draw_card_power_badge,
    draw_final_round_indicator,
    draw_game_over_screen,
    draw_people_ownership_bars,
    draw_player_hud,
    get_image_path,
    get_table_layout,
)
from gods_online.agent_remote import Agent_Local_Online, Agent_Remote
from kitchen_table.config import tweak
from kitchen_table.game_state import update_card_positions
from kitchen_table.input import find_card_at
from kitchen_table.rendering import draw_background, draw_table

app = typer.Typer()

STUN_SERVER = os.getenv('STUN_SERVER', 'stun.l.google.com')
STUN_PORT = int(os.getenv('STUN_PORT', '19302'))

def init_table_state(gods_state: Game_State, bottom_player: int = 0) -> kt.Table_State:
    cards = []
    gods_cards = []

    def draw_power(card: kt.Card):
        gods_card = gods_cards[card.id]
        power = str(effective_power(gods_state, gods_card))
        draw_card_power_badge(power, gods_card.destroyed)

    def register_cards(card_list):
        card_ids = []
        for i, card in enumerate(card_list):
            card_id = len(cards)
            kt_card = kt.Card(
                id=card_id,
                title=card.name,
                description=card.effect,
                image_path=get_image_path(card.name),
                draw_callback=draw_power,
            )
            card.id = card_id
            card_list[i].kt_card_id = card_id
            cards.append(kt_card)
            gods_cards.append(card)
            card_ids.append(card_id)
        return card_ids

    # Register all cards and build zone mapping
    zone_cards = {}
    for i in range(2):
        p = gods_state.players[i]
        zone_cards[f"p{i}_deck"] = register_cards(p.deck)
        zone_cards[f"p{i}_hand"] = register_cards(p.hand)
        zone_cards[f"p{i}_discard"] = register_cards(p.discard)
        zone_cards[f"p{i}_wonders"] = register_cards(p.wonders)
    zone_cards["peoples"] = register_cards(gods_state.peoples)
    zone_cards["shared_deck"] = register_cards(gods_state.shared_deck)

    # Create stacks from shared layout
    stacks = []
    for zone_name, sx, sy, sw, spx, spy, face_up in get_table_layout(bottom_player=bottom_player):
        card_ids = zone_cards.get(zone_name, [])
        stack = kt.Stack(x=sx, y=sy, cards=card_ids, width=sw, spread_x=spx, spread_y=spy, face_up=face_up, name=zone_name)
        stacks.append(stack)

    table_state = kt.Table_State(cards=cards, stacks=stacks)
    for stack in table_state.stacks:
        update_card_positions(stack, table_state)
    return table_state


def draw_hud(gods_state: Game_State, table_state: kt.Table_State, bottom_player: int = 0):
    for i in range(2):
        player = gods_state.players[i]
        score = compute_player_score(gods_state, i)
        is_current = i == gods_state.current_player
        hud_y = 650 if i == bottom_player else 260
        draw_player_hud(player.name, score, len(player.deck), is_current, hud_y)

    # People ownership
    people_info = [
        (p.id, p.owner) for p in gods_state.peoples
        if p.owner is not None and not p.destroyed
    ]
    draw_people_ownership_bars(people_info, table_state)

    # Final round indicator
    if gods_state.game_ending and not gods_state.game_over:
        draw_final_round_indicator()


def draw_highlighted_cards(highlighted_cards: list, gods_state: Game_State, table_state: kt.Table_State):
    kt_ids = []
    for card_id in highlighted_cards:
        try:
            card = gods_state.get_card(card_id)
            kt_ids.append(card.id)
        except Exception:
            continue
    draw_card_highlights(kt_ids, table_state)

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


def play(gods_state: Game_State, table_state: kt.Table_State, ui_state: UI_State, agent_local: Agent, agent_opponent: Agent, player_index: int):
    agent = Agent_Duel(agent_local, agent_opponent, swap=player_index != 0)
    table_state.draw_callback = lambda table: draw_hud(gods_state, table_state, bottom_player=player_index)

    def display(state):
        update_stacks(table_state, gods_state, bottom_player=player_index)

    # Window
    pyray.set_config_flags(pyray.ConfigFlags.FLAG_WINDOW_HIGHDPI)
    pyray.init_window(tweak["window_width"], tweak["window_height"], "Gods Online")
    pyray.set_target_fps(tweak["target_fps"])

    game_thread = threading.Thread(
        target=lambda: game_loop(gods_state, agent, display),
        daemon=True,
    )
    game_thread.start()

    while not pyray.window_should_close():
        if gods_state.game_over:
            break

        # Handle card zoom
        if pyray.is_key_down(pyray.KeyboardKey.KEY_SPACE):
            mx, my = pyray.get_mouse_x(), pyray.get_mouse_y()
            result = find_card_at(mx, my, table_state)
            table_state.zoomed_card_id = result[0] if result else -1
        else:
            table_state.zoomed_card_id = -1

        pyray.begin_drawing()
        draw_background()
        draw_table(table_state)
        draw_buttons(ui_state.buttons)
        draw_highlighted_cards(ui_state.highlighted_cards, gods_state, table_state)
        pyray.end_drawing()

    # Game over screen
    if gods_state.game_over:
        update_stacks(table_state, gods_state, bottom_player=player_index)
        scores = [compute_player_score(gods_state, 0), compute_player_score(gods_state, 1)]
        names = [gods_state.players[0].name, gods_state.players[1].name]
        pi = player_index
        if scores[pi] > scores[1 - pi]:
            result_text = "You win!"
        elif scores[pi] < scores[1 - pi]:
            result_text = "You lose!"
        else:
            result_text = "It's a tie!"
        draw_game_over_screen(table_state, result_text, names, scores)

    pyray.close_window()

@app.command()
def p2p(
    local: Annotated[bool, typer.Option("--local", help="Use local mode (no STUN, for testing on the same network)")] = False,
):
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
    
    main(*setup_online_game(sock, local))

@app.command()
def agent():
    main(player_index=0, seed=None, sock=None)

def main(
        player_index: int = 0, 
        seed: int | None = None, 
        sock: socket.socket | None = None,
        friend_addr: tuple[str, int] | None = None,
):
    gods_state = quick_setup(seed)
    table_state = init_table_state(gods_state, bottom_player=player_index)
    ui_state = UI_State()
    
    agent_ui = Agent_UI(table_state, ui_state, bottom_player=player_index)
    if sock is not None and friend_addr is not None:
        agent_local = Agent_Local_Online(agent_ui, sock, friend_addr)
        agent_opponent = Agent_Remote(sock)
    else:
        agent_local = agent_ui
        agent_opponent = Agent_Minimax_Stochastic()

    play(gods_state, table_state, ui_state, agent_local, agent_opponent, player_index)
    
    if sock is not None:
        sock.close()

if __name__ == "__main__":
    app()
