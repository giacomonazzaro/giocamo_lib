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

BUILD_DIR="build/gods_graphical_wasm"

emcmake cmake -S gods_graphical -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 8

URL="http://localhost:8080/gods_graphical.html"
echo ""
echo "Starting server at $URL"

# Start server in background, open the browser, then wait for Ctrl-C.
python3 -m http.server 8080 --directory "$BUILD_DIR" &
SERVER_PID=$!
sleep 0.5
open "$URL"

trap "kill $SERVER_PID 2>/dev/null" EXIT
wait $SERVER_PID
