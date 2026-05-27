#include "protocol.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

#include "http.h"

namespace {

// Relay base URL.
//   - Wasm defaults to "/ntfy/" — a same-origin path served by
//     web_server.py, so two browser tabs always reach the same relay with
//     no external network dependency.
//   - Native defaults to the public ntfy.sh service.
// Either platform can override via the NTFY_URL environment variable
// (e.g. to point native peers at a local web_server.py).
std::string ntfy_base_url() {
  static std::string base = []() {
    const char* env = std::getenv("NTFY_URL");
    std::string s;
    if (env && *env) {
      s = env;
    } else {
#ifdef __EMSCRIPTEN__
      s = "/ntfy";
#else
      s = "https://ntfy.sh";
#endif
    }
    if (s.empty() || s.back() != '/') s += "/";
    return s;
  }();
  return base;
}

// Per-topic state. Two queues so the main loop and Agent_Remote can drain
// independently — actions go to the agent, everything else to the loop's
// side-band handler. The Async_Get keeps a single fetch in flight at all
// times so we never block the game thread on HTTP.
struct Inbox {
  std::deque<nlohmann::json> actions;
  std::deque<nlohmann::json> other;
  std::mutex                 lock;
  // Seed `since_marker` to "now" the first time we ever look at a topic so
  // ntfy.sh doesn't dump stale matchmaking messages from previous test runs
  // (each peer would otherwise see another peer's seed from yesterday and
  // both would end up player 0 with mismatched seats).
  long                       since_marker = 0;
  // Already-seen message ids — ntfy's `since` is timestamp-granular (1s),
  // so polls naturally re-deliver messages from the same second. Dedup by
  // id so we don't process them twice.
  std::set<std::string>      seen_ids;
  Async_Get                  fetch;
};

// Heap-allocated and leaked at process exit so Async_Get worker threads
// can write to their inboxes without worrying about lifetime races.
std::map<std::string, Inbox*> s_inboxes;
std::mutex                    s_inboxes_lock;

Inbox& get_inbox(const std::string& topic) {
  std::lock_guard<std::mutex> lg(s_inboxes_lock);
  Inbox*&                     slot = s_inboxes[topic];
  if (!slot) {
    slot = new Inbox();
    // Skip everything posted to this topic before we started polling.
    slot->since_marker = (long)time(nullptr);
  }
  return *slot;
}

// ntfy.sh /<topic>/json?poll=1 returns one JSON event per line:
//   {"id":"…","time":N,"event":"message","message":"<our-stringified-json>"}
// Parse, advance the since_marker, and bucket payloads by "type".
void parse_into_inbox(const std::string& body, Inbox& inbox) {
  if (body.empty()) return;
  std::stringstream stream(body);
  std::string       line;
  while (std::getline(stream, line)) {
    if (line.empty()) continue;
    try {
      auto event = nlohmann::json::parse(line);
      if (event.value("event", "") != "message") continue;
      // Dedup by id BEFORE bumping since_marker so re-delivered messages
      // from the same second don't get processed twice.
      std::string id = event.value("id", "");
      if (!id.empty() && !inbox.seen_ids.insert(id).second) continue;
      if (event.contains("time")) {
        long t             = event["time"].get<long>();
        // No +1: a same-second message would otherwise be filtered out by
        // the next poll's `since=` — dedup handles re-delivery.
        inbox.since_marker = std::max(inbox.since_marker, t);
      }
      auto payload = nlohmann::json::parse(event.value("message", "{}"));
      if (payload.value("type", "") == "action")
        inbox.actions.push_back(std::move(payload));
      else
        inbox.other.push_back(std::move(payload));
    } catch (...) {
      // Ignore malformed lines — ntfy occasionally sends keepalives.
    }
  }
}

// Drain a completed fetch (if any) into the inbox, then start the next.
// Called every time anyone polls the inbox so the chain keeps running.
void pump(Inbox& inbox, const std::string& topic) {
  std::string body = inbox.fetch.consume();
  if (!body.empty()) parse_into_inbox(body, inbox);
  if (!inbox.fetch.busy()) {
    std::string url = ntfy_base_url() + topic + "/json?poll=1";
    if (inbox.since_marker > 0)
      url += "&since=" + std::to_string(inbox.since_marker);
    inbox.fetch.start(url);
  }
}

}  // namespace

void send_message(const Online& online, const nlohmann::json& data) {
  http_post(ntfy_base_url() + online.topic_send, data.dump());
}

std::optional<nlohmann::json> try_recv_message(
  const Online& online, const std::string& only_type
) {
  Inbox&                      inbox = get_inbox(online.topic_recv);
  std::lock_guard<std::mutex> lg(inbox.lock);
  pump(inbox, online.topic_recv);
  auto& queue = (only_type == "action") ? inbox.actions : inbox.other;
  if (queue.empty()) return std::nullopt;
  auto msg = std::move(queue.front());
  queue.pop_front();
  return msg;
}

nlohmann::json recv_message(const Online& online) {
  while (true) {
    auto msg = try_recv_message(online, "");
    if (msg) return *msg;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}
