#pragma once

// gods/models.h before anything that pulls raylib in: raylib defines
// RED/GREEN/BLUE as macros and they would expand inside Card_Color.
#include <gods/models.h>
//
#include <game/agent.h>
#include <giocamo/play.h>
#include <tabletop/config.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <set>

#include "ui.h"

// Thing-ids of a given player's zones, looked up by the names
// make_gods_stacks() gives them ("p0_deck", "p1_hand", ...).
struct Stack_Indices {
  int deck;
  int hand;
  int discard;
  int peoples;
  int wonders;
};

Stack_Indices stack_indices(const Table_State& table_state, int player_index);

// Copy current visual stack contents back into the Game_State (used when
// exiting Playground mode so game logic resumes from the user-arranged layout).
void sync_game_state_from_table(
  Table_State& table_state, Game_State& gods_state
);

// Push the current Game_State zones into Table_State.stacks and refresh card
// positions.
void update_stacks(Table_State& table_state, Game_State& gods_state);

// Comparator so Card_Id can sit in an ordered set.
struct Card_Id_Less {
  bool operator()(const Card_Id& a, const Card_Id& b) const {
    if (a.card_index != b.card_index) return a.card_index < b.card_index;
    if (a.owner_index != b.owner_index) return a.owner_index < b.owner_index;
    return a.area < b.area;
  }
};

// UI-driven agent: reads drag/drop, button clicks, and card presses from the
// player to feed choose_action with an action index. Mirrors agent_ui.py.
struct Gods_Agent_UI : Agent_UI {
  // The seat this screen plays. Set from Giocamo::bottom_player.
  int bottom_player = 0;
  // Highlights plus the power-edit target. The card draw callbacks read it.
  Gods_UI ui_state = Gods_UI(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);

  std::set<Card_Id, Card_Id_Less> card_multiselection;

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override;
};
