// @class TextBlock after EVec2 EVec4 ERFont
// A block of word-wrapped UI text (game/textblock.cpp), drawn with ERFont in window coordinates
// (0..1). The Wrapper toolkit's text items embed one (TextBaseItem +0x20). DrawWrappedText breaks the
// text into lines at whitespace (or mid-word when a word alone is too wide) to fit the display width
// less its padding; a measuring pass (no ERC) records the line count and the text's size, and runs
// first whenever the block is dirty.
#pragma once

struct TextBlock {
  // m_flags
  enum : unsigned int {
    kAlignLeft = 0x1,         // draw lines left-aligned from the left edge (else centred on the middle)
    kAlignYCenter = 0x4,      // each line centred vertically on the pen (else its top)
    kVerticalCenter = 0x8,    // the pen starts at the vertical middle, moved up by half the text height
    kPosIsCenterX = 0x10,     // m_pos.x is the middle of the display box (else its left edge)
    kPosIsCenterY = 0x20,     // m_pos.y is the middle of the display box (else its top)
    kDropShadow = 0x80,       // draw with ERFont::DrawDs
    kDirty = 0x10000000,      // sizes need measuring again
    kOverflow = 0x40000000,   // the last draw ran past the bottom of the box
    kLinesSkipped = 0x80000000,  // the last draw skipped m_firstLine lines
  };

  TextBlock() = default;
  TextBlock(const unsigned short* text, EVec2& pos, EVec4* color, ERFont* font, float fontSize, bool shadow,
            bool verticalCenter, bool unused);  // 8008396C
  TextBlock(const unsigned short* text, EVec2& pos, EVec2& size, EVec4* color, ERFont* font, float fontSize,
            bool shadow, bool verticalCenter, bool unused);  // 800839DC
  void Set(const unsigned short* text, EVec2& pos, EVec2& size, EVec4* color, ERFont* font, float fontSize,
           bool shadow, bool verticalCenter, bool unused);  // 80083A14
  void Clear();  // 80083B50
  void DrawWrappedText(ERC* rc, EVec2& pos, float maxY, float alpha);  // 80083C84
  void DrawText(ERC* rc, float alpha);  // 80084184
  void CalcSizes();  // 800842AC
  void SetDirty() { m_flags |= kDirty; }

  static ERFont* s_pDefaultFont;   // 80500590
  static float s_fDefaultFontSize;  // 80500594

  const unsigned short* m_text = nullptr;  // +0x00
  EVec2 m_pos;                     // +0x04
  EVec2 m_size;                    // +0x0C display box (0 width: no wrapping)
  EVec2 m_padSize;                 // +0x14 padding inside the box (x on both sides, y at the bottom)
  EVec2 m_textSize;                // +0x1C measured: widest line, total height
  EVec4* m_color = nullptr;        // +0x24 16-byte aligned on the main heap (Clear allocates it)
  float m_shadowAlpha = 1.0f;      // +0x28
  float m_shadowSize = 2.0f;       // +0x2C shadow offset in pixels
  ERFont* m_font = nullptr;        // +0x30
  float m_fontSize = 0.0f;         // +0x34
  unsigned int m_flags = 0;        // +0x38
  int m_firstLine = 0;             // +0x3C lines skipped before drawing (scrolling)
  int m_lineCount = 0;             // +0x40 measured
  int m_wrapCount = 0;             // +0x44 measured: lines broken for width
  float m_lineGap = 0.0f;          // +0x48 extra space between lines
};
