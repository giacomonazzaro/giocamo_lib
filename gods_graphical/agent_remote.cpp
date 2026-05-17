#include "agent_remote.h"

int Agent_Remote::choose_action(Game& state, const Choice& choice) {
  (void)state;
  (void)choice;
  auto msg = recv_message(*online.sock);
  if (!msg.contains("index")) return -1;
  return msg["index"].get<int>();
}

int Agent_Local_Online::choose_action(Game& state, const Choice& choice) {
  int            index = local_agent->choose_action(state, choice);
  nlohmann::json m;
  m["type"]  = "action";
  m["index"] = index;
  send_message(*online.sock, m, online.friend_addr);
  return index;
}
