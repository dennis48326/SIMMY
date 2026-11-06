// Port of game/timer.cpp: the frame timer the Apt viewer advances Flash by (whole milliseconds of
// real time per frame, or a fixed step when a frame delta is set).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
EClock s_timerClock;
EClock* gpClock = &s_timerClock;
float fDelta = 0.0f;
float fCurrentTime = 0.0f, fPrevTime = 0.0f, fFrameTime = 0.0f;
unsigned int uCurrentTime = 0, uPrevTime = 0, uFrameTime = 0;
}  // namespace

// 80076E74
int Timer_Init(EClock* clock) {
  gpClock = clock;
  _pClockMan->Start(clock);
  const double t = _pClockMan->GetSec(gpClock);
  fCurrentTime = static_cast<float>(t);
  uCurrentTime = static_cast<unsigned int>(t * 1000.0);
  return 1;
}

// 80076EF8
void Timer_SetFrameDelta(float delta) {
  fDelta = delta;
  if (delta == 0.0f) {
    fCurrentTime = fPrevTime;
    uCurrentTime = uPrevTime;
  }
  Timer_StartFrame();
}

// 80076F3C
// A frame longer than half a second (or negative) counts as no time.
void Timer_StartFrame() {
  if (fDelta == 0.0f) {
    fPrevTime = fCurrentTime;
    uPrevTime = uCurrentTime;
    const double t = _pClockMan->GetSec(gpClock);
    uCurrentTime = static_cast<unsigned int>(t * 1000.0);
    fCurrentTime = static_cast<float>(t);
    fFrameTime = static_cast<float>(t - fPrevTime);
    uFrameTime = uCurrentTime - uPrevTime;
    if (fFrameTime > 0.5f || fFrameTime < 0.0f) {
      const double now = _pClockMan->GetSec(gpClock);
      fCurrentTime = fPrevTime = static_cast<float>(now);
      uPrevTime = static_cast<unsigned int>(now * 1000.0);
      fFrameTime = 0.0f;
      uFrameTime = 0;
      uCurrentTime = uPrevTime;
    }
  } else {
    fCurrentTime += fDelta;
    fFrameTime = fDelta;
    uPrevTime = uCurrentTime;
    uFrameTime = static_cast<unsigned int>(static_cast<int>(fCurrentTime * 60.0f)) - uCurrentTime;
    uCurrentTime = static_cast<unsigned int>(static_cast<int>(fCurrentTime * 60.0f));
  }
}

// 800770C4
double Timer_GetTime() { return fFrameTime; }
// 800770CC
double Timer_GetTotalTime() { return fCurrentTime; }
// 800770D4
int Timer_GetTicks() { return static_cast<int>(uFrameTime); }
// 800770DC
int Timer_GetTotalTicks() { return static_cast<int>(uCurrentTime); }

// 800098B4
void AptAuxPCEorGL_TimerInit() { Timer_Init(gpClock); }
// 800098D8
void AptAuxPCEorGL_TimerStartFrame() { Timer_StartFrame(); }
// 800098F8
int AptAuxPCEorGL_TimerGetTicks() { return Timer_GetTicks(); }
