#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace nexyth
{
    /*!
        Buffer Class

    */
    //>>
    class Buffer;
    //<<
    //!

    /*!
        Vte Class
    */
    class Vte
    {
        /*!
            Public Members
        */
        public:
            //>>
            explicit Vte(Buffer& buffer);
            void feed(std::string_view data);
            //<<
        //!

        /*!
            Private Members
        */
        private:
            //>>
            enum class State
            {
                Ground,
                Escape,
                EscapeIntermediate,
                Csi,
                Osc,
                OscEscape,
                EscapeString,
                EscapeStringEscape
            };

            Buffer& buffer_;
            State state_ = State::Ground;
            std::string parameters_;
            std::string osc_;
            std::string utf8_pending_;
            int utf8_expected_ = 1;

            void handleGround(unsigned char byte);
            void handleCsi(unsigned char final);
            void handleSgr(const std::vector<int>& values);
            void moveCursor(int dx, int dy);
            void setCursor(int x, int y);
            void resetStyle();
            //<<
        //!
    };
    //!
}
