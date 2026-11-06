// Port of engine/interestfade.cpp (TrapezoidFade, PyramidFade, LineFade) and engine/e_cullplane.cpp.
// Not ported: 3D trapezoids (no wall fade tuning uses them).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cmath>

TrapezoidFade _interestFade;  // 804B0DDC

namespace {
void Normalize2(float* v) {
  const float l = std::sqrt(v[0] * v[0] + v[1] * v[1]);
  if (l != 0.0f) {
    const float k = 1.0f / l;
    v[0] *= k;
    v[1] *= k;
  }
}
float Dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
}  // namespace

// 80308344
// From the eye toward the point of interest: the far plane through the point of interest (plane 1),
// and the two sides through the ends of the near and far widths (planes 0 and 2), all relative to the
// origin (the eye, on the ground for 2D fades).
void TrapezoidFade::Setup() {
  if (!m_dirty) return;
  m_origin[0] = m_eye[0];
  m_origin[1] = m_eye[1];
  m_origin[2] = m_3d ? m_eye[2] : 0.0f;
  const float dx = m_interest[0] - m_eye[0], dy = m_interest[1] - m_eye[1];
  const float dz = m_3d ? m_interest[2] - m_eye[2] : 0.0f;
  float* far = m_planes[1];
  far[0] = dx, far[1] = dy, far[2] = dz, far[3] = 1.0f;
  const float length = std::sqrt(dz * dz + dx * dx + dy * dy + 1.0f);
  const float half = std::max(m_farWidth, m_nearWidth) * 0.5f + m_range + m_offset;
  m_box[0] = -half, m_box[1] = 0.0f, m_box[2] = 0.0f;
  m_box[3] = half, m_box[4] = length + m_range + m_offset, m_box[5] = 0.0f;
  if (!m_3d) {
    Normalize2(far);
    far[2] = 0.0f;
    far[3] = -length;
    // The sides' direction across the fade.
    const float px = far[1], py = -far[0];
    const float nearHalf = m_nearWidth * 0.5f, farHalf = m_farWidth * 0.5f;
    // Left: from the far end to the near end.
    const float l0x = m_eye[0] - nearHalf * px, l0y = m_eye[1] - nearHalf * py, l0z = m_eye[2];
    const float l1x = m_interest[0] - farHalf * px, l1y = m_interest[1] - farHalf * py;
    float* left = m_planes[0];
    left[0] = l0y - l1y, left[1] = -(l0x - l1x), left[2] = 0.0f;
    Normalize2(left);
    const float lrel[3] = {l0x - m_eye[0], l0y - m_eye[1], l0z - m_eye[2]};
    left[3] = -Dot3(lrel, left);
    // Right: from the near end to the far end.
    const float r0x = m_eye[0] + nearHalf * px, r0y = m_eye[1] + nearHalf * py, r0z = m_eye[2];
    const float r1x = m_interest[0] + farHalf * px, r1y = m_interest[1] + farHalf * py;
    float* right = m_planes[2];
    right[0] = r1y - r0y, right[1] = -(r1x - r0x), right[2] = 0.0f;
    Normalize2(right);
    const float rrel[3] = {r0x - m_eye[0], r0y - m_eye[1], r0z - m_eye[2]};
    right[3] = -Dot3(rrel, right);
  } else {
    SIMMY_STUB("TrapezoidFade::Setup: 3D fades (tilted sides, near and far planes)", 0x80308344);
  }
  m_dirty = 0;
}

// 803091EC
// 1 (there) unless enabled and the box's centre is inside: then how far it is inside its nearest side,
// past the offset, over the range; under the least alpha, the gone alpha.
double TrapezoidFade::CalcAlpha(EBound3& bounds) {
  if (!m_enabled) return 1.0;
  if (m_dirty) Setup();
  const float* b = reinterpret_cast<const float*>(&bounds);
  float c[3] = {(b[0] + b[3]) * 0.5f, (b[1] + b[4]) * 0.5f, (b[2] + b[5]) * 0.5f};
  if (!m_3d) {
    c[0] -= m_origin[0];
    c[1] -= m_origin[1];
    c[2] = 0.0f - m_origin[2];
  }
  double best = -100.0;
  for (int i = 0; i < m_planeCount; ++i) {
    const double d = static_cast<float>(Dot3(c, m_planes[i]) + m_planes[i][3]) - m_offset;
    if (best < d) best = d;
  }
  const double alpha = static_cast<float>(best * m_invRange);
  if (alpha < m_full) return m_least <= alpha ? alpha : m_gone;
  return 1.0;
}

