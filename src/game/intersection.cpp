// Port of game/intersection.cpp: swept circles against circles, points, segments and rectangles.
// Each test moves a circle (centre `c`, radius `r`) by `v` and reports the first contact in an
// Intersection (contact point, normal, fraction of the move), replacing it only by an earlier one.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>

// 8004938C
// The real roots of a x^2 + b x + c, smaller first (numerically stable form); their count.
int SolveQuadratic(float a, float b, float c, float* roots) {
  const float disc = b * b - a * 4.0f * c;
  if (disc < 0.0f) return 0;
  float s = std::sqrt(disc);
  if (b < 0.0f) s = -s;
  int count = a != 0.0f ? 1 : 0;
  const float q = (b + s) * -0.5f;
  if (count) roots[0] = q / a;
  if (q != 0.0f) {
    const float other = c / q;
    const float first = roots[0];
    roots[1] = other;
    if (other < first) roots[0] = other, roots[1] = first;
    ++count;
  }
  return count;
}

// 80049470
// Circle (c1, r1) moving by v1 against circle (c2, r2) moving by v2: the first time they touch, the
// normal from the second to the first, and the contact on the first's edge.
int GetCircleCircleIX(EVec2& c1, float r1, EVec2& v1, EVec2& c2, float r2, EVec2& v2, Intersection& out) {
  const float dx = c2.x - c1.x, dy = c2.y - c1.y;
  const float vx = v2.x - v1.x, vy = v2.y - v1.y;
  const float b = dx * vx + dy * vy;
  float roots[2] = {0.0f, 0.0f};
  const int n = SolveQuadratic(vx * vx + vy * vy, b + b, (dx * dx + dy * dy) - (r2 + r1) * (r2 + r1), roots);
  if (n == 0 || roots[0] < -0.001f || 1.001f < roots[0]) return 0;
  float t = roots[0];
  if (t < 0.0f) t = 0.0f;
  else if (t > 1.0f) t = 1.0f;
  out.m_t = t;
  out.m_normal.x = (v1.x * t + c1.x) - (c2.x + v2.x * t);
  out.m_normal.y = (v1.y * t + c1.y) - (c2.y + v2.y * t);
  const float l = std::sqrt(out.m_normal.x * out.m_normal.x + out.m_normal.y * out.m_normal.y);
  if (l != 0.0f) out.m_normal.x *= 1.0f / l, out.m_normal.y *= 1.0f / l;
  out.m_point.x = c1.x + out.m_normal.x * r1;
  out.m_point.y = c1.y + out.m_normal.y * r1;
  return 1;
}

// 80049700
// Against the line through a-b: the circle's leading edge (offset by r against the normal) crossing
// it within the move and within the segment (with a little slack). Without `twoSided` only a move
// against the normal (the segment's left side, a to b) counts.
int GetCircleSegmentIX(EVec2& c, float r, EVec2& v, EVec2& a, EVec2& b, Intersection& out, bool twoSided) {
  const float ex = b.x - a.x, ey = b.y - a.y;
  float nx = -ey, ny = ex;
  const float inv = 1.0f / std::sqrt(nx * nx + ny * ny);
  nx = nx * -inv, ny = ny * -inv;
  float vn = v.x * nx + v.y * ny;
  if (twoSided && 0.0f < vn) nx = -nx, ny = -ny, vn = -vn;
  if (!(vn < 0.0f)) return 0;
  float px = c.x + nx * -r, py = c.y + ny * -r;
  float t = ((px - a.x) * nx + (py - a.y) * ny) / -vn;
  if (t < -0.001f || 1.001f < t) return 0;
  if (t < 0.0f) t = 0.0f;
  else if (t > 1.0f) t = 1.0f;
  py += v.y * t;
  px += v.x * t;
  const float along = (ex * (px - a.x) + ey * (py - a.y)) * inv * inv;
  if (along < -0.001f || 1.001f < along) return 0;
  out.m_point = EVec2{px, py};
  out.m_t = t;
  out.m_normal = EVec2{nx, ny};
  return 1;
}

// 800499F8
// Against a still point: a circle of radius 0.
int GetCirclePointIX(EVec2& c, float r, EVec2& v, EVec2& p, Intersection& out) {
  EVec2 still{0.0f, 0.0f};
  return GetCircleCircleIX(c, r, v, p, 0.0f, still, out);
}

// 80049A38
// Against the segment or, missing it, its end points (the earlier contact).
int GetCircleSegmentClosedIX(EVec2& c, float r, EVec2& v, EVec2& a, EVec2& b, Intersection& out, bool twoSided) {
  if (GetCircleSegmentIX(c, r, v, a, b, out, twoSided)) return 1;
  int hit = 0;
  Intersection ia, ib;
  if (GetCirclePointIX(c, r, v, a, ia)) {
    if (ia.m_t < out.m_t) out = ia, hit = 1;
  }
  if (GetCirclePointIX(c, r, v, b, ib)) {
    if (ib.m_t < out.m_t) out = ib, hit = 1;
  }
  return hit;
}

// 80049BB0
// Against the axis-aligned rectangle with corners p0 and p1: its four sides, then (missing them all)
// its four corners; the earliest contact.
int GetCircleRectangleIX(EVec2& c, float r, EVec2& v, EVec2& p0, EVec2& p1, Intersection& out) {
  EVec2 c01{p0.x, p1.y}, c10{p1.x, p0.y};
  Intersection ix;
  bool hit = false;
  auto take = [&](int found) {
    if (found && ix.m_t < out.m_t) {
      out = ix;
      hit = true;
    }
  };
  take(GetCircleSegmentIX(c, r, v, p0, c10, ix, false));
  take(GetCircleSegmentIX(c, r, v, c10, p1, ix, false));
  take(GetCircleSegmentIX(c, r, v, p1, c01, ix, false));
  take(GetCircleSegmentIX(c, r, v, c01, p0, ix, false));
  if (hit) return 1;
  take(GetCirclePointIX(c, r, v, p0, ix));
  take(GetCirclePointIX(c, r, v, c10, ix));
  take(GetCirclePointIX(c, r, v, p1, ix));
  take(GetCirclePointIX(c, r, v, c01, ix));
  return hit ? 1 : 0;
}
