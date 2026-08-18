// Check for the online message log. Runs both players in one process and
// one thread, against a database, and fails if a message is lost, arrives
// out of order, or arrives twice.
//
// Run it against the fake database in web_server.py — no Google account and
// no Internet needed:
//   1. python3 web_server.py . 8080 &
//   2. FIREBASE_URL=http://localhost:8080 ./build/gods_app/giocamo_test_online
// It also runs against the real database: leave FIREBASE_URL unset.
//
// It prints "PASS" and returns 0 when everything arrived correctly.

#include <game/agent.h>
#include <game/game.h>

#include <chrono>
#include <cstdio>
#include <random>
#include <thread>

#include "online.h"

namespace {

// A room nobody else is using, so slots left over from an earlier run do
// not get read as this run's messages.
std::string test_room_code() {
  auto generator = std::mt19937(std::random_device{}());
  return "test" + std::to_string(generator() % 1000000);
}

void wait_a_moment() {
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Two players, six turns, two options each turn. Both sides hold the same
// state as long as they apply the same moves in the same order.
struct Test_Game : Game {
  int current_player = 0;
  int turn           = 0;

  bool is_game_over() const override { return turn >= 6; }

  Choice next_choice() override {
    if (is_game_over()) return {};
    Choice choice;
    choice.player_index = current_player;
    choice.description  = "test";
    choice.actions      = [](Game&) -> Choose {
      Choose_Option option;
      option.targets = {"A", "B"};
      return option;
    };
    choice.resolve = [](Game& game, int index) -> Choice {
      (void)index;
      Test_Game& test_game     = static_cast<Test_Game&>(game);
      test_game.current_player = 1 - test_game.current_player;
      test_game.turn += 1;
      return null_choice;
    };
    return choice;
  }
};

// Always picks the first option, so any difference between the two players
// comes from the messages and not from the moves.
struct Agent_Scripted : Agent {
  int choose_action(Game&, const Choice&) override { return 0; }
};

bool failed = false;

void check(bool condition, const std::string& what) {
  if (condition) return;
  std::fprintf(stderr, "FAIL: %s\n", what.c_str());
  failed = true;
}

// Every message one player sends must reach the other, once each, in the
// order they were sent. This is what the numbered slots are for.
void check_messages_arrive_in_order(const std::string& room_code) {
  auto host = Online{room_code, "host"};
  auto join = Online{room_code, "join"};

  const int count = 5;
  for (int i = 0; i < count; ++i) {
    auto message      = nlohmann::json();
    message["type"]   = "count";
    message["number"] = i;
    send_message(host, message);
  }

  auto received = std::vector<int>();
  for (int step = 0; step < 400 && (int)received.size() < count; ++step) {
    // The host writes one message per call, the joiner reads.
    send_message(host, nlohmann::json{{"type", "idle"}});
    while (auto message = try_recv_message(join)) {
      if (message->value("type", "") == "count") {
        received.push_back(message->value("number", -1));
      }
    }
    wait_a_moment();
  }

  check((int)received.size() == count, "all five messages arrived");
  for (int i = 0; i < (int)received.size(); ++i) {
    check(received[i] == i, "message " + std::to_string(i) + " came in order");
  }
}

// The real agents, the real duel, the real transport: both players must
// finish the same six turns.
void check_a_whole_game_plays_out(const std::string& room_code) {
  auto host_game = Test_Game();
  auto join_game = Test_Game();

  auto host_agent = Agent_Scripted();
  auto join_agent = Agent_Scripted();

  // The host takes seat 0, the joining player seat 1.
  Agent* host_duel = make_online_duel(&host_agent, {room_code, "host"}, 0);
  Agent* join_duel = make_online_duel(&join_agent, {room_code, "join"}, 1);

  host_game.begin_game();
  join_game.begin_game();

  for (int step = 0; step < 600; ++step) {
    if (host_game.is_game_over() && join_game.is_game_over()) break;
    if (!host_game.is_game_over()) game_frame(host_game, *host_duel);
    if (!join_game.is_game_over()) game_frame(join_game, *join_duel);
    wait_a_moment();
  }

  check(host_game.turn == 6, "the host played six turns");
  check(join_game.turn == 6, "the joining player played six turns");
}

}  // namespace

int main() {
  auto room_code = test_room_code();
  std::fprintf(stderr, "room %s\n", room_code.c_str());

  check_messages_arrive_in_order(room_code + "a");
  check_a_whole_game_plays_out(room_code + "b");

  std::fprintf(stderr, "%s\n", failed ? "FAILED" : "PASS");
  return failed ? 1 : 0;
}
