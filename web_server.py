#!/usr/bin/env python3
"""Dev server for the wasm build. Serves static files (the contents of the
build directory) AND answers the two Firebase Realtime Database requests the
game makes, so online play can be tested with no Google account and no
Internet. Point a build at it with FIREBASE_URL=http://localhost:8080, or in
the browser with ?firebase=http://localhost:8080.

The two requests are:
    PUT /rooms/<code>/<seat>/<slot>.json          body = one message
    GET /rooms/<code>/<seat>.json?orderBy="$key"&startAt="<slot>"

Usage: python3 web_server.py <serve-dir> [port]
"""

import http.server
import json
import os
import sys
import threading
from urllib.parse import urlparse, parse_qs, unquote

# Full path (without the .json suffix) -> stored message text.
store: dict[str, str] = {}
store_lock = threading.Lock()


def log(message: str) -> None:
    print(message, flush=True)


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args, **kwargs):  # Silence the default access log.
        pass

    def answer(self, body: bytes) -> None:
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_PUT(self):
        path = urlparse(self.path).path
        if not path.endswith(".json"):
            self.send_response(404)
            self.end_headers()
            return
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8") if length else ""
        with store_lock:
            store[path[1:-len(".json")]] = body
        log(f"PUT  {path}  {body[:100]}")
        # Firebase answers a PUT with the value that was written.
        self.answer(body.encode("utf-8"))

    def do_GET(self):
        parsed = urlparse(self.path)
        if not parsed.path.endswith(".json"):
            # Anything else is a static file from the served directory.
            super().do_GET()
            return
        prefix = parsed.path[1:-len(".json")] + "/"
        query = parse_qs(parsed.query)
        start_at = unquote(query.get("startAt", ['""'])[0]).strip('"')
        with store_lock:
            children = {
                key[len(prefix):]: value
                for key, value in store.items()
                if key.startswith(prefix) and key[len(prefix):] >= start_at
            }
        if children:
            body = "{" + ",".join(
                json.dumps(key) + ":" + value for key, value in children.items()
            ) + "}"
        else:
            body = "null"
        log(f"GET  {parsed.path} startAt={start_at} -> {len(children)} messages")
        self.answer(body.encode("utf-8"))


def main():
    serve_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    os.chdir(serve_dir)
    server = http.server.ThreadingHTTPServer(("0.0.0.0", port), Handler)
    print(f"serving {serve_dir} + fake database on http://localhost:{port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