// ---------------------------------------------------------------------------------------------------
// ECullPlane, PyramidFade, LineFade

PyramidFade g_pyramidFade;  // 804B0EA4
LineFade g_lineFade;        // 804B0F54

namespace {
void Normalize3(float* v) {
  if (v[0] != 0.0f || v[1] != 0.0f || v[2] != 0.0f) {
    const float k = 1.0f / std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] *= k, v[1] *= k, v[2] *= k;
  }
}
void Cross(const float* a, const float* b, float* out) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}
// A plane through `point` with normal `n` (normalised first): n, -point . n.
void PlaneThrough(ECullPlane& plane, float* n, const float* point) {
  Normalize3(n);
  const float neg[3] = {-point[0], -point[1], -point[2]};
  const float p[4] = {n[0], n[1], n[2], Dot3(neg, n)};
  plane.SetPlane(p);
}
float Side(const ECullPlane& p, const float* b, const unsigned char* idx) {
  return b[idx[2]] * p.m_plane[2] + b[idx[0]] * p.m_plane[0] + b[idx[1]] * p.m_plane[1] + p.m_plane[3];
}
}  // namespace

// 80362F28
// Per axis: the box component of the corner most behind the plane (the minimum where the normal is
// positive) and of the one most in front.
void ECullPlane::SetPlane(const float* plane) {
  for (int i = 0; i < 4; ++i) m_plane[i] = plane[i];
  for (int i = 0; i < 3; ++i) {
    if (m_plane[i] <= 0.0f) {
      m_front[i] = static_cast<unsigned char>(i);
      m_back[i] = static_cast<unsigned char>(i + 3);
    } else {
      m_front[i] = static_cast<unsigned char>(i + 3);
      m_back[i] = static_cast<unsigned char>(i);
    }
  }
}

// 80309388
PyramidFade::PyramidFade() {
  const float zero[4] = {};
  for (ECullPlane& p : m_planes) p.SetPlane(zero);
  m_depthLimits = 1;
  m_instant = 0;
  m_enabled = 1;
  m_exclude[0] = m_exclude[1] = nullptr;
}

// 80309480
// From `eye` to the quad (corners 0, 1 on the ground, 2 over 1, 3 over 0): the eye-side plane through
// the eye, 1 and 2; the top through the eye, 2 and 3; the far side through 0 and 1; the two sides; the
// depth limits about the bottom edge's middle.
void PyramidFade::Setup(const float* eye, const float* quad) {
  const float* q0 = quad;
  const float* q1 = quad + 3;
  const float* q2 = quad + 6;
  const float* q3 = quad + 9;
  const float a[3] = {q1[0] - eye[0], q1[1] - eye[1], q1[2] - eye[2]};
  const float b[3] = {q2[0] - eye[0], q2[1] - eye[1], q2[2] - eye[2]};
  float n0[3];
  Cross(a, b, n0);
  PlaneThrough(m_planes[0], n0, eye);
  float n1[3];
  Cross(n0, b, n1);
  PlaneThrough(m_planes[4], n1, eye);
  const float c[3] = {q3[0] - eye[0], q3[1] - eye[1], q3[2] - eye[2]};
  float n2[3];
  Cross(n1, c, n2);
  PlaneThrough(m_planes[1], n2, eye);
  const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  float n3[3];
  Cross(n0, d, n3);
  PlaneThrough(m_planes[2], n3, q1);
  float n4[3];
  Cross(a, n0, n4);
  PlaneThrough(m_planes[3], n4, eye);
  for (int i = 0; i < 2; ++i) m_centre[i] = (q0[i] + q1[i]) * 0.5f;
  DeriveDepthData();
}

