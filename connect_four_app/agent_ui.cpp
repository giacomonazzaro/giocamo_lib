#include "agent_ui.h"

#include <vector>

#include <connect_four/gameplay.h>

int Connect_Four_Agent_UI::choose_action(Game& game, const Choice&) {
  auto&            state   = static_cast<connect_four::Game_State&>(game);
  std::vector<int> columns = connect_four::legal_columns(state);

  // A left-click on a legal column drops a disc there. The returned index is
  // the column's position in `columns`, which matches the action ordering
  // next_choice exposes (both call legal_columns). Returns -1 until a click.
  for (int i = 0; i < (int)columns.size(); ++i) {
    int column_id = COLUMNS_OFFSET + columns[i];
    if (thing_pressed(column_id, table, *input)) {
      return i;
    }
  }
  return -1;
}
