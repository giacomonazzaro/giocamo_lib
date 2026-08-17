#pragma once

#include <dot/models.h>
#include <game/agent.h>
#include <giocamo/play.h>
#include <tabletop/config.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// UI-driven agent for the local player. The player drags cards into the play
// area (the shared pool during the split phase, the opponent's pool during the
// discard phase) and presses Commit once the right number are there. Until
// then choose_action returns -1, so the game waits.
struct Dot_Agent_UI : Agent_UI {
  // Which seat the local player controls. Set from Giocamo::bottom_player.
  int player_index = 0;
  // Which cards get a "legal move" border. The card draw callbacks read it.
  UI_State ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
