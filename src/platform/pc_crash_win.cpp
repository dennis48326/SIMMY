// Windows crash reporter: on a crash (or abort), writes a symbolized stack trace to stderr and to
// simmy_crash.txt next to the executable, plus a small minidump (simmy_crash.dmp), instead of Windows'
// "stopped working" dialog. Installed during static initialisation, before the game's own global objects
// are constructed, so crashes in those are caught too. Uses dbghelp and simmy.pdb (kept next to
// simmy.exe by the build).
//
// Covered (any thread, any phase of the process, including shutdown):
//   - unhandled SEH exceptions (access violation, stack overflow, illegal instruction, ...):
//     SetUnhandledExceptionFilter; re-installed by crash_reinstall_handlers() after aurora/SDL/Dawn
//     have started, in case one of them replaced it;
//   - heap corruption where Windows raises STATUS_HEAP_CORRUPTION: a vectored handler (first chance);
//   - abort() (SIGABRT), std::terminate, pure virtual calls (_set_purecall_handler), CRT invalid
//     parameters (_set_invalid_parameter_handler).
// Not covered: __fastfail / /GS stack-cookie failures (0xC0000409) end the process without running any
// handler; the launcher still reports their exit code.
//
// The report is written by a helper thread: the crashed thread may have no stack left (stack overflow)
// or hold locks. The process then ends with TerminateProcess (exit code: the exception code, or 3 for
// abort / terminate / purecall / invalid parameter).
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <exception>

#pragma comment(lib, "dbghelp.lib")

namespace {
// Path of a file next to the executable.
void ExePath(char* path, const char* name) {
  DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
  while (n > 0 && path[n - 1] != '\\') --n;
  lstrcpynA(path + n, name, static_cast<int>(MAX_PATH - n));
}

FILE* OpenReport() {
  char path[MAX_PATH];
  ExePath(path, "simmy_crash.txt");
  return std::fopen(path, "w");
}

void Out(FILE* f, const char* fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  std::fputs(buf, stderr);
  if (f) std::fputs(buf, f);
}

void WriteStack(FILE* f, CONTEXT ctx, HANDLE thread) {
  HANDLE process = GetCurrentProcess();
  SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
  SymInitialize(process, nullptr, TRUE);
  STACKFRAME64 frame{};
  frame.AddrPC.Offset = ctx.Rip;
  frame.AddrFrame.Offset = ctx.Rbp;
  frame.AddrStack.Offset = ctx.Rsp;
  frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
  for (int i = 0; i < 48; ++i) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr, SymFunctionTableAccess64,
                     SymGetModuleBase64, nullptr) ||
        !frame.AddrPC.Offset)
      break;
    const DWORD64 pc = frame.AddrPC.Offset;
    alignas(SYMBOL_INFO) char symBuf[sizeof(SYMBOL_INFO) + 512];
    auto* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 511;
    DWORD64 symOff = 0;
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof line;
    DWORD lineOff = 0;
    const bool haveSym = SymFromAddr(process, pc, &symOff, sym);
    const bool haveLine = SymGetLineFromAddr64(process, pc, &lineOff, &line);
    char module[MAX_PATH] = "?";
    const DWORD64 base = SymGetModuleBase64(process, pc);
    if (base) GetModuleFileNameA(reinterpret_cast<HMODULE>(base), module, MAX_PATH);
    const char* modName = module;
    for (const char* p = module; *p; ++p)
      if (*p == '\\') modName = p + 1;
    // The module offset too, so a report from another build can still be symbolized offline.
    Out(f, "  #%-2d %s+0x%llx %s+0x%llx", i, modName, static_cast<unsigned long long>(base ? pc - base : pc),
        haveSym ? sym->Name : "?", static_cast<unsigned long long>(symOff));
    if (haveLine) Out(f, "  (%s:%lu)", line.FileName, line.LineNumber);
    Out(f, "\n");
  }
}

void WriteMinidump(EXCEPTION_POINTERS* info, DWORD threadId) {
  char path[MAX_PATH];
  ExePath(path, "simmy_crash.dmp");
  HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  MINIDUMP_EXCEPTION_INFORMATION mei{};
  mei.ThreadId = threadId;
  mei.ExceptionPointers = info;
  mei.ClientPointers = FALSE;
  const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                                    static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo),
                                    info ? &mei : nullptr, nullptr, nullptr);
  CloseHandle(file);
  Out(nullptr, ok ? "[simmy] minidump written: %s\n" : "[simmy] minidump failed: %s\n", path);
}

