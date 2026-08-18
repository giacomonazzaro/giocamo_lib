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

#include <algorithm>
#include <string>
#include <vector>

#include <struct/imgui.h>  // for draw_editor_ui()
#include <struct/json.h>   // for to_json()

#include "agent_ui.h"
#include "ui.h"

// Square Things hold ids 0..63 (id == board index). The piece Things follow at
// 64..95: a pool of 32, one per piece that can be on the board at once. The two
// rows that hold the taken pieces come after them.
static const int PIECE_THING_BASE  = 64;
static const int PIECE_THING_COUNT = 32;
static const int TAKEN_ROW_THING[2] = {
  PIECE_THING_BASE + PIECE_THING_COUNT,      // White pieces, left of the board.
  PIECE_THING_BASE + PIECE_THING_COUNT + 1,  // Black pieces, right of it.
};

// How wide a row of taken pieces is, and how far apart the pieces sit in it.
// A full row of 16 does not fit at this spacing, so they overlap — the row
// shrinks the spacing itself once they no longer fit.
static const float TAKEN_ROW_WIDTH  = 470.0f;
static const float TAKEN_ROW_SPREAD = 30.0f;

// Piece identity, kept across moves so a piece keeps its Thing as it travels
// and the renderer slides it. There is one board per process, so file-static is
// fine.
namespace {
int thing_for_square[64];  // Pool Thing on each square, or -1.
int
  square_of_thing[PIECE_THING_COUNT];  // Square each pool Thing sits on, or -1.
int value_of_thing[PIECE_THING_COUNT];  // Piece value each pool Thing shows, 0
                                        // if it has never held one.
}  // namespace

// Root-local center of a board square. Pieces are root children, so they share
// the squares' coordinate space; this matches make_chess_squares().
static Transform2D square_transform(int square) {
  int row = square / 8;
  int col = square % 8;
  return Transform2D{
    ((float)col - 3.5f) * (float)CHESS_CELL,
    (3.5f - (float)row) * (float)CHESS_CELL,
    0.0f,
  };
}

// Image file for a piece value, e.g. white knight -> "...pieces/wN.png". The
// "w"/"b" prefix is the color and the glyph letter is the piece type.
static std::string piece_image_path(int value) {
  std::string color = chess::piece_color(value) == 0 ? "w" : "b";
  return "chess_app/data/pieces/" + color + chess_piece_glyph(value) + ".png";
}

// Iterative-deepening alpha-beta with a wall-clock budget: it plays much more
// soundly than MCTS in this tactical game, deepening the search until the time
// runs out and playing the best move from the deepest completed depth.
static Agent* make_minimax_agent() {
  return new Agent_Minimax<chess::Game_State>(
    6  // max_depth
  );
}

// MCTS whose leaves are scored by a shallow alpha-beta search.
static Agent* make_mcts_agent() {
  auto* agent = new Agent_MCTS<chess::Game_State>(
    /* num_iterations       */ 9999999,
    /* rollout_depth        */ 40,
    /* exploration_constant */ 1.41421356f,
    /* time_budget_seconds  */ 8.0f
  );
  const int minimax_depth = 2;
  agent->leaf_evaluator =
    [minimax_depth](const chess::Game_State& state, int player) {
      const float       infinity = std::numeric_limits<float>::infinity();
      chess::Game_State copy     = state;  // minimax needs a mutable copy.
      return minimax_detail::minimax(
        copy, minimax_depth, -infinity, infinity, player, [] { return false; }
      );
    };
  return agent;
}

// Chess on the table. The table is laid out once here; play_game sets the
// position up and drives the loop through these hooks.
struct Chess_Giocamo : Giocamo_With_History<chess::Game_State> {
  // --watch: the two bots play each other (White = minimax, Black = MCTS) and
  // we just spectate.
  bool watch = false;

  Chess_Giocamo(chess::Game_State& game, Chess_Agent_UI& agent_ui)
      : Giocamo_With_History<chess::Game_State>(game, agent_ui) {}

  chess::Game_State& chess_game() {
    return static_cast<chess::Game_State&>(game);
  }
  const chess::Game_State& chess_game() const {
    return static_cast<const chess::Game_State&>(game);
  }

  Chess_Agent_UI& chess_agent_ui() {
    return static_cast<Chess_Agent_UI&>(agent_ui);
  }

