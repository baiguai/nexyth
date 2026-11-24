#include "renderer.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

#include "../core/app.hpp"
#include "../utils.hpp"
#include "ftxui/screen/color.hpp"

namespace nexyth
{
    /*>
        Helper Methods
    */
    namespace
    {
        using ftxui::Canvas;

        /*!
            Parse Color

            !_method
        */
        ftxui::Color parseColor(std::string text, ftxui::Color fallback)
        {
            text = trim(text);

            if (text.size() == 7 && text[0] == '#')
            {
                try
                {
                    const int value = std::stoi(text.substr(1), nullptr, 16);
                    return ftxui::Color::RGB((value >> 16) & 0xFF, (value >> 8) & 0xFF,
                    value & 0xFF);
                }
                catch (...)
                {
                    return fallback;
                }
            }

            const std::string name = lower(text);

            if (name == "black") return ftxui::Color::RGB(0, 0, 0);
            if (name == "red") return ftxui::Color::RGB(205, 49, 49);
            if (name == "green") return ftxui::Color::RGB(13, 188, 121);
            if (name == "yellow") return ftxui::Color::RGB(229, 229, 16);
            if (name == "blue") return ftxui::Color::RGB(36, 114, 200);
            if (name == "magenta") return ftxui::Color::RGB(188, 63, 188);
            if (name == "cyan") return ftxui::Color::RGB(17, 168, 205);
            if (name == "white") return ftxui::Color::RGB(229, 229, 229);

            return fallback;
        }
        //!

        /*!
            Color From

            !_method
        */
        ftxui::Color colorFrom(const TermColor& color, ftxui::Color fallback)
        {
            if (!color.set) return fallback;
            if (color.rgb) return ftxui::Color::RGB(color.r, color.g, color.b);

            return ftxui::Color(static_cast<ftxui::Color::Palette256>(color.index));
        }
        //!

        /*!
            Write Cell

            !_method
        */
        void writeCell(ftxui::Canvas& canvas, int x, int y, const std::string& text, const Canvas::Stylizer& stylizer)
        {
            canvas.DrawText(x * 2, y * 4, text, stylizer);
        }
        //!

        /*!
            Write Text

            !_method
        */
        void writeText(ftxui::Canvas& canvas, int x, int y, const std::string& text, const Canvas::Stylizer& stylizer)
        {
            for (char c : text)
            {
                if (x >= 0 && y >= 0 && x < canvas.width() && y < canvas.height())
                {
                    writeCell(canvas, x, y, std::string(1, c), stylizer);
                }
                ++x;
            }
        }
        //!

        /*!
            Fill Rect

            !_method
        */
        void fillRect(Canvas& canvas, Rect area, const std::string& text, const Canvas::Stylizer& stylizer)
        {
            for (int y = area.y; y < area.y + area.height; ++y)
            {
                for (int x = area.x; x < area.x + area.width; ++x)
                {
                    if (x >= 0 && y >= 0 && x < canvas.width() && y < canvas.height())
                    {
                        writeCell(canvas, x, y, text, stylizer);
                    }
                }
            }
        }
        //!

        /*!
            Draw Box

            !_method
        */
        void drawBox(Canvas& canvas, Rect box, const Canvas::Stylizer& borderStyle)
        {
            if (box.width < 2 || box.height < 2) return;

            writeCell(canvas, box.x, box.y, "┌", borderStyle);
            writeCell(canvas, box.x + box.width - 1, box.y, "┐", borderStyle);
            writeCell(canvas, box.x, box.y + box.height - 1, "└", borderStyle);
            writeCell(canvas, box.x + box.width - 1, box.y + box.height - 1, "┘", borderStyle);

            for (int x = box.x + 1; x < box.x + box.width - 1; ++x)
            {
                writeCell(canvas, x, box.y, "─", borderStyle);
                writeCell(canvas, x, box.y + box.height - 1, "─", borderStyle);
            }

            for (int y = box.y + 1; y < box.y + box.height - 1; ++y)
            {
                writeCell(canvas, box.x, y, "│", borderStyle);
                writeCell(canvas, box.x + box.width - 1, y, "│", borderStyle);
            }
        }
        //!

