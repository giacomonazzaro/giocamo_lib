#include "online.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <random>
#include <thread>

#include "http.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

// The Firebase Realtime Database every build talks to. Replace this with
// your own database URL, without a trailing slash. See firebase_setup.md.
const char* DEFAULT_DATABASE_URL =
  "https://giocamo-default-rtdb.europe-west1.firebasedatabase.app";

// Read once, on the first call. FIREBASE_URL in the environment wins; in
// the browser, adding ?firebase=https://... to the page URL works too,
// which is useful for testing against a second database.
std::string database_url() {
  static std::string url = []() {
    auto        chosen           = std::string();
    const char* from_environment = std::getenv("FIREBASE_URL");
    if (from_environment && *from_environment) chosen = from_environment;
#ifdef __EMSCRIPTEN__
    if (chosen.empty()) {
      char* from_page = (char*)EM_ASM_PTR({
        var value = new URLSearchParams(window.location.search).get('firebase');
        if (!value) return 0;
        var length = lengthBytesUTF8(value) + 1;
        var buffer = _malloc(length);
        stringToUTF8(value, buffer, length);
        return buffer;
      });
      if (from_page) {
        chosen = from_page;
        free(from_page);
      }
    }
#endif
    if (chosen.empty()) chosen = DEFAULT_DATABASE_URL;
    while (!chosen.empty() && chosen.back() == '/') chosen.pop_back();
    return chosen;
  }();
  return url;
}

// Slot names are padded with zeros because Firebase sorts keys as text:
// without the padding "10" would come before "9" and the startAt query
// would skip messages.
// ponytail: six digits stop a match at 999999 messages, far more than a
// board game plays. Widen the format if that ever matters.
std::string slot_key(int index) {
  char text[16];
  std::snprintf(text, sizeof(text), "%06d", index);
  return text;
}

// Where one message lives: /rooms/<code>/<seat>/<slot>.json
std::string slot_url(const Online& online, int index) {
  return database_url() + "/rooms/" + online.room_code + "/" + online.seat +
         "/" + slot_key(index) + ".json";
}

// The other player's messages, from `index` on. orderBy="$key" needs no
// index rule — Firebase always keeps keys sorted — and startAt makes the
// answer hold only the slots we have not read yet. The quotes and the
// dollar sign are percent-encoded because they go in a URL.
std::string read_url(const Online& online, int index) {
  auto other_seat = std::string(online.seat == "host" ? "join" : "host");
  return database_url() + "/rooms/" + online.room_code + "/" + other_seat +
         ".json?orderBy=%22%24key%22&startAt=%22" + slot_key(index) + "%22";
}

// One player's side of a room: what is still to be written, and how far we
// have read. This lives here rather than in Online so that every copy of an
// Online value — the two agents each hold one — shares the same queues.
struct Channel {
  // Messages written but not yet taken by the database. The front one goes
  // into slot `send_index` and is only dropped once the database answers,
  // so a write that fails is simply made again.
  std::deque<std::string> outbox;
  int                     send_index = 0;
  // Next slot to read from the other player.
  int                     read_index = 0;

  std::deque<nlohmann::json> actions;
  std::deque<nlohmann::json> others;

  Http_Request                          request;
  bool                                  request_is_write = false;
  std::chrono::steady_clock::time_point next_request_at =
    std::chrono::steady_clock::now();
};

// Channels are never deleted: on native a worker thread may still be
// writing into a Channel's Http_Request when the room is left.
std::map<std::string, Channel*> channels;

Channel& get_channel(const Online& online) {
  Channel*& slot = channels[online.room_code + "/" + online.seat];
  if (!slot) slot = new Channel();
  return *slot;
}

// Forget the queues of a room, so entering the same room again starts from
// slot zero instead of carrying the old counters.
void forget_room(const std::string& room_code) {
  if (room_code.empty()) return;
  channels.erase(room_code + "/host");
  channels.erase(room_code + "/join");
}

// Read the answer to a read request. It is an object keyed by slot, like
// {"000003":{...},"000004":{...}}, or "null" when there is nothing new.
// Slots are taken in order and a gap stops the run, so a message that has
// not landed yet is waited for instead of stepped over.
void take_new_messages(Channel& channel, const std::string& body) {
  auto answer = nlohmann::json();
  try {
    answer = nlohmann::json::parse(body);
  } catch (...) {
    return;
  }
  if (!answer.is_object()) return;
  while (true) {
    auto found = answer.find(slot_key(channel.read_index));
    if (found == answer.end()) return;
    if (found->value("type", "") == "action")
      channel.actions.push_back(*found);
    else
      channel.others.push_back(*found);
    channel.read_index += 1;
  }
}

// How long to wait before asking the database for new messages again. A
// board game makes a move every few seconds, so half a second feels
// instant, and it keeps the traffic far inside Firebase's free allowance.
constexpr std::chrono::milliseconds REQUEST_INTERVAL{500};

// Move the channel forward by at most one HTTP request: finish the one in
// flight, then either write the oldest message that has not gone out or ask
// for what the other player wrote.
void pump(const Online& online, Channel& channel) {
  auto now = std::chrono::steady_clock::now();

  if (channel.request.done()) {
    bool write_failed = false;
    if (channel.request_is_write) {
      if (channel.request.succeeded) {
        channel.outbox.pop_front();
        channel.send_index += 1;
      } else {
        write_failed = true;
      }
    } else if (channel.request.succeeded) {
      take_new_messages(channel, channel.request.response);
    }
    channel.request.clear();
    // A write that worked is followed at once by the next request, so a
    // move reaches the other player without waiting. Reads and failed
    // writes wait, so a quiet match does not hammer the database.
    if (!channel.request_is_write || write_failed) {
      channel.next_request_at = now + REQUEST_INTERVAL;
    }
  }

  if (channel.request.running()) return;

  if (!channel.outbox.empty()) {
    if (channel.request_is_write && now < channel.next_request_at) return;
    channel.request_is_write = true;
    channel.request.start(
      "PUT", slot_url(online, channel.send_index), channel.outbox.front()
    );
    return;
  }

  if (now < channel.next_request_at) return;
  channel.request_is_write = false;
  channel.request.start("GET", read_url(online, channel.read_index), "");
}

