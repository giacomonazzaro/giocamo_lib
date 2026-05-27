#include "http.h"

#include <emscripten/fetch.h>

#include <cstring>
#include <string>

// Body owner: emscripten_fetch keeps a pointer to the request body but
// doesn't copy it, so we have to keep the bytes alive until onsuccess /
// onerror fires. The free function deletes the owner.
static void close_with_owner(emscripten_fetch_t* fetch) {
  auto* owner = static_cast<std::string*>(fetch->userData);
  delete owner;
  emscripten_fetch_close(fetch);
}

// Fire-and-forget POST. We don't use the response, and Chrome blocks
// synchronous XHR on the main thread (EMSCRIPTEN_FETCH_SYNCHRONOUS is
// pthread-only on the browser), so this MUST be async.
std::string http_post(const std::string& url, const std::string& body) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  std::strcpy(attr.requestMethod, "POST");
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  auto* owner            = new std::string(body);
  attr.requestData       = owner->data();
  attr.requestDataSize   = owner->size();
  attr.userData          = owner;
  attr.onsuccess         = close_with_owner;
  attr.onerror           = close_with_owner;
  emscripten_fetch(&attr, url.c_str());
  return "";
}

// Blocking GET. Currently unused on wasm (Async_Get drives polling
// directly), but kept for API parity with the native impl in case anything
// else picks it up. Synchronous + ASYNCIFY is OK on the main thread for a
// GET with no request body — Chrome's sync-XHR block is specifically for
// requests that have a body to upload.
std::string http_get(const std::string& url) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  std::strcpy(attr.requestMethod, "GET");
  attr.attributes =
    EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
  emscripten_fetch_t* fetch = emscripten_fetch(&attr, url.c_str());
  std::string         response;
  if (fetch && fetch->status == 200 && fetch->numBytes > 0) {
    response.assign(fetch->data, fetch->numBytes);
  }
  if (fetch) emscripten_fetch_close(fetch);
  return response;
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