struct CrashJob {
  const char* what;          // e.g. "exception", "abort()"
  EXCEPTION_POINTERS* info;  // null for abort / terminate / purecall / invalid parameter
  CONTEXT context;           // where it happened
  DWORD threadId;            // the crashed thread
};

void WriteReport(CrashJob& job) {
  FILE* f = OpenReport();
  if (job.info) {
    const EXCEPTION_RECORD* rec = job.info->ExceptionRecord;
    Out(f, "\n[simmy] CRASH: exception 0x%08lx at %p (thread %lu)\n", rec->ExceptionCode, rec->ExceptionAddress,
        job.threadId);
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
      Out(f, "  %s address %p\n", rec->ExceptionInformation[0] ? "writing" : "reading",
          reinterpret_cast<void*>(rec->ExceptionInformation[1]));
  } else {
    Out(f, "\n[simmy] CRASH: %s (thread %lu)\n", job.what, job.threadId);
  }
  HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME, FALSE, job.threadId);
  WriteStack(f, job.context, thread ? thread : GetCurrentThread());
  if (thread) CloseHandle(thread);
  Out(f, "A minidump (simmy_crash.dmp) is next to simmy.exe.\n");
  if (f) std::fclose(f);
  WriteMinidump(job.info, job.threadId);
  std::fflush(nullptr);
}

DWORD WINAPI ReportThread(void* param) {
  WriteReport(*static_cast<CrashJob*>(param));
  return 0;
}

volatile LONG g_crashing = 0;

[[noreturn]] void Crash(const char* what, EXCEPTION_POINTERS* info, const CONTEXT& ctx, UINT exitCode) {
  // One report only: a second crash (another thread, or inside the reporter) waits for the first.
  if (InterlockedExchange(&g_crashing, 1) != 0) {
    Sleep(30000);
    TerminateProcess(GetCurrentProcess(), exitCode);
    for (;;) Sleep(1000);
  }
  static CrashJob job;  // static: the crashed thread's stack may be nearly used up
  job.what = what;
  job.info = info;
  job.context = ctx;
  job.threadId = GetCurrentThreadId();
  if (HANDLE t = CreateThread(nullptr, 256 * 1024, ReportThread, &job, 0, nullptr)) {
    WaitForSingleObject(t, 60000);
    CloseHandle(t);
  } else {
    WriteReport(job);
  }
  TerminateProcess(GetCurrentProcess(), exitCode);
  for (;;) Sleep(1000);
}

[[noreturn]] void CrashHere(const char* what) {
  CONTEXT ctx{};
  RtlCaptureContext(&ctx);
  Crash(what, nullptr, ctx, 3);
}

LONG WINAPI OnCrash(EXCEPTION_POINTERS* info) {
  Crash("exception", info, *info->ContextRecord, info->ExceptionRecord->ExceptionCode);
}

LONG WINAPI OnVectored(EXCEPTION_POINTERS* info) {
  if (info->ExceptionRecord->ExceptionCode == 0xC0000374ul)  // STATUS_HEAP_CORRUPTION
    Crash("heap corruption", info, *info->ContextRecord, 0xC0000374ul);
  return EXCEPTION_CONTINUE_SEARCH;
}

void OnAbort(int) { CrashHere("abort()"); }
void OnTerminate() { CrashHere("std::terminate (uncaught C++ exception or a joinable std::thread destroyed)"); }
void OnPurecall() { CrashHere("pure virtual function call"); }
void OnInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
  CrashHere("invalid parameter passed to a C runtime function");
}

void InstallHandlers() {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  SetUnhandledExceptionFilter(OnCrash);
  std::signal(SIGABRT, OnAbort);
  std::set_terminate(OnTerminate);
  _set_purecall_handler(OnPurecall);
  _set_invalid_parameter_handler(OnInvalidParameter);
}

// SIMMY_WATCHDOG=N: after N seconds, write every thread's call stack (raw addresses, module-relative)
// to simmy_watchdog.txt next to the exe: a timed snapshot for diagnosing hangs (it does not detect one;
// the launcher ignores the file). Avoids the CRT and the loader lock, which a hung thread may hold;
// addresses are symbolized offline against simmy.pdb.
HMODULE g_exeBase = nullptr;
char g_hangPath[MAX_PATH];

