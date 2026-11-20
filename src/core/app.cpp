#include "app.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/screen/terminal.hpp"

#include "../config/keys.hpp"
#include "../core/action.hpp"
#include "../layout/node.hpp"
#include "../utils.hpp"

namespace nexyth {
namespace {

constexpr const char* kRefreshToken = "__nexyth_refresh__";

std::string unescapeKeys(const std::string& text) {
  std::string result;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '\\' || i + 1 >= text.size()) {
      result += text[i];
      continue;
    }
    const char next = text[++i];
    if (next == 'n') result += '\n';
    else if (next == 'r') result += '\r';
    else if (next == 't') result += '\t';
    else if (next == 'e') result += '\x1B';
    else if (next == '\\') result += '\\';
    else {
      result += '\\';
      result += next;
    }
  }
  return result;
}

bool containsInsensitive(const std::string& haystack,
                         const std::string& needle) {
  return lower(haystack).find(lower(needle)) != std::string::npos;
}

void forEachSplit(const NodePtr& node,
                  const std::function<void(const NodePtr&)>& callback) {
  if (!node || node->type != Node::Type::Split) return;
  callback(node);
  forEachSplit(node->first, callback);
  forEachSplit(node->second, callback);
}

}  // namespace

App::App(ftxui::ScreenInteractive* screen, std::string configPath)
    : screen_(screen),
      config_(Config::load(configPath)),
      workspace_(config_.settings.shell, config_.settings.scrollback,
                 [this](int) { refresh(); }),
      renderer_(*this) {
  workspace_.createInitial(80, 24);
  for (const auto& error : config_.errors) statusMessage(error);
  refresh_thread_ = std::thread([this] { refreshLoop(); });
}

App::~App() {
  refresh_running_.store(false, std::memory_order_relaxed);
  if (refresh_thread_.joinable()) refresh_thread_.join();
}

ftxui::Element App::Render() {
  const auto size = ftxui::Terminal::Size();
  const int width = std::max(10, size.dimx);
  const int height = std::max(4, size.dimy);
  renderer_.computeLayout(width, height);

  if (auto* active = ftxui::ScreenInteractive::Active()) {
    ftxui::Screen::Cursor cursor;
    cursor.shape = ftxui::Screen::Cursor::Hidden;
    if (const auto position = renderer_.terminalCursorScreenPosition()) {
      cursor.x = position->x;
      cursor.y = position->y;
      cursor.shape = ftxui::Screen::Cursor::Bar;
    }
    active->SetCursor(cursor);
  }

  return ftxui::canvas(width, height, [this, width, height](ftxui::Canvas& c) {
           renderer_.draw(c, width, height);
         }) |
         ftxui::flex;
}

bool App::OnEvent(ftxui::Event event) {
  if (event.input() == kRefreshToken) return true;
  if (event.is_mouse()) return handleMouse(event);
  if (event.input().empty()) return false;
  return handleKey(event);
}

