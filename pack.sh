#!/bin/bash
set -e

# Usage: sh pack.sh <game>   (default: gods)
GAME="${1:-gods}"
SOURCE_DIR="${GAME}_app"
BUILD_DIR="build/${SOURCE_DIR}_wasm"
OUTPUT_ZIP="${SOURCE_DIR}_package.zip"
TEMP_DIR="staging_${SOURCE_DIR}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -d "$SOURCE_DIR" ]; then
    echo "ERROR: no source directory '$SOURCE_DIR' (try: sh pack.sh gods|scopa|tressette)"
    exit 1
fi

# Activate Emscripten from the standard install location.
EMSDK_ENV="$HOME/emsdk/emsdk_env.sh"
if [ ! -f "$EMSDK_ENV" ]; then
    echo "ERROR: $EMSDK_ENV not found."
    echo "Install emsdk first:"
    echo "  cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest"
    exit 1
fi
# shellcheck disable=SC1090
source "$EMSDK_ENV"

# Build the wasm artifacts.
emcmake cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 8

for ext in html js wasm data; do
    if [ ! -f "$BUILD_DIR/${SOURCE_DIR}.${ext}" ]; then
        echo "ERROR: build produced no $BUILD_DIR/${SOURCE_DIR}.${ext}"
        exit 1
    fi
done

echo "Preparing distribution package for $GAME..."

# 1. Clean staging folder.
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR"

# 2. Copy artifacts. The .html is renamed to index.html so the launcher's
#    bare URL (no filename) finds it automatically.
cp "$BUILD_DIR/${SOURCE_DIR}.html" "$TEMP_DIR/index.html"
cp "$BUILD_DIR/${SOURCE_DIR}.js"   "$TEMP_DIR/"
cp "$BUILD_DIR/${SOURCE_DIR}.wasm" "$TEMP_DIR/"
cp "$BUILD_DIR/${SOURCE_DIR}.data" "$TEMP_DIR/"

# 3. Launcher script: starts a local server and opens the browser. The wasm
#    build talks to the Firebase database for online play, so nothing else
#    is needed here — only a local HTTP server to serve the page.
cat << 'EOF' > "$TEMP_DIR/run_app.sh"
#!/bin/bash
PORT=8000
URL="http://localhost:$PORT"

open_browser() {
    sleep 1
    if command -v open >/dev/null 2>&1; then open "$URL"
    elif command -v xdg-open >/dev/null 2>&1; then xdg-open "$URL"
    elif command -v start >/dev/null 2>&1; then start "$URL"
    fi
}

if command -v python3 >/dev/null 2>&1; then
    PY=python3
elif command -v python >/dev/null 2>&1; then
    PY=python
else
    echo "Error: Python is not installed. Please install Python to run this app."
    exit 1
fi

echo "Starting server at $URL"
open_browser &
"$PY" -m http.server "$PORT"
EOF
chmod +x "$TEMP_DIR/run_app.sh"

# 4. Zip flat (-j) so the files sit at the root of the archive.
rm -f "$OUTPUT_ZIP"
zip -rj "$OUTPUT_ZIP" "$TEMP_DIR/"*

# 5. Clean up.
rm -rf "$TEMP_DIR"

echo "------------------------------------------------"
echo "Success! Archive created: $OUTPUT_ZIP"
echo "Send this zip to your friend; unzip and run ./run_app.sh."