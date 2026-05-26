#include "http.h"

#include <emscripten/fetch.h>

#include <cstring>
#include <string>

// Synchronous send is fine — happens once per turn at most, and ASYNCIFY
// keeps the JS event loop ticking during the wait.
static std::string do_sync_fetch(
  const char* method, const std::string& url, const std::string& body
) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  for (int i = 0; method[i] && i < 31; ++i) attr.requestMethod[i] = method[i];
  attr.attributes =
    EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
  if (!body.empty()) {
    attr.requestData     = body.c_str();
    attr.requestDataSize = body.size();
  }
  emscripten_fetch_t* fetch = emscripten_fetch(&attr, url.c_str());
  std::string         response;
  if (fetch && fetch->status == 200 && fetch->numBytes > 0) {
    response.assign(fetch->data, fetch->numBytes);
  }
  if (fetch) emscripten_fetch_close(fetch);
  return response;
}

std::string http_post(const std::string& url, const std::string& body) {
  return do_sync_fetch("POST", url, body);
}

std::string http_get(const std::string& url) {
  return do_sync_fetch("GET", url, "");
}

// --- Async_Get -----------------------------------------------------------

// onsuccess / onerror fire on the main thread once the browser fetch lands;
// they flip our state to DONE so the next consume() call drains the body.
static void on_fetch_done(emscripten_fetch_t* fetch) {
  auto* self = static_cast<Async_Get*>(fetch->userData);
  if (fetch->status == 200 && fetch->numBytes > 0) {
    self->result.assign(fetch->data, fetch->numBytes);
  } else {
    self->result.clear();
  }
  self->state.store(Async_Get::DONE);
  emscripten_fetch_close(fetch);
}

Async_Get::Async_Get()  = default;
Async_Get::~Async_Get() = default;

void Async_Get::start(const std::string& url) {
  if (state.load() != IDLE) return;
  state.store(RUNNING);
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  std::strcpy(attr.requestMethod, "GET");
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  attr.userData   = this;
  attr.onsuccess  = on_fetch_done;
  attr.onerror    = on_fetch_done;
  emscripten_fetch(&attr, url.c_str());
}

std::string Async_Get::consume() {
  if (state.load() != DONE) return "";
  std::string body = std::move(result);
  result.clear();
  state.store(IDLE);
  return body;
}

bool Async_Get::busy() const { return state.load() != IDLE; }
