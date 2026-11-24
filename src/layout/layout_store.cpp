#include "layout_store.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

#include "../config/config.hpp"
#include "../utils.hpp"

namespace nexyth
{
    /*>
        Helper Methods
    */
    namespace
    {
        /*!
            Layout File Path

            !_method
        */
        std::string filePath()
        {
            return LayoutStore::directory() + "/layouts.conf";
        }
        //!

        /*!
            Encode a Text Field

            Percent-encodes everything except a small set of readable
            characters so names, commands and paths cannot break the format.

            !_method
        */
        std::string encodeField(std::string_view value)
        {
            static constexpr char hex[] = "0123456789ABCDEF";
            const auto readable = [](unsigned char character)
            {
                return (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == '.' || character == '_' ||
                character == '/' || character == ':';
            };

            std::string result;

            for (const char raw : value)
            {
                const auto character = static_cast<unsigned char>(raw);

                if (readable(character))
                {
                    result += static_cast<char>(character);
                }
                else
                {
                    result += '%';
                    result += hex[character >> 4];
                    result += hex[character & 0x0F];
                }
            }

            return result;
        }
        //!

        /*!
            Decode a Text Field

            !_method
        */
        std::string decodeField(std::string_view value)
        {
            if (value == "-") return "";

            const auto hexValue = [](char character) -> int
            {
                if (character >= '0' && character <= '9') return character - '0';
                if (character >= 'a' && character <= 'f') return character - 'a' + 10;
                if (character >= 'A' && character <= 'F') return character - 'A' + 10;
                return -1;
            };

            std::string result;

            for (std::size_t index = 0; index < value.size(); ++index)
            {
                if (value[index] == '%' && index + 2 < value.size())
                {
                    const int high = hexValue(value[index + 1]);
                    const int low = hexValue(value[index + 2]);

                    if (high >= 0 && low >= 0)
                    {
                        result += static_cast<char>((high << 4) | low);
                        index += 2;
                        continue;
                    }
                }

                result += value[index];
            }

            return result;
        }
        //!

        /*!
            Encode an Optional Text Field

            !_method
        */
        std::string optionalField(std::string_view value)
        {
            return value.empty() ? "-" : encodeField(value);
        }
        //!

        /*!
            Parse an Integer Field

            !_method
        */
        int toInt(const std::string& text, int fallback = 0)
        {
            try
            {
                return std::stoi(text);
            }
            catch (...)
            {
                return fallback;
            }
        }
        //!

        /*!
            Parse a Ratio Field

            !_method
        */
        double toRatio(const std::string& text)
        {
            try
            {
                return std::clamp(std::stod(text), 0.05, 0.95);
            }
            catch (...)
            {
                return 0.5;
            }
        }
        //!

        /*!
            Write One Node

            !_method
        */
        void writeNode(std::ostream& stream, const LayoutNodePtr& node, int depth)
        {
            if (!node) return;

            const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');

            if (node->type == LayoutNode::Type::Leaf)
            {
                stream << indent << "node leaf "
                << encodeField(node->pane.command) << " "
                << optionalField(node->pane.cwd) << "\n";
                return;
            }

            stream << indent << "node split "
            << (node->vertical ? "v" : "s") << " "
            << node->ratio << "\n";
            writeNode(stream, node->first, depth + 1);
            writeNode(stream, node->second, depth + 1);
        }
        //!
    }
    //!
    //!

    /*!
        Layout Store Directory

        Uses the same data directories as default.conf and themes.conf.

        !_method
    */
    std::string LayoutStore::directory()
    {
        std::error_code error;

        if (const char* overridePath = std::getenv("NEXYTH_CONFIG"))
        {
            if (*overridePath != '\0')
            {
                return (std::filesystem::path(overridePath).parent_path() / "layouts").string();
            }
        }

        const std::filesystem::path executable = executableDir();
        const std::vector<std::filesystem::path> candidates = {
            executable / "data" / "layouts",
            executable / ".." / "data" / "layouts",
            std::filesystem::path("config") / "layouts",
            std::filesystem::path(Config::defaultPath()).parent_path() / "layouts"
        };

        for (const auto& candidate : candidates)
        {
            if (std::filesystem::is_directory(candidate.parent_path(), error))
            {
                return candidate.string();
            }
        }

        return (executable / "data" / "layouts").string();
    }
    //!

