#include "vte.hpp"

#include <algorithm>
#include <cctype>

#include "../utils.hpp"
#include "buffer.hpp"

namespace nexyth {
namespace {

int utf8Length(unsigned char first) {
  if (first < 0x80) return 1;
  if ((first & 0xE0) == 0xC0) return 2;
  if ((first & 0xF0) == 0xE0) return 3;
  if ((first & 0xF8) == 0xF0) return 4;
  return 1;
}

std::vector<int> parseParameters(const std::string& text) {
  std::vector<int> values;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = text.find(';', start);
    const std::string part =
        text.substr(start, end == std::string::npos ? std::string::npos
                                                    : end - start);
    int value = 0;
    if (!part.empty()) {
      try {
        value = std::stoi(part);
      } catch (...) {
        value = 0;
      }
    }
    values.push_back(value);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return values;
}

int parameter(const std::vector<int>& values, std::size_t index, int fallback) {
  return index < values.size() ? values[index] : fallback;
}

}  // namespace

Vte::Vte(Buffer& buffer) : buffer_(buffer) {}

void Vte::feed(std::string_view data) {
  for (const char raw : data) {
    const auto byte = static_cast<unsigned char>(raw);
    switch (state_) {
      case State::Ground:
        handleGround(byte);
        break;
      case State::Escape:
        if (byte == '[') {
          parameters_.clear();
          state_ = State::Csi;
        } else if (byte == ']') {
          osc_.clear();
          state_ = State::Osc;
        } else if (byte == '_' || byte == 'P' || byte == '^' || byte == 'X') {
          state_ = State::EscapeString;
        } else if (byte == '(' || byte == ')' || byte == '#' || byte == '*') {
          state_ = State::EscapeIntermediate;
        } else if (byte == '7') {
          buffer_.saveCursor();
          state_ = State::Ground;
        } else if (byte == '8') {
          buffer_.restoreCursor();
          state_ = State::Ground;
        } else if (byte == 'D') {
          buffer_.lineFeed();
          state_ = State::Ground;
        } else if (byte == 'M') {
          buffer_.moveNavigation(0, -1);
          state_ = State::Ground;
        } else if (byte == 'c') {
          buffer_.reset();
          state_ = State::Ground;
        } else if (byte == '\x1B') {
          state_ = State::Escape;
        } else {
          state_ = State::Ground;
        }
        break;
      case State::EscapeIntermediate:
        state_ = State::Ground;
        break;
      case State::Csi:
        if (byte >= 0x20 && byte <= 0x3F) {
          parameters_ += static_cast<char>(byte);
        } else if (byte >= 0x40 && byte <= 0x7E) {
          handleCsi(byte);
          state_ = State::Ground;
        } else {
          state_ = State::Ground;
        }
        break;
      case State::Osc:
        if (byte == '\a') {
          state_ = State::Ground;
        } else if (byte == '\x1B') {
          state_ = State::OscEscape;
        } else if (osc_.size() < 4096) {
          osc_ += static_cast<char>(byte);
        }
        break;
      case State::OscEscape:
        if (byte == '\\') {
          state_ = State::Ground;
        } else {
          osc_ += static_cast<char>(byte);
          if (osc_.size() > 4096) osc_.clear();
          state_ = State::Osc;
        }
        break;
      case State::EscapeString:
        if (byte == '\x1B') state_ = State::EscapeStringEscape;
        break;
      case State::EscapeStringEscape:
        state_ = byte == '\\' ? State::Ground : State::EscapeString;
        break;
    }
  }
}

void Vte::handleGround(unsigned char byte) {
  const auto flushPending = [this] {
    if (!utf8_pending_.empty()) {
      buffer_.putText(utf8_pending_);
      utf8_pending_.clear();
    }
    utf8_expected_ = 1;
  };

  if (byte == '\x1B') {
    flushPending();
    state_ = State::Escape;
    return;
  }
  if (byte < 0x20 || byte == 0x7F) {
    flushPending();
    switch (byte) {
      case '\a': break;
      case '\b': buffer_.backspace(); break;
      case '\t': buffer_.tab(); break;
      case '\n':
      case '\v':
      case '\f': buffer_.lineFeed(); break;
      case '\r': buffer_.carriageReturn(); break;
      default: break;
    }
    return;
  }
  if (utf8_pending_.empty()) utf8_expected_ = utf8Length(byte);
  utf8_pending_ += static_cast<char>(byte);
  if (utf8_pending_.size() < static_cast<std::size_t>(utf8_expected_)) return;
  std::size_t position = 0;
  std::uint32_t codepoint = 0;
  const bool valid = utf8Decode(utf8_pending_, position, codepoint);
  if (!valid || position != utf8_pending_.size()) {
    buffer_.putText(utf8_pending_);
  } else {
    buffer_.putGrapheme(utf8_pending_);
  }
  utf8_pending_.clear();
  utf8_expected_ = 1;
}

void Vte::resetStyle() { buffer_.current_style_ = Style{}; }

void Vte::handleSgr(const std::vector<int>& values) {
  if (values.empty()) {
    resetStyle();
    return;
  }
  Style& style = buffer_.current_style_;
  for (std::size_t i = 0; i < values.size(); ++i) {
    const int value = values[i];
    if (value == 0) {
      style = Style{};
    } else if (value == 1) {
      style.bold = true;
    } else if (value == 2) {
      style.dim = true;
    } else if (value == 3) {
      style.italic = true;
    } else if (value == 4) {
      style.underlined = true;
    } else if (value == 7) {
      style.reverse = true;
    } else if (value == 9) {
      style.strikethrough = true;
    } else if (value == 21 || value == 22) {
      style.bold = false;
      style.dim = false;
    } else if (value == 23) {
      style.italic = false;
    } else if (value == 24) {
      style.underlined = false;
    } else if (value == 27) {
      style.reverse = false;
    } else if (value == 29) {
      style.strikethrough = false;
    } else if (value == 39) {
      style.foreground = TermColor{};
    } else if (value == 49) {
      style.background = TermColor{};
    } else if (value >= 30 && value <= 37) {
      style.foreground.set = true;
      style.foreground.rgb = false;
      style.foreground.index = static_cast<int>(value - 30);
    } else if (value >= 90 && value <= 97) {
      style.foreground.set = true;
      style.foreground.rgb = false;
      style.foreground.index = value - 90 + 8;
    } else if (value >= 40 && value <= 47) {
      style.background.set = true;
      style.background.rgb = false;
      style.background.index = value - 40;
    } else if (value >= 100 && value <= 107) {
      style.background.set = true;
      style.background.rgb = false;
      style.background.index = value - 100 + 8;
    } else if (value == 38 || value == 48) {
      TermColor color;
      color.set = true;
      const int mode = parameter(values, i + 1, -1);
      if (mode == 5) {
        color.rgb = false;
        color.index = parameter(values, i + 2, 0);
        i += 2;
      } else if (mode == 2) {
        color.rgb = true;
        color.r = parameter(values, i + 2, 0);
        color.g = parameter(values, i + 3, 0);
        color.b = parameter(values, i + 4, 0);
        i += 4;
      }
      if (value == 38) {
        style.foreground = color;
      } else {
        style.background = color;
      }
    }
  }
}

void Vte::moveCursor(int dx, int dy) {
  buffer_.wrap_pending_ = false;
  buffer_.cursor_x_ = std::clamp(buffer_.cursor_x_ + dx, 0, buffer_.columns() - 1);
  const int top = buffer_.visibleStart();
  buffer_.cursor_y_ = std::clamp(buffer_.cursor_y_ + dy, top, top + buffer_.rows() - 1);
  buffer_.ensureLine(buffer_.cursor_y_);
}

void Vte::setCursor(int x, int y) {
  buffer_.wrap_pending_ = false;
  buffer_.cursor_x_ = std::clamp(x, 0, buffer_.columns() - 1);
  const int top = buffer_.visibleStart();
  buffer_.cursor_y_ = std::clamp(top + std::max(0, y), top, top + buffer_.rows() - 1);
  buffer_.ensureLine(buffer_.cursor_y_);
}

void Vte::handleCsi(unsigned char final) {
  const bool privateMode = !parameters_.empty() &&
                           (parameters_[0] == '?' || parameters_[0] == '>' ||
                            parameters_[0] == '<' || parameters_[0] == '=');
  if (privateMode) {
    if (parameters_[0] == '?' && (final == 'h' || final == 'l')) {
      const auto modes = parseParameters(parameters_.substr(1));
      const bool enable = final == 'h';
      for (int mode : modes) {
        if (mode == 47 || mode == 1047 || mode == 1049) {
          if (enable) {
            buffer_.enterAlternateScreen();
          } else {
            buffer_.leaveAlternateScreen();
          }
        }
      }
    }
    return;
  }
  const std::vector<int> values = parseParameters(parameters_);
  const int first = parameter(values, 0, 0);
  const int second = parameter(values, 1, 0);
  switch (final) {
    case 'm': handleSgr(values); break;
    case 'A': moveCursor(0, -std::max(1, first)); break;
    case 'B': moveCursor(0, std::max(1, first)); break;
    case 'C': moveCursor(std::max(1, first), 0); break;
    case 'D': moveCursor(-std::max(1, first), 0); break;
    case 'E': moveCursor(0, std::max(1, first)); buffer_.carriageReturn(); break;
    case 'F': moveCursor(0, -std::max(1, first)); buffer_.carriageReturn(); break;
    case 'G':
    case '`': setCursor(std::max(0, first - 1), buffer_.cursor_y_); break;
    case 'd': setCursor(buffer_.cursor_x_, std::max(0, first - 1)); break;
    case 'H':
    case 'f': setCursor(std::max(0, second - 1), std::max(0, first - 1)); break;
    case 'J': buffer_.eraseDisplay(first); break;
    case 'K': buffer_.eraseLine(first); break;
    case '@': buffer_.insertBlanks(std::max(1, first)); break;
    case 'P': buffer_.deleteCharacters(std::max(1, first)); break;
    case 'X': buffer_.eraseCharacters(std::max(1, first)); break;
    case 's': buffer_.saveCursor(); break;
    case 'u': buffer_.restoreCursor(); break;
    default: break;
  }
}

}  // namespace nexyth
