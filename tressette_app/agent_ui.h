#pragma once

#include <game/agent.h>
#include <giocamo/play.h>
#include <tabletop/config.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// UI-driven agent for Tressette: highlights legal cards in the active player's
// hand and converts a successful drag-and-drop onto the table into an action
// index. Returns -1 when no card was dropped on this frame.
struct Tressette_Agent_UI : Agent_UI {
  // The seat this screen plays. Set from Giocamo::bottom_player.
  int player_index = 0;
  // Which cards get a "legal move" border. The card draw callbacks read it.
  UI_State ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);

  void message(const std::string&) override {}
  int  choose_action(Game& game, const Choice& choice) override;
};
