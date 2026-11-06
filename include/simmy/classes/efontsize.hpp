// @class EFontSize after EStorable EFontCharacter EFontPage
// One point size of a font (engine/e_fontdata.cpp): glyphs by character code, and the pages.
#pragma once
#include <unordered_map>
#include <vector>

struct EFontSize : public EStorable {
  SIMMY_STORABLE_DECL(EFontSize)
  EFontSize();
  ~EFontSize() override;
  void Deallocate();
  void DeallocateShaders();
  void DeallocatePages();
  void Write(EStream&) override;
  void Read(EStream&) override;
  void Load(EFile& file) override;
  void CreateCopy() const;
  static void RegisterType(unsigned short);

  int m_size = 0;                    // +0x04 point size (ERFont::SetSize picks the closest)
  unsigned int m_cellHeight = 0;     // +0x08 texel height of a row of glyph cells
  unsigned int m_pixelFont = 1;      // +0x0C 1: drawn at whole pixel positions (ERFont snaps)
  std::unordered_map<unsigned int, EFontCharacter*> m_chars;  // +0x10 THashTable<u32, EFontCharacter*>
  std::vector<EFontPage*> m_pages;   // +0x20 TArray<EFontPage*>
};
