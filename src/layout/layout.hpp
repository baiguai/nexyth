#pragma once

#include <memory>
#include <string>
#include <vector>

namespace nexyth
{
    /*!
        Layout Pane Struct

        Describes how to recreate one pane. Live processes and terminal
        scrollback cannot be serialized, so restore starts a fresh process.
    */
    //>>
    struct LayoutPane
    {
        std::string command;
        std::string cwd;
    };
    //<<
    //!

    /*!
        Layout Node Struct

        Serializable form of a node in a tab's split tree.
    */
    //>>
    struct LayoutNode
    {
        enum class Type { Leaf, Split };

        Type type = Type::Leaf;
        bool vertical = true;
        double ratio = 0.5;
        std::shared_ptr<LayoutNode> first;
        std::shared_ptr<LayoutNode> second;
        LayoutPane pane;
    };
    //<<
    //!

    using LayoutNodePtr = std::shared_ptr<LayoutNode>;

    /*!
        Layout Tab Struct

        Focus is stored as an index into the tab's panes (depth-first) so it
        stays valid when panes are recreated with new runtime ids.
    */
    //>>
    struct LayoutTab
    {
        std::string name;
        int focusIndex = 0;
        LayoutNodePtr root;
    };
    //<<
    //!

    /*!
        Layout Snapshot Struct

        Complete saved layout: every tab, its split tree and the active tab.
    */
    //>>
    struct LayoutSnapshot
    {
        std::string name;
        std::vector<LayoutTab> tabs;
        int activeIndex = 0;
    };
    //<<
    //!
}
