#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="nexyth"

SOURCES=(
    "src/main.cpp"
    "src/utils.cpp"
)

LIBS=(
    "ftxui::screen"
    "ftxui::dom"
    "ftxui::component"
)

HEADERS=(
    "src/main.hpp"
    "src/utils.hpp"
)
