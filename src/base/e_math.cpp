// Port of base/e_vec3.cpp, engine/e_vec4.cpp and the basics of base/e_mat4.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/detmath.hpp"

#include <cmath>
#include <utility>

namespace {
unsigned char to_u8(float v) {
  v *= 255.0f;
  return v < 0.0f ? 0 : v > 255.0f ? 255 : static_cast<unsigned char>(static_cast<int>(v));
}
signed char to_s8(float v) {
  v *= 127.0f;
  return v < -127.0f ? -127 : v > 127.0f ? 127 : static_cast<signed char>(static_cast<int>(v));
}
}  // namespace

// ---- EVec3 -------------------------------------------------------------------------------------
// 802D5A6C
void EVec3::Print() {}
// 802D5AA8
void EVec3::ToU8s(unsigned char* out) const {
  for (int i = 0; i < 3; ++i) out[i] = to_u8((*this)[i]);
}
// 802D5B20
void EVec3::FromU8s(unsigned char* in) {
  for (int i = 0; i < 3; ++i) (*this)[i] = in[i] * (1.0f / 255.0f);
}
// 802D5B78
void EVec3::ToS8s(signed char* out) const {
  for (int i = 0; i < 3; ++i) out[i] = to_s8((*this)[i]);
}
// 802D5BF0
void EVec3::FromS8s(signed char* in) {
  for (int i = 0; i < 3; ++i) (*this)[i] = in[i] * (1.0f / 127.0f);
}

// ---- EVec4 -------------------------------------------------------------------------------------
// 80361F34
void EVec4::Print() {}
// 80361F38
void EVec4::ToU8s(unsigned char* out) const {
  for (int i = 0; i < 4; ++i) out[i] = to_u8((*this)[i]);
}
// 80361FB0
void EVec4::FromU8s(unsigned char* in) {
  for (int i = 0; i < 4; ++i) (*this)[i] = in[i] * (1.0f / 255.0f);
}

// ---- EMat4 -------------------------------------------------------------------------------------
// 802C96D8
void EMat4::Id() {
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) m[r][c] = (r == c) ? 1.0f : 0.0f;
}
// 802C9730
EMat4& EMat4::Translate(EVec3& t) {
  Id();
  m[3][0] = t.x;
  m[3][1] = t.y;
  m[3][2] = t.z;
  return *this;
}
// 802C9780
EMat4& EMat4::Scale(EVec3& s) {
  Id();
  m[0][0] = s.x;
  m[1][1] = s.y;
  m[2][2] = s.z;
  return *this;
}
// 802CA2D4
void EMat4::PostTranslate(EVec3& t) {
  m[3][0] += t.x;
  m[3][1] += t.y;
  m[3][2] += t.z;
}

// ---- EVec2 -------------------------------------------------------------------------------------
// 802D5A34
// 802D5CB8  (a second, identical EVec2::Print from another file)
void EVec2::Print() {}

// 802C9270
void EMat4::PreScale(EVec3& s) {
  for (int c = 0; c < 3; ++c) {
    m[0][c] *= s.x;
    m[1][c] *= s.y;
    m[2][c] *= s.z;
  }
}
// 802C9308
void EMat4::PreTranslate(EVec3& t) {
  for (int c = 0; c < 3; ++c) m[3][c] += t.z * m[2][c] + t.x * m[0][c] + t.y * m[1][c];
}
// 802CA30C
void EMat4::PostScale(EVec3& s) {
  for (int r = 0; r < 4; ++r) {
    m[r][0] *= s.x;
    m[r][1] *= s.y;
    m[r][2] *= s.z;
  }
}

