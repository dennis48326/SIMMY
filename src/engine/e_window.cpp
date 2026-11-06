// Port of engine/e_window.cpp: 2D views.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>

EWindow* EWindow::m_pCurrentWindow = nullptr;
EWindow* EWindow::m_pCurrent3DWindow = nullptr;
float EWindow::m_globalScale[2] = {1, 1};
float EWindow::m_globalOffset[2] = {0, 0};

// 802FFB98
EWindow::EWindow() {
  CalcWindowMat();
  CalcClip();
}

// 802FFC80
EWindow::~EWindow() {
  if (m_pCurrentWindow == this) m_pCurrentWindow = nullptr;
  if (m_pCurrent3DWindow == this) m_pCurrent3DWindow = nullptr;
}

// 802FFCF0
void EWindow::SetRenderSurface(ERenderSurface* surface, int output) {
  if (m_surface == surface && m_outputIndex == output) return;
  m_surface = surface;
  m_outputIndex = output;
  TRect<float> out;
  if (!surface) {
    _pGfx->GetOutputRect(out, output);
    ScaleOutputRectForScreenshot(&out);
  } else {
    surface->GetOutputRect(out);
  }
  SetOutputCoordinates(out);
}

// 802FFD98
// Big screenshots render the screen as a grid of tiles: each tile scales the output by (sx, sy) and
// shifts it by (ox, oy) tiles.
void EWindow::ScaleForScreenshot(bool enable, int sx, int sy, int ox, int oy) {
  if (!enable) {
    m_globalScale[0] = m_globalScale[1] = 1.0f;
    m_globalOffset[0] = m_globalOffset[1] = 0.0f;
  } else {
    m_globalScale[0] = static_cast<float>(sx);
    m_globalScale[1] = static_cast<float>(sy);
    m_globalOffset[0] = static_cast<float>(ox);
    m_globalOffset[1] = static_cast<float>(oy);
  }
}

// 802FFF10
void EWindow::ScaleOutputRectForScreenshot(TRect<float>* r) {
  const float w = r->x1 - r->x0, h = r->y1 - r->y0;
  r->x1 = w * (m_globalScale[0] - 1.0f) + r->x1;
  r->y1 = h * (m_globalScale[1] - 1.0f) + r->y1;
  const float dx = w * m_globalOffset[0], dy = h * m_globalOffset[1];
  r->x1 -= dx;
  r->y1 -= dy;
  r->x0 -= dx;
  r->y0 -= dy;
}

// 802FFF94
void EWindow::ScaleScissorRectForScreenshot(TRect<float>* r, TRect<float>* screen) {
  const float dx = ((screen->x1 - screen->x0) + 1.0f) * m_globalOffset[0];
  const float dy = ((screen->y1 - screen->y0) + 1.0f) * m_globalOffset[1];
  r->x0 = r->x0 * m_globalScale[0] - dx;
  r->y0 = r->y0 * m_globalScale[1] - dy;
  r->x1 = r->x1 * m_globalScale[0] - dx;
  r->y1 = r->y1 * m_globalScale[1] - dy;
}

// 80300040
void EWindow::Select(ERC* rc) {
  TRect<float> out, savedOut, savedVisible;
  if (m_dirty || !m_surface) {
    if (!m_surface) {
      _pGfx->GetOutputRect(out, m_outputIndex);
      SetOutputCoordinates(out);
      savedOut = m_output;
      savedVisible = m_clipVisible;
      ScaleOutputRectForScreenshot(&out);
    } else {
      m_surface->GetOutputRect(out);
    }
    SetOutputCoordinates(out);
    m_dirty = 0;
  }
  m_pCurrentWindow = this;
  m_pCurrent3DWindow = Cast3DWindow();
  rc->RenderSurface(m_surface, m_outputIndex);
  TRect<float> scissor;
  if (!m_surface) {
    const bool unclipped = savedOut.x0 == savedVisible.x0 && savedOut.y0 == savedVisible.y0 &&
                           savedOut.x1 == savedVisible.x1 && savedOut.y1 == savedVisible.y1;
    if (unclipped) {
      TRect<float> screen;
      _pGfx->GetOutputRect(screen, m_outputIndex);
      _pGfx->GetScissorRect(&scissor, screen, screen);
    } else {
      TRect<float> screen;
      _pGfx->GetScissorRect(&scissor, savedVisible, savedOut);
      _pGfx->GetScissorRect(&screen, savedOut, savedOut);
      ScaleScissorRectForScreenshot(&scissor, &screen);
    }
  } else {
    _pGfx->GetScissorRect(&scissor, m_clipVisible, m_output);
  }
  rc->Scissor(&scissor);
  rc->WindowMatrix(&m_mat);
}

// 80300330
void EWindow::TransformToPixel(EVec2& in, EVec2& out) {
  out.x = in.x * m_mat.m[0][0] + m_mat.m[3][0];
  out.y = in.y * m_mat.m[1][1] + m_mat.m[3][1];
}

// 8030035C
void EWindow::CalcWindowMat() {
  float sx = m_input.x1 - m_input.x0;
  sx = sx == 0.0f ? 1.0f : (m_output.x1 - m_output.x0) / sx;
  float sy = m_input.y1 - m_input.y0;
  sy = sy == 0.0f ? 1.0f : (m_output.y1 - m_output.y0) / sy;
  EVec3 t{-m_input.x0, -m_input.y0, 0.0f};
  m_mat.Translate(t);
  EVec3 s{sx, sy, 1.0f};
  m_mat.PostScale(s);
  EVec3 o{m_output.x0, m_output.y0, 0.0f};
  m_mat.PostTranslate(o);
  WindowMatrixChanged();
}

