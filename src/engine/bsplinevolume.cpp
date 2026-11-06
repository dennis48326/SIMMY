// Port of engine/bsplinevolume.cpp: lattices that deform model vertices (see bsplinevolume.hpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstring>

using simmy::be32;
using simmy::befloat;

namespace {
constexpr float kFromFixed = 1.0f / 4096.0f;  // 0.00024414062

float FromFixed(short s) { return static_cast<float>(s) * kFromFixed; }
short ToFixed(float f) { return static_cast<short>(static_cast<int>(f * 4096.0f)); }
}  // namespace

// 8030D828
// The spline's tangent weights for a tension and a continuity-like parameter.
void ComputeTCBMults(float tension, float bias, float& a, float& b) {
  const float half = (1.0f - tension) * 0.5f;
  a = half * (1.0f - bias);
  b = half * (2.0f - (1.0f - bias));
}

// 8030EEE4
void ComputeHermiteBasis(float t, float* h) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  const float k = (t3 + t3) - t2 * 3.0f;
  h[0] = k + 1.0f;
  h[3] = -t2 + t3;
  h[1] = -k;
  h[2] = (t - (t2 + t2)) + t3;
}

// 8030EF38
// The spline through the middle two of four points at t (0..1), tangents from the TCB weights.
EVec3 InterpSpline(float t, const EVec3* p, float a, float b, float ba, float ab) {
  float h[4];
  ComputeHermiteBasis(t, h);
  const float w2 = h[3] * ba + (h[2] * a + h[1]);
  const float w0 = -h[2] * b;
  const float w1 = (h[2] * ab + h[0]) - h[3] * a;
  const float w3 = h[3] * b;
  EVec3 out;
  for (int i = 0; i < 3; ++i) out[i] = p[0][i] * w0 + p[1][i] * w1 + p[2][i] * w2 + p[3][i] * w3;
  return out;
}

// 8030D864
BSplineVolume::BSplineVolume() {
  ComputeTCBMults(0.0f, 1.0f, m_tcbA, m_tcbB);
  m_tcbAB = m_tcbB - m_tcbA;
  m_tcbBA = m_tcbA - m_tcbB;
}

// 8030D8F8
BSplineVolume::~BSplineVolume() { Free(); }

// 8030D944
void BSplineVolume::Free() {
  m_points.clear();
  m_blended.clear();
  m_weights.clear();
}

// 8030DEB4
// A flag word (non-zero: no lattice), the name, id, grid size and the number of point sets, then
// every set's points as floats (x, y, z), turned into 4.12 fixed point.
int BSplineVolume::Read(const unsigned char* src) {
  const unsigned char* p = src + 4;
  if (be32(src) != 0) return 4;
  std::memcpy(m_name, p, sizeof m_name);
  m_id = be32(src + 0x84);
  m_nx = static_cast<int>(be32(src + 0x88));
  m_ny = static_cast<int>(be32(src + 0x8C));
  m_nz = static_cast<int>(be32(src + 0x90));
  m_count = static_cast<int>(be32(src + 0x94));
  p = src + 0x98;
  if (m_count > 1) {
    m_points.assign(m_count, {});
    for (auto& set : m_points) set = AllocateControlPoints(m_nx, m_ny, m_nz, 6);
    m_weights.assign(m_count - 1, 0.0f);  // GC: count * 4 - 1 bytes
    for (int v = 0; v < m_count; ++v)
      for (int x = 0; x < m_nx; ++x)
        for (int y = 0; y < m_ny; ++y)
          for (int z = 0; z < m_nz; ++z, p += 12) {
            short* pt = &m_points[v][PointIndex(x, y, z)];
            for (int c = 0; c < 3; ++c) pt[c] = ToFixed(befloat(p + c * 4));
          }
    m_blended = AllocateControlPoints(m_nx, m_ny, m_nz, 6);
    ParameterizeControlPoints();
    GenerateControlPointDeltas();
  }
  return static_cast<int>(p - src);
}

// 8030E1B4
void BSplineVolume::Write(EStream&) {}

// 8030E250
std::vector<short> BSplineVolume::AllocateControlPoints(int nx, int ny, int nz, int stride) {
  return std::vector<short>(static_cast<std::size_t>(nx) * ny * nz * (stride / 2), 0);
}

