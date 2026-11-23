#include "pty.hpp"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace nexyth
{
    /*>
        Helper Functions
    */
    namespace
    {
        /*!
            Split Command

            !_method

            ----
        */
        //>>
        std::vector<std::string> splitCommand(const std::string& command)
        {
            std::istringstream stream(command);
            std::vector<std::string> result;
            std::string part;
            while (stream >> part) result.push_back(part);
            return result;
        }
        //<<
        //!
    }
    //!

    /*!
        Pty Destructor

        !_method
    */
    Pty::~Pty()
    {
        if (pid_ > 0 && !reaped_)
        {
            kill(pid_, SIGHUP);
            kill(pid_, SIGTERM);
            int status = 0;

            for (int i = 0; i < 20; ++i)
            {
                if (waitpid(pid_, &status, WNOHANG) != 0) break;
                usleep(20000);
            }

            if (!reaped_) kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
        if (master_ >= 0) close(master_);
    }
    //!

    /*!
        Spawn Pty

        !_method
    */
    bool Pty::spawn(const std::string& command, int columns, int rows)
    {
        if (master_ >= 0) return false;

        columns_ = std::max(1, columns);
        rows_ = std::max(1, rows);
        master_ = posix_openpt(O_RDWR | O_NOCTTY);
        if (master_ < 0) return false;

        if (grantpt(master_) != 0 || unlockpt(master_) != 0)
        {
            close(master_);
            master_ = -1;
            return false;
        }

        const char* slaveName = ptsname(master_);

        if (slaveName == nullptr)
        {
            close(master_);
            master_ = -1;
            return false;
        }

        pid_ = fork();

        if (pid_ < 0)
        {
            close(master_);
            master_ = -1;
            return false;
        }

        if (pid_ == 0)
        {
            close(master_);
            setsid();
            const int slave = open(slaveName, O_RDWR);

            if (slave < 0) _exit(127);

            ioctl(slave, TIOCSCTTY, 0);
            struct winsize size {};
            size.ws_col = static_cast<unsigned short>(columns_);
            size.ws_row = static_cast<unsigned short>(rows_);
            ioctl(slave, TIOCSWINSZ, &size);
            dup2(slave, STDIN_FILENO);
            dup2(slave, STDOUT_FILENO);
            dup2(slave, STDERR_FILENO);

            if (slave > STDERR_FILENO) close(slave);

            setenv("TERM", "xterm-256color", 1);
            setenv("NEXYTH", "1", 1);
            const auto arguments = splitCommand(command);
            std::vector<char*> argv;

            for (const auto& argument : arguments)
            {
                argv.push_back(const_cast<char*>(argument.c_str()));
            }

            argv.push_back(nullptr);
            if (!argv.empty() && argv[0] != nullptr) execvp(argv[0], argv.data());
            _exit(127);
        }

        const int flags = fcntl(master_, F_GETFL, 0);

        if (flags >= 0) fcntl(master_, F_SETFL, flags | O_NONBLOCK);

        struct winsize size {};
        size.ws_col = static_cast<unsigned short>(columns_);
        size.ws_row = static_cast<unsigned short>(rows_);
        ioctl(master_, TIOCSWINSZ, &size);

        return true;
    }
    //!

    /*!
        Respond To Queries

        !_method
    */
    void Pty::respondToQueries(const std::string& data)
    {
        if (master_ < 0 || data.empty()) return;

        const auto respond = [this, &data](const std::string& query,
        const std::string& response)
        {
            if (data.find(query) != std::string::npos) write(response);
        };

        respond("\x1B[?u", "\x1B[?0u");
        respond("\x1B[u", "\x1B[?0u");
        respond("\x1B[0c", "\x1B[?62;1;2;4;6;9;15;22c");
        respond("\x1B[c", "\x1B[?62;1;2;4;6;9;15;22c");
        respond("\x1B]11;?\x07", "\x1B]11;rgb:0000/0000/0000\x07");
        respond(std::string("\x1B") + "]11;?" + "\x1B" + "\\",
        std::string("\x1B") + "]11;rgb:0000/0000/0000" + "\x1B" + "\\");
        respond("\x1B[6n", "\x1B[1;1R");
        respond("\x1B[18t", "\x1B[8;" + std::to_string(rows_) + ";" +
        std::to_string(columns_) + "t");
        respond("\x1B[16t", "\x1B[4;" + std::to_string(rows_ * 16) + ";" +
        std::to_string(columns_ * 8) + "t");
        respond("\x1B[>q", std::string("\x1B") + "P>q" + "\x1B" + "\\");

        const std::string decrqss = std::string("\x1B") + "P$q" + "\x1B" + "\\";

        if (data.find(decrqss) != std::string::npos)
        {
            write(std::string("\x1B") + "P1$r" + "\x1B" + "\\");
        }

        std::size_t pos = 0;

        while ((pos = data.find("\x1B_G", pos)) != std::string::npos)
        {
            const std::size_t end = data.find(std::string("\x1B") + "\\", pos);
            if (end == std::string::npos) break;
            const std::string packet = data.substr(pos, end - pos);

            if (packet.find("a=q") != std::string::npos)
            {
                write(std::string("\x1B") + "_Gi=31;OK" + "\x1B" + "\\");
            }
            pos = end + 2;
        }
    }
    //!

    /*!
        Write

        !_method
    */
    void Pty::write(const std::string& data)
    {
        if (master_ < 0 || data.empty()) return;

        std::size_t written = 0;

        while (written < data.size())
        {
            const ssize_t count = ::write(master_, data.data() + written,
            data.size() - written);
            if (count > 0)
            {
                written += static_cast<std::size_t>(count);
                continue;
            }

            if (count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            break;
        }
    }
    //!

    /*!
        Read Available

        !_method
    */
    std::string Pty::readAvailable()
    {
        if (master_ < 0) return "";

        std::string result;
        char buffer[8192];

        while (true)
        {
            const ssize_t count = read(master_, buffer, sizeof(buffer));
        
            if (count > 0)
            {
                result.append(buffer, static_cast<std::size_t>(count));
                continue;
            }

            if (count == 0) break;
            if (errno == EINTR) continue;
            break;
        }
        respondToQueries(result);
        return result;
    }
    //!

    /*!
        Resize

        !_method
    */
    void Pty::resize(int columns, int rows)
    {
        if (master_ < 0) return;

        columns_ = std::max(1, columns);
        rows_ = std::max(1, rows);
        struct winsize size {};
        size.ws_col = static_cast<unsigned short>(columns_);
        size.ws_row = static_cast<unsigned short>(rows_);
        ioctl(master_, TIOCSWINSZ, &size);

        if (pid_ > 0) kill(pid_, SIGWINCH);
    }
    //!

    /*!
        Alive

        !_method
    */
    bool Pty::alive()
    {
        if (pid_ <= 0 || reaped_) return false;

        int status = 0;
        const pid_t result = waitpid(pid_, &status, WNOHANG);

        if (result == pid_)
        {
            reaped_ = true;
            return false;
        }

        if (result < 0 && errno == ECHILD)
        {
            reaped_ = true;
            return false;
        }

        return true;
    }
    //!
}