// 803004A0
void EWindow::CalcClip() {
  m_clipOut.x0 = m_clip.x0 * m_mat.m[0][0] + m_mat.m[3][0];
  m_clipOut.y0 = m_clip.y0 * m_mat.m[1][1] + m_mat.m[3][1];
  m_clipOut.x1 = m_clip.x1 * m_mat.m[0][0] + m_mat.m[3][0];
  m_clipOut.y1 = m_clip.y1 * m_mat.m[1][1] + m_mat.m[3][1];
  // Intersect with the output rectangle (which may be flipped).
  if (m_output.x1 <= m_output.x0) {
    m_clipVisible.x0 = std::min(m_output.x0, m_clipOut.x0);
    m_clipVisible.x1 = std::max(m_output.x1, m_clipOut.x1);
  } else {
    m_clipVisible.x0 = std::max(m_output.x0, m_clipOut.x0);
    m_clipVisible.x1 = std::min(m_output.x1, m_clipOut.x1);
  }
  if (m_output.y1 <= m_output.y0) {
    m_clipVisible.y0 = std::min(m_output.y0, m_clipOut.y0);
    m_clipVisible.y1 = std::max(m_output.y1, m_clipOut.y1);
  } else {
    m_clipVisible.y0 = std::max(m_output.y0, m_clipOut.y0);
    m_clipVisible.y1 = std::min(m_output.y1, m_clipOut.y1);
  }
}

// 803005AC
void EWindow::CalcClipInv() {
  m_clip.x1 = (m_clipOut.x1 - m_mat.m[3][0]) / m_mat.m[0][0];
  m_clip.y1 = (m_clipOut.y1 - m_mat.m[3][1]) / m_mat.m[1][1];
  m_clip.x0 = (m_clipOut.x0 - m_mat.m[3][0]) / m_mat.m[0][0];
  m_clip.y0 = (m_clipOut.y0 - m_mat.m[3][1]) / m_mat.m[1][1];
}

// 80300600
void EWindow::SetClip(TRect<float>& clip) {
  m_clip = clip;
  CalcClip();
}

namespace {
bool SameRect(const TRect<float>& a, const TRect<float>& b) {
  return a.x0 == b.x0 && a.y0 == b.y0 && a.x1 == b.x1 && a.y1 == b.y1;
}
}  // namespace

// 80300648
void EWindow::SetInputCoordinates(TRect<float>& r) {
  if (SameRect(m_input, r)) return;
  m_input = r;
  CalcWindowMat();
  CalcClipInv();
  InputCoordinatesChanged();
}

// 80300710
void EWindow::SetOutputCoordinates(TRect<float>& r) {
  if (SameRect(m_output, r)) return;
  m_output = r;
  CalcWindowMat();
  CalcClip();
  OutputCoordinatesChanged();
}

// 803008A0
void EWindow::SetInputCoordinatesAndClip(TRect<float>& r) {
  SetInputCoordinates(r);
  SetClip(r);
}

// 803008DC
void EWindow::SetRect(TRect<float>& r) {
  SetInputCoordinates(r);
  SetOutputCoordinates(r);
  SetClip(r);
}

// 80300924
void EWindow::Transform(EVec2& in, EVec2& out) {
  out.x = in.x * m_mat.m[0][0] + m_mat.m[3][0];
  out.y = in.y * m_mat.m[1][1] + m_mat.m[3][1];
}
// 80300950
void EWindow::Transform(float x, float y, float& ox, float& oy) {
  ox = x * m_mat.m[0][0] + m_mat.m[3][0];
  oy = y * m_mat.m[1][1] + m_mat.m[3][1];
}
// 80300974
void EWindow::TransformInv(EVec2& in, EVec2& out) {
  out.x = (in.x - m_mat.m[3][0]) / m_mat.m[0][0];
  out.y = (in.y - m_mat.m[3][1]) / m_mat.m[1][1];
}
// 803009A8
void EWindow::TransformInv(float x, float y, float& ox, float& oy) {
  ox = (x - m_mat.m[3][0]) / m_mat.m[0][0];
  oy = (y - m_mat.m[3][1]) / m_mat.m[1][1];
}
// 803009D4
void EWindow::TransformScale(float x, float y, float& ox, float& oy) {
  ox = x * m_mat.m[0][0];
  oy = y * m_mat.m[1][1];
}
// 803009F0
void EWindow::TransformScale(EVec2& in, EVec2& out) {
  out.x = in.x * m_mat.m[0][0];
  out.y = in.y * m_mat.m[1][1];
}

// 80300A14
bool EWindow::ClipTest(EVec2& p) { return ClipTest(p.x, p.y); }
// 80300A3C
bool EWindow::ClipTest(float x, float y) {
  return m_clip.x0 <= x && x < m_clip.x1 && m_clip.y0 <= y && y < m_clip.y1;
}
// 80300A84
// True when the rectangle is not entirely inside the clip.
bool EWindow::ClipTest(TRect<float>& r) {
  const bool inside = m_clip.x0 <= r.x0 && r.x1 <= m_clip.x1 && m_clip.y0 <= r.y0 && r.y1 <= m_clip.y1;
  return !inside;
}

// 80300AE8
EWindow* EWindow::GetCurrentWindow() { return m_pCurrentWindow; }
// 80300AF0
EWindow* EWindow::GetCurrent3DWindow() { return m_pCurrent3DWindow; }
// 80300AF8
void EWindow::WindowMatrixChanged() {}
// 80300AFC
void EWindow::InputCoordinatesChanged() {}
// 80300B00
void EWindow::OutputCoordinatesChanged() {}
// 80300B04
EWindow* EWindow::Cast3DWindow() { return nullptr; }
