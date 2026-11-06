// SimmyLauncher: the game's process (see game_process.hpp).
#include "game_process.hpp"

#include <SDL3/SDL_misc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace launcher {

namespace {

#ifdef _WIN32
std::wstring Widen(const std::string& s) {
  if (s.empty()) return std::wstring();
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
  return w;
}

std::string Narrow(const std::wstring& w) {
  if (w.empty()) return std::string();
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
  return s;
}

// One argument quoted the way CommandLineToArgvW / the MSVC runtime split it again.
void AppendQuoted(std::wstring& cmd, const std::wstring& arg) {
  if (!cmd.empty()) cmd += L' ';
  if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    cmd += arg;
    return;
  }
  cmd += L'"';
  for (size_t i = 0;; ++i) {
    size_t slashes = 0;
    while (i < arg.size() && arg[i] == L'\\') ++i, ++slashes;
    if (i == arg.size()) {
      cmd.append(slashes * 2, L'\\');  // before the closing quote
      break;
    }
    if (arg[i] == L'"') {
      cmd.append(slashes * 2 + 1, L'\\');
      cmd += L'"';
    } else {
      cmd.append(slashes, L'\\');
      cmd += arg[i];
    }
  }
  cmd += L'"';
}

std::string LastError() {
  wchar_t* text = nullptr;
  const DWORD code = GetLastError();
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                 code, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
  std::string msg = text ? Narrow(text) : std::string();
  if (text) LocalFree(text);
  while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r' || msg.back() == ' ')) msg.pop_back();
  char num[32];
  std::snprintf(num, sizeof num, " (error %lu)", static_cast<unsigned long>(code));
  return msg + num;
}

