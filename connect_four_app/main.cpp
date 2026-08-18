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

#include <struct/imgui.h>  // for draw_editor_ui()
#include <struct/json.h>   // for to_json()

#include "agent_ui.h"
#include "ui.h"

// Connect Four on the table. The table is laid out once here; play_game deals
// the game and drives the loop through these hooks.
struct Connect_Four_Giocamo : Giocamo_With_History<connect_four::Game_State> {
  Connect_Four_Giocamo(
    connect_four::Game_State& game, Connect_Four_Agent_UI& agent_ui
  )
      : Giocamo_With_History<connect_four::Game_State>(game, agent_ui) {}

  connect_four::Game_State& board_game() {
    return static_cast<connect_four::Game_State&>(game);
  }
  const connect_four::Game_State& board_game() const {
    return static_cast<const connect_four::Game_State&>(game);
  }

  void init_table() override {
    table.is_drop_allowed = [](int, int, int) { return false; };

    // One disc Thing per board slot; id = row*COLS + col. Discs start detached
    // from any column, so the empty board shows none of them.
    for (int row = 0; row < connect_four::ROWS; ++row) {
      for (int col = 0; col < connect_four::COLS; ++col) {
        Thing disc = make_card();
        disc.shape = circle_shape((float)CONNECT_FOUR_DISC);
        table.things.push_back(disc);
      }
    }

    // 7 column Things after the discs.
    std::vector<Thing> columns = make_connect_four_columns();
    std::vector<int>   column_ids;
    for (Thing& column : columns) {
      column_ids.push_back(add_thing(table, std::move(column)));
    }

    auto root = create_table_root(
      tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
    );
    root._children = column_ids;
    table.root     = add_thing(table, std::move(root));

    // Per-frame overlay. Connect Four is click-only, so cancel any drag the
    // table-top started this frame; snap discs straight to their slots (no
    // glide from the orphan origin); then draw the turn/winner HUD.
    table.draw_callbacks[-1] = [this](const Table_State&, const Input&, bool) {
      table.drag_state = Drag_State();
      for (int disc_id = 0; disc_id < COLUMNS_OFFSET; ++disc_id) {
        table.world_transforms_animated[disc_id] =
          table.world_transforms[disc_id];
      }
      draw_connect_four_hud(this->board_game());
    };
  }

  // Push the board into the table: each column owns the discs in its filled
  // slots, positioned bottom-to-top, and each disc is coloured by its owner.
  // Empty slots' discs stay detached (orphans aren't drawn).
  void update_table_from_game() override {
    connect_four::Game_State& state = this->board_game();

    const float cell = (float)CONNECT_FOUR_CELL;
    for (int col = 0; col < connect_four::COLS; ++col) {
      std::vector<int> children;
      for (int row = 0; row < connect_four::ROWS; ++row) {
        int value = state.board[row][col];
        if (value == connect_four::EMPTY) continue;
        int    disc_id = row * connect_four::COLS + col;
        Thing& disc    = table.things[disc_id];
        disc.color     = connect_four_disc_color(value);
        // Position in column-local space: row 0 sits at the bottom.
        disc.transform.x = 0.0f;
        disc.transform.y = (float)(connect_four::ROWS - 1) * cell / 2.0f -
                           (float)row * cell;
        children.push_back(disc_id);
      }
      table.things[COLUMNS_OFFSET + col]._children = children;
    }
  }

  // Nothing is draggable, so the table never holds an arrangement the game
  // does not already have.
  void update_game_from_table() override {}

  // Connect Four is perfect-information, so plain MCTS (no determinization /
  // sample_state) is the right fit. Strength/speed tune via iterations +
  // budget.
  Agent* agent_opponent() override {
    return new Agent_MCTS<connect_four::Game_State>(
      /* num_iterations       */ 20000,
      /* rollout_depth        */ 64,
      /* exploration_constant */ 1.41421356f,
      /* time_budget_seconds  */ 1.0f
    );
  }

  std::vector<int> player_scores() const override {
    return {
      connect_four::compute_player_score(this->board_game(), 0),
      connect_four::compute_player_score(this->board_game(), 1),
    };
  }
};

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto game     = connect_four::Game_State();
  auto agent_ui = Connect_Four_Agent_UI();
  auto giocamo  = Connect_Four_Giocamo(game, agent_ui);

  play_game(giocamo, options, "Connect Four");
  return 0;
}
