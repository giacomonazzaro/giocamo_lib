#pragma once
#include <atomic>
#include <string>
#ifndef __EMSCRIPTEN__
#include <thread>
#endif

// One HTTP request that does not block the game loop. Call start() once,
// then call done() every frame. When done() returns true, read `succeeded`
// and `response`, then call clear() before starting the next request.
//
// Native: a std::thread runs libcurl in the background.
// Web:    emscripten_fetch calls back on the main thread when the browser
//         is done with the request.
struct Http_Request {
  Http_Request() = default;
  ~Http_Request();
  Http_Request(const Http_Request&)            = delete;
  Http_Request& operator=(const Http_Request&) = delete;

  // `method` is "GET" or "PUT". `body` is empty for a GET.
  void start(
    const std::string& method, const std::string& url, const std::string& body
  );

  enum State { IDLE, RUNNING, DONE };
  bool running() const { return state.load() == RUNNING; }
  bool done() const { return state.load() == DONE; }
  void clear() { state.store(IDLE); }

  // Written by the transport when the request lands, read after done().
  // `succeeded` is false for any answer that is not 2xx, including a
  // request that never reached the server.
  std::atomic<int> state{IDLE};
  bool             succeeded = false;
  std::string      response;
  // emscripten_fetch does not copy the request body, so the bytes have to
  // stay alive until the callback fires.
  std::string body_storage;
#ifndef __EMSCRIPTEN__
  std::thread worker;
#endif
};
