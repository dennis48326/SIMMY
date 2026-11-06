// Port of engine/effect.cpp (Effects::Effect), engine/effectsfunctions.cpp (the rest of it),
// engine/effectsmath.cpp and engine/fastparticleemitter.cpp (Effects::FastParticleEmitter), the
// newer effects system's burst particle emitter: shower water and steam, stove smoke, the outhouse,
// fire extinguishers, the world map's effects.
//
// An emitter bursts every m_burstInterval seconds (a random count of particles each time, born on a
// ring/cone about it or in a box/sphere, at a speed and a spin), keeps them in packets of 16 from a
// shared pool (a ring buffer: the oldest die first) and each frame works out every particle's
// position, size, colour, alpha and sheet frame from its age alone, drawing them as camera-facing
// quads through ERC::QuadList (256 vertices at a time) with a shader made for it.
//
// Read from the raw decompilation and, where Ghidra lost track (DoSimulationAndRender, RenderQuad,
// UpdateBurst, RotateEmitter, the colour bits of AddBurst), the assembly.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

namespace {
float FloatFromBits(unsigned int u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
unsigned int BitsFromFloat(float f) {
  unsigned int u;
  std::memcpy(&u, &f, 4);
  return u;
}
unsigned int Rotl(unsigned int r, int n) { return r << n | r >> (32 - n); }
float* M(EMat4& m) { return &m.m[0][0]; }
}  // namespace

// ---------------------------------------------------------------------------------------------------
// Effects::Effect (engine/effect.cpp)

// 80351E20
Effects__Effect::Effects__Effect() {
  m_sortLayer = 0;
  m_dofForeground = 0;
  m_sortPos = nullptr;
  m_sortByEmitter = 0;
  m_matrix.Id();
  DefaultInit();
}

// 80351E7C
Effects__Effect::Effects__Effect(ClientParams& params) {
  m_sortLayer = 0;
  m_sortByEmitter = 0;
  m_sortPos = nullptr;
  m_dofForeground = 0;
  m_matrix.Id();
  if (static_cast<ClientParams*>(this) != &params) static_cast<ClientParams&>(*this) = params;
  DefaultInit();
}

// 80351F84
// The parent transform starts as the client's matrix; updated, drawn, keeping the parent's rotation
// and the local offset.
void Effects__Effect::DefaultInit() {
  m_parent = m_matrix;
  m_flags = kRender | kUpdate | kKeepLocalOffset | kKeepParentRotation;  // 0xD04
}

// 80352014
// Without kKeepParentRotation only the parent's position counts.
void Effects__Effect::SetParentTransform(EMat4& m) {
  m_parent = m;
  if (m_flags & kKeepParentRotation) return;
  float* p = M(m_parent);
  p[9] = 0.0f, p[10] = 1.0f, p[0] = 1.0f, p[1] = 0.0f, p[2] = 0.0f;
  p[4] = 0.0f, p[5] = 1.0f, p[6] = 0.0f, p[8] = 0.0f;
}

// ---------------------------------------------------------------------------------------------------
// engine/effectsfunctions.cpp (the random table is in psystem.cpp)

// 803521E0
ERTexture* Effects::GetTextureFromID(unsigned int id) { return static_cast<ERTexture*>(_textureman.GetRef(id)); }
// 8035220C
ERC* Effects::GetDefaultRenderContext() { return nullptr; }
// 80352214
void Effects::GetFaceCameraRotation(ERC*, EMat4&, EVec4&, bool) {}
// 80352218
void Effects::FreeFastAllocPool(FastAllocPool* pool) { delete pool; }

// ---------------------------------------------------------------------------------------------------
// engine/effectsmath.cpp

namespace {
// Effects::kSinCosTable (80422728): per 5 degrees of the first quadrant, sin and its slope, cos and
// its slope (s_pSinCosTable 804FF40C points at it).
const unsigned int kSinCosTableBits[20][4] = {
    {0x00000000, 0x3DB27EB6, 0x3F800000, 0xBB79625F}, {0x3DB27EB6, 0x3DB122F2, 0x3F7F069E, 0xBC3A904F},
    {0x3E31D0D4, 0x3DAE6E10, 0x3F7C1C5C, 0xBC9AAE45}, {0x3E8483EE, 0x3DAA6556, 0x3F7746EA, 0xBCD6E705},
    {0x3EAF1D44, 0x3DA510A0, 0x3F708FB2, 0xBD08BE89}, {0x3ED8616C, 0x3D9E7A51, 0x3F6803CA, 0xBD24FF24},
    {0x3F000000, 0x3D96AF3E, 0x3F5DB3D7, 0xBD3FFE48}, {0x3F12D5E8, 0x3D8DBE96, 0x3F51B3F3, 0xBD59875B},
    {0x3F248DBB, 0x3D83B9C5, 0x3F441B7D, 0xBD71689E}, {0x3F3504F3, 0x3D71689E, 0x3F3504F3, 0xBD83B9C5},
    {0x3F441B7D, 0x3D59875B, 0x3F248DBB, 0xBD8DBE96}, {0x3F51B3F3, 0x3D3FFE48, 0x3F12D5E8, 0xBD96AF3E},
    {0x3F5DB3D7, 0x3D24FF24, 0x3F000000, 0xBD9E7A51}, {0x3F6803CA, 0x3D08BE89, 0x3ED8616C, 0xBDA510A0},
    {0x3F708FB2, 0x3CD6E705, 0x3EAF1D44, 0xBDAA6556}, {0x3F7746EA, 0x3C9AAE45, 0x3E8483EE, 0xBDAE6E10},
    {0x3F7C1C5C, 0x3C3A904F, 0x3E31D0D4, 0xBDB122F2}, {0x3F7F069E, 0x3B79625F, 0x3DB27EB6, 0xBDB27EB6},
    {0x3F800000, 0x00000000, 0x00000000, 0x00000000}, {0x00000000, 0x00000000, 0x00000000, 0x00000000},
};
float SinCosTable(int row, int col) { return FloatFromBits(kSinCosTableBits[row][col]); }
}  // namespace

// 80352A14
// sin and cos from the table: the angle in quarter turns, the quadrant, then 18 linear segments.
void Effects::SinCosf(float a, float& s, float& c) {
  if (a == 0.0f) {
    s = 0.0f;
    c = 1.0f;
    return;
  }
  const float q = a * 0.6366197466850281f;
  const int qi = static_cast<int>(q);
  unsigned int quadrant = static_cast<unsigned int>(qi) & 3;
  float f = q - static_cast<float>(qi);
  if (f < 0.0f) {
    quadrant -= 1;
    f += 1.0f;
  }
  const int seg = static_cast<int>(f * 18.0f);
  f = f * 18.0f - static_cast<float>(seg);
  const float cv = f * SinCosTable(seg, 3) + SinCosTable(seg, 2);
  const float sv = f * SinCosTable(seg, 1) + SinCosTable(seg, 0);
  if (quadrant == 1) {
    s = cv;
    c = -sv;
  } else if (static_cast<int>(quadrant) < 2 && quadrant == 0) {
    s = sv;
    c = cv;
  } else if (quadrant == 2) {
    s = -sv;
    c = -cv;
  } else {
    s = -cv;
    c = sv;
  }
}

// 80352B68
// The rotation part of `m` from Euler angles (x, y, z), skipping the zero ones.
void Effects::Mat4SetEulerAngles34(float x, float y, float z, EMat4& m) {
  float* p = M(m);
  float sx, cx, sy, cy, sz, cz;
  if (z == 0.0f) {
    if (y == 0.0f) {
      p[9] = 0.0f, p[6] = 0.0f, p[5] = 1.0f, p[4] = 0.0f, p[1] = 0.0f;
      if (x == 0.0f) {
        p[0] = 1.0f, p[2] = 0.0f, p[10] = 1.0f, p[8] = 0.0f;
      } else {
        SinCosf(x, sx, cx);
        p[8] = sx, p[0] = cx, p[2] = -sx, p[10] = cx;
      }
    } else {
      SinCosf(y, sy, cy);
      if (x == 0.0f) {
        p[1] = 0.0f, p[0] = 1.0f, p[5] = cy, p[9] = -sy, p[8] = 0.0f, p[4] = 0.0f, p[2] = 0.0f, p[10] = cy, p[6] = sy;
      } else {
        SinCosf(x, sx, cx);
        p[2] = -sx, p[1] = 0.0f, p[6] = sy * cx, p[9] = -sy, p[8] = cy * sx, p[4] = sy * sx, p[10] = cy * cx;
        p[0] = cx, p[5] = cy;
      }
    }
  } else {
    SinCosf(z, sz, cz);
    if (y == 0.0f) {
      if (x == 0.0f) {
        p[2] = 0.0f, p[10] = 1.0f, p[5] = cz, p[4] = -sz, p[9] = 0.0f, p[8] = 0.0f, p[6] = 0.0f, p[0] = cz, p[1] = sz;
      } else {
        SinCosf(x, sx, cx);
        p[8] = sx, p[10] = cx, p[9] = 0.0f, p[6] = sz * sx, p[2] = -(cz * sx), p[4] = -(sz * cx), p[0] = cz * cx;
        p[1] = sz, p[5] = cz;
      }
    } else {
      SinCosf(y, sy, cy);
      if (x == 0.0f) {
        p[9] = -sy, p[8] = 0.0f, p[2] = sz * sy, p[5] = cz * cy, p[4] = -sz, p[1] = sz * cy, p[6] = cz * sy;
        p[10] = cy, p[0] = cz;
      } else {
        SinCosf(x, sx, cx);
        p[2] = sz * sy * cx - cz * sx;
        p[10] = cy * cx;
        p[6] = sz * sx + sy * cz * cx;
        p[0] = cz * cx + sz * sy * sx;
        p[4] = sy * cz * sx - sz * cx;
        p[5] = cz * cy;
        p[8] = cy * sx;
        p[9] = -sy;
        p[1] = sz * cy;
      }
    }
  }
  p[3] = 0.0f, p[11] = 0.0f, p[7] = 0.0f;
}

// 80352EA0
// The 3x3 rotation of a * b into out (out's last row untouched).
void Effects::Mat4Multiply34(EMat4& a, EMat4& b, EMat4& out) {
  float r[12];
  const float* pa = M(a);
  const float* pb = M(b);
  for (int row = 0; row < 3; ++row) {
    const float x = pa[row * 4], y = pa[row * 4 + 1], z = pa[row * 4 + 2];
    for (int k = 0; k < 3; ++k) r[row * 4 + k] = z * pb[8 + k] + x * pb[k] + y * pb[4 + k];
    r[row * 4 + 3] = 0.0f;
  }
  std::memcpy(M(out), r, sizeof(r));
}

// 8035301C
void Effects::Mat4Copy34(EMat4& src, EMat4& dst) { std::memcpy(M(dst), M(src), 12 * sizeof(float)); }

// ---------------------------------------------------------------------------------------------------
// Effects::FastParticleEmitter

FastAllocPool* Effects::gFastParticlePacketsPool = nullptr;  // 804FF410
int Effects::gsNumberFastParticlePacketsInPool = 0;          // 804FF414
float Effects::s_minAlpha = 10.0f;                           // 804FF418
unsigned char Effects::s_minAlphaTestThreshold = 5;          // 804FF41C
float Effects::s_minSize = 0.005f;                           // 804FF420
float* Effects::gpCurrentMatrix1Ptr = nullptr;
int Effects__FastParticleEmitter::s_renderBufferIndex = 0;
float Effects__FastParticleEmitter::s_vertArray[256][4];
float Effects__FastParticleEmitter::s_texCoordArray[256][2];
unsigned char Effects__FastParticleEmitter::s_colorArray[256][4];

namespace {
unsigned char* gFastParticlePoolBuffer = nullptr;  // 804FF40C.. (MainHeap, 64-aligned)
}

// 80353090 Effects::FastParticleEmitter::InitPlatform (effectsmanager.cpp)
// 80353094 Effects::FastParticleEmitter::InitModule (effectsmanager.cpp)

// 803530C4
Effects__FastParticleEmitter::Effects__FastParticleEmitter() {}
// 8035312C
Effects__FastParticleEmitter::Effects__FastParticleEmitter(ClientParams& params) : Effects__Effect(params) {}

// 80353194
// The particles back to the pool, the resource's reference, the shader made for it.
// (GC: an emitter made from copied data freed it here; the port only makes them from resources.)
Effects__FastParticleEmitter::~Effects__FastParticleEmitter() {
  FreeParticlePackets();
  if (m_resource) m_resource->DelRef();
  if (m_shader) _pGfx->Destroy(m_shader);
}

// 80353298
void Effects__FastParticleEmitter::FreeParticlePackets() {
  FastAllocPool* pool = Effects::gFastParticlePacketsPool;
  if (m_packets) {
    for (int i = 0; i < m_packetCount; ++i)
      if (m_packets[i]) pool->Free(m_packets[i]);
    delete[] m_packets;
    m_packets = nullptr;
  }
  m_head = 0;
  m_packetCount = 0;
  m_count = 0;
}

// 80353344
void Effects__FastParticleEmitter::SharedInitPart1() {
  m_local.Id();
  m_world.Id();
  m_state = 0;
  m_alphaScale = 1.0f;
  ComputeCombinedMatrix();
}

// 80353398
// Everything from the description: the emitter's rotation and offset, whether it ever ends, its
// packets (pre-rolled with flag 0x100000), and the shader its quads draw with (the texture, the
// blend from flags 0x80 / 0x100, the alpha test at least s_minAlphaTestThreshold).
int Effects__FastParticleEmitter::SharedInitPart2() {
  InitData* ei = m_init;
  if (ei->m_flags & 0x8000) m_flags &= ~kKeepParentRotation;
  if (ei->m_flags & 0x2000) m_flags &= ~kKeepLocalOffset;
  m_rotZ = ei->m_rotZ;
  m_rotY = ei->m_rotY;
  m_burstInterval = ei->m_burstInterval;
  m_rateScale = 1.0f;
  m_maxParticles = ei->m_maxParticles;
  m_radius = ei->m_radius;
  m_radiusSpeed = ei->m_radiusSpeed;
  if (m_flags & 0x10) {
    m_burstInterval = 1.0f;
    m_state |= kSuspended;
  }
  bool infinite = true;
  if (0.0f <= m_burstInterval || ei->m_life <= 0.0f) {
    if (0 < ei->m_maxBursts || 0.0f < ei->m_duration) infinite = false;
  } else {
    infinite = false;
    m_burstInterval = 0.0f;
    m_state |= kNoBurst;
  }
  if (infinite) m_flags |= kInfinite;
  m_time = 0.0f;
  RotateEmitter(0.0f);
  m_local.m[3][0] += ei->m_offset[0];
  m_local.m[3][1] += ei->m_offset[1];
  m_local.m[3][2] += ei->m_offset[2];
  ComputeCombinedMatrix();
  m_prevPos = EVec4(m_world.m[3][0], m_world.m[3][1], m_world.m[3][2], m_world.m[3][3]);
  m_deaths = 0;
  m_field16C = 0.0f;
  m_field170 = 0.5f;
  m_state |= kEmpty;
  m_lastDt = 0.0f;
  m_bursts = 0;
  m_burstTimer = 0.0f;
  m_colorScale = EVec4(1.0f, 1.0f, 1.0f, 1.0f);
  m_fieldEC = m_radius;
  FreeParticlePackets();
  const int packets = (m_init->m_maxParticles + 15) / 16;
  m_packets = new Packet*[packets > 0 ? packets : 1]();
  if (!Effects::gFastParticlePacketsPool) return 0;
  if (!AllocateParticlePackets(packets)) return 0;
  if ((m_init->m_flags & 0x100000) && !(m_flags & 0x10)) PreRollParticles();

  EShaderDef def;
  def.m_layerCount = 1;
  def.m_field01 = 0;
  def.m_field02 = 0;
  def.m_surface = 0;
  def.m_geometryModes = 0;
  EShaderLayer& layer = def.m_layers[0];
  layer.m_texture = m_texture ? m_texture->m_texture : nullptr;
  const unsigned int flags = m_init->m_flags;
  if (flags & 0x80) {
    layer.m_blend[1] = 2, layer.m_blend[3] = 1, layer.m_blend[0] = 0;
  } else if (flags & 0x100) {
    layer.m_blend[0] = 1, layer.m_blend[1] = 0, layer.m_blend[3] = 2;
  } else {
    layer.m_blend[3] = 1, layer.m_blend[0] = 0, layer.m_blend[1] = 1;
  }
  layer.m_blend[2] = 0;
  if (m_init->m_alphaTest < Effects::s_minAlphaTestThreshold) m_init->m_alphaTest = Effects::s_minAlphaTestThreshold;
  layer.m_field0C |= 0x40;
  layer.m_texGen = 0;
  layer.m_field14 = static_cast<float>(m_init->m_alphaTest) / 255.0f;
  layer.m_flags = 4;
  layer.m_field21 = 0;
  if ((m_init->m_flags2 ^ 1) & 1) layer.m_flags = 0xc;
  if (m_init->m_flags & 0x10000000) layer.m_flags |= 0x10;
  if (!m_shader) m_shader = _pGfx->CreateShader(def);
  else m_shader->Create(def);
  return 1;
}

// 80353930
// A new particle count (in whole packets); the emitter starts over (pre-rolled with flag 0x100000).
int Effects__FastParticleEmitter::ReallocateParticles(int count) {
  if (count == m_maxParticles) return 0;
  FreeParticlePackets();
  const int packets = (count + 15) / 16;
  m_packets = new Packet*[packets > 0 ? packets : 1]();
  FastAllocPool* pool = Effects::gFastParticlePacketsPool;
  if (!pool) return 0;
  int got = count >> 4;
  for (int i = 0; i < count >> 4; ++i) {
    m_packets[i] = static_cast<Packet*>(pool->Alloc());
    if (!m_packets[i]) {
      got = i;
      break;
    }
  }
  if (got == 0) {
    m_packetCount = 0;
    m_maxParticles = 0;
    return 0;
  }
  if (m_init->m_flags & 0x1000)
    for (int i = 0; i < got; ++i)
      for (State& s : m_packets[i]->m_state) s.m_rot = 1024.0f;
  m_packetCount = static_cast<short>(got);
  m_maxParticles = static_cast<short>(got << 4);
  if ((m_init->m_flags & 0x100000) && !(m_flags & 0x10)) PreRollParticles();
  return 1;
}

// 80353C84
// Runs `res`: its description, its texture and its birth data.
int Effects__FastParticleEmitter::Init(REffectsEmitter& res) {
  m_resource = &res;
  res.AddRef();
  SharedInitPart1();
  m_init = res.GetEmitterInitData();
  if (!m_init) return 0;
  m_field166 = m_init->m_fieldA8;
  m_texture = res.GetTexture();
  m_birth = res.GetParticleBirthData();
  if (!m_birth) return 0;
  return SharedInitPart2();
}

// 80353D08
// The next burst is due: the interval (random with flag 0x40000) until the one after, and a burst of
// burstMin + random * burstRange particles (times the rate scale, as many as fit) born at `birth`.
// Returns the interval (0: none, or no more bursts).
float Effects__FastParticleEmitter::UpdateBurst(int count, float birth, int* added) {
  *added = 0;
  if (m_burstInterval < 0.0f && (m_state & kBursting)) return 0.0f;
  float interval;
  if (m_init->m_flags & 0x40000)
    interval = m_burstInterval * (m_init->m_burstIntervalRandom + Effects::TableRand());
  else
    interval = m_burstInterval;
  InitData* ei = m_init;
  int n = static_cast<int>(m_rateScale * (Effects::TableRand() * static_cast<float>(ei->m_burstRange) +
                                          static_cast<float>(ei->m_burstMin)));
  m_burstTimer += interval;
  const int room = ei->m_maxParticles - count;
  if (room < n) n = room;
  if (n == 0 || (0 <= ei->m_maxBursts && ei->m_maxBursts <= m_bursts)) return 0.0f;
  if (!(m_state & kSuspended)) *added = AddBurst(n, birth);
  return interval;
}

// 80353EF4
// `count` particles into the ring after the live ones, born `birth` seconds ago: position and
// velocity (ComputeDispersionPositionAndVelocity, spread over the burst by t = 0..1), rotation and
// spin (or facing the direction of travel, flag 0x4000), size, alpha rate and colour (RGB 5:5:5 in
// sixteenths: fixed white, lerped with t, scaled by the emitter's colour, or random).
int Effects__FastParticleEmitter::AddBurst(int count, float birth) {
  if (count < 1) return 0;
  if (m_init->m_flags2 & 4) birth = 0.0f;
  const unsigned short flip = m_init->m_flipU == 1 ? 0x8000 : 0;
  const float step = 1.0f / static_cast<float>(count);
  unsigned int colorBits = 0x4210;
  m_state |= kBursting;
  int slot = m_head + m_count;
  if (m_maxParticles <= slot) slot -= m_maxParticles;
  m_state = (m_state & ~kEmpty) | kBursting;
  if (m_maxParticles < m_count + count) count = m_maxParticles - m_count;
  Effects::gpCurrentMatrix1Ptr = M(m_world);
  float t = 0.0f;
  int done = 0;
  for (; done < count; ++done) {
    Packet* pk = PacketOf(slot);
    const int i = slot & 15;
    pk->m_age[i] = birth;
    State* s = &pk->m_state[i];
    if (!(m_init->m_flags & 0x1000) || s->m_rot == 1024.0f) {
      ComputeDispersionPositionAndVelocity(s, t, step, birth);
      if (m_init->m_flags & 0x20000000) {
        const unsigned int r = Effects::NextRand32();
        s->m_pos[0] = FloatFromBits((BitsFromFloat(s->m_pos[0]) & 0xfffff000) | (r & 0xfff));
      }
      InitData* ei = m_init;
      const unsigned int flags = ei->m_flags;
      if (!(flags & 0x4000)) {
        if (!(flags & 1)) {
          s->m_rot = ei->m_rotation;
        } else if (!(flags & 0x4000000)) {
          s->m_rot = Effects::TableRand() * 6.2831855f;
        } else {
          s->m_rot = (0.5f - Effects::TableRand()) * (ei->m_rotationRandom + ei->m_rotationRandom) + ei->m_rotation;
        }
        if (!(ei->m_flags & 2)) {
          s->m_spin = ei->m_spin;
        } else {
          float f = Effects::TableRand();
          if (!(ei->m_flags & 0x400000)) f = 1.0f - (f + f);
          s->m_spin = ei->m_spin * f;
        }
      } else {
        // Facing the direction of travel in the view.
        const float mag = std::sqrt(s->m_vel[0] * s->m_vel[0] + s->m_vel[1] * s->m_vel[1] + s->m_vel[2] * s->m_vel[2]);
        auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
        if (0.01f < mag) {
          const float inv = 1.0f / mag;
          const float d[3] = {s->m_vel[0] * inv, s->m_vel[1] * inv, s->m_vel[2] * inv};
          if (win) {
            const float* v0 = win->m_lookAt.m[0];
            const float* v1 = win->m_lookAt.m[1];
            const float a = v0[0] * d[0] + v0[1] * d[1] + v0[2] * d[2];
            const float b = v1[0] * d[0] + v1[1] * d[1] + v1[2] * d[2];
            s->m_rot = -std::atan2(a, b);
            s->m_spin = (m_init->m_flags & 2) ? -m_init->m_spin : -m_init->m_spin * a;
            s->m_rot += m_init->m_rotation;
          }
        }
      }
      ei = m_init;
      float size = ei->m_size;
      if (!(ei->m_flags & 0x20000)) {
        if (ei->m_flags & 4) size = ei->m_sizeRandom * Effects::TableRand() + size;
      } else {
        size = (m_birth->m_sizeMax - size) * t + size;
      }
      pk->m_size[i] = size;
      if (!(m_init->m_flags & 0x200000)) {
        pk->m_alphaRate[i] = m_birth->m_alphaRate;
      } else {
        const float f = Effects::TableRand();
        pk->m_alphaRate[i] = m_init->m_alphaRateRandom * (1.0f - (f + f)) + m_birth->m_alphaRate;
      }
      ei = m_init;
      const unsigned int cf = ei->m_flags;
      float c[3];
      bool packed = true;
      if (cf & 0x20000) {
        // Lerped over the burst from the colour to the colour range.
        for (int k = 0; k < 3; ++k) c[k] = ((ei->m_colorRange[k] - ei->m_color[k]) * t + ei->m_color[k]) * 16.0f;
      } else if (cf & 0x800000) {
        // The colour range scaled by the emitter's colour, plus the colour.
        const float* scale = &m_colorScale.x;
        for (int k = 0; k < 3; ++k) c[k] = (ei->m_colorRange[k] * scale[k] + ei->m_color[k]) * 16.0f;
      } else if (cf & 0x9000000) {
        // Random within the range (one random for all three with flag 0x8000000: a grey).
        const unsigned int r = Effects::NextRand32();
        float rnd[3];
        rnd[0] = Effects::UnitFromBits(r);
        if (cf & 0x8000000) {
          rnd[1] = rnd[2] = rnd[0];
        } else {
          rnd[1] = Effects::UnitFromBits(Rotl(r, 8));
          rnd[2] = Effects::UnitFromBits(Rotl(r, 16));
        }
        for (int k = 0; k < 3; ++k) c[k] = (rnd[k] * m_init->m_colorRange[k] + m_init->m_color[k]) * 16.0f;
      } else {
        packed = false;
      }
      if (packed)
        colorBits = static_cast<unsigned int>(static_cast<int>(c[2])) << 10 |
                    static_cast<unsigned int>(static_cast<int>(c[1])) << 5 | static_cast<unsigned int>(static_cast<int>(c[0]));
      pk->m_color[i] = static_cast<unsigned short>(flip | colorBits);
    }
    t += step;
    if (++slot == m_maxParticles) slot = 0;
  }
  m_count = static_cast<unsigned short>(m_count + done);
  m_bursts += done;
  return done;
}

// 80354838
// (For an emitter made from copied data: its texture, by id or MISSINGSHADER.)
int Effects__FastParticleEmitter::InitResourceData() {
  if (m_texture) {
    m_texture->DelRef();
    m_texture = nullptr;
  }
  if (m_init->m_textureId == 0) m_init->m_textureId = EResourceManager::CalcId(const_cast<char*>("MISSINGSHADER"));
  _textureman.AddRef(m_init->m_textureId, nullptr, 0);
  m_texture = static_cast<ERTexture*>(_textureman.GetRef(m_init->m_textureId));
  return 1;
}

// 803548D4
// A rotating emitter (the description's flags2 sign bit) turns about z and y at its speeds (tilted
// about x); otherwise its rotation is the identity.
void Effects__FastParticleEmitter::RotateEmitter(float dt) {
  InitData* ei = m_init;
  if (static_cast<int>(ei->m_flags2) < 0) {
    m_rotZ = ei->m_rotZSpeed * dt + m_rotZ;
    m_rotY = ei->m_rotYSpeed * dt + m_rotY;
    Effects::Mat4SetEulerAngles34(-ei->m_tiltX, m_rotY, m_rotZ, m_local);
  } else {
    float* p = M(m_local);
    p[8] = 0.0f, p[3] = 0.0f, p[2] = 0.0f, p[1] = 0.0f, p[7] = 0.0f, p[6] = 0.0f, p[4] = 0.0f, p[11] = 0.0f, p[9] = 0.0f;
    p[5] = 1.0f, p[10] = 1.0f, p[0] = 1.0f;
  }
}

// 80354978
// 278 packets of 16 particles for all the emitters.
void Effects__FastParticleEmitter::InitParticlePool() {
  Effects::gsNumberFastParticlePacketsInPool = 0x116;
  gFastParticlePoolBuffer = static_cast<unsigned char*>(MainHeap()->MallocAligned(0x31f40, 0x40, 0, 0));
  Effects::gFastParticlePacketsPool = new FastAllocPool(const_cast<char*>("Fast Particle Packets"), gFastParticlePoolBuffer,
                                                        sizeof(Packet), Effects::gsNumberFastParticlePacketsInPool);
}
static_assert(sizeof(Effects__FastParticlePacket) == 0x2e0, "packet layout");

// 803549E0
int Effects__FastParticleEmitter::GetTotalPacketsInPool() { return 0; }

// 803549E8
// The world matrix: the local rotation (with its offset when kKeepLocalOffset) on the parent.
void Effects__FastParticleEmitter::ComputeCombinedMatrix() {
  EMat4 local = m_local;
  local.m[3][3] = 1.0f;
  if (!(m_flags & kKeepLocalOffset)) local.m[3][0] = local.m[3][1] = local.m[3][2] = 0.0f;
  m_world.Mult4x4(local, m_parent);
}

// 80354A5C
// One frame: the duration (then Stop), the birth radius, the particles' ages (the oldest die after
// their life, all of them after the kill time), the emitter's rotation, the due bursts (at most two
// a frame), and whether it is finished (no particles and no more bursts).
void Effects__FastParticleEmitter::Update(float dt) {
  if (m_flags & 0x10) m_state |= kSuspended;
  m_time += dt;
  const float dur = m_init->m_duration;
  if (0.0f < dur && !(m_state & kNoBurst) && !(m_state & kSuspended) && dur <= m_time) {
    m_state |= kSuspended;
    m_time = m_init->m_duration;
    Stop();
  }
  bool killAll = false;
  const float kill = m_init->m_killTime;
  if (0.0f < kill && kill <= m_time) killAll = true;
  if (m_radiusSpeed != 0.0f) {
    InitData* ei = m_init;
    if (1.1754943508222875e-38 < static_cast<double>(ei->m_radiusSpeedTime)) {
      const float v = m_radiusSpeed - (dt / ei->m_radiusSpeedTime) * ei->m_radiusSpeed;
      m_radiusSpeed = v;
      if (ei->m_radiusSpeed <= 0.0f) {
        if (0.0f <= v) m_radiusSpeed = 0.0f;
      } else if (v <= 0.0f) {
        m_radiusSpeed = 0.0f;
      }
    }
    m_radius = m_radiusSpeed * dt + m_radius;
  }
  if (m_init->m_flags & 0x800000) {
    if ((m_state & 0x10020) == 0x10020) {
      // GC: measures the position itself, not its change since kMoved was set.
      const float lim = m_fieldEC * 0.1f;
      const float sq = m_world.m[3][0] * m_world.m[3][0] + m_world.m[3][1] * m_world.m[3][1] + m_world.m[3][2] * m_world.m[3][2];
      if (lim * lim < sq) m_state &= ~kMoved;
    }
    if (!(m_state & kMoved)) {
      m_fieldE0 = EVec3{m_world.m[3][0], m_world.m[3][1], m_world.m[3][2]};
      m_colorScale = EVec4(1.0f, 1.0f, 1.0f, 1.0f);
      m_state |= kMoved;
    }
  }
  unsigned short count = m_count;
  unsigned short head = m_head;
  int dead = 0;
  unsigned int slot = head;
  for (unsigned int left = count; left != 0; --left) {
    Packet* pk = PacketOf(static_cast<int>(slot));
    const int i = static_cast<int>(slot) & 15;
    float life = m_init->m_life;
    const float age = pk->m_age[i] + dt;
    if (m_init->m_flags2 & 2) life = DieOnLastFrame_Lifetime(pk, static_cast<unsigned int>(i));
    if ((0.0f <= life && life < age) || killAll) {
      pk->m_age[i] = life;
      ++dead;
    } else {
      pk->m_age[i] = age;
    }
    if (++slot == static_cast<unsigned int>(m_maxParticles)) slot = 0;
  }
  if (dead) {
    int h = head + dead;
    if (m_maxParticles <= h) h -= m_maxParticles;
    head = static_cast<unsigned short>(h);
    count = static_cast<unsigned short>(count - dead);
    m_deaths += dead;
  }
  m_head = head;
  m_count = count;
  if (static_cast<int>(m_init->m_flags2) < 0) RotateEmitter(dt);
  if ((m_flags & kKeepParentRotation) || (m_init->m_flags & 0x40000000)) ComputeCombinedMatrix();
  m_burstTimer -= dt;
  if (dt != 0.0f && !(m_flags & kStopped)) {
    int tries = 0.06666667014360428 < dt ? 2 : 1;
    for (;;) {
      const bool none = tries < 1;
      --tries;
      if (none || 0.0f < m_burstTimer) break;
      int added;
      if (!(0.0f < UpdateBurst(m_count, 0.0f, &added))) break;
    }
  }
  bool finished = false;
  if (m_count == 0 && ((m_state & kBursting) || (m_flags & kStopped))) {
    const int maxBursts = m_init->m_maxBursts;
    if (maxBursts < 0) {
      if (!(m_flags & kInfinite)) finished = true;
    } else {
      finished = maxBursts <= m_bursts;
    }
  }
  if (finished) m_state |= kEmpty;
  else m_state &= ~kEmpty;
  if ((m_state & (kBursting | kNoBurst)) == (kBursting | kNoBurst)) m_state |= kSuspended;
  if (m_state & kEmpty) {
    const float d = m_init->m_duration;
    if (d <= 0.0f ? (m_state & kBursting) != 0 : !(m_time < d)) m_flags |= kDone;
  }
  m_prevPos = EVec4(m_world.m[3][0], m_world.m[3][1], m_world.m[3][2], m_world.m[3][3]);
  m_lastDt = dt;
  if (m_init->m_flags & 0x40000000) m_world.Id();
  if (finished) {
    const unsigned int f = m_flags;
    m_state |= 0x8000;
    m_flags = f | kDone;
    if (f & kReleased) m_flags = f | kDelete | kDone;
  }
}

// 80354FE0
// No more bursts; done at once unless its particles live a while (0 < life <= 60).
void Effects__FastParticleEmitter::Stop() {
  m_state |= kSuspended | kNoBurst;
  m_flags = (m_flags & ~kInfinite) | kStopped;
  const int maxBursts = m_init->m_maxBursts;
  if (-1 < maxBursts) m_bursts = maxBursts;
  if (0.0f < m_init->m_life && m_init->m_life <= 60.0f) return;
  m_flags |= kDone;
}

// 8035504C
// A new particle's position and velocity: speed (random with flag 8) and birth radius (random with
// 0x20, square-root distributed with 0x10, along the burst with 0x20000) in a direction at an azimuth
// (random over the range, or along the burst with flags2 0x2000) and an elevation; flag 0x200 adds a
// box (or with 0x400 a sphere/disc) spread; flag 0x40000000 takes it into world space (particles
// stay behind as the emitter moves), spread along last frame's movement and with flags2 0x40000000
// inheriting the emitter's velocity.
void Effects__FastParticleEmitter::ComputeDispersionPositionAndVelocity(State* s, float t, float dt, float) {
  InitData* ei = m_init;
  float* p = s->m_pos;
  float* v = s->m_vel;
  const unsigned int r = Effects::NextRand32();
  float speed = (ei->m_flags & 8) ? ei->m_speedRandom * Effects::UnitFromBits(r) + ei->m_speed : ei->m_speed;
  unsigned int flags = m_init->m_flags;
  float radius;
  if (!(flags & 0x20)) {
    radius = (flags & 0x20000) ? m_init->m_radiusRandom * t + m_radius : m_radius;
  } else {
    float f = Effects::UnitFromBits((r & 0x7f) << 16 | r >> 16);
    if (flags & 0x10) f = std::sqrt(f);
    radius = m_init->m_radiusRandom * f + m_radius;
  }
  ei = m_init;
  const unsigned int r2 = Effects::NextRand32();
  float azimuth = (ei->m_flags2 & 0x2000) ? ei->m_azimuthRange * t : Effects::UnitFromBits(r2) * ei->m_azimuthRange;
  float elevation;
  if (!(ei->m_flags & 0x40)) {
    elevation = (ei->m_flags2 & 0x4000) ? ei->m_elevationRandom * t + ei->m_elevation : ei->m_elevation;
  } else {
    elevation = ei->m_elevationRandom * Effects::UnitFromBits((r2 & 0x7f) << 16 | r2 >> 16) + ei->m_elevation;
  }
  if (ei->m_flags2 & 0x8000) {
    // Evenly spread over the sphere zone between the elevation and elevation + range.
    const float range = ei->m_azimuthRange;
    const float sTop = std::sin(ei->m_elevation + ei->m_elevationRandom);
    const float dz = sTop - std::sin(ei->m_elevation);
    const float area = std::fabs(dt * range * dz);
    if (area <= 1e-12f) {
      azimuth = 0.0f;
      elevation = 0.0f;
    } else {
      const float inv = 1.0f / std::sqrt(area + 1.1754944e-38f);
      const float a = t * dz * inv;
      const float b = t * range * inv;
      azimuth = (a - static_cast<float>(static_cast<int>(a))) * range;
      float z = (b - static_cast<float>(static_cast<int>(b))) * dz + std::sin(m_init->m_elevation);
      if (z < -1.0f) z = -1.0f;
      else if (1.0f < z) z = 1.0f;
      elevation = std::asin(z);
    }
  }
  float sa, ca, se, ce;
  Effects::SinCosf(azimuth, sa, ca);
  Effects::SinCosf(elevation, se, ce);
  const float rc = radius * ce, sc = speed * ce;
  v[0] = sc * ca;
  p[2] = radius * se;
  p[1] = -rc * sa;
  p[0] = rc * ca;
  v[1] = -sc * sa;
  v[2] = speed * se;
  ei = m_init;
  if (ei->m_flags & 0x200) {
    float dx, dy, dz;
    if (!(ei->m_flags & 0x400)) {
      // A box: three randoms from one word, a random corner.
      const unsigned int i0 = Effects::gPrecomputedTableIndex & (Effects::kPreComputedTableSize - 1);
      const unsigned int w = Effects::gPrecomputedRand32Table[i0];
      const unsigned int top = (w << 8) >> 24;
      float a = Effects::UnitFromBits(w), b = Effects::UnitFromBits(Rotl(w, 8)), c = Effects::UnitFromBits(Rotl(w, 16));
      if ((ei->m_flags2 & 0x10000) && ei->m_box[2] != 0.0f && ei->m_radiusRandom < ei->m_box[2]) {
        // A hollow box: one coordinate pushed out of the inner part.
        const float k = ei->m_radiusRandom / ei->m_box[2];
        if (a < k && b < k && c < k) {
          if (!(top & 1)) {
            if (!(top & 2)) c = (1.0f - k) * c + k;
            else a = (1.0f - k) * a + k;
          } else {
            b = (1.0f - k) * b + k;
          }
        }
      }
      const unsigned int i1 = (i0 + 1) & (Effects::kPreComputedTableSize - 1);
      const unsigned int w2 = Effects::gPrecomputedRand32Table[i1];
      Effects::gPrecomputedTableIndex = i1 + 1;
      dx = a * static_cast<float>(static_cast<int>(w2 & 2) - 1);
      dy = b * static_cast<float>(static_cast<int>((w2 >> 24) & 2) - 1);
      dz = c * static_cast<float>(static_cast<int>(((w2 << 8) >> 24) & 2) - 1);
      if (m_init->m_flags & 0x20000) dy = (t + t) - 1.0f;
      // GC order: x from the first, y from the third (negated), z from the second.
      const float bx = dx, by = dz, bz = dy;
      dx = bx, dy = by, dz = bz;
    } else {
      // A sphere (or with box z 0 a disc), on its surface or (flag 0x800) inside it.
      const unsigned int w = Effects::NextRand32();
      float s1, c1, s2, c2;
      Effects::SinCosf(Effects::UnitFromBits(w) * 6.2831855f, s1, c1);
      Effects::SinCosf(Effects::UnitFromBits(Rotl(w, 16)) * 6.2831855f, s2, c2);
      ei = m_init;
      if (!(ei->m_flags & 0x800)) {
        if (ei->m_box[2] == 0.0f) {
          dx = c1, dy = s1, dz = 0.0f;
        } else {
          dy = s2 * s1, dz = c2, dx = s2 * c1;
        }
      } else {
        float d = Effects::TableRand();
        if ((ei->m_flags2 & 0x10000) && ei->m_box[2] != 0.0f && ei->m_radiusRandom < ei->m_box[2]) {
          const float k = ei->m_radiusRandom / ei->m_box[2];
          if (d < k) d = (1.0f - k) * d + k;
        }
        dy = d * s2 * s1;
        dx = d * s2 * c1;
        dz = d * c2;
      }
      if (!(m_init->m_flags & 0x40)) {
        v[0] = speed * dx;
        v[1] = -speed * dy;
        v[2] = speed * dz;
      }
    }
    p[0] = m_init->m_box[0] * dx + p[0];
    p[1] = m_init->m_box[1] * -dy + p[1];
    p[2] = m_init->m_box[2] * dz + p[2];
  }
  if (m_init->m_flags & 0x40000000) {
    // GC: transformed in place (each coordinate sees the ones already done).
    const float* w = Effects::gpCurrentMatrix1Ptr;
    for (int k = 0; k < 3; ++k) p[k] = p[2] * w[8 + k] + p[0] * w[k] + p[1] * w[4 + k];
    p[0] += w[12];
    p[1] += w[13];
    p[2] += w[14];
    for (int k = 0; k < 3; ++k) v[k] = v[2] * w[8 + k] + v[0] * w[k] + v[1] * w[4 + k];
  }
  if (1e-05f < m_lastDt) {
    if (m_init->m_flags & 0x40000000) {
      const float f = -Effects::TableRand();
      p[1] = f * (m_world.m[3][1] - m_prevPos.y) + p[1];
      p[2] = f * (m_world.m[3][2] - m_prevPos.z) + p[2];
      p[0] = f * (m_world.m[3][0] - m_prevPos.x) + p[0];
    }
    if (m_init->m_flags2 & 0x40000000) {
      const float k = m_init->m_inheritVelocity / m_lastDt;
      v[0] = k * (m_world.m[3][0] - m_prevPos.x) + v[0];
      v[1] = k * (m_world.m[3][1] - m_prevPos.y) + v[1];
      v[2] = k * (m_world.m[3][2] - m_prevPos.z) + v[2];
    }
  }
}

// 80355A38
// Flags2 bit 2: a particle lives to the end of its last sheet frame (offset by its seed with flag
// 0x20000000).
float Effects__FastParticleEmitter::DieOnLastFrame_Lifetime(Packet* pk, unsigned int i) {
  InitData* ei = m_init;
  float life = ei->m_frameTime * (static_cast<float>(static_cast<int>(ei->m_life * ei->m_framesPerSecond)) + 1.0f);
  if (ei->m_flags & 0x20000000) {
    const float seed = static_cast<float>(BitsFromFloat(pk->m_state[i].m_pos[0]) & 0xfff) * 0.00024420026f;
    life = life - (seed - ei->m_frameTime * static_cast<float>(static_cast<int>(seed * ei->m_framesPerSecond)));
  }
  if (life <= 0.0f) life = 0.0f;
  return life;
}

// 80355B34
void Effects__FastParticleEmitter::Render(ERC* rc) {
  if (m_init->m_flags2 & 0x18) {
    EMat4 id;
    id.Id();
    Effects::Mat4Multiply34(m_local, id, m_local);
    ComputeCombinedMatrix();
  }
  DoSimulationAndRender(rc);
}

// 80355B9C
// Every live particle from its age: size (growing to the maximum), colour and alpha (rates from
// birth, clamped, the alpha also limited by the fade line and the effect's alpha), skipped when too
// small or faint; position (drifting from its birth velocity towards the drift velocity, with drag);
// rotation (spin slowing over the spin time); the sheet frame (u and v). Drawn with the identity
// model matrix and the emitter's shader.
void Effects__FastParticleEmitter::DoSimulationAndRender(ERC* rc) {
  unsigned int left = m_count;
  unsigned int slot = m_head;
  s_renderBufferIndex = 0;
  if (!(0.005f < m_alphaScale)) return;
  BirthData* bd = m_birth;
  rc->ModelMatrixId();
  m_shader->Select(rc, 0);
  Quad quad;
  for (int k = 0; k < 4; ++k) quad.m_corners[k][3] = 1.0f;
  int drawn = 0;
  for (; left != 0; --left, ++slot) {
    if (static_cast<unsigned int>(m_maxParticles) <= slot) slot = 0;
    Packet* pk = PacketOf(static_cast<int>(slot));
    const int i = static_cast<int>(slot) & 15;
    const float age = pk->m_age[i];
    State* s = &pk->m_state[i];
    float size = bd->m_sizeMax;
    const float grown = bd->m_sizeRate * age + pk->m_size[i];
    if (grown < bd->m_sizeMax) size = grown;
    if (!(Effects::s_minSize < size)) continue;
    EVec4 color(bd->m_colorRate[0] * age + bd->m_color0[0], bd->m_colorRate[1] * age + bd->m_color0[1],
                bd->m_colorRate[2] * age + bd->m_color0[2], pk->m_alphaRate[i] * age + bd->m_color0[3]);
    const float fade = age * bd->m_fadeRate + bd->m_fadeBase;
    if (fade < color.w) color.w = fade;
    unsigned int flipU = 0;
    if (m_init->m_flags & 0x9820000) {
      const unsigned short c = pk->m_color[i];
      color.x *= static_cast<float>(c & 0x1f) * 0.0625f;
      color.y *= static_cast<float>((c >> 5) & 0x1f) * 0.0625f;
      color.z *= static_cast<float>((c >> 10) & 0x1f) * 0.0625f;
      flipU = c >> 15;
    }
    for (int k = 0; k < 4; ++k) {
      float& ch = color[k];
      if (ch < bd->m_colorMin[k]) ch = bd->m_colorMin[k];
      else if (bd->m_colorMax[k] < ch) ch = bd->m_colorMax[k];
    }
    if (m_alphaScale < 0.99f) color.w *= m_alphaScale;
    if (color.w + color.w < Effects::s_minAlpha) continue;
    // Position: drift + (velocity - drift) * scale * (1 - 1 / (1 + drag * age)^4).
    float k4 = bd->m_drag * age + 1.0f;
    k4 = k4 * k4;
    const float ease = 1.0f - 1.0f / (k4 * k4);
    const float vs = bd->m_velocityScale;
    EVec4 pos;
    pos.x = age * bd->m_accel[0] + s->m_pos[0] + (s->m_vel[0] - bd->m_accel[0]) * vs * ease;
    pos.y = age * bd->m_accel[1] + s->m_pos[1] + (s->m_vel[1] - bd->m_accel[1]) * vs * ease;
    pos.z = age * bd->m_accel[2] + s->m_pos[2] + (s->m_vel[2] - bd->m_accel[2]) * vs * ease;
    pos.w = 0.0f;
    float spinAge = age;
    if (bd->m_spinTime <= age) spinAge = bd->m_spinTime;
    float frameOffset = 0.0f;
    if (m_init->m_flags & 0x20000000)
      frameOffset = FloatFromBits((BitsFromFloat(s->m_pos[0]) & 0x7fffff) | 0x3f800000) * bd->m_frameRandom;
    if (bd->m_framesU <= 1.0f) {
      quad.m_u1 = 1.0f;
      quad.m_u0 = 0.0f;
    } else {
      const float f = (age + frameOffset) * bd->m_frameRateU;
      const unsigned int frame = static_cast<unsigned int>(static_cast<int>((f - static_cast<float>(static_cast<int>(f))) * bd->m_framesU));
      quad.m_u0 = static_cast<float>(frame) * bd->m_frameU;
      quad.m_u1 = static_cast<float>(frame + 1) * bd->m_frameU;
    }
    if (bd->m_framesV <= 1.0f) {
      quad.m_v1 = 1.0f;
      quad.m_v0 = 0.0f;
    } else {
      const float f = (age + frameOffset) * bd->m_frameRateV;
      const unsigned int frame = static_cast<unsigned int>(static_cast<int>((f - static_cast<float>(static_cast<int>(f))) * bd->m_framesV));
      quad.m_v0 = static_cast<float>(frame) * bd->m_frameV;
      quad.m_v1 = static_cast<float>(frame + 1) * bd->m_frameV;
    }
    if (flipU) {
      quad.m_u0 = -quad.m_u0;
      quad.m_u1 = -quad.m_u1;
    }
    quad.m_index = drawn++;
    const float angle = s->m_spin * (1.0f - bd->m_spinDamp * spinAge) * spinAge + (3.1415927f - s->m_rot);
    RenderQuad(rc, quad, angle, color, size, pos, *bd);
  }
  // GC: drew the last (or a stale) batch even when no quad was made this frame.
  if (drawn && s_renderBufferIndex < 0xf9)
    rc->QuadList(s_renderBufferIndex + 4, &s_vertArray[0][0], &s_texCoordArray[0][0], &s_colorArray[0][0], nullptr, false);
}

// 803562D0
int Effects__FastParticleEmitter::Accept(Effects__EffectsVisitor& v) { return v.Visit(*this); }

// 8035630C
// The most particles alive at once over ten trial runs (rounded up to whole packets, at most the
// description's count); -1 if it never bursts.
int Effects__FastParticleEmitter::ComputeMaxParticles() {
  unsigned int best = (m_count + 0xf) & 0x1fff0;
  for (int run = 0;; ) {
    m_head = 0;
    bool interval = true;
    m_count = 0;
    int added = 0;
    ComputeCombinedMatrix();
    int bursts;
    float birth = 0.0f;
    if (0.0f < m_burstInterval) {
      InitData* ei = m_init;
      if (0.0f < ei->m_life) {
        bursts = 1000;
        birth = ei->m_life;
        interval = false;
      } else if (ei->m_maxBursts < 1) {
        bursts = 0;
      } else {
        const int n = static_cast<int>(m_rateScale * (Effects::TableRand() * static_cast<float>(ei->m_burstRange) +
                                                      static_cast<float>(ei->m_burstMin)));
        bursts = (ei->m_maxBursts + n - 1) / n;
      }
    } else {
      bursts = 1;
    }
    if (0 < bursts) {
      while (m_init->m_life <= 0.0f || 0.001f < birth) {
        const float r = UpdateBurst(m_count, birth, &added);
        m_burstTimer = 0.0f;
        if (r <= 0.0f) break;
        if (!interval) birth -= r;
        if (--bursts < 1) break;
      }
    }
    m_lastDt = 0.0f;
    m_burstTimer = 0.0f;
    if (bursts == 0) return -1;
    unsigned int n = (m_count + 0xf) & 0x1fff0;
    if (n < best) n = best;
    ++run;
    best = n;
    if (9 < run) {
      const unsigned int cap = static_cast<unsigned int>(m_init->m_maxParticles);
      if (((cap + 0xf) & 0xfffffff0) < n) n = cap;
      return static_cast<int>(n);
    }
  }
}

// 80356654
float Effects__FastParticleEmitter::GetAlphaScale() { return m_alphaScale; }
// 8035665C
void Effects__FastParticleEmitter::SetAlphaScale(float a) { m_alphaScale = a; }

// 80356664
void Effects__FastParticleEmitter::SuspendBurst() {
  m_state |= kSuspended;
  m_time = m_init->m_duration;
}

// 80356680
// Starts as if it had been running for a particle's life (bursts back to back over that time).
bool Effects__FastParticleEmitter::PreRollParticles() {
  m_count = 0;
  int added = 0;
  bool interval = true;
  m_head = 0;
  ComputeCombinedMatrix();
  int bursts;
  float birth = 0.0f;
  if (0.0f < m_burstInterval) {
    InitData* ei = m_init;
    if (0.0f < ei->m_life) {
      bursts = 1000;
      birth = ei->m_life;
      interval = false;
    } else if (ei->m_maxBursts < 1) {
      bursts = 0;
    } else {
      const int n = static_cast<int>(m_rateScale * (Effects::TableRand() * static_cast<float>(ei->m_burstRange) +
                                                    static_cast<float>(ei->m_burstMin)));
      bursts = (ei->m_maxBursts + n - 1) / n;
    }
  } else {
    bursts = 1;
  }
  if (0 < bursts) {
    while (m_init->m_life <= 0.0f || 0.001f < birth) {
      const float r = UpdateBurst(m_count, birth, &added);
      m_burstTimer = 0.0f;
      if (r <= 0.0f) break;
      if (!interval) birth -= r;
      if (--bursts < 1) break;
    }
  }
  m_burstTimer = 0.0f;
  m_lastDt = 0.0f;
  return bursts != 0;
}

// 80356870
// `count` packets from the pool (as many as there are); with flag 0x1000 every slot starts free.
int Effects__FastParticleEmitter::AllocateParticlePackets(int count) {
  FastAllocPool* pool = Effects::gFastParticlePacketsPool;
  int got = count;
  for (int i = 0; i < count; ++i) {
    m_packets[i] = static_cast<Packet*>(pool->Alloc());
    if (!m_packets[i]) {
      got = i;
      break;
    }
  }
  if (got != 0) {
    if (m_init->m_flags & 0x1000)
      for (int i = 0; i < got; ++i)
        for (State& s : m_packets[i]->m_state) s.m_rot = 1024.0f;
    m_maxParticles = static_cast<short>(got << 4);
    m_packetCount = static_cast<short>(got);
    return 1;
  }
  m_packetCount = 0;
  m_maxParticles = 0;
  return 0;
}

// 80356954
// One quad into the batch (slot = its number mod 64): colour (alpha doubled to 0..255); the rotation
// as a 1024-step angle with a parabolic sine/cosine of the 45-degree-turned axes; the centre through
// the emitter's world matrix, moved by the scaled offset; the corners along the view's right and up
// axes (the current 3D window's view matrix). A full batch (64 quads) goes out at once.
void Effects__FastParticleEmitter::RenderQuad(ERC* rc, Quad& q, float angle, EVec4& color, float size, EVec4& pos, BirthData& bd) {
  s_renderBufferIndex = (q.m_index & 0x3f) << 2;
  color.w = color.w + color.w;
  const int base = s_renderBufferIndex;
  const int steps = static_cast<int>(angle * 0.15915493667125702f * 1024.0f);
  const unsigned int quadrant = static_cast<unsigned int>(steps) & 0x300;
  const unsigned char rgba[4] = {static_cast<unsigned char>(static_cast<int>(color.x)), static_cast<unsigned char>(static_cast<int>(color.y)),
                                 static_cast<unsigned char>(static_cast<int>(color.z)), static_cast<unsigned char>(static_cast<int>(color.w))};
  for (int k = 0; k < 4; ++k) std::memcpy(s_colorArray[base + k], rgba, 4);
  float f = static_cast<float>(static_cast<int>(static_cast<unsigned int>(steps) & 0xff)) * 0.00390625f;
  const float half = size * 0.99999994f * 0.5f;
  float a, b;  // f7, f8
  if (quadrant == 0) {
    f = f - 0.5f;
    const float g = f * -0.8502703f * f + 0.71256757f;
    a = g - f;
    b = g + f;
  } else if (quadrant == 0x100) {
    f = 0.5f - f;
    const float g = f * -0.8502703f * f + 0.71256757f;
    a = f - g;
    b = f + g;
  } else if (quadrant == 0x200) {
    f = f - 0.5f;
    const float g = f * 0.8502703f * f - 0.71256757f;
    a = g + f;
    b = g - f;
  } else {
    f = f - 0.5f;
    const float g = f * -0.8502703f * f + 0.71256757f;
    a = f + g;
    b = f - g;
  }
  const float sv = bd.m_scaleV - 1.0f, su = bd.m_scaleU - 1.0f;
  const float d31 = half * (a - b);              // f31
  const float d19 = (sv - su) * 0.5f;            // f12
  const float d30 = half * (b + a);              // f30
  const float d20 = (su + sv) * 0.5f;            // f13
  const float d28 = d20 * d30 + d19 * d31;       // f28
  const float d29 = d20 * d31 - d19 * d30;       // f29
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (!win) return;
  const EMat4& w = m_world;
  const float px = pos.x, py = pos.y, pz = pos.z;
  pos.x = pz * w.m[2][0] + px * w.m[0][0] + py * w.m[1][0] + w.m[3][0];
  pos.y = pz * w.m[2][1] + px * w.m[0][1] + py * w.m[1][1] + w.m[3][1];
  pos.z = pz * w.m[2][2] + px * w.m[0][2] + py * w.m[1][2] + w.m[3][2];
  EMat4 t;
  t.Transpose(win->m_lookAt);
  const float* right = t.m[0];
  const float* up = t.m[1];
  float B[3], C[3], D[3];
  for (int k = 0; k < 3; ++k) {
    B[k] = d29 * right[k] + d28 * up[k];
    C[k] = d31 * right[k] + d30 * up[k];
    D[k] = -d30 * right[k] + d31 * up[k];
  }
  pos.x += B[0], pos.y += B[1], pos.z += B[2];
  const float P[3] = {pos.x, pos.y, pos.z};
  for (int k = 0; k < 3; ++k) {
    s_vertArray[base][k] = P[k] + C[k];
    s_vertArray[base + 1][k] = P[k] - D[k];
    s_vertArray[base + 2][k] = P[k] - C[k];
    s_vertArray[base + 3][k] = P[k] + D[k];
  }
  s_texCoordArray[base][0] = q.m_u0, s_texCoordArray[base][1] = q.m_v1;
  s_texCoordArray[base + 1][0] = q.m_u0, s_texCoordArray[base + 1][1] = q.m_v0;
  s_texCoordArray[base + 2][0] = q.m_u1, s_texCoordArray[base + 2][1] = q.m_v0;
  s_texCoordArray[base + 3][0] = q.m_u1, s_texCoordArray[base + 3][1] = q.m_v1;
  if (base == 0xfc) rc->QuadList(0x100, &s_vertArray[0][0], &s_texCoordArray[0][0], &s_colorArray[0][0], nullptr, false);
}

// 80356584 __static_initialization_and_destruction_0 (Effects::maxClamp = 255 x4: unused)
// 80356F84 global constructors keyed to Effects::gFastParticlePacketsPool
