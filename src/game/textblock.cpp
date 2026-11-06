// Port of game/textblock.cpp: word-wrapped UI text drawn with ERFont.
#include "simmy/types_gen.hpp"

#include <cstring>

ERFont* TextBlock::s_pDefaultFont = nullptr;  // 80500590
float TextBlock::s_fDefaultFontSize = 0.0f;   // 80500594

// 8008396C
TextBlock::TextBlock(const unsigned short* text, EVec2& pos, EVec4* color, ERFont* font, float fontSize, bool shadow,
                     bool verticalCenter, bool unused) {
  EVec2 size{0.0f, 0.0f};
  Set(text, pos, size, color, font, fontSize, shadow, verticalCenter, unused);
}

// 800839DC
TextBlock::TextBlock(const unsigned short* text, EVec2& pos, EVec2& size, EVec4* color, ERFont* font, float fontSize,
                     bool shadow, bool verticalCenter, bool unused) {
  Set(text, pos, size, color, font, fontSize, shadow, verticalCenter, unused);
}

// 80083A14
// A null color is white, a null font or zero size the defaults. The last flag is read but unused:
// either way the text is centred horizontally.
void TextBlock::Set(const unsigned short* text, EVec2& pos, EVec2& size, EVec4* color, ERFont* font, float fontSize,
                    bool shadow, bool verticalCenter, bool) {
  m_text = text;
  m_pos = pos;
  m_size = size;
  *m_color = color ? *color : EVec4{1.0f, 1.0f, 1.0f, 1.0f};
  m_flags = shadow ? m_flags | kDropShadow : m_flags & ~kDropShadow;
  m_flags = verticalCenter ? m_flags | kVerticalCenter : m_flags & ~kVerticalCenter;
  m_flags &= ~kAlignLeft;
  m_font = font ? font : s_pDefaultFont;
  m_fontSize = fontSize != 0.0f ? fontSize : s_fDefaultFontSize;
  m_flags |= kDirty;
}

// 80083B50
void TextBlock::Clear() {
  m_text = nullptr;
  m_pos = EVec2{0.0f, 0.0f};
  m_size = EVec2{0.0f, 0.0f};
  m_padSize = EVec2{0.0f, 0.0f};
  m_textSize = EVec2{0.0f, 0.0f};
  if (!m_color) m_color = static_cast<EVec4*>(MainHeap()->MallocAligned(0x10, 0x10, 0, 0));
  *m_color = EVec4{1.0f, 1.0f, 1.0f, 1.0f};
  m_lineGap = 0.0f;
  m_shadowAlpha = 1.0f;
  m_shadowSize = 2.0f;
  m_wrapCount = 0;
  m_font = nullptr;
  m_fontSize = 0.0f;
  m_flags = 0;
  m_firstLine = 0;
  m_lineCount = 0;
}

