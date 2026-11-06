// Port of engine/e_mirrorportal.cpp: a mirror (the CAS room's) drawn as a portal. See
// emirrorportal.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>

// 802F236C
// Four corners, a 45 degree field of view. GC: the vertices and the two matrices are allocated
// (0x140 / 0x40 / 0x40 bytes, 16-aligned); here they are members.
EMirrorPortal::EMirrorPortal() { SetNumCorners(4); }

// 802F241C
EMirrorPortal::~EMirrorPortal() {
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
}

// 802F249C
void EMirrorPortal::SetNumCorners(int count) { m_numCorners = count; }

// 802F24A4
EVec3 EMirrorPortal::GetCorner(int i) const { return m_corners[i]; }

// 802F24CC
void EMirrorPortal::SetCorner(int i, EVec3& corner) { m_corners[i] = corner; }

// 802F24F0
void EMirrorPortal::SetMirrorShader(unsigned int shaderId) {
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
  m_shader = static_cast<ERShader*>(_shaderman.AddRef(shaderId, nullptr, 0));
}

// 802F2550
void EMirrorPortal::SetFieldOfView(float fov) { m_fov = fov; }

// 802F2558
// Once per frame: the reflected camera and the quad; the whole screen white at the nearest depth, then
// the quad at its depth (seen by the real camera, the field of view scaled by the screen's height over
// its width, near 0.0001, far 1000), so only the quad's area takes what is drawn next; the window
// (the reflected camera) selected.
void EMirrorPortal::StartRendering(E3DWindow& win, ERC* rc) {
  if (m_rendering) return;
  m_rendering = 1;
  CalcMirrorMatrix();
  BuildPortalGeometry();
  PushPortal(win);
  InitRC(rc);
  EVec2 p0{0.0f, 0.0f}, p1{1.0f, 1.0f}, uv0{0.0f, 0.0f}, uv1{0.0f, 0.0f};
  EVec4 white{1.0f, 1.0f, 1.0f, 1.0f};
  rc->Rect(p0, p1, uv0, uv1, white, 0.0f);
  const float fovY = m_fov * static_cast<float>(_pGfx->m_screenHeight) / static_cast<float>(_pGfx->m_screenWidth);
  EMat4 projection;
  projection.Projection(fovY, _pGfx->GetScreenAspect(), 1.0e-4f, 1000.0f);
  rc->ProjectionMatrix(&projection);
  rc->ModelMatrixId();
  rc->TriStrip(m_verts, 4);
  win.Select(rc);
}

// 802F2744
// The camera back, the depth cleared, the quad drawn with the glass shader (depth test and write).
void EMirrorPortal::StopRendering(E3DWindow& win, ERC* rc) {
  if (!m_rendering) return;
  PopPortal(win);
  win.Select(rc);
  InitRC(rc);
  rc->ZClear(0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  if (m_shader) {
    if (EShader* shader = m_shader->GetShader()) shader->Select(rc, 0);
    rc->ZTest(true, 2, 0, 0);
    rc->ModelMatrixId();
    rc->TriStrip(m_verts, 4);
  }
  m_rendering = 0;
}

// 802F289C
// The reflection in the plane of the first three corners: turned so the plane's normal is the z axis
// (the y axis when it is within 5 degrees of -z), about the first corner, that axis negated, and
// turned back.
void EMirrorPortal::CalcMirrorMatrix() {
  const EVec3& c0 = m_corners[0];
  const EVec3& c1 = m_corners[1];
  const EVec3& c2 = m_corners[2];
  const EVec3 e1{c1.x - c0.x, c1.y - c0.y, c1.z - c0.z};
  const EVec3 e2{c2.x - c1.x, c2.y - c1.y, c2.z - c1.z};
  Vec n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
  if (n.x != 0.0f || n.y != 0.0f || n.z != 0.0f) PSVECNormalize(&n, &n);
  Vec axis{0.0f, 0.0f, 1.0f};
  int flip = 2;
  float angle = std::acos(PSVECDotProduct(&n, &axis));
  if (angle > 3.0543261f) {  // 175 degrees
    axis = Vec{0.0f, 1.0f, 0.0f};
    flip = 1;
    angle = std::acos(PSVECDotProduct(&n, &axis));
  }
  Vec r{n.y * axis.z - n.z * axis.y, n.z * axis.x - n.x * axis.z, n.x * axis.y - n.y * axis.x};
  if (r.x != 0.0f || r.y != 0.0f || r.z != 0.0f) PSVECNormalize(&r, &r);
  EVec3 rotAxis{r.x, r.y, r.z};
  EMat4 to;
  to.Rotate(rotAxis, angle);
  EVec3 origin{-c0.x, -c0.y, -c0.z};
  to.PreTranslate(origin);
  EVec3 scale{1.0f, 1.0f, 1.0f};
  (flip == 1 ? scale.y : scale.z) = -1.0f;
  to.PostScale(scale);
  EMat4 back;
  back.Rotate(rotAxis, -angle);
  EVec3 corner = c0;
  back.PostTranslate(corner);
  m_mirror.Mult4x4(to, back);
}

// 802F2BA4
// The render state for the portal's own draws: no texture, no geometry modes, raster modes 0x48, the
// plain blend, depth always (written), no alpha test, the plain combiner.
void EMirrorPortal::InitRC(ERC* rc) {
  rc->Texture(nullptr, 0);
  rc->SetGeometryModes(0);
  rc->SetRasterModes(0x48, 0);
  rc->SetBlendMode(2, 2, 2, 1, 0.0f, 0);
  rc->ZTest(true, 1, 0, 0);
  rc->AlphaTest(false, 5, 0.5f, 0);
  rc->SetCombineMode(0, 0);
}

// 802F2CD0
// The window's camera kept, then reflected (the mirror applied before it).
void EMirrorPortal::PushPortal(E3DWindow& win) {
  m_savedLookAt = win.m_lookAt;
  EMat4 reflected;
  reflected.Mult4x4(m_mirror, win.m_lookAt);
  win.SetLookAt(reflected);
}

// 802F2DA0
void EMirrorPortal::PopPortal(E3DWindow& win) { win.SetLookAt(m_savedLookAt); }

// 802F2DCC
// The quad's strip: corners 0, 1, 3, 2 (uvs 0,0 / 1,0 / 0,1 / 1,1), black, opaque.
void EMirrorPortal::BuildPortalGeometry() {
  for (int i = 0; i < m_numCorners && i < 4; ++i) {
    const int corner = i == 2 ? 3 : i == 3 ? 2 : i;
    EGEVert& v = m_verts[i];
    v = EGEVert();
    v.pos[0] = m_corners[corner].x;
    v.pos[1] = m_corners[corner].y;
    v.pos[2] = m_corners[corner].z;
    v.pos[3] = 1.0f;
    v.color[3] = 0xff;
  }
  m_verts[0].uv[0] = 0.0f, m_verts[0].uv[1] = 0.0f;
  m_verts[1].uv[0] = 1.0f, m_verts[1].uv[1] = 0.0f;
  m_verts[2].uv[0] = 0.0f, m_verts[2].uv[1] = 1.0f;
  m_verts[3].uv[0] = 1.0f, m_verts[3].uv[1] = 1.0f;
}
