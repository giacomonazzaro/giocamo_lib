#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Activate Emscripten from the standard install location.
EMSDK_ENV="$HOME/emsdk/emsdk_env.sh"
if [ ! -f "$EMSDK_ENV" ]; then
    echo "ERROR: $EMSDK_ENV not found."
    echo "Install emsdk first:"
    echo "  cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest"
    exit 1
fi
# shellcheck disable=SC1090
source "$EMSDK_ENV"

BUILD_DIR="build/gods_app_wasm"

emcmake cmake -S gods_app -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 8

URL="http://localhost:8080/gods_app.html"
echo ""
echo "Starting server at $URL"

# Start the dev server in the background. web_server.py serves the build
# directory AND runs a tiny same-origin ntfy.sh-compatible relay at /ntfy/
# so the wasm peers don't need any external network.
python3 "$SCRIPT_DIR/web_server.py" "$BUILD_DIR" 8080 &
SERVER_PID=$!
sleep 0.5
open "$URL"

trap "kill $SERVER_PID 2>/dev/null" EXIT
wait $SERVER_PID
