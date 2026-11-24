#pragma once

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"

#include "../core/mode.hpp"
#include "../layout/node.hpp"
#include "../terminal/buffer.hpp"

namespace nexyth
{
    /*!
        App Class
    */
    //>>
    class App;
    //<<
    //!

    /*!
        Rect Struct

    */
    //>>
    struct Rect
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        bool contains(int px, int py) const
        {
            return px >= x && py >= y && px < x + width && py < y + height;
        }
    };
    //<<
    //!

    /*!
        Pane Rect
    */
    //>>
    struct PaneRect
    {
        std::shared_ptr<Pane> pane;
        Rect border;
        Rect inner;
    };
    //<<
    //!

    /*!
        Renderer Class
    */
    class Renderer
    {
        /*!
            Public Members
        */
        public:
            //>>
            explicit Renderer(App& app);
            void computeLayout(int width, int height);
            void draw(ftxui::Canvas& canvas, int width, int height);

            const std::vector<PaneRect>& paneRects() const { return paneRects_; }
            std::optional<Rect> rectFor(int paneId) const;
            std::optional<Pos> terminalCursorScreenPosition() const;
            //<<
        //!

        /*!
            Private Members
        */
        private:
            //>>
            void buildLayout(const NodePtr& node, Rect area);
            void drawPane(ftxui::Canvas& canvas, const PaneRect& entry,
            ftxui::Color foreground, ftxui::Color background);
            void drawTabs(ftxui::Canvas& canvas, int width, int y,
            ftxui::Color foreground, ftxui::Color background);
            void drawStatus(ftxui::Canvas& canvas, int width, int y,
            ftxui::Color foreground, ftxui::Color background);
            void drawHelp(ftxui::Canvas& canvas, int width, int height,
            ftxui::Color foreground, ftxui::Color background);
            void drawClock(ftxui::Canvas& canvas,
            ftxui::Color foreground, ftxui::Color background);
            void drawTabsOverlay(ftxui::Canvas& canvas, int width, int height,
            ftxui::Color foreground, ftxui::Color background);
            void drawThemesOverlay(ftxui::Canvas& canvas, int width, int height,
            ftxui::Color foreground, ftxui::Color background);
            void drawRenameOverlay(ftxui::Canvas& canvas, int width, int height,
            ftxui::Color foreground, ftxui::Color background);
            void drawLayoutSaveOverlay(ftxui::Canvas& canvas, int width, int height,
            ftxui::Color foreground, ftxui::Color background);
            void drawLayoutListOverlay(ftxui::Canvas& canvas, int width, int height,
            ftxui::Color foreground, ftxui::Color background);
            void drawResizeIndicator(ftxui::Canvas& canvas, int width, int height,
            ftxui::Color foreground, ftxui::Color background);

            App& app_;
            std::vector<PaneRect> paneRects_;
            std::map<int, std::pair<int, int>> last_sizes_;
            std::optional<Pos> cursor_position_;
            //<<
        //!
    };
    //!
}
