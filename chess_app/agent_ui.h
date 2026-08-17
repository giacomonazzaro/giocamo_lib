#pragma once

#include <string>

#include <chess/models.h>
#include <game/agent.h>
#include <giocamo/play.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// UI agent for chess: the player clicks a piece, then a destination square. The
// first click selects (when the piece has a legal move); the second resolves
// the move. Clicking the selected square again, or an illegal target, clears
// the selection. Pawn promotions auto-queen.
struct Chess_Agent_UI : Agent_UI {
  int selected_square = -1;  // -1 = nothing selected yet.

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
