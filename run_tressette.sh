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

# tabletop is shared with Gods.
cmake -S tabletop -B tabletop/build \
  -DPython_EXECUTABLE="$VENV/bin/python" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build tabletop/build --parallel 4
cmake --install tabletop/build

cmake -S game -B game/build -DCMAKE_BUILD_TYPE=Release
cmake --build game/build --parallel 4

cmake -S tressette -B tressette/build \
  -DPython_EXECUTABLE="$VENV/bin/python" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build tressette/build --parallel 4
cmake --install tressette/build

exec "$VENV/bin/python" -m tressette.graphical.main "$@"