bool App::handleKey(const ftxui::Event& event) {
  const std::string raw = event.input();
  std::string token = keys::normalizeToken(raw);

  if (raw == "\x1B\x1B") token = "<C-Esc>";
  if (raw == "\x1B" && mode_ == Mode::Insert) {
    const auto now = std::chrono::steady_clock::now();
    if (now - last_escape_time_ < std::chrono::milliseconds(350)) {
      token = "<C-Esc>";
      last_escape_time_ = {};
    } else {
      last_escape_time_ = now;
    }
  }

  if (token.empty()) return false;

  if (clock_active_ &&
      (token == "q" || mode_ == Mode::Insert)) {
    if (auto pane = workspace_.focusedPane()) {
      if (pane->id() == clock_pane_id_) {
        closeClock();
        return true;
      }
    }
  }

  if (overlay_ == Overlay::Rename) {
    if (token == "<Esc>") {
      renameTabCancel();
      return true;
    }
    if (token == "<CR>") {
      renameTabApply();
      return true;
    }
    if (token == "<BS>" || raw == "\x7F") {
      backspaceRenameChar();
      return true;
    }
    if (raw.size() == 1 && static_cast<unsigned char>(raw[0]) >= 0x20) {
      appendRenameChar(raw[0]);
      return true;
    }
    return true;
  }

  if (overlay_ == Overlay::Help) {
    if (token == "<Up>") {
      helpScroll(-1);
      return true;
    }
    if (token == "<Down>") {
      helpScroll(1);
      return true;
    }
    if (token == "<C-u>") {
      helpScroll(-1);
      return true;
    }
    if (token == "<C-d>") {
      helpScroll(1);
      return true;
    }
    if (token == "<BS>" || raw == "\x7F") {
      backspaceHelpFilter();
      return true;
    }
    if (token == "<Esc>") {
      if (help_filtering_) {
        help_filtering_ = false;
        help_filter_.clear();
        help_offset_ = 0;
        refresh();
      } else {
        closeOverlay();
      }
      return true;
    }
    if (token == "<CR>") {
      closeOverlay();
      return true;
    }
    if (token == "/" && !help_filtering_) {
      help_filtering_ = true;
      help_filter_.clear();
      help_offset_ = 0;
      refresh();
      return true;
    }
    if (raw.size() == 1 && static_cast<unsigned char>(raw[0]) >= 0x20) {
      appendHelpFilter(raw[0]);
      return true;
    }
    return true;
  }

  if (overlay_ == Overlay::Tabs) {
    if (token == "<Esc>" || token == "q") {
      closeOverlay();
      return true;
    }
    if (token.size() == 1 && std::isdigit(static_cast<unsigned char>(token[0]))) {
      selectTab(token[0] - '0');
      closeOverlay();
      return true;
    }
    return true;
  }

  if (overlay_ == Overlay::Themes) {
    if (token == "<Esc>" || token == "q" ||
        (token == "?" && !help_filtering_)) {
      themeCancel();
      return true;
    }
    if (token == "<Up>" || token == "k") {
      themePrevious();
      return true;
    }
    if (token == "<Down>" || token == "j") {
      themeNext();
      return true;
    }
    if (token == "<CR>" || token == "Enter") {
      themeApply();
      return true;
    }
    return true;
  }

  if (resize_mode_ && token == "<Esc>") {
    resize_mode_ = false;
    refresh();
    return true;
  }
  if (resize_mode_) {
    if (token == "h" || token == "H") {
      resizeFocused(-1, 0);
      return true;
    }
    if (token == "l" || token == "L") {
      resizeFocused(1, 0);
      return true;
    }
    if (token == "k" || token == "K") {
      resizeFocused(0, -1);
      return true;
    }
    if (token == "j" || token == "J") {
      resizeFocused(0, 1);
      return true;
    }
    if (token == "=") {
      resizeEqual();
      return true;
    }
  }

  const auto now = std::chrono::steady_clock::now();
  if (!pending_.empty() &&
      now - pending_time_ > std::chrono::milliseconds(1200)) {
    clearPending();
  }
  if (!pending_.empty()) {
    const std::string sequence = pending_ + token;
    if (const Binding* sequenceBinding = config_.findBinding(mode_, sequence)) {
      clearPending();
      runCommand(sequenceBinding->command.action,
                 sequenceBinding->command.args);
      return true;
    }
    if (config_.hasPrefix(mode_, sequence)) {
      pending_ = sequence;
      pending_time_ = now;
      refresh();
      return true;
    }
    clearPending();
  }

  const Binding* binding = config_.findBinding(mode_, token);
  if (binding != nullptr) {
    clearPending();
    runCommand(binding->command.action, binding->command.args);
    return true;
  }

  if (mode_ == Mode::Insert) {
    clearPending();
    if (auto pane = workspace_.focusedPane()) {
      pane->write(token == "<CR>" ? "\r" : raw);
    }
    return true;
  }

  const std::string sequence = token;
  binding = config_.findBinding(mode_, sequence);
  if (binding != nullptr) {
    clearPending();
    runCommand(binding->command.action, binding->command.args);
    return true;
  }
  if (config_.hasPrefix(mode_, sequence)) {
    pending_ = sequence;
    pending_time_ = now;
    refresh();
    return true;
  }
  clearPending();
  statusMessage("unbound key: " + sequence);
  return true;
}

