#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nexyth {

struct Pos {
  int x = 0;
  int y = 0;
};

struct TermColor {
  bool set = false;
  bool rgb = false;
  int index = 0;
  int r = 0;
  int g = 0;
  int b = 0;
};

struct Style {
  TermColor foreground;
  TermColor background;
  bool bold = false;
  bool dim = false;
  bool italic = false;
  bool underlined = false;
  bool reverse = false;
  bool strikethrough = false;
};

struct Cell {
  std::string text;
  int width = 1;
  Style style;
};

class Vte;

class Buffer {
 public:
  Buffer(int columns, int rows, int scrollback);
  ~Buffer();
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  void feed(std::string_view data);
  void enterAlternateScreen();
  void leaveAlternateScreen();

  int columns() const { return columns_; }
  int rows() const { return rows_; }
  const std::vector<std::vector<Cell>>& lines() const { return lines_; }
  std::vector<std::vector<Cell>>& lines() { return lines_; }
  Pos cursor() const { return {cursor_x_, cursor_y_}; }
  Pos navigation() const { return navigation_; }
  std::optional<Pos> anchor() const { return anchor_; }
  std::optional<Pos> head() const { return head_; }
  int visibleStart() const;
  int maxScrollOffset() const;

  void resize(int columns, int rows);
  void putText(const std::string& text);
  void putGrapheme(const std::string& grapheme);
  void ensureLine(int index);
  Cell blankCell() const;
  void carriageReturn();
  void lineFeed();
  void backspace();
  void tab();
  void eraseLine(int mode);
  void eraseDisplay(int mode);
  void setScrollRegion(int top, int bottom);
  void insertLines(int count);
  void deleteLines(int count);
  void scrollRegionUp(int count);
  void scrollRegionDown(int count);
  void insertBlanks(int count);
  void deleteCharacters(int count);
  void eraseCharacters(int count);
  void saveCursor();
  void restoreCursor();
  void reset();

  void scrollBy(int delta);
  void scrollToTop();
  void scrollToBottom();
  void setNavigation(Pos position);
  void moveNavigation(int dx, int dy);
  void setAnchor(Pos position);
  void setHead(Pos position);
  void clearSelection();
  std::string selectedText() const;

  Style current_style_;
  Style saved_style_;
  bool wrap_pending_ = false;
  int cursor_x_ = 0;
  int cursor_y_ = 0;

 private:
  std::pair<int, int> scrollBounds() const;
  void shiftLines(int top, int bottom, int count, bool down);

  struct ScreenState {
    std::vector<std::vector<Cell>> lines;
    int cursor_x = 0;
    int cursor_y = 0;
    int saved_x = 0;
    int saved_y = 0;
    Style style;
    Style saved_style;
    int scroll_offset = 0;
    int scroll_top = 0;
    int scroll_bottom = -1;
    bool scroll_region_set = false;
    bool wrap_pending = false;
    Pos navigation;
  };

  int columns_ = 80;
  int rows_ = 24;
  int scrollback_limit_ = 20000;
  int saved_x_ = 0;
  int saved_y_ = 0;
  int scroll_offset_ = 0;
  int scroll_top_ = 0;
  int scroll_bottom_ = -1;
  bool scroll_region_set_ = false;
  Pos navigation_{0, 23};
  std::optional<Pos> anchor_;
  std::optional<Pos> head_;
  std::optional<ScreenState> saved_screen_;
  std::vector<std::vector<Cell>> lines_;
  Vte* parser_ = nullptr;
};

}  // namespace nexyth
