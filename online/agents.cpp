#include "agents.h"

#include <nlohmann/json.hpp>

int Agent_Remote::choose_action(Game& state, const Choice& choice) {
  (void)state;
  (void)choice;
  auto msg = try_recv_message(online, "action");
  if (!msg) return -1;
  if (!msg->contains("index")) return -1;
  return (*msg)["index"].get<int>();
}

int Agent_Local_Online::choose_action(Game& state, const Choice& choice) {
  int index = local_agent->choose_action(state, choice);
  if (index < 0) return -1;
  nlohmann::json m;
  m["type"]  = "action";
  m["index"] = index;
  send_message(online, m);
  return index;
}

Agent* make_online_duel(
  Agent* local_agent, const Online& online, int player_index
) {
  Agent* local_seat    = new Agent_Local_Online(local_agent, online);
  Agent* opponent_seat = new Agent_Remote(online);
  return new Agent_Duel(local_seat, opponent_seat, /*swap=*/player_index != 0);
}
