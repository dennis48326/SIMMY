// @class EFontCharacter after EStorable
// One glyph of a font size (engine/e_fontdata.cpp): where it is on its page and how wide it is.
#pragma once

struct EFontCharacter : public EStorable {
  SIMMY_STORABLE_DECL(EFontCharacter)
  void Write(EStream&) override;
  void Read(EStream&) override;
  void Load(EFile& file) override;
  void CreateCopy() const;
  static void RegisterType(unsigned short);

  unsigned int m_left = 0;     // +0x04 texel column where the glyph starts on its page
  unsigned int m_right = 0;    // +0x08 texel column where it ends
  unsigned short m_row = 0;    // +0x0C row of glyph cells (EFontSize::m_cellHeight texels each)
  unsigned short m_page = 0;   // +0x0E index into EFontSize::m_pages
};
