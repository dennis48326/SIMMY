// Port of engine/e_3dwindow.cpp, continued: the frustum tests. The planes are in world space with the
// inside on the negative side; for a box, each plane's m_planeMin/m_planeMax hold the byte offsets of
// the box corner nearest to and farthest from it (CalcViewportStructures).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

namespace {
float Corner(const EBound3& box, const unsigned char* offsets, const float* plane) {
  const auto* b = reinterpret_cast<const unsigned char*>(&box);
  float c[3];
  for (int i = 0; i < 3; ++i) std::memcpy(&c[i], b + offsets[i], 4);
  return c[2] * plane[2] + c[0] * plane[0] + c[1] * plane[1] + plane[3];
}
}  // namespace

// 802E55D0
// True when the sphere is outside: beyond a side plane, before the near plane or past the far one.
bool E3DWindow::Test(EBoundSphere& sphere) {
  const float* s = reinterpret_cast<const float*>(&sphere);
  auto dist = [&](const float* p) { return s[2] * p[2] + s[0] * p[0] + s[1] * p[1] + p[3]; };
  const float r = s[3];
  if (dist(m_planes[0]) > r || dist(m_planes[1]) > r) return true;
  const float nearDist = dist(m_planes[4]);
  if (m_near - r > nearDist) return true;
  if (dist(m_planes[3]) > r || dist(m_planes[2]) > r) return true;
  return m_far + r < nearDist;
}

// 802E56D8
// 1 when the box is outside (wholly beyond a side plane, before the near plane or past the far one);
// otherwise 4 when it crosses the camera plane, | 2 when part of it is nearer than the clamped near
// distance.
unsigned int E3DWindow::Test(EBound3& box) {
  for (int p : {0, 1}) {
    if (Corner(box, m_planeMin[p], m_planes[p]) > 0.0f && Corner(box, m_planeMax[p], m_planes[p]) > 0.0f) return 1;
  }
  const float n0 = Corner(box, m_planeMin[4], m_planes[4]);
  const float n1 = Corner(box, m_planeMax[4], m_planes[4]);
  if (m_near > n0 && m_near > n1) return 1;
  for (int p : {3, 2}) {
    if (Corner(box, m_planeMin[p], m_planes[p]) > 0.0f && Corner(box, m_planeMax[p], m_planes[p]) > 0.0f) return 1;
  }
  if (n0 > m_far && n1 > m_far) return 1;
  unsigned int r = (n0 <= 0.0f || n1 <= 0.0f) ? 4u : 0u;
  if (n0 <= m_nearClamped || n1 <= m_nearClamped) r |= 2;
  return r;
}

// 802E5998
// The box as a rectangle on the ground (x and y only) against the four side planes: 1 when wholly
// beyond one of them, else 0. Used for shadows, which lie on the ground whatever their bounds' height.
int E3DWindow::TestAsRect(EBound3& box) {
  const auto* b = reinterpret_cast<const unsigned char*>(&box);
  auto corner = [&](const unsigned char* offsets, const float* plane) {
    float x, y;
    std::memcpy(&x, b + offsets[0], 4);
    std::memcpy(&y, b + offsets[1], 4);
    return x * plane[0] + y * plane[1] + plane[3];
  };
  for (int p : {0, 1, 3, 2}) {
    if (corner(m_planeMin[p], m_planes[p]) > 0.0f && corner(m_planeMax[p], m_planes[p]) > 0.0f) return 1;
  }
  return 0;
}

// 802E6818
void E3DWindow::SetFrustumType(int type) {
  if (m_frustumType != type) {
    m_frustumType = type;
    CalcViewportStructures();
  }
}

// 802E61FC
// The point's screen position through the view-projection (x, y by the input viewport's scale and
// centre); 0 behind the eye.
RET_UNKNOWN E3DWindow::TransformToScreen(EVec3& p, EVec2& out) {
  const EMat4& m = m_lookAtProj;
  float v[4];
  for (int c = 0; c < 4; ++c) v[c] = p.x * m.m[0][c] + p.y * m.m[1][c] + p.z * m.m[2][c] + m.m[3][c];
  if (v[3] <= 0.0f) return 0;
  const float k = 1.0f / v[3];
  out.x = v[0] * k * m_vpIn.m_scale[0] + m_vpIn.m_center[0];
  out.y = v[1] * k * m_vpIn.m_scale[1] + m_vpIn.m_center[1];
  return 1;
}

// 802E62C0
// The same with the depth: (clip z + zOffset) / w.
RET_UNKNOWN E3DWindow::TransformToScreen(EVec3& p, EVec3& out, float zOffset) {
  const EMat4& m = m_lookAtProj;
  float v[4];
  for (int c = 0; c < 4; ++c) v[c] = p.x * m.m[0][c] + p.y * m.m[1][c] + p.z * m.m[2][c] + m.m[3][c];
  if (v[3] <= 0.0f) return 0;
  const float k = 1.0f / v[3];
  out.x = v[0] * k * m_vpIn.m_scale[0] + m_vpIn.m_center[0];
  out.y = v[1] * k * m_vpIn.m_scale[1] + m_vpIn.m_center[1];
  out.z = (v[2] + zOffset) * k;
  return 1;
}
