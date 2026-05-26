#include "setup.h"

#include <chrono>
#include <random>
#include <string>
#include <thread>

#include "protocol.h"

namespace {

// 4-character room code in [a-z0-9] — enough entropy for friend-to-friend
// matchmaking, short enough to type. Lowercase/digits stay readable.
std::string random_room_code() {
  static thread_local std::mt19937 rng{std::random_device{}()};
  static const char                  alphabet[] =
    "abcdefghijklmnopqrstuvwxyz0123456789";
  std::string                            code(4, '0');
  std::uniform_int_distribution<int>     pick(0, (int)sizeof(alphabet) - 2);
  for (char& c : code) c = alphabet[pick(rng)];
  return code;
}

long random_seed() {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<long>    pick(1, 1L << 30);
  return pick(rng);
}

// Both peers learn each other's random seed; the smaller one becomes the
// game seed (so both sides derive the same shuffled deal). Whoever holds
// the smaller seed takes player 0.
void resolve_seats(Connection_State& state, long peer_seed) {
  long mine = state.my_seed;
  if (mine < peer_seed) {
    state.player_index = 0;
    state.seed         = (int)mine;
  } else {
    state.player_index = 1;
    state.seed         = (int)peer_seed;
  }
}

}  // namespace

void Connection_State::tick() {
  if (ready.load()) return;

  Online wire{topic_send, topic_recv};

  // Joiner posts hello immediately; host waits and replies to whatever
  // hello it sees. Both end up sending exactly one hello.
  if (!sent_hello && !is_host) {
    nlohmann::json hello;
    hello["type"] = "hello";
    hello["seed"] = my_seed;
    send_message(wire, hello);
    sent_hello = true;
  }

  auto msg = try_recv_message(wire);
  if (!msg) return;
  if (msg->value("type", "") != "hello") return;

  long peer_seed = (*msg).value("seed", (long)0);
  {
    std::lock_guard<std::mutex> lg(state_lock);
    resolve_seats(*this, peer_seed);
  }

  if (is_host && !sent_hello) {
    nlohmann::json reply;
    reply["type"] = "hello";
    reply["seed"] = my_seed;
    send_message(wire, reply);
    sent_hello = true;
  }
  ready.store(true);
}

std::shared_ptr<Connection_State> start_hosting(bool /*local*/) {
  auto state        = std::make_shared<Connection_State>();
  state->room_code  = random_room_code();
  state->topic_send = "gods-room-" + state->room_code + "-host";
  state->topic_recv = "gods-room-" + state->room_code + "-join";
  state->is_host    = true;
  state->my_seed    = random_seed();
  return state;
}

std::shared_ptr<Connection_State> join_room(
  const std::string& room_code, bool /*local*/
) {
  auto state        = std::make_shared<Connection_State>();
  state->room_code  = room_code;
  // Joiner's send/recv topics are the host's flipped.
  state->topic_send = "gods-room-" + room_code + "-join";
  state->topic_recv = "gods-room-" + room_code + "-host";
  state->is_host    = false;
  state->my_seed    = random_seed();
  return state;
}

Online_Connection setup_local(bool host) {
  // Fixed code so the two test instances find each other without arguments.
  auto state         = host ? start_hosting() : join_room("local");
  if (host) {
    state->room_code  = "local";
    state->topic_send = "gods-room-local-host";
    state->topic_recv = "gods-room-local-join";
  }
  while (!state->ready.load()) {
    state->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  Online_Connection result;
  result.online       = {state->topic_send, state->topic_recv};
  result.player_index = state->player_index;
  result.seed         = state->seed;
  return result;
}

std::optional<Online_Connection> setup_local_from_argv(int argc, char** argv) {
  bool host = false, join = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a.rfind("--local-host", 0) == 0) host = true;
    else if (a.rfind("--local-join", 0) == 0) join = true;
  }
  if (!host && !join) return std::nullopt;
  return setup_local(host);
}