// 8030996C
// Along x and y: which side of the far plane the interest point's beyond lies (its sign, the box
// component to test); disabled, nothing passes (everything is outside).
void PyramidFade::DeriveDepthData() {
  const float* far = m_planes[2].m_plane;
  for (int i = 0; i < 2; ++i) {
    if (far[i] <= 0.0f) {
      m_depthIndex[i] = static_cast<unsigned char>(i + 3);
      m_depthN[i] = -1.0f;
    } else {
      m_depthIndex[i] = static_cast<unsigned char>(i);
      m_depthN[i] = 1.0f;
    }
    m_depthD[i] = -m_depthN[i] * m_centre[i];
  }
  if (!m_enabled) {
    m_depthD[0] = 1.0f;
    m_depthN[0] = 0.0f;
  }
}

// 80309AA0
// 1 unless the box reaches inside the eye-side and top planes and, with depth limits, does not reach
// beyond the interest point (either axis) or outside a side, and does not hold the interest point;
// without, inside the far plane and both sides. The excluded instances are always outside.
int PyramidFade::IsOutsideFadeVolume(const void* instance, const float* b) const {
  if (Side(m_planes[1], b, m_planes[1].m_back) > 0.0f || Side(m_planes[0], b, m_planes[0].m_back) > 0.0f) return 1;
  if (m_depthLimits) {
    if (0.0f < b[m_depthIndex[0]] * m_depthN[0] + m_depthD[0] || 0.0f < b[m_depthIndex[1]] * m_depthN[1] + m_depthD[1] ||
        0.0f < Side(m_planes[3], b, m_planes[3].m_back) || 0.0f < Side(m_planes[4], b, m_planes[4].m_back))
      return 1;
    if (b[0] <= m_centre[0] && m_centre[0] <= b[3] && b[1] <= m_centre[1] && m_centre[1] <= b[4]) return 1;
    return instance == m_exclude[0] || instance == m_exclude[1] ? 1 : 0;
  }
  if (0.0f < Side(m_planes[2], b, m_planes[2].m_back) || 0.0f < Side(m_planes[3], b, m_planes[3].m_back) ||
      0.0f < Side(m_planes[4], b, m_planes[4].m_back) || instance == m_exclude[0] || instance == m_exclude[1])
    return 1;
  return 0;
}

// 80309DD8
void PyramidFade::SetEnabled(bool enabled) {
  if (static_cast<int>(enabled) != m_enabled) {
    m_enabled = enabled ? 1 : 0;
    DeriveDepthData();
  }
}

// 80309E08
// The segment and its inverse direction (10000 for a direction near 0); disabled, nothing hits.
void LineFade::Setup(const float* from, const float* to) {
  const float f[3] = {from[0], from[1], from[2]};
  const float t[3] = {to[0], to[1], to[2]};
  for (int i = 0; i < 3; ++i) {
    m_start[i] = f[i];
    m_end[i] = t[i];
    m_point[i] = t[i];
    m_delta[i] = t[i] - f[i];
  }
  for (int i = 0; i < 3; ++i) m_inv[i] = std::fabs(m_delta[i]) <= 5e-06f ? 10000.0f : 1.0f / m_delta[i];
  if (!m_enabled) {
    m_inv[0] = m_inv[1] = m_inv[2] = 10000.0f;
    m_point[0] = m_point[1] = -10000.0f;
  }
}

// 80309F3C
// Whether the segment crosses one of the box's faces (per axis the face it meets first).
int LineFade::IsLineIntersected(const float* b) const {
  for (int i = 0; i < 3; ++i) {
    const int j = (i + 1) % 3, k = (i + 2) % 3;
    const float inv = m_inv[i];
    const float t = ((inv <= 0.0f ? b[3 + i] : b[i]) - m_start[i]) * inv;
    if (t < 0.0f || t > 1.0f) continue;
    const float pj = t * m_delta[j] + m_start[j];
    const float pk = t * m_delta[k] + m_start[k];
    if (b[j] <= pj && pj <= b[3 + j] && b[k] <= pk && pk <= b[3 + k]) return 1;
  }
  return 0;
}

// 8030A0C4
void LineFade::SetEnabled(bool enabled) {
  if (m_enabled != static_cast<int>(enabled)) {
    m_enabled = enabled ? 1 : 0;
    Setup(m_start, m_end);
  }
}
