// Port of engine/quad.cpp: point-in-quad tests on the ground plane (x, y; the edge normals ignore z).
// Quad::Draw (8030AD30) is the camera's debug drawing (ESimsCam::DrawDebug) and is not ported.
#include "simmy/types_gen.hpp"

#include <cmath>

namespace {
// The edge from `a` to `b` turned a quarter (its inward normal: (b.y - a.y, -(b.x - a.x))), normalized.
EVec3 EdgeNormal(const EVec3& a, const EVec3& b) {
  EVec3 n{b.y - a.y, -(b.x - a.x), 0.0f};
  if (n.x != 0.0f || n.y != 0.0f) {
    const float k = 1.0f / std::sqrt(n.x * n.x + n.y * n.y);
    n.x *= k, n.y *= k;
  }
  return n;
}
}  // namespace

// 8030AA9C
// Inside when the point is on the negative side of all four edges' normals.
int Quad::IsInQuad(EVec3* p) const {
  for (unsigned int i = 1;; ++i) {
    const EVec3& a = m_points[i - 1];
    const EVec3& b = m_points[i & 3];
    const EVec3 n = EdgeNormal(a, b);
    const float d = n.x * (p->x - a.x) + n.y * (p->y - a.y) + n.z * (p->z - a.z);
    if (0.0f <= d) return 0;
    if (3 < i) return 1;
  }
}

// 8030ABC4
// Moved back across each edge it is outside of (in turn, along that edge's normal); true if it moved.
int Quad::PinToQuad(EVec3* p) const {
  int moved = 0;
  for (unsigned int i = 1; i <= 4; ++i) {
    const EVec3& a = m_points[i - 1];
    const EVec3& b = m_points[i & 3];
    const EVec3 n = EdgeNormal(a, b);
    float d = n.x * (p->x - a.x) + n.y * (p->y - a.y) + n.z * (p->z - a.z);
    if (0.0f <= d) {
      d = -d;
      moved = 1;
      p->x += n.x * d;
      p->y += n.y * d;
      p->z += n.z * d;
    }
  }
  return moved;
}
