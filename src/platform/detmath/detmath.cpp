// The GameCube's single-precision libm, ported for bit-identical results on every PC build
// (include/simmy/detmath.hpp). Original: C:\Program Files\SN Systems\ngc\lib\libm.a (u2_ngc map), which
// is newlib's single-precision fdlibm (Sun Microsystems; licence in LICENSE.fdlibm next to this file)
// compiled by SN's GCC with fused multiply-adds. Each function follows the GameCube's code (the
// disassembly at the address above it): the same branches, the same constants (the ELF's .sdata
// words, as hex), and the same operations; where it has fmadds/fmsubs the port calls fmadds()/fmsubs()
// below (one rounding, done in portable integer-checked double arithmetic, so neither the CPU's FMA
// unit nor the C library's fmaf is involved). Plain float + - * / are IEEE single on the GameCube's
// paired-single unit and on x64 SSE, and float<->int conversions here are exact, so the results are the
// GameCube's, whatever the compiler or CPU.
//
// Build rules (CMakeLists.txt): no FMA contraction (-ffp-contract=off; MSVC /fp:precise without
// /fp:contract or /arch:AVX2), SSE2 floats (x64), no -ffast-math. The pragmas below repeat that here.
#include "simmy/detmath.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma float_control(precise, on)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")  // GCC ignores the STDC pragma (its default is contract=fast)
#endif

