#!/usr/bin/env python3
"""Dev server for the wasm build. Serves static files (the contents of the
build directory) AND speaks a tiny subset of ntfy.sh's API at /ntfy/<topic>.
Same-origin, so the wasm code talks to the relay without any external
network dependency — handy when ntfy.sh is unreachable.

Usage: python3 web_server.py <serve-dir> [port]
"""

import http.server
import json
import os
import sys
import threading
import time
from urllib.parse import urlparse, parse_qs

# topic -> list of {id, time, event, message}
topics: dict[str, list[dict]] = {}
topics_lock = threading.Lock()
next_id = 0


def add_msg(topic: str, body: str) -> None:
    global next_id
    with topics_lock:
        next_id += 1
        topics.setdefault(topic, []).append({
            "id": f"id{next_id}",
            "time": int(time.time()),
            "event": "message",
            "message": body,
            "topic": topic,
        })


def log(msg: str) -> None:
    print(msg, flush=True)


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args, **kwargs):  # silence default access log
        pass

    # ntfy publish: POST /ntfy/<topic>  body = stringified payload.
    def do_POST(self):
        u = urlparse(self.path)
        if u.path.startswith("/ntfy/"):
            topic = u.path[len("/ntfy/"):].strip("/")
            n = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(n).decode("utf-8") if n else ""
            add_msg(topic, body)
            log(f"POST {topic}  body={body[:120]}")
            self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            return
        log(f"POST 404 path={self.path!r}")
        self.send_response(404)
        self.end_headers()

    # ntfy subscribe-poll: GET /ntfy/<topic>/json?poll=1&since=<unix-seconds>
    def do_GET(self):
        u = urlparse(self.path)
        if u.path.startswith("/ntfy/"):
            rest = u.path[len("/ntfy/"):]
            parts = rest.split("/")
            if len(parts) != 2 or parts[1] != "json":
                log(f"GET 404 ntfy path={self.path!r}")
                self.send_response(404)
                self.end_headers()
                return
            topic = parts[0]
            qs = parse_qs(u.query)
            since_raw = qs.get("since", ["0"])[0]
            since = int(since_raw) if since_raw.isdigit() else 0
            with topics_lock:
                msgs = list(topics.get(topic, []))
            out = [m for m in msgs if m["time"] >= since]
            body = "".join(json.dumps(m) + "\n" for m in out).encode("utf-8")
            log(f"GET  {topic}  since={since}  -> {len(out)} msgs ({len(body)}B)")
            self.send_response(200)
            self.send_header("Content-Type", "application/x-ndjson")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        # Fall through to static-file serving from the configured root.
        super().do_GET()


def main():
    serve_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    os.chdir(serve_dir)
    server = http.server.ThreadingHTTPServer(("0.0.0.0", port), Handler)
    print(f"serving {serve_dir} + ntfy relay on http://localhost:{port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