void Append(HANDLE file, const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  const int n = wvsprintfA(buf, fmt, ap);
  va_end(ap);
  DWORD written;
  WriteFile(file, buf, static_cast<DWORD>(n), &written, nullptr);
}

void DumpThread(HANDLE file, DWORD tid) {
  HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
  if (!t) return;
  DWORD64 pcs[64];
  int count = 0;
  if (SuspendThread(t) != static_cast<DWORD>(-1)) {
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(t, &ctx)) {
      for (; count < 64 && ctx.Rip; ++count) {
        pcs[count] = ctx.Rip;
        DWORD64 base = 0;
        RUNTIME_FUNCTION* fn = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr);
        if (!fn) {  // leaf function
          ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
          ctx.Rsp += 8;
          continue;
        }
        void* handlerData;
        DWORD64 establisher;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, fn, &ctx, &handlerData, &establisher, nullptr);
      }
    }
    ResumeThread(t);
  }
  CloseHandle(t);
  Append(file, "thread %lu:\r\n", tid);
  for (int i = 0; i < count; ++i) {
    void* base = nullptr;
    RtlPcToFileHeader(reinterpret_cast<void*>(pcs[i]), &base);
    if (base == g_exeBase)
      Append(file, "  simmy.exe+0x%lx\r\n", static_cast<unsigned long>(pcs[i] - reinterpret_cast<DWORD64>(base)));
    else {
      char name[MAX_PATH] = "?";
      if (base) GetModuleFileNameA(static_cast<HMODULE>(base), name, MAX_PATH);  // after resume: no lock held by us
      const char* leaf = name;
      for (const char* c = name; *c; ++c)
        if (*c == '\\') leaf = c + 1;
      Append(file, "  %s+0x%lx\r\n", leaf, static_cast<unsigned long>(base ? pcs[i] - reinterpret_cast<DWORD64>(base) : 0));
    }
  }
}

DWORD WINAPI Watchdog(void* param) {
  Sleep(static_cast<DWORD>(reinterpret_cast<uintptr_t>(param)) * 1000);
  HANDLE file = CreateFileA(g_hangPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (file == INVALID_HANDLE_VALUE) return 0;
  Append(file, "[simmy] WATCHDOG snapshot after %lu s\r\n", static_cast<unsigned long>(reinterpret_cast<uintptr_t>(param)));
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  THREADENTRY32 te{};
  te.dwSize = sizeof te;
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
    if (te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != GetCurrentThreadId())
      DumpThread(file, te.th32ThreadID);
  CloseHandle(snap);
  CloseHandle(file);
  return 0;
}

struct Install {
  Install() {
    {  // PC diagnostics: proves static initialisation was reached (written straight to the console/log).
      DWORD w;
      WriteFile(GetStdHandle(STD_ERROR_HANDLE), "[simmy] static init\r\n", 21, &w, nullptr);
    }
    if (const char* w = std::getenv("SIMMY_WATCHDOG")) {
      g_exeBase = GetModuleHandleA(nullptr);
      ExePath(g_hangPath, "simmy_watchdog.txt");
      CreateThread(nullptr, 0, Watchdog, reinterpret_cast<void*>(static_cast<uintptr_t>(std::atoi(w))), 0, nullptr);
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    InstallHandlers();
    AddVectoredExceptionHandler(1, OnVectored);
  }
};
}  // namespace

namespace simmy {
// Called once the window, GPU and audio are up (pc_main.cpp): SDL, Dawn or a driver may have put in
// their own unhandled-exception filter or CRT handlers.
void crash_reinstall_handlers() { InstallHandlers(); }

// The end of a normal run (platform_shutdown_and_exit, pc_main.cpp): no static destructors, no DLL
// detach code; the launcher sees `code`.
[[noreturn]] void process_exit_now(int code) {
  TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
  ExitProcess(static_cast<UINT>(code));  // not reached
  for (;;) Sleep(1000);
}
}  // namespace simmy

#pragma warning(disable : 4073)
#pragma init_seg(lib)  // before the game's global objects (user segment)
static Install s_install;
#endif
