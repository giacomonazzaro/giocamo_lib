#include "play.h"

#include <online/agents.h>
#include <online/protocol.h>
#include <online/setup.h>

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

Menu_Result resolve_play_mode(
  const std::string& title,
  int                window_width,
  int                window_height,
  Input_Feed&        inputs,
  int                argc,
  char**             argv,
  bool               skip_menu
) {
  // --local-host / --local-join bypass the menu and STUN/ntfy entirely.
  if (auto local_conn = setup_local_from_argv(argc, argv)) {
    Menu_Result r;
    r.mode         = Menu_Result::ONLINE;
    r.online       = local_conn->online;
    r.player_index = local_conn->player_index;
    r.seed         = local_conn->seed;
    return r;
  }
  if (skip_menu) return Menu_Result{};
  return run_menu(title, window_width, window_height, inputs);
}

Agent* make_duel(
  Agent* local_agent, Agent* opponent, const Menu_Result& menu_result
) {
  if (menu_result.mode == Menu_Result::ONLINE) {
    return make_online_duel(
      local_agent, menu_result.online, menu_result.player_index
    );
  }
  return new Agent_Duel(
    local_agent, opponent, /*swap=*/menu_result.player_index != 0
  );
}