namespace simmy::dm {
namespace {

using i32 = std::int32_t;
using u32 = std::uint32_t;

inline i32 W(float x) { return std::bit_cast<i32>(x); }              // GET_FLOAT_WORD
inline float F(u32 w) { return std::bit_cast<float>(w); }            // SET_FLOAT_WORD
constexpr float K(u32 w) { return std::bit_cast<float>(w); }         // a constant's ELF word

// PowerPC fmadds frD = frA * frC + frB with a single rounding to float. a*c is exact in double (24+24
// bits); s = a*c + b is rounded to double, err is its exact rounding error (TwoSum); rounding s to odd
// (truncate, then set the last bit when inexact) and then to float gives the correctly rounded result
// (53 >= 24 + 2 bits). Plain double arithmetic, so it is the same on every IEEE machine.
inline float fma1(float a, float c, float b) {
  const double p = static_cast<double>(a) * static_cast<double>(c);
  const double bd = static_cast<double>(b);
  const double s = p + bd;
  const double v = s - p;
  const double err = (p - (s - v)) + (bd - v);
  if (err == 0.0 || !(s - s == 0.0)) return static_cast<float>(s);  // exact, or inf/NaN
  std::uint64_t u = std::bit_cast<std::uint64_t>(s);
  if ((err > 0.0) != (s > 0.0)) --u;  // the exact value is nearer zero: truncate
  u |= 1;
  return static_cast<float>(std::bit_cast<double>(u));
}
inline float fmsubs(float a, float c, float b) { return fma1(a, c, -b); }  // frA * frC - frB

inline float fabsf_(float x) { return F(static_cast<u32>(W(x)) & 0x7fffffffu); }  // 80241378 fabsf
// 8024265C copysignf
inline float copysignf_(float x, float y) {
  return F((static_cast<u32>(W(x)) & 0x7fffffffu) | (static_cast<u32>(W(y)) & 0x80000000u));
}

// 80242540
float scalbnf(float x, int n) {
  i32 ix = W(x);
  i32 k = (ix & 0x7f800000) >> 23;
  if (k == 0) {
    if ((ix & 0x7fffffff) == 0) return x;
    x *= K(0x4C000000);  // two25
    ix = W(x);
    k = ((ix & 0x7f800000) >> 23) - 25;
    if (n < -50000) return K(0x0DA24260) * x;  // tiny
  }
  if (k == 0xff) return x + x;
  k = k + n;
  if (k > 0xfe) return copysignf_(K(0x7149F2CA), x) * K(0x7149F2CA);  // huge
  if (k > 0) return F((static_cast<u32>(ix) & 0x807fffffu) | static_cast<u32>(k << 23));
  if (k <= -25) {
    if (n > 50000) return copysignf_(K(0x7149F2CA), x) * K(0x7149F2CA);
    return copysignf_(K(0x0DA24260), x) * K(0x0DA24260);
  }
  k += 25;
  x = F((static_cast<u32>(ix) & 0x807fffffu) | static_cast<u32>(k << 23));
  return x * K(0x33000000);  // twom25
}

// 802416AC
float kernel_sinf(float x, float y, int iy) {
  const i32 ix = W(x) & 0x7fffffff;
  if (ix < 0x32000000 && static_cast<int>(x) == 0) return x;
  const float z = x * x;
  const float v = z * x;
  float r = fma1(z, K(0x2F2EC9D3), K(0xB2D72F34));  // S6, S5
  r = fma1(z, r, K(0x3638EF1B));                    // S4
  r = fma1(z, r, K(0xB9500D01));                    // S3
  r = fma1(z, r, K(0x3C088889));                    // S2
  if (iy == 0) return fma1(v, fma1(z, r, K(0xBE2AAAAB)), x);  // x + v*(S1 + z*r)
  float t = fmsubs(y, K(0x3F000000), v * r);  // half*y - v*r
  t = fmsubs(z, t, y);
  t = t - v * K(0xBE2AAAAB);
  return x - t;
}

// 802415C0
float kernel_cosf(float x, float y) {
  const i32 ix = W(x) & 0x7fffffff;
  if (ix < 0x32000000 && static_cast<int>(x) == 0) return K(0x3F800000);
  const float z = x * x;
  float r = fma1(z, K(0xAD47D74E), K(0x310F74F6));  // C6, C5
  r = fma1(z, r, K(0xB493F27C));                    // C4
  r = fma1(z, r, K(0x37D00D01));                    // C3
  r = fma1(z, r, K(0xBAB60B61));                    // C2
  r = fma1(z, r, K(0x3D2AAAAB));                    // C1
  const float zr = z * r;
  if (ix < 0x3e99999a) {
    const float t = fmsubs(z, zr, x * y);        // z*r - x*y
    return K(0x3F800000) - fmsubs(z, K(0x3F000000), t);  // one - (0.5*z - (z*r - x*y))
  }
  const float qx = ix > 0x3f480000 ? K(0x3E900000) : F(static_cast<u32>(ix - 0x01000000));
  const float t = fmsubs(z, zr, x * y);
  float hz = fmsubs(z, K(0x3F000000), qx);  // 0.5*z - qx
  const float a = K(0x3F800000) - qx;
  hz = hz - t;
  return a - hz;
}

// 80241758
float kernel_tanf(float x, float y, int iy) {
  static constexpr float T[13] = {K(0x3EAAAAAB), K(0x3E088889), K(0x3D5D0DD1), K(0x3CB327A4), K(0x3C11371F),
                                  K(0x3B6B6916), K(0x3ABEDE48), K(0x3A1A26C8), K(0x398137B9), K(0x38A3F445),
                                  K(0x3895C07A), K(0xB79BAE5F), K(0x37D95384)};
  const i32 hx = W(x);
  const i32 ix = hx & 0x7fffffff;
  if (ix < 0x31800000 && static_cast<int>(x) == 0) {
    if ((ix | (iy + 1)) == 0) return K(0x3F800000) / fabsf_(x);
    return iy == 1 ? x : K(0xBF800000) / x;
  }
  if (ix >= 0x3f2ca140) {
    if (hx < 0) { x = -x; y = -y; }
    const float w = K(0x33222168) - y;  // pio4lo
    const float z = K(0x3F490FDA) - x;  // pio4
    x = z + w;
    y = 0.0f;
  }
  const float z = x * x;
  const float w = z * z;
  const float s = z * x;
  float r = fma1(w, T[11], T[9]);
  float v = fma1(w, T[12], T[10]);
  r = fma1(w, r, T[7]);
  v = fma1(w, v, T[8]);
  v = fma1(w, v, T[6]);
  r = fma1(w, r, T[5]);
  r = fma1(w, r, T[3]);
  v = fma1(w, v, T[4]);
  r = fma1(w, r, T[1]);
  v = fma1(w, v, T[2]);
  float q = fma1(z, v, r);   // r + z*v
  q = fma1(s, q, y);         // s*(r+v) + y
  r = fma1(z, q, y);         // y + z*(...)
  r = fma1(T[0], s, r);      // r += T0*s
  const float wv = x + r;
  if (ix >= 0x3f2ca140) {
    const float fv = static_cast<float>(iy);
    float t = wv * wv / (wv + fv);
    t = t - r;
    t = x - t;
    t = t + t;
    t = fv - t;
    return static_cast<float>(1 - ((hx >> 30) & 2)) * t;
  }
  if (iy == 1) return wv;
  const float a = K(0xBF800000) / wv;
  const float zt = F(static_cast<u32>(W(wv)) & 0xfffff000u);
  const float vv = r - (zt - x);
  const float t = F(static_cast<u32>(W(a)) & 0xfffff000u);
  const float s1 = fma1(t, zt, K(0x3F800000));  // 1 + t*z
  return fma1(a, fma1(t, vv, s1), t);             // t + a*(s + t*v)
}

const i32 two_over_pi[198] = {
    0xA2, 0xF9, 0x83, 0x6E, 0x4E, 0x44, 0x15, 0x29, 0xFC, 0x27, 0x57, 0xD1, 0xF5, 0x34, 0xDD, 0xC0,
    0xDB, 0x62, 0x95, 0x99, 0x3C, 0x43, 0x90, 0x41, 0xFE, 0x51, 0x63, 0xAB, 0xDE, 0xBB, 0xC5, 0x61,
    0xB7, 0x24, 0x6E, 0x3A, 0x42, 0x4D, 0xD2, 0xE0, 0x06, 0x49, 0x2E, 0xEA, 0x09, 0xD1, 0x92, 0x1C,
    0xFE, 0x1D, 0xEB, 0x1C, 0xB1, 0x29, 0xA7, 0x3E, 0xE8, 0x82, 0x35, 0xF5, 0x2E, 0xBB, 0x44, 0x84,
    0xE9, 0x9C, 0x70, 0x26, 0xB4, 0x5F, 0x7E, 0x41, 0x39, 0x91, 0xD6, 0x39, 0x83, 0x53, 0x39, 0xF4,
    0x9C, 0x84, 0x5F, 0x8B, 0xBD, 0xF9, 0x28, 0x3B, 0x1F, 0xF8, 0x97, 0xFF, 0xDE, 0x05, 0x98, 0x0F,
    0xEF, 0x2F, 0x11, 0x8B, 0x5A, 0x0A, 0x6D, 0x1F, 0x6D, 0x36, 0x7E, 0xCF, 0x27, 0xCB, 0x09, 0xB7,
    0x4F, 0x46, 0x3F, 0x66, 0x9E, 0x5F, 0xEA, 0x2D, 0x75, 0x27, 0xBA, 0xC7, 0xEB, 0xE5, 0xF1, 0x7B,
    0x3D, 0x07, 0x39, 0xF7, 0x8A, 0x52, 0x92, 0xEA, 0x6B, 0xFB, 0x5F, 0xB1, 0x1F, 0x8D, 0x5D, 0x08,
    0x56, 0x03, 0x30, 0x46, 0xFC, 0x7B, 0x6B, 0xAB, 0xF0, 0xCF, 0xBC, 0x20, 0x9A, 0xF4, 0x36, 0x1D,
    0xA9, 0xE3, 0x91, 0x61, 0x5E, 0xE6, 0x1B, 0x08, 0x65, 0x99, 0x85, 0x5F, 0x14, 0xA0, 0x68, 0x40,
    0x8D, 0xFF, 0xD8, 0x80, 0x4D, 0x73, 0x27, 0x31, 0x06, 0x06, 0x15, 0x56, 0xCA, 0x73, 0xA8, 0xC9,
    0x60, 0xE2, 0x7B, 0xC0, 0x8C, 0x6B};

// 80241CBC (prec is always 2 here: __ieee754_rem_pio2f's call)
int kernel_rem_pio2f(const float* x, float* y, int e0, int nx, const i32* ipio2) {
  static constexpr float PIo2[11] = {K(0x3FC90000), K(0x39F00000), K(0x37DA0000), K(0x33A20000),
                                     K(0x2E840000), K(0x2B500000), K(0x27C20000), K(0x22D00000),
                                     K(0x1FC40000), K(0x1BC60000), K(0x17440000)};
  const float zero = 0.0f, one = K(0x3F800000), two8 = K(0x43800000), twon8 = K(0x3B800000);
  i32 iq[20];
  float f[20], fq[20] = {}, q[20];
  const int jk = 9;  // init_jk[2]
  const int jp = jk;
  const int jx = nx - 1;
  int jv = (e0 - 3) / 8;
  if (jv < 0) jv = 0;
  int q0 = e0 - 8 * (jv + 1);

  int j = jv - jx, m = jx + jk;
  for (int i = 0; i <= m; i++, j++) f[i] = j < 0 ? zero : static_cast<float>(ipio2[j]);
  for (int i = 0; i <= jk; i++) {
    float fw = 0.0f;
    for (j = 0; j <= jx; j++) fw = fma1(x[j], f[jx + i - j], fw);
    q[i] = fw;
  }
  int jz = jk;
  int n, ih;
  float z;
recompute:
  {
    int i;
    for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
      const float fw = static_cast<float>(static_cast<i32>(twon8 * z));
      iq[i] = static_cast<i32>(z - two8 * fw);
      z = q[j - 1] + fw;
    }
  }
  z = scalbnf(z, q0);
  z -= K(0x41000000) * std::floor(z * K(0x3E000000));  // 8 * floorf(z * 0.125)
  n = static_cast<i32>(z);
  z -= static_cast<float>(n);
  ih = 0;
  if (q0 > 0) {
    const int i = iq[jz - 1] >> (8 - q0);
    n += i;
    iq[jz - 1] -= i << (8 - q0);
    ih = iq[jz - 1] >> (7 - q0);
  } else if (q0 == 0) {
    ih = iq[jz - 1] >> 7;
  } else if (z >= K(0x3F000000)) {
    ih = 2;
  }
  if (ih > 0) {
    n += 1;
    int carry = 0;
    for (int i = 0; i < jz; i++) {
      j = iq[i];
      if (carry == 0) {
        if (j != 0) { carry = 1; iq[i] = 0x100 - j; }
      } else {
        iq[i] = 0xff - j;
      }
    }
    if (q0 > 0) {
      switch (q0) {
        case 1: iq[jz - 1] &= 0x7f; break;
        case 2: iq[jz - 1] &= 0x3f; break;
      }
    }
    if (ih == 2) {
      z = one - z;
      if (carry != 0) z -= scalbnf(one, q0);
    }
  }
  if (z == zero) {
    j = 0;
    for (int i = jz - 1; i >= jk; i--) j |= iq[i];
    if (j == 0) {
      int k;
      for (k = 1; iq[jk - k] == 0; k++) {}
      for (int i = jz + 1; i <= jz + k; i++) {
        f[jx + i] = static_cast<float>(ipio2[jv + i]);
        float fw = 0.0f;
        for (j = 0; j <= jx; j++) fw = fma1(x[j], f[jx + i - j], fw);
        q[i] = fw;
      }
      jz += k;
      goto recompute;
    }
  }
  if (z == 0.0f) {
    jz -= 1;
    q0 -= 8;
    while (iq[jz] == 0) { jz--; q0 -= 8; }
  } else {
    z = scalbnf(z, -q0);
    if (z >= two8) {
      const float fw = static_cast<float>(static_cast<i32>(twon8 * z));
      iq[jz] = static_cast<i32>(z - two8 * fw);
      jz += 1;
      q0 += 8;
      iq[jz] = static_cast<i32>(fw);
    } else {
      iq[jz] = static_cast<i32>(z);
    }
  }
  float fw = scalbnf(one, q0);
  for (int i = jz; i >= 0; i--) {
    q[i] = fw * static_cast<float>(iq[i]);
    fw *= twon8;
  }
  for (int i = jz; i >= 0; i--) {
    fw = 0.0f;
    for (int k = 0; k <= jp && k <= jz - i; k++) fw = fma1(PIo2[k], q[i + k], fw);
    fq[jz - i] = fw;
  }
  fw = 0.0f;
  for (int i = jz; i >= 0; i--) fw += fq[i];
  y[0] = ih == 0 ? fw : -fw;
  fw = fq[0] - fw;
  for (int i = 1; i <= jz; i++) fw += fq[i];
  y[1] = ih == 0 ? fw : -fw;
  return n & 7;
}

// 8024196C
int ieee754_rem_pio2f(float x, float* y) {
  static constexpr u32 npio2_hw[32] = {
      0x3FC90F00, 0x40490F00, 0x4096CB00, 0x40C90F00, 0x40FB5300, 0x4116CB00, 0x412FED00, 0x41490F00,
      0x41623100, 0x417B5300, 0x418A3A00, 0x4196CB00, 0x41A35C00, 0x41AFED00, 0x41BC7E00, 0x41C90F00,
      0x41D5A000, 0x41E23100, 0x41EEC200, 0x41FB5300, 0x4203F200, 0x420A3A00, 0x42108300, 0x4216CB00,
      0x421D1400, 0x42235C00, 0x4229A500, 0x422FED00, 0x42363600, 0x423C7E00, 0x4242C700, 0x42490F00};
  const float pio2_1 = K(0x3FC90F80), pio2_1t = K(0x37354443), pio2_2 = K(0x37354400),
              pio2_2t = K(0x2E85A308), pio2_3 = K(0x2E85A300), pio2_3t = K(0x248D3132);
  const i32 hx = W(x);
  const i32 ix = hx & 0x7fffffff;
  if (ix <= 0x3f490fd8) { y[0] = x; y[1] = 0.0f; return 0; }
  if (ix < 0x4016cbe4) {  // |x| < 3pi/4
    if (hx > 0) {
      float z = x - pio2_1;
      float lo = pio2_1t;
      if ((ix & 0xfffffff0) == 0x3fc90fd0) { z -= pio2_2; lo = pio2_2t; }  // near pi/2
      y[0] = z - lo;
      y[1] = (z - y[0]) - lo;
      return 1;
    }
    float z = x + pio2_1;
    float lo = pio2_1t;
    if ((ix & 0xfffffff0) == 0x3fc90fd0) { z += pio2_2; lo = pio2_2t; }
    y[0] = z + lo;
    y[1] = (z - y[0]) + lo;
    return -1;
  }
  if (ix <= 0x43490f80) {  // |x| ~<= 2^7*(pi/2), medium size
    const float t = fabsf_(x);
    const i32 n = static_cast<i32>(fma1(t, K(0x3F22F984), K(0x3F000000)));  // t*invpio2 + half
    const float fn = static_cast<float>(n);
    float r = t - fn * pio2_1;
    float w = fn * pio2_1t;
    if (n < 32 && static_cast<u32>(ix & 0xffffff00) != npio2_hw[n - 1]) {
      y[0] = r - w;
    } else {
      const i32 jj = ix >> 23;
      y[0] = r - w;
      i32 i = jj - ((W(y[0]) >> 23) & 0xff);
      if (i > 8) {  // 2nd iteration, good to 57 bits
        float tt = r;
        w = fn * pio2_2;
        r = tt - w;
        w = fmsubs(fn, pio2_2t, (tt - r) - w);
        y[0] = r - w;
        i = jj - ((W(y[0]) >> 23) & 0xff);
        if (i > 25) {  // 3rd iteration, 74 bits
          tt = r;
          w = fn * pio2_3;
          r = tt - w;
          w = fmsubs(fn, pio2_3t, (tt - r) - w);
          y[0] = r - w;
        }
      }
    }
    y[1] = (r - y[0]) - w;
    if (hx < 0) { y[0] = -y[0]; y[1] = -y[1]; return -n; }
    return n;
  }
  if (ix >= 0x7f800000) { y[0] = y[1] = x - x; return 0; }  // inf or NaN
  const i32 e0 = (ix >> 23) - 134;
  float z = F(static_cast<u32>(ix - (e0 << 23)));
  float tx[3];
  for (int i = 0; i < 2; i++) {
    tx[i] = static_cast<float>(static_cast<i32>(z));
    z = (z - tx[i]) * K(0x43800000);  // two8
  }
  tx[2] = z;
  int nx = 3;
  while (tx[nx - 1] == 0.0f) nx--;
  const int n = kernel_rem_pio2f(tx, y, e0, nx, two_over_pi);
  if (hx < 0) { y[0] = -y[0]; y[1] = -y[1]; return -n; }
  return n;
}

}  // namespace

