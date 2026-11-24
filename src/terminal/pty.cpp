#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006
#endif
#endif

#include "pty.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace nexyth {
namespace {

#ifndef _WIN32
std::vector<std::string> splitCommand(const std::string& command) {
  std::istringstream stream(command);
  std::vector<std::string> result;
  std::string part;
  while (stream >> part) result.push_back(part);
  return result;
}
#endif

#ifdef _WIN32
void closeHandle(void*& handle) {
  if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
    CloseHandle(static_cast<HANDLE>(handle));
  }
  handle = INVALID_HANDLE_VALUE;
}

std::wstring utf8ToWide(const std::string& value) {
  if (value.empty() ||
      value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return L"";
  }
  const int length =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) return L"";
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(),
                          length) != length) {
    return L"";
  }
  return result;
}

COORD consoleSize(int columns, int rows) {
  COORD size{};
  size.X = static_cast<SHORT>(std::min(
      std::max(1, columns), static_cast<int>(std::numeric_limits<SHORT>::max())));
  size.Y = static_cast<SHORT>(std::min(
      std::max(1, rows), static_cast<int>(std::numeric_limits<SHORT>::max())));
  return size;
}
#endif

}  // namespace

/*!
    Pty Destructor

    !_method
*/
Pty::~Pty() {
#ifdef _WIN32
  if (process_ != nullptr) {
    if (WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) {
      TerminateProcess(process_, 1);
    }
    WaitForSingleObject(process_, INFINITE);
    CloseHandle(process_);
    process_ = nullptr;
  }
  if (thread_ != nullptr) {
    CloseHandle(thread_);
    thread_ = nullptr;
  }
  if (console_ != nullptr) {
    ClosePseudoConsole(console_);
    console_ = nullptr;
  }
  closeHandle(inputWrite_);
  closeHandle(outputRead_);
  reaped_ = true;
#else
  if (pid_ > 0 && !reaped_) {
    kill(pid_, SIGHUP);
    kill(pid_, SIGTERM);
    int status = 0;
    for (int i = 0; i < 20; ++i) {
      if (waitpid(pid_, &status, WNOHANG) != 0) break;
      usleep(20000);
    }
    if (!reaped_) kill(pid_, SIGKILL);
    waitpid(pid_, &status, 0);
  }
  if (master_ >= 0) close(master_);
#endif
}

