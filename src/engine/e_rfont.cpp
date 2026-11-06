// Port of engine/e_rfont.cpp: font resources, text measuring and drawing. ProcessFontFX (per-glyph
// text effects) is not ported yet.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern ENgcRenderer _ngcRenderer;

float kFontZValue = 0.5f;        // 804FF0F8
float ERFont::m_vScaler = 1.0f;  // 805000B0
static float s_hScaler = 1.0f;   // 805000B4

// 80317808 ERFont::SafeDelete
// 80317848 ERFont::GetTypeInfo
// 80317854 ERFont::GetTypeName
// 80317860 ERFont::GetTypeKey
// 8031786C ERFont::GetTypeVersion
// 80317790 ERFont::New
// 803177B8 ERFont::Construct
// 803177E4 ERFont::Destruct
// 80317878 ERFont::GetTypeInfoStatic
// 80317884 ERFont::GetReadVersion
SIMMY_STORABLE_IMPL(ERFont, "ERFont", 0, EResource)

// 80313F08
ERFont::ERFont() {}
// 80313F88
ERFont::~ERFont() { Deallocate(); }

// 80313FF0
void ERFont::Deallocate() {
  for (EFontSize* size : m_data.m_sizes) size->DeallocateShaders();
}

// 80314034
// 'FONT' version 10: the EFontData stored in place. The first size becomes the current one.
void ERFont::Load(EFile& file) {
  Deallocate();
  EDataHeader header;
  if (header.Read(file, 0x464F4E54 /* 'FONT' */, 10, 10)) m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  file >> static_cast<EStorable&>(m_data);
  if (!m_data.m_sizes.empty()) {
    m_size = m_data.m_sizes.front();
    m_pointSize = static_cast<float>(m_size->m_size);
  }
  if (std::getenv("SIMMY_LOG_LOADS"))
    for (EFontSize* s : m_data.m_sizes)
      std::fprintf(stderr, "[font] %08X size %d: %zu glyphs, %zu pages (first page shader %08X)\n", m_id, s->m_size,
                   s->m_chars.size(), s->m_pages.size(), s->m_pages.empty() ? 0u : s->m_pages[0]->m_shaderId);
}

// 80314128
// Sets the drawing size; with pickClosest, also picks the stored size closest to it (preferring a
// larger one, unless that is more than twice as large).
void ERFont::SetSize(float size, float aspect, bool pickClosest) {
  const float want = size * s_hScaler;
  if (m_vScaler != 0.0f) aspect *= s_hScaler / m_vScaler;
  if (pickClosest) {
    EFontSize* best = nullptr;
    float bestDiff = 0.0f;
    for (EFontSize* s : m_data.m_sizes) {
      const float diff = static_cast<float>(s->m_size) - want;
      if (diff == 0.0f) {
        best = s;
        break;
      }
      bool take;
      if (!best) take = true;
      else if (diff < 0.0f) take = bestDiff < diff;
      else take = std::fabs(diff) < std::fabs(bestDiff) || (bestDiff < 0.0f && static_cast<float>(s->m_size) / want <= 2.0f);
      if (take) {
        best = s;
        bestDiff = diff;
      }
    }
    m_size = best;
  }
  m_aspect = aspect;
  m_pointSize = want;
}

// ---- Drawing ---------------------------------------------------------------------------------------

