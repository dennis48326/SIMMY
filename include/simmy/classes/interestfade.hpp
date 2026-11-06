// @class TrapezoidFade after EVec3
// @class WallFadeParms
// @class ECullPlane
// @class PyramidFade after EVec3 ECullPlane
// @class LineFade after EVec3
// Cut-away walls (engine/interestfade.cpp, game/eroom.cpp): a TrapezoidFade is the ground area between
// the camera's eye and the player's point of interest (the cursor), widening toward the eye; things
// whose bound box centre lies inside it fade (alpha by how deep inside, from the planes' distances).
// WallFadeParms are its tuning for walls (two sets: "DC" with an interactor, "Classic" without).
#pragma once

// 0xC8 bytes (_interestFade, 804B0DDC).
struct TrapezoidFade {
  void Setup();  // 80308344
  double CalcAlpha(EBound3& bounds);  // 803091EC

  float m_eye[3] = {};           // +0x00 the camera's eye
  float m_dir[3] = {};           // +0x0C (3D fades: the camera's frame row the far side tilts along)
  float m_interest[3] = {};      // +0x18 the point of interest
  float m_origin[3] = {};        // +0x24 the eye (on the ground for 2D fades): positions are relative to it
  float m_box[6] = {};           // +0x30 the fade's own bound box (along it)
  float m_nearWidth = 0;         // +0x48 the width at the eye
  float m_farWidth = 0;          // +0x4C the width at the point of interest
  float m_range = 0;             // +0x50 the depth over which alpha goes from 0 to 1
  float m_offset = 0;            // +0x54 distances start this far outside the planes
  float m_invRange = 0;          // +0x58
  float m_full = 1.0f;           // +0x5C from this alpha on, fully there
  float m_least = 0;             // +0x60 below this alpha ...
  float m_gone = 0;              // +0x64 ... it is this
  int m_3d = 0;                  // +0x68 the far side tilts (else on the ground)
  int m_planeCount = 0;          // +0x6C 3 (5 for 3D fades with near and far sides)
  int m_enabled = 0;             // +0x70
  int m_dirty = 0;               // +0x74 Setup before the next CalcAlpha
  float m_planes[5][4] = {};     // +0x78 x, y, z, d; inside is negative
};

// The wall fade tuning (0x30 bytes): its fade's depth range is from `min` to `max` (outside the planes).
struct WallFadeParms {
  WallFadeParms(char* name, float min, float max, bool threeD);  // 800302CC
  void Validate();  // 80030340

  int m_enabled = 1;             // +0x00
  float m_max = 0;               // +0x04
  float m_min = 0;               // +0x08
  float m_least = 0.05f;         // +0x0C
  float m_full = 1.0f;           // +0x10
  float m_gone = 0;              // +0x14
  float m_farWidth = 2.55f;      // +0x18
  float m_nearWidth = 15.0f;     // +0x1C
  float m_range = 0;             // +0x20 max - min
  float m_invRange = 0;          // +0x24
  int m_3d = 0;                  // +0x28
};

extern TrapezoidFade _interestFade;  // 804B0DDC

// A culling plane with its box corners (engine/e_cullplane.cpp, 0x18 bytes): x, y, z, d, then per
// axis the EBound3 component (0..5) of the corner most behind the plane (+0x10) and most in front (+0x13).
struct ECullPlane {
  void SetPlane(const float* plane);  // 80362F28
  float m_plane[4] = {};
  unsigned char m_back[3] = {};
  unsigned char m_front[3] = {};
  unsigned char m_pad[2] = {};
};

// Objects between the camera and the point of interest fade (engine/interestfade.cpp, 0xB0 bytes):
// a pyramid from the eye to a quad standing on the ground around the interest point (as wide as the
// cursor, as high as it). Things that reach into it fade to a floor alpha, unless they are the
// interaction's object or its container, or (with depth limits) lie beyond the interest point or hold it.
struct PyramidFade {
  PyramidFade();  // 80309388
  void Setup(const float* eye, const float* quad);  // 80309480 (quad: four corners, x y z each)
  void DeriveDepthData();  // 8030996C
  int IsOutsideFadeVolume(const void* instance, const float* bounds) const;  // 80309AA0
  void SetEnabled(bool enabled);  // 80309DD8

  const void* m_exclude[2] = {};  // +0x00 the interaction's object's instance and its container's
  ECullPlane m_planes[5];         // +0x08 eye-side, top, far, side, side
  float m_centre[2] = {};         // +0x80 the quad's bottom edge's middle (x, y)
  float m_depthD[2] = {};         // +0x88
  float m_depthN[2] = {};         // +0x90
  unsigned char m_depthIndex[2] = {};  // +0x98
  int m_enabled = 1;              // +0x9C
  int m_depthLimits = 1;          // +0xA0
  float m_minHeight = 0;          // +0xA4 only things taller than this fade (DAT_804B0F48)
  float m_floor = 0;              // +0xA8 the alpha they fade to (DAT_804B0F4C)
  int m_instant = 0;              // +0xAC fade at once (DAT_804B0F50)
};

// Things crossed by the line from the eye to the interest point fade (0x40 bytes).
struct LineFade {
  void Setup(const float* from, const float* to);  // 80309E08
  int IsLineIntersected(const float* bounds) const;  // 80309F3C
  void SetEnabled(bool enabled);  // 8030A0C4

  float m_start[3] = {};    // +0x00
  float m_end[3] = {};      // +0x0C
  float m_delta[3] = {};    // +0x18
  float m_inv[3] = {};      // +0x24
  float m_point[3] = {};    // +0x30 the interest point (things over it fade too)
  int m_enabled = 0;        // +0x3C
};

extern PyramidFade g_pyramidFade;  // 804B0EA4
extern LineFade g_lineFade;        // 804B0F54
