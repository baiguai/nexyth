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
    "src/layout/node.hpp"
    "src/layout/pane.hpp"
    "src/layout/workspace.hpp"
    "src/terminal/buffer.hpp"
    "src/terminal/pty.hpp"
    "src/terminal/vte.hpp"
    "src/ui/renderer.hpp"
)
