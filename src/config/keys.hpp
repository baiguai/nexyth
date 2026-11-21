#pragma once

#include <string>

namespace nexyth
{
    namespace keys
    {
        /*!
            Forward Declarations

        */
        //>>
        std::string normalizeToken(const std::string& raw);
        std::string expandLeader(const std::string& token, const std::string& leader);
        //<<
        //!
    }
}
