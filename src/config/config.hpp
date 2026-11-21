#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../core/mode.hpp"

namespace nexyth
{
    /*!
        Command Struct

    */
    //>>
    struct Command
    {
        std::string action;
        std::vector<std::string> args;
    };
    //<<
    //!

    /*!
        Binding Struct

    */
    //>>
    struct Binding
    {
        Command command;
        std::string key;
        std::string mode;
    };
    //<<
    //!

    /*!
        Settings Struct

    */
    //>>
    struct Settings
    {
        std::string shell = "/bin/bash";
        int scrollback = 20000;
        bool tabbar = false;
        bool statusline = true;
        int resize_step = 5;
        std::string leader = "<C-b>";

        std::string foreground = "#c0caf5";
        std::string background = "#1a1b26";
        std::string border = "#3b4261";
        std::string active_border = "#7aa2f7";
        std::string inactive_border = "#3b4261";
        std::string focused_border = "#bb9af7";
        std::string status_background = "#24283b";
        std::string status_foreground = "#c0caf5";
        std::string tab_background = "#1a1b26";
        std::string tab_foreground = "#a9b1d6";
        std::string active_tab_background = "#414868";
        std::string active_tab_foreground = "#c0caf5";
        std::string selection_background = "#364a82";
        std::string selection_foreground = "#c0caf5";
        std::string cursor = "#c0caf5";
        std::string text = "#c0caf5";
        std::string keyword = "#bb9af7";
        std::string function = "#7aa2f7";
        std::string variable = "#c0caf5";
        std::string type = "#e0af68";
        std::string string = "#9ece6a";
        std::string comment = "#565f89";
        std::string number = "#ff9e64";
        std::string op = "#89ddff";
        std::string bracket = "#ff9e64";
    };
    //<<
    //!

    /*!
        Use Theme

    */
    //>>
    using Theme = std::pair<std::string, Settings>;
    //<<
    //!

    /*!
        Config Class

    */
    //>>
    class Config
    {
        public:
        Config();
        static Config load(const std::string& path);
        static std::string defaultPath();
        static bool applySetting(Settings& settings, const std::string& name,
        const std::string& value);
        static bool saveColorSettings(const std::string& path,
        const Settings& settings);

        std::vector<Command> resolve(const std::string& name) const;
        const Binding* findBinding(Mode mode, const std::string& key) const;
        bool hasPrefix(Mode mode, const std::string& key) const;
        std::vector<std::pair<std::string, std::string>> listBindings(Mode mode) const;

        Settings settings;
        std::vector<Theme> themes;
        std::vector<std::string> errors;
        std::string path;

        private:
        std::map<std::string, std::vector<Command>> commands_;
        std::vector<std::vector<Binding>> keymaps_;
    };
    //<<
    //!
}
