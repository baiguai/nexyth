#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "../terminal/buffer.hpp"
#include "../terminal/pty.hpp"

namespace nexyth
{
    /*!
        Pane Class
    */
    class Pane {
        /*!
            Public Members
        */
        public:
            //>>
            Pane(int id, std::string command, int columns, int rows, int scrollback,
            std::function<void(int)> onData);
            ~Pane();
            Pane(const Pane&) = delete;
            Pane& operator=(const Pane&) = delete;

            void start();
            void write(const std::string& data);
            void resize(int columns, int rows);
            int id() const { return id_; }
            std::string title() const;
            void setTitle(std::string title);
            bool alive() const;
            Buffer& buffer() { return buffer_; }
            std::mutex& mutex() { return mutex_; }
            //<<
        //!

        /*!
            Private Members
        */
        private:
            //>>
            void readLoop();

            int id_ = 0;
            std::string command_;
            Buffer buffer_;
            std::function<void(int)> onData_;
            Pty pty_;
            std::thread reader_;
            std::mutex mutex_;
            mutable std::mutex title_mutex_;
            std::string title_;
            std::atomic<bool> running_{true};
            std::atomic<bool> alive_{true};
            //<<
        //!
    };
    //!
}