namespace {
// Rounds to the nearest pixel the way the original does (add 0.5, truncate).
float SnapToPixel(float v) { return static_cast<float>(static_cast<int>(v + 0.5f)); }
float AsFloat(unsigned int stored) { return static_cast<float>(static_cast<int>(stored)); }

EFontCharacter* FindGlyph(EFontSize* size, unsigned int c) {
  auto it = size->m_chars.find(c);
  return it == size->m_chars.end() ? nullptr : it->second;
}

constexpr int kRectBatch = 22;  // rectangles per RectList call (the original's stack buffer)
constexpr int kMaxPages = 64;

// The body of DoDraw (normalize false: pixels) and DoDrawNormalize (window coordinates: scaled by
// the window's size on the way in and out). With rc null it only measures. Glyphs are drawn page by
// page, starting at the page already selected, as batches of textured rectangles.
void DrawString(ERFont* font, void* str, bool wide, bool snapX, bool snapY, EVec2& pos, ERC* rc, EVec2* endPos,
                EWindow* window, EMat4* matrix, EVec2* origin, int type, fontFXcommand* fx, bool normalize) {
  if (!str || ERFont::m_vScaler == 0.0f || s_hScaler == 0.0f) {
    if (endPos) *endPos = EVec2{0.0f, 0.0f};
    return;
  }
  font->LoadFont();
  EFontSize* size = font->m_size;
  const EFontData& data = font->m_data;
  const bool pixelFont = size->m_pixelFont == 1;

  float sx = 1.0f, sy = 1.0f;  // window size in pixels (normalised drawing)
  if (normalize) {
    if (!window) window = EWindow::m_pCurrentWindow;
    sx = window ? window->m_mat.m[0][0] : static_cast<float>(_pGfx->m_screenWidth);
    sy = window ? window->m_mat.m[1][1] : static_cast<float>(_pGfx->m_screenHeight);
  }
  const float invSx = 1.0f / sx, invSy = 1.0f / sy;

  // DoDraw's matrices map pixels to the 0..1 rectangle space: the current 2D window's orthographic
  // projection, or the 3D window's.
  EMat4 windowMat;
  windowMat.Id();
  if (matrix && !normalize) {
    if (EWindow::m_pCurrent3DWindow)
      windowMat = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow)->m_normalizedProjection;
    else if (EWindow* w = EWindow::m_pCurrentWindow) {
      windowMat.Ortho(0.0f, w->m_mat.m[0][0], w->m_mat.m[1][1], 0.0f, 0.01f, 10.0f);
      for (int c = 0; c < 4; ++c) {
        windowMat.m[0][c] *= 0.5f;
        windowMat.m[1][c] *= -0.5f;
      }
      windowMat.m[3][0] = windowMat.m[3][0] * 0.5f + 0.5f;
      windowMat.m[3][1] = windowMat.m[3][1] * -0.5f + 0.5f;
    }
  }
  const bool doFx = fx && *reinterpret_cast<unsigned char*>(fx) != 0;  // (the original passes a 4-byte copy)

  EVec2 pen{pos.x * sx, pos.y * sy};
  EMat4 rot;  // the matrix without its translation
  EVec2 org;  // the text origin the matrix applies about
  if (matrix) {
    rot = *matrix;
    rot.m[3][0] = rot.m[3][1] = rot.m[3][2] = 0.0f;
    rot.m[3][3] = 1.0f;
    org = EVec2{origin->x * sx, origin->y * sy};
  }
  if (pixelFont) {
    if (snapX) {
      pen.x = SnapToPixel(pen.x);
      if (matrix) org.x = SnapToPixel(org.x);
    }
    if (snapY) {
      pen.y = SnapToPixel(pen.y);
      if (matrix) org.y = SnapToPixel(org.y);
    }
  }

  const float scale = font->m_pointSize / AsFloat(data.m_designSize);       // font units to pixels
  const float sizeScale = font->m_pointSize / static_cast<float>(size->m_size);  // this size's pixels
  const int pageCount = static_cast<int>(size->m_pages.size());
  const float top = pen.y - (AsFloat(data.m_cellRaise) * scale + 1.0f);
  const float missingAdvance = AsFloat(data.m_missingAdvance) * scale * font->m_aspect;
  const float lineHeight = AsFloat(data.m_lineBottom) * scale + 1.0f;
  const float bottom = top + (AsFloat(size->m_cellHeight) * sizeScale - 1.0f);

  int first = 0, last = 0;
  int counts[kMaxPages] = {};
  EVec2 start;
  if (rc) {
    rc->EnableRasterModes(0x8000, 0);
    rc->EnableRasterModes(0x40, 0);
    first = font->m_page != -1 ? font->m_page : 0;
    start = pen;
    last = first + pageCount - 1;
    if (pageCount < 2) {
      while (font->GetChar(str, wide, counts[0])) ++counts[0];
    } else {
      for (int i = 0; unsigned int c = font->GetChar(str, wide, i); ++i)
        if (EFontCharacter* g = FindGlyph(size, c); g && g->m_page < kMaxPages) ++counts[g->m_page];
    }
  }

