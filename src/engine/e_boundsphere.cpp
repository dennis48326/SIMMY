// Port of engine/e_boundsphere.cpp (partial): EBoundSphere::Combine. ComputeFast has no caller in the
// paths the port reaches.
#include "simmy/types_gen.hpp"

#include <cmath>

// 8035A034
// The sphere through the far sides of `a` and `b` (along the line between their centres); `a` itself
// when that sphere is smaller than `a`, `b` when it is smaller than `b`; concentric spheres give the
// larger radius. Written in GC's order, so `this` may be `b` (then the `b` case cannot happen: `b` is
// already overwritten when it is compared).
EBoundSphere* EBoundSphere::Combine(EBoundSphere& a, EBoundSphere& b) {
  const float* pa = &a.x;
  const float* pb = &b.x;
  float* o = &x;
  const float d[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
  const double len = std::sqrt(static_cast<double>(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));  // PSVECMag
  if (len == 0.0) {
    o[0] = pa[0], o[1] = pa[1], o[2] = pa[2];
    o[3] = pb[3] < pa[3] ? pa[3] : pb[3];
    return this;
  }
  const float inv = static_cast<float>(1.0 / len);
  const float n[3] = {d[0] * inv, d[1] * inv, d[2] * inv};
  const float ra = pa[3], rb = pb[3];
  const float hiPt[3] = {rb * n[0] + pb[0], rb * n[1] + pb[1], rb * n[2] + pb[2]};
  const float loPt[3] = {pa[0] - ra * n[0], pa[1] - ra * n[1], pa[2] - ra * n[2]};
  o[0] = (loPt[0] + hiPt[0]) * 0.5f;
  o[1] = (loPt[1] + hiPt[1]) * 0.5f;
  o[2] = (loPt[2] + hiPt[2]) * 0.5f;
  const float e[3] = {loPt[0] - o[0], loPt[1] - o[1], loPt[2] - o[2]};
  const double r = std::sqrt(static_cast<double>(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]));
  o[3] = static_cast<float>(r);
  if (static_cast<double>(pa[3]) <= r) {
    if (r < static_cast<double>(pb[3])) o[0] = pb[0], o[1] = pb[1], o[2] = pb[2], o[3] = pb[3];
  } else {
    o[0] = pa[0], o[1] = pa[1], o[2] = pa[2], o[3] = pa[3];
  }
  return this;
}
