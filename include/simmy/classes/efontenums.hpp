// @class EFontAlignX
// @class EFontAlignY
// @class EFontMatrixType
// ERFont drawing options (engine/e_rfont.h), recovered from ERFont::DoDrawAlign and DoDrawNormalize.
#pragma once
#include "simmy/enum.hpp"

struct EFontAlignX {
  SIMMY_ENUM_BODY(EFontAlignX)
  static constexpr int Left = 0;
  static constexpr int Right = 1;
  static constexpr int Center = 2;
};
struct EFontAlignY {
  SIMMY_ENUM_BODY(EFontAlignY)
  static constexpr int Top = 0;
  static constexpr int Bottom = 1;
  static constexpr int Center = 2;
};
// What a text matrix does to each glyph (normalised drawing): Rotate moves the glyph centres through
// the matrix about the text origin, Scale only scales their offsets from it. Either way each glyph is
// then drawn with the matrix's 2D part about its own centre.
struct EFontMatrixType {
  SIMMY_ENUM_BODY(EFontMatrixType)
  static constexpr int Scale = 0;
  static constexpr int Rotate = 1;
};
