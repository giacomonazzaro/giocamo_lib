from __future__ import annotations

import os
import socket
import threading
from typing import Annotated, Optional


import pyray
import typer

from game.agents.randomized import Agent_Random
import kitchen_table.models as kt
from game.agents.duel import Agent_Duel
from game.agents.process import Agent_Process
from game.game import Choice, game_frame, game_loop, resolve_choice
from gods.gameplay import compute_player_score, Agent_Minimax_Stochastic_Gods
from gods.models import Game_State, effective_power
from gods.setup import quick_setup
from gods_graphical.agent_ui import Agent_UI, update_stacks, ZONE_ORDER
from gods_graphical.ui import (
    draw_card_power_badge,
    draw_game_over_screen,
    draw_player_hud,
    get_image_path,
    get_table_layout,
)
from gods_online.agent_remote import Agent_Local_Online, Agent_Remote
from gods_online.protocol import send_unreliable, try_recv_message
from kitchen_table.config import tweak
from kitchen_table.game_state import update_card_positions
from kitchen_table.input import find_card_at, update_input
from kitchen_table.rendering import color_from_tuple, draw_background, draw_table, render_text, text_width
from kitchen_table.ui import place_inside
from gods_online.setup import peer_to_peer, setup_online_game

app = typer.Typer()


def init_table_state(gods_state: Game_State, ui_state: UI_State, bottom_player: int = 0) -> kt.Table_State:
    def draw_power(card: kt.Card):
        # card.id == the all_cards index since both lists are aligned.
        gods_card = gods_state.all_cards[card.id]
        power = str(effective_power(gods_state, gods_card))
        draw_card_power_badge(power, gods_card.destroyed)

        highlight_color = color_from_tuple(tweak["highlight_color"])
        w = tweak["card_width"]
        h = tweak["card_height"]

        if card.id in ui_state.highlighted_cards.values():
            kt_card = table_state.animated_cards[card.id]
            pyray.draw_rectangle_rounded_lines_ex(
                pyray.Rectangle(0, 0, w, h), 0.25, 8, 4, highlight_color
            )

    # Build table_state.cards in the same order as game.all_cards so that
    # table_state.cards[i] corresponds to game.all_cards[i], making card.id
    # serve as the shared integer key with no separate kt_card_id needed.
    cards = [
        kt.Card(
            id=card.id,
            title=card.name,
            description=card.effect,
            image_path=get_image_path(card.name),
            draw_callback=draw_power,
        )
        for card in gods_state.all_cards
    ]

    # Stacks use the gods int IDs directly — no translation needed.
    zone_cards = {}
    for i in range(2):
        p = gods_state.players[i]
        zone_cards[f"p{i}_deck"] = list(p.deck)
        zone_cards[f"p{i}_hand"] = list(p.hand)
        zone_cards[f"p{i}_discard"] = list(p.discard)
        zone_cards[f"p{i}_wonders"] = list(p.wonders)
        zone_cards[f"p{i}_peoples"] = [
            pid for pid in gods_state.peoples if gods_state.all_cards[pid].owner == i
        ]
    zone_cards["shared_deck"] = list(gods_state.shared_deck)

    # Layout provides visual positions (rects) keyed by zone name.
    layout = get_table_layout(bottom_player=bottom_player)

    stacks = []
    for zone_name in ZONE_ORDER:
        z = layout[zone_name]
        card_ids = zone_cards.get(zone_name, [])
        stack = kt.Stack(rect=z.rect, cards=card_ids, spread_x=z.spread_x, spread_y=z.spread_y, face_up=z.face_up, name=zone_name)
        stacks.append(stack)

    table_state = kt.Table_State(cards=cards, stacks=stacks)
    for stack in table_state.stacks:
        update_card_positions(stack, table_state)
    return table_state


def draw_hud(gods_state: Game_State, choice: Choice, ui_state: UI_State, bottom_player: int = 0):
    H = tweak["window_height"]
    h = tweak["card_height"]
    margin = 20
    window = pyray.Rectangle(0, 0, tweak["window_width"], H)
    bottom_wonders_y = place_inside(window, 0, h, x="left", y="bottom", padding=2 * margin + h).y
    opponent_shift = int(h * 0.65)
    top_wonders_y = H - bottom_wonders_y - h - opponent_shift

    for i in range(2):
        player = gods_state.players[i]
        score = compute_player_score(gods_state, i)
        is_current = i == gods_state.current_player
        hud_y = bottom_wonders_y + h // 2
        if i != bottom_player: hud_y = top_wonders_y + h // 2
        name = "You" if i == bottom_player else "Opponent"
        draw_player_hud(name, score, len(player.deck), is_current, hud_y)

    ui_state.draw_buttons()
    text = choice.text_description if choice else ""
    if text:
        font_size = 22
        tw = text_width(text, font_size)
        r = ui_state.place(tw, font_size, x="right", y="center", padding=20)
        render_text(text, r.x, r.y - 50, font_size, pyray.Color(200, 200, 200, 255))

    # draw_choice_description(ui_state.current_choice_text)

