#include "buffer.hpp"

#include <algorithm>
#include <utility>

#include "../utils.hpp"
#include "vte.hpp"

namespace nexyth
{
    /*>
        Helper Methods
    */
    namespace
    {
        /*!
            Clamp Int

            !_method

            ----
        */
        //>>
        int clampInt(int value, int low, int high)
        {
            if (high < low) return low;
            return std::max(low, std::min(high, value));
        }
        //<<
        //!
    }
    //!

    /*!
        Buffer
    */
        /*!
            Buffer Constructor

            !_ctor
        */
        Buffer::Buffer(int columns, int rows, int scrollback)
            : columns_(std::max(1, columns)),
              rows_(std::max(1, rows)),
              scrollback_limit_(std::max(0, scrollback))
        {
            lines_.assign(rows_, std::vector<Cell>(columns_));
            navigation_ = {0, rows_ - 1};
            parser_ = new Vte(*this);
        }
        //!

        /*!
            Buffer Destructor

            !_method

            ----
        */
        //>>
        Buffer::~Buffer() { delete parser_; }
        //<<
        //!

        /*!
            Buffer Feed

            !_method
        */
        void Buffer::feed(std::string_view data)
        {
            parser_->feed(data);
        }
        //!

        /*!
            Enter Alternate Screen

            !_method

            ----

            This manages state.

        */
        void Buffer::enterAlternateScreen()
        {
            if (saved_screen_) return;

            //>>
            ScreenState state;
            state.lines = lines_;
            state.cursor_x = cursor_x_;
            state.cursor_y = cursor_y_;
            state.saved_x = saved_x_;
            state.saved_y = saved_y_;
            state.style = current_style_;
            state.saved_style = saved_style_;
            state.scroll_offset = scroll_offset_;
            state.scroll_top = scroll_top_;
            state.scroll_bottom = scroll_bottom_;
            state.scroll_region_set = scroll_region_set_;
            state.wrap_pending = wrap_pending_;
            state.navigation = navigation_;
            saved_screen_ = std::move(state);

            lines_.assign(rows_, std::vector<Cell>(columns_, Cell{}));
            cursor_x_ = 0;
            cursor_y_ = 0;
            saved_x_ = 0;
            saved_y_ = 0;
            current_style_ = Style{};
            saved_style_ = Style{};
            scroll_offset_ = 0;
            scroll_top_ = 0;
            scroll_bottom_ = -1;
            scroll_region_set_ = false;
            wrap_pending_ = false;
            navigation_ = {0, rows_ - 1};
            clearSelection();
            //<<
        }
        //!

        /*!
            Leave Alternate Screen

            !_method

            ----

            This restores from state.

        */
        void Buffer::leaveAlternateScreen()
        {
            if (!saved_screen_) return;

            //>>
            ScreenState state = std::move(*saved_screen_);
            saved_screen_.reset();

            lines_ = std::move(state.lines);
            cursor_x_ = state.cursor_x;
            cursor_y_ = state.cursor_y;
            saved_x_ = state.saved_x;
            saved_y_ = state.saved_y;
            current_style_ = state.style;
            saved_style_ = state.saved_style;
            scroll_offset_ = state.scroll_offset;
            scroll_top_ = state.scroll_top;
            scroll_bottom_ = state.scroll_bottom;
            scroll_region_set_ = state.scroll_region_set;
            wrap_pending_ = state.wrap_pending;
            navigation_ = state.navigation;
            clearSelection();
            //<<
        }
        //!

        /*!
            Black Cell

            !_method
        */
        Cell Buffer::blankCell() const
        {
            Cell cell;
            cell.style = current_style_;
            return cell;
        }
        //!

        /*!
            Put Grapheme

            !_method

            ----

            What is grapheme?

            #todo
        */
        void Buffer::putGrapheme(const std::string& grapheme)
        {
            if (grapheme.empty()) return;

            if (wrap_pending_)
            {
                wrap_pending_ = false;
                carriageReturn();
                lineFeed();
            }

            std::size_t position = 0;
            std::uint32_t codepoint = 0;
            if (!utf8Decode(grapheme, position, codepoint)) return;

            const int width = codepointWidth(codepoint);

            if (width == 0)
            {
                if (cursor_x_ > 0 && cursor_y_ < static_cast<int>(lines_.size()))
                {
                    lines_[cursor_y_][cursor_x_ - 1].text += grapheme;
                }
                return;
            }

            if (cursor_x_ + width > columns_)
            {
                carriageReturn();
                lineFeed();
            }

            ensureLine(cursor_y_);
            auto& line = lines_[cursor_y_];
            if (cursor_x_ >= columns_) return;

            Cell cell = blankCell();
            cell.text = grapheme;
            cell.width = width;
            line[cursor_x_] = cell;

            if (width > 1 && cursor_x_ + 1 < columns_)
            {
                Cell continuation = blankCell();
                continuation.text = "";
                continuation.width = 0;
                line[cursor_x_ + 1] = continuation;
            }

            cursor_x_ += width;

            if (cursor_x_ >= columns_)
            {
                cursor_x_ = columns_ - 1;
                wrap_pending_ = true;
            }
        }
        //!

