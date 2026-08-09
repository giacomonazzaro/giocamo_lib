#pragma once

#include <game/agent.h>
#include <giocamo/play.h>
#include <mindbug/models.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <vector>

// The player as an Agent: highlights the cards the pending choice can take,
// waits for one to be clicked (or for a button, where the choice isn't about a
// card), and answers with the matching action index.
struct Mindbug_Agent_UI : Agent_UI {
  // int          bottom_player;
  // Targets picked so far, for a choice that takes more than one.
  std::vector<int> selection;

  Mindbug_Agent_UI(int window_width, int window_height)
      : Agent_UI(window_width, window_height) {}

  void message(const std::string&) override {}

  int choose_action(Game& game, const Choice& choice) override;
};
