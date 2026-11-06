// @class EViewport
// Viewport transform as EGraphics::ComputeViewport fills it (0x20 bytes): half extents and centre.
#pragma once

struct EViewport {
  float m_scale[4] = {};   // half width, -half height, half depth range, 1
  float m_center[4] = {};  // centre x, centre y, centre z, 0
};
static_assert(sizeof(EViewport) == 0x20);