  float rects[kRectBatch * 16];
  const int stride = matrix ? 16 : 8;
  for (int i = first; i <= last; ++i) {
    const int p = pageCount ? i % pageCount : 0;
    if (rc && counts[p] == 0) continue;
    int n = 0;
    float* out = rects;
    float texelU = 0.0f, texelV = 0.0f;
    if (rc) {
      font->SelectPage(rc, p);
      EFontPage* page = size->m_pages[p];
      pen = start;
      texelU = 1.0f / AsFloat(page->m_width);
      texelV = 1.0f / AsFloat(page->m_height);
    }
    auto flush = [&] {
      if (matrix) rc->RectListRot(n, rects, font->m_color, kFontZValue);
      else rc->RectList(n, rects, font->m_color, kFontZValue);
      n = 0;
      out = rects;
    };
    int k = 0;
    for (unsigned int c = font->GetChar(str, wide, 0); c != 0;) {
      const unsigned int next = font->GetChar(str, wide, ++k);
      if (EFontCharacter* g = FindGlyph(size, c)) {
        float x0 = pen.x - 1.0f;
        float x1 = AsFloat(g->m_right - g->m_left) * font->m_aspect * sizeScale + x0;
        if (rc && g->m_page == static_cast<unsigned int>(font->m_page)) {
          // Pages are stored bottom row first: the top of the glyph cell has the larger v.
          const int cellH = static_cast<int>(size->m_cellHeight);
          const float vBottom = static_cast<float>(g->m_row * cellH) * texelV;
          const float vTop = static_cast<float>((g->m_row + 1) * cellH - 1) * texelV;
          const float u0 = AsFloat(g->m_left) * texelU;
          // PC: half a texel in from the cell's right edge. Scaled text (Apt's 12-point fields)
          // filtered linearly otherwise picks up the next column of the page, a thin line after
          // some glyphs.
          const float u1 = (AsFloat(g->m_right) - 0.5f) * texelU;
          float rx0 = x0, ry0 = top, rx1 = x1, ry1 = bottom;
          EMat4 m;
          if (matrix && normalize) {
            const float hw = (x1 - x0) * 0.5f, hh = (bottom - top) * 0.5f;
            float cx = x0 + hw - org.x, cy = top + hh - org.y;
            if (type == EFontMatrixType::Rotate) {
              EVec4 c4{cx, cy, 0.0f, 1.0f};
              EVec4 r = c4 * rot;
              cx = r.x, cy = r.y;
            } else if (type == EFontMatrixType::Scale) {
              cx *= rot.GetScaleX();
              cy *= rot.GetScaleY();
            }
            if (type == EFontMatrixType::Rotate || type == EFontMatrixType::Scale) {
              cx += org.x, cy += org.y;
              rx0 = cx - hw, rx1 = cx + hw, ry0 = cy - hh, ry1 = cy + hh;
            }
            m = *matrix;
            if (doFx) font->ProcessFontFX(fx, &m, k);
          } else if (matrix) {
            m = *matrix;
            if (doFx) {
              font->ProcessFontFX(fx, &m, k);
              EVec3 about{org.x, org.y, 0.0f};
              if (type == 0) about = EVec3{(x0 + x1) * 0.5f, (top + bottom) * 0.5f, 0.0f};
              EVec3 back{-about.x, -about.y, -about.z};
              m.PreTranslate(back);
              m.PostTranslate(about);
            }
          }
          out[0] = rx0 * invSx;
          out[1] = ry0 * invSy;
          out[2] = rx1 * invSx;
          out[3] = ry1 * invSy;
          if (matrix && !normalize) {
            CenterRectAndShiftMatrix(out, m);
            EMat4 t;
            t.Mult4x4(m, windowMat);
            m = t;
          }
          out[4] = u0, out[5] = vTop, out[6] = u1, out[7] = vBottom;
          if (matrix) {
            out[8] = m.m[0][0], out[9] = m.m[0][1], out[10] = m.m[3][0], out[11] = 0.0f;
            out[12] = m.m[1][0], out[13] = m.m[1][1], out[14] = m.m[3][1], out[15] = 0.0f;
          }
          out += stride;
          if (++n >= kRectBatch) flush();
        }
        pen.x = x1 - 1.0f;
      } else {
        pen.x += missingAdvance;
      }
      if (next != 0) {
        auto it = data.m_table.find(c << 16 | next);
        float kern = static_cast<float>(it != data.m_table.end() ? it->second : static_cast<int>(data.m_defaultKern));
        kern *= font->m_aspect;
        if (pixelFont) kern = static_cast<float>(static_cast<int>(kern + (kern < 0.0f ? -0.5f : 0.5f)));
        pen.x += kern * scale;
      }
      c = next;
    }
    if (rc && n > 0) flush();
  }

