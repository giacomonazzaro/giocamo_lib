#include <game/agent.h>
#include <game/game.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <scopa/ai.h>
#include <scopa/gameplay.h>
#include <scopa/models.h>
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "agent_ui.h"
#include "ui.h"

static Table_State init_table_state(
  scopa::Game_State& state,
  UI_State&          ui_state,
  int                bottom_player,
  bool               show_opponent_hand
) {
  Table_State table;
  table.is_drop_allowed = [](int, int, int) { return false; };

  // One Thing per card; ids 0..39 match all_cards indices.
  for (const auto& card : state.all_cards) {
    Thing thing = make_card();
    if (card.suit == scopa::Suit::COPPE) thing.color = {50, 100, 50, 255};
    if (card.suit == scopa::Suit::DENARI) thing.color = {150, 120, 20, 255};
    if (card.suit == scopa::Suit::BASTONI) thing.color = {80, 50, 50, 255};
    if (card.suit == scopa::Suit::SPADE) thing.color = {70, 80, 150, 255};
    table.things.push_back(thing);
    table.draw_callbacks[card.id] =
      make_card_draw_callback(state, ui_state, card.id);
  }

  // 6 stack Things appended after cards.
  std::vector<Thing> stacks =
    make_scopa_stacks(bottom_player, show_opponent_hand);
  std::vector<int> stack_ids;
  for (Thing& stack : stacks) {
    stack_ids.push_back(add_thing(table, std::move(stack)));
  }

  // Populate stack children from game state.
  auto set_children = [&](const char* name, const std::vector<int>& cards) {
    table.things[find_thing(table, name)]._children = cards;
  };
  set_children("p0_hand", state.players[0].hand);
  set_children("p1_hand", state.players[1].hand);
  set_children("p0_captured", state.players[0].captured);
  set_children("p1_captured", state.players[1].captured);
  set_children("stock", state.stock);
  set_children("table", state.table);

  // Root: a wooden table surface owning all stacks as direct children.
  auto root = create_table_root(
    tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
  );
  root._children = stack_ids;
  table.root = add_thing(table, std::move(root));
  for (int stack_id : stack_ids) {
    update_children_positions(stack_id, table, false);
  }
  return table;
}

static void create_coupling_between_table_and_game(
  const Table_State& table, scopa::Game_State& state
) {
  auto thing_from_card = std::unordered_map<int, int>{};
  auto card_from_thing = std::unordered_map<int, int>{};
  for (int i = 0; i < (int)state.all_cards.size(); i++) {
    /* code */
  }
}
static void update_stacks(Table_State& table, scopa::Game_State& state) {
  auto refresh = [&](const char* name, const std::vector<int>& cards) {
    int stack_id                     = find_thing(table, name);
    table.things[stack_id]._children = cards;
    update_children_positions(stack_id, table, false);
  };
  refresh("p0_hand", state.players[0].hand);
  refresh("p1_hand", state.players[1].hand);
  refresh("p0_captured", state.players[0].captured);
  refresh("p1_captured", state.players[1].captured);
  refresh("stock", state.stock);
  refresh("table", state.table);
}

#include <game/mcts.h>
static Agent* make_ai_opponent() {
  return new Agent_MCTS_Stochastic<scopa::Game_State>(
    /* num_iterations       */ 100000,
    /* rollout_depth        */ 60,
    /* num_samples          */ 20,
    /* exploration_constant */ 1.41421356f,
    /* time_budget_seconds  */ 3.0f
  );
}

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto        inputs      = Input_Feed(Input_Mode::Live, "");
  Menu_Result menu_result = run_menu(
    "Scopa Scientifica",
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    inputs,
    argc,
    argv,
    options.skip_menu,
    options.seed
  );

  scopa::Game_State state    = scopa::quick_setup(menu_result.seed);
  auto              ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
  const int         bottom_player = menu_result.player_index;
  // Show the opponent's hand only in hot-seat (one screen is shared).
  const bool  show_opponent_hand = !options.vs_ai && !menu_result.is_online();
  Table_State table =
    init_table_state(state, ui_state, bottom_player, show_opponent_hand);

  // Per-player HUD overlay. Drawn on top of every frame via the -1 callback.
  table.draw_callbacks[-1] =
    [&, bottom_player](const Table_State&, const Input&, bool) {
      for (int i = 0; i < 2; ++i) {
        bool is_current = (i == state.current_player);
        int  hud_y      = (i == bottom_player) ? (tt::WINDOW_HEIGHT - 56) : 16;
        draw_scopa_player_hud(state, i, is_current, hud_y);
      }
    };

  auto   agent_ui = Scopa_Agent_UI(&table, &ui_state, menu_result.player_index);
  Agent* agent =
    make_agent_pair(&agent_ui, make_ai_opponent(), menu_result, options.vs_ai);

  play_game(
    state,
    table,
    ui_state,
    *agent,
    inputs,
    menu_result,
    "Scopa Scientifica",
    [&] { update_stacks(table, state); },
    [&] {
      return std::vector<int>{
        scopa::compute_player_score(state, 0),
        scopa::compute_player_score(state, 1),
      };
    }
  );
  return 0;
}
