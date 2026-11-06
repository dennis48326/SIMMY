// @class BSplineVolume after EVec3
// A lattice (engine/bsplinevolume.cpp): a grid of nx * ny * nz control points that deforms the
// vertices inside it. The first set of points is the base lattice; every further set is a morph
// "volume", stored (after GenerateControlPointDeltas) as its offsets from the base. BlendVolumes
// adds the volumes by their weights into the blended lattice, and GetPosition evaluates that one at a
// vertex's parameters (ParameterizeVert: the vertex's cell and its position inside the cell) with a
// tensor-product Hermite (TCB) spline. Points are 4.12 fixed point (s16 x, y, z), as on the GC.
//
// ERModel keeps a model's lattices (TArray<BSplineVolume> at +0x104 on the GC, 0xE0 bytes each);
// sim body parts carry them so the CAS body shape sliders can fatten or thin them (MorphUtilities).
#pragma once
#include <vector>

struct BSplineVolume {
  BSplineVolume();  // 8030D864
  virtual ~BSplineVolume();  // 8030D8F8
  BSplineVolume(const BSplineVolume&) = delete;
  BSplineVolume& operator=(const BSplineVolume&) = delete;
  void Free();  // 8030D944
  virtual void Read(EStream &);  // 8030DB6C
  // Reads a stored lattice (big-endian); returns the bytes read.
  virtual int Read(const unsigned char* src);  // 8030DEB4
  virtual void Write(EStream &);  // 8030E1B4
  // One set of control points (GC: short** [nx][ny] of nz * 3 shorts), zeroed.
  std::vector<short> AllocateControlPoints(int nx, int ny, int nz, int stride);  // 8030E250
  void ParameterizeControlPoints();  // 8030E348
  // The vertex's position inside its cell (0..1 each way) and the cell, packed x << 24 | y << 16 | z.
  EVec3 ParameterizeVert(EVec3 pos, unsigned int& cell);  // 8030E758
  void GenerateControlPointDeltas();  // 8030E9EC
  int GetNumVolumes();  // 8030EB48
  int SetVolumeWeights(int volume, float weight);  // 8030EB70
  void BlendVolumes();  // 8030EBA0
  EVec3 GetPosition(const EVec3* params, unsigned int cell) const;  // 8030F0FC
  unsigned int GetID();  // 8030F3BC

  // The point (x, y, z) of a set of control points.
  int PointIndex(int x, int y, int z) const { return ((x * m_ny + y) * m_nz + z) * 3; }

  char m_name[0x80] = {};                     // +0x00
  unsigned int m_id = 0;                      // +0x80 low byte = vertex colour red * 2 of its vertices
  int m_nx = 0;                               // +0x84
  int m_ny = 0;                               // +0x88
  int m_nz = 0;                               // +0x8C
  int m_count = 0;                            // +0x90 sets of points: the base and the volumes
  std::vector<std::vector<short>> m_points;   // +0x94 [m_count] sets (GC: short*** each)
  std::vector<short> m_blended;               // +0x98
  int m_deltasMade = 0;                       // +0x9C GenerateControlPointDeltas is done
  int m_dirty = 1;                            // +0xA0 the weights changed since BlendVolumes
  std::vector<float> m_weights;               // +0xA4 [m_count - 1]
  float m_min[3] = {};                        // +0xA8 the base lattice's inner points' bounds
  float m_max[3] = {};                        // +0xB4
  float m_invRange[3] = {};                   // +0xC0
  float m_tcbA = 0, m_tcbB = 0;               // +0xCC, +0xD0 ComputeTCBMults(0, 1)
  float m_tcbBA = 0, m_tcbAB = 0;             // +0xD4 (a - b), +0xD8 (b - a)
};