bool App::handleMouse(ftxui::Event event) {
  const auto mouse = event.mouse();
  for (const auto& entry : renderer_.paneRects()) {
    if (!entry.pane || !entry.border.contains(mouse.x, mouse.y)) continue;
    workspace_.setFocus(entry.pane->id());
    if (mouse.button == ftxui::Mouse::WheelUp ||
        mouse.button == ftxui::Mouse::WheelDown) {
      scroll(mouse.button == ftxui::Mouse::WheelUp ? -3 : 3);
      return true;
    }
    if (mouse.button == ftxui::Mouse::Left) {
      if (mode_ != Mode::Visual) setMode(Mode::Visual);
      {
        std::lock_guard<std::mutex> lock(entry.pane->mutex());
        Buffer& buffer = entry.pane->buffer();
        const int x = std::clamp(mouse.x - entry.inner.x, 0,
                                 buffer.columns() - 1);
        const int y = std::clamp(mouse.y - entry.inner.y +
                                     buffer.visibleStart(),
                                 0,
                                 std::max(0, static_cast<int>(
                                                 buffer.lines().size()) -
                                                1));
        buffer.setAnchor({x, y});
        buffer.setNavigation({x, y});
      }
      refresh();
      return true;
    }
    return true;
  }
  return false;
}

void App::setMode(Mode mode) {
  if (mode_ == mode) return;
  mode_ = mode;
  clearPending();
  if (mode == Mode::Insert) {
    if (auto pane = workspace_.focusedPane()) {
      std::lock_guard<std::mutex> lock(pane->mutex());
      pane->buffer().scrollToBottom();
    }
  }
  refresh();
}

void App::enterNormal(bool home, bool end) {
  setMode(Mode::Insert);
  if (auto pane = workspace_.focusedPane()) {
    std::lock_guard<std::mutex> lock(pane->mutex());
    if (home) pane->write("\x1B[H");
    if (end) pane->write("\x1B[F");
  }
}

void App::enterVisual() {
  auto pane = workspace_.focusedPane();
  if (pane) {
    std::lock_guard<std::mutex> lock(pane->mutex());
    const Pos navigation = pane->buffer().navigation();
    pane->buffer().setAnchor(navigation);
  }
  setMode(Mode::Visual);
}

void App::toggleHelp() {
  if (overlay_ == Overlay::Help) {
    closeOverlay();
    return;
  }
  overlay_ = Overlay::Help;
  help_filter_.clear();
  help_filtering_ = false;
  help_offset_ = 0;
  clearPending();
  refresh();
}

void App::closeOverlay() {
  overlay_ = Overlay::None;
  clearPending();
  refresh();
}

void App::toggleTabs() {
  overlay_ = overlay_ == Overlay::Tabs ? Overlay::None : Overlay::Tabs;
  clearPending();
  refresh();
}

void App::toggleThemes() {
  if (overlay_ == Overlay::Themes) {
    themeCancel();
    return;
  }
  overlay_ = Overlay::Themes;
  theme_index_ = 0;
  clearPending();
  refresh();
}

void App::themeNext() {
  if (config_.themes.empty()) return;
  theme_index_ = (theme_index_ + 1) % config_.themes.size();
  applyTheme(config_.themes[theme_index_]);
}

void App::themePrevious() {
  if (config_.themes.empty()) return;
  theme_index_ =
      (theme_index_ + config_.themes.size() - 1) % config_.themes.size();
  applyTheme(config_.themes[theme_index_]);
}

void App::themeApply() {
  if (theme_index_ < config_.themes.size()) {
    applyTheme(config_.themes[theme_index_]);
    Config::saveColorSettings(config_.path, config_.settings);
    statusMessage("theme: " + config_.themes[theme_index_].first);
  }
  overlay_ = Overlay::None;
  refresh();
}

void App::themeCancel() {
  overlay_ = Overlay::None;
  refresh();
}

void App::startRenameTab() {
  overlay_ = Overlay::Rename;
  rename_buffer_ = workspace_.active().name;
  clearPending();
  refresh();
}

void App::appendRenameChar(char character) {
  rename_buffer_ += character;
  refresh();
}

void App::backspaceRenameChar() {
  if (!rename_buffer_.empty()) rename_buffer_.pop_back();
  refresh();
}

void App::renameTabApply() {
  if (rename_buffer_.empty()) rename_buffer_ = "shell";
  workspace_.setTabName(rename_buffer_);
  overlay_ = Overlay::None;
  rename_buffer_.clear();
  refresh();
}

void App::renameTabCancel() {
  overlay_ = Overlay::None;
  rename_buffer_.clear();
  refresh();
}

