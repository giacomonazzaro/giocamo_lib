#pragma once

#include <string>

#include <chess/models.h>
#include <game/agent.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// UI agent for chess: the player clicks a piece, then a destination square. The
// first click selects (when the piece has a legal move); the second resolves
// the move. Clicking the selected square again, or an illegal target, clears
// the selection. Pawn promotions auto-queen.
struct Chess_Agent_UI : Agent {
  Table_State* table_state;
  UI_State*    ui_state;
  int          player_index;
  int          selected_square = -1;  // -1 = nothing selected yet.

  Chess_Agent_UI(Table_State* table, UI_State* ui, int player_index)
      : table_state(table), ui_state(ui), player_index(player_index) {}

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
