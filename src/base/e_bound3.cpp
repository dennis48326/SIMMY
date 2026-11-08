// Port of base/e_bound3.cpp (partial): EBound3 is an axis-aligned box, six floats (min x y z, max x y z);
// the generated class has no fields, so its data is read through `this`. The box transforms and
// EBound3::Compute(EBoundSphere&) are inlined in their ported callers.
#include "simmy/types_gen.hpp"

#include <cmath>

// 802C5858
// The sphere through the box's corners: its centre, and the distance to the max corner.
void EBound3::CalcBoundSphere(EBoundSphere& out) {
  const float* b = reinterpret_cast<const float*>(this);
  out.y = (b[1] + b[4]) * 0.5f;
  out.z = (b[2] + b[5]) * 0.5f;
  out.x = (b[0] + b[3]) * 0.5f;
  const float d[3] = {out.x - b[3], out.y - b[4], out.z - b[5]};
  out.r = static_cast<float>(std::sqrt(static_cast<double>(d[0] * d[0] + d[1] * d[1] + d[2] * d[2])));  // PSVECMag
}
