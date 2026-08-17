#include "agent_ui.h"

#include <chess/gameplay.h>

int Chess_Agent_UI::choose_action(Game& game, const Choice&) {
  auto&            state = static_cast<chess::Game_State&>(game);
  chess::Move_List moves = chess::legal_moves(state);

  // Which square (thing-id == board index) got left-clicked this frame, if any.
  int clicked_square = -1;
  for (int square = 0; square < 64; ++square) {
    if (thing_pressed(square, table, *input)) {
      clicked_square = square;
      break;
    }
  }
  if (clicked_square < 0) return -1;

  // No piece picked yet: select the clicked square if it has a legal move.
  if (selected_square < 0) {
    for (const chess::Move& move : moves) {
      if (move.from == clicked_square) {
        selected_square = clicked_square;
        break;
      }
    }
    return -1;
  }

  // Clicking the selected square again cancels the selection.
  if (clicked_square == selected_square) {
    selected_square = -1;
    return -1;
  }

  // A legal move from the selected square to the click resolves it. Promotions
  // auto-queen, so skip the rook/bishop/knight options.
  for (int i = 0; i < (int)moves.size(); ++i) {
    if (moves[i].from == selected_square && moves[i].to == clicked_square) {
      if (moves[i].promotion != 0 && moves[i].promotion != chess::QUEEN) continue;
      selected_square = -1;
      return i;
    }
  }

  // Clicking another of the player's own movable pieces reselects it; anything
  // else clears the selection.
  for (const chess::Move& move : moves) {
    if (move.from == clicked_square) {
      selected_square = clicked_square;
      return -1;
    }
  }
  selected_square = -1;
  return -1;
}