  if (endPos) {
    EVec2 end{pen.x, top + lineHeight};
    if (pixelFont) {
      if (snapX) end.x = SnapToPixel(end.x);
      if (snapY) end.y = SnapToPixel(end.y);
    }
    *endPos = EVec2{end.x * invSx, end.y * invSy};
  }
  if (rc) rc->DisableRasterModes(0x8000, 0);
}
}  // namespace

// 80314298
// Selects page `page` of the current size: its shader, keeping the renderer's depth test as it was.
void ERFont::SelectPage(ERC* rc, int page) {
  if (m_page == page) return;
  m_page = page;
  const unsigned char zTest[3] = {_ngcRenderer.m_zTest[0], _ngcRenderer.m_zTest[1], _ngcRenderer.m_zTest[2]};
  m_size->m_pages[page]->m_shader->GetShader()->Select(rc, 0);
  rc->ZTest(zTest[0] != 0, zTest[1], zTest[2], 0);
}

// 80314388
// Moves a rectangle's centre to the origin and makes the matrix put it back.
void CenterRectAndShiftMatrix(float* rect, EMat4& m) {
  const float x1 = rect[2], y1 = rect[3];
  const float hw = (x1 - rect[0]) * 0.5f, hh = (y1 - rect[1]) * 0.5f;
  rect[2] = hw;
  rect[3] = hh;
  rect[0] = -hw;
  rect[1] = -hh;
  const float dx = hw - x1, dy = hh - y1;  // minus the old centre
  for (int c = 0; c < 4; ++c) m.m[3][c] -= dx * m.m[0][c] + dy * m.m[1][c];
}

// 803144A4
void ERFont::DoDraw(void* str, bool wide, bool snapX, bool snapY, EVec2& pos, ERC* rc, EVec2* endPos,
                    EWindow* window, EMat4* matrix, EVec2* origin, EFontMatrixType type, fontFXcommand* fx) {
  DrawString(this, str, wide, snapX, snapY, pos, rc, endPos, window, matrix, origin, type, fx, false);
}

// 80315340
void ERFont::DoDrawNormalize(void* str, bool wide, bool snapX, bool snapY, EVec2& pos, ERC* rc, EVec2* endPos,
                             EWindow* window, EMat4* matrix, EVec2* origin, EFontMatrixType type,
                             fontFXcommand* fx) {
  DrawString(this, str, wide, snapX, snapY, pos, rc, endPos, window, matrix, origin, type, fx, true);
}

// 80316280
EVec2 ERFont::DoGetStringSize(void* str, bool wide, bool normalize, EWindow* window) {
  EVec2 pos{0.0f, 0.0f}, size;
  if (normalize) DoDrawNormalize(str, wide, false, false, pos, nullptr, &size, window, nullptr, nullptr, 0, nullptr);
  else DoDraw(str, wide, false, false, pos, nullptr, &size, window, nullptr, nullptr, 0, nullptr);
  return size;
}

// 80316354
void ERFont::SnapPosToPixel(EVec2& pos, bool snapX, bool snapY, EWindow*) {
  if (snapX) pos.x = SnapToPixel(pos.x);
  if (snapY) pos.y = SnapToPixel(pos.y);
}

