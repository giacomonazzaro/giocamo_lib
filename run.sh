#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="build/gods_graphical"

cmake -S gods_graphical -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 8

exec "./$BUILD_DIR/gods_graphical" "$@"
