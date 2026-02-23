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
from game.game import game_frame, game_loop, resolve_choice
from gods.gameplay import compute_player_score, Agent_Minimax_Stochastic_Gods
from gods.models import Game_State, effective_power
from gods.setup import quick_setup
from gods_graphical.agent_ui import Agent_UI, update_stacks
from gods_graphical.ui import (
    draw_card_power_badge,
    draw_game_over_screen,
    draw_people_ownership_bars,
    draw_player_hud,
    get_image_path,
    get_table_layout,
)
from gods_online.agent_remote import Agent_Local_Online, Agent_Remote
from kitchen_table.config import tweak
from kitchen_table.game_state import update_card_positions
from kitchen_table.input import find_card_at, update_input
from kitchen_table.rendering import draw_background, draw_table
from gods_online.setup import peer_to_peer, setup_online_game

app = typer.Typer()


def init_table_state(gods_state: Game_State, bottom_player: int = 0) -> kt.Table_State:
    def draw_power(card: kt.Card):
        # card.id == the all_cards index since both lists are aligned.
        gods_card = gods_state.all_cards[card.id]
        power = str(effective_power(gods_state, gods_card))
        draw_card_power_badge(power, gods_card.destroyed)

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
    zone_cards["peoples"] = list(gods_state.peoples)
    zone_cards["shared_deck"] = list(gods_state.shared_deck)

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
    H = tweak["window_height"]
    h = tweak["card_height"]
    margin = 20
    bottom_wonders_y = H - h - margin - h - margin
    opponent_shift = int(h * 0.65)
    top_wonders_y = H - bottom_wonders_y - h - opponent_shift

    for i in range(2):
        player = gods_state.players[i]
        score = compute_player_score(gods_state, i)
        is_current = i == gods_state.current_player
        hud_y = (bottom_wonders_y - 40) if i == bottom_player else top_wonders_y
        draw_player_hud(player.name, score, len(player.deck), is_current, hud_y)

    # People ownership. Since table_state.cards is aligned with all_cards,
    # the gods card id == the kt card id, so we use pid directly.
    people_info = [
        (pid, gods_state.all_cards[pid].owner)
        for pid in gods_state.peoples
        if gods_state.all_cards[pid].owner is not None and not gods_state.all_cards[pid].destroyed
    ]
    draw_people_ownership_bars(people_info, table_state)


from kitchen_table.ui import UI_State

def play(gods_state: Game_State, table_state: kt.Table_State, ui_state: UI_State, agent: Agent | None, player_index: int):
    table_state.draw_callback = lambda table: draw_hud(gods_state, table_state, bottom_player=player_index)

    def display(state):
        update_stacks(table_state, gods_state, bottom_player=player_index)

    # Window: re-use an existing window (e.g. opened by the menu) if one is ready.
    if not pyray.is_window_ready():
        pyray.set_config_flags(pyray.ConfigFlags.FLAG_WINDOW_HIGHDPI)
        pyray.init_window(tweak["window_width"], tweak["window_height"], "Gods Online")
        pyray.set_target_fps(tweak["target_fps"])

    current_choice = None
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

        if agent:
            current_choice = game_frame(gods_state, agent, current_choice)
            update_stacks(table_state, gods_state, bottom_player=player_index)

        pyray.begin_drawing()
        # 1.0 when it's the opponent's turn, 0.0 when it's ours.
        turn = 1.0 if gods_state.current_player != player_index else 0.0
        draw_background(turn)
        draw_table(table_state)
        ui_state.draw_buttons()
        ui_state.draw_card_highlights(table_state)
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
def start():
    """Launch the game with the graphical menu (default entry point)."""
    from gods_graphical.menu import run_menu
    mode, params = run_menu()
    if mode == "vs_ai":
        main(vs_ai=True)
    else:  # "online"
        main(**params)


@app.command()
def p2p(
    local: Annotated[bool, typer.Option("--local", help="Use local mode (no STUN, for testing on the same network)")] = False,
    join: Annotated[Optional[str], typer.Option("--join", "-j", help="Room code to join")] = None,
):
    sock, your_ip, your_port = peer_to_peer(local)
    main(*setup_online_game(sock, local, your_ip, your_port, room_code=join))

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
    table_state = init_table_state(gods_state, bottom_player=player_index)
    ui_state = UI_State()

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

    play(gods_state, table_state, ui_state, agent, player_index)

    if sock is not None:
        sock.close()

if __name__ == "__main__":
    import sys
    # Default to the graphical menu when no subcommand is given.
    if len(sys.argv) == 1:
        sys.argv.append("start")
    app()