// 8031640C
void ERFont::SnapPosToPixelNormalize(EVec2& pos, bool snapX, bool snapY, EWindow* window) {
  if (!snapX && !snapY) return;
  if (!window) window = EWindow::m_pCurrentWindow;
  const float sx = window ? window->m_mat.m[0][0] : static_cast<float>(_pGfx->m_screenWidth);
  const float sy = window ? window->m_mat.m[1][1] : static_cast<float>(_pGfx->m_screenHeight);
  pos.x *= sx;
  pos.y *= sy;
  if (snapX) pos.x = SnapToPixel(pos.x);
  if (snapY) pos.y = SnapToPixel(pos.y);
  pos.x *= 1.0f / sx;
  pos.y *= 1.0f / sy;
}

// 80316588
// Aligns the text on pos (right/centre alignment measure it first). Pixel fonts snap the position
// to whole pixels: the aligned edge here, otherwise the pen in DoDraw.
void ERFont::DoDrawAlign(ERC* rc, void* str, bool wide, EVec2 pos, EFontAlignX alignX, EFontAlignY alignY,
                         EVec2* endPos, EMat4* matrix, EFontMatrixType type, fontFXcommand* fx, bool normalize) {
  EVec2 size{0.0f, 0.0f};
  if (alignX == EFontAlignX::Right || alignX == EFontAlignX::Center) {
    size = DoGetStringSize(str, wide, normalize, nullptr);
  } else if (alignY == EFontAlignY::Bottom || alignY == EFontAlignY::Center) {
    char empty[1] = {};
    size = DoGetStringSize(empty, false, normalize, nullptr);
  }
  bool snapX = false, snapY = false;
  if (m_size->m_pixelFont == 1) {
    snapX = alignX == EFontAlignX::Right;
    snapY = alignY == EFontAlignY::Bottom;
    if (normalize) SnapPosToPixelNormalize(pos, snapX, snapY, nullptr);
    else SnapPosToPixel(pos, snapX, snapY, nullptr);
    snapX = !snapX;
    snapY = !snapY;
  }
  EVec2 at = pos;
  if (alignX == EFontAlignX::Right) at.x = pos.x - size.x;
  else if (alignX == EFontAlignX::Center) at.x = pos.x - size.x * 0.5f;
  if (alignY == EFontAlignY::Bottom) at.y = pos.y - size.y;
  else if (alignY == EFontAlignY::Center) at.y = pos.y - size.y * 0.5f;
  if (normalize) DoDrawNormalize(str, wide, snapX, snapY, at, rc, endPos, nullptr, matrix, &pos, type, fx);
  else DoDraw(str, wide, snapX, snapY, at, rc, endPos, nullptr, matrix, &pos, type, fx);
}

// 8031682C
// Converts UTF-8 (up to three bytes per character) to UCS-2 and draws that.
void ERFont::Draw(ERC* rc, char* str, EVec2& pos, EFontAlignX alignX, EFontAlignY alignY, EVec2* endPos,
                  EMat4* matrix, EFontMatrixType type, fontFXcommand* fx, bool normalize) {
  const std::size_t len = std::strlen(str);
  auto* text = new unsigned short[len + 1];
  const auto* s = reinterpret_cast<const unsigned char*>(str);
  const unsigned char* end = s + len + 1;
  unsigned short* o = text;
  while (s < end) {
    const unsigned char c = *s;
    if (!(c & 0x80)) {
      *o++ = c;
      s += 1;
    } else if ((c & 0xe0) == 0xc0) {
      *o++ = static_cast<unsigned short>((s[1] & 0x3f) | (c & 0x1f) << 6);
      s += 2;
    } else if ((c & 0xf0) == 0xe0) {
      *o++ = static_cast<unsigned short>((s[2] & 0x3f) | c << 12 | (s[1] & 0x3f) << 6);
      s += 3;
    } else {
      s += 1;  // a stray byte: the original never advanced past it (it would hang)
    }
  }
  DoDrawAlign(rc, text, true, pos, alignX, alignY, endPos, matrix, type, fx, normalize);
  delete[] text;
}

