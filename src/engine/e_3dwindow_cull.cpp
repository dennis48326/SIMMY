// Port of engine/e_3dwindow.cpp, continued: the frustum tests. The planes are in world space with the
// inside on the negative side; for a box, each plane's m_planeMin/m_planeMax hold the byte offsets of
// the box corner nearest to and farthest from it (CalcViewportStructures).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
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

// ---- Screen to ground (the camera's cursor area, ESimsCam::ValidateCursorPosition) ----------------

// 802E67F4
// The view's half extents at unit distance: 1 / the projection's x and y scales.
void E3DWindow::GetFOVLengths(float* x, float* y) {
  *y = 1.0f / m_projection.m[1][1];
  *x = 1.0f / m_projection.m[0][0];
}

// 802E65D8
// The world direction (normalized) through a point of the window (input coordinates); false outside it.
RET_UNKNOWN E3DWindow::TransformToWorld(EVec2& p, EVec3& dir) {
  float sx = (p.x - m_vpIn.m_center[0]) / m_vpIn.m_scale[0];
  float sy = (p.y - m_vpIn.m_center[1]) / m_vpIn.m_scale[1];
  if (sx < -1.0f || 1.0f < sx || sy < -1.0f || 1.0f < sy) return 0;
  float fx, fy;
  GetFOVLengths(&fx, &fy);
  sy *= fy;
  sx *= fx;
  const EMat4& m = m_lookAtPos;
  dir.x = (m.m[0][0] * sx + m.m[1][0] * sy) - m.m[2][0];
  dir.y = (m.m[0][1] * sx + m.m[1][1] * sy) - m.m[2][1];
  dir.z = (m.m[0][2] * sx + m.m[1][2] * sy) - m.m[2][2];
  if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) {
    const float k = 1.0f / std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    dir.x *= k, dir.y *= k, dir.z *= k;
  }
  return 1;
}

// 802E6D90
// Where the line from the eye through `point` meets the plane z = `z`, when it looks down at least ~10
// degrees (sin 0.1736); else its direction across the ground (negated, as seen back from the point) with
// z = `z` - 1 marking it as a direction.
void E3DWindow::ProjectToZPlane(EVec3* out, float z, EVec3& point) {
  EVec3 v{m_lookPos.x - point.x, m_lookPos.y - point.y, m_lookPos.z - point.z};
  if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f) {
    const float k = 1.0f / std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    v.x *= k, v.y *= k, v.z *= k;
  }
  if (0.1736f <= v.z) {
    const float t = (z - m_lookPos.z) / v.z;
    out->x = m_lookPos.x + t * v.x;
    out->y = m_lookPos.y + t * v.y;
    out->z = z;
    return;
  }
  v.z = 0.0f;
  if (v.x != 0.0f || v.y != 0.0f) {
    const float k = 1.0f / std::sqrt(v.x * v.x + v.y * v.y);
    v.x *= k, v.y *= k;
  }
  out->x = -v.x;
  out->y = -v.y;
  out->z = z - 1.0f;
}

// 802E6F64
// The direction from the eye to `point` across the ground (x, y normalized), z = `z` - 1.
void E3DWindow::MakeLineOnPlane(EVec3* out, float z, EVec3& point) {
  EVec3 v{m_lookPos.x - point.x, m_lookPos.y - point.y, 0.0f};
  if (v.x != 0.0f || v.y != 0.0f) {
    const float k = 1.0f / std::sqrt(v.x * v.x + v.y * v.y);
    v.x *= k, v.y *= k;
  }
  out->z = z - 1.0f;
  out->x = -v.x;
  out->y = -v.y;
}

// 802E7210
// The ground under a trapezoid of the window: trap = {top y, top width, bottom y, bottom width} (input
// coordinates, centred on x = 0.5). The top corners (0: right, 1: left) are directions across the ground
// (MakeLineOnPlane), the bottom ones (2: left, 3: right) points on the plane z = `z` (ProjectToZPlane).
// (GC also computes each ray turned by the camera matrix and traces it; both unused.)
void E3DWindow::Get3DQuadFromScreenTrapAtZ(float z, float* trap, Quad* quad) {
  auto rayPoint = [this](float sx, float sy) {
    EVec2 s{sx, sy};
    EVec3 dir{0.0f, 0.0f, 0.0f};
    TransformToWorld(s, dir);
    return EVec3{dir.x + m_lookPos.x, dir.y + m_lookPos.y, dir.z + m_lookPos.z};
  };
  float half = trap[1] * 0.5f;
  EVec3 p = rayPoint(half + 0.5f, trap[0]);
  MakeLineOnPlane(&quad->m_points[0], z, p);
  p = rayPoint(0.5f - half, trap[0]);
  MakeLineOnPlane(&quad->m_points[1], z, p);
  half = trap[3] * 0.5f;
  p = rayPoint(0.5f - half, trap[2]);
  ProjectToZPlane(&quad->m_points[2], z, p);
  p = rayPoint(half + 0.5f, trap[2]);
  ProjectToZPlane(&quad->m_points[3], z, p);
}