float fmadds(float a, float c, float b) { return fma1(a, c, b); }

// 80241464
float sin(float x) {
  const i32 ix = W(x) & 0x7fffffff;
  if (ix <= 0x3f490fd8) return kernel_sinf(x, 0.0f, 0);
  if (ix >= 0x7f800000) return x - x;
  float y[2];
  const int n = ieee754_rem_pio2f(x, y);
  switch (n & 3) {
    case 0: return kernel_sinf(y[0], y[1], 1);
    case 1: return kernel_cosf(y[0], y[1]);
    case 2: return -kernel_sinf(y[0], y[1], 1);
    default: return -kernel_cosf(y[0], y[1]);
  }
}

// 8024129C
float cos(float x) {
  const i32 ix = W(x) & 0x7fffffff;
  if (ix <= 0x3f490fd8) return kernel_cosf(x, 0.0f);
  if (ix >= 0x7f800000) return x - x;
  float y[2];
  const int n = ieee754_rem_pio2f(x, y);
  switch (n & 3) {
    case 0: return kernel_cosf(y[0], y[1]);
    case 1: return -kernel_sinf(y[0], y[1], 1);
    case 2: return -kernel_cosf(y[0], y[1]);
    default: return kernel_sinf(y[0], y[1], 1);
  }
}

