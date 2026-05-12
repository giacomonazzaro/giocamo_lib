#pragma once
#include <game/agent.h>
#include <tabletop/models.h>
#include <tabletop/ui.h>

struct Tressette_Agent_UI : Agent {
  Table_State* table_state;
  UI_State*    ui_state;
  float        last_play_time    = 0.0f;
  int          action_to_perform = -1;

  Tressette_Agent_UI(Table_State* ts, UI_State* ui)
      : table_state(ts), ui_state(ui) {}

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
