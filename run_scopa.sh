#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BUILD_DIR="$SCRIPT_DIR/build/scopa_app"

cmake -S "$SCRIPT_DIR/scopa_app" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 8

exec "$BUILD_DIR/scopa_app" "$@"
