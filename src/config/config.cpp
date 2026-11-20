#include "config.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "../utils.hpp"
#include "keys.hpp"

namespace nexyth {
namespace {

Mode modeFromToken(const std::string& token) {
  if (token == "plex" || token == "normal-mode") return Mode::Normal;
  if (token == "normal" || token == "insert") return Mode::Insert;
  return Mode::Visual;
}

std::string stripComment(const std::string& line) {
  char quote = 0;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (quote != 0) {
      if (c == quote) quote = 0;
      continue;
    }
    if (c == '"' || c == '\'') {
      quote = c;
      continue;
    }
    if (c == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1]))) &&
        (i + 1 >= line.size() || !std::isxdigit(static_cast<unsigned char>(line[i + 1])))) {
      return line.substr(0, i);
    }
  }
  return line;
}

bool parseBool(const std::string& value, bool fallback) {
  const std::string normalized = lower(trim(value));
  if (normalized == "true" || normalized == "yes" || normalized == "on" ||
      normalized == "1") {
    return true;
  }
  if (normalized == "false" || normalized == "no" || normalized == "off" ||
      normalized == "0") {
    return false;
  }
  return fallback;
}

bool isColorSetting(const std::string& name) {
  static const std::vector<std::string> kColors = {
      "foreground", "background", "status-foreground", "status-background",
      "border", "focus-border", "selection", "selection-foreground",
      "active-border", "inactive-border", "tab-background", "tab-foreground",
      "active-tab-background", "active-tab-foreground", "cursor", "text",
      "keyword", "function", "variable", "type", "string", "comment",
      "number", "operator", "bracket"};
  return std::find(kColors.begin(), kColors.end(), name) != kColors.end();
}

std::vector<std::string> readLines(const std::string& path) {
  std::ifstream input(path);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) lines.push_back(line);
  return lines;
}

std::string themePathFor(const std::string& configPath) {
  std::error_code error;
  const std::filesystem::path parent =
      std::filesystem::path(configPath).parent_path();
  const std::vector<std::filesystem::path> candidates = {
      parent / "themes.conf",
      std::filesystem::path(executableDir()) / "data" / "themes.conf",
      std::filesystem::path(executableDir()) / ".." / "data" / "themes.conf",
      std::filesystem::path("config") / "themes.conf"};
  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate, error)) return candidate.string();
  }
  return "";
}

int modeIndex(Mode mode) {
  switch (mode) {
    case Mode::Normal: return 0;
    case Mode::Insert: return 1;
    case Mode::Visual: return 2;
  }
  return 0;
}

}  // namespace

Config::Config() : keymaps_(3) {}

bool Config::applySetting(Settings& settings, const std::string& name,
                          const std::string& value) {
  const std::string key = lower(trim(name));
  const std::string text = trim(value);
  if (key == "shell") settings.shell = expandHome(text);
  else if (key == "leader") settings.leader = keys::normalizeToken(text);
  else if (key == "tabbar") settings.tabbar = parseBool(text, settings.tabbar);
  else if (key == "statusline" || key == "status-line")
    settings.statusline = parseBool(text, settings.statusline);
  else if (key == "scrollback") {
    try { settings.scrollback = std::stoi(text); } catch (...) {}
  } else if (key == "resize-step" || key == "resize_step") {
    try { settings.resize_step = std::stoi(text); } catch (...) {}
  } else if (key == "foreground") settings.foreground = text;
  else if (key == "background") settings.background = text;
  else if (key == "status-foreground" || key == "status_foreground")
    settings.status_foreground = text;
  else if (key == "status-background" || key == "status_background")
    settings.status_background = text;
  else if (key == "border") settings.border = text;
  else if (key == "focus-border" || key == "focused-border" ||
           key == "focus_border")
    settings.focused_border = text;
  else if (key == "active-border" || key == "active_border")
    settings.active_border = text;
  else if (key == "inactive-border" || key == "inactive_border")
    settings.inactive_border = text;
  else if (key == "selection" || key == "selection-background" ||
           key == "selection_background")
    settings.selection_background = text;
  else if (key == "selection-foreground" || key == "selection_foreground")
    settings.selection_foreground = text;
  else if (key == "tab-background" || key == "tab_background")
    settings.tab_background = text;
  else if (key == "tab-foreground" || key == "tab_foreground")
    settings.tab_foreground = text;
  else if (key == "active-tab-background" || key == "active_tab_background")
    settings.active_tab_background = text;
  else if (key == "active-tab-foreground" || key == "active_tab_foreground")
    settings.active_tab_foreground = text;
  else if (key == "cursor") settings.cursor = text;
  else if (key == "text") settings.text = text;
  else if (key == "keyword") settings.keyword = text;
  else if (key == "function") settings.function = text;
  else if (key == "variable") settings.variable = text;
  else if (key == "type") settings.type = text;
  else if (key == "string") settings.string = text;
  else if (key == "comment") settings.comment = text;
  else if (key == "number") settings.number = text;
  else if (key == "operator") settings.op = text;
  else if (key == "bracket") settings.bracket = text;
  else return false;
  return true;
}

