// Port of engine/e_ngcengine.cpp: the platform engine, rewritten for PC.
//
// GameCube-only work dropped: ARAM and virtual-memory setup, carving the OS arena into fixed heap
// ranges (heaps are host-backed, see base/e_standard_heap.cpp), the SN debug-target check and the
// DVD fatal-error screen. The main thread's Idle() becomes the window/event loop.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>

EEngine* _pEngine = nullptr;     // 804FF190
static ENgcEngine _ngcEngine;    // the engine object (global constructor keyed to _pEngine)
static int s_bShuttingDown = 0;  // set when the console reset button is pressed

// 8032E04C
ENgcEngine::ENgcEngine() {
  _pEngine = this;
  m_field50 = 0;
}

// 8032E09C
ENgcEngine::~ENgcEngine() = default;

// 8032E0CC
int ENgcEngine::Init() {
  m_predictiveSmoothing = 0;
  return EEngine::Init();
}

// 8032E18C
int ENgcEngine::InitMemoryManager() {
  // Original: ARAM + VM init, OSInit, then 11 EAHeaps carved out of the arena with fixed sizes.
  // On PC the heaps are created on first use; touch them so they exist in a fixed order.
  for (EAHeap* h : {RootHeap(), ResourceHeap(), ModelHeap(), TextureHeap(), ShaderHeap(), AnimationHeap(),
                    FlashesHeap(), AptHeap(), AudioHeap(), MainHeap(), VMHeap()})
    (void)h;
  EAHeap::InitializeHeapStatics();
  FPHeap::InitializeHeapStatics();
  return 1;
}

// 8032E0F4
int ENgcEngine::InitConsole() {
  // Original: profiler, OS alarms, DVDInit, an event thread (DVD/reset polling), then spins until
  // "data/index.ind" opens. On PC the data check happens when the file system is ported
  // (base/e_ngcfilesystem.cpp); the event thread's job is done by the main-thread loop.
  InitializeProfiler();
  return 1;
}

// 8032E638
void ENgcEngine::InitializeProfiler() {}

// 8032E63C
// PC: also does the event thread's audio work (_pActualAudio->Update(), see InitConsole) once a frame.
void ENgcEngine::PreFrameUpdate() {
  if (_pActualAudio) _pActualAudio->Update();
  EEngine::PreFrameUpdate();
}

// 8032E884
int ENgcEngine::IsResetting() { return s_bShuttingDown; }

// PC only: run the app's Main() here, on the main thread (see platform/pc_main.cpp for why).
// Closing the window asks the app to quit, as the console's reset button did.
void ENgcEngine::Idle() {
  simmy::platform_set_quit_handler([] {
    s_bShuttingDown = 1;
    if (_pApp) _pApp->m_quit = 1;
  });
  _pApp->RunOnCallingThread();
}