  // One square Thing per board slot (id == board index), 32 detached piece
  // Things, then a screen-filling root that parents the squares.
  void init_table() override {
    table.is_drop_allowed = [](int, int, int) { return false; };

    std::vector<Thing> squares = make_chess_squares();
    std::vector<int>   square_ids;
    for (Thing& square : squares) {
      // Ends up at 0..63 == row*8 + col.
      square_ids.push_back(add_thing(table, std::move(square)));
    }

    // Piece Things: a square body that draws a piece image (set in
    // update_table_from_game). A zero corner radius keeps the renderer from
    // rounding the texture. They are children of the root, listed after the
    // squares so every piece draws on top of every square — otherwise a sliding
    // piece would be hidden behind squares drawn later in the tree.
    Shape piece_shape =
      Shape_Rectangle{{(float)CHESS_CELL, (float)CHESS_CELL}, 0.0f};
    for (int i = 0; i < PIECE_THING_COUNT; ++i) {
      Thing piece;
      piece.name  = "piece" + std::to_string(i);
      // Lands at PIECE_THING_BASE + i.
      piece.shape = piece_shape;
      piece.color = Color{0, 0, 0, 0};  // Only used if the image fails to load.
      table.things.push_back(piece);
    }

    // A row of taken pieces on each flank of the board, in root-local coords
    // (the root is centered, so the board spans -360..360). White's taken
    // pieces go on the left, black's on the right.
    const float board_half = 4.0f * (float)CHESS_CELL;
    const float row_y      = -(float)CHESS_CELL / 2.0f;
    Rectangle   white_row  = {
      -(board_half + 20.0f + TAKEN_ROW_WIDTH),
      row_y,
      TAKEN_ROW_WIDTH,
      (float)CHESS_CELL
    };
    Rectangle black_row = {
      board_half + 20.0f, row_y, TAKEN_ROW_WIDTH, (float)CHESS_CELL
    };
    // Land at TAKEN_ROW_THING[0] and [1].
    add_thing(
      table,
      make_container_thing(
        white_row, TAKEN_ROW_SPREAD, 0.0f, true, "white_taken"
      )
    );
    add_thing(
      table,
      make_container_thing(
        black_row, TAKEN_ROW_SPREAD, 0.0f, true, "black_taken"
      )
    );

    auto root = create_table_root(
      tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
    );
    // update_table_from_game fills in the rest: the two rows and the pieces
    // still on the board.
    root._children = square_ids;
    table.root     = add_thing(table, std::move(root));

    for (int square = 0; square < 64; ++square) thing_for_square[square] = -1;
    for (int i = 0; i < PIECE_THING_COUNT; ++i) {
      square_of_thing[i] = -1;
      value_of_thing[i]  = 0;
    }

    // Per-frame overlay: cancel any table-top drag (chess is click-only) and
    // pin the squares, highlight the picked piece's legal destinations, then the
    // HUD. Pieces are Things now, so they're drawn and animated by the renderer
    // itself.
    table.draw_callbacks[-1] = [this](const Table_State&, const Input&, bool) {
      chess::Game_State& state = this->chess_game();
      table.drag_state         = Drag_State();
      for (int square = 0; square < 64; ++square) {
        table.world_transforms_animated[square] =
          table.world_transforms[square];
      }

      const int selected = this->chess_agent_ui().selected_square;
      if (selected >= 0) {
        const float half = (float)CHESS_CELL / 2.0f;
        float       sx   = table.world_transforms[selected].x;
        float       sy   = table.world_transforms[selected].y;
        DrawRectangleLinesEx(
          Rectangle{sx - half, sy - half, (float)CHESS_CELL, (float)CHESS_CELL},
          4.0f,
          Color{60, 180, 90, 255}
        );
        for (const chess::Move& move : chess::legal_moves(state)) {
          if (move.from != selected) continue;
          float dx = table.world_transforms[move.to].x;
          float dy = table.world_transforms[move.to].y;
          DrawCircleV(Vector2{dx, dy}, 14.0f, Color{60, 180, 90, 160});
        }
      }

      draw_chess_hud(state);
    };
  }

