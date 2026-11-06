// @class EFontPage after EStorable ERShader
// One texture page of glyphs (engine/e_fontdata.cpp), drawn with the shader of id m_shaderId.
#pragma once

struct EFontPage : public EStorable {
  SIMMY_STORABLE_DECL(EFontPage)
  EFontPage();
  void Write(EStream&) override;
  void Read(EStream&) override;
  void Load(EFile& file) override;
  void DeallocateShader();
  void CreateCopy() const;
  static void RegisterType(unsigned short);

  unsigned int m_shaderId = 0;      // +0x04
  ERShader* m_shader = nullptr;     // +0x08, loaded by ERFont::LoadFont
  unsigned int m_width = 0;         // +0x0C texture size in texels
  unsigned int m_height = 0;        // +0x10
};
