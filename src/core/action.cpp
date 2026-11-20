#include "action.hpp"

#include <algorithm>

#include "app.hpp"

namespace nexyth {
namespace {

void add(ActionRegistry& registry, const std::string& name,
         ActionRegistry::Handler handler) {
  registry.registerAction(name, std::move(handler));
}

int toInt(const std::vector<std::string>& args, int index, int fallback) {
  if (index < 0 || index >= static_cast<int>(args.size())) return fallback;
  try {
    return std::stoi(args[static_cast<std::size_t>(index)]);
  } catch (...) {
    return fallback;
  }
}

std::string join(const std::vector<std::string>& args) {
  std::string result;
  for (const auto& argument : args) {
    if (!result.empty()) result += " ";
    result += argument;
  }
  return result;
}

}  // namespace

ActionRegistry::ActionRegistry() {
  add(*this, "plex", [](App& app, const auto&) { app.setMode(Mode::Normal); });
  add(*this, "normal", [](App& app, const auto&) { app.enterNormal(); });
  add(*this, "normal-home",
      [](App& app, const auto&) { app.enterNormal(true, false); });
  add(*this, "normal-end",
      [](App& app, const auto&) { app.enterNormal(false, true); });
  add(*this, "visual", [](App& app, const auto&) { app.enterVisual(); });
  add(*this, "help", [](App& app, const auto&) { app.toggleHelp(); });
  add(*this, "tab-list", [](App& app, const auto&) { app.toggleTabs(); });
  add(*this, "theme-list", [](App& app, const auto&) { app.toggleThemes(); });
  add(*this, "quit", [](App& app, const auto&) { app.quit(); });

  add(*this, "focus-left",
      [](App& app, const auto&) { app.focusDirection(-1, 0); });
  add(*this, "focus-right",
      [](App& app, const auto&) { app.focusDirection(1, 0); });
  add(*this, "focus-up",
      [](App& app, const auto&) { app.focusDirection(0, -1); });
  add(*this, "focus-down",
      [](App& app, const auto&) { app.focusDirection(0, 1); });
  add(*this, "focus-next", [](App& app, const auto&) { app.focusNext(); });
  add(*this, "focus-previous",
      [](App& app, const auto&) { app.focusPrevious(); });

  add(*this, "split-horizontal",
      [](App& app, const auto&) { app.splitWindow(false); });
  add(*this, "split-vertical",
      [](App& app, const auto&) { app.splitWindow(true); });
  add(*this, "split-pane-horizontal",
      [](App& app, const auto&) { app.splitFocused(false); });
  add(*this, "split-pane-vertical",
      [](App& app, const auto&) { app.splitFocused(true); });
  add(*this, "close-pane", [](App& app, const auto&) { app.closePane(); });

  add(*this, "new-tab", [](App& app, const auto&) { app.newTab(); });
  add(*this, "close-tab", [](App& app, const auto&) { app.closeTab(); });
  add(*this, "tab-next", [](App& app, const auto&) { app.nextTab(); });
  add(*this, "tab-prev", [](App& app, const auto&) { app.previousTab(); });
  add(*this, "tab-select", [](App& app, const auto& args) {
    app.selectTab(toInt(args, 0, 1));
  });

  add(*this, "resize-mode",
      [](App& app, const auto&) { app.toggleResizeMode(); });
  add(*this, "resize-left",
      [](App& app, const auto& args) {
        app.resizeFocused(-1, 0, toInt(args, 0, 0));
      });
  add(*this, "resize-right", [](App& app, const auto& args) {
    app.resizeFocused(1, 0, toInt(args, 0, 0));
  });
  add(*this, "resize-up", [](App& app, const auto& args) {
    app.resizeFocused(0, -1, toInt(args, 0, 0));
  });
  add(*this, "resize-down", [](App& app, const auto& args) {
    app.resizeFocused(0, 1, toInt(args, 0, 0));
  });
  add(*this, "resize-equal",
      [](App& app, const auto&) { app.resizeEqual(); });
  add(*this, "zoom-toggle",
      [](App& app, const auto&) { app.toggleZoom(); });
  add(*this, "clock", [](App& app, const auto&) { app.toggleClock(); });
  add(*this, "clock-toggle",
      [](App& app, const auto&) { app.toggleClock(); });

  add(*this, "scroll-up", [](App& app, const auto& args) {
    app.scroll(-toInt(args, 0, 5));
  });
  add(*this, "scroll-down", [](App& app, const auto& args) {
    app.scroll(toInt(args, 0, 5));
  });
  add(*this, "scroll-top",
      [](App& app, const auto&) { app.scrollToEdge(true); });
  add(*this, "scroll-bottom",
      [](App& app, const auto&) { app.scrollToEdge(false); });

  add(*this, "move-left",
      [](App& app, const auto&) { app.moveNavigation(-1, 0); });
  add(*this, "move-right",
      [](App& app, const auto&) { app.moveNavigation(1, 0); });
  add(*this, "move-up",
      [](App& app, const auto&) { app.moveNavigation(0, -1); });
  add(*this, "move-down",
      [](App& app, const auto&) { app.moveNavigation(0, 1); });
  add(*this, "move-word-forward",
      [](App& app, const auto&) { app.moveNavigationWord(1); });
  add(*this, "move-word-backward",
      [](App& app, const auto&) { app.moveNavigationWord(-1); });
  add(*this, "move-line-start",
      [](App& app, const auto&) { app.moveNavigationEdge(true, true); });
  add(*this, "move-line-end",
      [](App& app, const auto&) { app.moveNavigationEdge(false, true); });
  add(*this, "move-top",
      [](App& app, const auto&) { app.moveNavigationEdge(true, false); });
  add(*this, "move-bottom",
      [](App& app, const auto&) { app.moveNavigationEdge(false, false); });
  add(*this, "select-here",
      [](App& app, const auto&) { app.selectHere(); });
  add(*this, "yank", [](App& app, const auto&) { app.yank(false); });
  add(*this, "yank-line", [](App& app, const auto&) { app.yank(true); });
  add(*this, "paste", [](App& app, const auto&) { app.paste(); });
  add(*this, "send-keys",
      [](App& app, const auto& args) { app.sendKeys(join(args)); });
}

ActionRegistry& ActionRegistry::instance() {
  static ActionRegistry registry;
  return registry;
}

void ActionRegistry::registerAction(const std::string& name, Handler handler) {
  handlers_[name] = std::move(handler);
}

void ActionRegistry::execute(const std::string& name, App& app,
                             const std::vector<std::string>& args) {
  const auto found = handlers_.find(name);
  if (found != handlers_.end()) {
    found->second(app, args);
    return;
  }
  app.statusMessage("unknown action: " + name);
}

}  // namespace nexyth
