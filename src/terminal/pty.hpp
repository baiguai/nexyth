#pragma once

#ifndef _WIN32
#include <sys/types.h>
#endif

#include <atomic>
#include <cstdint>
#include <string>

namespace nexyth {

class Pty {
 public:
  Pty() = default;
  ~Pty();
  Pty(const Pty&) = delete;
  Pty& operator=(const Pty&) = delete;

  bool spawn(const std::string& command, int columns, int rows);
  void write(const std::string& data);
  std::string readAvailable();
  void resize(int columns, int rows);
  bool alive();
#ifdef _WIN32
  bool hasProcess() const { return process_ != nullptr; }
#else
  bool hasProcess() const { return pid_ > 0; }
#endif

 private:
  void respondToQueries(const std::string& data);

#ifdef _WIN32
  void* console_ = nullptr;
  void* process_ = nullptr;
  void* thread_ = nullptr;
  void* inputWrite_ = reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
  void* outputRead_ = reinterpret_cast<void*>(static_cast<std::intptr_t>(-1));
#else
  int master_ = -1;
  pid_t pid_ = -1;
#endif
  int columns_ = 80;
  int rows_ = 24;
  std::atomic<bool> reaped_{false};
};

}  // namespace nexyth
