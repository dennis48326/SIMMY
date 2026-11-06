// @class EMat4 after EVec3 EVec4
// 4x4 float matrix, row-major, translation in row 3 (base/e_mat4.cpp).
#pragma once

struct EMat4 {
  float m[4][4] = {};
  EMat4() = default;

  void Mult4x4(EMat4& a, EMat4& b);  // 802C9208
  void PreScale(float);  // 802C9238
  void PreScale(EVec3 &);  // 802C9270
  void PreTranslate(EVec3 &);  // 802C9308
  void operator=(float);  // 802C9454
  void GetColumn(int column, EVec3& out);  // 802C949C
  void GetColumn(int, EVec4 &);  // 802C94C4
  void SetColumn(int, EVec3 &);  // 802C94F8
  void SetColumn(int, EVec4 &);  // 802C9520
  RET_UNKNOWN Normalize();  // 802C9554
  void Transpose(EMat4 &);  // 802C95F4
  void Transpose();  // 802C9640
  void Id();  // 802C96D8
  EMat4& Translate(EVec3& t);  // 802C9730
  EMat4& Scale(EVec3& s);  // 802C9780
  float GetScaleX();  // 802C97CC
  float GetScaleY();  // 802C97FC
  void GetScaleZ();  // 802C982C
  EMat4& RotateX(float angle);  // 802C985C
  EMat4& RotateY(float angle);  // 802C98C8
  EMat4& RotateZ(float angle);  // 802C9934
  int Invert(EMat4& src);  // 802C99A0
  void InvertSimple(EMat4 &);  // 802C9C90
  EMat4& Rotate(EVec3& axis, float angle);  // 802C9D34
  EMat4& PreRotateX(float angle);  // 802C9E3C
  EMat4& PostRotateX(float angle);  // 802C9EF0
  EMat4& PreRotateY(float angle);  // 802C9FC4
  EMat4& PostRotateY(float angle);  // 802CA078
  EMat4& PreRotateZ(float angle);  // 802CA14C
  EMat4& PostRotateZ(float angle);  // 802CA200
  void PostTranslate(EVec3& t);  // 802CA2D4
  void PostScale(EVec3 &);  // 802CA30C
  void PostScale(float);  // 802CA3D4
  void Conform(EVec3 &);  // 802CA40C
  RET_UNKNOWN OrientPosNormal(EVec3 &, EVec3 &, EVec3 &);  // 802CA590
  void Clamp();  // 802CA5DC
  void Print() const;  // 802CA640
  float GetMaxScale() const;  // 802CA644
  void GetHPR(float &, float &, float &);  // 802CA69C
  void ExtractAxisRotation(EVec3 &) const;  // 802CA720
  void GetLookAtPos(EVec3 &, EVec3 &, EVec3 &);  // 802CA760
  EMat4& LookAtPos(EVec3& eye, EVec3& target, EVec3& up);  // 802CA844
  RET_UNKNOWN LookAt(EVec3 &, EVec3 &, EVec3 &);  // 802CAA40
  RET_UNKNOWN LookAtDirect(EVec3 &, EVec3 &, float);  // 802CAA8C
  RET_UNKNOWN LookTo(EVec3 &, EVec3 &, EVec3 &);  // 802CAB98
  EMat4& Projection(float fovY, float aspect, float zNear, float zFar);  // 802CAD94
  EMat4& Ortho(float left, float right, float bottom, float top, float zNear, float zFar);  // 802CAE68
  RET_UNKNOWN BlendEuler(float, EMat4 &, EMat4 &);  // 802CAF4C
  RET_UNKNOWN BlendQuat(float, EMat4 &, EMat4 &);  // 802CB2B8
  RET_UNKNOWN TexturePerspectiveProjection(EVec3 &, EVec3 &, EVec3 &, float, float, float, float);  // 802CB42C
  RET_UNKNOWN TexturePlanarProjection(EVec3 &, EVec3 &, EVec3 &, float, float, float, float);  // 802CB528

};
