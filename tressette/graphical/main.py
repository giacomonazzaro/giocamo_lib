from __future__ import annotations

from typing import Annotated

import pyray
import typer

import kitchen_table.models as kt
from game.agents.duel import Agent_Duel
from game.agents.process import Agent_Process
from kitchen_table.config import tweak
from kitchen_table.game_state import update_card_positions
from kitchen_table.input import update_input
from kitchen_table.rendering import draw_background, draw_table
from kitchen_table.ui import UI_State

from tressette.game.game import game_frame
from tressette.game.gameplay import (
    Tressette_Agent,
    compute_player_score,
)
from tressette.game.models import Game_State
from tressette.game.setup import quick_setup
from tressette.graphical.agent_ui import Agent_UI
from tressette.graphical.ui import (
    HAND,
    STOCK_IDX,
    TABLE_IDX,
    TRICKS,
    draw_game_over_overlay,
    draw_player_hud,
    make_card_draw_callback,
    make_tressette_stacks,
)


def init_table_state(
    state: Game_State, ui_state: UI_State, hot_seat: bool
) -> kt.Table_State:
    draw = make_card_draw_callback(state, ui_state)
    cards = [
        kt.Card(id=card.id, image_path="", draw_callback=draw)
        for card in state.all_cards
    ]
    stacks = make_tressette_stacks(both_hands_visible=hot_seat)

    stacks[HAND[0]].cards = list(state.players[0].hand)
    stacks[HAND[1]].cards = list(state.players[1].hand)
    stacks[TRICKS[0]].cards = list(state.players[0].tricks_won)
    stacks[TRICKS[1]].cards = list(state.players[1].tricks_won)
    stacks[STOCK_IDX].cards = list(state.stock)
    stacks[TABLE_IDX].cards = list(state.trick)

    table_state = kt.Table_State(cards=cards, stacks=stacks)
    for stack in table_state.stacks:
        update_card_positions(stack, table_state, sort=False)
    return table_state


def update_stacks(table_state: kt.Table_State, state: Game_State):
    """Sync visual stacks to the current game state and reposition cards."""
    table_state.stacks[HAND[0]].cards = list(state.players[0].hand)
    table_state.stacks[HAND[1]].cards = list(state.players[1].hand)
    table_state.stacks[TRICKS[0]].cards = list(state.players[0].tricks_won)
    table_state.stacks[TRICKS[1]].cards = list(state.players[1].tricks_won)
    table_state.stacks[STOCK_IDX].cards = list(state.stock)
    table_state.stacks[TABLE_IDX].cards = list(state.trick)
    for stack in table_state.stacks:
        update_card_positions(stack, table_state, sort=False)
    # table_state.stacks[HAND[0]].cards.sort(key=lambda cid: state.all_cards[cid].suit)
    # table_state.stacks[HAND[1]].cards.sort(key=lambda cid: state.all_cards[cid].suit)


def draw_hud(state: Game_State, ui_state: UI_State):
    H = tweak["window_height"]
    for i in range(2):
        score = compute_player_score(state, i)
        is_current = i == state.current_player
        # Place above the bottom HUD for player 0 and below the top hand for player 1.
        hud_y = H - 56 if i == 0 else 16
        draw_player_hud(i, score, is_current, hud_y)


def play_tressette(
    state: Game_State,
    table_state: kt.Table_State,
    ui_state: UI_State,
    agent,
):
    if not pyray.is_window_ready():
        pyray.set_config_flags(pyray.ConfigFlags.FLAG_WINDOW_HIGHDPI)
        pyray.init_window(tweak["window_width"], tweak["window_height"], "Tressette")
        pyray.set_target_fps(tweak["target_fps"])

    current_choice = None
    table_state.draw_callback = lambda _: draw_hud(state, ui_state)

    while not pyray.window_should_close():
        if state.game_over:
            break

        update_input(table_state)

        pyray.begin_drawing()
        draw_background(0.0)
        draw_table(table_state)

        if agent is not None:
            current_choice = game_frame(state, agent, current_choice)
            update_stacks(table_state, state)

        pyray.end_drawing()

    if state.game_over:
        update_stacks(table_state, state)
        scores = [compute_player_score(state, 0), compute_player_score(state, 1)]
        while not pyray.window_should_close():
            pyray.begin_drawing()
            draw_background(0.0)
            draw_table(table_state)
            draw_game_over_overlay(scores)
            pyray.end_drawing()

    pyray.close_window()


def main(
    vs_ai: Annotated[
        bool,
        typer.Option(
            "--vs-ai/--hot-seat",
            help="--vs-ai plays against the C++ minimax agent. --hot-seat opens both hands for two humans on one machine.",
        ),
    ] = True,
    seed: Annotated[
        int, typer.Option("--seed", help="Deal seed (omit for random).")
    ] = None,
):
    state = quick_setup(seed)
    ui_state = UI_State()
    table_state = init_table_state(state, ui_state, hot_seat=not vs_ai)

    agent_ui = Agent_UI(table_state, ui_state)
    if vs_ai:
        opponent = Agent_Process(Tressette_Agent())
        agent = Agent_Duel(agent_ui, opponent, swap=False)
    else:
        agent = Agent_Duel(agent_ui, agent_ui, swap=False)

    play_tressette(state, table_state, ui_state, agent)


if __name__ == "__main__":
    typer.run(main)
