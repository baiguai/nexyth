#pragma once

#include <string>
#include <vector>

#include "layout.hpp"

namespace nexyth
{
    /*!
        Layout Store Class

        Persists named layout snapshots in a simple text file alongside the
        default/themes config data. Layouts are independent of config bindings.
    */
    //>>
    class LayoutStore
    {
        public:
            static std::string directory();
            static std::vector<std::string> list(std::string& error);
            static bool save(const LayoutSnapshot& snapshot, std::string& error);
            static bool load(const std::string& name, LayoutSnapshot& snapshot, std::string& error);
            static bool remove(const std::string& name, std::string& error);

        private:
            static std::vector<LayoutSnapshot> readAll(std::string& error);
            static bool writeAll(const std::vector<LayoutSnapshot>& layouts, std::string& error);
    };
    //<<
    //!
}
