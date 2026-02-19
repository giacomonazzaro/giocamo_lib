from __future__ import annotations

import os
import socket
import threading
from typing import Annotated

import pyray
import typer

import kitchen_table.models as kt
from game.agents.duel import Agent_Duel
from game.agents.process import Agent_Process
from game.game import game_loop
from gods.gameplay import compute_player_score, Agent_Minimax_Stochastic_Gods
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
from kitchen_table.input import find_card_at, update_input
from kitchen_table.rendering import draw_background, draw_table
from gods_online.setup import peer_to_peer, setup_online_game

app = typer.Typer()


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


def play(gods_state: Game_State, table_state: kt.Table_State, ui_state: UI_State, agent: Agent | None, player_index: int):
    table_state.draw_callback = lambda table: draw_hud(gods_state, table_state, bottom_player=player_index)

    def display(state):
        update_stacks(table_state, gods_state, bottom_player=player_index)

    # Window
    pyray.set_config_flags(pyray.ConfigFlags.FLAG_WINDOW_HIGHDPI)
    pyray.init_window(tweak["window_width"], tweak["window_height"], "Gods Online")
    pyray.set_target_fps(tweak["target_fps"])

    if agent:
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

        if not agent:
            update_input(table_state)

        pyray.begin_drawing()
        # 1.0 when it's the opponent's turn, 0.0 when it's ours.
        turn = 1.0 if gods_state.current_player != player_index else 0.0
        draw_background(turn)
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
    sock = peer_to_peer(local)
    main(*setup_online_game(sock, local))

@app.command()
def agent(game_logic: bool = True):
    main(player_index=0, seed=None, sock=None, game_logic=game_logic)

def main(
        player_index: int = 0, 
        seed: int | None = None, 
        sock: socket.socket | None = None,
        friend_addr: tuple[str, int] | None = None,
        game_logic: bool = True
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
        agent_opponent = Agent_Process(Agent_Minimax_Stochastic_Gods())
    
    if not game_logic:
        agent = None
    else:
        agent = Agent_Duel(agent_local, agent_opponent, swap=player_index != 0)

    play(gods_state, table_state, ui_state, agent, player_index)

    if sock is not None:
        sock.close()

if __name__ == "__main__":
    app()
