#pragma once

#include <string>

#include <connect_four/models.h>
#include <game/agent.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// UI agent for Connect Four: the player drops a disc by clicking a column.
// No drag/drop — a left-click on a legal column resolves the move.
struct Connect_Four_Agent_UI : Agent {
  Table_State* table_state;
  UI_State*    ui_state;
  int          player_index;
  int          columns_offset;  // Thing-id of column 0.

  Connect_Four_Agent_UI(
    Table_State* table, UI_State* ui, int player_index, int columns_offset
  )
      : table_state(table)
      , ui_state(ui)
      , player_index(player_index)
      , columns_offset(columns_offset) {}

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
