#pragma once

#include <game/agent.h>
#include <giocamo/play.h>
#include <scopa/models.h>
#include <tabletop/config.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <vector>

// UI-driven agent for Scopa: highlights legal hand cards, lets the player
// drag one onto the table, and (when the drop has more than one capture
// option) prompts them to pick which subset of the table cards to take.
struct Scopa_Agent_UI : Agent_UI {
  // The seat this screen plays. Set from Giocamo::bottom_player.
  int player_index = 0;
  // Which cards get a "legal move" border. The card draw callbacks read it.
  UI_State ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);

  // Cross-frame state for the two-step "play card → choose capture" flow.
  int                           pending_played_card_id = -1;
  std::vector<int>              pending_action_indices;
  std::vector<std::vector<int>> pending_capture_options;

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