        /*!
            Erase Display

            !_method
        */
        void Buffer::eraseDisplay(int mode)
        {
            if (mode == 2 || mode == 3)
            {
                lines_.assign(rows_, std::vector<Cell>(columns_, blankCell()));
                cursor_x_ = 0;
                cursor_y_ = 0;
                clearSelection();
                return;
            }

            if (mode == 1)
            {
                for (int y = 0; y <= cursor_y_ && y < static_cast<int>(lines_.size()); ++y)
                {
                    const int end = y == cursor_y_ ? cursor_x_ + 1 : columns_;

                    for (int x = 0; x < end && x < columns_; ++x)
                    {
                        lines_[y][x] = blankCell();
                    }
                }
            }
            else
            {
                eraseLine(0);

                for (int y = cursor_y_ + 1; y < static_cast<int>(lines_.size()); ++y)
                {
                    lines_[y].assign(columns_, blankCell());
                }
            }
        }
        //!

        /*!
            Shift Lines

            !_method
        */
        void Buffer::shiftLines(int top, int bottom, int count, bool down)
        {
            if (top > bottom) return;

            while (static_cast<int>(lines_.size()) <= bottom)
            {
                lines_.emplace_back(columns_, blankCell());
            }

            count = clampInt(count, 0, bottom - top + 1);

            for (int i = 0; i < count; ++i)
            {
                if (down)
                {
                    lines_.insert(lines_.begin() + top,
                    std::vector<Cell>(columns_, blankCell()));
                    lines_.erase(lines_.begin() + bottom + 1);
                }
                else
                {
                    lines_.erase(lines_.begin() + top);
                    lines_.insert(lines_.begin() + bottom,
                    std::vector<Cell>(columns_, blankCell()));
                }
            }

            clearSelection();
        }
        //!

        /*!
            Reset

            !_method
        */
        void Buffer::reset()
        {
            lines_.assign(rows_, std::vector<Cell>(columns_, blankCell()));
            cursor_x_ = 0;
            cursor_y_ = 0;
            saved_x_ = 0;
            saved_y_ = 0;
            current_style_ = Style{};
            saved_style_ = Style{};
            scroll_offset_ = 0;
            scroll_top_ = 0;
            scroll_bottom_ = -1;
            scroll_region_set_ = false;
            clearSelection();
        }
        //!
    //!

    /*!
        Cursor Handlers
    */
        /*!
            Save Cursor

            !_method
        */
        void Buffer::saveCursor()
        {
            saved_x_ = cursor_x_;
            saved_y_ = cursor_y_;
            saved_style_ = current_style_;
        }
        //!

        /*!
            Restore Cursor

            !_method
        */
        void Buffer::restoreCursor()
        {
            cursor_x_ = clampInt(saved_x_, 0, columns_ - 1);
            cursor_y_ = saved_y_;
            ensureLine(cursor_y_);
            current_style_ = saved_style_;
        }
        //!
    //!

    /*!
        Text Handlers
    */
        /*!
            Ensure Line

            !_method
        */
        void Buffer::ensureLine(int index)
        {
            while (static_cast<int>(lines_.size()) <= index)
            {
                lines_.emplace_back(columns_, blankCell());
            }

            const auto limit = static_cast<int>(rows_) + scrollback_limit_;

            if (static_cast<int>(lines_.size()) > limit && cursor_y_ > 0)
            {
                const int excess = static_cast<int>(lines_.size()) - limit;
                lines_.erase(lines_.begin(), lines_.begin() + excess);
                cursor_y_ = std::max(0, cursor_y_ - excess);
                saved_y_ = std::max(0, saved_y_ - excess);
            }
        }
        //!

        /*!
            Put Text

            !_method
        */
        void Buffer::putText(const std::string& text)
        {
            std::size_t position = 0;
            std::string pending;

            while (position < text.size())
            {
                const std::size_t start = position;
                std::uint32_t codepoint = 0;
                const bool valid = utf8Decode(text, position, codepoint);

                if (!valid)
                {
                    position = start + 1;
                    pending.clear();
                    continue;
                }

                pending += text.substr(start, position - start);
                const int width = codepointWidth(codepoint);

                if (width >= 0)
                {
                    putGrapheme(pending);
                    pending.clear();
                }
            }

            if (!pending.empty()) putGrapheme(pending);
        }
        //!

        /*!
            Carriage Return

            !_method
        */
        void Buffer::carriageReturn()
        {
            cursor_x_ = 0;
            wrap_pending_ = false;
        }
        //!

