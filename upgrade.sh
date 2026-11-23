#!/bin/bash
#
# upgrade.sh - bump the FTXUI dependency to a newer release.
#
#   ./upgrade.sh              upgrade to the latest FTXUI release on GitHub
#   ./upgrade.sh <tag>        pin a specific tag, e.g. ./upgrade.sh v7.0.3
#
# What it does:
#   - reads the current pin from CMakeLists.txt.in (the template build.sh uses)
#   - updates it to the chosen/latest tag
#   - drops the legacy CAN/SUB PATCH_COMMAND when upgrading to FTXUI >= 6,
#     where that fix is upstream
#   - clears the cached FetchContent checkouts so the new tag is actually used
#   - runs a configure pass to fetch the new tag; on failure it rolls the
#     template back to the previous pin
#
# FTXUI upgrades can break the app compile if its API changed - configure
# passing here only means the fetch succeeded; then run ./build.sh.

set -euo pipefail

REPO="https://github.com/ArthurSonzogni/FTXUI.git"
TEMPLATE="CMakeLists.txt.in"

command -v git >/dev/null 2>&1 || { echo "ERROR: git is required."; exit 1; }
[ -f "$TEMPLATE" ] || { echo "ERROR: $TEMPLATE not found. Run from the repo root."; exit 1; }

version_tuple() {
    echo "${1#[vV]}" | tr '.' ' '
}

version_ge() {
    local -a a b
    read -ra a <<< "$(version_tuple "$1")"
    read -ra b <<< "$(version_tuple "$2")"
    local i av bv
    for i in 0 1 2; do
        av=${a[$i]:-0}
        bv=${b[$i]:-0}
        if (( av > bv )); then return 0; fi
        if (( av < bv )); then return 1; fi
    done
    return 0
}

clear_cache() {
    for dep in build/_deps/ftxui-src build/_deps/ftxui-build build/_deps/ftxui-subbuild \
               build-windows/_deps/ftxui-src build-windows/_deps/ftxui-build \
               build-windows/_deps/ftxui-subbuild; do
        if [ -e "$dep" ]; then
            rm -rf "$dep"
            echo "Removed cached: $dep"
        fi
    done
}

CURRENT=$(grep -oP '^  GIT_TAG\s+\K\S+' "$TEMPLATE")
echo "FTXUI currently pinned at: $CURRENT"

if [ $# -ge 1 ]; then
    TARGET="$1"
    echo "Using requested tag: $TARGET"
    git ls-remote --tags --refs "$REPO" | grep -q "refs/tags/$TARGET\$" \
        || { echo "ERROR: tag '$TARGET' not found in $REPO."; exit 1; }
else
    echo "Querying latest FTXUI release from GitHub..."
    VER=$(git ls-remote --tags --refs "$REPO" \
        | grep -oP 'refs/tags/\K[vV][0-9]+(\.[0-9]+)+$' \
        | sort -V | tail -1)
    [ -n "$VER" ] || { echo "ERROR: could not determine the latest FTXUI tag (network?)."; exit 1; }
    TARGET="$VER"
fi

if [ "$TARGET" = "$CURRENT" ]; then
    echo "FTXUI is already at $TARGET - nothing to do."
    exit 0
fi

echo ""
echo "Upgrading FTXUI: $CURRENT -> $TARGET"

BACKUP=$(mktemp)
cp "$TEMPLATE" "$BACKUP"
trap 'rm -f "$BACKUP"' EXIT

sed -i "s|^  GIT_TAG .*|  GIT_TAG $TARGET|" "$TEMPLATE"

if version_ge "$TARGET" 6.0.0 && grep -q 'ftxui-c0-delivery.patch' "$TEMPLATE"; then
    sed -i '/ftxui-c0-delivery\.patch/d' "$TEMPLATE"
    echo "Removed legacy CAN/SUB patch (fixed upstream in FTXUI 6+)."
fi

clear_cache

echo ""
echo "Verifying FTXUI $TARGET fetches and applies..."
source ./config.sh
cp "$TEMPLATE" CMakeLists.txt
sed -i "s/<<TARGET_NAME>>/$APP_NAME/g" CMakeLists.txt
SOURCES_TMP=$(mktemp)
for s in "${SOURCES[@]}"; do
    echo "    $s" >> "$SOURCES_TMP"
done
sed -i "/^<<SOURCES>>$/{
    r $SOURCES_TMP
    d
}" CMakeLists.txt
rm -f "$SOURCES_TMP"
for lib in "${LIBS[@]}"; do
    if [[ "$lib" == *::* ]]; then
        echo "target_link_libraries($APP_NAME PRIVATE $lib)" >> CMakeLists.txt
    else
        echo "target_link_libraries($APP_NAME PRIVATE \${CMAKE_SOURCE_DIR}/$lib)" >> CMakeLists.txt
    fi
done

mkdir -p build
if ! cmake -S . -B build; then
    echo ""
    echo "ERROR: configure failed for FTXUI $TARGET."
    echo "  Rolling back the pin to $CURRENT..."
    cp "$BACKUP" "$TEMPLATE"
    clear_cache
    echo "Rolled back. Nothing was changed."
    exit 1
fi

echo ""
echo "Done: FTXUI pinned to $TARGET ($TEMPLATE)."
echo "Run ./build.sh to rebuild with the new FTXUI."
