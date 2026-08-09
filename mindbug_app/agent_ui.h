#pragma once

#include <game/agent.h>
#include <mindbug/models.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <vector>

// The player as an Agent: highlights the cards the pending choice can take,
// waits for one to be clicked (or for a button, where the choice isn't about a
// card), and answers with the matching action index.
struct Mindbug_Agent_UI : Agent {
  Table_State* table;
  UI_State*    ui_state;
  int          bottom_player;
  // Targets picked so far, for a choice that takes more than one.
  std::vector<int> selection;

  Mindbug_Agent_UI(Table_State* table, UI_State* ui_state, int bottom_player)
      : table(table), ui_state(ui_state), bottom_player(bottom_player) {}

  void message(const std::string&) override {}

  int choose_action(Game& game, const Choice& choice) override;
};
