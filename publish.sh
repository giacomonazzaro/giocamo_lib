#!/bin/bash
set -e

# Usage: sh publish.sh <game>   (default: gods)
#
# Builds the wasm version of <game>_app and publishes it to GitHub Pages
# under a single deploy repo (default name: giocamo). Each game lives
# in its own subfolder, so one repo hosts gods, scopa, tressette, and any
# future game you add.
#
# Output URL: https://<your-github-user>.github.io/<repo>/<game>/
#
# Override the deploy repo with: PUBLISH_REPO=my-games sh publish.sh scopa

GAME="${1:-gods}"
REPO_NAME="${PUBLISH_REPO:-giocamo}"
SOURCE_DIR="${GAME}_app"
BUILD_DIR="build/${SOURCE_DIR}_wasm"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -d "$SOURCE_DIR" ]; then
    echo "ERROR: no source directory '$SOURCE_DIR' (try: sh publish.sh gods|scopa|tressette|...)"
    exit 1
fi

# 1. Prereqs: GitHub CLI must be installed and logged in.
if ! command -v gh >/dev/null 2>&1; then
    echo "ERROR: 'gh' (GitHub CLI) is not installed."
    echo "Install it once:    brew install gh"
    echo "Then log in:        gh auth login"
    exit 1
fi
if ! gh auth status >/dev/null 2>&1; then
    echo "ERROR: GitHub CLI is not authenticated. Run once:"
    echo "    gh auth login"
    exit 1
fi

# 2. Build the wasm artifacts.
EMSDK_ENV="$HOME/emsdk/emsdk_env.sh"
if [ ! -f "$EMSDK_ENV" ]; then
    echo "ERROR: $EMSDK_ENV not found. Install emsdk first."
    exit 1
fi
# shellcheck disable=SC1090
source "$EMSDK_ENV"
emcmake cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel 8

for ext in html js wasm data; do
    if [ ! -f "$BUILD_DIR/${SOURCE_DIR}.${ext}" ]; then
        echo "ERROR: build produced no $BUILD_DIR/${SOURCE_DIR}.${ext}"
        exit 1
    fi
done

# 3. Make sure the deploy repo exists on your GitHub account.
GH_USER=$(gh api user --jq .login)
if ! gh repo view "$GH_USER/$REPO_NAME" >/dev/null 2>&1; then
    echo "Creating deploy repo $GH_USER/$REPO_NAME..."
    gh repo create "$GH_USER/$REPO_NAME" --public \
        --description "Web builds of gods-app games"
fi

# 4. Clone (or refresh) a local working copy of the deploy repo.
WORKDIR=".publish_clone"
EXPECTED_REMOTE="$GH_USER/$REPO_NAME"
CACHED_REMOTE=$(git -C "$WORKDIR" config --get remote.origin.url 2>/dev/null || true)
# If the cached clone points at a different repo (e.g. PUBLISH_REPO changed),
# nuke it so we re-clone the right one.
case "$CACHED_REMOTE" in
    *"$EXPECTED_REMOTE"*) ;;
    *) rm -rf "$WORKDIR" ;;
esac
if [ ! -d "$WORKDIR/.git" ]; then
    gh repo clone "$GH_USER/$REPO_NAME" "$WORKDIR"
fi
(
    cd "$WORKDIR"
    git pull --rebase 2>/dev/null || true
)

# 5. Copy build artifacts into the per-game subfolder (html → index.html).
GAME_DIR="$WORKDIR/$GAME"
rm -rf "$GAME_DIR"
mkdir -p "$GAME_DIR"
cp "$BUILD_DIR/${SOURCE_DIR}.html" "$GAME_DIR/index.html"
cp "$BUILD_DIR/${SOURCE_DIR}.js"   "$GAME_DIR/"
cp "$BUILD_DIR/${SOURCE_DIR}.wasm" "$GAME_DIR/"
cp "$BUILD_DIR/${SOURCE_DIR}.data" "$GAME_DIR/"

# Cache-busting. The assets keep the same filenames on every publish, so a
# browser that already cached the old .js/.wasm/.data keeps running the old
# build until a hard refresh. Stamp a unique version onto every asset URL so
# each publish forces fresh files. The .js include gets a ?v=VERSION query,
# and a locateFile hook adds the same query to the .wasm/.data the loader
# fetches.
VERSION=$(date +%s)
perl -0pi -e "s{${SOURCE_DIR}\.js}{${SOURCE_DIR}.js?v=$VERSION}g" \
    "$GAME_DIR/index.html"
perl -0pi -e "s{<script src=${SOURCE_DIR}\.js}{<script>Module.locateFile=function(path,prefix){return prefix+path+'?v=$VERSION'};</script><script src=${SOURCE_DIR}.js}" \
    "$GAME_DIR/index.html"

# 6. Commit and push to `main` (the branch Pages serves from).
(
    cd "$WORKDIR"
    # Make sure the local branch is named `main`. On a freshly cloned empty
    # repo there's no branch yet, and on machines whose git defaults to
    # `master` the first commit would land on the wrong branch.
    git checkout -B main >/dev/null 2>&1
    git add -A
    if git diff --cached --quiet; then
        echo "No changes to publish for $GAME (already up to date)."
    else
        git -c user.email=publish@local -c user.name=publish \
            commit -m "publish $GAME"
        git push -u origin main
    fi
)

# 7. Turn on Pages (first publish only). Subsequent runs hit a 409 and
#    we ignore it.
gh api -X POST "/repos/$GH_USER/$REPO_NAME/pages" \
    -F source[branch]=main -F source[path]=/ >/dev/null 2>&1 || true

URL="https://$GH_USER.github.io/$REPO_NAME/$GAME/"
echo ""
echo "------------------------------------------------"
echo "Published: $URL"
echo "(GitHub Pages takes ~30 seconds to build the first time.)"
echo "Share that URL with your friend; you both open it and play."
