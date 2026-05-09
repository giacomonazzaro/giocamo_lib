#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV="$SCRIPT_DIR/venv"

# Reuse the Gods venv (Tressette has no extra runtime deps).
if [ ! -d "$VENV" ]; then
    echo "Setting up virtual environment (first run only)..."
    PYTHON=$(command -v python3.12 || command -v python3 || command -v python)
    "$PYTHON" -m venv "$VENV"
    "$VENV/bin/pip" install --quiet "raylib>=5.5.0.3" "typer>=0.23.1"
fi

# kitchen_table_cpp is shared with Gods.
cmake -S kitchen_table_cpp -B kitchen_table_cpp/build \
  -DPython_EXECUTABLE="$VENV/bin/python" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build kitchen_table_cpp/build --parallel 4
cmake --install kitchen_table_cpp/build

cmake -S game_cpp -B game_cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build game_cpp/build --parallel 4

cmake -S tressette/cpp -B tressette/cpp/build \
  -DPython_EXECUTABLE="$VENV/bin/python" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build tressette/cpp/build --parallel 4
cmake --install tressette/cpp/build

exec "$VENV/bin/python" -m tressette.graphical.main "$@"
