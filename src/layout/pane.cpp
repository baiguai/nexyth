#include "pane.hpp"

#include <chrono>

namespace nexyth
{
    /*!
        Pane Constructor

        !_ctor
    */
    Pane::Pane(int id, std::string command, int columns, int rows, int scrollback, std::function<void(int)> onData)
        : id_(id), command_(std::move(command)), buffer_(columns, rows, scrollback), onData_(std::move(onData)) {}
    //!

    /*!
        Pane Destructor

        !_method

        ----
    */
    //>>
    Pane::~Pane()
    {
        running_ = false;
        if (reader_.joinable()) reader_.join();
    }
    //<<
    //!

    /*!
        Start

        !_method

        ----
    */
    //>>
    void Pane::start()
    {
        if (reader_.joinable()) return;

        if (!pty_.spawn(command_, buffer_.columns(), buffer_.rows()))
        {
            alive_ = false;
            setTitle("[failed]");
            return;
        }

        reader_ = std::thread([this] { readLoop(); });
    }
    //<<
    //!

    /*!
        Write

        !_method

        ----
    */
    //>>
    void Pane::write(const std::string& data)
    {
        pty_.write(data);
    }
    //<<
    //!


    /*!
        Resize

        !_method
    */
    void Pane::resize(int columns, int rows)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            buffer_.resize(columns, rows);
        }
        pty_.resize(columns, rows);
    }
    //!

    /*!
        Pane Tile

        !_method
    */
    std::string Pane::title() const
    {
        std::lock_guard<std::mutex> lock(title_mutex_);
        return title_.empty() ? command_ : title_;
    }
    //!


    /*!
        Set Title

        !_method
    */
    void Pane::setTitle(std::string title)
    {
        std::lock_guard<std::mutex> lock(title_mutex_);
        title_ = std::move(title);
    }
    //!

    /*!
        Alive Bool

        !_method
    */
    bool Pane::alive() const
    {
        return alive_ && pty_.hasProcess();
    }
    //!

    /*!
        Read Loop

        !_method
    */
    void Pane::readLoop()
    {
        while (running_)
        {
            const std::string data = pty_.readAvailable();

            if (!data.empty())
            {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    buffer_.feed(data);
                }
                if (onData_) onData_(id_);
            }

            if (!pty_.alive())
            {
                alive_ = false;
                setTitle("[exited]");
                if (onData_) onData_(id_);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }
    //!
}
