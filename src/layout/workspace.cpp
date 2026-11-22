#include "workspace.hpp"

namespace nexyth
{
    /*!
        Workspace Constructor

        !_ctor
    */
    Workspace::Workspace(std::string shell, int scrollback, std::function<void(int)> onData)
        : shell_(std::move(shell)), scrollback_(scrollback), onData_(std::move(onData)) {}
    //!

    /*!
        Make Pane

        !_method

        ----
    */
    //>>
    std::shared_ptr<Pane> Workspace::makePane(int columns, int rows)
    {
        const int id = next_id_++;
        auto pane = std::make_shared<Pane>(id, shell_, columns, rows, scrollback_, onData_);
        pane->start();
        return pane;
    }
    //<<
    //!

    /*!
        Create Initial

        !_method
    */
    void Workspace::createInitial(int columns, int rows)
    {
        if (!tabs_.empty()) return;

        auto pane = makePane(columns, rows);
        Tab tab;
        tab.root = makePaneNode(pane);
        tab.focusId = pane->id();
        tabs_.push_back(tab);

        active_ = 0;
    }
    //!

    /*!
        Tab Label

        !_method
    */
    std::string Workspace::tabLabel(int index) const
    {
        if (index < 0 || index >= static_cast<int>(tabs_.size())) return "?";

        if (!tabs_[static_cast<std::size_t>(index)].name.empty())
        {
            return std::to_string(index + 1) + ": " +
            tabs_[static_cast<std::size_t>(index)].name;
        }

        auto pane = focusedPane();

        if (index == active_ && pane)
        {
            return std::to_string(index + 1) + ": " + pane->title();
        }

        return std::to_string(index + 1) + ": /bin/bash";
    }
    //!

    /*!
        Set Tab Name

        !_method
    */
    void Workspace::setTabName(std::string name)
    {
        if (active_ >= 0 && active_ < static_cast<int>(tabs_.size()))
        {
            tabs_[static_cast<std::size_t>(active_)].name = std::move(name);
        }
    }
    //!

    /*!
        Focus Pane

        !_method
    */
    std::shared_ptr<Pane> Workspace::focusedPane() const
    {
        if (active_ < 0 || active_ >= static_cast<int>(tabs_.size())) return nullptr;
        const auto found = findPane(tabs_[active_].root, tabs_[active_].focusId);
        return found ? found->pane : nullptr;
    }
    //!

    /*!
        Collect Panes

        !_method
    */
    void Workspace::collectPanes(const NodePtr& node, std::vector<int>& out) const
    {
        if (!node) return;

        if (node->type == Node::Type::Leaf)
        {
            if (node->pane) out.push_back(node->pane->id());
            return;
        }
        collectPanes(node->first, out);
        collectPanes(node->second, out);
    }
    //!

    /*!
        Normalize Focus

        !_method
    */
    void Workspace::normalizeFocus(Tab& tab)
    {
        std::vector<int> panes;
        collectPanes(tab.root, panes);

        if (panes.empty())
        {
            tab.focusId = -1;
            return;
        }

        for (int id : panes)
        {
            if (id == tab.focusId) return;
        }

        tab.focusId = panes.front();
    }
    //!

    /*!
        Set Focus

        !_method
    */
    void Workspace::setFocus(int paneId)
    {
        if (active_ < 0 || active_ >= static_cast<int>(tabs_.size())) return;
        if (findPane(tabs_[active_].root, paneId)) tabs_[active_].focusId = paneId;
    }
    //!

    /*!
        Focus Next Pane

        !_method
    */
    void Workspace::focusNextPane()
    {
        std::vector<int> panes;
        collectPanes(tabs_[active_].root, panes);

        if (panes.empty()) return;

        for (std::size_t i = 0; i < panes.size(); ++i)
        {
            if (panes[i] == tabs_[active_].focusId)
            {
                tabs_[active_].focusId = panes[(i + 1) % panes.size()];
                return;
            }
        }

        tabs_[active_].focusId = panes.front();
    }
    //!

    /*!
        Focus Previous Pane

        !_method
    */
    void Workspace::focusPreviousPane()
    {
        std::vector<int> panes;
        collectPanes(tabs_[active_].root, panes);

        if (panes.empty()) return;

        for (std::size_t i = 0; i < panes.size(); ++i)
        {
            if (panes[i] == tabs_[active_].focusId)
            {
                tabs_[active_].focusId =
                panes[(i + panes.size() - 1) % panes.size()];
                return;
            }
        }

        tabs_[active_].focusId = panes.back();
    }
    //!

    /*!
        Select Tab

        !_method
    */
    void Workspace::selectTab(int oneBased)
    {
        const int index = oneBased - 1;
        if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

        active_ = index;
        normalizeFocus(tabs_[active_]);
    }
    //!

    /*!
        New Tab

        !_method
    */
    void Workspace::newTab(int columns, int rows)
    {
        auto pane = makePane(columns, rows);
        Tab tab;
        tab.root = makePaneNode(pane);
        tab.focusId = pane->id();
        tabs_.push_back(tab);
        active_ = static_cast<int>(tabs_.size()) - 1;
    }
    //!

    /*!
        Close Tab

        !_method
    */
    void Workspace::closeTab()
    {
        if (tabs_.size() <= 1) return;

        tabs_.erase(tabs_.begin() + active_);

        if (active_ >= static_cast<int>(tabs_.size()))
        {
            active_ = static_cast<int>(tabs_.size()) - 1;
        }

        normalizeFocus(tabs_[active_]);
    }
    //!

    /*!
        Split Focused

        !_method
    */
    void Workspace::splitFocused(bool vertical, int columns, int rows)
    {
        Tab& tab = tabs_[active_];
        if (!tab.root) return;

        auto current = findPane(tab.root, tab.focusId);
        if (!current || !current->pane) return;

        auto fresh = makePane(columns, rows);
        const SplitKind kind = vertical ? SplitKind::SideBySide : SplitKind::Stacked;
        auto split = makeSplitNode(current, makePaneNode(fresh), kind);
        replacePane(tab.root, tab.focusId, split);
        tab.focusId = fresh->id();
    }
    //!

    /*!
        Split Window

        !_method
    */
    void Workspace::splitWindow(bool vertical, int columns, int rows)
    {
        Tab& tab = tabs_[active_];
        if (!tab.root) return;

        auto fresh = makePane(columns, rows);
        const SplitKind kind =
        vertical ? SplitKind::SideBySide : SplitKind::Stacked;
        tab.root = makeSplitNode(tab.root, makePaneNode(fresh), kind);
        tab.focusId = fresh->id();
    }
    //!

    /*!
        Close Pane

        !_method
    */
    void Workspace::closePane()
    {
        Tab& tab = tabs_[active_];
        auto current = findPane(tab.root, tab.focusId);

        if (!current) return;
        if (!current->pane) return;

        std::vector<int> panes;
        collectPanes(tab.root, panes);

        if (panes.size() == 1)
        {
            current->pane.reset();
            tab.focusId = -1;
            return;
        }

        auto parent = findParent(tab.root, tab.focusId);
        if (!parent) return;

        const bool isFirst = parent->first && findPane(parent->first, tab.focusId);
        const auto replacement = isFirst ? parent->second : parent->first;
        replaceNode(tab.root, parent, replacement);
        std::vector<int> remaining;
        collectPanes(tab.root, remaining);
        tab.focusId = remaining.empty() ? -1 : remaining.front();
    }
    //!
}
