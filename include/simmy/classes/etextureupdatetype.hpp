// @class ETextureUpdateType
#pragma once
#include "simmy/enum.hpp"

struct ETextureUpdateType {
  SIMMY_ENUM_BODY(ETextureUpdateType)
  static constexpr int Full = 0;
  static constexpr int PaletteOnly = 1;  // UpdateEnd skips the pixels
  static constexpr int Image = 2;        // what resource loads use
  static constexpr int None = 3;         // UpdateEnd only releases the lock
};
