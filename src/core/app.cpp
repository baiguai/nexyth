#include "app.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/screen/terminal.hpp"

#include "../config/keys.hpp"
#include "../core/action.hpp"
#include "../layout/node.hpp"
#include "../utils.hpp"

namespace nexyth
{
    /*>
        Helper Methods
    */
        namespace
        {
            /*!
                Refresh Token

            */
            //>>
            constexpr const char* kRefreshToken = "__nexyth_refresh__";
            //<<
            //!

            /*!
                Unescape Keys

                !_method
            */
            std::string unescapeKeys(const std::string& text)
            {
                std::string result;

                for (std::size_t i = 0; i < text.size(); ++i)
                {
                    if (text[i] != '\\' || i + 1 >= text.size())
                    {
                        result += text[i];
                        continue;
                    }

                    const char next = text[++i];

                    if (next == 'n') result += '\n';
                    else if (next == 'r') result += '\r';
                    else if (next == 't') result += '\t';
                    else if (next == 'e') result += '\x1B';
                    else if (next == '\\') result += '\\';
                    else
                    {
                        result += '\\';
                        result += next;
                    }
                }
                return result;
            }
            //!

            /*!
                Case Insensitive Contains

                !_method
            */
            bool containsInsensitive(const std::string& haystack, const std::string& needle)
            {
                return lower(haystack).find(lower(needle)) != std::string::npos;
            }
            //!

            /*!
                For Each Split

                !_method
            */
            void forEachSplit(const NodePtr& node, const std::function<void(const NodePtr&)>& callback)
            {
                if (!node || node->type != Node::Type::Split) return;

                callback(node);
                forEachSplit(node->first, callback);
                forEachSplit(node->second, callback);
            }
            //!

#ifdef _WIN32
            bool writeWindowsClipboard(const std::string& text)
            {
                if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
                const int required = MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                    static_cast<int>(text.size()), nullptr, 0);
                if (required <= 0) return false;

                std::vector<wchar_t> wide(static_cast<std::size_t>(required) + 1, L'\0');
                if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                        static_cast<int>(text.size()), wide.data(),
                                        required) != required)
                {
                    return false;
                }

                const HGLOBAL memory =
                    GlobalAlloc(GMEM_MOVEABLE, wide.size() * sizeof(wchar_t));
                if (memory == nullptr) return false;
                wchar_t* destination = static_cast<wchar_t*>(GlobalLock(memory));
                if (destination == nullptr)
                {
                    GlobalFree(memory);
                    return false;
                }
                std::copy(wide.begin(), wide.end(), destination);
                GlobalUnlock(memory);

                if (!OpenClipboard(nullptr))
                {
                    GlobalFree(memory);
                    return false;
                }
                EmptyClipboard();
                const HANDLE result = SetClipboardData(CF_UNICODETEXT, memory);
                CloseClipboard();
                if (result == nullptr)
                {
                    GlobalFree(memory);
                    return false;
                }
                return true;
            }
#else
            /*!
                Run Clipboard Command

                !_method
            */
            bool runClipboardCommand(const std::vector<std::string>& argv, const std::string& text)
            {
                int pipes[2];
                if (pipe(pipes) != 0) return false;
                const pid_t pid = fork();

                if (pid < 0)
                {
                    close(pipes[0]);
                    close(pipes[1]);
                    return false;
                }

                if (pid == 0)
                {
                    close(pipes[1]);
                    dup2(pipes[0], STDIN_FILENO);
                    close(pipes[0]);
                    const int devnull = open("/dev/null", O_WRONLY);

                    if (devnull >= 0)
                    {
                        dup2(devnull, STDOUT_FILENO);
                        dup2(devnull, STDERR_FILENO);
                        close(devnull);
                    }

                    std::vector<char*> args;

                    for (const auto& argument : argv)
                    {
                        args.push_back(const_cast<char*>(argument.c_str()));
                    }

                    args.push_back(nullptr);
                    execvp(args[0], args.data());
                    _exit(127);
                }

                close(pipes[0]);
                struct sigaction ignorePipe {};
                struct sigaction previousPipe {};
                ignorePipe.sa_handler = SIG_IGN;
                sigemptyset(&ignorePipe.sa_mask);
                sigaction(SIGPIPE, &ignorePipe, &previousPipe);
                std::size_t offset = 0;

                while (offset < text.size())
                {
                    const ssize_t written =
                    write(pipes[1], text.data() + offset, text.size() - offset);

                    if (written > 0)
                    {
                        offset += static_cast<std::size_t>(written);
                        continue;
                    }
                    if (written < 0 && errno == EINTR) continue;
                    break;
                }

                close(pipes[1]);
                sigaction(SIGPIPE, &previousPipe, nullptr);

                int status = 0;

                while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
                {
                }

                return WIFEXITED(status) && WEXITSTATUS(status) == 0;
            }
            //!
