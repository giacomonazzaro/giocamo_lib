#include "agent_ui.h"

#include <tressette/models.h>

#include <set>
#include <variant>

#include "ui.h"

int Tressette_Agent_UI::choose_action(Game& game, const Choice& choice) {
  auto actions = choice.actions(game);
  if (choice.description == "acknowledge") {
    int  base   = stacks_offset;
    auto middle = world_rect(base + TRESSETTE_TABLE_IDX, *table_state);
    auto rect   = place_next(middle, 100, 50, "right", "center", 100);
    if (immediate_button(rect, "Ok", *this->ui_state->input)) {
      return 0;
    }
  }
  auto* cc = std::get_if<Choose_Card>(&actions);
  if (!cc || cc->targets.empty()) return -1;

  auto legal_set = std::set<int>(cc->targets.begin(), cc->targets.end());

  // stacks_offset is set by main to point at the first stack thing-id.
  int base = stacks_offset;
  int hand_id =
    base + (choice.player_index == 0 ? TRESSETTE_HAND_0 : TRESSETTE_HAND_1);
  int table_id = base + TRESSETTE_TABLE_IDX;

  auto* state_ptr = static_cast<tressette::Game_State*>(&game);
  table_state->is_drop_allowed =
    [hand_id, table_id, legal_set, this, state_ptr](int src, int dst, int cid) {
      if (src == dst) return true;
      return state_ptr->current_player == this->player_index &&
             src == hand_id && dst == table_id && legal_set.count(cid) > 0;
    };

  // Highlight legal cards so the draw callback can draw a border around them.
  ui_state->highlighted_things.clear();
  for (int cid : legal_set) ui_state->highlighted_things[cid] = cid;

  auto dropped = table_state->poll_dropped_thing();
  if (!dropped) return -1;

  auto [src, dst, dropped_id] = *dropped;
  if (src == hand_id && dst == table_id && legal_set.count(dropped_id)) {
    ui_state->highlighted_things.clear();
    for (int i = 0; i < (int)cc->targets.size(); ++i) {
      if (cc->targets[i] == dropped_id) return i;
    }
  }
  return -1;
}
