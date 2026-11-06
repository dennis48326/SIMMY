// Windows crash reporter: on a crash (or abort), writes a symbolized stack trace to stderr and to
// simmy_crash.txt next to the executable, instead of Windows' "stopped working" dialog. Installed
// during static initialisation, before the game's own global objects are constructed, so crashes in
// those are caught too. Uses dbghelp and simmy.pdb (kept next to simmy.exe by the build).
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

#pragma comment(lib, "dbghelp.lib")

namespace {
FILE* OpenReport() {
  char path[MAX_PATH];
  DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
  while (n > 0 && path[n - 1] != '\\') --n;
  snprintf(path + n, MAX_PATH - n, "simmy_crash.txt");
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

void WriteStack(FILE* f, CONTEXT ctx, HANDLE thread = GetCurrentThread()) {
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
    if (const DWORD64 base = SymGetModuleBase64(process, pc)) GetModuleFileNameA(reinterpret_cast<HMODULE>(base), module, MAX_PATH);
    const char* modName = module;
    for (const char* p = module; *p; ++p)
      if (*p == '\\') modName = p + 1;
    Out(f, "  #%-2d %s!%s+0x%llx", i, modName, haveSym ? sym->Name : "?", static_cast<unsigned long long>(symOff));
    if (haveLine) Out(f, "  (%s:%lu)", line.FileName, line.LineNumber);
    Out(f, "\n");
  }
}

LONG WINAPI OnCrash(EXCEPTION_POINTERS* info) {
  FILE* f = OpenReport();
  Out(f, "\n[simmy] CRASH: exception 0x%08lx at %p\n", info->ExceptionRecord->ExceptionCode,
      info->ExceptionRecord->ExceptionAddress);
  if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
    Out(f, "  %s address %p\n", info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
        reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
  WriteStack(f, *info->ContextRecord);
  if (f) std::fclose(f);
  std::fflush(stderr);
  TerminateProcess(GetCurrentProcess(), 3);
  return EXCEPTION_EXECUTE_HANDLER;
}

void OnAbort(int) {
  CONTEXT ctx{};
  RtlCaptureContext(&ctx);
  FILE* f = OpenReport();
  Out(f, "\n[simmy] CRASH: abort()\n");
  WriteStack(f, ctx);
  if (f) std::fclose(f);
  std::fflush(stderr);
  TerminateProcess(GetCurrentProcess(), 3);
}

// SIMMY_WATCHDOG=N: after N seconds, write every thread's call stack (raw addresses, module-relative)
// to simmy_hang.txt next to the exe, for diagnosing hangs. Avoids the CRT and the loader lock, which a
// hung thread may hold; addresses are symbolized offline against simmy.pdb.
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
  Append(file, "[simmy] WATCHDOG after %lu s\r\n", static_cast<unsigned long>(reinterpret_cast<uintptr_t>(param)));
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
      DWORD n = GetModuleFileNameA(nullptr, g_hangPath, MAX_PATH);
      while (n > 0 && g_hangPath[n - 1] != '\\') --n;
      lstrcpynA(g_hangPath + n, "simmy_hang.txt", MAX_PATH - n);
      CreateThread(nullptr, 0, Watchdog, reinterpret_cast<void*>(static_cast<uintptr_t>(std::atoi(w))), 0, nullptr);
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    SetUnhandledExceptionFilter(OnCrash);
    std::signal(SIGABRT, OnAbort);
  }
};
}  // namespace

#pragma warning(disable : 4073)
#pragma init_seg(lib)  // before the game's global objects (user segment)
static Install s_install;
#endif
