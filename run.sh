#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV="$SCRIPT_DIR/venv"

# Create venv and install dependencies on first run.
if [ ! -d "$VENV" ]; then
    echo "Setting up virtual environment (first run only)..."
    # Python 3.10+ is required. On Windows (MINGW64), python3.12 is not a valid command.
    PYTHON=$(command -v python3.12 || command -v python3 || command -v python)
    "$PYTHON" -m venv "$VENV"
    echo "Installing required packages..."
    "$VENV/bin/pip" install --quiet "raylib>=5.5.0.3" "typer>=0.23.1" "pystun3>=2.0.0"
fi

cmake -S tabletop2D -B tabletop2D/build \
  -DPython_EXECUTABLE=venv/bin/python \
  -DCMAKE_BUILD_TYPE=Release
cmake --build tabletop2D/build --parallel 4
cmake --install tabletop2D/build


exec "$VENV/bin/python" -m gods_graphical.main "$@"
