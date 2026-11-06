// @class EQuat after EMat4 EVec3 EBitArray
// @class ERotKeyframe
// @class EVec3Keyframe
// @class ScalarKeyframe
// @class ERotDecomp
// @class EVec3Decomp
// @class ScalarDecomp
// @class EVisInfoDecomp
// Quaternions (base/e_quat.cpp) and the animation stream decompressors (engine/e_rotdecomp.cpp,
// base/e_vec3decomp.cpp, engine/scalardecomp.cpp, engine/e_visinfodecomp.cpp).
//
// A compressed stream lives in an animation's EBitArray. It starts with a header (key count: 20 bits,
// frame-delta bits: 5, tension bits: 5, value bits: 5, 0 meaning 32), then fixed-size key records:
// the frames since the previous key minus one, a signed tension, then the value (a quaternion's y, z, w
// as signed fractions plus the sign of the derived x; a vector's or scalar's components as unsigned
// fractions of a per-component scale and offset stored after the header). Between keys the value is a
// Kochanek-Bartels (tension) spline: quaternions by repeated lerps, vectors and scalars by cubic Hermite.
// A decompressor keeps the segment around the last frame asked for and steps it forwards or backwards.
#pragma once

struct EQuat {
  float x = 0, y = 0, z = 0, w = 1;
  static void Print();  // 802CCB00
  void ToMat4(EMat4& out) const;  // 802CCB04
  void FromMat4(EMat4 &);  // 802CCBC8
  void ToAxisAngle(EVec3& axis, float& angle) const;  // 802CCD84
  float ExtractAxisRotation(EVec3& axis) const;  // 802CCE3C
  EQuat& Set(float xAngle, float yAngle, float zAngle);  // 802CCE84
  void RemoveAxisFlip(EQuat &);  // 802CD054
  EQuat& Slerp(float t, EQuat& a, EQuat b);  // 802CD0B8
  RET_UNKNOWN SlerpNoInvert(float, EQuat &, EQuat &);  // 802CD3E0
  RET_UNKNOWN Scale(float, EQuat &);  // 802CD6BC
  EQuat& Set(EVec3& axis, float angle);  // 802CD7B4

  void Id() { x = y = z = 0.0f, w = 1.0f; }  // (inline in the original)
};

struct ERotKeyframe {     // 0x18
  EQuat q;                // +0x00
  int frame = 0;          // +0x10
  float tension = 0;      // +0x14
};
struct EVec3Keyframe {    // 0x14
  EVec3 v;                // +0x00
  int frame = 0;          // +0x0C
  float tension = 0;      // +0x10
};
struct ScalarKeyframe {   // 0x0C
  float v = 0;            // +0x00
  int frame = 0;          // +0x04
  float tension = 0;      // +0x08
};

// 0x70 bytes on the GameCube (allocated from a pool shared with EVec3Decomp).
struct ERotDecomp {
  void Init(EBitArray* bits, int bit);  // 8035D2E8
  int GetBitCount();  // 8035D5CC
  EQuat GetFrame(float frame);  // 8035D5E0
  void NextSegment(float frame);  // 8035DCD0
  void LastSegment(float frame);  // 8035DE0C
  void Reset();  // 8035DF44
  void ResetEnd();  // 8035DFB0
  void ReadAllQs();  // 8035E038
  void GetKeyframe(int key, ERotKeyframe& out);  // 8035E0B4
  void GetQuatVal(int bit, EQuat& out);  // 8035E1D8
  void GetQ(int type, int key, EQuat& out);  // 8035E2DC

  EBitArray* m_bits = nullptr;  // +0x00
  int m_keyBits = 0;            // +0x04 first key record
  int m_startFrame = 0;         // +0x08 the segment's first frame
  int m_endFrame = 0;           // +0x0C and last
  int m_frameCount = 0;         // +0x10
  int m_segment = 0;            // +0x14 key index the segment starts at
  int m_keyCount = 0;           // +0x18
  short m_recordBits = 0;       // +0x1C
  signed char m_frameBits = 0;  // +0x1E
  signed char m_valueBits = 0;  // +0x1F
  signed char m_tensionBits = 0;// +0x20
  bool m_linear = false;        // +0x21 the segment is long: interpolate linearly
  float m_valueScale = 0;       // +0x24
  float m_tensionScale = 0;     // +0x28
  float m_lastFrame = 0;        // +0x2C
  EQuat m_q0;                   // +0x30 the value at the segment start
  EQuat m_a;                    // +0x40 the outgoing control quaternion there
  EQuat m_b;                    // +0x50 the incoming one at the end
  EQuat m_q1;                   // +0x60 the value at the segment end
};

