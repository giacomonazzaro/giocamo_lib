#include "play.h"

#include <online/protocol.h>

void update_zoomed_thing(Table_State& table_state, const Input& input) {
  if (key_down(input, KEY_SPACE)) {
    auto path = find_thing_at(
      (float)input.mouse_x, (float)input.mouse_y, table_state
    );
    table_state.zoomed_thing_id = std::move(path);
  } else {
    table_state.zoomed_thing_id.clear();
  }
}

nlohmann::json serialize_stacks(const Table_State& table_state) {
  nlohmann::json out = nlohmann::json::array();
  for (const Thing& t : table_state.things) {
    out.push_back(t.children);
  }
  return out;
}

void apply_stacks_message(
  Table_State& table_state, const nlohmann::json& arr
) {
  for (size_t i = 0; i < arr.size() && i < table_state.things.size(); ++i) {
    table_state.things[i].children = arr[i].get<std::vector<int>>();
    update_children_positions((int)i, table_state, false);
  }
}

void send_stacks(const Online& online, const Table_State& table_state) {
  nlohmann::json msg;
  msg["type"]   = "stacks";
  msg["stacks"] = serialize_stacks(table_state);
  send_message(online, msg);
}
