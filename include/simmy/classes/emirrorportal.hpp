// @class EMirrorPortal after EVec3 EMat4 EGEVert
// A mirror (engine/e_mirrorportal.cpp, 0x4C bytes): a quad given by four corners. StartRendering
// reflects the window's camera in the quad's plane and lets only the quad's area take the reflected
// scene (the screen first set to the nearest depth, then the quad drawn with its own depth); the
// caller draws the scene; StopRendering puts the camera back, clears the depth and draws the quad with
// the mirror's glass shader, so the room drawn afterwards keeps out of the reflection.
#pragma once

struct EMirrorPortal {
  EMirrorPortal();  // 802F236C
  ~EMirrorPortal();  // 802F241C
  void SetNumCorners(int count);  // 802F249C
  EVec3 GetCorner(int i) const;  // 802F24A4
  void SetCorner(int i, EVec3& corner);  // 802F24CC
  void SetMirrorShader(unsigned int shaderId);  // 802F24F0
  void SetFieldOfView(float fov);  // 802F2550
  void StartRendering(E3DWindow& win, ERC* rc);  // 802F2558
  void StopRendering(E3DWindow& win, ERC* rc);  // 802F2744
  void CalcMirrorMatrix();  // 802F289C
  void InitRC(ERC* rc);  // 802F2BA4
  void PushPortal(E3DWindow& win);  // 802F2CD0
  void PopPortal(E3DWindow& win);  // 802F2DA0
  void BuildPortalGeometry();  // 802F2DCC

  float m_fov = 45.0f;             // +0x00
  EVec3 m_corners[4] = {};         // +0x04
  EGEVert m_verts[4];              // +0x34 (GC: a pointer to 0x140 aligned bytes) the quad, in strip order
  int m_numCorners = 0;            // +0x38
  EMat4 m_savedLookAt;             // +0x3C (GC: a pointer) the window's camera while reflected
  EMat4 m_mirror;                  // +0x40 (GC: a pointer) the reflection in the quad's plane
  ERShader* m_shader = nullptr;    // +0x44 the glass
  int m_rendering = 0;             // +0x48
};
