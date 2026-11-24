#include "keys.hpp"

#include <algorithm>
#include <cctype>

namespace nexyth
{
    namespace keys
    {
        /*!
            Translate the Key Code

            !_method
        */
        std::string normalizeToken(const std::string& raw)
        {
            if (raw.empty()) return "";
            if (raw == "\x1B" || raw == "\x1B[27;5;27~" || raw == "\x1B[27;5;27u" || raw == "\x1B[27;5:27u" || raw == "\x1B[27;5;27^")
            {
                return "<Esc>";
            }
            if (raw == "\x1B\x1B" || raw == "\x1B[27;5;27;27~") return "<C-Esc>";
            if (raw == "\t") return "<Tab>";
            if (raw == "\r") return "<CR>";
            if (raw == "\x7F") return "<BS>";
            if (raw == " ") return "<Space>";
            if (raw == "\0" || raw == std::string(1, '\0')) return "<C-Space>";
            if (raw == "\x1B[A") return "<Up>";
            if (raw == "\x1B[B") return "<Down>";
            if (raw == "\x1B[C") return "<Right>";
            if (raw == "\x1B[D") return "<Left>";
            if (raw == "\x1B[H") return "<Home>";
            if (raw == "\x1B[F") return "<End>";
            if (raw == "\x1B[1~") return "<Home>";
            if (raw == "\x1B[4~") return "<End>";
            if (raw == "\x1B[3~") return "<Delete>";
            if (raw == "\x1BOP") return "<F1>";
            if (raw == "\x1BOQ") return "<F2>";
            if (raw == "\x1BOR") return "<F3>";
            if (raw == "\x1BOS") return "<F4>";
            if (raw.size() == 3 && raw[0] == '\x1B' && raw[1] == '[' && raw[2] >= 'A' && raw[2] <= 'Z')
            {
                return "<M-" + std::string(1, static_cast<char>(std::tolower(raw[2]))) + ">";
            }
            if (raw.size() == 1) {
                const auto byte = static_cast<unsigned char>(raw[0]);

                if (byte < 0x20 && byte != 0)
                {
                    return "<C-" + std::string(1, static_cast<char>('a' + byte - 1)) + ">";
                }
                if (byte >= 0x80) return "";
            }
            return raw;
        }
        //!

        /*!
            Translate the Leader Key Code

            !_method
        */
        std::string expandLeader(const std::string& token, const std::string& leader)
        {
            const std::string prefix = "<Leader>";

            if (token.size() >= prefix.size() && token.substr(0, prefix.size()) == prefix)
            {
                return leader + token.substr(prefix.size());
            }
            return token;
        }
        //!
    }
}