// 0x74 bytes.
struct EVec3Decomp {
  void Init(EBitArray* bits, int bit);  // 802D5CE4
  int GetBitCount();  // 802D6010
  EVec3 GetFrame(float frame);  // 802D6024
  void NextSegment(float frame);  // 802D649C
  void LastSegment(float frame);  // 802D65D0
  void Reset();  // 802D6700
  void ResetEnd();  // 802D676C
  void ReadAllVs();  // 802D67F4
  void GetKeyframe(int key, EVec3Keyframe& out);  // 802D6870
  void GetVecVal(int bit, EVec3& out);  // 802D6994
  void GetV(int type, int key, EVec3& out);  // 802D6A38

  EBitArray* m_bits = nullptr;  // +0x00
  int m_keyBits = 0;            // +0x04
  int m_segment = 0;            // +0x08
  int m_startFrame = 0;         // +0x0C
  int m_endFrame = 0;           // +0x10
  int m_frameCount = 0;         // +0x14
  int m_keyCount = 0;           // +0x18
  short m_recordBits = 0;       // +0x1C
  signed char m_frameBits = 0;  // +0x1E
  signed char m_valueBits = 0;  // +0x1F
  signed char m_tensionBits = 0;// +0x20
  bool m_linear = false;        // +0x21
  float m_tensionScale = 0;     // +0x24
  float m_lastFrame = 0;        // +0x28
  EVec3 m_p0;                   // +0x2C
  EVec3 m_m0;                   // +0x38 outgoing tangent at the start
  EVec3 m_m1;                   // +0x44 incoming tangent at the end
  EVec3 m_p1;                   // +0x50
  EVec3 m_scale;                // +0x5C value = bits * scale + offset
  EVec3 m_offset;               // +0x68
};

// 0x44 bytes.
struct ScalarDecomp {
  void Init(EBitArray* bits, int bit);  // 803622E8
  int GetBitCount();  // 80362490
  float GetFrame(float frame);  // 803624A4
  void NextSegment(float frame);  // 803627A4
  void LastSegment(float frame);  // 803628C4
  void Reset();  // 803629E0
  void ResetEnd();  // 80362A4C
  void ReadAllVs();  // 80362AD4
  void GetKeyframe(int key, ScalarKeyframe& out);  // 80362B50
  void GetVal(int bit, float& out);  // 80362C74
  void GetV(int type, int key, float& out);  // 80362CE0

  EBitArray* m_bits = nullptr;  // +0x00
  int m_keyBits = 0;            // +0x04
  int m_segment = 0;            // +0x08
  int m_startFrame = 0;         // +0x0C
  int m_endFrame = 0;           // +0x10
  int m_frameCount = 0;         // +0x14
  int m_keyCount = 0;           // +0x18
  short m_recordBits = 0;       // +0x1C
  signed char m_frameBits = 0;  // +0x1E
  signed char m_valueBits = 0;  // +0x1F
  signed char m_tensionBits = 0;// +0x20
  bool m_linear = false;        // +0x21
  float m_tensionScale = 0;     // +0x24
  float m_lastFrame = 0;        // +0x28
  float m_p0 = 0;               // +0x2C
  float m_m0 = 0;               // +0x30
  float m_m1 = 0;               // +0x34
  float m_p1 = 0;               // +0x38
  float m_scale = 0;            // +0x3C
  float m_offset = 0;           // +0x40
};

// One visibility bit per frame (0xC bytes).
struct EVisInfoDecomp {
  void Init(EBitArray* bits, int bit);  // 803620E8
  bool GetFrame(float frame);  // 803620F4
  RET_UNKNOWN GetBitCount();  // 8036212C

  EBitArray* m_bits = nullptr;  // +0x00
  int m_bit = 0;                // +0x04
  int m_bitCount = 0;           // +0x08
};
