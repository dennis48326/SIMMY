// Port of base/e_bitarray.cpp (reading), base/e_quat.cpp (partial) and the animation decompressors:
// engine/e_rotdecomp.cpp, base/e_vec3decomp.cpp, engine/scalardecomp.cpp, engine/e_visinfodecomp.cpp.
// See eanimdecomp.hpp for the stream format.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/detmath.hpp"

#include <cfloat>
#include <cmath>
#include <cstring>

// ---- EBitArray ---------------------------------------------------------------------------------

// 802DF544
EBitArray::EBitArray() { SetGrowBy(0x1000); }
// 802E06F8
EBitArray::~EBitArray() = default;
// 802DF8DC
void EBitArray::SetGrowBy(int growBy) { m_growBy = growBy; }
// 802E076C
int EBitArray::GetSize() const { return m_size; }

// 802DF780
bool EBitArray::Get(int bit) const {
  if (bit >= m_size) return false;
  return (m_words[static_cast<unsigned int>(bit) >> 5] >> (bit & 31)) & 1;
}

// 802E0148
// `count` bits from `bit` on, the first bit lowest; a field may straddle two words.
unsigned int EBitArray::Get(int bit, int count) const {
  if (count == 0) return 0;
  const unsigned int lo = static_cast<unsigned int>(bit) >> 5, shift = bit & 31;
  const unsigned int hi = static_cast<unsigned int>(bit + count - 1) >> 5;
  if (lo == hi) {
    const unsigned int word = m_words[lo] >> shift;
    return count >= 32 ? word : word & ((1u << count) - 1);
  }
  const unsigned int high = m_words[hi] & ((1u << ((bit + count) & 31)) - 1);
  return high << (32 - shift) | m_words[lo] >> shift;
}

