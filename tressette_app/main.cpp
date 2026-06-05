#include <game/agent.h>
#include <game/game.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>
#include <tressette/ai.h>
#include <tressette/gameplay.h>
#include <tressette/models.h>
#include <tressette/neural_agent.h>

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
  tressette::Game_State& state,
  UI_State&              ui_state,
  int                    bottom_player,
  bool                   show_opponent_hand
) {
  auto table            = Table_State();
  table.is_drop_allowed = [](int, int, int) { return false; };

  // One Thing per card; ids 0..39 match all_cards indices.
  for (const auto& c : state.all_cards) {
    auto t = make_card(c.id);
    if (c.suit == tressette::Suit::COPPE) t.color = {50, 100, 50, 255};
    if (c.suit == tressette::Suit::DENARI) t.color = {150, 120, 20, 255};
    if (c.suit == tressette::Suit::BASTONI) t.color = {80, 50, 50, 255};
    if (c.suit == tressette::Suit::SPADE) t.color = {70, 80, 150, 255};

    table.things.push_back(t);
    table.draw_callbacks[c.id] = make_card_draw_callback(state, ui_state, c.id);
  }

  // 6 stack Things appended after cards.
  std::vector<Thing> stacks =
    make_tressette_stacks(bottom_player, show_opponent_hand);
  auto stack_ids = std::vector<int>();
  for (Thing& s : stacks) {
    s.id = (int)table.things.size();
    stack_ids.push_back(s.id);
    table.things.push_back(std::move(s));
  }

  // Populate stack children from game state.
  int base                                       = (int)state.all_cards.size();
  table.things[base + TRESSETTE_HAND_0].children = state.players[0].hand;
  table.things[base + TRESSETTE_HAND_1].children = state.players[1].hand;
  table.things[base + TRESSETTE_TRICKS_0].children =
    state.players[0].tricks_won;
  table.things[base + TRESSETTE_TRICKS_1].children =
    state.players[1].tricks_won;
  table.things[base + TRESSETTE_STOCK_IDX].children = state.stock;
  table.things[base + TRESSETTE_TABLE_IDX].children = state.trick;

  // Root: a wooden table surface owning all stacks as direct children.
  auto root = create_table_root(
    tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
  );
  root.id       = (int)table.things.size();
  root.children = stack_ids;
  table.things.push_back(root);
  table.root = root.id;
  for (int stack_id : stack_ids) {
    update_children_positions(stack_id, table, false);
  }
  return table;
}

static void update_stacks(Table_State& table, tressette::Game_State& state) {
  int  base    = (int)state.all_cards.size();
  auto refresh = [&](int idx, const std::vector<int>& cards) {
    int thing_id                    = base + idx;
    table.things[thing_id].children = cards;
  };
  refresh(TRESSETTE_HAND_0, state.players[0].hand);
  refresh(TRESSETTE_HAND_1, state.players[1].hand);
  refresh(TRESSETTE_TRICKS_0, state.players[0].tricks_won);
  refresh(TRESSETTE_TRICKS_1, state.players[1].tricks_won);
  refresh(TRESSETTE_STOCK_IDX, state.stock);
  refresh(TRESSETTE_TABLE_IDX, state.trick);

  update_local_transforms_to_match_world_transforms(table);
  for (size_t i = 0; i < table.things.size(); i++) {
    update_children_positions(i, table, true);
  }
}

// The opponent agent picked for solo (vs-AI) play. Hides the TORCH_AVAILABLE
// fork so the caller doesn't have to know about it.
#include <game/mcts.h>
static Agent* make_ai_opponent() {
#ifdef TORCH_AVAILABLE
  return new tressette::Agent_Minimax_Neural(
    "tressette/tressette_value_traced.pt", 3, 20
  );
#else
  return new Agent_MCTS_Stochastic<tressette::Game_State>(
    /* num_iterations       */ 1000000,
    /* rollout_depth        */ 40,
    /* num_samples          */ 50,
    /* exploration_constant */ 1.41421356f,
    /* time_budget_seconds  */ 5.0f
  );
#endif
}

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  // Menu opens its own window; play_game reuses it via run_tabletop's
  // IsWindowReady() guard. run_menu handles --local-host /
  // --local-join, skip-menu fallback, and the menu itself, and folds the
  // CLI seed into the result for solo play.
  auto inputs      = Input_Feed(Input_Mode::Live, "");
  auto menu_result = run_menu(
    "Tressette",
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    inputs,
    argc,
    argv,
    options.skip_menu,
    options.seed
  );

  auto state    = tressette::quick_setup(menu_result.seed);
  auto ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
  // The local player always sits at the bottom of the screen; in online play
  // that may be seat 1, in solo it's always seat 0. Show the opponent's hand
  // only in hot-seat (where one screen is shared).
  const int  bottom_player      = menu_result.player_index;
  const bool show_opponent_hand = !options.vs_ai && !menu_result.is_online();
  auto       table =
    init_table_state(state, ui_state, bottom_player, show_opponent_hand);

  state.human_player = bottom_player;

  // Per-player HUD overlay. Drawn on top of every frame via the -1 callback.
  table.draw_callbacks[-1] =
    [&, bottom_player](const Table_State&, const Input&, bool) {
      for (int i = 0; i < 2; ++i) {
        int  score      = tressette::compute_player_score(state, i);
        bool is_current = (i == state.current_player);
        int  hud_y      = (i == bottom_player) ? (tt::WINDOW_HEIGHT - 56) : 16;
        draw_tressette_player_hud(i, score, is_current, hud_y);
      }
    };

  auto agent_ui = Tressette_Agent_UI(
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
    "Tressette",
    [&] { update_stacks(table, state); },
    [&] {
      return std::vector<int>{
        tressette::compute_player_score(state, 0),
        tressette::compute_player_score(state, 1),
      };
    }
  );
  return 0;
}