        /*!
            Centered Box

            !_method
        */
        Rect centeredBox(int width, int height, int preferredWidth, int preferredHeight)
        {
            const int boxWidth = std::min(preferredWidth, std::max(20, width - 6));
            const int boxHeight = std::min(preferredHeight, std::max(8, height - 4));
            return {(width - boxWidth) / 2, (height - boxHeight) / 2, boxWidth, boxHeight};
        }
        //!

        /*!
            Clock Digits Array
        */
        //>>
        const char* const kClockDigits[10][5] =
        {
            {"###", "# #", "# #", "# #", "###"},
            {"## ", " # ", " # ", " # ", "###"},
            {"###", "  #", "###", "#  ", "###"},
            {"###", "  #", "###", "  #", "###"},
            {"# #", "# #", "###", "  #", "  #"},
            {"###", "#  ", "###", "  #", "###"},
            {"###", "#  ", "###", "# #", "###"},
            {"###", "  #", "  #", "  #", "  #"},
            {"###", "# #", "###", "# #", "###"},
            {"###", "# #", "###", "  #", "###"}
        };
        //<<
        //!

        /*!
            Clock Colon Array
        */
        //>>
        const char* const kClockColon[5] = {"   ", " # ", "   ", " # ", "   "};
        //<<
        //!

        /*!
            Draw the Clock

            !_method
        */
        void drawClockGlyph(Canvas& canvas, int x, int y, const char* const glyph[5], const Canvas::Stylizer& stylizer)
        {
            for (int row = 0; row < 5; ++row)
            {
                for (int column = 0; column < 3; ++column)
                {
                    if (glyph[row][column] != '#') continue;
                    writeCell(canvas, x + column, y + row, "█", stylizer);
                }
            }
        }
        //!
    }
    //!

    /*!
        Renderer Constructor

        !_ctor
    */
    //>>
    Renderer::Renderer(App& app) : app_(app) {}
    //<<
    //!

    /*!
        Compute Layout

        !_method
    */
    void Renderer::computeLayout(int width, int height)
    {
        const Settings& settings = app_.config().settings;

        paneRects_.clear();
        cursor_position_.reset();

        const int top = settings.tabbar ? 1 : 0;
        const int bottom = settings.statusline ? 1 : 0;
        const Rect content{0, top, width, std::max(0, height - top - bottom)};
        auto pane = app_.workspace().focusedPane();

        if (pane)
        {
            if (app_.zoom())
            {
                PaneRect entry;
                entry.pane = pane;
                entry.border = content;
                entry.inner = {content.x + 1, content.y + 1,
                std::max(0, content.width - 2),
                std::max(0, content.height - 2)};
                paneRects_.push_back(entry);
            }
            else
            {
                buildLayout(app_.workspace().active().root, content);
            }
        }

        for (const auto& entry : paneRects_)
        {
            if (!entry.pane) continue;

            const int columns = std::max(1, entry.inner.width);
            const int rows = std::max(1, entry.inner.height);
            const auto previous = last_sizes_.find(entry.pane->id());
            
            if (previous == last_sizes_.end() || previous->second.first != columns || previous->second.second != rows)
            {
                entry.pane->resize(columns, rows);
                last_sizes_[entry.pane->id()] = {columns, rows};
            }
        }

        if (pane && app_.mode() == Mode::Insert)
        {
            std::lock_guard<std::mutex> lock(pane->mutex());
            const Buffer& buffer = pane->buffer();
            const int start = buffer.visibleStart();
            const Pos cursor = buffer.cursor();

            for (const auto& entry : paneRects_)
            {
                if (!entry.pane || entry.pane->id() != pane->id()) continue;

                const int x = entry.inner.x + cursor.x;
                const int y = entry.inner.y + cursor.y - start;

                if (x >= 0 && x < width && y >= 0 && y < height && cursor.y >= start && cursor.y < start + buffer.rows())
                {
                    cursor_position_ = Pos{x, y};
                }
            }
        }
    }
    //!

