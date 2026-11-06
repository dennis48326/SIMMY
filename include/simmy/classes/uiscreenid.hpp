// @class UIScreenID
// A UI screen number (UIScreenManager::LoadUIScreen; e.g. 3 the CAS genetics screen, 0x17 the name
// keyboard, 0x2A the credits).
#pragma once
#include "simmy/enum.hpp"

struct UIScreenID {
  SIMMY_ENUM_BODY(UIScreenID)
};
