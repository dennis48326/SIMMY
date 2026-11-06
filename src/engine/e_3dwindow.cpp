// Port of engine/e_3dwindow.cpp (partial): construction, projection, camera, viewport, Select and the
// frustum planes. The orthographic UI projections, the culling tests other than spheres and the
// screen/world transforms are not ported yet.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

// 802E3AF0
E3DWindow::E3DWindow() {
  m_projection.Id();
  m_lookAt.Id();
  m_lookAtProj.Id();
  m_frustumType = 1;
  m_viewport = m_input;
  m_viewportOut = m_output;
  m_fov = 0.0f;
  m_aspect = 1.0f;
}

// 802E3BD4
void E3DWindow::SetProjection(float fovY, float aspect, float zNear, float zFar) {
  EMat4 p;
  p.Projection(fovY, aspect, zNear, zFar);
  m_fov = fovY;
  m_aspect = aspect;
  m_near = zNear;
  m_nearClamped = zNear < 3.2f ? zNear : 3.2f;
  m_far = zFar;
  m_projection = p;
  ProjectionMatrixChanged();
}

// 802E3CEC
void E3DWindow::CopyProjection(E3DWindow* other) {
  m_fov = other->m_fov;
  m_aspect = other->m_aspect;
  m_near = other->m_near;
  m_nearClamped = other->m_nearClamped;
  m_far = other->m_far;
  m_projection = other->m_projection;
  ProjectionMatrixChanged();
}

// 802E4294
// An orthographic projection (no field of view); m_normalizedProjection maps the same space to the
// 0..1 rectangle coordinates (y down) used by 2D drawing.
void E3DWindow::SetOrthoProjection(float left, float right, float bottom, float top, float zNear, float zFar) {
  EMat4 p;
  p.Ortho(left, right, bottom, top, zNear, zFar);
  m_fov = 0.0f;
  m_nearClamped = zNear;
  m_far = zFar;
  m_projection = p;
  m_near = zNear;
  m_aspect = (right - left) / (bottom - top);
  ProjectionMatrixChanged();
  m_normalizedProjection = m_projection;
  for (int c = 0; c < 4; ++c) {
    m_normalizedProjection.m[0][c] *= 0.5f;
    m_normalizedProjection.m[1][c] *= -0.5f;
  }
  m_normalizedProjection.m[3][0] = m_normalizedProjection.m[3][0] * 0.5f + 0.5f;
  m_normalizedProjection.m[3][1] = m_normalizedProjection.m[3][1] * -0.5f + 0.5f;
}

// 802E44D8
void E3DWindow::SetLookAt(EMat4& lookAt) {
  m_lookAt = lookAt;
  m_lookAtPos.Invert(m_lookAt);
  LookAtMatrixChanged();
}

// 802E4598
void E3DWindow::SetLookAtPos(EMat4& lookAtPos) {
  m_lookAtPos = lookAtPos;
  m_lookAt.Invert(m_lookAtPos);
  LookAtMatrixChanged();
}

// 802E4658
void E3DWindow::SetLookAt(EVec3& eye, EVec3& target, EVec3& up) {
  m_lookAtPos.LookAtPos(eye, target, up);
  m_lookAt.Invert(m_lookAtPos);
  LookAtMatrixChanged();
}

// 802E46A0
// The part of the window the 3D view covers, in input coordinates; it is also the clip rectangle.
void E3DWindow::SetViewport(TRect<float>& viewport) {
  m_viewport = viewport;
  SetClip(viewport);
  CalcViewport();
}

// 802E46FC
// The viewport in output coordinates, through the window matrix.
void E3DWindow::CalcViewport() {
  m_viewportOut.x0 = m_viewport.x0 * m_mat.m[0][0] + m_mat.m[3][0];
  m_viewportOut.y0 = m_viewport.y0 * m_mat.m[1][1] + m_mat.m[3][1];
  m_viewportOut.x1 = m_viewport.x1 * m_mat.m[0][0] + m_mat.m[3][0];
  m_viewportOut.y1 = m_viewport.y1 * m_mat.m[1][1] + m_mat.m[3][1];
  CalcViewportStructures();
}