// 802C9208
// this = a * b (row vectors: a is applied first).
void EMat4::Mult4x4(EMat4& a, EMat4& b) {
  EMat4 r;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
  *this = r;
}
// 802C949C
void EMat4::GetColumn(int column, EVec3& out) {
  out.x = m[0][column];
  out.y = m[1][column];
  out.z = m[2][column];
}
// 802C94F8
void EMat4::SetColumn(int column, EVec3& v) {
  m[0][column] = v.x;
  m[1][column] = v.y;
  m[2][column] = v.z;
}
// 802C9554
// The three axis columns made unit length (zero ones left alone).
RET_UNKNOWN EMat4::Normalize() {
  for (int c = 0; c < 3; ++c) {
    EVec3 v;
    GetColumn(c, v);
    Vec n{v.x, v.y, v.z};
    if (n.x != 0.0f || n.y != 0.0f || n.z != 0.0f) PSVECNormalize(&n, &n);
    v = EVec3{n.x, n.y, n.z};
    SetColumn(c, v);
  }
  return {};
}
// 802CB2B8
// this = the rotation slerped from a's to b's by t, the translation lerped.
RET_UNKNOWN EMat4::BlendQuat(float t, EMat4& a, EMat4& b) {
  EQuat qa, qb, q;
  qa.FromMat4(a);
  qb.FromMat4(b);
  q.Slerp(t, qa, qb);
  q.ToMat4(*this);
  for (int i = 0; i < 3; ++i) m[3][i] = a.m[3][i] + (b.m[3][i] - a.m[3][i]) * t;
  return {};
}
// 802CCBC8
// The rotation of `mat` (its upper 3x3) as a quaternion.
void EQuat::FromMat4(EMat4& mat) {
  const float* p = &mat.m[0][0];
  float* q[3] = {&x, &y, &z};
  const float trace = p[0] + p[5] + p[10];
  if (trace > 0.0f) {
    const float s = std::sqrt(trace + 1.0f);
    const float f = 0.5f / s;
    w = s * 0.5f;
    x = (p[6] - p[9]) * f;
    y = (p[8] - p[2]) * f;
    z = (p[1] - p[4]) * f;
    return;
  }
  static const int kNext[3] = {1, 2, 0};  // s_iNext
  int i = p[0] < p[5] ? 1 : 0;
  if (p[i * 5] < p[10]) i = 2;
  const int j = kNext[i], k = kNext[j];
  const float s = std::sqrt(p[i * 5] - p[j * 5] - p[k * 5] + 1.0f);
  const float f = 0.5f / s;
  *q[i] = s * 0.5f;
  w = (p[j * 4 + k] - p[k * 4 + j]) * f;
  *q[j] = (p[i * 4 + j] + p[j * 4 + i]) * f;
  *q[k] = (p[i * 4 + k] + p[k * 4 + i]) * f;
}
// 802C97CC
float EMat4::GetScaleX() {
  EVec3 c;
  GetColumn(0, c);
  return std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
}
// 802C97FC
float EMat4::GetScaleY() {
  EVec3 c;
  GetColumn(1, c);
  return std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
}
// 802CAE68
EMat4& EMat4::Ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
  Id();
  m[3][3] = 1.0f;
  m[3][1] = -(top + bottom) / (top - bottom);
  m[1][1] = 2.0f / (top - bottom);
  m[3][0] = -(right + left) / (right - left);
  m[3][2] = -zFar / (zFar - zNear);
  m[2][2] = -1.0f / (zFar - zNear);
  m[0][0] = 2.0f / (right - left);
  return *this;
}
// 802C9160
// Row vector times matrix.
EVec4 operator*(EVec4& v, EMat4& m) {
  EVec4 r;
  for (int j = 0; j < 4; ++j) r[j] = v.x * m.m[0][j] + v.y * m.m[1][j] + v.z * m.m[2][j] + v.w * m.m[3][j];
  return r;
}