    /*!
        Build Layout

        !_method
    */
    void Renderer::buildLayout(const NodePtr& node, Rect area)
    {
        if (!node || area.width <= 0 || area.height <= 0) return;

        if (node->type == Node::Type::Leaf)
        {
            if (!node->pane) return;

            PaneRect entry;
            entry.pane = node->pane;
            entry.border = area;
            entry.inner = {area.x + 1, area.y + 1,
            std::max(0, area.width - 2),
            std::max(0, area.height - 2)};
            paneRects_.push_back(entry);

            return;
        }

        const bool sideBySide = node->kind == SplitKind::SideBySide;
        const int total = sideBySide ? area.width : area.height;
        int firstSize = static_cast<int>(total * std::clamp(node->ratio, 0.05, 0.95));
        firstSize = std::clamp(firstSize, 1, std::max(1, total - 1));

        if (sideBySide)
        {
            buildLayout(node->first, {area.x, area.y, firstSize, area.height});
            buildLayout(node->second,
            {area.x + firstSize, area.y, area.width - firstSize,
            area.height});
        }
        else
        {
            buildLayout(node->first, {area.x, area.y, area.width, firstSize});
            buildLayout(node->second,
            {area.x, area.y + firstSize, area.width,
            area.height - firstSize});
        }
    }
    //!

    /*!
        Renderer Draw

        !_method
    */
    void Renderer::draw(ftxui::Canvas& canvas, int width, int height)
    {
        const Settings& settings = app_.config().settings;
        const ftxui::Color foreground = parseColor(settings.foreground, ftxui::Color::Default);
        const ftxui::Color background = parseColor(settings.background, ftxui::Color::Default);

        const auto plain = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = foreground;
            pixel.background_color = background;
            pixel.bold = false;
            pixel.dim = false;
            pixel.underlined = false;
            pixel.strikethrough = false;
        };

        fillRect(canvas, {0, 0, width, height}, " ", plain);

        const ftxui::Color border = parseColor(settings.border, foreground);
        const ftxui::Color focusBorder =
        parseColor(settings.focused_border, foreground);
        const auto focusedPane = app_.workspace().focusedPane();

        for (const auto& entry : paneRects_)
        {
            drawPane(canvas, entry, foreground, background);
        }

        for (const auto& entry : paneRects_)
        {
            const bool focused = focusedPane && entry.pane && entry.pane->id() == focusedPane->id();
            const ftxui::Color edge = focused ? focusBorder : border;

            const auto edgeStyle = [edge, background, focused](ftxui::Cell& pixel)
            {
                pixel.foreground_color = edge;
                pixel.background_color = background;
                pixel.bold = focused;
            };

            drawBox(canvas, entry.border, edgeStyle);

            if (focused && entry.border.width > 12)
            {
                const auto& tabName = app_.workspace().active().name;
                std::string title = " " + (tabName.empty() ? entry.pane->title() : tabName) + " ";

                if (static_cast<int>(title.size()) > entry.border.width - 3)
                {
                    title = title.substr(0, static_cast<std::size_t>(entry.border.width - 3));
                }

                writeText(canvas, entry.border.x + 2, entry.border.y, title, edgeStyle);
            }
        }

        if (app_.clockActive())
        {
            drawClock(canvas, foreground, background);
        }

        if (settings.tabbar) drawTabs(canvas, width, 0, foreground, background);

        if (settings.statusline)
        {
            drawStatus(canvas, width, height - 1, foreground, background);
        }

        if (app_.resizeMode())
        {
            drawResizeIndicator(canvas, width, height, foreground, background);
        }