// 80241540
float tan(float x) {
  const i32 ix = W(x) & 0x7fffffff;
  if (ix <= 0x3f490fda) return kernel_tanf(x, 0.0f, 1);
  if (ix >= 0x7f800000) return x - x;
  float y[2];
  const int n = ieee754_rem_pio2f(x, y);
  return kernel_tanf(y[0], y[1], 1 - ((n & 1) << 1));
}

// 80240FC8
float atan(float x) {
  static constexpr float atanhi[4] = {K(0x3EED6338), K(0x3F490FDA), K(0x3F7B985E), K(0x3FC90FDA)};
  static constexpr float atanlo[4] = {K(0x31AC3769), K(0x33222168), K(0x33140FB4), K(0x33A22168)};
  static constexpr float aT[11] = {K(0x3EAAAAAB), K(0xBE4CCCCD), K(0x3E124925), K(0xBDE38E38),
                                   K(0x3DBA2E6E), K(0xBD9D8795), K(0x3D886B35), K(0xBD6EF16B),
                                   K(0x3D4BDA59), K(0xBD15A221), K(0x3C8569D7)};
  const float one = K(0x3F800000);
  const i32 hx = W(x);
  const i32 ix = hx & 0x7fffffff;
  int id;
  if (ix >= 0x50800000) {  // |x| >= 2^34
    if (ix > 0x7f800000) return x + x;
    if (hx > 0) return atanhi[3] + atanlo[3];
    return -atanhi[3] - atanlo[3];
  }
  if (ix < 0x3ee00000) {  // |x| < 0.4375
    if (ix < 0x31000000 && K(0x7149F2CA) + x > one) return x;  // huge + x > one: inexact
    id = -1;
  } else {
    x = fabsf_(x);
    if (ix < 0x3f980000) {
      if (ix < 0x3f300000) { id = 0; x = ((x + x) - one) / (x + K(0x40000000)); }
      else { id = 1; x = (x - one) / (x + one); }
    } else if (ix < 0x401c0000) {
      id = 2;
      x = (x - K(0x3FC00000)) / fma1(x, K(0x3FC00000), one);  // (x-1.5) / (1 + 1.5*x)
    } else {
      id = 3;
      x = K(0xBF800000) / x;
    }
  }
  const float z = x * x;
  const float w = z * z;
  float s1 = fma1(w, aT[10], aT[8]);
  s1 = fma1(w, s1, aT[6]);
  s1 = fma1(w, s1, aT[4]);
  float s2 = fma1(w, aT[9], aT[7]);
  s2 = fma1(w, s2, aT[5]);
  s1 = fma1(w, s1, aT[2]);
  s2 = fma1(w, s2, aT[3]);
  s1 = fma1(w, s1, aT[0]);
  s2 = fma1(w, s2, aT[1]);
  s1 = z * s1;
  s2 = w * s2;
  if (id < 0) return x - x * (s1 + s2);
  const float zz = atanhi[id] - (fmsubs(x, s1 + s2, atanlo[id]) - x);
  return hx < 0 ? -zz : zz;
}

