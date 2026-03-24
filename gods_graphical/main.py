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
from gods.models import Game_State
from gods.setup import quick_setup
from gods_graphical.agent_ui import Agent_UI, update_stacks, sync_game_state_from_table
from gods_graphical.ui import (
    draw_card_power_badge,
    draw_game_over_screen,
    draw_player_hud,
    get_image_path,
    make_gods_stacks,
)
from gods_online.agent_remote import Agent_Local_Online, Agent_Remote
from gods_online.protocol import send_message, try_recv_message
from kitchen_table.config import tweak
from kitchen_table.game_state import update_card_positions
from kitchen_table.input import find_card_at, point_in_stack_area, update_input
from kitchen_table.rendering import color_from_tuple, draw_background, draw_table, render_text, text_width
from kitchen_table.ui import immediate_button, place_inside, place_next
from gods_online.setup import peer_to_peer, setup_online_game

app = typer.Typer()

def find(iterable, predicate, default=None):
    """
    Returns the index of the first item in the iterable that satisfies the predicate.
    """
    return next((i for i, x in enumerate(iterable) if predicate(x)), default)

def init_table_state(gods_state: Game_State, ui_state: UI_State, bottom_player: int = 0) -> kt.Table_State:
    def draw_power(card: kt.Card):
        # card.id == the all_cards index since both lists are aligned.
        gods_card = gods_state.all_cards[card.id]
        power = str(gods_state.effective_power(card.id))
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
    from gods.models import card_designs
    cards = [
        kt.Card(
            id=card.id,
            title=card_designs[card.id].name,
            description=card_designs[card.id].effect,
            image_path=get_image_path(card_designs[card.id].name),
            draw_callback=draw_power,
        )
        for card in gods_state.all_cards
    ]

    # Layout provides visual positions (rects) keyed by zone name.
    stacks = make_gods_stacks(bottom_player=bottom_player)

    # Fill stacks with cards.
    name_to_stack = {stack.name: i for i, stack in enumerate(stacks)}
    for i in range(2):
        p = gods_state.players[i]
        stacks[name_to_stack[f"p{i}_deck"]].cards    = list(p.deck)
        stacks[name_to_stack[f"p{i}_hand"]].cards    = list(p.hand)
        stacks[name_to_stack[f"p{i}_discard"]].cards = list(p.discard)
        stacks[name_to_stack[f"p{i}_wonders"]].cards = list(p.wonders)
        stacks[name_to_stack[f"p{i}_peoples"]].cards = [pid for pid in gods_state.peoples if gods_state.owner(pid) == i]
    stacks[name_to_stack["shared_deck"]].cards = list(gods_state.shared_deck)


    table_state = kt.Table_State(cards=cards, stacks=stacks)
    for stack in table_state.stacks:
        update_card_positions(stack, table_state, sort=False)
    return table_state


