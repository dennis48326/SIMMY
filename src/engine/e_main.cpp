// Port of engine/e_main.cpp: process entry and main-thread startup.
// Original: 802E391C..802E3AF0 in u2_ngc_release_dvd.elf
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// ---- globals owned by this file -------------------------------------------------------------
EThread _idleThread;                    // was 0x340 bytes at 804AD220
static void (*_pExitFunc)() = nullptr;  // was at 804FF0B0

extern "C" int MainInit(int argc, char** argv);
extern "C" int StartMainThread();
extern "C" int InitHeap();
extern "C" void RegisterExitFunction(void (*func)());
int simmy_game_main(int argc, char** argv);

// 802E391C
extern "C" int MainInit(int argc, char** argv) {
  ENgcEngine::InitMemoryManager();
  ENgcEngine::InitConsole();
  EGlobalManager::Startup();
  _pApp->SetArgs(argc, argv);
  _idleThread.AttachToCallingThread();
  _idleThread.m_name = "Idle";
  return 0;
}

// 802E3984
extern "C" int StartMainThread() {
  _pApp->CreateAndStartAppThread();
  _idleThread.SetPriority(0x1e);
  _pEngine->Idle();  // vtable slot 0x4C; on PC this is the window/event loop
  EGlobalManager::Shutdown();
  if (_pExitFunc) {
    _pExitFunc();
  }
  return 0;
}

// 802E39F0
// The original `main`; the real process entry is in platform/pc_main.cpp.
// __main() (C++ static constructors) is the host runtime's job on PC.
int simmy_game_main(int argc, char** argv) {
  MainInit(argc, argv);
  StartMainThread();
  return 0;
}

// 802E3A34
extern "C" int InitHeap() { return 1; }

// 802E3A3C
extern "C" void RegisterExitFunction(void (*func)()) { _pExitFunc = func; }
