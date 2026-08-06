#pragma once

#include <dot/models.h>
#include <game/agent.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// UI-driven agent for the local player. The player drags cards into the play
// area (the shared pool during the split phase, the opponent's pool during the
// discard phase) and presses Commit once the right number are there. Until
// then choose_action returns -1, so the game waits.
struct Dot_Agent_UI : Agent {
  Table_State* table_state;
  UI_State*    ui_state;
  int          player_index;  // Which seat the local player controls.

  Dot_Agent_UI(Table_State* table, UI_State* ui, int player_index)
      : table_state(table), ui_state(ui), player_index(player_index) {}

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