std::string Config::defaultPath() {
  if (const char* overridePath = std::getenv("NEXYTH_CONFIG")) {
    if (*overridePath != '\0') return overridePath;
  }
  if (const char* home = std::getenv("HOME")) {
    const std::string path = std::string(home) + "/.config/nexyth/config";
    if (std::filesystem::exists(path)) return path;
  }
  const std::vector<std::string> candidates = {
      executableDir() + "/data/default.conf",
      executableDir() + "/../data/default.conf",
      "config/default.conf"};
  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate)) return candidate;
  }
  return candidates.empty() ? "config/default.conf" : candidates.front();
}

Config Config::load(const std::string& path) {
  Config config;
  config.path = path.empty() ? defaultPath() : path;
  const std::vector<std::string> lines = readLines(config.path);
  Theme currentTheme;
  bool inTheme = false;

  for (const std::string& rawLine : lines) {
    const std::string content = stripComment(rawLine);
    const auto tokens = tokenizeConfigLine(content);
    if (tokens.empty()) continue;
    const std::string& verb = tokens[0];

    if (verb == "theme" && tokens.size() >= 2) {
      if (inTheme && !currentTheme.first.empty()) {
        config.themes.push_back(currentTheme);
      }
      currentTheme = {tokens[1], Settings{}};
      inTheme = true;
      continue;
    }
    if (verb == "set" && tokens.size() >= 3) {
      const std::string value = joinTokens(
          std::vector<std::string>(tokens.begin() + 2, tokens.end()), " ");
      if (!applySetting(config.settings, tokens[1], value)) {
        config.errors.push_back("unknown setting: " + tokens[1]);
      }
      if (inTheme) applySetting(currentTheme.second, tokens[1], value);
      continue;
    }
    if (verb == "bind" && tokens.size() >= 4) {
      const std::string modeToken = lower(tokens[1]);
      const std::string key = keys::expandLeader(
          keys::normalizeToken(tokens[2]), config.settings.leader);
      if (key.empty()) continue;
      Command command{tokens[3],
                      std::vector<std::string>(tokens.begin() + 4, tokens.end())};
      const std::string description =
          command.args.empty()
              ? command.action
              : command.action + " " + joinTokens(command.args, " ");
      if (modeToken == "all") {
        for (int index = 0; index < 3; ++index) {
          config.keymaps_[index].push_back({command, key, "all"});
          (void)description;
        }
      } else {
        config.keymaps_[modeIndex(modeFromToken(modeToken))].push_back(
            {command, key, modeToken});
      }
      continue;
    }
    if (verb == "unbind" && tokens.size() >= 3) {
      const std::string key = keys::expandLeader(
          keys::normalizeToken(tokens[2]), config.settings.leader);
      const std::string modeToken = lower(tokens[1]);
      auto removeFrom = [&](std::vector<Binding>& bindings) {
        bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                      [&](const Binding& binding) {
                                        return binding.key == key;
                                      }),
                       bindings.end());
      };
      if (modeToken == "all") {
        for (auto& bindings : config.keymaps_) removeFrom(bindings);
      } else {
        removeFrom(config.keymaps_[modeIndex(modeFromToken(modeToken))]);
      }
      continue;
    }
    if (verb == "define" && tokens.size() >= 3) {
      std::vector<Command> commands;
      Command current;
      for (std::size_t i = 2; i < tokens.size(); ++i) {
        if (tokens[i] == ";") {
          if (!current.action.empty()) commands.push_back(current);
          current = Command{};
          continue;
        }
        if (current.action.empty()) {
          current.action = keys::expandLeader(tokens[i],
                                              config.settings.leader);
        } else {
          current.args.push_back(tokens[i]);
        }
      }
      if (!current.action.empty()) commands.push_back(current);
      config.commands_[tokens[1]] = commands;
      continue;
    }
  }
  if (inTheme && !currentTheme.first.empty()) {
    config.themes.push_back(currentTheme);
  }

  const std::string themesFile = themePathFor(config.path);
  if (!themesFile.empty()) {
    auto saved = config;
    for (const auto& setting : {
             "foreground", "background", "status-foreground",
             "status-background", "border", "focus-border", "selection"}) {
      (void)setting;
    }
    saved.themes.clear();
    Theme active;
    bool inTheme = false;
    for (const std::string& rawLine : readLines(themesFile)) {
      const auto tokens = tokenizeConfigLine(stripComment(rawLine));
      if (tokens.empty()) continue;
      if (tokens[0] == "theme" && tokens.size() >= 2) {
        if (inTheme && !active.first.empty()) saved.themes.push_back(active);
        active = {tokens[1], Settings{}};
        inTheme = true;
        continue;
      }
      if (tokens[0] == "set" && tokens.size() >= 3) {
        const std::string value =
            joinTokens(std::vector<std::string>(tokens.begin() + 2,
                                                tokens.end()),
                       " ");
        applySetting(active.second, tokens[1], value);
      }
    }
    if (inTheme && !active.first.empty()) saved.themes.push_back(active);
    if (!saved.themes.empty()) config.themes = saved.themes;
  }
  return config;
}