// 80240AE8
float atan2(float y, float x) {
  const float pi = K(0x40490FDA), pi_lo = K(0x34222168);
  const i32 hx = W(x), ix = hx & 0x7fffffff;
  const i32 hy = W(y), iy = hy & 0x7fffffff;
  if (ix > 0x7f800000 || iy > 0x7f800000) return x + y;  // NaN
  if (hx == 0x3f800000) return atan(y);                    // x = 1
  const int m = static_cast<int>((static_cast<u32>(hy) >> 31) | ((hx >> 30) & 2));
  if (iy == 0) {
    switch (m) {
      case 0: case 1: return y;
      case 2: return pi;        // pi + tiny
      case 3: return K(0xC0490FDA);  // -pi - tiny
    }
  }
  if (ix == 0) return hy < 0 ? K(0xBFC90FDB) : K(0x3FC90FDB);
  if (ix == 0x7f800000) {
    if (iy == 0x7f800000) {
      switch (m) {
        case 0: return K(0x3F490FDB);
        case 1: return K(0xBF490FDB);
        case 2: return K(0x4016CBE4);
        case 3: return K(0xC016CBE4);
      }
    } else {
      switch (m) {
        case 0: return 0.0f;
        case 1: return K(0x80000000);
        case 2: return pi;
        case 3: return K(0xC0490FDA);
      }
    }
  }
  if (iy == 0x7f800000) return hy < 0 ? K(0xBFC90FDB) : K(0x3FC90FDB);
  const i32 k = (iy - ix) >> 23;
  float z;
  if (k > 60) z = K(0x3FC90FDC);                 // |y/x| > 2^60: pi/2 + 0.5*pi_lo
  else if (hx < 0 && k < -60) z = 0.0f;          // |y|/x < -2^60
  else z = atan(fabsf_(y / x));
  switch (m) {
    case 0: return z;
    case 1: return F(static_cast<u32>(W(z)) ^ 0x80000000u);
    case 2: return pi - (z - pi_lo);
    default: return (z - pi_lo) - pi;
  }
}

namespace {
// The R(z) = p/q rational approximation shared by asinf and acosf (pS0..pS5, qS1..qS4).
inline float asin_p(float z) {
  float p = fma1(z, K(0x3811EF08), K(0x3A4F7F04));
  p = fma1(z, p, K(0xBD241146));
  p = fma1(z, p, K(0x3E4E0AA8));
  p = fma1(z, p, K(0xBEA6B090));
  p = fma1(z, p, K(0x3E2AAAAB));
  return z * p;
}
inline float asin_q(float z) {
  float q = fma1(z, K(0x3D9DC62E), K(0xBF303361));
  q = fma1(z, q, K(0x4001572D));
  q = fma1(z, q, K(0xC019D139));
  return fma1(z, q, K(0x3F800000));
}
}  // namespace

// 8024089C
float acos(float x) {
  const float one = K(0x3F800000), pio2_hi = K(0x3FC90FDA), pio2_lo = K(0x33A22168);
  const i32 hx = W(x);
  const i32 ix = hx & 0x7fffffff;
  if (ix == 0x3f800000) return hx > 0 ? 0.0f : K(0x40490FDB);  // pi + 2*pio2_lo
  if (ix > 0x3f800000) return (x - x) / (x - x);
  if (ix < 0x3f000000) {  // |x| < 0.5
    if (ix <= 0x23000000) return K(0x3FC90FDB);  // pio2_hi + pio2_lo
    const float z = x * x;
    const float r = asin_p(z) / asin_q(z);
    return pio2_hi - (x - (pio2_lo - x * r));
  }
  if (hx < 0) {  // x < -0.5
    const float z = (one + x) * K(0x3F000000);
    const float p = asin_p(z), q = asin_q(z);
    const float s = std::sqrt(z);  // 80240ED0 sqrtf (correctly rounded, as IEEE sqrt)
    const float r = p / q;
    const float w = fmsubs(r, s, pio2_lo);
    const float sw = s + w;
    return K(0x40490FDA) - (sw + sw);  // pi - 2*(s+w)
  }
  const float z = (one - x) * K(0x3F000000);  // x > 0.5
  const float s = std::sqrt(z);
  const float df = F(static_cast<u32>(W(s)) & 0xfffff000u);
  const float q = asin_q(z);
  const float c = (z - df * df) / (s + df);
  const float p = asin_p(z);
  const float r = p / q;
  const float w = fma1(r, s, c);
  const float t = df + w;
  return t + t;
}

// 8036D2A8
float asin(float x) {
  const float one = K(0x3F800000), pio2_hi = K(0x3FC90FDA), pio2_lo = K(0x33A22168), pio4_hi = K(0x3F490FDB);
  const i32 hx = W(x);
  const i32 ix = hx & 0x7fffffff;
  if (ix == 0x3f800000) return fma1(x, pio2_hi, x * pio2_lo);  // asin(+-1) = +-pi/2 with inexact
  if (ix > 0x3f800000) return (x - x) / (x - x);
  if (ix < 0x3f000000) {  // |x| < 0.5
    if (ix < 0x32000000 && K(0x7149F2CA) + x > one) return x;
    const float t = x * x;
    const float w = asin_p(t) / asin_q(t);
    return fma1(x, w, x);
  }
  const float w0 = one - fabsf_(x);
  const float t = w0 * K(0x3F000000);
  const float p = asin_p(t), q = asin_q(t);
  const float s = std::sqrt(t);
  float r;
  if (ix >= 0x3F79999A) {  // |x| > 0.975
    const float w = p / q;
    float u = fma1(s, w, s);
    u = u + u;
    r = pio2_hi - (u - pio2_lo);
  } else {
    const float df = F(static_cast<u32>(W(s)) & 0xfffff000u);
    float c = (t - df * df) / (s + df);
    const float rr = p / q;
    c = pio2_lo - (c + c);
    const float pp = fmsubs(s + s, rr, c);  // 2s*r - (pio2_lo - 2c)
    const float qq = pio4_hi - (df + df);
    r = pio4_hi - (pp - qq);
  }
  return hx > 0 ? r : -r;
}

