#pragma once

#include <game/agent.h>
#include <gods/models.h>
#include <tabletop/input.h>
#include <tabletop/models.h>
#include <tabletop/ui.h>

#include <set>

// Thing-ids of a given player's zones. Layout is the one produced by
// make_gods_stacks(): deck/hand/discard/peoples/wonders, each player gets 5
// consecutive entries starting at stacks_offset + player_index*5.
struct Stack_Indices {
  int deck;
  int hand;
  int discard;
  int peoples;
  int wonders;
};

inline Stack_Indices stack_indices(int player_index, int stacks_offset) {
  int base = stacks_offset + player_index * 5;
  return Stack_Indices{base, base + 1, base + 2, base + 3, base + 4};
}

// Copy current visual stack contents back into the Game_State (used when
// exiting Playground mode so game logic resumes from the user-arranged layout).
void sync_game_state_from_table(
  Table_State& table_state, Game_State& gods_state, int stacks_offset
);

// Push the current Game_State zones into Table_State.stacks and refresh card
// positions.
void update_stacks(
  Table_State& table_state, Game_State& gods_state, int stacks_offset
);

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
struct Agent_UI : Agent {
  Table_State*                    table_state;
  UI_State*                       ui_state;
  int                             bottom_player;
  int                             stacks_offset;
  std::set<Card_Id, Card_Id_Less> card_multiselection;

  Agent_UI(Table_State* t, UI_State* u, int bp, int stacks_offset)
      : table_state(t)
      , ui_state(u)
      , bottom_player(bp)
      , stacks_offset(stacks_offset) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override;
};
