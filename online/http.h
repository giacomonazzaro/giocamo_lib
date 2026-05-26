#pragma once
#include <atomic>
#include <string>
#ifndef __EMSCRIPTEN__
#include <thread>
#endif

// Blocking HTTP POST. Returns response body or empty string on error.
std::string http_post(const std::string& url, const std::string& body);

// Blocking HTTP GET. Used for ad-hoc fetches; for polling loops use
// Async_Get below so the game thread never stalls.
std::string http_get(const std::string& url);

// Non-blocking HTTP GET. Owner calls start(url) once, then on each frame
// calls consume() — empty string means "no response yet", a non-empty
// string is the body (consumed). After consuming, start() can be called
// again to fire the next request. busy() reports whether a fetch is in
// flight (i.e. start has been called and consume hasn't drained it).
//
// Native: a std::thread runs http_get in the background.
// Wasm:   emscripten_fetch fires asynchronously and the onsuccess callback
//         flips state to DONE on the main thread (no extra threads needed).
class Async_Get {
 public:
  Async_Get();
  ~Async_Get();
  Async_Get(const Async_Get&)            = delete;
  Async_Get& operator=(const Async_Get&) = delete;

  void        start(const std::string& url);
  std::string consume();
  bool        busy() const;

  // Internal state, exposed only because the wasm onsuccess callback needs
  // to write to it from a free function. Don't poke from outside.
  enum State { IDLE, RUNNING, DONE };
  std::atomic<int> state{IDLE};
  std::string      result;
#ifndef __EMSCRIPTEN__
  std::thread worker;
#endif
};
