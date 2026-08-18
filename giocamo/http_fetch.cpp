#include "http.h"

#include <emscripten/fetch.h>

#include <cstring>

// Fires on the main thread once the browser is done with the request.
static void on_request_done(emscripten_fetch_t* fetch) {
  auto* request      = static_cast<Http_Request*>(fetch->userData);
  request->succeeded = fetch->status >= 200 && fetch->status < 300;
  if (request->succeeded && fetch->numBytes > 0) {
    request->response.assign(fetch->data, fetch->numBytes);
  }
  request->state.store(Http_Request::DONE);
  emscripten_fetch_close(fetch);
}

Http_Request::~Http_Request() = default;

void Http_Request::start(
  const std::string& method, const std::string& url, const std::string& body
) {
  if (state.load() != IDLE) return;
  response.clear();
  succeeded    = false;
  body_storage = body;
  state.store(RUNNING);

  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  std::strncpy(
    attr.requestMethod, method.c_str(), sizeof(attr.requestMethod) - 1
  );
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  if (!body_storage.empty()) {
    attr.requestData     = body_storage.data();
    attr.requestDataSize = body_storage.size();
  }
  attr.userData  = this;
  attr.onsuccess = on_request_done;
  attr.onerror   = on_request_done;
  emscripten_fetch(&attr, url.c_str());
}