HANDLE OpenLog(const std::string& path, SECURITY_ATTRIBUTES* sa) {
  return CreateFileW(Widen(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, sa,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

const char* WindowsCrashName(unsigned long code) {
  switch (code) {
  case 0xC0000005ul: return "the game crashed (access violation)";
  case 0xC00000FDul: return "the game crashed (stack overflow)";
  case 0xC0000409ul: return "the game stopped itself (fail-fast / stack buffer check)";
  case 0xC0000374ul: return "the game crashed (heap corruption)";
  case 0xC000001Dul: return "the game crashed (illegal instruction: an older CPU?)";
  case 0xC0000135ul: return "a DLL the game needs is missing";
  case 0xC0000139ul: return "a DLL the game needs is the wrong version (entry point not found)";
  case 0xC0000142ul: return "a DLL the game needs failed to start";
  case 0x80000003ul: return "the game hit a breakpoint";
  case 0xC000013Aul: return "the game was interrupted (Ctrl+C)";
  case 3ul: return "the game aborted";
  default: return "";
  }
}
#endif

}  // namespace

std::string describe_command(const LaunchRequest& r) {
  std::string s;
  for (const auto& kv : r.env)
    if (!kv.second.empty()) s += kv.first + "=" + kv.second + " ";
  s += "\"" + r.exe + "\"";
  for (const std::string& a : r.args) s += a.find(' ') != std::string::npos ? " \"" + a + "\"" : " " + a;
  return s;
}

GameProcess::~GameProcess() {
#ifdef _WIN32
  if (process_) CloseHandle(static_cast<HANDLE>(process_));  // the game itself keeps running
#endif
}

#ifdef _WIN32

bool GameProcess::start(const LaunchRequest& r, std::string& error) {
  if (running_) return error = "The game is already running.", false;
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof sa;
  sa.bInheritHandle = TRUE;
  HANDLE out = OpenLog(r.stdoutPath, &sa);
  HANDLE err = OpenLog(r.stderrPath, &sa);
  HANDLE in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
  if (out == INVALID_HANDLE_VALUE || err == INVALID_HANDLE_VALUE) {
    error = "Couldn't create the game's log file " + (out == INVALID_HANDLE_VALUE ? r.stdoutPath : r.stderrPath) + ": " +
            LastError();
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (err != INVALID_HANDLE_VALUE) CloseHandle(err);
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    return false;
  }

  // Only these three handles are inherited.
  HANDLE inherit[3] = {in, out, err};
  const DWORD inheritCount = in == INVALID_HANDLE_VALUE ? 2 : 3;
  if (in == INVALID_HANDLE_VALUE) inherit[0] = out, inherit[1] = err;
  SIZE_T attrSize = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
  std::vector<unsigned char> attrBuffer(attrSize ? attrSize : 1);  // operator new: suitably aligned
  auto* attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuffer.data());
  bool haveAttrs = InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize) != FALSE;
  if (haveAttrs && !UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
                                              inheritCount * sizeof(HANDLE), nullptr, nullptr)) {
    DeleteProcThreadAttributeList(attrs);
    haveAttrs = false;
  }

  STARTUPINFOEXW si{};
  si.StartupInfo.cb = haveAttrs ? sizeof si : sizeof si.StartupInfo;
  si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  si.StartupInfo.hStdInput = in == INVALID_HANDLE_VALUE ? nullptr : in;
  si.StartupInfo.hStdOutput = out;
  si.StartupInfo.hStdError = err;
  si.lpAttributeList = haveAttrs ? attrs : nullptr;

  const std::wstring exe = Widen(r.exe);
  std::wstring cmd;
  AppendQuoted(cmd, exe);
  for (const std::string& a : r.args) AppendQuoted(cmd, Widen(a));
  std::wstring cwd = Widen(r.workingDir);

  // The child inherits this process's environment at creation: set the variables for the call, then
  // put the previous values back.
  std::vector<std::pair<std::wstring, std::wstring>> saved;
  std::vector<bool> hadValue;
  for (const auto& kv : r.env) {
    const std::wstring name = Widen(kv.first);
    wchar_t buf[32768];
    const DWORD n = GetEnvironmentVariableW(name.c_str(), buf, 32768);
    hadValue.push_back(n > 0 && n < 32768);
    saved.emplace_back(name, hadValue.back() ? std::wstring(buf, n) : std::wstring());
    SetEnvironmentVariableW(name.c_str(), kv.second.empty() ? nullptr : Widen(kv.second).c_str());
  }

  PROCESS_INFORMATION pi{};
  // CREATE_NO_WINDOW: simmy.exe is a console program; its output goes to the logs, so no console
  // window opens. The game is not put in a job, so it keeps running if the launcher is closed.
  const DWORD flags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | (haveAttrs ? EXTENDED_STARTUPINFO_PRESENT : 0);
  const BOOL ok = CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, TRUE, flags, nullptr,
                                 cwd.empty() ? nullptr : cwd.c_str(), &si.StartupInfo, &pi);
  if (!ok) error = "Couldn't start " + r.exe + ": " + LastError();

  for (size_t i = 0; i < saved.size(); ++i)
    SetEnvironmentVariableW(saved[i].first.c_str(), hadValue[i] ? saved[i].second.c_str() : nullptr);
  if (haveAttrs) DeleteProcThreadAttributeList(attrs);
  CloseHandle(out);
  CloseHandle(err);
  if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
  if (!ok) return false;
  CloseHandle(pi.hThread);
  process_ = pi.hProcess;
  running_ = true;
  return true;
}

bool GameProcess::poll(int& exitCode, std::string& description) {
  if (!running_ || !process_) return false;
  if (WaitForSingleObject(static_cast<HANDLE>(process_), 0) != WAIT_OBJECT_0) return false;
  DWORD code = 0;
  GetExitCodeProcess(static_cast<HANDLE>(process_), &code);
  CloseHandle(static_cast<HANDLE>(process_));
  process_ = nullptr;
  running_ = false;
  exitCode = static_cast<int>(code);
  description = WindowsCrashName(code);
  return true;
}

