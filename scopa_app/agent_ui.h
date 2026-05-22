#pragma once

#include <game/agent.h>
#include <scopa/models.h>
#include <tabletop/models.h>
#include <tabletop/ui.h>

#include <vector>

// UI-driven agent for Scopa: highlights legal hand cards, lets the player
// drag one onto the table, and (when the drop has more than one capture
// option) prompts them to pick which subset of the table cards to take.
struct Scopa_Agent_UI : Agent {
  Table_State* table_state;
  UI_State*    ui_state;
  int          player_index;

  // Cross-frame state for the two-step "play card → choose capture" flow.
  int                            pending_played_card_id = -1;
  std::vector<int>               pending_action_indices;
  std::vector<std::vector<int>>  pending_capture_options;

  Scopa_Agent_UI(Table_State* table, UI_State* ui, int player_index)
      : table_state(table), ui_state(ui), player_index(player_index) {}

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
