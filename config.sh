#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="nexyth"

SOURCES=(
    "src/main.cpp"
    "src/utils.cpp"
    "src/config/config.cpp"
    "src/config/keys.cpp"
    "src/core/action.cpp"
    "src/core/app.cpp"
    "src/layout/layout_store.cpp"
    "src/layout/node.cpp"
    "src/layout/pane.cpp"
    "src/layout/workspace.cpp"
    "src/terminal/buffer.cpp"
    "src/terminal/pty.cpp"
    "src/terminal/vte.cpp"
    "src/ui/renderer.cpp"
)

LIBS=(
    "ftxui::screen"
    "ftxui::dom"
    "ftxui::component"
)

HEADERS=(
    "src/utils.hpp"
    "src/config/config.hpp"
    "src/config/keys.hpp"
    "src/core/action.hpp"
    "src/core/app.hpp"
    "src/core/mode.hpp"
    "src/layout/layout.hpp"
    "src/layout/layout_store.hpp"
    "src/layout/node.hpp"
    "src/layout/pane.hpp"
    "src/layout/workspace.hpp"
    "src/terminal/buffer.hpp"
    "src/terminal/pty.hpp"
    "src/terminal/vte.hpp"
    "src/ui/renderer.hpp"
)

clean_stale_cmake_cache() {
    local build_dir="$1"
    [ -d "$build_dir" ] || return 0
    local source_dir cache cache_dir recorded_dir
    source_dir=$(pwd -P)
    while IFS= read -r -d '' cache; do
        cache_dir=$(cd "$(dirname "$cache")" && pwd -P)
        recorded_dir=$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "$cache" | head -1)
        if [ "$recorded_dir" != "$cache_dir" ]; then
            echo "Removing stale CMake cache in $cache_dir"
            rm -rf "$cache" "$cache_dir/CMakeFiles"
            continue
        fi
        if [ "$cache_dir" = "$(cd "$build_dir" && pwd -P)" ] &&
           ! grep -Fq "CMAKE_HOME_DIRECTORY:INTERNAL=$source_dir" "$cache"; then
            echo "Removing stale CMake cache in $cache_dir"
            rm -rf "$cache" "$cache_dir/CMakeFiles"
        fi
    done < <(find "$build_dir" -name CMakeCache.txt -print0 2>/dev/null)
}