        if (app_.overlay() == Overlay::Help)
        {
            drawHelp(canvas, width, height, foreground, background);
        }
        else if (app_.overlay() == Overlay::Tabs)
        {
            drawTabsOverlay(canvas, width, height, foreground, background);
        }
        else if (app_.overlay() == Overlay::Themes)
        {
            drawThemesOverlay(canvas, width, height, foreground, background);
        }
        else if (app_.overlay() == Overlay::Rename)
        {
            drawRenameOverlay(canvas, width, height, foreground, background);
        }
        else if (app_.overlay() == Overlay::LayoutSave)
        {
            drawLayoutSaveOverlay(canvas, width, height, foreground, background);
        }
        else if (app_.overlay() == Overlay::Layouts)
        {
            drawLayoutListOverlay(canvas, width, height, foreground, background);
        }
    }
    //!

    /*!
        Draw Pane

        !_method
    */
    void Renderer::drawPane(ftxui::Canvas& canvas, const PaneRect& entry, ftxui::Color foreground, ftxui::Color background)
    {
        auto pane = entry.pane;
        if (!pane || entry.inner.width <= 0 || entry.inner.height <= 0) return;

        std::lock_guard<std::mutex> lock(pane->mutex());
        Buffer& buffer = pane->buffer();
        const int start = buffer.visibleStart();
        const auto anchor = buffer.anchor();
        const auto head = buffer.head();
        const bool hasSelection = anchor && head;
        Pos selectionStart = hasSelection ? *anchor : Pos{};
        Pos selectionEnd = hasSelection ? *head : Pos{};

        if (hasSelection && (selectionEnd.y < selectionStart.y || (selectionEnd.y == selectionStart.y && selectionEnd.x < selectionStart.x)))
        {
            std::swap(selectionStart, selectionEnd);
        }

        const auto focusedPane = app_.workspace().focusedPane();
        const bool focused = focusedPane && pane && focusedPane->id() == pane->id();
        const ftxui::Color selectionColor = parseColor(app_.config().settings.selection_background, ftxui::Color::RGB(38, 79, 120));

        for (int row = 0; row < entry.inner.height; ++row)
        {
            const int bufferY = start + row;
            const bool lineExists =
            bufferY >= 0 && bufferY < static_cast<int>(buffer.lines().size());

            for (int column = 0; column < entry.inner.width; ++column)
            {
                const int x = entry.inner.x + column;
                const int y = entry.inner.y + row;
                Cell cell;

                if (lineExists && column < static_cast<int>(buffer.lines()[static_cast<std::size_t>(bufferY)].size()))
                {
                    cell = buffer.lines()[static_cast<std::size_t>(bufferY)]
                    [static_cast<std::size_t>(column)];
                }

                bool inverted = false;

                if (hasSelection && bufferY >= selectionStart.y && bufferY <= selectionEnd.y)
                {
                    const int from = bufferY == selectionStart.y ? selectionStart.x : 0;
                    const int to = bufferY == selectionEnd.y ? selectionEnd.x + 1 : buffer.columns();
                    if (column >= from && column < to) inverted = true;
                }

                if (focused && app_.mode() == Mode::Visual && buffer.navigation().x == column && buffer.navigation().y == bufferY)
                {
                    inverted = true;
                }

                ftxui::Color fg = colorFrom(cell.style.foreground, foreground);
                ftxui::Color bg = colorFrom(cell.style.background, background);

                if (inverted)
                {
                    fg = background;
                    bg = selectionColor;
                }

                const auto stylizer = [fg, bg, &cell](ftxui::Cell& pixel)
                {
                    pixel.foreground_color = fg;
                    pixel.background_color = bg;
                    pixel.bold = cell.style.bold;
                    pixel.dim = cell.style.dim;
                    pixel.underlined = cell.style.underlined;
                    pixel.strikethrough = cell.style.strikethrough;
                    pixel.inverted = false;
                };

                const std::string text =
                cell.width == 0 || cell.text.empty() ? " " : cell.text;
                writeCell(canvas, x, y, text, stylizer);
            }
        }
    }
    //!

    /*!
        Draw Tabs

        !_method
    */
    void Renderer::drawTabs(ftxui::Canvas& canvas, int width, int y, ftxui::Color foreground, ftxui::Color background)
    {
        const auto& tabs = app_.workspace().tabs();
        const int active = app_.workspace().activeIndex();
        int x = 0;

        for (std::size_t i = 0; i < tabs.size() && x < width; ++i)
        {
            const std::string label = app_.workspace().tabLabel(static_cast<int>(i));
            if (static_cast<int>(label.size()) + 2 > width - x) break;

            const bool selected = static_cast<int>(i) == active;
            const ftxui::Color fg = selected ? background : foreground;
            const ftxui::Color bg = selected ? foreground : background;

            const auto style = [fg, bg, selected](ftxui::Cell& pixel)
            {
                pixel.foreground_color = fg;
                pixel.background_color = bg;
                pixel.bold = selected;
            };

            const std::string text = " " + label + " ";
            writeText(canvas, x, y, text, style);
            x += static_cast<int>(text.size());
        }

        const auto fillStyle = [background](ftxui::Cell& pixel)
        {
            pixel.background_color = background;
            pixel.foreground_color = background;
        };

        while (x < width)
        {
            writeCell(canvas, x, y, " ", fillStyle);
            ++x;
        }
    }
    //!

    /*!
        Draw Status

        !_method
    */
    void Renderer::drawStatus(ftxui::Canvas& canvas, int width, int y, ftxui::Color foreground, ftxui::Color background)
    {
        const Settings& settings = app_.config().settings;
        const ftxui::Color statusFg = parseColor(settings.status_foreground, foreground);
        const ftxui::Color statusBg = parseColor(settings.status_background, background);

        const auto statusStyle = [statusFg, statusBg](ftxui::Cell& pixel)
        {
            pixel.foreground_color = statusFg;
            pixel.background_color = statusBg;
            pixel.bold = false;
            pixel.dim = false;
            pixel.underlined = false;
            pixel.strikethrough = false;
        };

        for (int x = 0; x < width; ++x) writeCell(canvas, x, y, " ", statusStyle);

        const ftxui::Color modeFg = background;
        const ftxui::Color modeBg =
        app_.mode() == Mode::Insert ? ftxui::Color::RGB(70, 150, 80) : (app_.mode() == Mode::Visual ? ftxui::Color::RGB(190, 140, 60) : ftxui::Color::RGB(70, 110, 180));

        const auto modeStyle = [modeFg, modeBg](ftxui::Cell& pixel)
        {
            pixel.foreground_color = modeFg;
            pixel.background_color = modeBg;
            pixel.bold = true;
        };

        const std::string mode = " " + std::string(modeName(app_.mode())) + (app_.selecting() ? " SELECT " : " ");
        writeText(canvas, 0, y, mode, modeStyle);

        std::ostringstream text;
        text << " tab " << (app_.workspace().activeIndex() + 1) << "/" << app_.workspace().tabs().size();

        if (auto pane = app_.workspace().focusedPane())
        {
            const auto& tabName = app_.workspace().active().name;
            text << "  " << (tabName.empty() ? pane->title() : tabName);
        }

        if (!app_.pendingKeys().empty()) text << "  pending: " << app_.pendingKeys();
        if (app_.resizeMode()) text << "  RESIZE";
        const std::string message = app_.statusMessage();
        if (!message.empty()) text << "  " << message;
        text << "   ? help";
        writeText(canvas, static_cast<int>(mode.size()), y, text.str(), statusStyle);
    }
    //!

    /*!
        Draw Help

        !_method
    */
    void Renderer::drawHelp(ftxui::Canvas& canvas, int width, int height, ftxui::Color foreground, ftxui::Color background)
    {
        const ftxui::Color boxBg = parseColor("#202020", background);
        const ftxui::Color boxFg = foreground;

        const auto fill = [boxFg, boxBg](ftxui::Cell& pixel)
        {
            pixel.foreground_color = boxFg;
            pixel.background_color = boxBg;
        };

        const auto heading = [boxBg, boxFg](ftxui::Cell& pixel)
        {
            pixel.foreground_color = boxBg;
            pixel.background_color = boxFg;
            pixel.bold = true;
        };

        const Rect box = centeredBox(width, height, 96, 30);
        fillRect(canvas, box, " ", fill);
        drawBox(canvas, box, fill);

        std::string header = " nexyth keys   (Escape close)";
        if (app_.helpFiltering()) header += "  filter: " + app_.helpFilter();
        writeText(canvas, box.x + 1, box.y, header, heading);

        const auto bindings = app_.filteredBindings();
        const int rows = std::max(0, box.height - 3);
        const int columnWidth = box.width / 2;
        const std::size_t offset = app_.helpOffset() * static_cast<std::size_t>(rows) * 2;

        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < 2; ++column)
            {
                const std::size_t index =
                offset + static_cast<std::size_t>(column) * static_cast<std::size_t>(rows)
                + static_cast<std::size_t>(row);
                if (index >= bindings.size()) break;
                const std::string text = bindings[index].first + "  " + bindings[index].second;
                const int x = box.x + 1 + column * columnWidth;
                const int y = box.y + 2 + row;
                writeText(canvas, x, y,
                text.substr(0, static_cast<std::size_t>(columnWidth - 1)), fill);
            }
        }
    }
    //!

    /*!
        Draw Clock

        !_method
    */
    void Renderer::drawClock(ftxui::Canvas& canvas, ftxui::Color foreground, ftxui::Color background)
    {
        const auto rect = rectFor(app_.clockPaneId());
        if (!rect || rect->width < 10 || rect->height < 5) return;

        const auto style = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = foreground;
            pixel.background_color = background;
            pixel.bold = true;
        };

        fillRect(canvas, *rect, " ", style);

        const std::time_t now = std::time(nullptr);
        const std::tm* local = std::localtime(&now);
        if (local == nullptr) return;

        char timeBuffer[16] = {0};
        char dateBuffer[48] = {0};
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", local);
        std::strftime(dateBuffer, sizeof(dateBuffer), "%a %Y-%m-%d", local);

        if (rect->width >= 24 && rect->height >= 9)
        {
            const int clockWidth = 19;
            const int x = rect->x + std::max(0, (rect->width - clockWidth) / 2);
            const int y = rect->y + std::max(0, (rect->height - 8) / 2);
            int cursorX = x;

            for (int index : {0, 1})
            {
                drawClockGlyph(canvas, cursorX, y,
                kClockDigits[timeBuffer[index] - '0'], style);
                cursorX += 4;
            }

            drawClockGlyph(canvas, cursorX, y, kClockColon, style);
            cursorX += 4;

            for (int index : {3, 4})
            {
                drawClockGlyph(canvas, cursorX, y,
                kClockDigits[timeBuffer[index] - '0'], style);
                cursorX += 4;
            }

            const std::string date = dateBuffer;
            const int dateX = rect->x + std::max(0, (rect->width - static_cast<int>(date.size())) / 2);
            writeText(canvas, dateX, y + 6, date, style);
            // writeText(canvas, rect->x + 1, rect->y + rect->height - 1, "q close",
            //          style);
        }
        else
        {
            const std::string text = timeBuffer;
            const int x = rect->x + std::max(0, (rect->width - static_cast<int>(text.size())) / 2);
            const int y = rect->y + rect->height / 2;
            writeText(canvas, x, y, text, style);
            writeText(canvas, rect->x + 1, rect->y + rect->height - 1, "q close", style);
        }
    }
    //!

    /*!
        Draw Tabs Overlay

        !_method
    */
    void Renderer::drawTabsOverlay(ftxui::Canvas& canvas, int width, int height, ftxui::Color foreground, ftxui::Color background)
    {
        const auto fill = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = foreground;
            pixel.background_color = background;
        };

        const Rect box = centeredBox(width, height, 60, 12);
        fillRect(canvas, box, " ", fill);
        drawBox(canvas, box, fill);
        writeText(canvas, box.x + 1, box.y, " nexyth tabs (Esc close)", fill);

        const auto& tabs = app_.workspace().tabs();

        for (std::size_t i = 0; i < tabs.size() && i + 2 < static_cast<std::size_t>(box.height); ++i)
        {
            writeText(canvas, box.x + 1, box.y + 2 + static_cast<int>(i), std::to_string(i + 1) + "  " + app_.workspace().tabLabel(static_cast<int>(i)), fill);
        }
    }
    //!

    /*!
        Draw Themes Overlay

        !_method
    */
    void Renderer::drawThemesOverlay(ftxui::Canvas& canvas, int width, int height, ftxui::Color foreground, ftxui::Color background)
    {
        const auto fill = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = foreground;
            pixel.background_color = background;
        };

        const auto selected = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = background;
            pixel.background_color = foreground;
            pixel.bold = true;
        };

        const Rect box = centeredBox(width, height, 60, 18);
        fillRect(canvas, box, " ", fill);
        drawBox(canvas, box, fill);
        writeText(canvas, box.x + 1, box.y, " nexyth themes (j/k, Enter save, Esc cancel)", fill);
        const Canvas::Stylizer normalStyle = fill;
        const Canvas::Stylizer selectedStyle = selected;
        const auto& themes = app_.config().themes;
        const std::size_t index = app_.themeIndex();
        const std::size_t start = index > 6 ? index - 6 : 0;

        for (std::size_t i = start; i < themes.size() && i - start + 2 < static_cast<std::size_t>(box.height); ++i)
        {
            const std::string prefix = i == index ? "> " : "  ";
            writeText(canvas, box.x + 1, box.y + 2 + static_cast<int>(i - start), prefix + themes[i].first, i == index ? selectedStyle : normalStyle);
        }
    }
    //!

    /*!
        Draw Rename Overlay

        !_method
    */
    void Renderer::drawRenameOverlay(ftxui::Canvas& canvas, int width, int height, ftxui::Color foreground, ftxui::Color background)
    {
        const auto fill = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = foreground;
            pixel.background_color = background;
        };

        const auto heading = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = background;
            pixel.background_color = foreground;
            pixel.bold = true;
        };

        const Rect box = centeredBox(width, height, 64, 8);
        fillRect(canvas, box, " ", fill);
        drawBox(canvas, box, fill);
        writeText(canvas, box.x + 1, box.y, " nexyth rename tab (Enter save, Esc cancel)", heading);

        const std::string value = "> " + app_.renameBuffer();
        writeText(canvas, box.x + 1, box.y + 2, value.substr(0, static_cast<std::size_t>(box.width - 2)), fill);
        const std::string current = " current: " + app_.workspace().tabLabel(app_.workspace().activeIndex());
        writeText(canvas, box.x + 1, box.y + 3, current.substr(0, static_cast<std::size_t>(box.width - 2)), fill);
        writeText(canvas, box.x + 1, box.y + box.height - 2, " type a new name; Backspace edits", fill);
    }
    //!

    /*!
        Draw Layout Save Overlay

        Shows existing layout names and warns when the typed name would
        overwrite one.

        !_method
    */
    void Renderer::drawLayoutSaveOverlay(ftxui::Canvas& canvas, int width, int height, ftxui::Color foreground, ftxui::Color background)
    {
        const auto fill = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = foreground;
            pixel.background_color = background;
        };

        const auto heading = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = background;
            pixel.background_color = foreground;
            pixel.bold = true;
        };

        const Rect box = centeredBox(width, height, 64, 18);
        fillRect(canvas, box, " ", fill);
        drawBox(canvas, box, fill);
        writeText(canvas, box.x + 1, box.y, " nexyth save layout (Enter save, Esc cancel)", heading);

        const auto& names = app_.layoutNames();
        const std::string typed = trim(app_.layoutBuffer());
        const auto match = std::find(names.begin(), names.end(), typed);
        const bool overwrite = !typed.empty() && match != names.end();
        const std::size_t matchIndex = overwrite ? static_cast<std::size_t>(std::distance(names.begin(), match)) : names.size();

        const std::string value = "> " + app_.layoutBuffer();
        writeText(canvas, box.x + 1, box.y + 2, value.substr(0, static_cast<std::size_t>(box.width - 2)), fill);

        const std::string status = overwrite ? "! overwrites existing layout: " + typed
        : names.empty() ? "  no saved layouts yet" : "  new layout";
        const Canvas::Stylizer normalStyle = fill;
        const Canvas::Stylizer warnStyle = heading;
        writeText(canvas, box.x + 1, box.y + 3, status.substr(0, static_cast<std::size_t>(box.width - 2)), overwrite ? warnStyle : normalStyle);
        writeText(canvas, box.x + 1, box.y + 4, " saved layouts:", fill);

        const int visible = std::max(0, box.height - 7);
        std::size_t start = 0;

        if (overwrite && matchIndex >= static_cast<std::size_t>(visible))
        {
            start = matchIndex - static_cast<std::size_t>(visible) + 1;
        }

        if (names.size() > static_cast<std::size_t>(visible) &&
            start + static_cast<std::size_t>(visible) > names.size())
        {
            start = names.size() - static_cast<std::size_t>(visible);
        }

        for (std::size_t i = start; i < names.size() && i - start < static_cast<std::size_t>(visible); ++i)
        {
            const std::string prefix = i == matchIndex ? "> " : "  ";
            const std::string line = prefix + names[i];
            writeText(canvas, box.x + 1, box.y + 5 + static_cast<int>(i - start),
            line.substr(0, static_cast<std::size_t>(box.width - 2)),
            i == matchIndex ? warnStyle : normalStyle);
        }

        writeText(canvas, box.x + 1, box.y + box.height - 2, " type a name; Backspace edits; processes restart on restore", fill);
    }
    //!

    /*!
        Draw Layout List Overlay

        !_method
    */
    void Renderer::drawLayoutListOverlay(ftxui::Canvas& canvas, int width, int height, ftxui::Color foreground, ftxui::Color background)
    {
        const auto fill = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = foreground;
            pixel.background_color = background;
        };

        const auto selected = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = background;
            pixel.background_color = foreground;
            pixel.bold = true;
        };

        const Rect box = centeredBox(width, height, 60, 16);
        fillRect(canvas, box, " ", fill);
        drawBox(canvas, box, fill);
        writeText(canvas, box.x + 1, box.y, " nexyth layouts (j/k, Enter restore, D delete, Esc cancel)", fill);
        const Canvas::Stylizer normalStyle = fill;
        const Canvas::Stylizer selectedStyle = selected;

        const auto& names = app_.layoutNames();
        const std::size_t index = app_.layoutIndex();
        const std::size_t start = index > 10 ? index - 10 : 0;

        if (names.empty())
        {
            writeText(canvas, box.x + 1, box.y + 2, " no saved layouts", fill);
        }

        for (std::size_t i = start; i < names.size() && i - start + 2 < static_cast<std::size_t>(box.height); ++i)
        {
            const std::string prefix = i == index ? "> " : "  ";
            writeText(canvas, box.x + 1, box.y + 2 + static_cast<int>(i - start), prefix + names[i], i == index ? selectedStyle : normalStyle);
        }
    }
    //!

    /*!
        Draw Resize Indicator

        !_method
    */
    void Renderer::drawResizeIndicator(ftxui::Canvas& canvas, int width, int height, ftxui::Color foreground, ftxui::Color background)
    {
        const auto style = [foreground, background](ftxui::Cell& pixel)
        {
            pixel.foreground_color = background;
            pixel.background_color = foreground;
            pixel.bold = true;
        };

        const std::string text = " RESIZE (H/J/K/L, =, Esc) ";
        const int x = std::max(0, width - static_cast<int>(text.size()) - 1);
        const int y = std::max(0, height - 2);
        writeText(canvas, x, y, text, style);
    }
    //!

    /*!
        Rect For

        !_method
    */
    std::optional<Rect> Renderer::rectFor(int paneId) const
    {
        for (const auto& entry : paneRects_)
        {
            if (entry.pane && entry.pane->id() == paneId) return entry.inner;
        }
        return std::nullopt;
    }
    //!

    /*!
        Terminal Cursor Screen Position

        !_method
    */
    std::optional<Pos> Renderer::terminalCursorScreenPosition() const
    {
        return cursor_position_;
    }
    //!
}
