// @class EResourceManager__DelRefMode
#pragma once
#include "simmy/enum.hpp"

struct EResourceManager__DelRefMode {
  SIMMY_ENUM_BODY(EResourceManager__DelRefMode)
  static constexpr int Immediate = 0;  // free when the count reaches zero
  static constexpr int Cache = 1;      // keep in the free list if the manager can cache it
};