// 8036D4D4
float exp(float x) {
  static constexpr float halF[2] = {K(0x3F000000), K(0xBF000000)};
  static constexpr float ln2HI[2] = {K(0x3F317180), K(0xBF317180)};
  static constexpr float ln2LO[2] = {K(0x3717F7D1), K(0xB717F7D1)};
  const float one = K(0x3F800000);
  const u32 w = static_cast<u32>(W(x));
  const int xsb = static_cast<int>(w >> 31);
  const u32 hx = w & 0x7fffffffu;
  if (hx > 0x42b17217u) {  // |x| >= 88.72...
    if (hx > 0x7f800000u) return x + x;               // NaN
    if (hx == 0x7f800000u) return xsb == 0 ? x : 0.0f;  // exp(+-inf)
    if (x > K(0x42B17180)) return K(0x7F800000);      // overflow: huge*huge
    if (x < K(0xC2CFF1B5)) return 0.0f;               // underflow: twom100*twom100
  }
  float hi = 0.0f, lo = 0.0f;
  int k = 0;
  if (hx > 0x3eb17218u) {  // |x| > 0.5 ln2
    if (hx < 0x3F851592u) {  // and < 1.5 ln2
      hi = x - ln2HI[xsb];
      lo = ln2LO[xsb];
      k = 1 - xsb - xsb;
    } else {
      k = static_cast<int>(fma1(x, K(0x3FB8AA3B), halF[xsb]));  // invln2*x + halF
      const float t = static_cast<float>(k);
      hi = x - t * ln2HI[0];
      lo = t * ln2LO[0];
    }
    x = hi - lo;
  } else if (hx < 0x31800000u) {  // |x| < 2^-28
    if (K(0x7149F2CA) + x > one) return one + x;
  }
  const float t = x * x;
  float c = fma1(t, K(0x3331BB4C), K(0xB5DDEA0E));  // P5, P4
  c = fma1(t, c, K(0x388AB355));
  c = fma1(t, c, K(0xBB360B61));
  c = fma1(t, c, K(0x3E2AAAAB));
  c = x - t * c;
  if (k == 0) return one - ((x * c) / (c - K(0x40000000)) - x);
  const float y = one - ((lo - (x * c) / (K(0x40000000) - c)) - hi);
  if (k >= -125) return F(static_cast<u32>(W(y) + (k << 23)));
  return F(static_cast<u32>(W(y) + ((k + 100) << 23))) * K(0x0D800000);  // * twom100
}

// 8036D700
float log(float x) {
  const float ln2_hi = K(0x3F317180), ln2_lo = K(0x3717F7D1);
  const float Lg1 = K(0x3F2AAAAB), Lg2 = K(0x3ECCCCCD), Lg3 = K(0x3E924925), Lg4 = K(0x3E638E29),
              Lg5 = K(0x3E3A3325), Lg6 = K(0x3E1CD04F), Lg7 = K(0x3E178897);
  i32 ix = W(x);
  i32 k = 0;
  if (ix < 0x00800000) {  // x < 2^-126
    if ((ix & 0x7fffffff) == 0) return K(0xFF800000);  // log(+-0) = -inf
    if (ix < 0) return (x - x) / 0.0f;                  // log(-#) = NaN
    k -= 25;
    x *= K(0x4C000000);  // two25: scale up subnormal x
    ix = W(x);
  }
  if (ix >= 0x7f800000) return x + x;
  k += (ix >> 23) - 127;
  ix &= 0x007fffff;
  const i32 i = (ix + (0x95f64 << 3)) & 0x800000;
  x = F(static_cast<u32>(ix | (i ^ 0x3f800000)));  // normalize x or x/2
  k += i >> 23;
  const float f = x - K(0x3F800000);
  if ((0x007fffff & (15 + ix)) < 16) {  // |f| < 2^-20
    if (f == 0.0f) {
      if (k == 0) return 0.0f;
      const float dk = static_cast<float>(k);
      return fma1(dk, ln2_hi, dk * ln2_lo);
    }
    const float R = (f * f) * (K(0x3F000000) - f * K(0x3EAAAAAB));
    if (k == 0) return f - R;
    const float dk = static_cast<float>(k);
    return fmsubs(dk, ln2_hi, (R - dk * ln2_lo) - f);
  }
  const float s = f / (K(0x40000000) + f);
  const float dk = static_cast<float>(k);
  const float z = s * s;
  const i32 ii = ix - (0x6147a << 3);
  const float w = z * z;
  const i32 j = (0x6b851 << 3) - ix;
  float t2 = fma1(w, Lg7, Lg5);
  t2 = fma1(w, t2, Lg3);
  t2 = fma1(w, t2, Lg1);
  float t1 = fma1(w, Lg6, Lg4);
  t2 = z * t2;
  t1 = fma1(w, t1, Lg2);
  const float R = fma1(w, t1, t2);  // t2 + t1
  if ((ii | j) > 0) {
    const float hfsq = (f * K(0x3F000000)) * f;
    if (k == 0) return f - (hfsq - s * (hfsq + R));
    return fmsubs(dk, ln2_hi, (hfsq - fma1(s, hfsq + R, dk * ln2_lo)) - f);
  }
  if (k == 0) return f - s * (f - R);
  return fmsubs(dk, ln2_hi, fmsubs(s, f - R, dk * ln2_lo) - f);
}

// 8036D980
float log10(float x) {
  i32 hx = W(x);
  i32 k = 0;
  if (hx < 0x00800000) {
    if ((hx & 0x7fffffff) == 0) return K(0xFF800000);
    if (hx < 0) return (x - x) / 0.0f;
    k -= 25;
    x *= K(0x4C000000);
    hx = W(x);
  }
  if (hx >= 0x7f800000) return x + x;
  k += (hx >> 23) - 127;
  const i32 i = static_cast<i32>((static_cast<u32>(k) & 0x80000000u) >> 31);
  hx = (hx & 0x007fffff) | ((0x7f - i) << 23);
  const float y = static_cast<float>(k + i);
  x = F(static_cast<u32>(hx));
  const float z = fma1(y, K(0x355427DB), log(x) * K(0x3EDE5BD9));  // y*log10_2lo + ivln10*log(x)
  return fma1(y, K(0x3E9A2080), z);                                // z + y*log10_2hi
}

