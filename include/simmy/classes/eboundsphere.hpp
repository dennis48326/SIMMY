// @class EBoundSphere
// A bounding sphere (engine/e_boundsphere.cpp): centre and radius, 0x10 bytes. Ported code also passes
// float[4] arrays (EInstance::m_sphere) where the GameCube takes an EBoundSphere&.
#pragma once

struct EBoundSphere {
  float x = 0.0f, y = 0.0f, z = 0.0f, r = 0.0f;
  EBoundSphere() = default;
  // This sphere becomes the smallest one around `a` and `b` (either of them when it holds the other).
  EBoundSphere* Combine(EBoundSphere& a, EBoundSphere& b);  // 8035A034
  RET_UNKNOWN ComputeFast(EVec3 *, int);  // 8035A258
};