from kitchen_table.ui import UI_State

def play(gods_state: Game_State, table_state: kt.Table_State, ui_state: UI_State, agent: Agent | None, player_index: int, sock: socket.socket | None = None, friend_addr: tuple[str, int] | None = None):

    # Window: re-use an existing window (e.g. opened by the menu) if one is ready.
    if not pyray.is_window_ready():
        pyray.set_config_flags(pyray.ConfigFlags.FLAG_WINDOW_HIGHDPI)
        pyray.init_window(tweak["window_width"], tweak["window_height"], "Gods Online")
        pyray.set_target_fps(tweak["target_fps"])

    current_choice = None
    table_state.draw_callback = lambda table: draw_hud(gods_state, current_choice, ui_state, bottom_player=player_index)
    
    while not pyray.window_should_close():
        if gods_state.game_over:
            break

        # Handle card zoom TODO(giacomo, claude): move to kitchen_table
        if pyray.is_key_down(pyray.KeyboardKey.KEY_SPACE):
            mx, my = pyray.get_mouse_x(), pyray.get_mouse_y()
            result = find_card_at(mx, my, table_state)
            table_state.zoomed_card_id = result[0] if result else -1
        else:
            table_state.zoomed_card_id = -1

        # if not agent:
        update_input(table_state)

        # In no-game-logic online mode, sync stacks with the remote player.
        if agent is None and sock is not None and friend_addr is not None:
            # Send full state every frame — it's small and unreliable is fine.
            # Losing one packet is harmless; the next frame corrects it.
            send_unreliable(sock, {"type": "stacks", "stacks": [s.cards for s in table_state.stacks]}, friend_addr)

            # Drain the queue and apply only the latest stacks message received
            # this frame, discarding any stale buffered updates.
            latest_stacks = None
            while True:
                msg = try_recv_message(sock)
                if msg is None:
                    break
                if msg.get("type") == "stacks":
                    latest_stacks = msg
            if latest_stacks:
                for i, cards in enumerate(latest_stacks["stacks"]):
                    table_state.stacks[i].cards = list(cards)
                    update_card_positions(table_state.stacks[i], table_state)

        pyray.begin_drawing()

        # 1.0 when it's the opponent's turn, 0.0 when it's ours.
        turn = 1.0 if gods_state.current_player != player_index else 0.0
        draw_background(turn)
        draw_table(table_state)
        
        if agent:
            current_choice = game_frame(gods_state, agent, current_choice)
            update_stacks(table_state, gods_state)
            current_choice.text_description if current_choice else ""
        pyray.end_drawing()

    # Game over screen
    if gods_state.game_over:
        update_stacks(table_state, gods_state)
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
def start(game_logic: bool = True):
    """Launch the game with the graphical menu (default entry point)."""
    from gods_graphical.menu import run_menu
    mode, params = run_menu()
    if mode == "vs_ai":
        main(vs_ai=True, game_logic=game_logic)
    else:  # "online"
        main(**params, game_logic=game_logic)


@app.command()
def p2p(
    local: Annotated[bool, typer.Option("--local", help="Use local mode (no STUN, for testing on the same network)")] = False,
    join: Annotated[Optional[str], typer.Option("--join", "-j", help="Room code to join")] = None,
    game_logic: bool = True,
):
    sock, your_ip, your_port = peer_to_peer(local)
    player_index, seed, sock, friend_addr = setup_online_game(sock, local, your_ip, your_port, room_code=join)
    main(player_index=player_index, seed=seed, sock=sock, friend_addr=friend_addr, game_logic=game_logic)

@app.command()
def agent(game_logic: bool = True, seed=None):
    main(player_index=0, seed=seed, sock=None, game_logic=game_logic)

def main(
        player_index: int = 0,
        seed: int | None = None,
        sock: socket.socket | None = None,
        friend_addr: tuple[str, int] | None = None,
        game_logic: bool = True,
        vs_ai: bool = False,
):
    gods_state = quick_setup(seed)
    ui_state = UI_State()
    table_state = init_table_state(gods_state, ui_state, bottom_player=player_index)

    agent_ui = Agent_UI(table_state, ui_state, bottom_player=player_index)
    if sock is not None and friend_addr is not None:
        agent_local = Agent_Local_Online(agent_ui, sock, friend_addr)
        agent_opponent = Agent_Remote(sock)
    elif vs_ai:
        agent_local = agent_ui
        agent_opponent = Agent_Process(Agent_Minimax_Stochastic_Gods())
    else:
        agent_local = agent_ui
        agent_opponent = agent_ui

    if not game_logic:
        agent = None
    else:
        agent = Agent_Duel(agent_local, agent_opponent, swap=player_index != 0)

    play(gods_state, table_state, ui_state, agent, player_index, sock=sock, friend_addr=friend_addr)

    if sock is not None:
        sock.close()

if __name__ == "__main__":
    import sys
    # Default to the graphical menu when no subcommand is given.
    if len(sys.argv) == 1:
        sys.argv.append("start")
    app()
