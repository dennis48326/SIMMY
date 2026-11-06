// @class RCMode
// How a render context records (engine/e_rc.h): immediate contexts execute each command as it is
// issued; deferred ones build a display list that is executed (often many times) later.
#pragma once
#include "simmy/enum.hpp"

struct RCMode {
  SIMMY_ENUM_BODY(RCMode)
  static constexpr int Immediate = 0;
  static constexpr int Deferred = 1;
};