// 8030E348
// The bounds of the base lattice's inner points (the outer layer only shapes the spline).
void BSplineVolume::ParameterizeControlPoints() {
  bool first = true;
  for (int x = 1; x < m_nx - 1; ++x)
    for (int y = 1; y < m_ny - 1; ++y)
      for (int z = 1; z < m_nz - 1; ++z) {
        const short* pt = &m_points[0][PointIndex(x, y, z)];
        for (int c = 0; c < 3; ++c) {
          const float f = FromFixed(pt[c]);
          if (first) {
            m_min[c] = m_max[c] = f;
          } else {
            if (f < m_min[c]) m_min[c] = f;
            if (m_max[c] < f) m_max[c] = f;
          }
        }
        first = false;
      }
  m_invRange[2] = 1.0f / (m_max[2] - m_min[2]);
  m_invRange[0] = 1.0f / (m_max[0] - m_min[0]);
  m_invRange[1] = 1.0f / (m_max[1] - m_min[1]);
}

// 8030E758
EVec3 BSplineVolume::ParameterizeVert(EVec3 pos, unsigned int& cell) {
  const int n[3] = {m_nx, m_ny, m_nz};
  int idx[3];
  EVec3 out;
  for (int c = 0; c < 3; ++c) {
    float t = (pos[c] - m_min[c]) * m_invRange[c];
    if (1.0f < t) t = 1.0f;
    if (t < 0.0f) t = 0.0f;
    t *= static_cast<float>(n[c] - 3);
    int i = static_cast<int>(t);
    if (i < 0) i = 0;
    if (n[c] - 4 < i) i = n[c] - 4;
    idx[c] = i;
    out[c] = t - static_cast<float>(i);
  }
  cell = static_cast<unsigned int>(idx[0]) << 24 | static_cast<unsigned int>(idx[1]) << 16 |
         static_cast<unsigned int>(idx[2]);
  return out;
}

// 8030E9EC
// Every volume's points become their offsets from the base lattice (once).
void BSplineVolume::GenerateControlPointDeltas() {
  if (m_deltasMade) return;
  for (int v = 1; v < m_count; ++v)
    for (std::size_t i = 0; i < m_points[v].size(); ++i) m_points[v][i] = static_cast<short>(m_points[v][i] - m_points[0][i]);
  m_deltasMade = 1;
}

// 8030EB48
int BSplineVolume::GetNumVolumes() { return (!m_points.empty() && m_count > 1) ? m_count - 1 : 0; }

// 8030EB70
int BSplineVolume::SetVolumeWeights(int volume, float weight) {
  if (m_weights[volume] == weight) return m_dirty;
  m_weights[volume] = weight;
  m_dirty = 1;
  return 1;
}

// 8030EBA0
// blended = base + the sum of each volume's offsets times its weight (rounded to fixed point after
// each volume, as on the GC).
void BSplineVolume::BlendVolumes() {
  if (!m_dirty) return;
  for (std::size_t i = 0; i < m_blended.size(); ++i) {
    m_blended[i] = m_points[0][i];
    for (int v = 1; v < m_count; ++v) {
      const float w = m_weights[v - 1];
      if (w != 0.0f) m_blended[i] = ToFixed(FromFixed(m_blended[i]) + FromFixed(m_points[v][i]) * w);
    }
  }
  m_dirty = 0;
}

// 8030F0FC
// The blended lattice at a vertex's parameters: 4 x 4 splines along z, four along y, one along x.
EVec3 BSplineVolume::GetPosition(const EVec3* params, unsigned int cell) const {
  const int cx = static_cast<int>(cell >> 24) & 0xff, cy = static_cast<int>(cell >> 16) & 0xff,
            cz = static_cast<int>(cell) & 0xff;
  EVec3 alongZ[4][4];
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      EVec3 pts[4];
      for (int k = 0; k < 4; ++k) {
        const short* pt = &m_blended[PointIndex(cx + i, cy + j, cz + k)];
        pts[k] = EVec3(FromFixed(pt[0]), FromFixed(pt[1]), FromFixed(pt[2]));
      }
      alongZ[i][j] = InterpSpline(params->z, pts, m_tcbA, m_tcbB, m_tcbBA, m_tcbAB);
    }
  EVec3 alongY[4];
  for (int i = 0; i < 4; ++i) alongY[i] = InterpSpline(params->y, alongZ[i], m_tcbA, m_tcbB, m_tcbBA, m_tcbAB);
  return InterpSpline(params->x, alongY, m_tcbA, m_tcbB, m_tcbBA, m_tcbAB);
}

// 8030F3BC
unsigned int BSplineVolume::GetID() { return m_id; }

// operator<<(EStream&, BSplineVolume&) and operator>>(EStream&, BSplineVolume&) (8030E1B8, 8030E204)
// call the virtual Write/Read(EStream&); EStream reading is not ported (models use Read(src)).
