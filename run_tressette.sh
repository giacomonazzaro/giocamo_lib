#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cmake -S "$SCRIPT_DIR/tressette" -B "$SCRIPT_DIR/tressette/build" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$SCRIPT_DIR/tressette/build" --parallel 8

exec "$SCRIPT_DIR/tressette/build/tressette_graphical" "$@"