def draw_hud(table_state: kt.Table_State, gods_state: Game_State, choice: Choice, ui_state: UI_State, bottom_player: int = 0):
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
        draw_player_hud(i, score, len(player.deck), is_current, hud_y)

    ui_state.draw_buttons()

    text = choice.text_description if choice else ""
    if text and not ui_state.playground:
        font_size = 22
        tw = text_width(text, font_size)
        r = ui_state.place(tw, font_size, x="right", y="center", padding=20)
        render_text(text, r.x, r.y - 50, font_size, pyray.Color(200, 200, 200, 255))


    # Playground toggle button: top-left corner.
    label = "Playground: ON" if ui_state.playground else "Playground: OFF"
    button = ui_state.place(160, 32, x="right", y="top", padding=20)
    if immediate_button(button, label, color=(20,20,20,100)):
        ui_state.playground = not ui_state.playground

        # When leaving playground mode, sync visual state back into game logic.
        if not ui_state.playground:
            sync_game_state_from_table(table_state, gods_state)
            ui_state.power_edit_card_id = -1
        else:
            table_state.is_drop_card_allowed = lambda *_: True
            ui_state.highlighted_cards = {}

    # Power editor overlay: shown when a card's power is being edited in playground mode.
    card_id = ui_state.power_edit_card_id
    if card_id != -1 and ui_state.playground:
        kt_card = table_state.animated_cards[card_id]
        btn_w, btn_h, gap = 44, 36, 6
        panel_w = 10 * btn_w + 9 * gap + 16
        card_rect = pyray.Rectangle(kt_card.x, kt_card.y, tweak["card_width"], tweak["card_height"])
        # Place the panel below the card, horizontally centered on it.
        panel = place_next(card_rect, panel_w, btn_h + 16, x="center", y="bottom", padding=8)
        # Clamp to window bounds.
        panel.x = max(0, min(panel.x, tweak["window_width"] - panel_w))
        panel.y = max(0, min(panel.y, tweak["window_height"] - panel.height))

        pyray.draw_rectangle_rounded(panel, 0.3, 8, pyray.Color(20, 20, 20, 200))
        # First button inside the panel, left edge, vertically centered.
        btn = place_inside(panel, btn_w, btn_h, x="left", y="center", padding=8)
        current_power = gods_state.all_cards[card_id].power
        for v in range(1, 11):
            # Highlight the currently active power value.
            color = pyray.Color(80, 160, 80, 255) if v == current_power else None
            if immediate_button(btn, str(v), color=color):
                gods_state.all_cards[card_id].power = v
                ui_state.power_edit_card_id = -1
                gods_state.on_cards_changed()
            btn.x += btn_w + gap

    # Place shared deck.
    i = find(table_state.stacks, lambda s: s.name == "shared_deck")
    if not ui_state.playground:
        table_state.stacks[i].rect = place_next(window, tweak["card_width"], tweak["card_height"], x="right", y="center", padding=10)
    else:
        table_state.stacks[i].rect = place_inside(window, tweak["card_width"], tweak["card_height"], x="right", y="center", padding=10)
    update_card_positions(table_state.stacks[i], table_state, sort=False)

from kitchen_table.ui import UI_State

