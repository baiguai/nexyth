#!/bin/bash
#
# app setup - checks prerequisites and prepares the FTXUI dependency.
#
# Safe to run at any time; it does NOT assume a blank/fresh project and never
# creates scaffolding. Optional flag:
#   ./setup.sh --prefetch   download FTXUI now so the first ./build.sh works
#                           even without network access.
#   ./setup.sh --check      only verify prerequisites, never install anything.
#
# FTXUI requirements covered here:
#   - cmake 3.16+            (FetchContent needs it; build.sh uses cmake+make)
#   - a C++20 compiler        (g++ or clang++)
#   - git                     (FetchContent clones FTXUI from GitHub; the
#                              PATCH_COMMAND also runs 'git apply')
#   - make                    (build.sh compiles with 'make')
#   - X11 dev files + python3 venv for docsgen
#   - network to github.com   (to download FTXUI on first build)
#   - mingw-w64               (optional, only for ./build-windows.sh)

set -e

PREFETCH=0
INSTALL=1
for argument in "$@"; do
    case "$argument" in
        --prefetch|-p) PREFETCH=1 ;;
        --check|-c) INSTALL=0 ;;
        *) echo "usage: ./setup.sh [--prefetch] [--check]"; exit 2 ;;
    esac
done

echo "=== app setup ==="

