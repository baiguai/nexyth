#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nexyth
{
    /*!
        Forward Declarations

    */
    //>>
    std::string trim(std::string_view value);
    std::string lower(std::string value);
    std::vector<std::string> tokenizeConfigLine(std::string_view line);
    std::string joinTokens(const std::vector<std::string>& tokens, const std::string& separator);
    std::string base64Encode(std::string_view data);
    bool utf8Decode(std::string_view data, std::size_t& position, std::uint32_t& codepoint);
    int codepointWidth(std::uint32_t codepoint);
    std::string executableDir();
    std::string expandHome(std::string path);
    //<<
    //!
}
