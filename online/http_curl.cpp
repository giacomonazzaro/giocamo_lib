#include "http.h"

#include <curl/curl.h>

namespace {

// libcurl needs global init once before any easy handles in a multi-threaded
// process. The static guarantees it happens once across all threads.
struct Curl_Global {
  Curl_Global() { curl_global_init(CURL_GLOBAL_ALL); }
};
static Curl_Global s_curl_global;

// libcurl write callback that appends bytes to a std::string.
size_t write_to_string(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t total = size * nmemb;
  static_cast<std::string*>(userp)->append((char*)contents, total);
  return total;
}

// Common setup: NOSIGNAL is required for multi-threaded use (libcurl
// otherwise installs SIGALRM handlers that race across threads), TIMEOUT
// caps a stalled connection so we don't block the worker forever.
void common_setup(CURL* curl, std::string& response) {
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
}

}  // namespace

std::string http_post(const std::string& url, const std::string& body) {
  CURL* curl = curl_easy_init();
  if (!curl) return "";
  std::string response;
  common_setup(curl, response);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  return response;
}

std::string http_get(const std::string& url) {
  CURL* curl = curl_easy_init();
  if (!curl) return "";
  std::string response;
  common_setup(curl, response);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  return response;
}

// --- Async_Get -----------------------------------------------------------

Async_Get::Async_Get() = default;

Async_Get::~Async_Get() {
  // Detach instead of join — process is exiting and we don't want a 20-second
  // wait on libcurl's timeout. Worker writes to *this; if it's still running
  // when the destructor runs we'd have a problem, but in practice these are
  // file-scope long-lived inboxes that live until process exit.
  if (worker.joinable()) worker.detach();
}

void Async_Get::start(const std::string& url) {
  if (state.load() != IDLE) return;
  if (worker.joinable()) worker.join();
  state.store(RUNNING);
  worker = std::thread([this, url]() {
    result = http_get(url);
    state.store(DONE);
  });
}

std::string Async_Get::consume() {
  if (state.load() != DONE) return "";
  if (worker.joinable()) worker.join();
  std::string body = std::move(result);
  result.clear();
  state.store(IDLE);
  return body;
}

bool Async_Get::busy() const { return state.load() != IDLE; }