// 80083C84
// Measures (rc null) or draws the text from pen position `pos`, line by line. Lines are at most 254
// characters; '\n' ends a line. Lines whose pen starts below maxY still draw but set kOverflow.
void TextBlock::DrawWrappedText(ERC* rc, EVec2& pos, float maxY, float alpha) {
  if (!m_text) return;
  if (m_flags & kDirty) {
    m_flags &= ~kDirty;
    DrawWrappedText(nullptr, pos, maxY, 1.0f);
  }
  m_font->SetSize(m_fontSize, 1.0f, true);
  EVec4 color = *m_color;
  color.w *= alpha;
  if (color.w == 0.0f) return;
  m_font->m_color = color;
  m_font->Select(rc);

  static const unsigned short kEmpty[1] = {0};
  const EVec2 empty = m_font->DoGetStringSize(const_cast<unsigned short*>(kEmpty), true, true, nullptr);
  const float lineHeight = empty.y + m_lineGap;
  float maxWidth = m_size.x;
  if (maxWidth != 0.0f) maxWidth -= m_padSize.x + m_padSize.x;
  const bool measuring = rc == nullptr;

  EVec2 pen = pos;
  if ((m_flags & kVerticalCenter) && !measuring)
    pen.y -= static_cast<float>(m_lineCount) * lineHeight * 0.5f;
  m_flags &= ~(kOverflow | kLinesSkipped);
  if (measuring) {
    m_lineCount = 0;
    m_textSize = EVec2{0.0f, 0.0f};
  }

  int wraps = 0;
  int skipped = 0;
  const unsigned short* src = m_text;
  unsigned short line[256];
  while (*src) {
    int n = 0;
    std::memset(line, 0, sizeof(line));
    bool endLine = false;
    int lastBreak = 0;
    unsigned short c = *src;
    while (c) {
      line[n] = c;
      if (*src == '\n') {
        ++src;
        endLine = true;
      } else {
        // (the GC also measured the single character here and threw the result away)
        if (*src == ' ' || static_cast<unsigned int>(*src - 9) < 5) lastBreak = n;
        if (maxWidth > 0.0f && maxWidth < m_font->DoGetStringSize(line, true, true, nullptr).x) {
          endLine = true;
          ++wraps;
          int back = n - lastBreak;
          if (back == 0) {
            line[n] = 0;  // the overflowing character is the break: drop it
          } else {
            if (n == back) {  // no break on this line: split the word before this character
              back = 0;
              --src;
            }
            n -= back;
            src -= back;
            line[n] = 0;
          }
          --n;
        }
        ++n;
        ++src;
        if (n > 0xFD) break;
      }
      c = *src;
      if (endLine) break;
    }
    if (!endLine && line[0] == 0) continue;
    line[n] = 0;
    const EVec2 size = m_font->DoGetStringSize(line, true, true, nullptr);
    if (measuring && m_textSize.x < size.x) m_textSize.x = size.x;
    if (skipped < m_firstLine) {
      ++skipped;
      m_flags |= kLinesSkipped;
    } else {
      if (!measuring) {
        const EFontAlignX alignX = (m_flags & kAlignLeft) ? EFontAlignX::Left : EFontAlignX::Center;
        const EFontAlignY alignY = (m_flags & kAlignYCenter) ? EFontAlignY::Center : EFontAlignY::Top;
        if (m_flags & kDropShadow)
          m_font->DrawDs(rc, line, pen, alignX, alignY, nullptr, m_shadowSize, m_shadowAlpha * alpha, true);
        else
          m_font->DoDrawAlign(rc, line, true, pen, alignX, alignY, nullptr, nullptr, 0, nullptr, true);
      }
      pen.x = pos.x;
      pen.y += lineHeight;
      if (measuring) ++m_lineCount;
      if (maxY < pen.y) m_flags |= kOverflow;
    }
    if (measuring) m_textSize.y += lineHeight;
  }
  if (measuring) m_wrapCount = wraps;
}

// 80084184
// Works out the pen from the position flags and draws; text below the box (less its bottom
// padding) is flagged as overflow.
void TextBlock::DrawText(ERC* rc, float alpha) {
  float left = m_pos.x, centerX = m_pos.x;
  float top = m_pos.y, centerY = m_pos.y;
  if (m_flags & kPosIsCenterX) left = m_pos.x - m_size.x * 0.5f;
  else centerX = m_size.x * 0.5f + m_pos.x;
  if (m_flags & kPosIsCenterY) top = m_pos.y - m_size.y * 0.5f;
  else centerY = m_size.y * 0.5f + m_pos.y;
  EVec2 pen{(m_flags & kAlignLeft) ? left : centerX, (m_flags & kVerticalCenter) ? centerY : top};
  DrawWrappedText(rc, pen, top + m_size.y - m_padSize.y, alpha);
}

// 800842AC
void TextBlock::CalcSizes() { DrawText(nullptr, 1.0f); }