// 802E4760
void E3DWindow::CalcViewportInv() {
  m_viewport.x1 = (m_viewportOut.x1 - m_mat.m[3][0]) / m_mat.m[0][0];
  m_viewport.y1 = (m_viewportOut.y1 - m_mat.m[3][1]) / m_mat.m[1][1];
  m_viewport.x0 = (m_viewportOut.x0 - m_mat.m[3][0]) / m_mat.m[0][0];
  m_viewport.y0 = (m_viewportOut.y0 - m_mat.m[3][1]) / m_mat.m[1][1];
  CalcViewportStructures();
}

namespace {
// 802E47D4
// For a plane normal: the byte offsets (into an EBound3: min x, y, z at 0, 4, 8, max at 12, 16, 20)
// of the box corner nearest to the plane's inside and of the one farthest.
void setClipPlaneOffsets(unsigned char* nearest, unsigned char* farthest, const EVec3& n) {
  for (int i = 0; i < 3; ++i) {
    const auto minOff = static_cast<unsigned char>(i * 4), maxOff = static_cast<unsigned char>(12 + i * 4);
    if (n[i] < 0.0f) nearest[i] = maxOff, farthest[i] = minOff;
    else nearest[i] = minOff, farthest[i] = maxOff;
  }
}
}  // namespace

