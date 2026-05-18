#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

TORCH_CMAKE=$(python3 -c "import torch; print(torch.utils.cmake_prefix_path)" 2>/dev/null || true)

BUILD_DIR="$SCRIPT_DIR/build/tressette"

cmake -S "$SCRIPT_DIR/tressette" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  ${TORCH_CMAKE:+-DCMAKE_PREFIX_PATH="$TORCH_CMAKE"}
cmake --build "$BUILD_DIR" --parallel 8

exec "$BUILD_DIR/tressette_graphical" "$@"