bool Pty::spawn(const std::string& command, int columns, int rows,
                const std::string& cwd) {
#ifdef _WIN32
  if (console_ != nullptr || process_ != nullptr) return false;
  columns_ = std::max(1, columns);
  rows_ = std::max(1, rows);

  SECURITY_ATTRIBUTES attributes{};
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = TRUE;

  HANDLE inputRead = INVALID_HANDLE_VALUE;
  HANDLE inputWrite = INVALID_HANDLE_VALUE;
  HANDLE outputRead = INVALID_HANDLE_VALUE;
  HANDLE outputWrite = INVALID_HANDLE_VALUE;
  if (!CreatePipe(&inputRead, &inputWrite, &attributes, 0)) return false;
  if (!CreatePipe(&outputRead, &outputWrite, &attributes, 0)) {
    closeHandle(inputRead);
    closeHandle(inputWrite);
    return false;
  }
  SetHandleInformation(inputWrite, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0);

  HPCON console = nullptr;
  if (FAILED(CreatePseudoConsole(consoleSize(columns_, rows), inputRead,
                                 outputWrite, 0, &console))) {
    closeHandle(inputRead);
    closeHandle(inputWrite);
    closeHandle(outputRead);
    closeHandle(outputWrite);
    return false;
  }

  SIZE_T attributeSize = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
  std::vector<char> attributeBuffer(attributeSize);
  auto* attributeList =
      reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
  bool attributesInitialized =
      InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeSize) != 0;
  const auto fail = [&]() {
    if (attributesInitialized) DeleteProcThreadAttributeList(attributeList);
    ClosePseudoConsole(console);
    closeHandle(inputRead);
    closeHandle(inputWrite);
    closeHandle(outputRead);
    closeHandle(outputWrite);
    return false;
  };
  if (!attributesInitialized) return fail();
  if (!UpdateProcThreadAttribute(attributeList, 0,
                                  PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console,
                                  sizeof(console), nullptr, nullptr)) {
    return fail();
  }


  const std::wstring wideCommand = utf8ToWide(command);
  if (wideCommand.empty()) return fail();
  const std::wstring wideCwd = cwd.empty() ? std::wstring() : utf8ToWide(cwd);
  std::vector<wchar_t> commandLine(wideCommand.begin(), wideCommand.end());
  commandLine.push_back(L'\0');
  _putenv_s("TERM", "xterm-256color");
  _putenv_s("NEXYTH", "1");

  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = nullptr;
  startup.StartupInfo.hStdOutput = nullptr;
  startup.StartupInfo.hStdError = nullptr;
  startup.lpAttributeList = attributeList;
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessW(
      nullptr, commandLine.data(), nullptr, nullptr, FALSE,
      EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, nullptr,
      wideCwd.empty() ? nullptr : wideCwd.c_str(), &startup.StartupInfo,
      &process);
  if (!created) return fail();

  DeleteProcThreadAttributeList(attributeList);
  CloseHandle(inputRead);
  CloseHandle(outputWrite);
  CloseHandle(process.hThread);
  console_ = console;
  process_ = process.hProcess;
  thread_ = nullptr;
  inputWrite_ = inputWrite;
  outputRead_ = outputRead;
  reaped_ = false;
  return true;
