#!/bin/bash

# Configuration
OUTPUT_ZIP="gods_app_package.zip"
TEMP_DIR="staging_area"

echo "Preparing distribution package..."

# 1. Create a temporary staging folder
mkdir -p $TEMP_DIR

# 2. Copy and rename files
# We rename gods_graphical.html to index.html so the server finds it automatically
cp build/gods_graphical_wasm/gods_graphical.html "$TEMP_DIR/index.html"
cp build/gods_graphical_wasm/gods_graphical.js "$TEMP_DIR/"
cp build/gods_graphical_wasm/gods_graphical.wasm "$TEMP_DIR/"
cp build/gods_graphical_wasm/gods_graphical.data "$TEMP_DIR/"

# 3. Create the 'run_app.sh' launcher inside the package
cat << 'EOF' > "$TEMP_DIR/run_app.sh"
#!/bin/bash
PORT=8000
# Since we renamed the file to index.html, we don't need the filename in the URL
URL="http://localhost:$PORT"

if command -v python3 &>/dev/null; then
    echo "Starting server at $URL"
    (sleep 1 && open "$URL" || xdg-open "$URL" || start "$URL") &
    python3 -m http.server $PORT
elif command -v python &>/dev/null; then
    echo "Starting server at $URL"
    (sleep 1 && open "$URL" || xdg-open "$URL" || start "$URL") &
    python -m http.server $PORT
else
    echo "Error: Python is not installed. Please install Python to run this app."
    exit 1
fi
EOF

# 4. Make the launcher executable
chmod +x "$TEMP_DIR/run_app.sh"

# 5. Zip the contents of the staging area
# -j (junk paths) ensures the files are at the root of the zip
zip -rj "$OUTPUT_ZIP" "$TEMP_DIR/"*

# 6. Clean up the temporary folder
rm -rf "$TEMP_DIR"

echo "------------------------------------------------"
echo "✅ Success! Archive created: $OUTPUT_ZIP"
echo "Send this zip file to your friend."