void App::applyTheme(const Theme& theme) {
  const Settings& source = theme.second;
  Settings& target = config_.settings;
  target.foreground = source.foreground;
  target.background = source.background;
  target.status_foreground = source.status_foreground;
  target.status_background = source.status_background;
  target.border = source.border;
  target.focused_border = source.focused_border;
  target.selection_background = source.selection_background;
  refresh();
}

void App::helpScroll(int delta) {
  const auto bindings = filteredBindings();
  const int boxHeight =
      std::min(30, std::max(8, ftxui::Terminal::Size().dimy - 4));
  const int perPage = std::max(1, (boxHeight - 3) * 2);
  const int pages =
      std::max(0, (static_cast<int>(bindings.size()) + perPage - 1) / perPage - 1);
  const int next = static_cast<int>(help_offset_) + delta;
  help_offset_ = static_cast<std::size_t>(std::clamp(next, 0, pages));
  refresh();
}

void App::appendHelpFilter(char character) {
  help_filtering_ = true;
  help_filter_ += character;
  help_offset_ = 0;
  refresh();
}

void App::backspaceHelpFilter() {
  if (help_filtering_ && !help_filter_.empty()) {
    help_filter_.pop_back();
    help_offset_ = 0;
    refresh();
  }
}

void App::toggleZoom() {
  zoom_ = !zoom_;
  refresh();
}

void App::toggleResizeMode() {
  resize_mode_ = !resize_mode_;
  refresh();
}

void App::toggleClock() {
  if (clock_active_) {
    closeClock();
    return;
  }
  if (auto pane = workspace_.focusedPane()) {
    clock_pane_id_ = pane->id();
    clock_active_.store(true, std::memory_order_relaxed);
    refresh();
  }
}

void App::closeClock() {
  clock_active_.store(false, std::memory_order_relaxed);
  clock_pane_id_ = -1;
  refresh();
}

void App::focusDirection(int dx, int dy) {
  auto pane = workspace_.focusedPane();
  if (!pane) return;
  const auto currentRect = renderer_.rectFor(pane->id());
  if (!currentRect) {
    workspace_.focusNextPane();
    refresh();
    return;
  }
  const int cx = currentRect->x + currentRect->width / 2;
  const int cy = currentRect->y + currentRect->height / 2;
  int bestDistance = std::numeric_limits<int>::max();
  int bestId = -1;
  for (const auto& entry : renderer_.paneRects()) {
    if (!entry.pane || entry.pane->id() == pane->id()) continue;
    const int x = entry.inner.x + entry.inner.width / 2;
    const int y = entry.inner.y + entry.inner.height / 2;
    if (dx > 0 && x <= cx) continue;
    if (dx < 0 && x >= cx) continue;
    if (dy > 0 && y <= cy) continue;
    if (dy < 0 && y >= cy) continue;
    const int distance = std::abs(x - cx) + std::abs(y - cy);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestId = entry.pane->id();
    }
  }
  if (bestId >= 0) workspace_.setFocus(bestId);
  refresh();
}

void App::focusNext() {
  workspace_.focusNextPane();
  refresh();
}

void App::focusPrevious() {
  workspace_.focusPreviousPane();
  refresh();
}

void App::splitFocused(bool vertical) {
  auto pane = workspace_.focusedPane();
  int columns = 80;
  int rows = 24;
  if (pane) {
    std::lock_guard<std::mutex> lock(pane->mutex());
    columns = std::max(1, pane->buffer().columns());
    rows = std::max(1, pane->buffer().rows());
  }
  workspace_.splitFocused(vertical, columns, rows);
  zoom_ = false;
  refresh();
}

void App::splitWindow(bool vertical) {
  auto pane = workspace_.focusedPane();
  int columns = 80;
  int rows = 24;
  if (pane) {
    std::lock_guard<std::mutex> lock(pane->mutex());
    columns = std::max(1, pane->buffer().columns());
    rows = std::max(1, pane->buffer().rows());
  }
  workspace_.splitWindow(vertical, columns, rows);
  zoom_ = false;
  refresh();
}

void App::closePane() {
  workspace_.closePane();
  zoom_ = false;
  refresh();
}

void App::newTab() {
  workspace_.newTab(80, 24);
  zoom_ = false;
  refresh();
}

void App::closeTab() {
  workspace_.closeTab();
  zoom_ = false;
  refresh();
}

void App::selectTab(int oneBased) {
  workspace_.selectTab(oneBased);
  zoom_ = false;
  refresh();
}

