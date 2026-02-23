#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV="$SCRIPT_DIR/.venv"

# Create venv and install dependencies on first run.
if [ ! -d "$VENV" ]; then
    echo "Setting up virtual environment (first run only)..."
    python3 -m venv "$VENV"
    echo "Installing required packages..."
    "$VENV/bin/pip" install --quiet "raylib>=5.5.0.3" "typer>=0.23.1" "pystun3>=2.0.0"
fi

exec "$VENV/bin/python" -m gods_graphical.main "$@"