    /*!
        Read All Saved Layouts

        !_method
    */
    std::vector<LayoutSnapshot> LayoutStore::readAll(std::string& error)
    {
        std::vector<LayoutSnapshot> layouts;
        const std::string path = filePath();
        std::ifstream stream(path);

        if (!stream)
        {
            if (std::filesystem::exists(path)) error = "cannot read: " + path;
            return layouts;
        }

        LayoutSnapshot current;
        std::vector<LayoutNodePtr> stack;
        std::string rawLine;

        while (std::getline(stream, rawLine))
        {
            if (!rawLine.empty() && rawLine.back() == '\r') rawLine.pop_back();

            std::size_t position = 0;

            while (position < rawLine.size() && rawLine[position] == ' ')
            {
                ++position;
            }

            const int depth = static_cast<int>(position / 2);
            std::istringstream fields(rawLine.substr(position));
            std::string keyword;
            fields >> keyword;

            if (keyword.empty() || keyword[0] == '#') continue;

            if (keyword == "layout")
            {
                std::string name;
                fields >> name;
                current = LayoutSnapshot{};
                current.name = decodeField(name);
                stack.clear();
                continue;
            }

            if (keyword == "end")
            {
                if (!current.name.empty()) layouts.push_back(current);
                current = LayoutSnapshot{};
                stack.clear();
                continue;
            }

            if (keyword == "version") continue;

            if (keyword == "active")
            {
                std::string index;
                fields >> index;
                current.activeIndex = toInt(index);
                continue;
            }

            if (keyword == "tab")
            {
                std::string name;
                std::string focus;
                fields >> name >> focus;

                LayoutTab tab;
                tab.name = decodeField(name);
                tab.focusIndex = std::max(0, toInt(focus));
                current.tabs.push_back(tab);
                stack.clear();
                continue;
            }

            if (keyword != "node" || current.tabs.empty()) continue;

            std::string kind;
            fields >> kind;

            auto node = std::make_shared<LayoutNode>();

            if (kind == "leaf")
            {
                std::string command;
                std::string cwd;
                fields >> command >> cwd;
                node->pane.command = decodeField(command);
                node->pane.cwd = decodeField(cwd);
            }
            else if (kind == "split")
            {
                std::string orientation;
                std::string ratio;
                fields >> orientation >> ratio;
                node->type = LayoutNode::Type::Split;
                node->vertical = orientation != "s";
                node->ratio = toRatio(ratio);
            }
            else
            {
                continue;
            }

            LayoutTab& tab = current.tabs.back();

            if (depth <= 0)
            {
                tab.root = node;
            }
            else if (depth - 1 < static_cast<int>(stack.size()))
            {
                const auto parent = stack[static_cast<std::size_t>(depth - 1)];

                if (!parent->first) parent->first = node;
                else if (!parent->second) parent->second = node;
                else continue;
            }
            else
            {
                continue;
            }

            stack.resize(static_cast<std::size_t>(depth));
            stack.push_back(node);
        }

        return layouts;
    }
    //!

    /*!
        Write All Saved Layouts

        !_method
    */
    bool LayoutStore::writeAll(const std::vector<LayoutSnapshot>& layouts, std::string& error)
    {
        std::error_code ec;
        std::filesystem::create_directories(directory(), ec);

        if (ec)
        {
            error = "cannot create " + directory() + ": " + ec.message();
            return false;
        }

        const std::string path = filePath();
        const std::string temporary = path + ".tmp";
        std::ofstream stream(temporary, std::ios::trunc);

        if (!stream)
        {
            error = "cannot write: " + temporary;
            return false;
        }

        stream << "version 1\n";

        for (const auto& layout : layouts)
        {
            stream << "layout " << encodeField(layout.name) << "\n";
            stream << "active " << layout.activeIndex << "\n";

            for (const auto& tab : layout.tabs)
            {
                stream << "tab " << optionalField(tab.name) << " " << tab.focusIndex << "\n";
                writeNode(stream, tab.root, 0);
            }

            stream << "end\n";
        }

        stream.close();

        if (!stream)
        {
            error = "cannot write: " + temporary;
            return false;
        }

        std::filesystem::remove(path, ec);
        std::filesystem::rename(temporary, path, ec);

        if (ec)
        {
            error = "cannot save: " + ec.message();
            return false;
        }

        return true;
    }
    //!

    /*!
        List Saved Layout Names

        !_method
    */
    std::vector<std::string> LayoutStore::list(std::string& error)
    {
        std::vector<std::string> names;

        for (const auto& layout : readAll(error))
        {
            names.push_back(layout.name);
        }

        return names;
    }
    //!

    /*!
        Save a Named Layout

        !_method
    */
    bool LayoutStore::save(const LayoutSnapshot& snapshot, std::string& error)
    {
        if (snapshot.name.empty())
        {
            error = "layout name is empty";
            return false;
        }

        std::vector<LayoutSnapshot> layouts = readAll(error);
        if (!error.empty()) return false;

        bool replaced = false;

        for (auto& layout : layouts)
        {
            if (layout.name == snapshot.name)
            {
                layout = snapshot;
                replaced = true;
                break;
            }
        }

        if (!replaced) layouts.push_back(snapshot);

        return writeAll(layouts, error);
    }
    //!

    /*!
        Load a Named Layout

        !_method
    */
    bool LayoutStore::load(const std::string& name, LayoutSnapshot& snapshot, std::string& error)
    {
        for (const auto& layout : readAll(error))
        {
            if (layout.name == name)
            {
                snapshot = layout;
                return true;
            }
        }

        if (error.empty()) error = "layout not found: " + name;
        return false;
    }
    //!

    /*!
        Delete a Named Layout

        !_method
    */
    bool LayoutStore::remove(const std::string& name, std::string& error)
    {
        std::vector<LayoutSnapshot> layouts = readAll(error);
        if (!error.empty()) return false;

        const auto original = layouts.size();

        layouts.erase(std::remove_if(layouts.begin(), layouts.end(), [&](const LayoutSnapshot& layout)
        {
            return layout.name == name;
        }), layouts.end());

        if (layouts.size() == original)
        {
            error = "layout not found: " + name;
            return false;
        }

        return writeAll(layouts, error);
    }
    //!
}