void App::nextTab() {
  workspace_.selectTab(workspace_.activeIndex() + 2 > workspace_.tabCount()
                           ? 1
                           : workspace_.activeIndex() + 2);
  refresh();
}

void App::previousTab() {
  workspace_.selectTab(workspace_.activeIndex() == 0 ? workspace_.tabCount()
                                                     : workspace_.activeIndex());
  refresh();
}

void App::resizeFocused(int dx, int dy, int amount) {
  auto pane = workspace_.focusedPane();
  if (!pane || zoom_) return;
  const NodePtr parent = findParent(workspace_.active().root, pane->id());
  if (!parent) return;
  const bool matchesAxis =
      (dx != 0 && parent->kind == SplitKind::SideBySide) ||
      (dy != 0 && parent->kind == SplitKind::Stacked);
  if (!matchesAxis) return;
  const int step =
      amount != 0 ? amount : config_.settings.resize_step;
  const double delta = std::clamp(step, -100, 100) / 100.0;
  const int direction = dx != 0 ? dx : dy;
  parent->ratio =
      std::clamp(parent->ratio + direction * delta, 0.05, 0.95);
  refresh();
}

void App::resizeEqual() {
  forEachSplit(workspace_.active().root,
               [](const NodePtr& node) { node->ratio = 0.5; });
  refresh();
}

void App::scroll(int delta) {
  auto pane = workspace_.focusedPane();
  if (!pane) return;
  std::lock_guard<std::mutex> lock(pane->mutex());
  pane->buffer().scrollBy(delta);
  refresh();
}

void App::scrollToEdge(bool top) {
  auto pane = workspace_.focusedPane();
  if (!pane) return;
  std::lock_guard<std::mutex> lock(pane->mutex());
  if (top) pane->buffer().scrollToTop();
  else pane->buffer().scrollToBottom();
  refresh();
}

void App::moveNavigation(int dx, int dy) {
  auto pane = workspace_.focusedPane();
  if (!pane) return;
  std::lock_guard<std::mutex> lock(pane->mutex());
  pane->buffer().moveNavigation(dx, dy);
  refresh();
}

void App::moveNavigationWord(int direction) {
  auto pane = workspace_.focusedPane();
  if (!pane) return;
  std::lock_guard<std::mutex> lock(pane->mutex());
  Buffer& buffer = pane->buffer();
  const int start = buffer.visibleStart();
  const int end =
      std::min(static_cast<int>(buffer.lines().size()) - 1,
               start + buffer.rows() - 1);
  int y = buffer.navigation().y;
  int x = buffer.navigation().x;
  const auto isWord = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
  };
  if (direction > 0) {
    while (y <= end && y < static_cast<int>(buffer.lines().size())) {
      const auto& line = buffer.lines()[static_cast<std::size_t>(y)];
      while (x + 1 < buffer.columns() &&
             static_cast<std::size_t>(x) < line.size() &&
             line[static_cast<std::size_t>(x)].text.empty()) {
        ++x;
      }
      ++x;
      if (x < buffer.columns() && static_cast<std::size_t>(x) < line.size() &&
          !line[static_cast<std::size_t>(x)].text.empty() &&
          isWord(line[static_cast<std::size_t>(x)].text[0])) {
        break;
      }
      if (x >= buffer.columns()) {
        x = 0;
        ++y;
      } else {
        break;
      }
    }
  } else {
    while (y >= start && y < static_cast<int>(buffer.lines().size())) {
      const auto& line = buffer.lines()[static_cast<std::size_t>(y)];
      --x;
      if (x < 0) {
        x = buffer.columns() - 1;
        --y;
        continue;
      }
      if (static_cast<std::size_t>(x) < line.size() &&
          !line[static_cast<std::size_t>(x)].text.empty() &&
          isWord(line[static_cast<std::size_t>(x)].text[0])) {
        break;
      }
    }
  }
  buffer.setNavigation({std::clamp(x, 0, buffer.columns() - 1),
                        std::clamp(y, start, std::max(start, end))});
  refresh();
}

