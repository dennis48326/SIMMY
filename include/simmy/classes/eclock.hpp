// @class EClock
// A stopwatch: last sample time plus seconds accumulated since Start() (engine/e_ngcclock.cpp).
#pragma once
#include <cstdint>

struct EClock {
  std::uint64_t m_last = 0;  // +0x0, host ticks (was OSTime)
  float m_seconds = 0.0f;    // +0x8
};