// 8036DA8C
float pow(float x, float y) {
  static constexpr float bp[2] = {K(0x3F800000), K(0x3FC00000)};
  static constexpr float dp_h[2] = {0.0f, K(0x3F15C000)};
  static constexpr float dp_l[2] = {0.0f, K(0x35D1CFDC)};
  const float zero = 0.0f, one = K(0x3F800000), huge = K(0x7149F2CA), tiny = K(0x0DA24260);
  const i32 hx = W(x), hy = W(y);
  i32 ix = hx & 0x7fffffff;
  const i32 iy = hy & 0x7fffffff;
  if (iy == 0) return one;                                // y = 0
  if (ix > 0x7f800000 || iy > 0x7f800000) return x + y;  // NaN
  // yisint: 0 y is not an integer, 1 odd, 2 even (only for x < 0)
  int yisint = 0;
  if (hx < 0) {
    if (iy >= 0x4b800000) {
      yisint = 2;
    } else if (iy >= 0x3f800000) {
      const int k = (iy >> 23) - 0x7f;
      const i32 j = iy >> (23 - k);
      if ((j << (23 - k)) == iy) yisint = 2 - (j & 1);
    }
  }
  if (iy == 0x7f800000) {  // y = +-inf
    if (ix == 0x3f800000) return y - y;
    if (ix > 0x3f800000) return hy >= 0 ? y : zero;
    return hy < 0 ? -y : zero;
  }
  if (iy == 0x3f800000) return hy < 0 ? one / x : x;
  if (hy == 0x40000000) return x * x;
  if (hy == 0x3f000000 && hx >= 0) return std::sqrt(x);  // 80240ED0 sqrtf
  float ax = fabsf_(x);
  if (ix == 0x7f800000 || ix == 0 || ix == 0x3f800000) {
    float z = ax;
    if (hy < 0) z = one / z;
    if (hx < 0) {
      if (((ix - 0x3f800000) | yisint) == 0) z = (z - z) / (z - z);
      else if (yisint == 1) z = -z;
    }
    return z;
  }
  const u32 xsign = static_cast<u32>(hx) >> 31;
  if (((xsign - 1) | static_cast<u32>(yisint)) == 0) return (x - x) / (x - x);  // (x<0)**(non-int)
  float t1, t2;
  float sgn = one;
  if (iy > 0x4d000000) {  // |y| > 2^27
    if (ix < 0x3f7ffff8) return hy < 0 ? K(0x7F800000) : zero;
    if (ix > 0x3f800007) return hy > 0 ? K(0x7F800000) : zero;
    const float t = x - one;  // GameCube (fdlibm): x - 1, not ax - 1
    const float w = (t * t) * (K(0x3F000000) - t * (K(0x3EAAAAAB) - t * K(0x3E800000)));
    const float v = fmsubs(t, K(0x36ECA570), w * K(0x3FB8AA3B));  // t*ivln2_l - w*ivln2
    const float u = t * K(0x3FB8AA00);                              // ivln2_h*t
    t1 = F(static_cast<u32>(W(u + v)) & 0xfffff000u);
    t2 = v - (t1 - u);
  } else {
    int n = 0;
    if (ix < 0x00800000) {
      ax *= K(0x4B800000);  // two24
      n -= 24;
      ix = W(ax);
    }
    n += (ix >> 23) - 0x7f;
    const i32 j = ix & 0x007fffff;
    ix = j | 0x3f800000;
    int k;
    if (j <= 0x1cc471) k = 0;
    else if (j < 0x5db3d7) k = 1;
    else { k = 0; n += 1; ix -= 0x00800000; }
    ax = F(static_cast<u32>(ix));
    const float v0 = one / (ax + bp[k]);
    const float u0 = ax - bp[k];
    const float s = u0 * v0;
    const float s_h = F(static_cast<u32>(W(s)) & 0xfffff000u);
    // t_h = ax + bp[k] high (the GameCube's fdlibm: + 0x0040000, no low-bits mask)
    const float t_h = F(static_cast<u32>(((ix >> 1) | 0x20000000) + 0x0040000 + (k << 21)));
    const float t_l = ax - (t_h - bp[k]);
    float s2 = s * s;
    float rp = fma1(s2, K(0x3E53F142), K(0x3E6C3255));  // L6, L5
    rp = fma1(s2, rp, K(0x3E8BA305));
    rp = fma1(s2, rp, K(0x3EAAAAAB));
    rp = fma1(s2, rp, K(0x3EDB6DB7));
    rp = fma1(s2, rp, K(0x3F19999A));
    const float s_l = v0 * ((u0 - s_h * t_h) - s_h * t_l);
    float r = fma1(s2 * s2, rp, s_l * (s_h + s));
    s2 = s_h * s_h;
    const float th = F(static_cast<u32>(W((s2 + K(0x40400000)) + r)) & 0xfffff000u);
    const float tl = r - ((th - K(0x40400000)) - s2);
    const float u = s_h * th;
    const float v = fma1(s_l, th, tl * s);
    const float p_h = F(static_cast<u32>(W(u + v)) & 0xfffff000u);
    const float p_l = v - (p_h - u);
    const float z_h = p_h * K(0x3F763800);                                // cp_h
    const float z_l = fma1(p_h, K(0x369DC3A0), p_l * K(0x3F76384F)) + dp_l[k];  // cp_l*p_h + p_l*cp + dp_l
    const float t = static_cast<float>(n);
    t1 = F(static_cast<u32>(W(((z_h + z_l) + dp_h[k]) + t)) & 0xfffff000u);
    t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
  }
  if (((xsign - 1) | static_cast<u32>(yisint - 1)) == 0) sgn = K(0xBF800000);  // (-ve)**(odd int)
  // split y into y1 + y2 and compute (y1+y2)*(t1+t2)
  const float y1 = F(static_cast<u32>(hy) & 0xfffff000u);
  const float p_l = fma1(y - y1, t1, y * t2);
  float p_h = y1 * t1;
  float z = p_l + p_h;
  i32 j = W(z);
  if (j > 0x43000000) return sgn * huge * huge;  // z > 128
  if (j == 0x43000000) {
    if (p_l + K(0x3338AA3C) > z - p_h) return sgn * huge * huge;  // ovt
  } else if ((j & 0x7fffffff) > 0x43160000) {
    return sgn * tiny * tiny;  // z <= -150
  } else if (static_cast<u32>(j) == 0xc3160000u) {
    if (p_l <= z - p_h) return sgn * tiny * tiny;
  }
  // 2**(p_h+p_l)
  const i32 i = j & 0x7fffffff;
  int k = (i >> 23) - 0x7f;
  int n = 0;
  if (i > 0x3f000000) {  // |z| > 0.5: n = [z+0.5]
    n = j + (0x00800000 >> (k + 1));
    k = ((n & 0x7fffffff) >> 23) - 0x7f;
    const float t = F(static_cast<u32>(n & ~(0x007fffff >> k)));
    n = ((n & 0x007fffff) | 0x00800000) >> (23 - k);
    if (j < 0) n = -n;
    p_h -= t;
  }
  const float t = F(static_cast<u32>(W(p_l + p_h)) & 0xfffff000u);
  const float u = t * K(0x3F317200);                                  // lg2_h
  const float v = fma1(p_l - (t - p_h), K(0x3F317218), t * K(0x35BFBE8C));  // (p_l-(t-p_h))*lg2 + t*lg2_l
  z = u + v;
  const float w = v - (z - u);
  const float tt = z * z;
  float c = fma1(tt, K(0x3331BB4C), K(0xB5DDEA0E));
  c = fma1(tt, c, K(0x388AB355));
  c = fma1(tt, c, K(0xBB360B61));
  c = fma1(tt, c, K(0x3E2AAAAB));
  const float tc = z - tt * c;
  const float r = (z * tc) / (tc - K(0x40000000)) - fma1(z, w, w);
  z = one - (r - z);
  j = W(z) + (n << 23);
  if ((j >> 23) <= 0) z = scalbnf(z, n);  // subnormal output
  else z = F(static_cast<u32>(j));
  return sgn * z;
}

