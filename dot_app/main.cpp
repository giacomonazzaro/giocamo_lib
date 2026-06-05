#include <dot/gameplay.h>
#include <dot/models.h>
#include <game/agent.h>
#include <game/game.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <vector>

#include "agent_ui.h"
#include "ui.h"

// The local player always sits in seat 0; the computer plays seat 1.
static const int LOCAL_SEAT = 0;

// Copy the game state into the table: each stack owns the matching cards, the
// play area is cleared (it isn't backed by game state), and the shared pool
// stays face-down until both players have committed their three cards.
static void update_table_from_game(Table_State& table, dot::Game_State& state) {
  int  base    = (int)state.all_cards.size();
  auto set_stack = [&](int stack, const std::vector<int>& cards) {
    table.things[base + stack].children = cards;
    update_children_positions(base + stack, table, false);
  };
  set_stack(DOT_POOL_1, state.players[1].pool);
  set_stack(DOT_SHARED, state.shared_pool);
  set_stack(DOT_PLAY_AREA, {});
  set_stack(DOT_HAND_0, state.players[0].hand);
  set_stack(DOT_POOL_0, state.players[0].pool);
  set_stack(DOT_HAND_1, state.players[1].hand);
  set_stack(DOT_DRAW_0, state.players[0].draw_deck);
  set_stack(DOT_STAR_0, state.players[0].star_deck);
  set_stack(DOT_DRAW_1, state.players[1].draw_deck);
  set_stack(DOT_STAR_1, state.players[1].star_deck);

  // Reveal the shared pool only once both players' three cards are in it.
  bool both_committed = (int)state.shared_pool.size() >= 2 * dot::SHARED_COUNT;
  table.things[base + DOT_SHARED].face_up = both_committed;
}

static Table_State init_table_state(dot::Game_State& state, UI_State& ui_state) {
  Table_State table;

  // One Thing per card; ids match all_cards indices. Cream face so the
  // colored dots stand out.
  for (const dot::Card& card : state.all_cards) {
    Thing thing = make_card(card.id);
    thing.color = {235, 225, 205, 255};
    table.things.push_back(thing);
    table.draw_callbacks[card.id] =
      make_dot_card_draw_callback(state.all_cards, ui_state, card.id);
  }

  // Stacks appended after the cards.
  int                base   = (int)table.things.size();
  std::vector<Thing> stacks = make_dot_stacks();
  std::vector<int>   stack_ids;
  for (Thing& stack : stacks) {
    stack.id = (int)table.things.size();
    stack_ids.push_back(stack.id);
    table.things.push_back(std::move(stack));
  }

  // Only the local player, on their own turn, may drag: hand <-> play area
  // during the split, opponent's pool <-> play area during the discard.
  int hand_local = base + DOT_HAND_0;
  int pool_opp   = base + DOT_POOL_1;
  int play_area  = base + DOT_PLAY_AREA;
  table.is_drop_allowed =
    [&state, hand_local, pool_opp, play_area](int src, int dst, int) {
      if (src == dst) return true;
      if (state.acting_player != LOCAL_SEAT) return false;
      if (state.phase == dot::Phase::SPLIT) {
        return (src == hand_local && dst == play_area) ||
               (src == play_area && dst == hand_local);
      }
      if (state.phase == dot::Phase::DISCARD) {
        return (src == pool_opp && dst == play_area) ||
               (src == play_area && dst == pool_opp);
      }
      return false;  // Acknowledge pause: nothing is draggable.
    };

  auto root = create_table_root(
    tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
  );
  root.id       = (int)table.things.size();
  root.children = stack_ids;
  table.things.push_back(root);
  table.root = root.id;

  update_table_from_game(table, state);
  return table;
}

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto inputs = Input_Feed(Input_Mode::Live, "");
  // Single screen vs the computer: skip the online/hot-seat menu.
  Menu_Result menu_result = run_menu(
    "D.O.T",
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    inputs,
    argc,
    argv,
    /*skip_menu=*/true,
    options.seed
  );

  auto state    = dot::quick_setup(menu_result.seed);
  state.human_player = LOCAL_SEAT;  // The acknowledge pause is owned by the human.
  auto ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
  auto table    = init_table_state(state, ui_state);

  // Always-on HUD overlay (round, tokens, pool totals).
  table.draw_callbacks[-1] = [&](const Table_State&, const Input&, bool) {
    draw_dot_hud(state);
  };

  int base = (int)state.all_cards.size();
  auto human = Dot_Agent_UI(&table, &ui_state, LOCAL_SEAT, base);
  // The computer opponent picks its split/discard at random.
  auto computer = Agent_Random((unsigned)menu_result.seed + 1u);
  auto duel     = Agent_Duel(&human, &computer, /*swap=*/false);

  play_game(
    state,
    table,
    ui_state,
    duel,
    inputs,
    menu_result,
    "D.O.T",
    [&] { update_table_from_game(table, state); },
    [&] {
      return std::vector<int>{
        dot::compute_player_score(state, 0),
        dot::compute_player_score(state, 1),
      };
    }
  );
  return 0;
}