std::vector<Command> Config::resolve(const std::string& name) const {
  const auto found = commands_.find(name);
  if (found == commands_.end()) return {Command{name, {}}};
  return found->second;
}

const Binding* Config::findBinding(Mode mode, const std::string& key) const {
  const auto& bindings = keymaps_[modeIndex(mode)];
  for (auto it = bindings.rbegin(); it != bindings.rend(); ++it) {
    if (it->key == key) return &*it;
  }
  return nullptr;
}

bool Config::hasPrefix(Mode mode, const std::string& key) const {
  if (key.empty()) return false;
  for (const auto& binding : keymaps_[modeIndex(mode)]) {
    if (binding.key.size() > key.size() &&
        binding.key.compare(0, key.size(), key) == 0) {
      return true;
    }
  }
  return false;
}

std::vector<std::pair<std::string, std::string>> Config::listBindings(
    Mode mode) const {
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto& binding : keymaps_[modeIndex(mode)]) {
    std::string description = binding.command.action;
    for (const auto& argument : binding.command.args) {
      description += " " + argument;
    }
    result.push_back({binding.key, description});
  }
  return result;
}

bool Config::saveColorSettings(const std::string& path,
                               const Settings& settings) {
  const std::map<std::string, std::string> colors = {
      {"foreground", settings.foreground},
      {"background", settings.background},
      {"status-foreground", settings.status_foreground},
      {"status-background", settings.status_background},
      {"border", settings.border},
      {"focus-border", settings.focused_border},
      {"selection", settings.selection_background}};
  const auto lines = readLines(path);
  std::vector<std::string> output;
  std::vector<std::string> seen;
  for (const std::string& line : lines) {
    const auto tokens = tokenizeConfigLine(stripComment(line));
    if (tokens.size() >= 2 && tokens[0] == "set") {
      const std::string name = lower(tokens[1]);
      if (isColorSetting(name) && colors.count(name) != 0) {
        output.push_back("set " + name + " " + colors.at(name));
        seen.push_back(name);
        continue;
      }
    }
    output.push_back(line);
  }
  for (const auto& [name, value] : colors) {
    if (std::find(seen.begin(), seen.end(), name) == seen.end()) {
      output.push_back("set " + name + " " + value);
    }
  }
  std::ofstream outputFile(path, std::ios::trunc);
  if (!outputFile) return false;
  for (const std::string& line : output) outputFile << line << "\n";
  return static_cast<bool>(outputFile);
}

}  // namespace nexyth