// ---- simmy --mathcheck -------------------------------------------------------------------------------

namespace {
struct Fnv {
  std::uint64_t h = 1469598103934665603ull;
  void add(float f) {
    u32 w = static_cast<u32>(W(f));
    if (f != f) w = 0x7fc00000u;  // any NaN (payloads are not part of the contract)
    for (int i = 0; i < 4; ++i) { h ^= (w >> (8 * i)) & 0xff; h *= 1099511628211ull; }
  }
};
// Fixed inputs: special values, then pseudo-random floats in the ranges the game uses and over all
// exponents (a fixed LCG, so every build sees the same inputs).
struct Inputs {
  u32 s = 12345;
  int i = 0;
  float next(float lo, float hi) {
    static constexpr u32 special[] = {0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000,
                                      0x7FC00000, 0x00000001, 0x00800000, 0x7F7FFFFF, 0x3F000000, 0x3FC90FDB,
                                      0x40490FDB, 0x40C90FDB, 0x3F490FDB, 0x43490F80, 0x4016CBE4, 0x3F7FFFFF};
    if (i < static_cast<int>(sizeof special / sizeof special[0])) return F(special[i++]);
    ++i;
    s = s * 1103515245u + 12345u;
    const u32 a = s;
    s = s * 1103515245u + 12345u;
    if ((i & 3) == 0) return F(a ^ (s >> 16));                             // any bit pattern
    const float u = static_cast<float>((a >> 8) & 0xffffff) * K(0x33800000);  // [0, 1)
    return lo + (hi - lo) * u;
  }
};
}  // namespace

namespace {
struct Row { const char* name; Fnv dm, crt; };
void MathcheckRows(Row (&rows)[11]) {
  const int N = 100000;
  {
    Inputs in;
    for (int k = 0; k < N; ++k) {
      const float a = in.next(-1000.0f, 1000.0f), b = in.next(-7.0f, 7.0f);
      for (const float x : {a, b}) {
        rows[0].dm.add(sin(x)); rows[0].crt.add(std::sin(x));
        rows[1].dm.add(cos(x)); rows[1].crt.add(std::cos(x));
        rows[2].dm.add(tan(x)); rows[2].crt.add(std::tan(x));
        rows[3].dm.add(atan(x)); rows[3].crt.add(std::atan(x));
        rows[7].dm.add(exp(x)); rows[7].crt.add(std::exp(x));
        rows[8].dm.add(log(x)); rows[8].crt.add(std::log(x));
        rows[9].dm.add(log10(x)); rows[9].crt.add(std::log10(x));
      }
      rows[4].dm.add(atan2(a, b)); rows[4].crt.add(std::atan2(a, b));
      rows[4].dm.add(atan2(b, a)); rows[4].crt.add(std::atan2(b, a));
      const float c = in.next(-1.0f, 1.0f);
      rows[5].dm.add(asin(c)); rows[5].crt.add(std::asin(c));
      rows[6].dm.add(acos(c)); rows[6].crt.add(std::acos(c));
      const float px = in.next(0.0f, 50.0f), py = in.next(-12.0f, 12.0f);
      rows[10].dm.add(pow(px, py)); rows[10].crt.add(std::pow(px, py));
      rows[10].dm.add(pow(b, std::floor(py))); rows[10].crt.add(std::pow(b, std::floor(py)));
    }
  }
}
}  // namespace

// PC mod: network play (pc_net.cpp): the total --mathcheck prints, compared in the handshake.
unsigned long long mathcheck_hash() {
  Row rows[11] = {{"sin"}, {"cos"}, {"tan"}, {"atan"}, {"atan2"}, {"asin"}, {"acos"},
                  {"exp"}, {"log"}, {"log10"}, {"pow"}};
  MathcheckRows(rows);
  Fnv total;
  for (const Row& r : rows) {
    total.add(F(static_cast<u32>(r.dm.h))); total.add(F(static_cast<u32>(r.dm.h >> 32)));
  }
  return static_cast<unsigned long long>(total.h);
}

int mathcheck(const char* outPath) {
  Row rows[11] = {{"sin"}, {"cos"}, {"tan"}, {"atan"}, {"atan2"}, {"asin"}, {"acos"},
                  {"exp"}, {"log"}, {"log10"}, {"pow"}};
  MathcheckRows(rows);
  Fnv total, crt;
  for (const Row& r : rows) {
    total.add(F(static_cast<u32>(r.dm.h))); total.add(F(static_cast<u32>(r.dm.h >> 32)));
    crt.add(F(static_cast<u32>(r.crt.h))); crt.add(F(static_cast<u32>(r.crt.h >> 32)));
  }
  FILE* out = outPath ? std::fopen(outPath, "w") : stdout;
  if (!out) out = stdout;
  std::fprintf(out, "mathcheck %016llx\n", static_cast<unsigned long long>(total.h));
  for (const Row& r : rows)
    std::fprintf(out, "  %-6s %016llx  (C library %016llx)\n", r.name, static_cast<unsigned long long>(r.dm.h),
                 static_cast<unsigned long long>(r.crt.h));
  std::fprintf(out, "crt %016llx (the C library's own functions: expected to differ between Windows and Linux)\n",
               static_cast<unsigned long long>(crt.h));
  if (out != stdout) std::fclose(out);
  return 0;
}

}  // namespace simmy::dm
