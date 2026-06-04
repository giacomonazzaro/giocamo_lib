// Headless smoke test for the online stack. Spawns two peers in the same
// process, both with scripted agents that always pick option 0, and drives
// a trivial 6-turn game through the real Agent_Local_Online / Agent_Remote
// / Agent_Duel plumbing + the ntfy.sh transport.
//
// Build target: `online_test_online` (see CMakeLists.txt). Run it directly:
//   ./build/.../online_test_online
// Expected output: both peers print "applying idx=0" alternating, and
// `[done] turns=6` at the end. Any divergence (one peer stalls, both pick
// the same player, etc.) is a transport bug.

#include <game/agent.h>
#include <game/game.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include "agents.h"
#include "protocol.h"
#include "setup.h"

namespace {

// Minimal Game: 2 players alternating, 6 turns total, Choose_Option with 2
// options each turn. Identical deterministic state on both peers as long as
// they apply the same action sequence.
struct Test_Game : Game {
  int current_player = 0;
  int turn           = 0;
  int log_tag        = 0;  // 0 = host, 1 = join — just for prefixed prints.

  bool is_game_over() const override { return turn >= 6; }

  Choice next_choice() override {
      if (is_game_over()) return {};
    Choice c;
    c.player_index = current_player;
    c.description  = "test";
    c.actions      = [](Game&) -> Choose {
      Choose_Option o;
      o.targets = {"A", "B"};
      return o;
    };
//    c.resolve = [](Game& g, int idx) -> std::vector<Choice> {
//      Test_Game& tg = static_cast<Test_Game&>(g);
//      fprintf(
//        stderr,
//        "  [%s] resolve: player %d picked %d -> turn %d -> %d\n",
//        tg.log_tag == 0 ? "host" : "join",
//        tg.current_player,
//        idx,
//        tg.turn + 1,
//        1 - tg.current_player
//      );
//      tg.current_player = 1 - tg.current_player;
//      tg.turn++;
//      return {};
//    };
    return c;
  }
};

struct Agent_Scripted : Agent {
  const char* tag;
  explicit Agent_Scripted(const char* t) : tag(t) {}
  int choose_action(Game&, const Choice& c) override {
    fprintf(stderr, "  [%s] scripted picks 0 for player %d\n", tag, c.player_index);
    return 0;
  }
};

void run_peer(bool is_host, std::atomic<int>* turns_done) {
  const char* tag = is_host ? "host" : "join";
  fprintf(stderr, "[%s] starting handshake...\n", tag);
  Online_Connection conn = setup_local(is_host);
  fprintf(
    stderr,
    "[%s] handshake done: player_index=%d seed=%d\n",
    tag,
    conn.player_index,
    conn.seed
  );

  Test_Game      game;
  game.log_tag         = is_host ? 0 : 1;
  Agent_Scripted scripted(tag);
  Agent*         duel = make_online_duel(&scripted, conn.online, conn.player_index);

  while (!game.is_game_over()) {
    auto choice = game.next_choice();
//    if (!choice) break;
    int idx = duel->choose_action(game, choice);
    if (idx < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    choice.resolve(game, idx);
    turns_done->store(game.turn);
  }
  fprintf(stderr, "[%s] done. turns=%d player=%d\n", tag, game.turn, game.current_player);
}

}  // namespace

int main() {
  std::atomic<int> host_turns{0};
  std::atomic<int> join_turns{0};

  std::thread host_thread(run_peer, true, &host_turns);
  // Stagger so the host has a chance to start polling before the joiner
  // posts its first hello.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::thread join_thread(run_peer, false, &join_turns);

  host_thread.join();
  join_thread.join();

  fprintf(stderr, "\n=== summary ===\n");
  fprintf(stderr, "host turns: %d\n", host_turns.load());
  fprintf(stderr, "join turns: %d\n", join_turns.load());
  bool ok = host_turns.load() == 6 && join_turns.load() == 6;
  fprintf(stderr, "result: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
