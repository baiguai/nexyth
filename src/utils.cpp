#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cwchar>
#include <limits.h>
#include <unistd.h>
#endif

namespace nexyth {
namespace {

bool isSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

#ifdef _WIN32
struct CodepointRange {
  std::uint32_t first;
  std::uint32_t last;
};

bool inRanges(std::uint32_t codepoint, const CodepointRange* ranges,
              std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    if (codepoint < ranges[i].first) return false;
    if (codepoint <= ranges[i].last) return true;
  }
  return false;
}

int wideCharWidth(std::uint32_t codepoint) {
  if (codepoint == 0) return 0;
  if (codepoint < 0x20 || (codepoint >= 0x7F && codepoint < 0xA0)) return -1;
  if (codepoint == 0x200B || codepoint == 0x200C || codepoint == 0x200D ||
      codepoint == 0xFEFF) {
    return 0;
  }

  const wchar_t character = static_cast<wchar_t>(codepoint);
  WORD type = 0;
  if (GetStringTypeExW(LOCALE_USER_DEFAULT, CT_CTYPE3, &character, 1, &type) !=
      0) {
    if ((type & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK)) != 0) return 0;
  }

  static constexpr CodepointRange kWideRanges[] = {
      {0x1100, 0x115F}, {0x2329, 0x232A}, {0x2E80, 0x303E},
      {0x3040, 0xA4CF}, {0xAC00, 0xD7A3}, {0xF900, 0xFAFF},
      {0xFE10, 0xFE19}, {0xFE30, 0xFE6F}, {0xFF00, 0xFF60},
      {0xFFE0, 0xFFE6}, {0x1F300, 0x1FAFF}, {0x20000, 0x3FFFD}};
  return inRanges(codepoint, kWideRanges,
                  sizeof(kWideRanges) / sizeof(kWideRanges[0]))
             ? 2
             : 1;
}
#endif

}  // namespace

std::string trim(std::string_view value) {
  std::size_t first = 0;
  while (first < value.size() && isSpace(value[first])) ++first;
  std::size_t last = value.size();
  while (last > first && isSpace(value[last - 1])) --last;
  return std::string(value.substr(first, last - first));
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::vector<std::string> tokenizeConfigLine(std::string_view line) {
  std::vector<std::string> result;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && isSpace(line[i])) ++i;
    if (i >= line.size()) break;
    if (line[i] == ';') {
      result.emplace_back(";");
      ++i;
      continue;
    }
    std::string token;
    char quote = 0;
    while (i < line.size()) {
      const char c = line[i];
      if (quote != 0) {
        if (c == '\\' && i + 1 < line.size()) {
          const char next = line[i + 1];
          if (next == '\\' || next == '"' || next == '\'') {
            token += next;
            i += 2;
            continue;
          }
        }
        if (c == quote) {
          quote = 0;
          ++i;
          continue;
        }
        token += c;
        ++i;
        continue;
      }
      if (c == '"' || c == '\'') {
        quote = c;
        ++i;
        continue;
      }
      if (isSpace(c) || c == ';') break;
      if (c == '\\' && i + 1 < line.size()) {
        const char next = line[i + 1];
        if (next == '\\' || next == '"' || next == '\'') {
          token += next;
          i += 2;
          continue;
        }
      }
      token += c;
      ++i;
    }
    result.push_back(token);
  }
  return result;
}

std::string joinTokens(const std::vector<std::string>& tokens,
                       const std::string& separator) {
  std::string result;
  for (const auto& token : tokens) {
    if (!result.empty()) result += separator;
    result += token;
  }
  return result;
}

std::string base64Encode(std::string_view data) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  for (std::size_t i = 0; i < data.size(); i += 3) {
    const std::uint32_t chunk =
        (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
        ((i + 1 < data.size())
             ? (static_cast<std::uint32_t>(
                    static_cast<unsigned char>(data[i + 1])) << 8)
             : 0) |
        ((i + 2 < data.size())
             ? static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 2]))
             : 0);
    result += kAlphabet[(chunk >> 18) & 0x3F];
    result += kAlphabet[(chunk >> 12) & 0x3F];
    result += i + 1 < data.size() ? kAlphabet[(chunk >> 6) & 0x3F] : '=';
    result += i + 2 < data.size() ? kAlphabet[chunk & 0x3F] : '=';
  }
  return result;
}

bool utf8Decode(std::string_view data, std::size_t& position,
                std::uint32_t& codepoint) {
  if (position >= data.size()) return false;
  const auto first = static_cast<unsigned char>(data[position]);
  std::size_t length = 0;
  std::uint32_t value = 0;
  if (first < 0x80) {
    codepoint = first;
    ++position;
    return true;
  } else if ((first & 0xE0) == 0xC0) {
    length = 2;
    value = first & 0x1F;
  } else if ((first & 0xF0) == 0xE0) {
    length = 3;
    value = first & 0x0F;
  } else if ((first & 0xF8) == 0xF0) {
    length = 4;
    value = first & 0x07;
  } else {
    codepoint = first;
    ++position;
    return false;
  }
  if (position + length > data.size()) {
    codepoint = first;
    ++position;
    return false;
  }
  for (std::size_t i = 1; i < length; ++i) {
    const auto next = static_cast<unsigned char>(data[position + i]);
    if ((next & 0xC0) != 0x80) {
      codepoint = first;
      position += i;
      return false;
    }
    value = (value << 6) | (next & 0x3F);
  }
  position += length;
  codepoint = value;
  return true;
}

int codepointWidth(std::uint32_t codepoint) {
  if (codepoint == 0) return 0;
#ifdef _WIN32
  const int width = wideCharWidth(codepoint);
#else
  const int width = wcwidth(static_cast<wchar_t>(codepoint));
#endif
  return width < 0 ? 1 : width;
}

std::string executableDir() {
#ifdef _WIN32
  std::vector<wchar_t> buffer(4096);
  const DWORD length = GetModuleFileNameW(
      nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length > 0 && length < buffer.size()) {
    return std::filesystem::path(buffer.data()).parent_path().string();
  }
#else
  char buffer[PATH_MAX] = {0};
  const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (length > 0) {
    buffer[length] = '\0';
    return std::filesystem::path(buffer).parent_path().string();
  }
#endif
  return std::filesystem::current_path().string();
}

std::string expandHome(std::string path) {
  if (path == "~" || (path.size() > 2 && path.substr(0, 2) == "~/")) {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (home != nullptr) {
      return std::string(home) + (path.size() > 1 ? path.substr(1) : "");
    }
  }
  return path;
}

}  // namespace nexyth
