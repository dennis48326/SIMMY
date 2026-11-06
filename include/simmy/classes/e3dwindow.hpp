// @class E3DWindow after EWindow EMat4 EVec3 EVec4 EViewport
// A 3D view (engine/e_3dwindow.cpp): an EWindow plus a camera (look-at matrix), a projection and a
// viewport inside the window. Select() makes it current and hands viewport, view and projection
// matrices to the render context. CalcViewportStructures keeps the world-space frustum planes the
// culling tests (Test) use.
#pragma once

struct E3DWindow : public EWindow {
  E3DWindow();  // 802E3AF0
  void SetProjection(float fovY, float aspect, float zNear, float zFar);  // 802E3BD4
  void CopyProjection(E3DWindow* other);  // 802E3CEC
  void SetOrthoProjectionForUIHD(float, float, float, float, float, float, float, float, float, float, float);  // 802E3DC0
  void SetOrthoProjection(float left, float right, float bottom, float top, float zNear, float zFar);  // 802E4294
  void SetLookAt(EMat4& lookAt);  // 802E44D8
  void SetLookAtPos(EMat4& lookAtPos);  // 802E4598
  void SetLookAt(EVec3& eye, EVec3& target, EVec3& up);  // 802E4658
  void SetViewport(TRect<float>& viewport);  // 802E46A0
  void CalcViewport();  // 802E46FC
  void CalcViewportInv();  // 802E4760
  void CalcViewportStructures();  // 802E4830
  bool Test(EBoundSphere& sphere);  // 802E55D0
  unsigned int Test(EBound3& bounds);  // 802E56D8
  int TestAsRect(EBound3& box);  // 802E5998
  RET_UNKNOWN Test(EVec3 *, int);  // 802E5B1C
  void ProjectionMatrixChanged();  // 802E5DA0
  void LookAtMatrixChanged();  // 802E5E80
  void CalcLookAtDotProjection();  // 802E5F7C
  void Select(ERC* rc) override;  // 802E5FBC
  void InputCoordinatesChanged() override;  // 802E60A0
  void OutputCoordinatesChanged() override;  // 802E60C0
  void CalcTextureProjection(EMat4 &);  // 802E60E0
  RET_UNKNOWN TransformToScreen(EVec3 &, EVec2 &);  // 802E61FC
  RET_UNKNOWN TransformToScreen(EVec3 &, EVec3 &, float);  // 802E62C0
  RET_UNKNOWN CameraTransformToScreen(EVec3 &, EVec3 &);  // 802E63A4
  bool BackCullTest(EVec3 *);  // 802E6478
  RET_UNKNOWN TransformToWorld(EVec2 &, EVec3 &);  // 802E65D8
  void GetNearFar(float *, float *);  // 802E67C0
  void GetFOVLengths(float *, float *);  // 802E67F4
  void SetFrustumType(int type);  // 802E6818
  bool PinToFrustrum(EVec3 *);  // 802E6848
  void CornerToWorld(EVec3 *, int);  // 802E6CA0
  void ProjectToZPlane(EVec3 *, float, EVec3 &);  // 802E6D90
  void MakeLineOnPlane(EVec3 *, float, EVec3 &);  // 802E6F64
  void GetFrustrumQuadAtZ(float, Quad *);  // 802E7078
  void Get3DQuadFromScreenTrapAtZ(float, float *, Quad *);  // 802E7210
  TRect<float> GetViewport() const;  // 802E7650
  EVec3* GetLookPos();  // 802E767C
  EVec3* GetLookDir();  // 802E7684
  EMat4* GetLookAt();  // 802E768C
  const EMat4* GetProjection() const;  // 802E7694
  const EMat4* GetNormalizedProjection() const;  // 802E769C
  EWindow* Cast3DWindow() override;  // 802E76A4

  // GC: the vtable pointer is at +0x9C, after EWindow's members.
  EMat4 m_lookAt;                   // +0x0A0 world -> camera (the view matrix)
  EMat4 m_lookAtPos;                // +0x0E0 camera -> world
  EMat4 m_lookAtProj;               // +0x120 m_lookAt * m_projection
  EMat4 m_projection;               // +0x160
  EMat4 m_projectionInv;            // +0x1A0
  EMat4 m_normalizedProjection;     // +0x1E0 the projection mapped to 0..1 rectangle space (SetOrthoProjection)
  TRect<float> m_viewport;          // +0x220 the viewport in input coordinates
  TRect<float> m_viewportOut;       // +0x230 the same in output coordinates (pixels)
  EViewport m_vpIn;                 // +0x240 ComputeViewport of m_viewport
  EViewport m_vpOut;                // +0x260 ComputeViewport of m_viewportOut (what Select sends)
  EVec3 m_lookPos;                  // +0x280 camera position
  float m_fov = 0;                  // +0x28C vertical, degrees
  EVec3 m_lookDir;                  // +0x290 camera forward (-z of the camera)
  float m_aspect = 1;               // +0x29C
  float m_planes[5][4] = {};        // +0x2A0 frustum planes (x, y, z, d; inside is <= 0): two sides,
                                    //        the other two sides, near
  float m_near = 0;                 // +0x2F0
  float m_nearClamped = 0;          // +0x2F4 min(near, 3.2)
  float m_far = 0;                  // +0x2F8
  unsigned char m_planeMin[5][3] = {};  // +0x2FC per plane: byte offsets into an EBound3 of the corner
  unsigned char m_planeMax[5][3] = {};  // +0x30B nearest to / farthest from the plane
  int m_frustumType = 1;            // +0x31C 2/3: degenerate frusta that drop a pair of planes
  EVec3 m_corners[4];               // +0x320 near-plane corners in camera space (clip rectangle)
};
