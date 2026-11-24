#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "layout.hpp"
#include "node.hpp"

namespace nexyth
{
    /*!
        Tab Struct
    */
    //>>
    struct Tab
    {
        NodePtr root;
        int focusId = -1;
        std::string name;
    };
    //<<
    //!

    /*!
        Workspace Class
    */
    class Workspace {
        /*!
            Public Members
        */
        public:
            //>>
            Workspace(std::string shell, int scrollback, std::function<void(int)> onData);

            void createInitial(int columns, int rows);
            std::vector<Tab>& tabs() { return tabs_; }
            const std::vector<Tab>& tabs() const { return tabs_; }
            Tab& active() { return tabs_[active_]; }
            int activeIndex() const { return active_; }
            int tabCount() const { return static_cast<int>(tabs_.size()); }
            std::string tabLabel(int index) const;
            void setTabName(std::string name);

            std::shared_ptr<Pane> focusedPane() const;
            void setFocus(int paneId);
            void focusNextPane();
            void focusPreviousPane();
            void collectPanes(const NodePtr& node, std::vector<int>& out) const;

            void selectTab(int oneBased);
            void newTab(int columns, int rows);
            void closeTab();

            void splitFocused(bool vertical, int columns, int rows);
            void splitWindow(bool vertical, int columns, int rows);
            void closePane();

            LayoutSnapshot snapshot() const;
            void restore(const LayoutSnapshot& snapshot, int columns, int rows);
            //<<
        //!

        /*!
            Private Members
        */
        private:
            //>>
            std::shared_ptr<Pane> makePane(int columns, int rows,
            const std::string& command = {}, const std::string& cwd = {});
            LayoutNodePtr snapshotNode(const NodePtr& node) const;
            NodePtr restoreNode(const LayoutNodePtr& node, int columns, int rows);
            void normalizeFocus(Tab& tab);

            std::string shell_;
            int scrollback_ = 20000;
            std::function<void(int)> onData_;
            std::vector<Tab> tabs_;
            int active_ = 0;
            int next_id_ = 1;
            //<<
        //!
    };
    //!
}