void App::moveNavigationEdge(bool top, bool line) {
  auto pane = workspace_.focusedPane();
  if (!pane) return;
  std::lock_guard<std::mutex> lock(pane->mutex());
  Buffer& buffer = pane->buffer();
  const int start = buffer.visibleStart();
  const int end = std::min(static_cast<int>(buffer.lines().size()) - 1,
                           start + buffer.rows() - 1);
  Pos position = buffer.navigation();
  if (line) {
    position.x = top ? 0 : buffer.columns() - 1;
  } else {
    position.y = top ? start : end;
  }
  buffer.setNavigation(position);
  refresh();
}

void App::selectHere() {
  auto pane = workspace_.focusedPane();
  if (!pane) return;
  std::lock_guard<std::mutex> lock(pane->mutex());
  pane->buffer().setAnchor(pane->buffer().navigation());
  refresh();
}

void App::yank(bool wholeLine) {
  auto pane = workspace_.focusedPane();
  if (!pane) return;
  std::string text;
  {
    std::lock_guard<std::mutex> lock(pane->mutex());
    Buffer& buffer = pane->buffer();
    if (!wholeLine) text = buffer.selectedText();
    if (text.empty() && buffer.navigation().y >= 0 &&
        buffer.navigation().y < static_cast<int>(buffer.lines().size())) {
      for (const auto& cell :
           buffer.lines()[static_cast<std::size_t>(buffer.navigation().y)]) {
        text += cell.text;
      }
    }
    buffer.clearSelection();
  }
  while (!text.empty() && text.back() == '\n') text.pop_back();
  clipboard_ = text;
  emitClipboard(clipboard_);
  setMode(Mode::Normal);
  statusMessage("yanked " + std::to_string(clipboard_.size()) + " bytes");
}

void App::paste() {
  if (clipboard_.empty()) {
    statusMessage("clipboard empty");
    return;
  }
  sendKeys("\x1B[200~" + clipboard_ + "\x1B[201~");
  statusMessage("pasted " + std::to_string(clipboard_.size()) + " bytes");
}

void App::sendKeys(const std::string& data) {
  if (auto pane = workspace_.focusedPane()) pane->write(unescapeKeys(data));
}

void App::emitClipboard(const std::string& text) {
  std::cout << "\x1B]52;c;" << base64Encode(text) << "\a" << std::flush;
}

void App::quit() {
  if (screen_ != nullptr) screen_->Exit();
}

void App::runCommand(const std::string& name,
                     const std::vector<std::string>& args) {
  for (const auto& command : config_.resolve(name)) {
    if (command.action == name && command.args == args &&
        config_.resolve(name).size() == 1) {
      ActionRegistry::instance().execute(name, *this, args);
      return;
    }
    runCommand(command.action, command.args);
  }
}

void App::refresh() {
  refresh_pending_.store(true, std::memory_order_relaxed);
}

void App::statusMessage(std::string message) {
  {
    std::lock_guard<std::mutex> lock(message_mutex_);
    message_ = std::move(message);
    message_time_ = std::chrono::steady_clock::now();
  }
  refresh();
}

std::string App::statusMessage() const {
  std::lock_guard<std::mutex> lock(message_mutex_);
  if (!message_.empty() &&
      std::chrono::steady_clock::now() - message_time_ >
          std::chrono::seconds(5)) {
    const_cast<App*>(this)->message_.clear();
  }
  return message_;
}

void App::refreshLoop() {
  constexpr auto kFrameInterval = std::chrono::milliseconds(16);
  auto last_clock_tick = std::chrono::steady_clock::now();
  while (refresh_running_.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(kFrameInterval);
    const auto now = std::chrono::steady_clock::now();
    if (clock_active_.load(std::memory_order_relaxed) &&
        now - last_clock_tick >= std::chrono::seconds(1)) {
      refresh_pending_.store(true, std::memory_order_relaxed);
      last_clock_tick = now;
    }
    if (refresh_pending_.exchange(false, std::memory_order_relaxed) &&
        screen_ != nullptr) {
      screen_->PostEvent(ftxui::Event::Special(kRefreshToken));
    }
  }
}

void App::clearPending() {
  pending_.clear();
}

std::vector<std::pair<std::string, std::string>> App::filteredBindings() const {
  const auto bindings = config_.listBindings(mode_);
  if (!help_filtering_ || help_filter_.empty()) return bindings;
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto& binding : bindings) {
    if (containsInsensitive(binding.first + " " + binding.second,
                            help_filter_)) {
      result.push_back(binding);
    }
  }
  return result;
}

}  // namespace nexyth
