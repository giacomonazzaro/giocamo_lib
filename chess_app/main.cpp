#include <chess/ai.h>
#include <chess/gameplay.h>
#include <chess/models.h>
#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
#include <game/minimax.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <vector>

#include "agent_ui.h"
#include "ui.h"

// One square Thing per board slot, with thing-id == board index (squares are
// appended first), then a screen-filling root that parents all 64 squares.
static Table_State init_table_state() {
  Table_State table;
  table.is_drop_allowed = [](int, int, int) { return false; };

  std::vector<Thing> squares = make_chess_squares();
  std::vector<int>   square_ids;
  for (Thing& square : squares) {
    square.id = (int)table.things.size();  // 0..63 == row*8 + col.
    square_ids.push_back(square.id);
    table.things.push_back(std::move(square));
  }

  auto root = create_table_root(
    tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
  );
  root.id        = (int)table.things.size();
  root._children = square_ids;
  table.things.push_back(root);
  table.root = root.id;

  return table;
}

// Draw a single piece glyph centered on its square's on-screen position.
static void draw_piece(const Table_State& table, int square, int piece) {
  const char* glyph = chess_piece_glyph(piece);
  if (glyph[0] == '\0') return;
  const int   size = 52;
  const float x    = table.world_transforms[square].x -
                  (float)text_width(glyph, size) / 2.0f;
  const float y = table.world_transforms[square].y - (float)size / 2.0f;
  render_text(glyph, x, y, size, chess_piece_color(piece));
}

// Iterative-deepening alpha-beta with a wall-clock budget: it plays much more
// soundly than MCTS in this tactical game, deepening the search until the time
// runs out and playing the best move from the deepest completed depth.
static Agent* make_ai_opponent() {
  return new Agent_Minimax_Timed<chess::Game_State>(/* time_budget_seconds */ 6.0f);
}

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto inputs      = Input_Feed(Input_Mode::Live, "");
  auto menu_result = run_menu(
    "Chess",
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    inputs,
    argc,
    argv,
    options.skip_menu,
    options.seed
  );

  auto state    = chess::quick_setup(menu_result.seed);
  auto ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
  auto table    = init_table_state();

  auto agent_ui = Chess_Agent_UI(&table, &ui_state, menu_result.player_index);

  // Per-frame overlay: highlight the picked piece and its legal destinations,
  // draw every piece as a letter, then the turn/result HUD.
  table.draw_callbacks[-1] = [&](const Table_State&, const Input&, bool) {
    // Chess is click-only, so cancel any drag the table-top started this frame
    // (otherwise pressing a square would drag it) and keep the squares pinned
    // to their grid positions.
    table.drag_state = Drag_State();
    for (int square = 0; square < 64; ++square) {
      table.world_transforms_animated[square] = table.world_transforms[square];
    }

    if (agent_ui.selected_square >= 0) {
      // Outline the selected square; mark each legal destination from it.
      const float half = (float)CHESS_CELL / 2.0f;
      float       sx   = table.world_transforms[agent_ui.selected_square].x;
      float       sy   = table.world_transforms[agent_ui.selected_square].y;
      DrawRectangleLinesEx(
        Rectangle{sx - half, sy - half, (float)CHESS_CELL, (float)CHESS_CELL},
        4.0f,
        Color{60, 180, 90, 255}
      );
      for (const chess::Move& move : chess::legal_moves(state)) {
        if (move.from != agent_ui.selected_square) continue;
        float dx = table.world_transforms[move.to].x;
        float dy = table.world_transforms[move.to].y;
        DrawCircleV(Vector2{dx, dy}, 14.0f, Color{60, 180, 90, 160});
      }
    }

    for (int row = 0; row < 8; ++row) {
      for (int col = 0; col < 8; ++col) {
        int piece = state.board[row][col];
        if (piece != chess::EMPTY) draw_piece(table, row * 8 + col, piece);
      }
    }

    draw_chess_hud(state);
  };

  Agent* agent =
    make_agent_pair(&agent_ui, make_ai_opponent(), menu_result, options.vs_ai);

  play_game(
    state,
    table,
    ui_state,
    *agent,
    inputs,
    menu_result,
    "Chess",
    [] {},  // Pieces are drawn from the game state each frame, so no sync step.
    [&] {
      return std::vector<int>{
        chess::compute_player_score(state, 0),
        chess::compute_player_score(state, 1),
      };
    }
  );
  return 0;
}
