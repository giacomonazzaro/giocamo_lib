#pragma once

#include <string>

#include <connect_four/models.h>
#include <game/agent.h>
#include <giocamo/play.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// Thing-id of column 0. The ROWS*COLS disc Things come before it.
static const int COLUMNS_OFFSET = connect_four::ROWS * connect_four::COLS;

// UI agent for Connect Four: the player drops a disc by clicking a column.
// No drag/drop — a left-click on a legal column resolves the move.
struct Connect_Four_Agent_UI : Agent_UI {
  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
