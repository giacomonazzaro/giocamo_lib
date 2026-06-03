#include <connect_four/ai.h>
#include <connect_four/gameplay.h>
#include <connect_four/models.h>
#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
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

// Column thing-ids start right after the ROWS*COLS disc Things.
static const int COLUMNS_OFFSET = connect_four::ROWS * connect_four::COLS;

// Push the board into the table: each column owns the discs in its filled
// slots, positioned bottom-to-top, and each disc is coloured by its owner.
// Empty slots' discs stay detached (orphans aren't drawn).
static void update_board(Table_State& table, connect_four::Game_State& state) {
  const float cell = (float)CONNECT_FOUR_CELL;
  for (int col = 0; col < connect_four::COLS; ++col) {
    std::vector<int> children;
    for (int row = 0; row < connect_four::ROWS; ++row) {
      int value = state.board[row][col];
      if (value == connect_four::EMPTY) continue;
      int    disc_id   = row * connect_four::COLS + col;
      Thing& disc      = table.things[disc_id];
      disc.color       = connect_four_disc_color(value);
      // Position in column-local space: row 0 sits at the bottom.
      disc.transform.x = 0.0f;
      disc.transform.y =
        (float)(connect_four::ROWS - 1) * cell / 2.0f - (float)row * cell;
      children.push_back(disc_id);
    }
    table.things[COLUMNS_OFFSET + col].children = children;
  }
}

static Table_State init_table_state(connect_four::Game_State& state) {
  Table_State table;
  table.is_drop_allowed = [](int, int, int) { return false; };

  // One disc Thing per board slot; id = row*COLS + col. Discs start detached
  // from any column, so the empty board shows none of them.
  for (int row = 0; row < connect_four::ROWS; ++row) {
    for (int col = 0; col < connect_four::COLS; ++col) {
      Thing disc = make_card(row * connect_four::COLS + col);
      disc.size  = {(float)CONNECT_FOUR_DISC, (float)CONNECT_FOUR_DISC};
      table.things.push_back(disc);
    }
  }

  // 7 column Things after the discs.
  std::vector<Thing> columns = make_connect_four_columns();
  std::vector<int>   column_ids;
  for (Thing& column : columns) {
    column.id = (int)table.things.size();
    column_ids.push_back(column.id);
    table.things.push_back(std::move(column));
  }

  auto root =
    create_table_root(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png");
  root.id       = (int)table.things.size();
  root.children = column_ids;
  table.things.push_back(root);
  table.root = root.id;

  update_board(table, state);
  return table;
}

// Connect Four is perfect-information, so plain MCTS (no determinization /
// sample_state) is the right fit. Strength/speed tune via iterations + budget.
static Agent* make_ai_opponent() {
  return new Agent_MCTS<connect_four::Game_State>(
    /* num_iterations       */ 20000,
    /* rollout_depth        */ 64,
    /* exploration_constant */ 1.41421356f,
    /* time_budget_seconds  */ 1.0f
  );
}

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto inputs      = Input_Feed(Input_Mode::Live, "");
  auto menu_result = run_menu(
    "Connect Four",
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    inputs,
    argc,
    argv,
    options.skip_menu,
    options.seed
  );

  auto state    = connect_four::quick_setup(menu_result.seed);
  auto ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
  auto table    = init_table_state(state);

  // Per-frame overlay. Connect Four is click-only, so cancel any drag the
  // table-top started this frame; snap discs straight to their slots (no glide
  // from the orphan origin); then draw the turn/winner HUD.
  table.draw_callbacks[-1] = [&](const Table_State&, const Input&, bool) {
    table.drag_state = Drag_State();
    for (int disc_id = 0; disc_id < COLUMNS_OFFSET; ++disc_id) {
      table.world_transforms_animated[disc_id] =
        table.world_transforms[disc_id];
    }
    draw_connect_four_hud(state);
  };

  auto agent_ui = Connect_Four_Agent_UI(
    &table, &ui_state, menu_result.player_index, COLUMNS_OFFSET
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
    "Connect Four",
    [&] { update_board(table, state); },
    [&] {
      return std::vector<int>{
        connect_four::compute_player_score(state, 0),
        connect_four::compute_player_score(state, 1),
      };
    }
  );
  return 0;
}
