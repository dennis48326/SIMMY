// @class ERFont after EResource EFontData EVec2 EVec4 EMat4 EFontAlignX EFontAlignY EFontMatrixType
// A font resource (engine/e_rfont.cpp): the stored EFontData plus the current size, scale and color.
// Text is drawn glyph by glyph as textured rectangles (ERC::RectList), one batch per glyph page.
#pragma once

struct ERFont : public EResource {
  ERFont();
  ~ERFont() override;
  SIMMY_STORABLE_DECL(ERFont)
  void Load(EFile& file) override;
  void Deallocate();
  void SetSize(float size, float aspect, bool pickClosest);
  void SelectPage(ERC* rc, int page);
  // Draw (rc set) or measure (rc null) a string; endPos gets the pen position after it. DoDraw works
  // in pixels, DoDrawNormalize in window coordinates (0..1 over the current window or the screen).
  void DoDraw(void* str, bool wide, bool snapX, bool snapY, EVec2& pos, ERC* rc, EVec2* endPos, EWindow* window,
              EMat4* matrix, EVec2* origin, EFontMatrixType type, fontFXcommand* fx);
  void DoDrawNormalize(void* str, bool wide, bool snapX, bool snapY, EVec2& pos, ERC* rc, EVec2* endPos,
                       EWindow* window, EMat4* matrix, EVec2* origin, EFontMatrixType type, fontFXcommand* fx);
  EVec2 DoGetStringSize(void* str, bool wide, bool normalize, EWindow* window);
  void SnapPosToPixel(EVec2& pos, bool snapX, bool snapY, EWindow* window);
  void SnapPosToPixelNormalize(EVec2& pos, bool snapX, bool snapY, EWindow* window);
  void DoDrawAlign(ERC* rc, void* str, bool wide, EVec2 pos, EFontAlignX alignX, EFontAlignY alignY, EVec2* endPos,
                   EMat4* matrix, EFontMatrixType type, fontFXcommand* fx, bool normalize);
  void Draw(ERC* rc, char* str, EVec2& pos, EFontAlignX alignX, EFontAlignY alignY, EVec2* endPos, EMat4* matrix,
            EFontMatrixType type, fontFXcommand* fx, bool normalize);
  void Draw(ERC* rc, unsigned short* str, EVec2& pos, EFontAlignX alignX, EFontAlignY alignY, EVec2* endPos,
            EMat4* matrix, EFontMatrixType type, fontFXcommand* fx, bool normalize);
  // Draws the text twice: a shadow offset by `offset` pixels in black with alpha `alpha`, then the text.
  void DrawDs(ERC* rc, char* str, EVec2& pos, EFontAlignX alignX, EFontAlignY alignY, EVec2* endPos, float offset,
              float alpha, bool normalize);
  void DrawDs(ERC* rc, unsigned short* str, EVec2& pos, EFontAlignX alignX, EFontAlignY alignY, EVec2* endPos,
              float offset, float alpha, bool normalize);
  void LoadFont();
  void Select(ERC* rc);
  float GetLineSpacing(EWindow* window, bool normalize);
  void ProcessFontFX(fontFXcommand*, EMat4*, int);
  static void RegisterType(unsigned short);
  void CreateCopy() const;
  float GetYSize();
  float GetAspect();
  EVec2 GetStringSize(char* str, EWindow* window, bool normalize);
  EVec2 GetStringSize(unsigned short* str, EWindow* window, bool normalize);
  void SetColor(EVec4& color);
  void SetColor(float gray);
  EVec4* GetColor();
  unsigned short GetChar(void* str, bool wide, int i);

  static float m_vScaler;              // 805000B0

  EFontData m_data;                    // +0x14
  float m_pointSize = -1.0f;           // +0x4C
  float m_aspect = 1.0f;               // +0x50
  EFontSize* m_size = nullptr;         // +0x54 the size in use
  EVec4 m_color{1.0f, 1.0f, 1.0f, 1.0f};  // +0x58
  int m_page = -1;                     // +0x68 page currently selected
};