def play_gods(gods_state: Game_State, table_state: kt.Table_State, ui_state: UI_State, agent: Agent | None, player_index: int, sock: socket.socket | None = None, friend_addr: tuple[str, int] | None = None):

    # Window: re-use an existing window (e.g. opened by the menu) if one is ready.
    if not pyray.is_window_ready():
        pyray.set_config_flags(pyray.ConfigFlags.FLAG_WINDOW_HIGHDPI)
        pyray.init_window(tweak["window_width"], tweak["window_height"], "Gods Online")
        pyray.set_target_fps(tweak["target_fps"])

    current_choice = None
    table_state.draw_callback = lambda table: draw_hud(table, gods_state, current_choice, ui_state, bottom_player=player_index)

    # In online mode, send all_cards whenever card state changes (power, counters, destroyed, owner).
    if sock is not None and friend_addr is not None:
        def _send_all_cards():
            cards_data = [{"power": c.power, "counters": c.counters, "destroyed": c.destroyed, "owner": c.owner} for c in gods_state.all_cards]
            send_message(sock, {"type": "all_cards", "all_cards": cards_data}, friend_addr)
        gods_state.on_cards_changed = _send_all_cards

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
        mx, my = pyray.get_mouse_x(), pyray.get_mouse_y()

        # In playground mode, P opens the power editor for the hovered card.
        if ui_state.playground and pyray.is_key_pressed(pyray.KeyboardKey.KEY_P):
            result = find_card_at(mx, my, table_state)
            if result:
                hovered_id = result[0]
                # Toggle: pressing P on the same card again closes the editor.
                ui_state.power_edit_card_id = -1 if ui_state.power_edit_card_id == hovered_id else hovered_id
            else:
                ui_state.power_edit_card_id = -1

        # While the power editor is open, keys 1-9 and 0 (=10) set the power directly.
        if ui_state.power_edit_card_id != -1:
            number_keys = [
                pyray.KeyboardKey.KEY_ONE, pyray.KeyboardKey.KEY_TWO, pyray.KeyboardKey.KEY_THREE,
                pyray.KeyboardKey.KEY_FOUR, pyray.KeyboardKey.KEY_FIVE, pyray.KeyboardKey.KEY_SIX,
                pyray.KeyboardKey.KEY_SEVEN, pyray.KeyboardKey.KEY_EIGHT, pyray.KeyboardKey.KEY_NINE,
                pyray.KeyboardKey.KEY_ZERO,
            ]
            for i, key in enumerate(number_keys):
                if pyray.is_key_pressed(key):
                    gods_state.all_cards[ui_state.power_edit_card_id].power = i + 1 if i < 9 else 10
                    ui_state.power_edit_card_id = -1
                    gods_state.on_cards_changed()
                    break
        discard_stack_you = find(table_state.stacks, lambda s: s.name == f"p{player_index}_discard")
        discard_stack_opponent = find(table_state.stacks, lambda s: s.name == f"p{1 - player_index}_discard")
        if pyray.is_mouse_button_pressed(pyray.MouseButton.MOUSE_BUTTON_LEFT):
            for stack_id in (discard_stack_opponent, discard_stack_you):
                stack = table_state.stacks[stack_id]
                is_expanded = stack.spread_x > 0
                inside = point_in_stack_area(mx, my, stack)
                if inside and not is_expanded:
                    # Expand when clicking on a collapsed stack.
                    stack.rect = ui_state.place(tweak["card_width"] * 7, tweak["card_height"], x="center", y="center")
                    stack.spread_x = 150
                    stack.depth = +1.0 # bring to front
                    update_card_positions(stack, table_state, sort=False)
                elif is_expanded and not inside:
                    # Clicking outside an expanded stack collapses it.
                    stack.rect = make_gods_stacks(bottom_player=player_index)[stack_id].rect
                    stack.spread_x = 0
                    stack.depth = 0.0 # reset depth
                    update_card_positions(stack, table_state, sort=False)
        

        # In no-game-logic online mode, sync stacks with the remote player.
        if agent is None and sock is not None and friend_addr is not None:
            # poll_dropped_card() consumes the event so it only fires once per drop,
            # not every frame while dropped_card stays set.
            dropped = table_state.poll_dropped_card()
            should_send = (
                dropped is not None
                or pyray.is_key_pressed(pyray.KeyboardKey.KEY_R) # rotation
                or pyray.is_key_pressed(pyray.KeyboardKey.KEY_S) # shuffle
            )
            if should_send:
                stacks_data = [s.cards for s in table_state.stacks]
                send_message(sock, {"type": "stacks", "stacks": stacks_data}, friend_addr)
            # Apply any incoming update from the remote player.
            msg = try_recv_message(sock)
            if msg and msg.get("type") == "stacks":
                for i, cards in enumerate(msg["stacks"]):
                    table_state.stacks[i].cards = list(cards)
                    update_card_positions(table_state.stacks[i], table_state, sort=False)
            elif msg and msg.get("type") == "all_cards":
                for card, data in zip(gods_state.all_cards, msg["all_cards"]):
                    card.power = data["power"]
                    card.counters = data["counters"]
                    card.destroyed = data["destroyed"]
                    card.owner = data["owner"]

        pyray.begin_drawing()

        # 1.0 when it's the opponent's turn, 0.0 when it's ours.
        turn = 1.0 if gods_state.current_player != player_index else 0.0
        draw_background(turn)
        draw_table(table_state)

        if agent and not ui_state.playground:
            current_choice = game_frame(gods_state, agent, current_choice)
            update_stacks(table_state, gods_state)
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

    play_gods(gods_state, table_state, ui_state, agent, player_index, sock=sock, friend_addr=friend_addr)

    if sock is not None:
        sock.close()

if __name__ == "__main__":
    import sys
    # Default to the graphical menu when no subcommand is given.
    if len(sys.argv) == 1:
        sys.argv.append("start")
    app()
