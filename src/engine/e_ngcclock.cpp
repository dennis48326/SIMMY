// Port of engine/e_ngcclock.cpp: the clock manager, on the host's monotonic clock.
#include "simmy/types_gen.hpp"

#include <chrono>

static ENgcClockMan _ngcClockMan;
ENgcClockMan* _pClockMan = &_ngcClockMan;  // 804FF17C (statically initialised in the original)

// PC: nanoseconds of the game's clock (the wall clock, or SIMMY_DET's virtual clock: pc_det.cpp).
static std::uint64_t now_ticks() { return simmy::game_clock_ns(); }
static double ticks_to_sec(std::uint64_t t) { return static_cast<double>(t) * 1e-9; }

// 8032D9EC  ENgcClockMan::ENgcClockMan  (defaulted in the header)
// 8032DA00  ENgcClockMan::~ENgcClockMan (defaulted in the header)

// 8032DA34
void ENgcClockMan::Init() {}

// 8032DA38
void ENgcClockMan::Update() {}

// 8032DA3C
void ENgcClockMan::Start(EClock* clock) {
  clock->m_last = now_ticks();
  clock->m_seconds = 0.0f;
}

// 8032DA7C
void ENgcClockMan::UpdateData(EClock* clock) {
  // The original accumulated whole milliseconds since the previous sample.
  const std::uint64_t now = now_ticks();
  clock->m_seconds += static_cast<float>(ticks_to_sec(now - clock->m_last));
  clock->m_last = now;
}

// 8032DB0C
double ENgcClockMan::GetSec(EClock* clock) {
  UpdateData(clock);
  return clock->m_seconds;
}

// 8032DB3C
double ENgcClockMan::GetSecDouble(EClock* clock) {
  UpdateData(clock);
  return clock->m_seconds;
}