#else
  if (master_ >= 0) return false;
  columns_ = std::max(1, columns);
  rows_ = std::max(1, rows);
  master_ = posix_openpt(O_RDWR | O_NOCTTY);
  if (master_ < 0) return false;
  if (grantpt(master_) != 0 || unlockpt(master_) != 0) {
    close(master_);
    master_ = -1;
    return false;
  }
  const char* slaveName = ptsname(master_);
  if (slaveName == nullptr) {
    close(master_);
    master_ = -1;
    return false;
  }
  pid_ = fork();
  if (pid_ < 0) {
    close(master_);
    master_ = -1;
    return false;
  }
  if (pid_ == 0) {
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
    if (!cwd.empty())
    {
      const int changed = chdir(cwd.c_str());
      (void)changed;
    }
    const auto arguments = splitCommand(command);
    std::vector<char*> argv;
    for (const auto& argument : arguments) {
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
#endif
}

void Pty::respondToQueries(const std::string& data) {
  if (!hasProcess() || data.empty()) return;
  const auto respond = [this, &data](const std::string& query,
                                     const std::string& response) {
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
  if (data.find(decrqss) != std::string::npos) {
    write(std::string("\x1B") + "P1$r" + "\x1B" + "\\");
  }
  std::size_t pos = 0;
  while ((pos = data.find("\x1B_G", pos)) != std::string::npos) {
    const std::size_t end = data.find(std::string("\x1B") + "\\", pos);
    if (end == std::string::npos) break;
    const std::string packet = data.substr(pos, end - pos);
    if (packet.find("a=q") != std::string::npos) {
      write(std::string("\x1B") + "_Gi=31;OK" + "\x1B" + "\\");
    }
    pos = end + 2;
  }
}

void Pty::write(const std::string& data) {
#ifdef _WIN32
  if (inputWrite_ == INVALID_HANDLE_VALUE || data.empty()) return;
  const std::size_t chunkLimit =
      static_cast<std::size_t>(std::numeric_limits<DWORD>::max());
  std::size_t writtenTotal = 0;
  while (writtenTotal < data.size()) {
    DWORD written = 0;
    const std::size_t chunk = std::min(data.size() - writtenTotal, chunkLimit);
    if (!WriteFile(inputWrite_, data.data() + writtenTotal,
                   static_cast<DWORD>(chunk), &written, nullptr) ||
        written == 0) {
      break;
    }
    writtenTotal += written;
  }
#else
  if (master_ < 0 || data.empty()) return;
  std::size_t written = 0;
  while (written < data.size()) {
    const ssize_t count = ::write(master_, data.data() + written,
                                  data.size() - written);
    if (count > 0) {
      written += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
    break;
  }
#endif
}

std::string Pty::readAvailable() {
#ifdef _WIN32
  if (outputRead_ == INVALID_HANDLE_VALUE) return "";
  std::string result;
  char buffer[8192];
  while (true) {
    DWORD available = 0;
    if (!PeekNamedPipe(outputRead_, nullptr, 0, nullptr, &available, nullptr) ||
        available == 0) {
      break;
    }
    DWORD read = 0;
    const DWORD requested =
        std::min(available, static_cast<DWORD>(sizeof(buffer)));
    if (!ReadFile(outputRead_, buffer, requested, &read, nullptr) || read == 0) {
      break;
    }
    result.append(buffer, read);
  }
  respondToQueries(result);
  return result;
#else
  if (master_ < 0) return "";
  std::string result;
  char buffer[8192];
  while (true) {
    const ssize_t count = read(master_, buffer, sizeof(buffer));
    if (count > 0) {
      result.append(buffer, static_cast<std::size_t>(count));
      continue;
    }
    if (count == 0) break;
    if (errno == EINTR) continue;
    break;
  }
  respondToQueries(result);
  return result;
#endif
}

void Pty::resize(int columns, int rows) {
#ifdef _WIN32
  if (console_ == nullptr) return;
  columns_ = std::max(1, columns);
  rows_ = std::max(1, rows);
  ResizePseudoConsole(console_, consoleSize(columns_, rows));
#else
  if (master_ < 0) return;
  columns_ = std::max(1, columns);
  rows_ = std::max(1, rows);
  struct winsize size {};
  size.ws_col = static_cast<unsigned short>(columns_);
  size.ws_row = static_cast<unsigned short>(rows_);
  ioctl(master_, TIOCSWINSZ, &size);
  if (pid_ > 0) kill(pid_, SIGWINCH);
#endif
}

bool Pty::alive() {
#ifdef _WIN32
  if (process_ == nullptr || reaped_) return false;
  const DWORD status = WaitForSingleObject(process_, 0);
  if (status != WAIT_TIMEOUT) {
    reaped_ = true;
    return false;
  }
  return true;
#else
  if (pid_ <= 0 || reaped_) return false;
  int status = 0;
  const pid_t result = waitpid(pid_, &status, WNOHANG);
  if (result == pid_) {
    reaped_ = true;
    return false;
  }
  if (result < 0 && errno == ECHILD) {
    reaped_ = true;
    return false;
  }
  return true;
#endif
}

/*!
    Working Directory

    Best-effort lookup of the child process' current directory. Linux can
    read it from procfs; other platforms leave it empty and use the app's
    working directory when the pane is recreated.

    !_method
*/
std::string Pty::workingDirectory() const {
#if defined(__linux__)
  if (pid_ <= 0) return "";

  char linkPath[64];
  std::snprintf(linkPath, sizeof(linkPath), "/proc/%d/cwd",
                static_cast<int>(pid_));

  std::vector<char> buffer(4096);
  const ssize_t length =
      readlink(linkPath, buffer.data(), buffer.size() - 1);
  if (length <= 0 || length >= static_cast<ssize_t>(buffer.size() - 1)) {
    return "";
  }

  buffer[static_cast<std::size_t>(length)] = '\0';
  const std::string result(buffer.data());
  return result.empty() || result[0] != '/' ? "" : result;
#else
  return "";
#endif
}
}  // namespace nexyth
