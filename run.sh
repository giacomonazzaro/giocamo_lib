#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

cmake -S gods_graphical_cpp -B gods_graphical_cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build gods_graphical_cpp/build --parallel -1

exec ./gods_graphical_cpp/build/gods_graphical_cpp "$@"