        /*!
            Line Feed

            !_method
        */
        void Buffer::lineFeed()
        {
            wrap_pending_ = false;

            if (scroll_region_set_)
            {
                const auto bounds = scrollBounds();

                if (cursor_y_ >= bounds.second)
                {
                    shiftLines(bounds.first, bounds.second, 1, false);
                    cursor_y_ = bounds.second;
                }
                else
                {
                    ++cursor_y_;
                    ensureLine(cursor_y_);
                }
            }
            else
            {
                ++cursor_y_;
                ensureLine(cursor_y_);
            }

            scroll_offset_ = 0;
            clearSelection();
        }
        //!

        /*!
            Back Space

            !_method
        */
        void Buffer::backspace()
        {
            cursor_x_ = std::max(0, cursor_x_ - 1);
            wrap_pending_ = false;
        }
        //!

        /*!
            Tab

            !_method
        */
        void Buffer::tab()
        {
            wrap_pending_ = false;
            int next = ((cursor_x_ / 8) + 1) * 8;
            if (next >= columns_) next = columns_ - 1;
            cursor_x_ = next;
        }
        //!

        /*!
            Erase Line

            !_method
        */
        void Buffer::eraseLine(int mode)
        {
            ensureLine(cursor_y_);
            auto& line = lines_[cursor_y_];
            const int from = mode == 1 ? 0 : cursor_x_;
            const int to = mode == 2 ? columns_ : (mode == 1 ? cursor_x_ + 1 : columns_);

            for (int x = from; x < to && x < columns_; ++x)
            {
                line[x] = blankCell();
            }
        }
        //!

        /*!
            Insert Lines

            !_method
        */
        void Buffer::insertLines(int count)
        {
            const auto bounds = scrollBounds();
            shiftLines(bounds.first, bounds.second, count, true);
        }
        //!

        /*!
            Delete Lines

            !_method
        */
        void Buffer::deleteLines(int count)
        {
            const auto bounds = scrollBounds();
            shiftLines(bounds.first, bounds.second, count, false);
        }
        //!

        /*!
            Insert Blanks

            !_method
        */
        void Buffer::insertBlanks(int count)
        {
            ensureLine(cursor_y_);
            auto& line = lines_[cursor_y_];
            count = clampInt(count, 0, columns_ - cursor_x_);

            for (int i = 0; i < count; ++i)
            {
                line.insert(line.begin() + cursor_x_, blankCell());
                line.pop_back();
            }
        }
        //!

        /*!
            Delete Characters

            !_method
        */
        void Buffer::deleteCharacters(int count)
        {
            ensureLine(cursor_y_);
            auto& line = lines_[cursor_y_];
            count = clampInt(count, 0, columns_ - cursor_x_);

            for (int i = 0; i < count; ++i)
            {
                line.erase(line.begin() + cursor_x_);
                line.push_back(blankCell());
            }
        }
        //!

        /*!
            Erase Characters

            !_method
        */
        void Buffer::eraseCharacters(int count)
        {
            ensureLine(cursor_y_);
            const int end = std::min(columns_, cursor_x_ + count);

            for (int x = cursor_x_; x < end; ++x)
            {
                lines_[cursor_y_][x] = blankCell();
            }
        }
        //!

        /*!
            Clear Selection

            !_method
        */
        void Buffer::clearSelection()
        {
            anchor_.reset();
            head_.reset();
        }
        //!

        /*!
            Selected Text

            !_method
        */
        std::string Buffer::selectedText() const
        {
            if (!anchor_ || !head_) return "";

            Pos first = *anchor_;
            Pos last = *head_;

            if (last.y < first.y || (last.y == first.y && last.x < first.x))
            {
                std::swap(first, last);
            }

            std::string result;

            for (int y = first.y; y <= last.y && y < static_cast<int>(lines_.size()); ++y)
            {
                const int from = y == first.y ? first.x : 0;
                const int to = y == last.y ? last.x + 1 : columns_;

                for (int x = from; x < to && x < static_cast<int>(lines_[y].size()); ++x)
                {
                    result += lines_[y][x].text;
                }

                if (y != last.y) result += '\n';
            }
            return result;
        }
        //!
    //!

    /*!
        Scrolling Handlers
    */
        /*!
            Scroll Bounds

            !_method
        */
        std::pair<int, int> Buffer::scrollBounds() const
        {
            if (scroll_region_set_)
            {
                const int base = visibleStart();
                const int bottom = scroll_bottom_ < 0 ? rows_ - 1 : scroll_bottom_;
                return {base + scroll_top_, base + bottom};
            }

            const int base = visibleStart();
            return {base, base + rows_ - 1};
        }
        //!

