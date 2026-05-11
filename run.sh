#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

cmake -S gods_graphical -B gods_graphical/build -DCMAKE_BUILD_TYPE=Release
cmake --build gods_graphical/build --parallel 8

exec ./gods_graphical/build/gods_graphical "$@"