#endif

            /*!
                Write System Clipboard

                !_method
            */
            bool writeSystemClipboard(const std::string& text)
            {
#ifdef _WIN32
                return writeWindowsClipboard(text);
#else
                /*|
                    Commands:

                */
                //>>
                static const std::vector<std::vector<std::string>> commands = {
                    {"wl-copy"},
                    {"xclip", "-selection", "clipboard"},
                    {"xsel", "--clipboard", "--input"},
                    {"pbcopy"},
                    {"clip.exe"},
                };
                //<<
                //!

                for (const auto& command : commands)
                {
                    if (runClipboardCommand(command, text)) return true;
                }
                return false;
#endif
            }
            //!
        }
    //!

    /*>
        Ftxui Methods
    */
        /*!
            Ftxui App

        */
        //>>
        App::App(ftxui::ScreenInteractive* screen, std::string configPath)
            : screen_(screen),
              config_(Config::load(configPath)),
              workspace_(config_.settings.shell, config_.settings.scrollback,
                         [this](int) { refresh(); }),
              renderer_(*this)
        {
            workspace_.createInitial(80, 24);
            for (const auto& error : config_.errors) statusMessage(error);
            refresh_thread_ = std::thread([this] { refreshLoop(); });
        }
        //<<
        //!

        /*!
            Ftxui App Destructor

        */
        //>>
        App::~App()
        {
            refresh_running_.store(false, std::memory_order_relaxed);
            if (refresh_thread_.joinable()) refresh_thread_.join();
        }
        //<<
        //!

        /*!
            Ftxui Render

        */
        //>>
        ftxui::Element App::OnRender()
        {
            const auto size = ftxui::Terminal::Size();
            const int width = std::max(10, size.dimx);
            const int height = std::max(4, size.dimy);
            renderer_.computeLayout(width, height);

            if (auto* active = ftxui::ScreenInteractive::Active())
            {
                ftxui::Screen::Cursor cursor;
                cursor.shape = ftxui::Screen::Cursor::Hidden;

                if (const auto position = renderer_.terminalCursorScreenPosition())
                {
                    cursor.x = position->x;
                    cursor.y = position->y;
                    cursor.shape = ftxui::Screen::Cursor::Bar;
                }
                active->SetCursor(cursor);
            }

            return ftxui::canvas(width, height, [this, width, height](ftxui::Canvas& c)
            {
                renderer_.draw(c, width, height);
            }) |
            ftxui::flex;
        }
        //<<
        //!

        /*!
            Ftxui On Event

            !_method
        */
        bool App::OnEvent(ftxui::Event event)
        {
            if (event.input() == kRefreshToken) return true;
            if (event.is_mouse()) return handleMouse(event);
            if (event.input().empty()) return false;
            return handleKey(event);
        }
        //!

        /*!
            Handle the Key Combination

            !_method
        */
        bool App::handleKey(const ftxui::Event& event)
        {
            const std::string raw = event.input();
            std::string token = keys::normalizeToken(raw);

            if (raw == "\x1B\x1B") token = "<C-Esc>";

            if (raw == "\x1B" && mode_ == Mode::Insert)
            {
                const auto now = std::chrono::steady_clock::now();

                if (now - last_escape_time_ < std::chrono::milliseconds(350))
                {
                    token = "<C-Esc>";
                    last_escape_time_ = {};
                }
                else
                {
                    last_escape_time_ = now;
                }
            }

            if (token.empty()) return false;

            if (clock_active_ && (token == "q" || mode_ == Mode::Insert))
            {
                if (auto pane = workspace_.focusedPane())
                {
                    if (pane->id() == clock_pane_id_)
                    {
                        closeClock();
                        return true;
                    }
                }
            }

            if (overlay_ == Overlay::Rename)
            {
                if (token == "<Esc>")
                {
                    renameTabCancel();
                    return true;
                }

                if (token == "<CR>")
                {
                    renameTabApply();
                    return true;
                }

                if (token == "<BS>" || raw == "\x7F")
                {
                    backspaceRenameChar();
                    return true;
                }

                if (raw.size() == 1 && static_cast<unsigned char>(raw[0]) >= 0x20)
                {
                    appendRenameChar(raw[0]);
                    return true;
                }

                return true;
            }

            if (overlay_ == Overlay::Help)
            {
                if (token == "<Up>")
                {
                    helpScroll(-1);
                    return true;
                }
                if (token == "<Down>")
                {
                    helpScroll(1);
                    return true;
                }
                if (token == "<C-u>")
                {
                    helpScroll(-1);
                    return true;
                }
                if (token == "<C-d>")
                {
                    helpScroll(1);
                    return true;
                }
                if (token == "<BS>" || raw == "\x7F")
                {
                    backspaceHelpFilter();
                    return true;
                }

                if (token == "<Esc>")
                {
                    if (help_filtering_)
                    {
                        help_filtering_ = false;
                        help_filter_.clear();
                        help_offset_ = 0;
                        refresh();
                    }
                    else
                    {
                        closeOverlay();
                    }
                    return true;
                }

                if (token == "<CR>")
                {
                    closeOverlay();
                    return true;
                }

                if (token == "/" && !help_filtering_)
                {
                    help_filtering_ = true;
                    help_filter_.clear();
                    help_offset_ = 0;
                    refresh();
                    return true;
                }

                if (raw.size() == 1 && static_cast<unsigned char>(raw[0]) >= 0x20)
                {
                    appendHelpFilter(raw[0]);
                    return true;
                }

                return true;
            }

            if (overlay_ == Overlay::Tabs) {
                if (token == "<Esc>" || token == "q")
                {
                    closeOverlay();
                    return true;
                }

                if (token.size() == 1 && std::isdigit(static_cast<unsigned char>(token[0])))
                {
                    selectTab(token[0] - '0');
                    closeOverlay();
                    return true;
                }

                return true;
            }

            if (overlay_ == Overlay::Themes)
            {
                if (token == "<Esc>" || token == "q" || (token == "?" && !help_filtering_))
                {
                    themeCancel();
                    return true;
                }
                if (token == "<Up>" || token == "k")
                {
                    themePrevious();
                    return true;
                }
                if (token == "<Down>" || token == "j")
                {
                    themeNext();
                    return true;
                }
                if (token == "<CR>" || token == "Enter")
                {
                    themeApply();
                    return true;
                }

                return true;
            }

            if (resize_mode_ && token == "<Esc>")
            {
                resize_mode_ = false;
                refresh();
                return true;
            }

            if (resize_mode_)
            {
                if (token == "h" || token == "H")
                {
                    resizeFocused(-1, 0);
                    return true;
                }
                if (token == "l" || token == "L")
                {
                    resizeFocused(1, 0);
                    return true;
                }
                if (token == "k" || token == "K")
                {
                    resizeFocused(0, -1);
                    return true;
                }
                if (token == "j" || token == "J")
                {
                    resizeFocused(0, 1);
                    return true;
                }
                if (token == "=")
                {
                    resizeEqual();
                    return true;
                }
            }

            const auto now = std::chrono::steady_clock::now();

            if (!pending_.empty() && now - pending_time_ > std::chrono::milliseconds(1200))
            {
                clearPending();
            }

            if (!pending_.empty())
            {
                const std::string sequence = pending_ + token;

                if (const Binding* sequenceBinding = config_.findBinding(mode_, sequence))
                {
                    clearPending();
                    runCommand(sequenceBinding->command.action,
                    sequenceBinding->command.args);
                    return true;
                }

                if (config_.hasPrefix(mode_, sequence))
                {
                    pending_ = sequence;
                    pending_time_ = now;
                    refresh();
                    return true;
                }

                clearPending();
            }

            const Binding* binding = config_.findBinding(mode_, token);

            if (binding != nullptr)
            {
                clearPending();
                runCommand(binding->command.action, binding->command.args);
                return true;
            }

            if (mode_ == Mode::Insert)
            {
                clearPending();
                if (auto pane = workspace_.focusedPane())
                {
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

            if (config_.hasPrefix(mode_, sequence))
            {
                pending_ = sequence;
                pending_time_ = now;
                refresh();
                return true;
            }

            clearPending();
            statusMessage("unbound key: " + sequence);
            return true;
        }
        //!

        /*!
            Handle the Mouse

            !_method
        */
        bool App::handleMouse(ftxui::Event event) {
            const auto mouse = event.mouse();

            for (const auto& entry : renderer_.paneRects())
            {
                if (!entry.pane || !entry.border.contains(mouse.x, mouse.y)) continue;
                workspace_.setFocus(entry.pane->id());

                if (mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown)
                {
                    scroll(mouse.button == ftxui::Mouse::WheelUp ? -3 : 3);
                    return true;
                }

                if (mouse.button == ftxui::Mouse::Left)
                {
                    if (mode_ != Mode::Visual) setMode(Mode::Visual);

                    selecting_ = true;

                    {
                        std::lock_guard<std::mutex> lock(entry.pane->mutex());
                        Buffer& buffer = entry.pane->buffer();
                        const int x = std::clamp(mouse.x - entry.inner.x, 0, buffer.columns() - 1);
                        const int y = std::clamp(
                            mouse.y - entry.inner.y +
                            buffer.visibleStart(),
                            0,
                            std::max
                            (
                                0, static_cast<int>(
                                buffer.lines().size()) -
                                1
                            )
                        );
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
        //!
    //!

    /*!
        App Methods
    */
        /*!
            Quit

            !_method
        */
        void App::quit()
        {
            if (screen_ != nullptr) screen_->Exit();
        }
        //!

        /*!
            Run Command

            !_method
        */
        void App::runCommand(const std::string& name, const std::vector<std::string>& args)
        {
            for (const auto& command : config_.resolve(name))
            {
                if (command.action == name && command.args == args && config_.resolve(name).size() == 1)
                {
                    ActionRegistry::instance().execute(name, *this, args);
                    return;
                }

                runCommand(command.action, command.args);
            }
        }
        //!

        /*!
            Refresh

            !_method
        */
        void App::refresh()
        {
            refresh_pending_.store(true, std::memory_order_relaxed);
        }
        //!

        /*!
            Status Message

            !_method
        */
        void App::statusMessage(std::string message)
        {
            {
                std::lock_guard<std::mutex> lock(message_mutex_);
                message_ = std::move(message);
                message_time_ = std::chrono::steady_clock::now();
            }
            refresh();
        }
        //!

        /*!
            Status Message String

            !_method
        */
        std::string App::statusMessage() const
        {
            std::lock_guard<std::mutex> lock(message_mutex_);
            if (!message_.empty() && std::chrono::steady_clock::now() - message_time_ > std::chrono::seconds(5))
            {
                const_cast<App*>(this)->message_.clear();
            }
            return message_;
        }
        //!

        /*!
            Refresh Loop

            !_method
        */
        void App::refreshLoop()
        {
            constexpr auto kFrameInterval = std::chrono::milliseconds(16);
            auto last_clock_tick = std::chrono::steady_clock::now();

            while (refresh_running_.load(std::memory_order_relaxed))
            {
                std::this_thread::sleep_for(kFrameInterval);
                const auto now = std::chrono::steady_clock::now();

                if (clock_active_.load(std::memory_order_relaxed) && now - last_clock_tick >= std::chrono::seconds(1))
                {
                    refresh_pending_.store(true, std::memory_order_relaxed);
                    last_clock_tick = now;
                }

                if (refresh_pending_.exchange(false, std::memory_order_relaxed) && screen_ != nullptr)
                {
                    screen_->PostEvent(ftxui::Event::Special(kRefreshToken));
                }
            }
        }
        //!

        /*!
            Clear Pending

            !_method
        */
        void App::clearPending() {
            pending_.clear();
        }
        //!
    //!

    /*!
        Mode Switching
    */
        /*!
            Set the Mode

            !_method
        */
        void App::setMode(Mode mode)
        {
            if (mode_ == mode) return;

            if (mode != Mode::Visual && selecting_)
            {
                selecting_ = false;

                if (auto pane = workspace_.focusedPane())
                {
                    std::lock_guard<std::mutex> lock(pane->mutex());
                    pane->buffer().clearSelection();
                }
            }

            mode_ = mode;
            clearPending();

            if (mode == Mode::Insert)
            {
                if (auto pane = workspace_.focusedPane())
                {
                    std::lock_guard<std::mutex> lock(pane->mutex());
                    pane->buffer().scrollToBottom();
                }
            }
            refresh();
        }
        //!

        /*!
            Enter Normal Mode

            !_method
        */
        void App::enterNormal(bool home, bool end)
        {
            setMode(Mode::Insert);

            if (auto pane = workspace_.focusedPane())
            {
                std::lock_guard<std::mutex> lock(pane->mutex());

                if (home) pane->write("\x1B[H");
                if (end) pane->write("\x1B[F");
            }
        }
        //!

        /*!
            Enter Visual Mode

            !_method
        */
        void App::enterVisual()
        {
            selecting_ = false;

            if (auto pane = workspace_.focusedPane())
            {
                std::lock_guard<std::mutex> lock(pane->mutex());
                pane->buffer().clearSelection();
            }

            setMode(Mode::Visual);
        }
        //!

        /*!
            Exit Visual Mode

            !_method
        */
        void App::exitVisual() {
            if (selecting_)
            {
                selecting_ = false;

                if (auto pane = workspace_.focusedPane())
                {
                    std::lock_guard<std::mutex> lock(pane->mutex());
                    pane->buffer().clearSelection();
                }

                refresh();
                return;
            }

            setMode(Mode::Normal);
        }
        //!
    //!

    /*!
        Pane Handling
    */
        /*!
            Split Current Pane

            !_method
        */
        void App::splitFocused(bool vertical)
        {
            auto pane = workspace_.focusedPane();
            int columns = 80;
            int rows = 24;

            if (pane)
            {
                std::lock_guard<std::mutex> lock(pane->mutex());
                columns = std::max(1, pane->buffer().columns());
                rows = std::max(1, pane->buffer().rows());
            }

            workspace_.splitFocused(vertical, columns, rows);
            zoom_ = false;

            refresh();
        }
        //!

        /*!
            Close Current Pane

            !_method
        */
        void App::closePane()
        {
            workspace_.closePane();
            zoom_ = false;
            refresh();
        }
        //!

        /*!
            Resize Focused Pane

            !_method
        */
        void App::resizeFocused(int dx, int dy, int amount) {
            auto pane = workspace_.focusedPane();

            if (!pane || zoom_ || (dx == 0 && dy == 0)) return;
            const SplitKind kind = dx != 0 ? SplitKind::SideBySide : SplitKind::Stacked;
            const NodePtr split =
            findResizeSplit(workspace_.active().root, pane->id(), kind);

            if (!split) return;

            const int step = amount != 0 ? amount : config_.settings.resize_step;
            const double delta = std::clamp(step, -100, 100) / 100.0;
            const int direction = dx != 0 ? dx : dy;
            split->ratio = std::clamp(split->ratio + direction * delta, 0.05, 0.95);

            refresh();
        }
        //!

        /*!
            Equal Size All Panes

            !_method
        */
        void App::resizeEqual()
        {
            forEachSplit(workspace_.active().root, [](const NodePtr& node) { node->ratio = 0.5; });
            refresh();
        }
        //!

        /*!
            Scroll Pane

            !_method
        */
        void App::scroll(int delta)
        {
            auto pane = workspace_.focusedPane();

            if (!pane) return;

            std::lock_guard<std::mutex> lock(pane->mutex());
            pane->buffer().scrollBy(delta);

            refresh();
        }
        //!

        /*!
            Scroll To Edge

            !_method
        */
        void App::scrollToEdge(bool top)
        {
            auto pane = workspace_.focusedPane();

            if (!pane) return;

            std::lock_guard<std::mutex> lock(pane->mutex());
            if (top) pane->buffer().scrollToTop();
            else pane->buffer().scrollToBottom();

            refresh();
        }
        //!

        /*!
            Toggle Zoom

            !_method
        */
        void App::toggleZoom()
        {
            zoom_ = !zoom_;
            refresh();
        }
        //!

        /*!
            Deprecated: Toggle Resize Mode

            !_method
        */
        void App::toggleResizeMode()
        {
            resize_mode_ = !resize_mode_;
            refresh();
        }
        //!

        /*!
            Toggle the Clock

            !_method
        */
        void App::toggleClock()
        {
            if (clock_active_)
            {
                closeClock();
                return;
            }

            if (auto pane = workspace_.focusedPane())
            {
                clock_pane_id_ = pane->id();
                clock_active_.store(true, std::memory_order_relaxed);
                refresh();
            }
        }
        //!

        /*!
            Close the Clock

            !_method
        */
        void App::closeClock()
        {
            clock_active_.store(false, std::memory_order_relaxed);
            clock_pane_id_ = -1;
            refresh();
        }
        //!

        /*!
            Split Window

            !_method
        */
        void App::splitWindow(bool vertical)
        {
            auto pane = workspace_.focusedPane();
            int columns = 80;
            int rows = 24;

            if (pane)
            {
                std::lock_guard<std::mutex> lock(pane->mutex());
                columns = std::max(1, pane->buffer().columns());
                rows = std::max(1, pane->buffer().rows());
            }

            workspace_.splitWindow(vertical, columns, rows);
            zoom_ = false;

            refresh();
        }
        //!

        /*!
            Move Navigation

            !_method
        */
        void App::moveNavigation(int dx, int dy)
        {
            auto pane = workspace_.focusedPane();
            
            if (!pane) return;

            std::lock_guard<std::mutex> lock(pane->mutex());
            pane->buffer().moveNavigation(dx, dy);

            refresh();
        }
        //!

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

        /*!
            Move Navigation Edge

            !_method
        */
        void App::moveNavigationEdge(bool top, bool line)
        {
            auto pane = workspace_.focusedPane();
            if (!pane) return;

            std::lock_guard<std::mutex> lock(pane->mutex());
            Buffer& buffer = pane->buffer();
            const int start = buffer.visibleStart();
            const int end = std::min(static_cast<int>(buffer.lines().size()) - 1,
            start + buffer.rows() - 1);
            Pos position = buffer.navigation();

            if (line)
            {
                position.x = top ? 0 : buffer.columns() - 1;
            }
            else
            {
                position.y = top ? start : end;
            }

            buffer.setNavigation(position);
            refresh();
        }
        //!

        /*!
            Select Here

            !_method
        */
        void App::selectHere()
        {
            auto pane = workspace_.focusedPane();
            if (!pane) return;

            std::lock_guard<std::mutex> lock(pane->mutex());
            pane->buffer().setAnchor(pane->buffer().navigation());

            refresh();
        }
        //!

        /*!
            Send Keys

            !_method
        */
        void App::sendKeys(const std::string& data)
        {
            if (auto pane = workspace_.focusedPane()) pane->write(unescapeKeys(data));
        }
        //!
    //!

    /*!
        Focus Handling
    */
        /*!
            Directional Pane Focus

            !_method
        */
        void App::focusDirection(int dx, int dy)
        {
            auto pane = workspace_.focusedPane();

            if (!pane) return;

            const auto currentRect = renderer_.rectFor(pane->id());

            if (!currentRect)
            {
                workspace_.focusNextPane();
                refresh();
                return;
            }

            const int cx = currentRect->x + currentRect->width / 2;
            const int cy = currentRect->y + currentRect->height / 2;
            int bestDistance = std::numeric_limits<int>::max();
            int bestId = -1;

            for (const auto& entry : renderer_.paneRects())
            {
                if (!entry.pane || entry.pane->id() == pane->id()) continue;
                const int x = entry.inner.x + entry.inner.width / 2;
                const int y = entry.inner.y + entry.inner.height / 2;
                if (dx > 0 && x <= cx) continue;
                if (dx < 0 && x >= cx) continue;
                if (dy > 0 && y <= cy) continue;
                if (dy < 0 && y >= cy) continue;
                const int distance = std::abs(x - cx) + std::abs(y - cy);

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestId = entry.pane->id();
                }
            }
            if (bestId >= 0) workspace_.setFocus(bestId);

            refresh();
        }
        //!

        /*!
            Focus Next Pane

            !_method
        */
        void App::focusNext()
        {
            workspace_.focusNextPane();
            refresh();
        }
        //!

        /*!
            Focus Previous Pane

            !_method
        */
        void App::focusPrevious()
        {
            workspace_.focusPreviousPane();
            refresh();
        }
        //!
    //!

    /*!
        Selection Handling
    */
        /*!
            Start Selection

            !_method
        */
        void App::startSelection()
        {
            if (mode_ != Mode::Visual || selecting_) return;

            if (auto pane = workspace_.focusedPane())
            {
                std::lock_guard<std::mutex> lock(pane->mutex());
                pane->buffer().setAnchor(pane->buffer().navigation());
            }

            selecting_ = true;
            refresh();
        }
        //!

        /*!
            Yank

            !_method
        */
        void App::yank(bool wholeLine) {
            auto pane = workspace_.focusedPane();
            if (!pane) return;

            std::string text;
            {
                std::lock_guard<std::mutex> lock(pane->mutex());
                Buffer& buffer = pane->buffer();

                if (!wholeLine) text = buffer.selectedText();

                if (text.empty() && buffer.navigation().y >= 0 && buffer.navigation().y < static_cast<int>(buffer.lines().size()))
                {
                    for (const auto& cell : buffer.lines()[static_cast<std::size_t>(buffer.navigation().y)])
                    {
                        text += cell.text;
                    }
                }
                buffer.clearSelection();
            }

            while (!text.empty() && text.back() == '\n') text.pop_back();

            clipboard_ = text;
            emitClipboard(clipboard_);
            selecting_ = false;
            setMode(Mode::Normal);
            statusMessage("yanked " + std::to_string(clipboard_.size()) + " bytes");
        }
        //!

        /*!
            Paste

            !_method
        */
        void App::paste()
        {
            if (clipboard_.empty())
            {
                statusMessage("clipboard empty");
                return;
            }

            sendKeys("\x1B[200~" + clipboard_ + "\x1B[201~");
            statusMessage("pasted " + std::to_string(clipboard_.size()) + " bytes");
        }
        //!

        /*!
            Emit Clipboard

            !_method
        */
        void App::emitClipboard(const std::string& text)
        {
            const bool inTmux = std::getenv("TMUX") != nullptr;
            if (inTmux) std::cout << "\x1BPtmux;\x1B";

            std::cout << "\x1B]52;c;" << base64Encode(text) << "\a";

            if (inTmux) std::cout << "\x1B\\";

            std::cout << std::flush;
            writeSystemClipboard(text);
        }
        //!
    //!

    /*!
        Help Dialog
    */
        /*!
            Toggle Help

            !_method
        */
        void App::toggleHelp()
        {
            if (overlay_ == Overlay::Help)
            {
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
        //!

        /*!
            Close the Overlay

            !_method
        */
        void App::closeOverlay()
        {
            overlay_ = Overlay::None;

            clearPending();
            refresh();
        }
        //!

        /*!
            Scroll Help Dialog

            !_method
        */
        void App::helpScroll(int delta)
        {
            const auto bindings = filteredBindings();
            const int boxHeight = std::min(30, std::max(8, ftxui::Terminal::Size().dimy - 4));
            const int perPage = std::max(1, (boxHeight - 3) * 2);
            const int pages = std::max(0, (static_cast<int>(bindings.size()) + perPage - 1) / perPage - 1);
            const int next = static_cast<int>(help_offset_) + delta;
            help_offset_ = static_cast<std::size_t>(std::clamp(next, 0, pages));

            refresh();
        }
        //!

        /*!
            Help Filter: Type Character

            !_method
        */
        void App::appendHelpFilter(char character)
        {
            help_filtering_ = true;
            help_filter_ += character;
            help_offset_ = 0;
            refresh();
        }
        //!

        /*!
            Help Filter: Remove Character

            !_method
        */
        void App::backspaceHelpFilter()
        {
            if (help_filtering_ && !help_filter_.empty())
            {
                help_filter_.pop_back();
                help_offset_ = 0;
                refresh();
            }
        }
        //!

        /*!
            Filtered Bindings

            !_method
        */
        std::vector<std::pair<std::string, std::string>> App::filteredBindings() const
        {
            const auto bindings = config_.listBindings(mode_);

            if (!help_filtering_ || help_filter_.empty()) return bindings;

            std::vector<std::pair<std::string, std::string>> result;

            for (const auto& binding : bindings)
            {
                if (containsInsensitive(binding.first + " " + binding.second, help_filter_))
                {
                    result.push_back(binding);
                }
            }
            return result;
        }
        //!
    //!

    /*!
        Tabs Handling
    */
        /*!
            Toggle Tabs

            !_method
        */
        void App::toggleTabs()
        {
            overlay_ = overlay_ == Overlay::Tabs ? Overlay::None : Overlay::Tabs;
            clearPending();
            refresh();
        }
        //!

        /*!
            Renaming Tabs
        */
            /*!
                Show the Rename Dialog

                !_method
            */
            void App::startRenameTab()
            {
                overlay_ = Overlay::Rename;
                rename_buffer_ = workspace_.active().name;
                clearPending();
                refresh();
            }
            //!

            /*!
                Rename: Add Character

                !_method
            */
            void App::appendRenameChar(char character)
            {
                rename_buffer_ += character;
                refresh();
            }
            //!

            /*!
                Rename: Remove Character

                !_method
            */
            void App::backspaceRenameChar()
            {
                if (!rename_buffer_.empty()) rename_buffer_.pop_back();
                refresh();
            }
            //!

            /*!
                Apply the Rename

                !_method
            */
            void App::renameTabApply()
            {
                if (rename_buffer_.empty()) rename_buffer_ = "shell";

                workspace_.setTabName(rename_buffer_);
                overlay_ = Overlay::None;
                rename_buffer_.clear();

                refresh();
            }
            //!

            /*!
                Cancel Rename

                !_method
            */
            void App::renameTabCancel()
            {
                overlay_ = Overlay::None;
                rename_buffer_.clear();
                refresh();
            }
            //!
        //!

        /*!
            Create a New Tab

            !_method
        */
        void App::newTab()
        {
            workspace_.newTab(80, 24);
            zoom_ = false;
            refresh();
        }
        //!

        /*!
            Close Current Tab

            !_method
        */
        void App::closeTab()
        {
            workspace_.closeTab();
            zoom_ = false;
            refresh();
        }
        //!

        /*!
            Select Tab By Number

            !_method
        */
        void App::selectTab(int oneBased)
        {
            workspace_.selectTab(oneBased);
            zoom_ = false;
            refresh();
        }
        //!

        /*!
            Select Next Tab

            !_method
        */
        void App::nextTab()
        {
            workspace_.selectTab(workspace_.activeIndex() + 2 > workspace_.tabCount() ? 1 : workspace_.activeIndex() + 2);
            refresh();
        }
        //!

        /*!
            Select Previous Tab

            !_method
        */
        void App::previousTab()
        {
            workspace_.selectTab(workspace_.activeIndex() == 0 ? workspace_.tabCount() : workspace_.activeIndex());
            refresh();
        }
        //!
    //!

    /*!
        Theme Handling
    */
        /*!
            Toggle the Themes

            !_method
        */
        void App::toggleThemes()
        {
            if (overlay_ == Overlay::Themes)
            {
                themeCancel();
                return;
            }

            overlay_ = Overlay::Themes;
            theme_index_ = 0;
            clearPending();
            refresh();
        }
        //!

        /*!
            Select Next Theme

            !_method
        */
        void App::themeNext()
        {
            if (config_.themes.empty()) return;

            theme_index_ = (theme_index_ + 1) % config_.themes.size();
            applyTheme(config_.themes[theme_index_]);
        }
        //!

        /*!
            Select Previous Theme

            !_method
        */
        void App::themePrevious()
        {
            if (config_.themes.empty()) return;

            theme_index_ = (theme_index_ + config_.themes.size() - 1) % config_.themes.size();
            applyTheme(config_.themes[theme_index_]);
        }
        //!

        /*!
            Select Theme to Apply

            !_method
        */
        void App::themeApply()
        {
            if (theme_index_ < config_.themes.size())
            {
                applyTheme(config_.themes[theme_index_]);
                Config::saveColorSettings(config_.path, config_.settings);
                statusMessage("theme: " + config_.themes[theme_index_].first);
            }

            overlay_ = Overlay::None;
            refresh();
        }
        //!

        /*!
            Apply Theme

            !_method
        */
        void App::applyTheme(const Theme& theme)
        {
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
        //!

        /*!
            Cancel Theme Dialog

            !_method
        */
        void App::themeCancel()
        {
            overlay_ = Overlay::None;
            refresh();
        }
        //!
    //!

}
