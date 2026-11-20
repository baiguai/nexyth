#pragma once

#include <string>

namespace nexyth {

enum class Mode { Normal, Insert, Visual };

inline const char* modeName(Mode mode) {
  switch (mode) {
    case Mode::Normal: return "PLEX";
    case Mode::Insert: return "NORMAL";
    case Mode::Visual: return "VISUAL";
  }
  return "PLEX";
}

inline bool parseMode(const std::string& text, Mode& mode) {
  const std::string value = text == "insert" ? "normal" : text;
  if (value == "plex" || value == "normal-mode") {
    mode = Mode::Normal;
  } else if (value == "normal" || value == "insert") {
    mode = Mode::Insert;
  } else if (value == "visual") {
    mode = Mode::Visual;
  } else {
    return false;
  }
  return true;
}

}  // namespace nexyth
