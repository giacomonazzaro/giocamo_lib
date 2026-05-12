#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

TORCH_CMAKE=$(python3 -c "import torch; print(torch.utils.cmake_prefix_path)" 2>/dev/null || true)

cmake -S "$SCRIPT_DIR/tressette" -B "$SCRIPT_DIR/tressette/build" \
  -DCMAKE_BUILD_TYPE=Release \
  ${TORCH_CMAKE:+-DCMAKE_PREFIX_PATH="$TORCH_CMAKE"}
cmake --build "$SCRIPT_DIR/tressette/build" --parallel 8

exec "$SCRIPT_DIR/tressette/build/tressette_graphical" "$@"
