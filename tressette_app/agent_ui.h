#pragma once

#include <game/agent.h>
#include <tabletop/models.h>
#include <tabletop/ui.h>

// UI-driven agent for Tressette: highlights legal cards in the active player's
// hand and converts a successful drag-and-drop onto the table into an action
// index. Returns -1 when no card was dropped on this frame.
struct Tressette_Agent_UI : Agent {
  Table_State* table_state;
  UI_State*    ui_state;
  int          player_index;
  int          stacks_offset;
  int          action_to_perform = -1;

  Tressette_Agent_UI(
    Table_State* ts, UI_State* ui, int player_index, int stacks_offset
  )
      : table_state(ts)
      , ui_state(ui)
      , player_index(player_index)
      , stacks_offset(stacks_offset) {}

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