void GameProcess::kill() {
  if (running_ && process_) TerminateProcess(static_cast<HANDLE>(process_), 1);
}

void open_folder(const std::string& path) {
  ShellExecuteW(nullptr, L"open", Widen(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

#else  // POSIX

bool GameProcess::start(const LaunchRequest& r, std::string& error) {
  if (running_) return error = "The game is already running.", false;
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_addopen(&actions, 1, r.stdoutPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  posix_spawn_file_actions_addopen(&actions, 2, r.stderrPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
#ifdef POSIX_SPAWN_SETSID
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);  // its own session: survives the launcher's terminal
#endif

  std::vector<std::string> argStore;
  argStore.push_back(r.exe);
  argStore.insert(argStore.end(), r.args.begin(), r.args.end());
  std::vector<char*> argv;
  for (std::string& a : argStore) argv.push_back(&a[0]);
  argv.push_back(nullptr);

  // Environment and working directory for the call, restored afterwards.
  std::vector<std::pair<std::string, std::string>> saved;
  std::vector<bool> hadValue;
  for (const auto& kv : r.env) {
    const char* old = std::getenv(kv.first.c_str());
    hadValue.push_back(old != nullptr);
    saved.emplace_back(kv.first, old ? old : "");
    if (kv.second.empty()) unsetenv(kv.first.c_str());
    else setenv(kv.first.c_str(), kv.second.c_str(), 1);
  }
  char oldCwd[PATH_MAX] = "";
  const bool haveCwd = getcwd(oldCwd, sizeof oldCwd) != nullptr;
  if (!r.workingDir.empty() && chdir(r.workingDir.c_str()) != 0)
    std::fprintf(stderr, "[launcher] cannot enter %s: %s\n", r.workingDir.c_str(), std::strerror(errno));

  pid_t pid = -1;
  const int rc = posix_spawn(&pid, r.exe.c_str(), &actions, &attr, argv.data(), environ);

  if (haveCwd && chdir(oldCwd) != 0) std::fprintf(stderr, "[launcher] cannot return to %s\n", oldCwd);
  for (size_t i = 0; i < saved.size(); ++i) {
    if (hadValue[i]) setenv(saved[i].first.c_str(), saved[i].second.c_str(), 1);
    else unsetenv(saved[i].first.c_str());
  }
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attr);
  if (rc != 0) {
    error = "Couldn't start " + r.exe + ": " + std::strerror(rc);
    return false;
  }
  pid_ = pid;
  running_ = true;
  return true;
}

bool GameProcess::poll(int& exitCode, std::string& description) {
  if (!running_ || pid_ <= 0) return false;
  int status = 0;
  const pid_t done = waitpid(pid_, &status, WNOHANG);
  if (done == 0) return false;
  running_ = false;
  pid_ = -1;
  description.clear();
  if (done < 0) {
    exitCode = -1;
    description = "lost track of the game";
  } else if (WIFEXITED(status)) {
    exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    const int sig = WTERMSIG(status);
    exitCode = 128 + sig;
    description = sig == SIGSEGV   ? "the game crashed (segmentation fault)"
                  : sig == SIGABRT ? "the game aborted"
                  : sig == SIGKILL ? "the game was killed"
                  : sig == SIGBUS  ? "the game crashed (bus error)"
                  : sig == SIGILL  ? "the game crashed (illegal instruction)"
                  : sig == SIGFPE  ? "the game crashed (arithmetic error)"
                                   : "the game was stopped by a signal";
  }
  return true;
}

void GameProcess::kill() {
  if (running_ && pid_ > 0) ::kill(pid_, SIGKILL);
}

void open_folder(const std::string& path) {
  std::string url = "file://" + path;
  if (!SDL_OpenURL(url.c_str())) std::fprintf(stderr, "[launcher] cannot open %s\n", url.c_str());
}

#endif

}  // namespace launcher
