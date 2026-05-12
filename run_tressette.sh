#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cmake -S "$SCRIPT_DIR/tressette/graphical" -B "$SCRIPT_DIR/tressette/graphical/build" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$SCRIPT_DIR/tressette/graphical/build" --parallel 8

exec "$SCRIPT_DIR/tressette/graphical/build/tressette_graphical" "$@"