// 802E01F0
// 32 bits reinterpreted as a float.
float EBitArray::GetFloat(int bit) const {
  const unsigned int u = Get(bit, 32);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

// 802E0220
// A two's-complement field: the top bit is the sign.
int EBitArray::GetSigned(int bit, int count) const {
  if (count == 0) return 0;
  --count;
  const unsigned int v = Get(bit, count);
  return Get(bit + count) ? static_cast<int>(v | (~0u << count)) : static_cast<int>(v);
}

// 802E050C
float EBitArray::SignedBitsToFloatScaler(int bits) {
  return 1.0f / static_cast<float>(static_cast<int>((1u << (bits - 1)) - 1));
}
// 802E0558
float EBitArray::UnsignedBitsToFloatScaler(int bits) {
  return 1.0f / static_cast<float>(static_cast<int>((bits >= 32 ? 0u : 1u << bits) - 1));
}

// ---- EQuat -------------------------------------------------------------------------------------

namespace {
EQuat operator-(const EQuat& a, const EQuat& b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
EQuat operator+(const EQuat& a, const EQuat& b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
EQuat operator*(float s, const EQuat& a) { return {s * a.x, s * a.y, s * a.z, s * a.w}; }
// Scales to unit length; a zero quaternion becomes the identity.
EQuat Normalized(EQuat q) {
  const float len = std::sqrt(q.w * q.w + q.z * q.z + q.x * q.x + q.y * q.y);
  if (len == 0.0f) {
    q.Id();
    return q;
  }
  const float inv = 1.0f / len;
  return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}
}  // namespace

// 802CCB04
void EQuat::ToMat4(EMat4& out) const {
  const float x2 = x + x, y2 = y + y, z2 = z + z;
  const float xx = x2 * x, xw = x2 * w;
  auto& m = out.m;
  m[3][2] = 0.0f, m[0][3] = 0.0f, m[1][3] = 0.0f, m[2][3] = 0.0f;
  m[3][0] = 0.0f, m[3][1] = 0.0f, m[3][3] = 1.0f;
  m[1][0] = y2 * x - z2 * w;
  m[2][0] = z2 * x + y2 * w;
  m[2][1] = z2 * y - xw;
  m[0][0] = 1.0f - (y2 * y + z2 * z);
  m[1][1] = 1.0f - (xx + z2 * z);
  m[2][2] = 1.0f - (xx + y2 * y);
  m[0][1] = y2 * x + z2 * w;
  m[0][2] = z2 * x - y2 * w;
  m[1][2] = z2 * y + xw;
}

// 802CCD84
void EQuat::ToAxisAngle(EVec3& axis, float& angle) const {
  const float s2 = 1.0f - w * w;
  if (s2 > 0.0f) {
    angle = simmy::dm::acos(w) * 2.0f;
    const float inv = 1.0f / std::sqrt(s2);
    axis = EVec3{x * inv, y * inv, z * inv};
  } else {
    angle = 0.0f;
    axis = EVec3{1.0f, 0.0f, 0.0f};
  }
}

// 802CCE3C
// The rotation about `axis`: the angle times the cosine between the rotation's axis and `axis`.
float EQuat::ExtractAxisRotation(EVec3& axis) const {
  EVec3 a;
  float angle;
  ToAxisAngle(a, angle);
  return (a.x * axis.x + a.y * axis.y + a.z * axis.z) * angle;
}

// 802CD7B4
EQuat& EQuat::Set(EVec3& axis, float angle) {
  const float s = simmy::dm::sin(angle * 0.5f), c = simmy::dm::cos(angle * 0.5f);
  x = axis.x * s, y = axis.y * s, w = c, z = axis.z * s;
  return *this;
}

// 802CD6BC
// GC also runs SlerpNoInvert into the result first; the Slerp after it overwrites it.
EQuat& EQuat::Scale(float t, EQuat& q) {
  if (t == 0.0f) {
    x = 0.0f, z = 0.0f, y = 0.0f, w = 1.0f;
  } else if (t == 1.0f) {
    *this = q;
  } else {
    EQuat id;
    Slerp(t, id, q);
  }
  return *this;
}

// 802CCE84
// Euler angles: the rotations about x, then y, then z, composed.
EQuat& EQuat::Set(float xAngle, float yAngle, float zAngle) {
  EVec3 ax{1.0f, 0.0f, 0.0f}, ay{0.0f, 1.0f, 0.0f}, az{0.0f, 0.0f, 1.0f};
  EQuat qx, qy, qz;
  qx.Set(ax, xAngle);
  qy.Set(ay, yAngle);
  qz.Set(az, zAngle);
  // qy * qx
  const float a = qy.y * qx.w + ((qy.z * qx.x + qy.w * qx.y) - qy.x * qx.z);
  const float b = qy.z * qx.w + qy.w * qx.z + -qy.y * qx.x + qy.x * qx.y;
  const float c = qy.x * qx.w + qy.y * qx.z + (qy.w * qx.x - qy.z * qx.y);
  const float d = qy.w * qx.w + ((-qy.x * qx.x - qy.y * qx.y) - qy.z * qx.z);
  // qz * (qy * qx)
  x = qz.x * d + qz.y * b + (qz.w * c - qz.z * a);
  y = qz.y * d + ((qz.z * c + qz.w * a) - qz.x * b);
  z = qz.z * d + qz.w * b + -qz.y * c + qz.x * a;
  w = qz.w * d + ((-qz.x * c - qz.y * a) - qz.z * b);
  return *this;
}

// 802CD0B8
// Spherical interpolation from `a` (t = 0) to `b` (t = 1) along the shorter arc; nearly parallel
// quaternions are lerped and renormalised.
EQuat& EQuat::Slerp(float t, EQuat& a, EQuat b) {
  float cosom = b.w * a.w + b.z * a.z + a.x * b.x + a.y * b.y;
  if (cosom < 0.0f) {
    b = EQuat{-b.x, -b.y, -b.z, -b.w};
    cosom = -cosom;
  }
  if (t == 0.0f) {
    *this = a;
  } else if (t == 1.0f) {
    *this = b;
  } else if (cosom >= 0.999f) {
    *this = Normalized(a + t * (b - a));
  } else {
    const float om = simmy::dm::acos(cosom);
    const float inv = 1.0f / simmy::dm::sin(om);
    const float sa = simmy::dm::sin((1.0f - t) * om) * inv, sb = simmy::dm::sin(t * om) * inv;
    *this = EQuat{sa * a.x + sb * b.x, sa * a.y + sb * b.y, sa * a.z + sb * b.z, sa * a.w + sb * b.w};
  }
  return *this;
}

// ---- ERotDecomp --------------------------------------------------------------------------------

// 8035D2E8
// Header: key count, frame-delta bits, tension bits, value bits (0 = 32); then the key records (frame
// delta, tension, x, y, z and the sign of w). The frame count is the sum of the deltas.
void ERotDecomp::Init(EBitArray* bits, int bit) {
  m_linear = false;
  m_bits = bits;
  m_keyCount = static_cast<int>(bits->Get(bit, 0x14));
  m_frameBits = static_cast<signed char>(bits->Get(bit + 0x14, 5));
  m_tensionBits = static_cast<signed char>(bits->Get(bit + 0x19, 5));
  m_tensionScale = m_tensionBits ? EBitArray::SignedBitsToFloatScaler(m_tensionBits) : 0.0f;
  m_valueBits = static_cast<signed char>(bits->Get(bit + 0x1e, 5));
  if (!m_valueBits) m_valueBits = 0x20;
  m_valueScale = EBitArray::SignedBitsToFloatScaler(m_valueBits);
  m_keyBits = bit + 0x23;
  m_recordBits = static_cast<short>(m_frameBits + m_tensionBits + m_valueBits * 3 + 1);
  m_frameCount = 0;
  for (int i = 0; i < m_keyCount; ++i)
    m_frameCount += static_cast<int>(m_bits->Get(m_keyBits + i * m_recordBits, m_frameBits)) + 1;
  Reset();
}

// 8035D5CC
int ERotDecomp::GetBitCount() { return m_keyCount * m_recordBits + 0x23; }

// 8035D5E0
// Moves the segment to the one holding `frame` (from whichever end or the current segment is nearest)
// and interpolates: by repeated lerps between the four control quaternions, or linearly in long
// segments; the result is normalised.
EQuat ERotDecomp::GetFrame(float frame) {
  float start = static_cast<float>(m_startFrame);
  float end = static_cast<float>(m_endFrame);
  if (frame <= end) {
    if (start > frame) {
      if (start - frame <= frame) {
        do {
          LastSegment(frame);
          start = static_cast<float>(m_startFrame);
        } while (frame < start);
        end = static_cast<float>(m_endFrame);
      } else {
        Reset();
        end = static_cast<float>(m_endFrame);
        while (end < frame) {
          NextSegment(frame);
          end = static_cast<float>(m_endFrame);
        }
        start = static_cast<float>(m_startFrame);
      }
    }
  } else if (static_cast<float>(m_frameCount - 1) - frame < frame - end) {
    ResetEnd();
    start = static_cast<float>(m_startFrame);
    while (frame < start) {
      LastSegment(frame);
      start = static_cast<float>(m_startFrame);
    }
    end = static_cast<float>(m_endFrame);
  } else {
    do {
      NextSegment(frame);
      end = static_cast<float>(m_endFrame);
    } while (end < frame);
    start = static_cast<float>(m_startFrame);
  }
  m_lastFrame = frame;
  const float t = (frame - start) / (end - start);
  EQuat q;
  if (!m_linear) {
    const EQuat ab = m_a + t * (m_b - m_a);
    const EQuat l = m_q0 + t * (ab - m_q0);
    const EQuat r = ab + t * (m_q1 - ab);
    q = l + t * (r - l);
  } else {
    q = m_q0 + t * (m_q1 - m_q0);
  }
  return Normalized(q);
}

// 8035DCD0
// The next segment: long ones (a third of their frames or more since the last frame asked for) are
// marked linear and skip the control quaternions.
void ERotDecomp::NextSegment(float frame) {
  const int seg = m_segment;
  m_startFrame = m_endFrame;
  m_segment = seg + 1;
  m_endFrame = m_endFrame + 1 + static_cast<int>(m_bits->Get(m_keyBits + (seg + 2) * m_recordBits, m_frameBits));
  const int next = m_segment + 1;
  m_linear = static_cast<float>(m_endFrame - m_startFrame) * 0.33333334f <= frame - m_lastFrame;
  if (!m_linear) {
    GetQ(1, m_segment, m_a);
    GetQ(0, next, m_b);
  }
  m_q0 = m_q1;
  GetQ(2, next, m_q1);
}

// 8035DE0C
void ERotDecomp::LastSegment(float frame) {
  const int seg = m_segment;
  m_segment = seg - 1;
  m_endFrame = m_startFrame;
  m_startFrame = (m_startFrame - 1) - static_cast<int>(m_bits->Get(m_keyBits + seg * m_recordBits, m_frameBits));
  m_linear = static_cast<float>(m_endFrame - m_startFrame) * 0.33333334f <= m_lastFrame - frame;
  if (!m_linear) {
    GetQ(1, m_segment, m_a);
    GetQ(0, m_segment + 1, m_b);
  }
  m_q1 = m_q0;
  GetQ(2, m_segment, m_q0);
}

// 8035DF44
void ERotDecomp::Reset() {
  m_startFrame = 0;
  m_lastFrame = FLT_MAX;
  m_segment = 0;
  m_endFrame = static_cast<int>(m_bits->Get(m_keyBits + m_recordBits, m_frameBits)) + 1;
  ReadAllQs();
}

// 8035DFB0
void ERotDecomp::ResetEnd() {
  m_segment = m_keyCount - 2;
  m_endFrame = m_frameCount - 1;
  m_lastFrame = -FLT_MAX;
  m_startFrame = (m_endFrame - 1) - static_cast<int>(m_bits->Get(m_keyBits + (m_keyCount - 1) * m_recordBits, m_frameBits));
  ReadAllQs();
}

// 8035E038
void ERotDecomp::ReadAllQs() {
  GetQ(2, m_segment, m_q0);
  GetQ(1, m_segment, m_a);
  GetQ(0, m_segment + 1, m_b);
  GetQ(2, m_segment + 1, m_q1);
}

// 8035E0B4
// A key's frame (walking the deltas from the current segment), tension and value.
void ERotDecomp::GetKeyframe(int key, ERotKeyframe& out) {
  int seg = m_segment, frame = m_startFrame;
  int bit = m_keyBits + seg * m_recordBits;
  for (; key < seg; --seg) {
    frame -= static_cast<int>(m_bits->Get(bit, m_frameBits)) + 1;
    bit -= m_recordBits;
  }
  for (int n = key - seg; n > 0; --n) {
    bit += m_recordBits;
    frame += static_cast<int>(m_bits->Get(bit, m_frameBits)) + 1;
  }
  out.frame = frame;
  bit += m_frameBits;
  out.tension = m_tensionScale * static_cast<float>(m_bits->GetSigned(bit, m_tensionBits));
  GetQuatVal(bit + m_tensionBits, out.q);
}

// 8035E1D8
// y, z, w as signed fractions; x = sqrt(1 - y2 - z2 - w2), negated when the following bit is set.
void ERotDecomp::GetQuatVal(int bit, EQuat& out) {
  // Components 1..3 (y, z, w) are stored and component 0 (x) derived.
  float v[4];
  for (int i = 1; i <= 3; ++i) {
    v[i] = m_valueScale * static_cast<float>(m_bits->GetSigned(bit, m_valueBits));
    bit += m_valueBits;
  }
  const float r = 1.0f - (v[3] * v[3] + v[1] * v[1] + v[2] * v[2]);
  if (r > 0.0f) {
    v[0] = std::sqrt(r);
    if (m_bits->Get(bit)) v[0] = -v[0];
  } else {
    v[0] = 0.0f;
  }
  out = EQuat{v[0], v[1], v[2], v[3]};
}

// 8035E2DC
// type 2: the key's value. Types 0 (incoming) and 1 (outgoing): the Kochanek-Bartels control
// quaternion, from the neighbouring keys, the key's tension and the frame spacing (one-sided at the
// ends).
void ERotDecomp::GetQ(int type, int key, EQuat& out) {
  if (type == 2) {
    GetQuatVal(m_keyBits + key * m_recordBits + m_frameBits + m_tensionBits, out);
    return;
  }
  ERotKeyframe cur, prev, next;
  GetKeyframe(key, cur);
  EQuat d;
  float s;
  if (key == 0) {
    GetKeyframe(1, next);
    d = next.q - cur.q;
    s = cur.tension + 1.0f;
  } else if (key != m_keyCount - 1) {
    GetKeyframe(key - 1, prev);
    GetKeyframe(key + 1, next);
    float sign;
    int span;
    if (type == 0) {
      sign = 1.0f;
      span = cur.frame - prev.frame;
    } else {
      sign = -1.0f;
      span = next.frame - cur.frame;
    }
    const float ratio = static_cast<float>(span) / static_cast<float>((cur.frame - prev.frame) + (next.frame - cur.frame));
    const EQuat a = Normalized(cur.q + (-(cur.tension + 1.0f) * 0.5f) * (prev.q - cur.q));
    const EQuat b = Normalized(cur.q + ((1.0f - cur.tension) * 0.5f) * (next.q - cur.q));
    const EQuat mid = Normalized(a + 0.5f * (b - a));
    out = Normalized(cur.q + (sign * ratio * -2.0f) * (mid - cur.q));
    return;
  } else {
    GetKeyframe(key - 1, prev);
    d = prev.q - cur.q;
    s = 1.0f - cur.tension;
  }
  out = Normalized(cur.q + (s * 0.5f) * d);
}

// ---- EVec3Decomp -------------------------------------------------------------------------------

namespace {
EVec3 operator-(const EVec3& a, const EVec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
EVec3 operator+(const EVec3& a, const EVec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
EVec3 operator*(float s, const EVec3& a) { return {s * a.x, s * a.y, s * a.z}; }
}  // namespace

// 802D5CE4
// Header as for rotations, then per component a 32-bit scale (times the value step) and offset.
void EVec3Decomp::Init(EBitArray* bits, int bit) {
  m_linear = false;
  m_bits = bits;
  m_keyCount = static_cast<int>(bits->Get(bit, 0x14));
  m_frameBits = static_cast<signed char>(bits->Get(bit + 0x14, 5));
  m_tensionBits = static_cast<signed char>(bits->Get(bit + 0x19, 5));
  m_tensionScale = m_tensionBits ? EBitArray::SignedBitsToFloatScaler(m_tensionBits) : 0.0f;
  m_valueBits = static_cast<signed char>(bits->Get(bit + 0x1e, 5));
  if (!m_valueBits) m_valueBits = 0x20;
  bit += 0x23;
  const float step = EBitArray::UnsignedBitsToFloatScaler(m_valueBits);
  for (int i = 0; i < 3; ++i) {
    m_scale[i] = step * bits->GetFloat(bit);
    m_offset[i] = bits->GetFloat(bit + 0x20);
    bit += 0x40;
  }
  m_keyBits = bit;
  m_recordBits = static_cast<short>(m_frameBits + m_tensionBits + m_valueBits * 3);
  m_frameCount = 0;
  for (int i = 0; i < m_keyCount; ++i)
    m_frameCount += static_cast<int>(m_bits->Get(m_keyBits + i * m_recordBits, m_frameBits)) + 1;
  Reset();
}

// 802D6010
int EVec3Decomp::GetBitCount() { return m_keyCount * m_recordBits + 0xe3; }

// 802D6024
// As ERotDecomp::GetFrame; cubic Hermite between the end points and tangents (linear in long segments).
EVec3 EVec3Decomp::GetFrame(float frame) {
  float start = static_cast<float>(m_startFrame);
  float end = static_cast<float>(m_endFrame);
  if (frame <= end) {
    if (start > frame) {
      if (start - frame <= frame) {
        do {
          LastSegment(frame);
          start = static_cast<float>(m_startFrame);
        } while (frame < start);
        end = static_cast<float>(m_endFrame);
      } else {
        Reset();
        end = static_cast<float>(m_endFrame);
        while (end < frame) {
          NextSegment(frame);
          end = static_cast<float>(m_endFrame);
        }
        start = static_cast<float>(m_startFrame);
      }
    }
  } else if (static_cast<float>(m_frameCount - 1) - frame < frame - end) {
    ResetEnd();
    start = static_cast<float>(m_startFrame);
    while (frame < start) {
      LastSegment(frame);
      start = static_cast<float>(m_startFrame);
    }
    end = static_cast<float>(m_endFrame);
  } else {
    do {
      NextSegment(frame);
      end = static_cast<float>(m_endFrame);
    } while (end < frame);
    start = static_cast<float>(m_startFrame);
  }
  m_lastFrame = frame;
  const float t = (frame - start) / (end - start);
  if (!m_linear) {
    const float t2 = t * t, t3 = t2 * t;
    const float h00 = ((t3 + t3) - t2 * 3.0f) + 1.0f, h10 = (t3 - (t2 + t2)) + t;
    const float h11 = t3 - t2, h01 = t3 * -2.0f + t2 * 3.0f;
    return EVec3{m_p0.x * h00 + m_m0.x * h10 + m_p1.x * h01 + m_m1.x * h11,
                 m_p0.y * h00 + m_m0.y * h10 + m_p1.y * h01 + m_m1.y * h11,
                 m_p0.z * h00 + m_m0.z * h10 + m_p1.z * h01 + m_m1.z * h11};
  }
  return m_p0 + t * (m_p1 - m_p0);
}

// 802D649C
void EVec3Decomp::NextSegment(float frame) {
  const int seg = m_segment;
  m_startFrame = m_endFrame;
  m_segment = seg + 1;
  m_endFrame = m_endFrame + 1 + static_cast<int>(m_bits->Get(m_keyBits + (seg + 2) * m_recordBits, m_frameBits));
  const int next = m_segment + 1;
  m_linear = static_cast<float>(m_endFrame - m_startFrame) * 0.33333334f <= frame - m_lastFrame;
  if (!m_linear) {
    GetV(1, m_segment, m_m0);
    GetV(0, next, m_m1);
  }
  m_p0 = m_p1;
  GetV(2, next, m_p1);
}

// 802D65D0
void EVec3Decomp::LastSegment(float frame) {
  const int seg = m_segment;
  m_segment = seg - 1;
  m_endFrame = m_startFrame;
  m_startFrame = (m_startFrame - 1) - static_cast<int>(m_bits->Get(m_keyBits + seg * m_recordBits, m_frameBits));
  m_linear = static_cast<float>(m_endFrame - m_startFrame) * 0.33333334f <= m_lastFrame - frame;
  if (!m_linear) {
    GetV(1, m_segment, m_m0);
    GetV(0, m_segment + 1, m_m1);
  }
  m_p1 = m_p0;
  GetV(2, m_segment, m_p0);
}

// 802D6700
void EVec3Decomp::Reset() {
  m_startFrame = 0;
  m_lastFrame = FLT_MAX;
  m_segment = 0;
  m_endFrame = static_cast<int>(m_bits->Get(m_keyBits + m_recordBits, m_frameBits)) + 1;
  ReadAllVs();
}

// 802D676C
void EVec3Decomp::ResetEnd() {
  m_segment = m_keyCount - 2;
  m_endFrame = m_frameCount - 1;
  m_lastFrame = -FLT_MAX;
  m_startFrame = (m_endFrame - 1) - static_cast<int>(m_bits->Get(m_keyBits + (m_keyCount - 1) * m_recordBits, m_frameBits));
  ReadAllVs();
}

// 802D67F4
void EVec3Decomp::ReadAllVs() {
  GetV(2, m_segment, m_p0);
  GetV(1, m_segment, m_m0);
  GetV(0, m_segment + 1, m_m1);
  GetV(2, m_segment + 1, m_p1);
}

// 802D6870
void EVec3Decomp::GetKeyframe(int key, EVec3Keyframe& out) {
  int seg = m_segment, frame = m_startFrame;
  int bit = m_keyBits + seg * m_recordBits;
  for (; key < seg; --seg) {
    frame -= static_cast<int>(m_bits->Get(bit, m_frameBits)) + 1;
    bit -= m_recordBits;
  }
  for (int n = key - seg; n > 0; --n) {
    bit += m_recordBits;
    frame += static_cast<int>(m_bits->Get(bit, m_frameBits)) + 1;
  }
  out.frame = frame;
  bit += m_frameBits;
  out.tension = m_tensionScale * static_cast<float>(m_bits->GetSigned(bit, m_tensionBits));
  GetVecVal(bit + m_tensionBits, out.v);
}

// 802D6994
void EVec3Decomp::GetVecVal(int bit, EVec3& out) {
  for (int i = 0; i < 3; ++i) {
    out[i] = static_cast<float>(m_bits->Get(bit, m_valueBits)) * m_scale[i] + m_offset[i];
    bit += m_valueBits;
  }
}

// 802D6A38
// type 2: the key's value; 0/1: its incoming/outgoing tangent (at the ends, from the neighbour's
// tangent, or the difference when there are only two keys).
void EVec3Decomp::GetV(int type, int key, EVec3& out) {
  if (type == 2) {
    GetVecVal(m_keyBits + key * m_recordBits + m_frameBits + m_tensionBits, out);
    return;
  }
  EVec3Keyframe cur, prev, next;
  GetKeyframe(key, cur);
  if (key == 0) {
    GetKeyframe(1, next);
    if (m_keyCount != 2) {
      EVec3 t;
      GetV(0, 1, t);
      out = 1.5f * (next.v - cur.v) - 0.5f * t;
    } else {
      out = next.v - cur.v;
    }
  } else if (key == m_keyCount - 1) {
    GetKeyframe(key - 1, prev);
    if (m_keyCount != 2) {
      EVec3 t;
      GetV(1, key - 1, t);
      out = 1.5f * (cur.v - prev.v) - 0.5f * t;
    } else {
      out = cur.v - prev.v;
    }
  } else {
    GetKeyframe(key - 1, prev);
    GetKeyframe(key + 1, next);
    const int span = type == 0 ? cur.frame - prev.frame : next.frame - cur.frame;
    const float ratio = static_cast<float>(span) / static_cast<float>((cur.frame - prev.frame) + (next.frame - cur.frame));
    out = ratio * ((cur.tension + 1.0f) * (cur.v - prev.v) + (1.0f - cur.tension) * (next.v - cur.v));
  }
}

// ---- ScalarDecomp ------------------------------------------------------------------------------

// 803622E8
void ScalarDecomp::Init(EBitArray* bits, int bit) {
  m_linear = false;
  m_bits = bits;
  m_keyCount = static_cast<int>(bits->Get(bit, 0x14));
  m_frameBits = static_cast<signed char>(bits->Get(bit + 0x14, 5));
  m_tensionBits = static_cast<signed char>(bits->Get(bit + 0x19, 5));
  m_tensionScale = m_tensionBits ? EBitArray::SignedBitsToFloatScaler(m_tensionBits) : 0.0f;
  m_valueBits = static_cast<signed char>(bits->Get(bit + 0x1e, 5));
  if (!m_valueBits) m_valueBits = 0x20;
  m_scale = EBitArray::UnsignedBitsToFloatScaler(m_valueBits) * bits->GetFloat(bit + 0x23);
  m_offset = bits->GetFloat(bit + 0x43);
  m_keyBits = bit + 99;
  m_recordBits = static_cast<short>(m_valueBits + m_frameBits + m_tensionBits);
  m_frameCount = 0;
  for (int i = 0; i < m_keyCount; ++i)
    m_frameCount += static_cast<int>(m_bits->Get(m_keyBits + i * m_recordBits, m_frameBits)) + 1;
  Reset();
}

// 80362490
int ScalarDecomp::GetBitCount() { return m_keyCount * m_recordBits + 99; }

// 803624A4
// As EVec3Decomp::GetFrame. (The original's linear case weights the segment's start by t and its
// end by 1 - t; kept as it was.)
float ScalarDecomp::GetFrame(float frame) {
  float start = static_cast<float>(m_startFrame);
  float end = static_cast<float>(m_endFrame);
  if (frame <= end) {
    if (start > frame) {
      if (start - frame <= frame) {
        do {
          LastSegment(frame);
          start = static_cast<float>(m_startFrame);
        } while (frame < start);
        end = static_cast<float>(m_endFrame);
      } else {
        Reset();
        end = static_cast<float>(m_endFrame);
        while (end < frame) {
          NextSegment(frame);
          end = static_cast<float>(m_endFrame);
        }
        start = static_cast<float>(m_startFrame);
      }
    }
  } else if (static_cast<float>(m_frameCount - 1) - frame < frame - end) {
    ResetEnd();
    start = static_cast<float>(m_startFrame);
    while (frame < start) {
      LastSegment(frame);
      start = static_cast<float>(m_startFrame);
    }
    end = static_cast<float>(m_endFrame);
  } else {
    do {
      NextSegment(frame);
      end = static_cast<float>(m_endFrame);
    } while (end < frame);
    start = static_cast<float>(m_startFrame);
  }
  m_lastFrame = frame;
  const float t = (frame - start) / (end - start);
  if (!m_linear) {
    const float t2 = t * t, t3 = t2 * t;
    return m_m1 * (t3 - t2) + m_p1 * (t3 * -2.0f + t2 * 3.0f) + m_p0 * (((t3 + t3) - t2 * 3.0f) + 1.0f) +
           m_m0 * ((t3 - (t2 + t2)) + t);
  }
  return t * m_p0 + (1.0f - t) * m_p1;
}

// 803627A4
void ScalarDecomp::NextSegment(float frame) {
  const int seg = m_segment;
  m_startFrame = m_endFrame;
  m_segment = seg + 1;
  m_endFrame = m_endFrame + 1 + static_cast<int>(m_bits->Get(m_keyBits + (seg + 2) * m_recordBits, m_frameBits));
  const int next = m_segment + 1;
  m_linear = static_cast<float>(m_endFrame - m_startFrame) * 0.33333334f <= frame - m_lastFrame;
  if (!m_linear) {
    GetV(1, m_segment, m_m0);
    GetV(0, next, m_m1);
  }
  m_p0 = m_p1;
  GetV(2, next, m_p1);
}

// 803628C4
void ScalarDecomp::LastSegment(float frame) {
  const int seg = m_segment;
  m_segment = seg - 1;
  m_endFrame = m_startFrame;
  m_startFrame = (m_startFrame - 1) - static_cast<int>(m_bits->Get(m_keyBits + seg * m_recordBits, m_frameBits));
  m_linear = static_cast<float>(m_endFrame - m_startFrame) * 0.33333334f <= m_lastFrame - frame;
  if (!m_linear) {
    GetV(1, m_segment, m_m0);
    GetV(0, m_segment + 1, m_m1);
  }
  m_p1 = m_p0;
  GetV(2, m_segment, m_p0);
}

// 803629E0
void ScalarDecomp::Reset() {
  m_startFrame = 0;
  m_lastFrame = FLT_MAX;
  m_segment = 0;
  m_endFrame = static_cast<int>(m_bits->Get(m_keyBits + m_recordBits, m_frameBits)) + 1;
  ReadAllVs();
}

// 80362A4C
void ScalarDecomp::ResetEnd() {
  m_segment = m_keyCount - 2;
  m_endFrame = m_frameCount - 1;
  m_lastFrame = -FLT_MAX;
  m_startFrame = (m_endFrame - 1) - static_cast<int>(m_bits->Get(m_keyBits + (m_keyCount - 1) * m_recordBits, m_frameBits));
  ReadAllVs();
}

// 80362AD4
void ScalarDecomp::ReadAllVs() {
  GetV(2, m_segment, m_p0);
  GetV(1, m_segment, m_m0);
  GetV(0, m_segment + 1, m_m1);
  GetV(2, m_segment + 1, m_p1);
}

// 80362B50
void ScalarDecomp::GetKeyframe(int key, ScalarKeyframe& out) {
  int seg = m_segment, frame = m_startFrame;
  int bit = m_keyBits + seg * m_recordBits;
  for (; key < seg; --seg) {
    frame -= static_cast<int>(m_bits->Get(bit, m_frameBits)) + 1;
    bit -= m_recordBits;
  }
  for (int n = key - seg; n > 0; --n) {
    bit += m_recordBits;
    frame += static_cast<int>(m_bits->Get(bit, m_frameBits)) + 1;
  }
  out.frame = frame;
  bit += m_frameBits;
  out.tension = m_tensionScale * static_cast<float>(m_bits->GetSigned(bit, m_tensionBits));
  GetVal(bit + m_tensionBits, out.v);
}

// 80362C74
void ScalarDecomp::GetVal(int bit, float& out) {
  out = static_cast<float>(m_bits->Get(bit, m_valueBits)) * m_scale + m_offset;
}

// 80362CE0
void ScalarDecomp::GetV(int type, int key, float& out) {
  if (type == 2) {
    GetVal(m_keyBits + key * m_recordBits + m_frameBits + m_tensionBits, out);
    return;
  }
  ScalarKeyframe cur, prev, next;
  GetKeyframe(key, cur);
  if (key == 0) {
    GetKeyframe(1, next);
    if (m_keyCount == 2) {
      out = next.v - cur.v;
      return;
    }
    float t;
    GetV(0, 1, t);
    out = (next.v - cur.v) * 1.5f - t * 0.5f;
  } else if (key == m_keyCount - 1) {
    GetKeyframe(key - 1, prev);
    if (m_keyCount == 2) {
      out = cur.v - prev.v;
      return;
    }
    float t;
    GetV(1, key - 1, t);
    out = (cur.v - prev.v) * 1.5f - t * 0.5f;
  } else {
    GetKeyframe(key - 1, prev);
    GetKeyframe(key + 1, next);
    const int span = type == 0 ? cur.frame - prev.frame : next.frame - cur.frame;
    out = ((cur.v - prev.v) * (cur.tension + 1.0f) + (next.v - cur.v) * (1.0f - cur.tension)) *
          (static_cast<float>(span) / static_cast<float>((cur.frame - prev.frame) + (next.frame - cur.frame)));
  }
}

// ---- EVisInfoDecomp ----------------------------------------------------------------------------

// 803620E8
void EVisInfoDecomp::Init(EBitArray* bits, int bit) {
  m_bit = bit;
  m_bits = bits;
}
// 803620F4
bool EVisInfoDecomp::GetFrame(float frame) { return m_bits->Get(m_bit + static_cast<int>(frame)); }
