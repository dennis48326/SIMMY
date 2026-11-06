// @class Quad after EVec3
// A convex quadrilateral on the ground (engine/quad.cpp, 0x30 bytes): four corners in order. The camera
// keeps the classic cursor's free area in one (ESimsCam +0x360, E3DWindow::Get3DQuadFromScreenTrapAtZ).
// An edge's inside is to the left of the corners' order, as the camera builds them.
#pragma once

struct Quad {
  int IsInQuad(EVec3* p) const;   // 8030AA9C
  int PinToQuad(EVec3* p) const;  // 8030ABC4
  void Draw(ERC *, unsigned int) const;  // 8030AD30

  EVec3 m_points[4];  // +0x00
};