// 802E4830
// Viewports for both rectangles, the camera-space corners of the clip rectangle on the near plane,
// and the world-space frustum planes through the camera and pairs of corners (plus the near plane).
// Frustum types 2 and 3 replace one pair of opposite side planes by the diagonal plane.
void E3DWindow::CalcViewportStructures() {
  _pGfx->ComputeViewport(m_vpIn, m_viewport);
  _pGfx->ComputeViewport(m_vpOut, m_viewportOut);
  const float zNear = _pGfx->GetNearZVal();
  const float sx = 1.0f / m_vpIn.m_scale[0], sy = 1.0f / m_vpIn.m_scale[1], sz = 1.0f / m_vpIn.m_scale[2];
  const float z = (zNear - m_vpIn.m_center[2]) * sz;
  auto corner = [&](float cx, float cy, EVec3& out) {
    EVec4 v{(cx - m_vpIn.m_center[0]) * sx, (cy - m_vpIn.m_center[1]) * sy, z, 1.0f};
    const EVec4 r = v * m_projectionInv;
    out = EVec3{r.x, r.y, r.z};
  };
  corner(m_clip.x0, m_clip.y0, m_corners[0]);
  corner(m_clip.x1, m_clip.y0, m_corners[1]);
  corner(m_clip.x0, m_clip.y1, m_corners[2]);
  corner(m_clip.x1, m_clip.y1, m_corners[3]);

  // A plane through the camera with camera-space normal a x b, in world space.
  auto plane = [&](int index, const EVec3& a, const EVec3& b) {
    Vec n{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    if (n.x != 0.0f || n.y != 0.0f || n.z != 0.0f) PSVECNormalize(&n, &n);
    const auto& w = m_lookAtPos.m;
    const EVec3 wn{n.z * w[2][0] + n.x * w[0][0] + n.y * w[1][0], n.z * w[2][1] + n.x * w[0][1] + n.y * w[1][1],
                   n.z * w[2][2] + n.x * w[0][2] + n.y * w[1][2]};
    float* p = m_planes[index];
    p[0] = wn.x, p[1] = wn.y, p[2] = wn.z;
    p[3] = -(wn.z * m_lookPos.z + wn.x * m_lookPos.x + wn.y * m_lookPos.y);
    setClipPlaneOffsets(m_planeMin[index], m_planeMax[index], wn);
  };
  const EVec3* c = m_corners;
  if (m_frustumType != 2) {
    plane(0, c[0], c[2]);
    plane(3, c[2], c[3]);
  }
  if (m_frustumType != 3) {
    plane(1, c[3], c[1]);
    plane(2, c[1], c[0]);
  }
  {  // the near plane: the camera's -z
    const auto& w = m_lookAtPos.m;
    const EVec3 wn{-w[2][0], -w[2][1], -w[2][2]};
    float* p = m_planes[4];
    p[0] = wn.x, p[1] = wn.y, p[2] = wn.z;
    p[3] = -(wn.z * m_lookPos.z + wn.x * m_lookPos.x + wn.y * m_lookPos.y);
    setClipPlaneOffsets(m_planeMin[4], m_planeMax[4], wn);
  }
  if (m_frustumType == 2) {
    plane(0, c[0], c[3]);
    std::memcpy(m_planes[3], m_planes[0], sizeof m_planes[0]);
    setClipPlaneOffsets(m_planeMin[3], m_planeMax[3], EVec3{m_planes[0][0], m_planes[0][1], m_planes[0][2]});
  } else if (m_frustumType == 3) {
    plane(1, c[3], c[0]);
    std::memcpy(m_planes[2], m_planes[1], sizeof m_planes[1]);
    setClipPlaneOffsets(m_planeMin[2], m_planeMax[2], EVec3{m_planes[1][0], m_planes[1][1], m_planes[1][2]});
  }
}

// 802E5DA0
void E3DWindow::ProjectionMatrixChanged() {
  m_projectionInv = m_projection;
  float* rows[4] = {m_projectionInv.m[0], m_projectionInv.m[1], m_projectionInv.m[2], m_projectionInv.m[3]};
  MatrixInvert(rows, 4);
  CalcLookAtDotProjection();
}

// 802E5E80
// The camera position (translation of camera-to-world) and forward direction (its -z axis).
void E3DWindow::LookAtMatrixChanged() {
  CalcLookAtDotProjection();
  m_lookPos = EVec3{m_lookAtPos.m[3][0], m_lookAtPos.m[3][1], m_lookAtPos.m[3][2]};
  const auto& w = m_lookAtPos.m;
  m_lookDir.x = w[2][0] * -1.0f + w[0][0] * 0.0f + w[1][0] * 0.0f;
  m_lookDir.y = w[2][1] * -1.0f + w[0][1] * 0.0f + w[1][1] * 0.0f;
  m_lookDir.z = w[2][2] * -1.0f + w[0][2] * 0.0f + w[1][2] * 0.0f;
}

// 802E5F7C
void E3DWindow::CalcLookAtDotProjection() {
  m_lookAtProj.Mult4x4(m_lookAt, m_projection);
  CalcViewportStructures();
}

// 802E5FBC
// Makes the window current (EWindow::Select), refreshes the viewport from the output rectangle and
// sends viewport, view and projection matrices; then selects the window again so that the window
// matrix and scissor are the last state set.
void E3DWindow::Select(ERC* rc) {
  EWindow::Select(rc);
  m_viewportOut = m_output;
  OutputCoordinatesChanged();
  CalcViewportInv();
  rc->Viewport(&m_vpOut);
  rc->ViewMatrix(&m_lookAt, 0, 0.0f);
  rc->ProjectionMatrix(&m_projection);
  EWindow::Select(rc);
}

// 802E60A0
void E3DWindow::InputCoordinatesChanged() { CalcViewportInv(); }
// 802E60C0
void E3DWindow::OutputCoordinatesChanged() { CalcViewport(); }

// 802E7650
TRect<float> E3DWindow::GetViewport() const { return m_viewport; }
// 802E767C
EVec3* E3DWindow::GetLookPos() { return &m_lookPos; }
// 802E7684
EVec3* E3DWindow::GetLookDir() { return &m_lookDir; }
// 802E768C
EMat4* E3DWindow::GetLookAt() { return &m_lookAt; }
// 802E7694
const EMat4* E3DWindow::GetProjection() const { return &m_projection; }
// 802E769C
const EMat4* E3DWindow::GetNormalizedProjection() const { return &m_normalizedProjection; }
// 802E76A4
EWindow* E3DWindow::Cast3DWindow() { return this; }