namespace {
// DrawDs: a shadow `offset` screen pixels down and right in black (alpha `alpha`), then the text.
void DrawDropShadow(ERFont* font, ERC* rc, void* str, bool wide, EVec2& pos, EFontAlignX alignX,
                    EFontAlignY alignY, EVec2* endPos, float offset, float alpha, bool normalize) {
  const float off = offset >= 1.0f ? static_cast<float>(static_cast<int>(offset)) : 1.0f;
  EVec2 end{off * (1.0f / static_cast<float>(_pGfx->m_screenWidth)) + pos.x,
            off * (1.0f / static_cast<float>(_pGfx->m_screenHeight)) + pos.y};
  const EVec4 saved = font->m_color;
  font->m_color = EVec4{0.0f, 0.0f, 0.0f, alpha};
  font->DoDrawAlign(rc, str, wide, end, alignX, alignY, nullptr, nullptr, 0, nullptr, normalize);
  font->m_color = saved;
  font->DoDrawAlign(rc, str, wide, pos, alignX, alignY, &end, nullptr, 0, nullptr, normalize);
  if (endPos) *endPos = end;
}
}  // namespace

// 803169A0
// The string is drawn as single bytes (no UTF-8 decoding, unlike Draw).
void ERFont::DrawDs(ERC* rc, char* str, EVec2& pos, EFontAlignX alignX, EFontAlignY alignY, EVec2* endPos,
                    float offset, float alpha, bool normalize) {
  DrawDropShadow(this, rc, str, false, pos, alignX, alignY, endPos, offset, alpha, normalize);
}
// 80316BE4
void ERFont::DrawDs(ERC* rc, unsigned short* str, EVec2& pos, EFontAlignX alignX, EFontAlignY alignY,
                    EVec2* endPos, float offset, float alpha, bool normalize) {
  DrawDropShadow(this, rc, str, true, pos, alignX, alignY, endPos, offset, alpha, normalize);
}

// 80316EE8
// The line advance: the height of a line times the font's line-spacing factor.
float ERFont::GetLineSpacing(EWindow* window, bool normalize) {
  EVec2 pos{0.0f, 0.0f}, end;
  char empty[1] = {};
  if (normalize) DoDrawNormalize(empty, false, false, false, pos, nullptr, &end, window, nullptr, nullptr, 0, nullptr);
  else DoDraw(empty, false, false, false, pos, nullptr, &end, window, nullptr, nullptr, 0, nullptr);
  const float factor = AsFloat(m_data.m_lineSpacing);
  const float spacing = factor > 0.0f ? factor : 1.0f;
  return end.y * spacing / static_cast<float>(static_cast<int>(m_data.m_lineBottom - m_data.m_cellRaise));
}

// 80316E28
// Loads the page shaders of the current size.
void ERFont::LoadFont() {
  for (EFontPage* page : m_size->m_pages)
    if (!page->m_shader) page->m_shader = static_cast<ERShader*>(_shaderman.AddRef(page->m_shaderId, nullptr, 0));
}

// 80316EB4
void ERFont::Select(ERC*) {
  LoadFont();
  m_page = -1;
}

// 80317980
float ERFont::GetYSize() { return m_pointSize; }
// 80317988
float ERFont::GetAspect() { return m_aspect; }
// 80317A00
void ERFont::SetColor(EVec4& color) { m_color = color; }
// 80317A28
void ERFont::SetColor(float gray) { m_color = EVec4{gray, gray, gray, gray}; }
// 80317A40
EVec4* ERFont::GetColor() { return &m_color; }

// 80317990
EVec2 ERFont::GetStringSize(char* str, EWindow* window, bool normalize) {
  return DoGetStringSize(str, false, normalize, window);
}
// 803179C8
EVec2 ERFont::GetStringSize(unsigned short* str, EWindow* window, bool normalize) {
  return DoGetStringSize(str, true, normalize, window);
}

// 80317A48
void ERFont::Draw(ERC* rc, unsigned short* str, EVec2& pos, EFontAlignX alignX, EFontAlignY alignY, EVec2* endPos,
                  EMat4* matrix, EFontMatrixType type, fontFXcommand* fx, bool normalize) {
  DoDrawAlign(rc, str, true, pos, alignX, alignY, endPos, matrix, type, fx, normalize);
}

// 80317AC0
unsigned short ERFont::GetChar(void* str, bool wide, int i) {
  if (wide) return static_cast<unsigned short*>(str)[i];
  return static_cast<unsigned char*>(str)[i];
}
