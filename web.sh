#!/bin/bash
set -e

GAME="${1:-gods}"
SOURCE_DIR="${GAME}_app"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -d "$SOURCE_DIR" ]; then
    echo "ERROR: no source directory '$SOURCE_DIR' (try: sh web.sh gods|scopa|tressette)"
    exit 1
fi

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

BUILD_DIR="build/${SOURCE_DIR}_wasm"

emcmake cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 8

URL="http://localhost:8080/${SOURCE_DIR}.html"
echo ""
echo "Starting server at $URL"

# Start the dev server in the background. web_server.py serves the build
# directory AND answers the two Firebase requests the game makes, so adding
# ?firebase=http://localhost:8080 to the page URL tests online play without
# touching the real database.
python3 "$SCRIPT_DIR/web_server.py" "$BUILD_DIR" 8080 &
SERVER_PID=$!
sleep 0.5
open "$URL"

trap "kill $SERVER_PID 2>/dev/null" EXIT
wait $SERVER_PID