        /*!
            Set Scroll Region

            !_method
        */
        void Buffer::setScrollRegion(int top, int bottom)
        {
            if (top <= 0 && bottom <= 0)
            {
                scroll_top_ = 0;
                scroll_bottom_ = -1;
                scroll_region_set_ = false;
                return;
            }

            const int first = clampInt(top - 1, 0, rows_ - 1);
            const int last = bottom > 0 ? clampInt(bottom - 1, first, rows_ - 1) : rows_ - 1;
            scroll_top_ = first;
            scroll_bottom_ = last;
            scroll_region_set_ = true;
        }
        //!

        /*!
            Scroll Region Up

            !_method
        */
        void Buffer::scrollRegionUp(int count)
        {
            const auto bounds = scrollBounds();
            shiftLines(bounds.first, bounds.second, count, false);
        }
        //!

        /*!
            Scroll Region Down

            !_method
        */
        void Buffer::scrollRegionDown(int count)
        {
            const auto bounds = scrollBounds();
            shiftLines(bounds.first, bounds.second, count, true);
        }
        //!

        /*!
            Max Scroll Offset

            !_method
        */
        int Buffer::maxScrollOffset() const
        {
            return std::max(0, cursor_y_ - rows_ + 1);
        }
        //!

        /*!
            Visible Start

            !_method
        */
        int Buffer::visibleStart() const
        {
            return maxScrollOffset() - scroll_offset_;
        }
        //!

        /*!
            Resize

            !_method
        */
        void Buffer::resize(int columns, int rows)
        {
            columns_ = std::max(1, columns);
            rows_ = std::max(1, rows);

            for (auto& line : lines_)
            {
                if (static_cast<int>(line.size()) < columns_)
                {
                    line.resize(columns_, blankCell());
                }
                else if (static_cast<int>(line.size()) > columns_)
                {
                    line.resize(columns_);
                }
            }

            while (static_cast<int>(lines_.size()) < rows_)
            {
                lines_.emplace_back(columns_, blankCell());
            }

            cursor_x_ = clampInt(cursor_x_, 0, columns_ - 1);
            cursor_y_ = clampInt(cursor_y_, 0, static_cast<int>(lines_.size()) - 1);
            scroll_offset_ = clampInt(scroll_offset_, 0, maxScrollOffset());
            navigation_.x = clampInt(navigation_.x, 0, columns_ - 1);
            const int start = visibleStart();
            navigation_.y = clampInt(navigation_.y, start, start + rows_ - 1);

            if (anchor_) *anchor_ = {clampInt(anchor_->x, 0, columns_ - 1), clampInt(anchor_->y, 0, static_cast<int>(lines_.size()) - 1)};
            if (head_) *head_ = {clampInt(head_->x, 0, columns_ - 1), clampInt(head_->y, 0, static_cast<int>(lines_.size()) - 1)};
        }
        //!

        /*!
            Set Navigation

            !_method
        */
        void Buffer::setNavigation(Pos position)
        {
            const int start = visibleStart();
            const int end = std::min(static_cast<int>(lines_.size()) - 1, start + rows_ - 1);

            if (position.y < start)
            {
                scroll_offset_ = clampInt(scroll_offset_ + start - position.y, 0,
                maxScrollOffset());
            }
            else if (position.y > end)
            {
                scroll_offset_ = clampInt(scroll_offset_ - position.y + end, 0,
                maxScrollOffset());
            }

            navigation_ = {clampInt(position.x, 0, columns_ - 1),
            clampInt(position.y, start, end)};

            if (anchor_ && head_)
            {
                *head_ = navigation_;
            }
        }
        //!

        /*!
            Move Navigation

            !_method
        */
        void Buffer::moveNavigation(int dx, int dy)
        {
            setNavigation({navigation_.x + dx, navigation_.y + dy});
        }
        //!

        /*!
            Set Anchor

            !_method
        */
        void Buffer::setAnchor(Pos position)
        {
            anchor_ = position;
            head_ = position;
            navigation_ = position;
        }
        //!

        /*!
            Set Head

            !_method
        */
        void Buffer::setHead(Pos position)
        {
            head_ = position;
            navigation_ = position;
        }
        //!

        /*!
            Scroll By

            !_method
        */
        void Buffer::scrollBy(int delta)
        {
            scroll_offset_ = clampInt(scroll_offset_ + delta, 0, maxScrollOffset());
            const int start = visibleStart();
            navigation_.y = clampInt(navigation_.y, start, start + rows_ - 1);
        }
        //!

        /*!
            Scroll To Top

            !_method
        */
        void Buffer::scrollToTop()
        {
            scroll_offset_ = maxScrollOffset();
        }
        //!

        /*!
            Scroll To Bottom

            !_method
        */
        void Buffer::scrollToBottom()
        {
            scroll_offset_ = 0;
        }
        //!
    //!
}
