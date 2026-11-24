#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/node.hpp"

#include "../config/config.hpp"
#include "../layout/workspace.hpp"
#include "../ui/renderer.hpp"
#include "mode.hpp"

namespace nexyth
{
    /*!
        Overlay Enum Class
    */
    //>>
    enum class Overlay { None, Help, Tabs, Themes, Rename, LayoutSave, Layouts };
    //<<
    //!

    /*!
        App Class
    */
    class App : public ftxui::ComponentBase
    {
        /*!
            Public Members
        */
        public:
            //>>
            App(ftxui::ScreenInteractive* screen, std::string configPath);
            ~App() override;

            ftxui::Element OnRender() override;
            bool OnEvent(ftxui::Event event) override;

            Config& config() { return config_; }
            Workspace& workspace() { return workspace_; }
            Renderer& renderer() { return renderer_; }
            Mode mode() const { return mode_; }
            Overlay overlay() const { return overlay_; }
            bool zoom() const { return zoom_; }
            bool clockActive() const { return clock_active_; }
            int clockPaneId() const { return clock_pane_id_; }
            bool helpVisible() const { return overlay_ == Overlay::Help; }
            std::string pendingKeys() const { return pending_; }
            std::string statusMessage() const;
            std::string helpFilter() const { return help_filter_; }
            bool helpFiltering() const { return help_filtering_; }
            std::size_t helpOffset() const { return help_offset_; }
            bool selecting() const { return selecting_; }
            bool resizeMode() const { return resize_mode_; }
            std::size_t themeIndex() const { return theme_index_; }
            std::string renameBuffer() const { return rename_buffer_; }
            std::string layoutBuffer() const { return layout_buffer_; }
            const std::vector<std::string>& layoutNames() const { return layout_names_; }
            std::size_t layoutIndex() const { return layout_index_; }
            std::vector<std::pair<std::string, std::string>> filteredBindings() const;

            void refresh();
            void statusMessage(std::string message);
            void quit();
            void runCommand(const std::string& name,
            const std::vector<std::string>& args);

            void setMode(Mode mode);
            void enterNormal(bool home = false, bool end = false);
            void enterVisual();
            void startSelection();
            void exitVisual();
            void toggleHelp();
            void closeOverlay();
            void toggleTabs();
            void toggleThemes();
            void themeNext();
            void themePrevious();
            void themeApply();
            void themeCancel();
            void startRenameTab();
            void appendRenameChar(char character);
            void backspaceRenameChar();
            void renameTabApply();
            void renameTabCancel();
            void startLayoutSave();
            void appendLayoutChar(char character);
            void backspaceLayoutChar();
            void layoutSaveApply();
            void layoutSaveCancel();
            void toggleLayoutList();
            void layoutListNext();
            void layoutListPrevious();
            void layoutListApply();
            void layoutListDelete();
            void layoutListCancel();
            void helpScroll(int delta);
            void appendHelpFilter(char character);
            void backspaceHelpFilter();
            void toggleZoom();
            void toggleResizeMode();
            void toggleClock();
            void closeClock();

            void focusDirection(int dx, int dy);
            void focusNext();
            void focusPrevious();
            void splitFocused(bool vertical);
            void splitWindow(bool vertical);
            void closePane();
            void newTab();
            void closeTab();
            void selectTab(int oneBased);
            void nextTab();
            void previousTab();

            void resizeFocused(int dx, int dy, int amount = 0);
            void resizeEqual();
            void scroll(int delta);
            void scrollToEdge(bool top);
            void moveNavigation(int dx, int dy);
            void moveNavigationWord(int direction);
            void moveNavigationEdge(bool top, bool line);
            void selectHere();
            void yank(bool wholeLine);
            void paste();
            void sendKeys(const std::string& data);
            //<<
        //!

        /*!
            Private Members
        */
        private:
            //>>
            void refreshLoop();
            bool handleKey(const ftxui::Event& event);
            bool handleMouse(ftxui::Event event);
            void clearPending();
            void emitClipboard(const std::string& text);
            void applyTheme(const Theme& theme);

            ftxui::ScreenInteractive* screen_ = nullptr;
            Config config_;
            Workspace workspace_;
            Renderer renderer_;
            Mode mode_ = Mode::Normal;
            Overlay overlay_ = Overlay::None;
            bool zoom_ = false;
            bool resize_mode_ = false;
            bool selecting_ = false;
            std::atomic<bool> clock_active_{false};
            int clock_pane_id_ = -1;
            std::string pending_;
            std::string message_;
            std::string help_filter_;
            bool help_filtering_ = false;
            std::string rename_buffer_;
            std::string layout_buffer_;
            std::vector<std::string> layout_names_;
            std::size_t layout_index_ = 0;
            std::string clipboard_;
            std::size_t help_offset_ = 0;
            std::size_t theme_index_ = 0;
            std::chrono::steady_clock::time_point pending_time_;
            std::chrono::steady_clock::time_point message_time_;
            std::chrono::steady_clock::time_point last_escape_time_;
            std::atomic<bool> refresh_pending_{false};
            std::atomic<bool> refresh_running_{true};
            std::thread refresh_thread_;
            mutable std::mutex message_mutex_;
            //<<
        //!
    };
    //!

}
