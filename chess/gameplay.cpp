#include "gameplay.h"

namespace chess {

// Row/column of a square, and the square of a row/column.
static int square_row(int square) { return square / 8; }
static int square_col(int square) { return square % 8; }
static int square_of(int row, int col) { return row * 8 + col; }

static bool on_board(int row, int col) {
  return row >= 0 && row < 8 && col >= 0 && col < 8;
}

// Forward direction of a player's pawns: white advances up the board (+1),
// black down (-1).
static int pawn_forward(int player) { return player == 0 ? 1 : -1; }

// Core attack test, working on the bare board so move generation can probe a
// board copy without copying the whole game state.
static bool is_square_attacked_on(
  const Board& board, int square, int by_player
) {
  int row = square_row(square);
  int col = square_col(square);

  // Pawn attacks: a by_player pawn sits one rank "behind" the square (from the
  // attacker's point of view) on an adjacent file.
  int forward    = pawn_forward(by_player);
  int pawn_value = make_piece(PAWN, by_player);
  int pawn_row   = row - forward;
  for (int d_col = -1; d_col <= 1; d_col += 2) {
    int pawn_col = col + d_col;
    if (on_board(pawn_row, pawn_col) &&
        board[pawn_row][pawn_col] == pawn_value) {
      return true;
    }
  }

  // Knight attacks.
  static const int knight_offsets[8][2] = {
    {1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}
  };
  int knight_value = make_piece(KNIGHT, by_player);
  for (const auto& offset : knight_offsets) {
    int r = row + offset[0];
    int c = col + offset[1];
    if (on_board(r, c) && board[r][c] == knight_value) return true;
  }

  // King attacks (adjacent squares).
  int king_value = make_piece(KING, by_player);
  for (int d_row = -1; d_row <= 1; ++d_row) {
    for (int d_col = -1; d_col <= 1; ++d_col) {
      if (d_row == 0 && d_col == 0) continue;
      int r = row + d_row;
      int c = col + d_col;
      if (on_board(r, c) && board[r][c] == king_value) return true;
    }
  }

  // Sliding attacks: walk each ray until the first piece. Diagonals are hit by
  // a bishop or queen, orthogonals by a rook or queen.
  static const int diagonal_dirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  static const int straight_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  int              bishop_value        = make_piece(BISHOP, by_player);
  int              rook_value          = make_piece(ROOK, by_player);
  int              queen_value         = make_piece(QUEEN, by_player);

  for (const auto& dir : diagonal_dirs) {
    int r = row + dir[0];
    int c = col + dir[1];
    while (on_board(r, c)) {
      int value = board[r][c];
      if (value != EMPTY) {
        if (value == bishop_value || value == queen_value) return true;
        break;
      }
      r += dir[0];
      c += dir[1];
    }
  }
  for (const auto& dir : straight_dirs) {
    int r = row + dir[0];
    int c = col + dir[1];
    while (on_board(r, c)) {
      int value = board[r][c];
      if (value != EMPTY) {
        if (value == rook_value || value == queen_value) return true;
        break;
      }
      r += dir[0];
      c += dir[1];
    }
  }

  return false;
}

static int find_king_on(const Board& board, int player) {
  int king_value = make_piece(KING, player);
  for (int row = 0; row < 8; ++row) {
    for (int col = 0; col < 8; ++col) {
      if (board[row][col] == king_value) return square_of(row, col);
    }
  }
  return -1;
}

bool is_square_attacked(const Game_State& state, int square, int by_player) {
  return is_square_attacked_on(state.board, square, by_player);
}

bool in_check(const Game_State& state, int player) {
  int king_square = find_king_on(state.board, player);
  if (king_square < 0) return false;
  return is_square_attacked_on(state.board, king_square, 1 - player);
}

// Append a pawn move, expanding it into the four promotion choices when it
// lands on the last rank.
static void add_pawn_move(Move_List& moves, int from, int to, int last_rank) {
  if (square_row(to) == last_rank) {
    moves.push_back(Move{from, to, QUEEN});
    moves.push_back(Move{from, to, ROOK});
    moves.push_back(Move{from, to, BISHOP});
    moves.push_back(Move{from, to, KNIGHT});
  } else {
    moves.push_back(Move{from, to, 0});
  }
}

// All moves a player could make ignoring whether they leave their own king in
// check. Castling already checks that the king is not in or passing through
// check, since the general legality filter only inspects the landing square.
static Move_List generate_pseudo_legal(const Game_State& state, int player) {
  Move_List moves;
  int       opponent   = 1 - player;
  int       forward    = pawn_forward(player);
  int       start_rank = player == 0 ? 1 : 6;
  int       last_rank  = player == 0 ? 7 : 0;

  static const int knight_offsets[8][2] = {
    {1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}
  };
  static const int diagonal_dirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  static const int straight_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

  for (int row = 0; row < 8; ++row) {
    for (int col = 0; col < 8; ++col) {
      int value = state.board[row][col];
      if (piece_color(value) != player) continue;
      int from = square_of(row, col);
      int type = piece_type(value);

      if (type == PAWN) {
        // One step forward onto an empty square.
        int one_row = row + forward;
        if (on_board(one_row, col) && state.board[one_row][col] == EMPTY) {
          add_pawn_move(moves, from, square_of(one_row, col), last_rank);
          // Two steps forward from the start rank.
          int two_row = row + 2 * forward;
          if (row == start_rank && state.board[two_row][col] == EMPTY) {
            moves.push_back(Move{from, square_of(two_row, col), 0});
          }
        }
        // Diagonal captures, including en passant.
        for (int d_col = -1; d_col <= 1; d_col += 2) {
          int c = col + d_col;
          if (!on_board(one_row, c)) continue;
          int target = square_of(one_row, c);
          if (piece_color(state.board[one_row][c]) == opponent) {
            add_pawn_move(moves, from, target, last_rank);
          } else if (target == state.en_passant_target) {
            moves.push_back(Move{from, target, 0});
          }
        }
      } else if (type == KNIGHT) {
        for (const auto& offset : knight_offsets) {
          int r = row + offset[0];
          int c = col + offset[1];
          if (on_board(r, c) && piece_color(state.board[r][c]) != player) {
            moves.push_back(Move{from, square_of(r, c), 0});
          }
        }
      } else if (type == KING) {
        for (int d_row = -1; d_row <= 1; ++d_row) {
          for (int d_col = -1; d_col <= 1; ++d_col) {
            if (d_row == 0 && d_col == 0) continue;
            int r = row + d_row;
            int c = col + d_col;
            if (on_board(r, c) && piece_color(state.board[r][c]) != player) {
              moves.push_back(Move{from, square_of(r, c), 0});
            }
          }
        }
      } else {
        // Sliding pieces: bishop diagonals, rook straights, queen both.
        const int (*dirs)[2] = type == BISHOP ? diagonal_dirs : straight_dirs;
        int  num_dirs        = 4;
        bool both            = type == QUEEN;
        for (int pass = 0; pass < (both ? 2 : 1); ++pass) {
          const int (*active)[2] =
            both ? (pass == 0 ? diagonal_dirs : straight_dirs) : dirs;
          for (int d = 0; d < num_dirs; ++d) {
            int r = row + active[d][0];
            int c = col + active[d][1];
            while (on_board(r, c)) {
              int occupant = state.board[r][c];
              if (occupant == EMPTY) {
                moves.push_back(Move{from, square_of(r, c), 0});
              } else {
                if (piece_color(occupant) == opponent) {
                  moves.push_back(Move{from, square_of(r, c), 0});
                }
                break;
              }
              r += active[d][0];
              c += active[d][1];
            }
          }
        }
      }
    }
  }

  // Castling: the king is on its home square, the squares between it and the
  // rook are empty, the king is not currently in check, and the two squares it
  // crosses are not attacked.
  int  home_row   = player == 0 ? 0 : 7;
  bool kingside   = player == 0 ? state.white_can_castle_kingside
                                : state.black_can_castle_kingside;
  bool queenside  = player == 0 ? state.white_can_castle_queenside
                                : state.black_can_castle_queenside;
  int  king_value = make_piece(KING, player);
  int  rook_value = make_piece(ROOK, player);
  bool king_in_check =
    is_square_attacked(state, square_of(home_row, 4), opponent);

  if (state.board[home_row][4] == king_value && !king_in_check) {
    if (kingside && state.board[home_row][7] == rook_value &&
        state.board[home_row][5] == EMPTY &&
        state.board[home_row][6] == EMPTY &&
        !is_square_attacked(state, square_of(home_row, 5), opponent) &&
        !is_square_attacked(state, square_of(home_row, 6), opponent)) {
      moves.push_back(Move{square_of(home_row, 4), square_of(home_row, 6), 0});
    }
    if (queenside && state.board[home_row][0] == rook_value &&
        state.board[home_row][1] == EMPTY &&
        state.board[home_row][2] == EMPTY &&
        state.board[home_row][3] == EMPTY &&
        !is_square_attacked(state, square_of(home_row, 3), opponent) &&
        !is_square_attacked(state, square_of(home_row, 2), opponent)) {
      moves.push_back(Move{square_of(home_row, 4), square_of(home_row, 2), 0});
    }
  }

  return moves;
}

// Apply only the board changes of `move` for `player`: move the piece, promote
// it, remove an en-passant-captured pawn, and hop the castling rook. This is
// the part move generation needs to test king safety, so it runs on a bare
// board (a cheap 64-byte copy) without touching the rest of the game state.
static void apply_board_move(
  Board& board, const Move& move, int player, int en_passant_target
) {
  int from_row = square_row(move.from);
  int from_col = square_col(move.from);
  int to_row   = square_row(move.to);
  int to_col   = square_col(move.to);
  int moving   = board[from_row][from_col];
  int type     = piece_type(moving);

  bool is_en_passant = type == PAWN && move.to == en_passant_target &&
                       board[to_row][to_col] == EMPTY;

  // Move the piece (promoting if a pawn reaches the last rank).
  board[from_row][from_col] = EMPTY;
  board[to_row][to_col] =
    move.promotion != 0 ? make_piece(move.promotion, player) : moving;

  // En passant removes the pawn that just double-stepped, beside the target.
  if (is_en_passant) board[from_row][to_col] = EMPTY;

  // Castling: the king moved two files, so hop the matching rook.
  if (type == KING && to_col - from_col == 2) {
    board[to_row][5] = board[to_row][7];
    board[to_row][7] = EMPTY;
  } else if (type == KING && from_col - to_col == 2) {
    board[to_row][3] = board[to_row][0];
    board[to_row][0] = EMPTY;
  }
}

// Apply `move` to the full state without switching turn or deciding the
// outcome: the board changes above, then refresh castling rights, the
// en-passant target and the 50-move clock. Used inside apply_move.
static void make_move_on_board(Game_State& state, const Move& move) {
  int player   = state.current_player;
  int from_row = square_row(move.from);
  int from_col = square_col(move.from);
  int to_row   = square_row(move.to);
  int to_col   = square_col(move.to);
  int moving   = state.board[from_row][from_col];
  int type     = piece_type(moving);
  int captured = state.board[to_row][to_col];

  bool is_pawn_move  = type == PAWN;
  bool is_en_passant = is_pawn_move && move.to == state.en_passant_target &&
                       captured == EMPTY;
  bool is_capture = captured != EMPTY || is_en_passant;

  apply_board_move(state.board, move, player, state.en_passant_target);

  // Castling rights: lose them when the king or a rook leaves home, or when a
  // rook is captured on its home square.
  if (type == KING) {
    if (player == 0) {
      state.white_can_castle_kingside  = false;
      state.white_can_castle_queenside = false;
    } else {
      state.black_can_castle_kingside  = false;
      state.black_can_castle_queenside = false;
    }
  }
  if (move.from == square_of(0, 0) || move.to == square_of(0, 0))
    state.white_can_castle_queenside = false;
  if (move.from == square_of(0, 7) || move.to == square_of(0, 7))
    state.white_can_castle_kingside = false;
  if (move.from == square_of(7, 0) || move.to == square_of(7, 0))
    state.black_can_castle_queenside = false;
  if (move.from == square_of(7, 7) || move.to == square_of(7, 7))
    state.black_can_castle_kingside = false;

  // En-passant target: only a pawn double-step offers one next turn.
  if (is_pawn_move && to_row - from_row == 2 * pawn_forward(player)) {
    state.en_passant_target =
      square_of(from_row + pawn_forward(player), from_col);
  } else {
    state.en_passant_target = -1;
  }

  // 50-move clock: resets on a pawn move or any capture.
  if (is_pawn_move || is_capture) {
    state.halfmove_clock = 0;
  } else {
    state.halfmove_clock += 1;
  }
}

// True if `square` shares a rank, file, or diagonal with the king. These are
// the only lines along which vacating `square` could discover an attack on the
// king, so a piece off all of them can never expose its king by moving.
static bool on_king_line(int king_square, int square) {
  int d_row = square_row(king_square) - square_row(square);
  int d_col = square_col(king_square) - square_col(square);
  return d_row == 0 || d_col == 0 || d_row == d_col || d_row == -d_col;
}

Move_List legal_moves(const Game_State& state) {
  int       player       = state.current_player;
  int       opponent     = 1 - player;
  Move_List pseudo_legal = generate_pseudo_legal(state, player);
  Move_List legal;

  // The king's square is the same for every non-king move, so find it once.
  int  king_square  = find_king_on(state.board, player);
  bool in_check_now = king_square >= 0 &&
                      is_square_attacked_on(state.board, king_square, opponent);

  for (const Move& move : pseudo_legal) {
    bool king_move = move.from == king_square;
    // A pawn reaching the en-passant target captures en passant, which clears
    // two squares on one rank and can discover a check, so it needs checking.
    bool en_passant =
      !king_move && move.to == state.en_passant_target &&
      piece_type(state.board[square_row(move.from)][square_col(move.from)]) ==
        PAWN;

    // Fast path: with the king safe and the moved piece off every king line,
    // the move cannot expose the king — it is legal with no attack scan at all.
    if (king_square >= 0 && !king_move && !en_passant && !in_check_now &&
        !on_king_line(king_square, move.from)) {
      legal.push_back(move);
      continue;
    }

    // Otherwise verify on a cheap board copy: the king (which moves on a king
    // move) must not be attacked afterwards.
    Board board = state.board;
    apply_board_move(board, move, player, state.en_passant_target);
    int king_after = king_move ? move.to : king_square;
    if (!is_square_attacked_on(board, king_after, opponent))
      legal.push_back(move);
  }
  return legal;
}

// A draw by lack of mating material: bare kings, or a lone king plus a single
// minor piece.
static bool is_insufficient_material(const Game_State& state) {
  int minor_pieces = 0;
  for (int row = 0; row < 8; ++row) {
    for (int col = 0; col < 8; ++col) {
      int type = piece_type(state.board[row][col]);
      if (type == EMPTY || type == KING) continue;
      if (type == KNIGHT || type == BISHOP) {
        minor_pieces += 1;
      } else {
        return false;  // A pawn, rook, or queen can still force mate.
      }
    }
  }
  return minor_pieces <= 1;
}

void apply_move(Game_State& state, const Move& move) {
  int mover = state.current_player;
  make_move_on_board(state, move);
  state.switch_turn();

  // The game ends if the side to move has no legal reply: checkmate when in
  // check, otherwise stalemate.
  Move_List replies = legal_moves(state);
  if (replies.empty()) {
    state.winner    = in_check(state, state.current_player) ? mover : 2;
    state.game_over = true;
    return;
  }

  if (state.halfmove_clock >= 100 || is_insufficient_material(state)) {
    state.winner    = 2;
    state.game_over = true;
  }
}

int compute_player_score(const Game_State& state, int player) {
  return state.winner == player ? 1 : 0;
}

Game_State quick_setup(int /*seed*/) {
  Game_State       game;
  static const int back_rank[8] = {
    ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK
  };
  for (int col = 0; col < 8; ++col) {
    game.board[0][col] = make_piece(back_rank[col], 0);  // White back rank.
    game.board[1][col] = make_piece(PAWN, 0);            // White pawns.
    game.board[6][col] = make_piece(PAWN, 1);            // Black pawns.
    game.board[7][col] = make_piece(back_rank[col], 1);  // Black back rank.
  }
  game.begin_game();  // The opening decision to present.
  return game;
}

// Write a UCI-style label for a move, e.g. "e2e4" or "e7e8q", into `out` (which
// must hold at least 6 chars: four coordinates, an optional promotion, a null).
static void write_move_label(char* out, const Move& move) {
  static const char* promotion_chars = " pnbrqk";  // Indexed by Piece type.
  int                length          = 0;
  out[length++]                      = (char)('a' + square_col(move.from));
  out[length++]                      = (char)('1' + square_row(move.from));
  out[length++]                      = (char)('a' + square_col(move.to));
  out[length++]                      = (char)('1' + square_row(move.to));
  if (move.promotion != 0) out[length++] = promotion_chars[move.promotion];
  out[length] = '\0';
}

Choice Game_State::next_choice() {
  if (game_over) return Choice{};

  Choice choice;
  choice.player_index     = current_player;
  choice.description      = "move";
  choice.text_description = "Move a piece";

  // Offer the legal moves; option index i corresponds to legal_moves()[i]. The
  // labels live in a thread-local buffer so the option targets can stay
  // non-owning const char*; it is safe under MCTS's threaded rollouts.
  choice.actions = [](Game& game) -> Choose {
    Game_State& state = static_cast<Game_State&>(game);
    Move_List   moves = legal_moves(state);

    // One label per move, kept in a thread-local buffer so the option targets
    // can stay non-owning const char*; 256 covers the 218-move ceiling, and it
    // is safe under MCTS's threaded rollouts.
    static thread_local char labels[256][6];
    Choose_Option            option;
    for (int i = 0; i < moves.size(); ++i) {
      write_move_label(labels[i], moves[i]);
      option.targets.push_back(labels[i]);
    }
    return option;
  };

  // Apply the chosen move, then return the next decision.
  choice.resolve = [](Game& game, int index) -> Choice {
    Game_State& state = static_cast<Game_State&>(game);
    Move_List   moves = legal_moves(state);
    apply_move(state, moves[index]);
    return no_choice;
  };

  return choice;
}

}  // namespace chess