// ---- EMat4, continued (models and 3D views) ----------------------------------------------------
// 802C9238
void EMat4::PreScale(float s) {
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) m[r][c] *= s;
}
// 802C985C
EMat4& EMat4::RotateX(float angle) {
  Id();
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  m[1][1] = c;
  m[2][1] = -s;
  m[1][2] = s;
  m[2][2] = c;
  return *this;
}
// 802C98C8
EMat4& EMat4::RotateY(float angle) {
  Id();
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  m[0][0] = c;
  m[0][2] = -s;
  m[2][0] = s;
  m[2][2] = c;
  return *this;
}
// 802C9934
EMat4& EMat4::RotateZ(float angle) {
  Id();
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  m[0][0] = c;
  m[1][0] = -s;
  m[0][1] = s;
  m[1][1] = c;
  return *this;
}
// 802C9EF0
// Rotates the result about x afterwards (columns 1 and 2 of every row).
EMat4& EMat4::PostRotateX(float angle) {
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  for (int r = 0; r < 4; ++r) {
    const float y = m[r][1], z = m[r][2];
    m[r][2] = y * s + z * c;
    m[r][1] = y * c + z * -s;
  }
  return *this;
}
// 802CA078
// this = this * (rotation about y by `angle`): columns 0 and 2 of every row.
EMat4& EMat4::PostRotateY(float angle) {
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  for (int r = 0; r < 4; ++r) {
    const float x = m[r][0], z = m[r][2];
    m[r][2] = x * -s + z * c;
    m[r][0] = x * c + z * s;
  }
  return *this;
}
// 802C9E3C
// this = (rotation about x by `angle`) * this: rows 1 and 2 turned.
EMat4& EMat4::PreRotateX(float angle) {
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  for (int col = 0; col < 3; ++col) {
    const float r1 = m[1][col], r2 = m[2][col];
    m[2][col] = -s * r1 + c * r2;
    m[1][col] = c * r1 + s * r2;
  }
  return *this;
}
// 802C9FC4
// this = (rotation about y by `angle`) * this: rows 0 and 2 turned.
EMat4& EMat4::PreRotateY(float angle) {
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  for (int col = 0; col < 3; ++col) {
    const float r0 = m[0][col], r2 = m[2][col];
    m[2][col] = s * r0 + c * r2;
    m[0][col] = c * r0 + -s * r2;
  }
  return *this;
}
// 802CA14C
// this = (rotation about z by `angle`) * this: rows 0 and 1 turned.
EMat4& EMat4::PreRotateZ(float angle) {
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  for (int col = 0; col < 3; ++col) {
    const float r0 = m[0][col], r1 = m[1][col];
    m[1][col] = -s * r0 + c * r1;
    m[0][col] = c * r0 + s * r1;
  }
  return *this;
}
// 802CA200
// this = this * (rotation about z by `angle`).
EMat4& EMat4::PostRotateZ(float angle) {
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle);
  for (int r = 0; r < 4; ++r) {
    const float x = m[r][0], y = m[r][1];
    m[r][1] = x * s + y * c;
    m[r][0] = x * c + y * -s;
  }
  return *this;
}
// 802C99A0
// this = the inverse of the affine matrix `src` (3x3 part by cofactors, then the translation).
// Returns 0, leaving this unchanged, when it is singular.
int EMat4::Invert(EMat4& src) {
  const auto& a = src.m;
  const float det = a[0][0] * (a[1][1] * a[2][2] - a[2][1] * a[1][2]) - a[1][0] * (a[0][1] * a[2][2] - a[2][1] * a[0][2]) +
                    a[2][0] * (a[0][1] * a[1][2] - a[1][1] * a[0][2]);
  if (det == 0.0f) return 0;
  const float k = 1.0f / det;
  EMat4 r;
  r.m[0][0] = (a[1][1] * a[2][2] - a[2][1] * a[1][2]) * k;
  r.m[0][1] = -(a[0][1] * a[2][2] - a[2][1] * a[0][2]) * k;
  r.m[0][2] = (a[0][1] * a[1][2] - a[1][1] * a[0][2]) * k;
  r.m[1][0] = -(a[1][0] * a[2][2] - a[2][0] * a[1][2]) * k;
  r.m[1][1] = (a[0][0] * a[2][2] - a[2][0] * a[0][2]) * k;
  r.m[1][2] = -(a[0][0] * a[1][2] - a[1][0] * a[0][2]) * k;
  r.m[2][0] = (a[1][0] * a[2][1] - a[2][0] * a[1][1]) * k;
  r.m[2][1] = -(a[0][0] * a[2][1] - a[2][0] * a[0][1]) * k;
  r.m[2][2] = (a[0][0] * a[1][1] - a[1][0] * a[0][1]) * k;
  for (int c = 0; c < 3; ++c) r.m[3][c] = -(a[3][0] * r.m[0][c] + a[3][1] * r.m[1][c] + a[3][2] * r.m[2][c]);
  r.m[0][3] = r.m[1][3] = r.m[2][3] = 0.0f;
  r.m[3][3] = 1.0f;
  *this = r;
  return 1;
}
// 802CA644
// The largest absolute row sum of the 3x3 part (a bound on how much the matrix scales).
float EMat4::GetMaxScale() const {
  float best = 0.0f;
  for (int c = 0; c < 3; ++c) {
    const float sum = std::fabs(m[0][c]) + std::fabs(m[1][c]) + std::fabs(m[2][c]);
    if (best < sum) best = sum;
  }
  return best;
}
// 802C95F4
// This matrix becomes the transpose of src (src must be another matrix).
void EMat4::Transpose(EMat4& src) {
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) m[r][c] = src.m[c][r];
}

