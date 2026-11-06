// Port of engine/e_ngcscheduler.cpp. The GameCube scheduler thread only recycled command blocks;
// every Queue* call forwarded straight to the renderer. On PC there is no thread.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

extern ENgcRenderer _ngcRenderer;
ENgcScheduler* _pSched = nullptr;  // 804FF3E8
static ENgcScheduler _ngcSched;    // global constructor keyed to _pSched

// 8034D7C4
ENgcScheduler::ENgcScheduler() { _pSched = this; }
// 8034D828
ENgcScheduler::~ENgcScheduler() = default;

// 8034D89C
RET_UNKNOWN ENgcScheduler::Init() {
  m_name = "Scheduler";
  return 1;
}

// 8034D918
void ENgcScheduler::QueueSetupFrameBuffer(int buffer) { _ngcRenderer.QueueBeginFrame(buffer); }
// 8034D940
void ENgcScheduler::QueueDisplayList(EDL* dl, bool freeAfter) { _ngcRenderer.QueueDisplayList(dl, freeAfter); }
// 8034D968
void ENgcScheduler::QueueCompletionEvent(EEvent& done) { _ngcRenderer.QueueEndFrame(done); }
// 8034D990
void ENgcScheduler::QueueSwapBuffer(int a, int b) { _ngcRenderer.QueueSwap(a, b); }
// 8034D9B8
void ENgcScheduler::Flush() { _ngcRenderer.Flush(); }
// 8034D9E0
void ENgcScheduler::TextureLoadsComplete(ESchedCommand*) {}
// 8034D9E4
void ENgcScheduler::RenderingComplete(ESchedCommand*) {}
// 8034D9E8
RET_UNKNOWN ENgcScheduler::GetLastRetraceCount() { return 1; }
// 8034D9F0
void ENgcScheduler::Main() {}  // the command-recycling loop is not needed on PC
// 8034DA28
void ENgcScheduler::SendCommand(ESchedCommand*) {}
// 8034DA50
RET_UNKNOWN ENgcScheduler::AllocSchedCommand() { return 0; }
// 8034DADC
void ENgcScheduler::FreeSchedCommand(ESchedCommand*) {}
// 8034DBA8
void ENgcScheduler::RendFrameComplete(ESchedCommand*) {}
