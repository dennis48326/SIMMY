// @class EFontData after EStorable EFontSize
// A font (engine/e_fontdata.cpp): its sizes plus metrics and a kerning-style table.
#pragma once
#include <unordered_map>
#include <vector>

struct EFontData : public EStorable {
  SIMMY_STORABLE_DECL(EFontData)
  EFontData();
  ~EFontData() override;
  void Deallocate();
  void Write(EStream&) override;
  void Read(EStream&) override;
  void Load(EFile& file) override;
  void CreateCopy() const;
  static void RegisterType(unsigned short);

  std::vector<EFontSize*> m_sizes;   // +0x04 ENodeList<EFontSize*>
  int m_ownsSizes = 1;               // +0x0C
  // Metrics in font units (ERFont scales them by point size / m_designSize).
  unsigned int m_lineBottom = 0;     // +0x10 bottom of a line, from the top of the glyph cells
  unsigned int m_cellRaise = 0;      // +0x14 how far the glyph cells start above the text position
  unsigned int m_defaultKern = 0;    // +0x18 extra advance for character pairs not in m_table
  unsigned int m_lineSpacing = 0;    // +0x1C line advance factor (0: 1)
  unsigned int m_designSize = 0;     // +0x20
  unsigned int m_missingAdvance = 0; // +0x24 advance for characters without a glyph
  std::unordered_map<unsigned int, int> m_table;  // +0x28 THashTable<u32, int>
};