  // Reconcile the piece Things with the board: the moving piece keeps its Thing
  // (matched by value) and is repositioned onto the destination square, which is
  // what makes the renderer slide it there.
  void update_table_from_game() override {
    chess::Game_State& state = this->chess_game();

    // Release every Thing whose square no longer holds its piece; remember them
    // as free so the squares that still need a piece can reuse them.
    int freed[PIECE_THING_COUNT];
    int freed_count = 0;
    for (int i = 0; i < PIECE_THING_COUNT; ++i) {
      int square = square_of_thing[i];
      if (square < 0) continue;
      if (state.board[square / 8][square % 8] != value_of_thing[i]) {
        thing_for_square[square] = -1;
        square_of_thing[i]       = -1;
        freed[freed_count++]     = i;
      }
    }

    // Fill each square that needs a piece. Prefer a freed Thing of the same
    // value (the piece that actually moved — it slides over), then any freed
    // Thing (a promotion reuses the pawn's Thing), then a piece taken earlier
    // that is back on the board (an undo), then a spare (first placement).
    bool used[PIECE_THING_COUNT];
    for (int k = 0; k < freed_count; ++k) used[k] = false;
    for (int square = 0; square < 64; ++square) {
      int value = state.board[square / 8][square % 8];
      if (value == 0 || thing_for_square[square] != -1) continue;

      int chosen = -1;
      for (int k = 0; k < freed_count; ++k) {
        if (!used[k] && value_of_thing[freed[k]] == value) {
          chosen  = freed[k];
          used[k] = true;
          break;
        }
      }
      if (chosen < 0) {
        for (int k = 0; k < freed_count; ++k) {
          if (!used[k]) {
            chosen  = freed[k];
            used[k] = true;
            break;
          }
        }
      }
      if (chosen < 0) {
        for (int i = 0; i < PIECE_THING_COUNT; ++i) {
          if (square_of_thing[i] < 0 && value_of_thing[i] == value) {
            chosen = i;
            break;
          }
        }
      }
      if (chosen < 0) {
        for (int i = 0; i < PIECE_THING_COUNT; ++i) {
          if (square_of_thing[i] < 0 && value_of_thing[i] == 0) {
            chosen = i;
            break;
          }
        }
      }
      if (chosen < 0) continue;  // Pool exhausted — cannot happen with 32
                                 // pieces.

      value_of_thing[chosen]   = value;
      square_of_thing[chosen]  = square;
      thing_for_square[square] = chosen;
      Thing& piece             = table.things[PIECE_THING_BASE + chosen];
      piece.image_path         = piece_image_path(value);
      // Setting the target square moves the Thing; the renderer slides it from
      // wherever it was (the source square) to here.
      piece.transform = square_transform(square);
    }

    // A Thing that holds a piece but sits on no square was taken. It keeps its
    // image and goes to the row for its colour; the renderer slides it there
    // from the square it was taken on.
    auto on_board = std::vector<int>();
    auto taken    = std::vector<std::vector<int>>(2);
    for (int i = 0; i < PIECE_THING_COUNT; ++i) {
      int value = value_of_thing[i];
      if (value == 0) continue;  // Never held a piece.
      if (square_of_thing[i] >= 0) {
        on_board.push_back(PIECE_THING_BASE + i);
      } else {
        taken[chess::piece_color(value)].push_back(PIECE_THING_BASE + i);
      }
    }

    // Strongest piece first, so a row reads the same however the Things were
    // handed out.
    for (int color = 0; color < 2; ++color) {
      std::sort(
        taken[color].begin(),
        taken[color].end(),
        [](int a, int b) {
          return chess::piece_type(value_of_thing[a - PIECE_THING_BASE]) >
                 chess::piece_type(value_of_thing[b - PIECE_THING_BASE]);
        }
      );
      table.things[TAKEN_ROW_THING[color]]._children = taken[color];
      update_children_positions(TAKEN_ROW_THING[color], table, false);
    }

    // The squares, then the two rows, then the pieces still on the board, so a
    // piece always draws on top of the square it stands on.
    std::vector<int>& root_children = table.things[table.root]._children;
    root_children.clear();
    for (int square = 0; square < 64; ++square) root_children.push_back(square);
    root_children.push_back(TAKEN_ROW_THING[0]);
    root_children.push_back(TAKEN_ROW_THING[1]);
    for (int thing : on_board) root_children.push_back(thing);
  }

  // Nothing is draggable, so the table never holds an arrangement the game
  // does not already have.
  void update_game_from_table() override {}

  // Watch mode: Black is the MCTS bot. Otherwise the human plays against the
  // minimax bot.
  Agent* agent_opponent() override {
    if (watch) return new Agent_Async(make_mcts_agent());
    return new Agent_Async(make_minimax_agent());
  }

  // Watch mode: White is the minimax bot instead of the player.
  Agent* agent_player() override {
    if (watch) return new Agent_Async(make_minimax_agent());
    return &agent_ui;
  }

  std::vector<int> player_scores() const override {
    return {
      chess::compute_player_score(this->chess_game(), 0),
      chess::compute_player_score(this->chess_game(), 1),
    };
  }
};

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto game     = chess::Game_State();
  auto agent_ui = Chess_Agent_UI();
  auto giocamo  = Chess_Giocamo(game, agent_ui);

  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--watch") giocamo.watch = true;
  }
  // Nothing to choose when both seats are bots.
  if (giocamo.watch) options.skip_menu = true;

  play_game(giocamo, options, "Chess");
  return 0;
}
