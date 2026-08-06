// Sanity checks that the search plays forced/obvious best moves, to tell a real
// search bug apart from mere evaluation weakness. Each case sets up a position
// by hand and checks the agent picks the winning move. Exits non-zero on any
// failure (so it works even when asserts are compiled out).

#include <chess/ai.h>
#include <chess/gameplay.h>
#include <chess/models.h>
#include <game/minimax.h>

#include <cstdio>

using namespace chess;

// The move the depth-`depth` agent chooses in `game`.
static Move agent_move(Game_State& game, int depth) {
  auto agent = Agent_Minimax<Game_State>(depth, 1);
  game.begin_game();  // The position is built by hand here.
  int       index = agent.choose_action(game, pending_choice(game));
  Move_List moves = legal_moves(game);
  return moves[index];
}

static Game_State empty_position(int player_to_move) {
  Game_State game;  // Constructor fills the board with EMPTY.
  game.current_player            = player_to_move;
  game.white_can_castle_kingside = game.white_can_castle_queenside = false;
  game.black_can_castle_kingside = game.black_can_castle_queenside = false;
  return game;
}

int main() {
  bool all_ok = true;

  // Case 1: white rook a1 can take an undefended black queen on a8.
  {
    Game_State game  = empty_position(0);
    game.board[0][4] = make_piece(KING, 0);   // white king e1.
    game.board[7][4] = make_piece(KING, 1);   // black king e8.
    game.board[0][0] = make_piece(ROOK, 0);   // white rook a1.
    game.board[7][0] = make_piece(QUEEN, 1);  // black queen a8.
    Move move        = agent_move(game, 6);
    bool ok          = move.from == 0 && move.to == 56;
    all_ok           = all_ok && ok;
    std::printf(
      "case 1 (win free queen): Ra1xa8 expected, got %d->%d  %s\n",
      move.from,
      move.to,
      ok ? "OK" : "FAILED"
    );
  }

  // Case 2: back-rank mate. Black king g8 is boxed by its own f7/g7/h7 pawns;
  // Ra1-a8 is mate.
  {
    Game_State game  = empty_position(0);
    game.board[0][4] = make_piece(KING, 0);  // white king e1.
    game.board[0][0] = make_piece(ROOK, 0);  // white rook a1.
    game.board[7][6] = make_piece(KING, 1);  // black king g8.
    game.board[6][5] = make_piece(PAWN, 1);  // black pawn f7.
    game.board[6][6] = make_piece(PAWN, 1);  // black pawn g7.
    game.board[6][7] = make_piece(PAWN, 1);  // black pawn h7.
    Move move        = agent_move(game, 6);
    bool ok          = move.from == 0 && move.to == 56;
    all_ok           = all_ok && ok;
    std::printf(
      "case 2 (mate in 1): Ra1-a8# expected, got %d->%d  %s\n",
      move.from,
      move.to,
      ok ? "OK" : "FAILED"
    );
  }

  std::printf(all_ok ? "tactical test passed\n" : "tactical test FAILED\n");
  return all_ok ? 0 : 1;
}