// 802CA760
// The inverse of LookAtPos: eye = the translation, target one unit along -z, up = z x x.
void EMat4::GetLookAtPos(EVec3& eye, EVec3& target, EVec3& up) {
  eye = EVec3{m[3][0], m[3][1], m[3][2]};
  target = EVec3{eye.x - m[2][0], eye.y - m[2][1], eye.z - m[2][2]};
  up = EVec3{m[2][1] * m[0][2] - m[2][2] * m[0][1], m[2][2] * m[0][0] - m[2][0] * m[0][2],
             m[2][0] * m[0][1] - m[2][1] * m[0][0]};
}
// 802CA844
// Camera-to-world: z points from the target to the eye, x = up x z, y = z x x, translation = eye.
EMat4& EMat4::LookAtPos(EVec3& eye, EVec3& target, EVec3& up) {
  Vec z{eye.x - target.x, eye.y - target.y, eye.z - target.z};
  if (z.x != 0.0f || z.y != 0.0f || z.z != 0.0f) PSVECNormalize(&z, &z);
  Vec x{up.y * z.z - up.z * z.y, up.z * z.x - up.x * z.z, up.x * z.y - up.y * z.x};
  if (x.x != 0.0f || x.y != 0.0f || x.z != 0.0f) PSVECNormalize(&x, &x);
  Vec y{z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z, z.x * x.y - z.y * x.x};
  if (y.x != 0.0f || y.y != 0.0f || y.z != 0.0f) PSVECNormalize(&y, &y);
  m[0][0] = x.x, m[0][1] = x.y, m[0][2] = x.z, m[0][3] = 0.0f;
  m[1][0] = y.x, m[1][1] = y.y, m[1][2] = y.z, m[1][3] = 0.0f;
  m[2][0] = z.x, m[2][1] = z.y, m[2][2] = z.z, m[2][3] = 0.0f;
  m[3][0] = eye.x, m[3][1] = eye.y, m[3][2] = eye.z, m[3][3] = 1.0f;
  return *this;
}
// 802CAD94
// Perspective projection (vertical field of view in degrees), the layout GX expects once transposed.
EMat4& EMat4::Projection(float fovY, float aspect, float zNear, float zFar) {
  Id();
  const float half = fovY * 0.017453292f * 0.5f;
  const float cot = simmy::dm::cos(half) / simmy::dm::sin(half);
  m[2][3] = -1.0f;
  m[3][3] = 0.0f;
  m[1][1] = cot;
  m[3][2] = (zNear * zFar) / (zNear - zFar);
  m[0][0] = cot / aspect;
  m[2][2] = zNear / (zNear - zFar);
  return *this;
}

// 802CB5B4
// In-place Gauss-Jordan inversion with full pivoting of the n x n matrix whose rows are `a[0..n-1]`.
// Returns 0 if it is singular.
int MatrixInvert(float** a, int n) {
  int pivoted[256], rowOf[256], colOf[256];
  for (int j = 0; j < n; ++j) pivoted[j] = -1;
  int irow = 0, icol = 0;
  for (int i = 0; i < n; ++i) {
    float big = 0.0f;
    for (int j = 0; j < n; ++j) {
      if (pivoted[j] == 0) continue;
      for (int k = 0; k < n; ++k) {
        if (pivoted[k] == -1) {
          const float v = std::fabs(a[j][k]);
          if (big <= v) big = v, irow = j, icol = k;
        } else if (pivoted[k] > 0) {
          return 0;
        }
      }
    }
    ++pivoted[icol];
    if (irow != icol)
      for (int l = 0; l < n; ++l) std::swap(a[irow][l], a[icol][l]);
    rowOf[i] = irow;
    colOf[i] = icol;
    const float pivot = a[icol][icol];
    if (pivot == 0.0f) return 0;
    a[icol][icol] = 1.0f;
    for (int l = 0; l < n; ++l) a[icol][l] *= 1.0f / pivot;
    for (int ll = 0; ll < n; ++ll) {
      if (ll == icol) continue;
      const float d = a[ll][icol];
      a[ll][icol] = 0.0f;
      for (int l = 0; l < n; ++l) a[ll][l] -= a[icol][l] * d;
    }
  }
  for (int l = n - 1; l >= 0; --l)
    if (rowOf[l] != colOf[l])
      for (int k = 0; k < n; ++k) std::swap(a[k][rowOf[l]], a[k][colOf[l]]);
  return 1;
}

// 802C9D34
// A rotation by `angle` (radians) about the unit `axis` (row vectors).
EMat4& EMat4::Rotate(EVec3& axis, float angle) {
  const float s = simmy::dm::sin(angle), c = simmy::dm::cos(angle), t = 1.0f - c;
  const float x = axis.x, y = axis.y, z = axis.z;
  m[0][0] = x * x * t + c;
  m[0][1] = x * y * t + z * s;
  m[0][2] = x * z * t - y * s;
  m[0][3] = 0.0f;
  m[1][0] = x * y * t - z * s;
  m[1][1] = y * y * t + c;
  m[1][2] = y * z * t + x * s;
  m[1][3] = 0.0f;
  m[2][0] = x * z * t + y * s;
  m[2][1] = y * z * t - x * s;
  m[2][2] = z * z * t + c;
  m[2][3] = 0.0f;
  m[3][0] = m[3][1] = m[3][2] = 0.0f;
  m[3][3] = 1.0f;
  return *this;
}
