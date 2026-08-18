#include "http.h"

#include <curl/curl.h>

namespace {

// libcurl needs one global init before any easy handle is made in a
// multi-threaded process. The static makes that happen once.
struct Curl_Global {
  Curl_Global() { curl_global_init(CURL_GLOBAL_ALL); }
};
Curl_Global curl_global;

size_t write_to_string(void* contents, size_t size, size_t count, void* user) {
  size_t total = size * count;
  static_cast<std::string*>(user)->append((char*)contents, total);
  return total;
}

// Runs on the worker thread. Returns false when the server did not answer
// with a 2xx status, so the caller knows to send the message again.
bool perform(
  const std::string& method,
  const std::string& url,
  const std::string& body,
  std::string&       response
) {
  CURL* curl = curl_easy_init();
  if (!curl) return false;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  // NOSIGNAL is required on a worker thread: without it libcurl installs
  // SIGALRM handlers that race. TIMEOUT caps a stalled connection.
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
  if (!body.empty()) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
  }
  CURLcode code   = curl_easy_perform(curl);
  long     status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_easy_cleanup(curl);
  return code == CURLE_OK && status >= 200 && status < 300;
}

}  // namespace

Http_Request::~Http_Request() {
  // Detach rather than join: the process is exiting and joining would wait
  // out libcurl's 20 second timeout.
  if (worker.joinable()) worker.detach();
}

void Http_Request::start(
  const std::string& method, const std::string& url, const std::string& body
) {
  if (state.load() != IDLE) return;
  if (worker.joinable()) worker.join();
  response.clear();
  succeeded = false;
  state.store(RUNNING);
  worker = std::thread([this, method, url, body]() {
    succeeded = perform(method, url, body, response);
    state.store(DONE);
  });
}
