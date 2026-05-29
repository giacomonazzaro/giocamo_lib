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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

// Relay base URL. Both wasm and native default to the public ntfy.sh
// service so two peers — on different machines, different networks, or
// across the desktop/browser line — can find each other without anyone
// running a server. Either platform can override via the NTFY_URL
// environment variable (or, in the browser, by appending `?ntfy=...` to
// the page URL) — handy when ntfy.sh is unreachable and you want to point
// at a local web_server.py instead.
std::string ntfy_base_url() {
  static std::string base = []() {
    std::string s;
    const char* env = std::getenv("NTFY_URL");
    if (env && *env) s = env;
#ifdef __EMSCRIPTEN__
    if (s.empty()) {
      char* override_url = (char*)EM_ASM_PTR({
        var p = new URLSearchParams(window.location.search).get('ntfy');
        if (!p) return 0;
        var len = lengthBytesUTF8(p) + 1;
        var buf = _malloc(len);
        stringToUTF8(p, buf, len);
        return buf;
      });
      if (override_url) {
        s = override_url;
        free(override_url);
      }
    }
#endif
    if (s.empty()) s = "https://ntfy.sh";
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
  // Wall-clock time of the next allowed poll. ntfy.sh rate-limits each
  // subscriber IP (~30 req/min sustained); without throttling the per-
  // frame pump() would burn through that budget in seconds and get the
  // whole IP blocked for the rest of the session.
  std::chrono::steady_clock::time_point next_poll_at =
    std::chrono::steady_clock::now();
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

// Minimum gap between consecutive polls. ntfy.sh's public service caps
// subscribers at ~30 req/min sustained; 2 seconds keeps us comfortably
// under that (peak 30 req/min) for two concurrent inboxes (one per topic
// direction) and still feels instant in-game.
static constexpr std::chrono::milliseconds POLL_INTERVAL{2000};

// Drain a completed fetch (if any) into the inbox, then start the next —
// but at most once per POLL_INTERVAL so we don't blow through the relay's
// rate limit.
void pump(Inbox& inbox, const std::string& topic) {
  std::string body = inbox.fetch.consume();
  if (!body.empty()) parse_into_inbox(body, inbox);
  if (inbox.fetch.busy()) return;
  auto now = std::chrono::steady_clock::now();
  if (now < inbox.next_poll_at) return;
  inbox.next_poll_at = now + POLL_INTERVAL;
  std::string url    = ntfy_base_url() + topic + "/json?poll=1";
  if (inbox.since_marker > 0)
    url += "&since=" + std::to_string(inbox.since_marker);
  inbox.fetch.start(url);
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
