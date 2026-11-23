#pragma once

#include <sys/types.h>
#include <atomic>
#include <string>

namespace nexyth
{
    /*!
        Pty Class
    */
    class Pty
    {
        /*!
            Public Members
        */
        public:
            //>>
            Pty() = default;
            ~Pty();
            Pty(const Pty&) = delete;
            Pty& operator=(const Pty&) = delete;

            bool spawn(const std::string& command, int columns, int rows);
            void write(const std::string& data);
            std::string readAvailable();
            void resize(int columns, int rows);
            bool alive();
            bool hasProcess() const { return pid_ > 0; }
            //<<
        //!

        /*!
            Private Members
        */
        private:
            //>>
            void respondToQueries(const std::string& data);

            int master_ = -1;
            pid_t pid_ = -1;
            int columns_ = 80;
            int rows_ = 24;
            std::atomic<bool> reaped_{false};
            //<<
        //!
    };
    //!
}
