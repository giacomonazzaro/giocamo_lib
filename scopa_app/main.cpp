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
    Thing thing = make_card(card.id);
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
    stack.id = (int)table.things.size();
    stack_ids.push_back(stack.id);
    table.things.push_back(std::move(stack));
  }

  // Populate stack children from game state.
  int base                                       = (int)state.all_cards.size();
  table.things[base + SCOPA_HAND_0].children     = state.players[0].hand;
  table.things[base + SCOPA_HAND_1].children     = state.players[1].hand;
  table.things[base + SCOPA_CAPTURED_0].children = state.players[0].captured;
  table.things[base + SCOPA_CAPTURED_1].children = state.players[1].captured;
  table.things[base + SCOPA_STOCK_IDX].children  = state.stock;
  table.things[base + SCOPA_TABLE_IDX].children  = state.table;

  // Root: owns all stacks as direct children.
  Thing root;
  root.name = "root";
  // Root is centered on the screen, so its bounding rect spans (0,0)-(W,H)
  // in world coords.
  root.size        = {(float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT};
  root.transform.x = (float)tt::WINDOW_WIDTH / 2.0f;
  root.transform.y = (float)tt::WINDOW_HEIGHT / 2.0f;
  root.id          = (int)table.things.size();
  root.children    = stack_ids;
  // Wooden table surface stretched across the full window.
  root.image_path = "tabletop/data/wood.png";
  table.things.push_back(root);
  table.root = root.id;

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
  int  base    = (int)state.all_cards.size();
  auto refresh = [&](int idx, const std::vector<int>& cards) {
    int stack_id                    = base + idx;
    table.things[stack_id].children = cards;
    update_children_positions(stack_id, table, false);
  };
  refresh(SCOPA_HAND_0, state.players[0].hand);
  refresh(SCOPA_HAND_1, state.players[1].hand);
  refresh(SCOPA_CAPTURED_0, state.players[0].captured);
  refresh(SCOPA_CAPTURED_1, state.players[1].captured);
  refresh(SCOPA_STOCK_IDX, state.stock);
  refresh(SCOPA_TABLE_IDX, state.table);
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

  auto agent_ui = Scopa_Agent_UI(
    &table, &ui_state, menu_result.player_index, (int)state.all_cards.size()
  );
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