// Four characters, with no pair that looks alike (no 0 and o, no 1 and l),
// so a code read out loud is typed back correctly.
std::string random_room_code() {
  static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
  auto              generator  = std::mt19937(std::random_device{}());
  auto pick = std::uniform_int_distribution<int>(0, (int)sizeof(alphabet) - 2);
  auto code = std::string(4, 'a');
  for (char& letter : code) letter = alphabet[pick(generator)];
  return code;
}

int random_seed() {
  auto generator = std::mt19937(std::random_device{}());
  return (int)(generator() & 0x3fffffff);
}

// How long the joining player waits before saying the code is wrong. The
// hosting player never gives up: it is waiting for a friend.
constexpr std::chrono::seconds JOIN_TIMEOUT{20};

// The one match being set up. The menu shows a single screen at a time, so
// one is enough, and handing back a pointer to it keeps the ownership here.
Connection_State matchmaking;

}  // namespace

void send_message(const Online& online, const nlohmann::json& message) {
  Channel& channel = get_channel(online);
  channel.outbox.push_back(message.dump());
  pump(online, channel);
}

std::optional<nlohmann::json> try_recv_message(
  const Online& online, const std::string& only_type
) {
  Channel& channel = get_channel(online);
  pump(online, channel);
  auto& queue = (only_type == "action") ? channel.actions : channel.others;
  if (queue.empty()) return std::nullopt;
  auto message = std::move(queue.front());
  queue.pop_front();
  return message;
}

void Connection_State::tick() {
  if (ready || !error.empty()) return;

  // Each player says hello once. The host's hello carries the seed, so both
  // sides deal the same cards.
  if (!said_hello) {
    auto hello    = nlohmann::json();
    hello["type"] = "hello";
    if (is_host) hello["seed"] = seed;
    send_message(online, hello);
    said_hello = true;
  }

  auto message = try_recv_message(online);
  if (!message) {
    if (!is_host && std::chrono::steady_clock::now() > give_up_at) {
      error = "No game with that code.";
    }
    return;
  }
  if (message->value("type", "") != "hello") return;

  if (!is_host) seed = message->value("seed", 0);
  // Both players work out the same seats from the seed they now share, so
  // the player who created the room does not always move first.
  int host_index = seed % 2;
  player_index   = is_host ? host_index : 1 - host_index;
  ready          = true;
}

Connection_State* start_hosting() {
  forget_room(matchmaking.room_code);
  matchmaking           = Connection_State();
  matchmaking.room_code = random_room_code();
  matchmaking.online    = {matchmaking.room_code, "host"};
  matchmaking.is_host   = true;
  matchmaking.seed      = random_seed();
  return &matchmaking;
}

Connection_State* join_room(const std::string& room_code) {
  forget_room(matchmaking.room_code);
  matchmaking            = Connection_State();
  matchmaking.room_code  = room_code;
  matchmaking.online     = {room_code, "join"};
  matchmaking.is_host    = false;
  matchmaking.give_up_at = std::chrono::steady_clock::now() + JOIN_TIMEOUT;
  return &matchmaking;
}

Online_Connection setup_local(bool host) {
  Connection_State* state = host ? start_hosting() : join_room("local");
  if (host) {
    state->room_code = "local";
    state->online    = {"local", "host"};
  }
  // No deadline here: the two instances are started by hand, one after the
  // other, and the second one can take a while to arrive.
  state->give_up_at = std::chrono::steady_clock::now() + std::chrono::hours(1);
  while (!state->ready && state->error.empty()) {
    state->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  auto result         = Online_Connection();
  result.online       = state->online;
  result.player_index = state->player_index;
  result.seed         = state->seed;
  return result;
}

std::optional<Online_Connection> setup_local_from_argv(int argc, char** argv) {
  bool host = false;
  bool join = false;
  for (int i = 1; i < argc; ++i) {
    auto argument = std::string(argv[i]);
    if (argument == "--local-host") host = true;
    else if (argument == "--local-join") join = true;
  }
  if (!host && !join) return std::nullopt;
  return setup_local(host);
}

int Agent_Remote::choose_action(Game& state, const Choice& choice) {
  (void)state;
  (void)choice;
  auto message = try_recv_message(online, "action");
  if (!message) return -1;
  if (!message->contains("index")) return -1;
  return (*message)["index"].get<int>();
}

int Agent_Local_Online::choose_action(Game& state, const Choice& choice) {
  int index = local_agent->choose_action(state, choice);
  if (index < 0) return -1;
  auto message     = nlohmann::json();
  message["type"]  = "action";
  message["index"] = index;
  send_message(online, message);
  return index;
}

Agent* make_online_duel(
  Agent* local_agent, const Online& online, int player_index
) {
  Agent* local_seat = new Agent_Local_Online(local_agent, online);
  Agent* other_seat = new Agent_Remote(online);
  return new Agent_Duel(local_seat, other_seat, /*swap=*/player_index != 0);
}
