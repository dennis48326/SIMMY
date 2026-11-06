// SimmyLauncher: starts the game as its own process (CreateProcessW on Windows, posix_spawn
// elsewhere) with its output in log files, and watches for it to end. The game keeps running if the
// launcher is closed.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace launcher {

struct LaunchRequest {
  std::string exe;                    // UTF-8 path of simmy.exe
  std::vector<std::string> args;      // after argv[0]
  std::string workingDir;             // the game's current directory
  std::string stdoutPath, stderrPath; // created / truncated
  // Environment changes: value empty = remove the variable.
  std::vector<std::pair<std::string, std::string>> env;
};

class GameProcess {
 public:
  GameProcess() = default;
  GameProcess(const GameProcess&) = delete;
  GameProcess& operator=(const GameProcess&) = delete;
  ~GameProcess();

  bool start(const LaunchRequest& request, std::string& error);
  bool running() const { return running_; }
  // Checks whether the game has ended; true once it has, with its exit code (on POSIX a signal
  // number is reported as 128 + signal, like a shell). `description` explains a crash code.
  bool poll(int& exitCode, std::string& description);
  // Ends the game at once (the "Force quit" button).
  void kill();

 private:
  bool running_ = false;
#ifdef _WIN32
  void* process_ = nullptr;  // HANDLE
#else
  int pid_ = -1;
#endif
};

// The command line as the game will see it (for the log).
std::string describe_command(const LaunchRequest& request);

// Opens a folder in the desktop's file manager.
void open_folder(const std::string& path);

}  // namespace launcher