for script in ./*.sh ./docsgen/*.sh; do
    [ -f "$script" ] || continue
    [ -x "$script" ] && continue
    if [ "$INSTALL" -eq 0 ]; then
        echo "ERROR: $script is not executable; run: chmod +x $script"
        exit 1
    fi
    chmod +x "$script"
    echo "[OK] made $script executable"
done

detect_package_manager() {
    if command -v apt-get >/dev/null 2>&1; then
        echo apt
    elif command -v dnf >/dev/null 2>&1; then
        echo dnf
    elif command -v yum >/dev/null 2>&1; then
        echo yum
    elif command -v pacman >/dev/null 2>&1; then
        echo pacman
    else
        echo unknown
    fi
}

PACKAGE_MANAGER=$(detect_package_manager)
case "$PACKAGE_MANAGER" in
    apt)
        REQUIRED_PACKAGES=(
            build-essential cmake git pkg-config libstdc++6
            libx11-dev libxext-dev libxfixes-dev libxi-dev
            python3 python3-venv python3-pip
        )
        ;;
    dnf|yum)
        REQUIRED_PACKAGES=(
            gcc-c++ make cmake git pkgconf-pkg-config libstdc++
            libX11-devel libXext-devel libXfixes-devel libXi-devel
            python3 python3-pip
        )
        ;;
    pacman)
        REQUIRED_PACKAGES=(
            base-devel cmake git pkgconf gcc-libs
            libx11 libxext libxfixes libxi
            python python-pip
        )
        ;;
    *)
        echo "ERROR: unsupported package manager; install cmake, a C++20 compiler,"
        echo "git, make, X11 development headers, python3 and python3-venv manually."
        exit 1
        ;;
esac

package_installed() {
    case "$PACKAGE_MANAGER" in
        apt)
            dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "ok installed"
            ;;
        dnf|yum)
            rpm -q "$1" >/dev/null 2>&1
            ;;
        pacman)
            pacman -Q "$1" >/dev/null 2>&1
            ;;
    esac
}

missing_packages=()
for package in "${REQUIRED_PACKAGES[@]}"; do
    if ! package_installed "$package"; then
        missing_packages+=("$package")
    fi
done

if [ ${#missing_packages[@]} -gt 0 ]; then
    if [ "$INSTALL" -eq 0 ]; then
        echo "ERROR: missing packages: ${missing_packages[*]}"
        exit 1
    fi

    echo "Installing missing packages: ${missing_packages[*]}"
    if [ "$(id -u)" -eq 0 ]; then
        ROOT_COMMAND=()
    elif command -v sudo >/dev/null 2>&1; then
        ROOT_COMMAND=(sudo)
    else
        echo "ERROR: root privileges are required to install packages (sudo not found)."
        exit 1
    fi

    case "$PACKAGE_MANAGER" in
        apt)
            "${ROOT_COMMAND[@]}" apt-get update
            if [ ${#ROOT_COMMAND[@]} -eq 0 ]; then
                DEBIAN_FRONTEND=noninteractive apt-get install -y "${missing_packages[@]}"
            else
                "${ROOT_COMMAND[@]}" env DEBIAN_FRONTEND=noninteractive \
                    apt-get install -y "${missing_packages[@]}"
            fi
            ;;
        dnf|yum)
            "${ROOT_COMMAND[@]}" "$PACKAGE_MANAGER" install -y "${missing_packages[@]}"
            ;;
        pacman)
            "${ROOT_COMMAND[@]}" pacman -S --noconfirm --needed "${missing_packages[@]}"
            ;;
    esac
else
    echo "[OK] required system packages are installed"
fi

command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake is required (3.16+)."; exit 1; }
CMAKE_VER=$(cmake --version | head -1 | grep -oP '\d+\.\d+')
if [ "$(printf '%s\n3.16\n' "$CMAKE_VER" | sort -V | head -1)" != "3.16" ]; then
    echo "ERROR: cmake 3.16+ required (found $CMAKE_VER)"
    exit 1
fi
echo "[OK] cmake $CMAKE_VER"

if command -v g++ >/dev/null 2>&1; then
    CXX=g++
    echo "[OK] g++ $(g++ -dumpfullversion -dumpversion 2>/dev/null || g++ -dumpversion)"
elif command -v clang++ >/dev/null 2>&1; then
    CXX=clang++
    echo "[OK] clang++ ($(clang++ --version | head -1))"
else
    echo "ERROR: no C++ compiler found (g++ or clang++)"
    exit 1
fi
CXX_PROBE=$(mktemp --suffix=.cpp)
trap 'rm -f "$CXX_PROBE" "$CXX_PROBE.out"' EXIT
cat > "$CXX_PROBE" <<'EOF'
#include <iostream>
#include <string>
int main() { std::string value = "nexyth"; std::cout << value; }
EOF
if ! "$CXX" -std=c++20 "$CXX_PROBE" -o "$CXX_PROBE.out" >/dev/null 2>&1; then
    echo "ERROR: $CXX does not support the required C++20 features"
    exit 1
fi
case "$PACKAGE_MANAGER" in
    apt) RUNTIME_PACKAGE=libstdc++6 ;;
    dnf|yum) RUNTIME_PACKAGE=libstdc++ ;;
    pacman) RUNTIME_PACKAGE=gcc-libs ;;
esac

runtime_error() {
    echo "ERROR: the C++ runtime found by the loader is older than $CXX requires."
    echo "  Compiler: $CXX ($($CXX --version | head -1))"
    echo "  LD_LIBRARY_PATH: ${LD_LIBRARY_PATH:-<empty>}"
    echo "  Upgrade $RUNTIME_PACKAGE to match this compiler (or remove an old"
    echo "  libstdc++ from LD_LIBRARY_PATH), then rerun setup.sh."
    exit 1
}

if ! "$CXX_PROBE.out" >/dev/null 2>&1; then
    if [ "$INSTALL" -eq 0 ]; then
        runtime_error
    fi
    if [ "$(id -u)" -eq 0 ]; then
        ROOT_COMMAND=()
    elif command -v sudo >/dev/null 2>&1; then
        ROOT_COMMAND=(sudo)
    else
        runtime_error
    fi
    echo "Refreshing $RUNTIME_PACKAGE..."
    case "$PACKAGE_MANAGER" in
        apt)
            if [ ${#ROOT_COMMAND[@]} -eq 0 ]; then
                DEBIAN_FRONTEND=noninteractive apt-get install -y "$RUNTIME_PACKAGE" >/dev/null
            else
                "${ROOT_COMMAND[@]}" env DEBIAN_FRONTEND=noninteractive \
                    apt-get install -y "$RUNTIME_PACKAGE" >/dev/null
            fi
            ;;
        dnf|yum)
            "${ROOT_COMMAND[@]}" "$PACKAGE_MANAGER" install -y "$RUNTIME_PACKAGE" >/dev/null
            ;;
        pacman)
            "${ROOT_COMMAND[@]}" pacman -S --noconfirm --needed "$RUNTIME_PACKAGE" >/dev/null
            ;;
    esac
    if ! "$CXX_PROBE.out" >/dev/null 2>&1; then
        runtime_error
    fi
    echo "[OK] C++ runtime refreshed"
fi
echo "[OK] C++20 compiler and runtime"

command -v git >/dev/null 2>&1 || { echo "ERROR: git is required (FTXUI is fetched from GitHub and patched)."; exit 1; }
echo "[OK] git $(git --version | cut -d' ' -f3)"

command -v make >/dev/null 2>&1 || { echo "ERROR: make is required (build.sh runs 'make')."; exit 1; }
echo "[OK] make $(make --version | head -1 | sed 's/^GNU Make //')"

VENV_PROBE=$(mktemp -d)
if ! python3 -m venv "$VENV_PROBE" >/dev/null 2>&1; then
    rm -rf "$VENV_PROBE"
    echo "ERROR: python3 with venv support is required (build.sh runs docsgen)."
    exit 1
fi
rm -rf "$VENV_PROBE"
echo "[OK] python3 with venv"

# --- FTXUI dependency ------------------------------------------------------
FTXUI_SRC="build/_deps/ftxui-src"

# A checkout counts as present only if it is actually populated (a stale empty
# directory must not look like a cached dependency).
ftxui_present() {
    [ -f "$FTXUI_SRC/CMakeLists.txt" ] || [ -d "$FTXUI_SRC/src" ]
}

# Report the checked-out version, but only from the checkout's own git repo -
# `git -C` would otherwise walk up into the project's repo if .git is missing.
ftxui_version() {
    if [ -e "$FTXUI_SRC/.git" ] || [ -f "$FTXUI_SRC/.git" ]; then
        git -C "$FTXUI_SRC" describe --tags --always 2>/dev/null || echo "cached"
    else
        echo "cached"
    fi
}

if ftxui_present; then
    echo "[OK] FTXUI $(ftxui_version) (cached in $FTXUI_SRC)"
elif [ "$PREFETCH" -eq 1 ]; then
    echo "Prefetching FTXUI dependency (fetch only, no compile)..."
    # Regenerate CMakeLists.txt from the template, exactly like build.sh does
    # (keep this block in sync with build.sh / build-windows.sh).
    source ./config.sh
    cp CMakeLists.txt.in CMakeLists.txt
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
    clean_stale_cmake_cache build
    # Clear any stale/empty checkout so FetchContent's download step can clone
    # fresh (git clone refuses an existing non-empty directory).
    rm -rf "$FTXUI_SRC"
    cmake -S . -B build
    if ftxui_present; then
        echo "[OK] FTXUI $(ftxui_version) fetched into $FTXUI_SRC"
    else
        echo "WARNING: FTXUI fetch did not produce $FTXUI_SRC - check network and rerun."
    fi
else
    echo "FTXUI not fetched yet - it will be downloaded on first ./build.sh (needs network)."
    echo "  To fetch it now:  ./setup.sh --prefetch"
fi

# --- Windows cross-build (optional) ----------------------------------------
if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    echo "[OK] MinGW-w64 cross-compiler (Windows builds supported)"
elif [ "$INSTALL" -eq 1 ] && [ "$PACKAGE_MANAGER" = "apt" ]; then
    echo "MinGW-w64 not found - Windows builds (./build-windows.sh) are skipped."
    echo "  Install it with:  sudo apt install -y mingw-w64"
else
    echo "MinGW-w64 not found - Windows builds (./build-windows.sh) are skipped."
fi

echo ""
echo "=== Setup complete ==="
echo "Run ./build.sh to build."
