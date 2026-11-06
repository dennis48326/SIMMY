// Port of engine/psystem.cpp (the particle systems: emitters, particles, birth shapes, the per-frame
// processing), the random-number part of engine/effectsfunctions.cpp, engine/rparticle.cpp and
// engine/particleman.cpp ('PART' resources), engine/reffects_attachment.cpp and
// engine/effects_attachmentman.cpp ('EATC' resources). See psystem.hpp.
//
// PC: the GC's pools (PoolCreate: 5 systems, 0x80 emitters, 0x400 particles) and LL lists become a
// fixed particle pool with a free list, counted emitters and vectors; the limits are the GC's.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

psystem* g_pParticleSystem = nullptr;            // 804B08F8 "globalpsys"
psystem* g_pParticleWeatherSystem = nullptr;     // "weatherpsys"
psystem* g_pParticleModelSystem = nullptr;       // "modelspsys"
psystem* g_pParticleTransitionSystem = nullptr;  // "transpsys"

unsigned int Effects::gPrecomputedRand32Table[0x80];  // 804DCA00
unsigned int Effects::gPrecomputedTableIndex = 0;     // 804FF400
namespace {
int gRandTable2[0x14];     // the lagged-Fibonacci state (SetSeed2)
int gRandIndexk2 = 0x14;
unsigned int gRand = 0;
}  // namespace

// 803520DC
// 128 numbers from the additive generator (each the sum of the entries 3 apart, written back).
void Effects::PreComputeRand32Table() {
  for (unsigned int& out : gPrecomputedRand32Table) {
    int a = gRandIndexk2 + 2;
    int b = gRandIndexk2 - 1;
    if (b < 0) b = 0x13;
    else if (0x13 < a) a = gRandIndexk2 - 0x12;
    const int sum = gRandTable2[b] + gRandTable2[a];
    gRandIndexk2 = b;
    out = static_cast<unsigned int>(sum);
    gRandTable2[b] = sum;
  }
  gPrecomputedTableIndex = 0;
}

// 8035214C
// Park-Miller (16807) minimal standard generator.
int Effects::PMRand() {
  gRand = (gRand & 0xffff) * 0x41a7 + (((gRand >> 16) * 0x41a70000u) & 0x7fff0000u) + (((gRand >> 16) * 0x41a7) >> 15);
  if (static_cast<int>(gRand) < 0) gRand += 0x80000001u;  // - 0x7FFFFFFF
  return static_cast<int>(gRand);
}

// 80352190
void Effects::SetSeed2(int seed) {
  gRandIndexk2 = 0x14;
  gRand = static_cast<unsigned int>(seed);
  for (int& v : gRandTable2) v = PMRand();
}

namespace {
float TableRand() { return Effects::TableRand(); }

const int kMaxEmitters = 0x80;    // ParticleSysOpen(5, 0x80, 0x400)
const int kMaxParticles = 0x400;
std::vector<particle> s_particles;       // g_pParticlePool
std::vector<particle*> s_freeParticles;
int s_activeParticles = 0;               // the pool's count (+8)
int s_emitterCount = 0;                  // g_pPEmitterPool
int s_systemCount = 0;                   // g_pPSystemPool

float g_PSystemGravity[4] = {};          // 804B090C: x, y, z, 1
float g_TurbulanceTable[64][4];          // 804B091C: a unit direction and a 0..100 weight

// PC: the GC's newlib rand (31 bits), so the turbulence table spans the same range.
unsigned int s_randState = 1;
int SnRand() {
  s_randState = s_randState * 0x41c64e6du + 0x3039u;
  return static_cast<int>(s_randState & 0x7fffffffu);
}

particle* ParticlePoolAlloc() {
  if (s_freeParticles.empty()) return nullptr;
  particle* p = s_freeParticles.back();
  s_freeParticles.pop_back();
  ++s_activeParticles;
  p->m_inUse = true;
  return p;
}
void ParticlePoolFree(particle* p) {
  if (!p || !p->m_inUse) return;
  *p = particle{};
  s_freeParticles.push_back(p);
  --s_activeParticles;
}

// LLRemoveNode on an emitter's particle list.
void Unlink(pemitter* e, particle* p) {
  if (p == e->m_head) {
    e->m_head = p->m_next;
    if (!e->m_head) e->m_tail = nullptr;
  } else if (p == e->m_tail) {
    e->m_tail = p->m_prev;
  }
  if (p->m_prev) p->m_prev->m_next = p->m_next;
  if (p->m_next) p->m_next->m_prev = p->m_prev;
  --e->m_count;
  p->m_next = p->m_prev = nullptr;
}

void Normalize3(PVector4& v) {
  const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  if (l != 0.0f) {
    const float s = 1.0f / l;
    v.x *= s, v.z *= s, v.y *= s;
  }
}
}  // namespace

// PC: the particle system's rand state, for the determinism harness's dumps (pc_det.cpp).
unsigned int simmy::psystem_rand_state() { return s_randState; }

// 80300B38
// The particle library: the pools, the four systems, gravity up the z axis.
int PSysLibOpen() {
  // GC: LLSysOpen(0x85, 0x85) (the list library's node pools).
  if (ParticleSysOpen(5, kMaxEmitters, kMaxParticles) < 0) return -1;
  PSystemCreate(&g_pParticleSystem, "globalpsys");
  PSystemCreate(&g_pParticleWeatherSystem, "weatherpsys");
  PSystemCreate(&g_pParticleModelSystem, "modelspsys");
  PSystemCreate(&g_pParticleTransitionSystem, "transpsys");
  const float up[3] = {0.0f, 0.0f, 1.0f};
  PSystemSetWorldGravityDir(up);
  return 0;
}

// 80300BF8
int PSysLibClose() {
  PSystemDestroy(&g_pParticleSystem);
  PSystemDestroy(&g_pParticleWeatherSystem);
  PSystemDestroy(&g_pParticleModelSystem);
  PSystemDestroy(&g_pParticleTransitionSystem);
  ParticleSysClose();
  return 0;
}

// 80303AC8
// Birth type 0: a random point of the emitter's plane (width x depth about its centre), heading up
// (+y) spread by up to the spread angle (flags 0x100 / 0x200 keep x / z straight).
void ParticleBirthPlane(pemitter* e, PVector4* pos, PVector4* dir) {
  const pemitterinfo* info = e->m_info;
  const float a = TableRand(), b = TableRand(), c = TableRand(), d = TableRand();
  pos->x = a * info->m_volume[0] - info->m_volume[0] * 0.5f;
  pos->z = b * info->m_volume[1] - info->m_volume[1] * 0.5f;
  const float angle = c * 6.2831855f;
  const float spread = d * info->m_volume[2];
  dir->x = (info->m_flags & 0x100) ? 0.0f : spread * std::cos(angle);
  dir->z = (info->m_flags & 0x200) ? 0.0f : spread * std::sin(angle);
  dir->y = 10.0f;
  dir->w = 1.0f;
  Normalize3(*dir);
}

// 80300C40
// Birth type 1: a point of the ellipsoid (radius x, height), heading outward.
void ParticleBirthSphere(pemitter* e, PVector4* pos, PVector4* dir) {
  const pemitterinfo* info = e->m_info;
  const float phi = TableRand() * 3.1415927f;
  const float theta = TableRand() * 6.2831855f;
  const float st = std::sin(theta), sp = std::sin(phi), ct = std::cos(theta), cp = std::cos(phi);
  const float x = info->m_volume[0] * st * cp;
  const float z = info->m_volume[1] * st * sp;
  const float y = info->m_volume[1] * ct;
  pos->x = x, pos->z = z, pos->y = y;
  *dir = PVector4{x, y, z, 1.0f};
  Normalize3(*dir);
}

// 80300DD4
// Birth type 2: a point of the cone's surface (radius at the base narrowing to the tip at its
// height), heading up spread by the spread angle.
void ParticleBirthCone(pemitter* e, PVector4* pos, PVector4* dir) {
  const pemitterinfo* info = e->m_info;
  const float h = TableRand() * info->m_volume[1];
  const float theta = TableRand() * 6.2831855f;
  pos->y = h;
  const float r = info->m_volume[0] - info->m_volume[0] * h / info->m_volume[1];
  pos->z = r * std::sin(theta);
  pos->x = r * std::cos(theta);
  const float angle = TableRand() * 6.2831855f;
  const float spread = TableRand() * info->m_volume[2];
  dir->x = spread * std::cos(angle);
  dir->z = spread * std::sin(angle);
  dir->w = 1.0f;
  dir->y = 10.0f;
  Normalize3(*dir);
}

// 80300FD4
// Birth type 3: a point of the cylinder's side (radius, height), heading outward from its axis.
void ParticleBirthCylinder(pemitter* e, PVector4* pos, PVector4* dir) {
  const pemitterinfo* info = e->m_info;
  const float h = TableRand() * info->m_volume[1];
  const float theta = TableRand() * 6.2831855f;
  const float x = info->m_volume[0] * std::cos(theta), z = info->m_volume[0] * std::sin(theta);
  pos->y = h, pos->z = z, pos->x = x;
  *dir = PVector4{x, h, z, 1.0f};
  Normalize3(*dir);
}

namespace {
// 8043ED70 g_aDefaultCreateCB: the birth shapes by type.
const ParticleBirthFn g_aDefaultCreateCB[4] = {ParticleBirthPlane, ParticleBirthSphere, ParticleBirthCone,
                                               ParticleBirthCylinder};
}  // namespace

// 8030112C
// A tabbed geometry particle's model: the first whose running total of weights (percent) passes r.
void PCalcTabDataSet(particle* p, pemitterinfo* info, float r) {
  float total = 0.0f;
  for (int i = 0; i < info->m_modelCount; ++i) {
    total += info->m_modelDefs[i].m_weight;
    if (r * 100.0f < total) {
      p->m_last2.w = static_cast<float>(i);
      return;
    }
  }
  p->m_last2.w = 0.0f;
}

// 803011D4
// Up to `count` new particles (at most 0x200 at once, as many as the pool has): one random number
// sets the sprite frame, speed, spin and life; the birth shape the place and heading, both turned
// by the emitter's matrix (and moved to it unless the emitter works in local space, flag 0x2000).
// New particles go to the front of the list.
int ParticleCreate(pemitter* e, int count) {
  if (0x200 < count) count = 0x200;
  particle* made[0x200];
  int n = 0;
  while (n < count) {
    particle* p = ParticlePoolAlloc();
    if (!p) break;
    made[n++] = p;
  }
  const float* m = &e->m_matrix.m[0][0];
  for (int i = 0; i < n; ++i) {
    particle* p = made[i];
    pemitterinfo* info = e->m_info;
    const float r = TableRand();
    if (!(info->m_flags & 0x800)) p->m_last2.w = 0.0f;
    else if (!(info->m_flags & 0x2000000)) p->m_last2.w = static_cast<float>(static_cast<int>(r * static_cast<float>(info->m_frames)));
    else PCalcTabDataSet(p, info, r);
    p->m_pos = PVector4{};
    PVector4 dir;
    const int type = info->m_birthType;
    if (type == 0) {
      const float a = TableRand(), b = TableRand(), c = TableRand(), d = TableRand();
      p->m_pos.x = a * info->m_volume[0] - info->m_volume[0] * 0.5f;
      p->m_pos.z = b * info->m_volume[1] - info->m_volume[1] * 0.5f;
      const float angle = c * 6.2831855f;
      const float s = std::sin(angle), co = std::cos(angle);
      const float spread = d * info->m_volume[2];
      dir.x = (info->m_flags & 0x100) ? 0.0f : spread * co;
      dir.z = (info->m_flags & 0x200) ? 0.0f : spread * s;
      dir.y = 10.0f;
      Normalize3(dir);
    } else if (static_cast<unsigned int>(type - 1) < 3) {
      g_aDefaultCreateCB[type](e, &p->m_pos, &dir);
    } else {
      e->m_birth(e, &p->m_pos, &dir);
    }
    const float speed = r * (info->m_speedMax - info->m_speedMin) + info->m_speedMin;
    const float vx = dir.x * speed, vy = dir.y * speed, vz = dir.z * speed;
    p->m_vel.x = vz * m[8] + vx * m[0] + vy * m[4];
    p->m_vel.y = vz * m[9] + vx * m[1] + vy * m[5];
    p->m_vel.z = vz * m[10] + vx * m[2] + vy * m[6];
    const float spin = r * info->m_spin;
    p->m_last.w = (spin + spin) - info->m_spin;
    float life = r * (info->m_lifeMax - info->m_lifeMin) + info->m_lifeMin;
    if (life <= 0.0f) life = info->m_lifeMax * 0.5f;
    p->m_vel.w = 0.0f;
    p->m_pos.w = life;
    const float x = p->m_pos.z * m[8] + p->m_pos.x * m[0] + p->m_pos.y * m[4];
    const float y = p->m_pos.z * m[9] + p->m_pos.x * m[1] + p->m_pos.y * m[5];
    p->m_pos.z = p->m_pos.z * m[10] + p->m_pos.x * m[2] + p->m_pos.y * m[6];
    p->m_pos.x = x;
    p->m_pos.y = y;
    if (!(e->m_flags & 0x2000)) {
      p->m_pos.x += m[12];
      p->m_pos.y += m[13];
      p->m_pos.z += m[14];
    }
    if (!e->m_head) {
      e->m_head = e->m_tail = p;
    } else {
      p->m_next = e->m_head;
      e->m_head->m_prev = p;
      e->m_head = p;
    }
    ++e->m_count;
    p->m_last2.x = p->m_pos.x, p->m_last2.y = p->m_pos.y, p->m_last2.z = p->m_pos.z;
    p->m_last.x = p->m_pos.x, p->m_last.y = p->m_pos.y, p->m_last.z = p->m_pos.z;
  }
  return 0;
}

namespace {
// The colour (by its two keys) and size (by its key) of a particle `t` of the way through its life,
// handed with its place and rotation to the emitter's draw callback.
void SubmitParticle(pemitter* e, particle* p, float t, PVector4& pos, float rot) {
  const pemitterinfo* info = e->m_info;
  const float* c0;
  const float* c1;
  float f;
  if (info->m_colorKey[0] < t) {
    if (info->m_colorKey[1] < t) {
      f = (t - info->m_colorKey[1]) * info->m_invColorSeg[2];
      c0 = info->m_color[2], c1 = info->m_color[3];
    } else {
      f = (t - info->m_colorKey[0]) * info->m_invColorSeg[1];
      c0 = info->m_color[1], c1 = info->m_color[2];
    }
  } else {
    f = t * info->m_invColorSeg[0];
    c0 = info->m_color[0], c1 = info->m_color[1];
  }
  PVector4 color{(c1[0] - c0[0]) * f + c0[0], (c1[1] - c0[1]) * f + c0[1], (c1[2] - c0[2]) * f + c0[2],
                 (c1[3] - c0[3]) * f + c0[3]};
  float size;
  if (info->m_sizeKey <= t) size = (info->m_size[2] - info->m_size[1]) * (t - info->m_sizeKey) * info->m_invSizeSeg[1] + info->m_size[1];
  else size = (info->m_size[1] - info->m_size[0]) * t * info->m_invSizeSeg[0] + info->m_size[0];
  e->m_submit(p, &pos, rot, size, &color, e->m_data);
}

// The particle has lived its life: off the list and back to the pool.
void KillParticle(pemitter* e, particle* p) {
  Unlink(e, p);
  ParticlePoolFree(p);
}

// One integration step of `dt`: drag, then gravity; the position moves by the velocity as it was
// after the drag.
void Integrate(pemitter* e, particle* p, float dt) {
  const pemitterinfo* info = e->m_info;
  p->m_last2.x = p->m_last.x, p->m_last2.y = p->m_last.y, p->m_last2.z = p->m_last.z;
  p->m_last.x = p->m_pos.x, p->m_last.y = p->m_pos.y, p->m_last.z = p->m_pos.z;
  const float g = info->m_gravity * dt;
  const float drag = info->m_drag * dt;
  p->m_vel.x += p->m_vel.x * drag;
  p->m_vel.y += p->m_vel.y * drag;
  p->m_vel.z += p->m_vel.z * drag;
  const float vx = p->m_vel.x, vy = p->m_vel.y, vz = p->m_vel.z;
  p->m_vel.x += e->m_gravity.x * g;
  p->m_vel.y += e->m_gravity.y * g;
  p->m_vel.z += e->m_gravity.z * g;
  p->m_pos.x += vx * dt;
  p->m_pos.y += vy * dt;
  p->m_pos.z += vz * dt;
}

// Every so many frames a random kick from the turbulence table.
void Turbulence(pemitter* e, particle* p) {
  const pemitterinfo* info = e->m_info;
  const int k = static_cast<int>(TableRand() * 64.0f);
  p->m_vel.x += g_TurbulanceTable[k][0] * info->m_turbulence[0];
  p->m_vel.y += g_TurbulanceTable[k][1] * info->m_turbulence[1];
  p->m_vel.z += g_TurbulanceTable[k][2] * info->m_turbulence[2];
}

bool HasTurbulence(pemitter* e, float dt) {
  const pemitterinfo* info = e->m_info;
  return 0.0f < dt && info->m_turbulence[0] + info->m_turbulence[1] + info->m_turbulence[2] != 0.0f;
}
}  // namespace

// 8030186C
// Particles that move step by step (models; type 3): one step of dt, the turbulence, then drawn
// (when the emitter is drawn and in view).
void ParticleProcess(pemitter* e, float dt) {
  const bool turbulence = HasTurbulence(e, dt);
  e->m_cursor = e->m_head;
  while (particle* p = e->m_cursor) {
    e->m_cursor = p->m_next;
    p->m_vel.w += dt;
    if (p->m_vel.w < p->m_pos.w) {
      const float t = p->m_vel.w / p->m_pos.w;
      Integrate(e, p, dt);
      if (turbulence && e->m_info->m_turbulenceFrames <= e->m_frame) Turbulence(e, p);
      PVector4 pos{p->m_pos.x, p->m_pos.y, p->m_pos.z, p->m_pos.w};
      if ((e->m_flags & 4) && !e->m_culled)
        SubmitParticle(e, p, t, pos, p->m_last.w * p->m_vel.w + p->m_last.w + e->m_info->m_rotation);
    } else {
      KillParticle(e, p);
    }
  }
}

// 80301E34
// Sprites (type 0): as ParticleProcess but in steps of at most 1/15 s.
void ParticleProcessType0(pemitter* e, float dt) {
  const bool turbulence = HasTurbulence(e, dt);
  e->m_cursor = e->m_head;
  while (particle* p = e->m_cursor) {
    e->m_cursor = p->m_next;
    p->m_vel.w += dt;
    if (p->m_vel.w < p->m_pos.w) {
      const float t = p->m_vel.w / p->m_pos.w;
      const double step = 0.06700000166893005 < dt ? 0.06666000187397003 : static_cast<double>(dt);
      bool done = false, stepped = false;
      double rest = dt, last = step;
      do {
        double use = step;
        bool integrate = true;
        if (rest < 0.06666000187397003) {
          done = true;
          if (0.0006000000284984708 < rest) use = rest;
          else if (rest < 0.0) use = static_cast<float>(step + rest);
          else {
            use = last;
            integrate = !stepped;
          }
        }
        if (integrate) {
          stepped = true;
          rest = static_cast<float>(rest - step);
          Integrate(e, p, static_cast<float>(use));
          last = use;
        }
      } while (!done);
      if (turbulence && e->m_info->m_turbulenceFrames <= e->m_frame) Turbulence(e, p);
      PVector4 pos{p->m_pos.x, p->m_pos.y, p->m_pos.z, p->m_pos.w};
      if ((e->m_flags & 4) && !e->m_culled)
        SubmitParticle(e, p, t, pos, p->m_last.w * p->m_vel.w + p->m_last.w + e->m_info->m_rotation);
    } else {
      KillParticle(e, p);
    }
  }
}

namespace {
// Types 1 and 2: where a particle is, from its birth place and velocity, `age` seconds on.
PVector4 Ballistic(pemitter* e, particle* p) {
  const float age = p->m_vel.w;
  const float g = e->m_info->m_gravity * age * age * 0.5f;
  return PVector4{p->m_pos.x + p->m_vel.x * age + e->m_gravity.x * g, p->m_pos.y + p->m_vel.y * age + e->m_gravity.y * g,
                  p->m_pos.z + p->m_vel.z * age + e->m_gravity.z * g, 0.0f};
}
}  // namespace

// 803024A0
// Flag 0x80000: particles on a ballistic path (no drag or turbulence), spinning.
void ParticleProcessType1(pemitter* e, float dt) {
  e->m_cursor = e->m_head;
  while (particle* p = e->m_cursor) {
    e->m_cursor = p->m_next;
    p->m_vel.w += dt;
    if (p->m_vel.w < p->m_pos.w) {
      if ((e->m_flags & 4) && !e->m_culled) {
        const float t = p->m_vel.w / p->m_pos.w;
        PVector4 pos = Ballistic(e, p);
        SubmitParticle(e, p, t, pos, p->m_last.w * p->m_vel.w + p->m_last.w + e->m_info->m_rotation);
      }
    } else {
      KillParticle(e, p);
    }
  }
}

// 803028B8
// Flag 0x100000: as type 1, at the fixed rotation (degrees).
void ParticleProcessType2(pemitter* e, float dt) {
  e->m_cursor = e->m_head;
  while (particle* p = e->m_cursor) {
    e->m_cursor = p->m_next;
    p->m_vel.w += dt;
    if (p->m_vel.w < p->m_pos.w) {
      if ((e->m_flags & 4) && !e->m_culled) {
        const float t = p->m_vel.w / p->m_pos.w;
        PVector4 pos = Ballistic(e, p);
        SubmitParticle(e, p, t, pos, static_cast<float>(e->m_info->m_rotation * 0.01745319925248623 + 0.0));
      }
    } else {
      KillParticle(e, p);
    }
  }
}

// 80302CE0
void ParticleProcessType3(pemitter* e, float dt) { ParticleProcess(e, dt); }

// 80302D00
// An emitting emitter's frame: its age (it dies after its lifetime, flag 0x400000, or stops
// emitting after its stop time, flag 0x800000, telling its interval callback), the world's gravity,
// and unless stopped (8) its new particles: rate x dt, the fractions carried over, up to the maximum.
void PEmitterProcess(pemitter* e, float dt) {
  if (!(e->m_flags & 2)) return;
  pemitterinfo* info = e->m_info;
  e->m_age += dt;
  if ((info->m_flags & 0x400000) && info->m_lifetime < e->m_age) e->m_flags |= 0x4008;
  if ((info->m_flags & 0x800000) && info->m_stopTime < e->m_age) {
    e->m_flags &= ~2u;
    if (e->m_interval) e->m_interval(e->m_intervalData);
  }
  e->m_gravity.x = g_PSystemGravity[0];
  e->m_gravity.y = g_PSystemGravity[1];
  e->m_gravity.z = g_PSystemGravity[2];
  if (!(e->m_flags & 8)) {
    const float want = info->m_rate * dt;
    int whole = static_cast<int>(static_cast<unsigned int>(want));
    e->m_carry += want - static_cast<float>(whole);
    if (1.0f <= e->m_carry) {
      ++whole;
      e->m_carry = 0.0f;
    }
    const int max = static_cast<int>(info->m_maxParticles);
    if (max < whole + e->m_count) whole = max - e->m_count;
    if (whole != 0) ParticleCreate(e, whole);
  }
  if (info->m_turbulenceFrames <= e->m_frame) e->m_frame = 0.0f;
  e->m_frame += 1.0f;
}

// 80302ED0
// A system's frame: every emitter not paused (flag 1) emits; then the dying and always-submitted
// ones (0x4004000; with `submitAll`, all) move and draw their particles (the others do so from the
// level's order table, Emitter::Draw).
void PSystemProcess(float dt, psystem* sys, int submitAll) {
  if (!sys) sys = g_pParticleSystem;
  if (!sys) return;
  const std::vector<pemitter*> emitters = sys->m_emitters;
  for (pemitter* e : emitters)
    if (!(e->m_flags & 1)) PEmitterProcess(e, dt);
  const std::vector<pemitter*> now = sys->m_emitters;
  for (pemitter* e : now)
    if (submitAll || (e->m_flags & 0x4004000)) PEmitterSubmitParticles(dt, e);
}

// 80303008
// The emitter's particles moved and drawn (no time passes while paused, flag 1): its pre-process
// callback, the kind's process, its process callback. A dying emitter (0x4000) with no particles
// left goes after a few more frames: its post-destroy callback, out of its system, destroyed.
void PEmitterSubmitParticles(float dt, pemitter* e) {
  const float use = (e->m_flags & 1) ? 0.0f : dt;
  psystem* sys = e->m_system;
  if (e->m_preProcess) e->m_preProcess(e->m_data);
  const unsigned int flags = e->m_info->m_flags;
  if (flags & 0x80000) ParticleProcessType1(e, use);
  else if (flags & 0x100000) ParticleProcessType2(e, dt);
  else if (!(flags & 0x200000) && !(flags & 0x2000000)) ParticleProcessType0(e, use);
  else ParticleProcessType3(e, use);
  if (e->m_process) e->m_process(e->m_data);
  if ((e->m_flags & 0x4000) && e->m_count == 0 && --e->m_deathFrames < 1) {
    if (e->m_postDestroy) e->m_postDestroy(e->m_postDestroyData);
    PEmitterUnReg(sys, e);
    PEmitterDestroy(&e);
  }
}

// 80303154
void PSystemClean(psystem* sys) {
  if (!sys) return;
  const std::vector<pemitter*> emitters = sys->m_emitters;
  for (pemitter* e : emitters) {
    PEmitterReset(e);
    if (e->m_postDestroy) e->m_postDestroy(e->m_postDestroyData);
    PEmitterUnReg(sys, e);
    PEmitterDestroy(&e);
  }
}

// 80303204
// GC: walks the list and returns nothing (the release build lost the result).
int PSystemIsEmpty(psystem* sys) { return sys && sys->m_emitters.empty(); }

// 80303248
void ParticleDestroy(void** p) {
  ParticlePoolFree(static_cast<particle*>(*p));
  *p = nullptr;
}

// 80303284
void PEmitterReset(pemitter* e) {
  e->m_cursor = e->m_head;
  while (particle* p = e->m_cursor) {
    e->m_cursor = p->m_next;
    KillParticle(e, p);
  }
  e->m_flags |= 6;
  e->m_frame = 0.0f;
  e->m_age = 0.0f;
}

// 8030331C
// A new emitter (from the pool) at `matrix` (identity if none), emitting and drawn (6) plus its
// description's flags.
int PEmitterCreate(pemitter** out, const char* name, const float* matrix, pemitterinfo* info, ParticleSubmitFn submit,
                   EmitterCallbackFn process, ParticleBirthFn birth, void* data) {
  *out = nullptr;
  if (kMaxEmitters <= s_emitterCount) return -1;
  ++s_emitterCount;
  auto* e = new pemitter;
  if (matrix) std::memcpy(&e->m_matrix, matrix, sizeof(EMat4));
  else e->m_matrix.Id();
  e->m_info = info;
  e->m_submit = submit;
  e->m_process = process;
  e->m_birth = birth;
  e->m_data = data;
  e->m_scale = 1.0f;
  std::strncpy(e->m_name, name ? name : "", 0x1c);
  e->m_name[0x1b] = 0;
  e->m_deathFrames = 3;
  e->m_flags = info->m_flags | 6;
  *out = e;
  return 0;
}

// 80303500
void PEmitterDestroy(pemitter** pe) {
  pemitter* e = *pe;
  if (!e) return;
  if (e->m_node) PEmitterUnReg(e->m_system, e);
  e->m_cursor = e->m_head;
  while (particle* p = e->m_cursor) {
    e->m_cursor = p->m_next;
    KillParticle(e, p);
  }
  std::memcpy(e->m_name, "DELETED", 8);
  delete e;
  --s_emitterCount;
  *pe = nullptr;
}

// 803035D4
void PEmitterReg(psystem* sys, pemitter* e) {
  sys->m_emitters.push_back(e);
  e->m_system = sys;
  e->m_node = e;
}

// 80303620
void PEmitterUnReg(psystem*, pemitter* e) {
  if (!e->m_node) return;
  if (psystem* sys = e->m_system) {
    auto& v = sys->m_emitters;
    v.erase(std::remove(v.begin(), v.end(), e), v.end());
  }
  e->m_system = nullptr;
  e->m_node = nullptr;
}

// 8030367C
void PEmitterSetMatrix(pemitter* e, const float* m) { std::memcpy(&e->m_matrix, m, sizeof(EMat4)); }

// 80303708
void PEmitterSetState(pemitter* e, unsigned int bits, int on) {
  if (on) e->m_flags |= bits;
  else e->m_flags &= ~bits;
}

// 80303730
int PEmitterGetNumActiveParticles(pemitter* e) { return e->m_count; }

// 8030373C
int PSystemCreate(psystem** out, const char* name) {
  if (5 <= s_systemCount) return -1;
  ++s_systemCount;
  *out = new psystem;
  (*out)->m_name = name;
  return 0;
}

// 80303798
// GC: LLDestroy runs PEmitterDestroy on each emitter left.
void PSystemDestroy(psystem** sys) {
  if (!*sys) return;
  const std::vector<pemitter*> emitters = (*sys)->m_emitters;
  for (pemitter* e : emitters) {
    pemitter* d = e;
    PEmitterDestroy(&d);
  }
  delete *sys;
  --s_systemCount;
  *sys = nullptr;
}

// 803037DC
// The pools, gravity (+y until PSysLibOpen sets it), and the turbulence table: 64 random unit
// directions with a 0..100 weight (rand seeded from the system clock).
int ParticleSysOpen(int, int, int particles) {
  s_particles.assign(static_cast<size_t>(particles), particle{});
  s_freeParticles.clear();
  for (int i = particles - 1; i >= 0; --i) s_freeParticles.push_back(&s_particles[static_cast<size_t>(i)]);
  s_activeParticles = 0;
  g_PSystemGravity[0] = 0.0f;
  g_PSystemGravity[3] = 1.0f;
  g_PSystemGravity[1] = 1.0f;
  g_PSystemGravity[2] = 0.0f;
  const double now = _pClockMan ? _pClockMan->GetSecDouble(&_sysclock)
                                : std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  s_randState = now >= 2147483648.0 ? static_cast<unsigned int>(static_cast<int>(now - 2147483648.0)) ^ 0x80000000u
                                    : static_cast<unsigned int>(now);
  const double k = 4.656612873077393e-10;
  for (auto& t : g_TurbulanceTable) {
    const float a = static_cast<float>(static_cast<float>(SnRand()) * k);
    const float b = static_cast<float>(static_cast<float>(SnRand()) * k);
    const float c = static_cast<float>(static_cast<float>(SnRand()) * k);
    const float d = static_cast<float>(static_cast<float>(SnRand()) * k);
    t[1] = static_cast<float>((b + b) - 1.0);
    t[0] = static_cast<float>((a + a) - 1.0);
    t[2] = static_cast<float>((c + c) - 1.0);
    t[3] = d * 100.0f;
    const float l = std::sqrt(t[2] * t[2] + t[0] * t[0] + t[1] * t[1]);
    if (l != 0.0f) {
      const float s = 1.0f / l;
      t[0] *= s, t[2] *= s, t[1] *= s;
    }
  }
  return 0;
}

// 80303A54
int ParticleSysClose() {
  s_particles.clear();
  s_freeParticles.clear();
  return 0;
}

// 80303A8C
void PSystemSetWorldGravityDir(const float* dir) {
  g_PSystemGravity[0] = dir[0];
  g_PSystemGravity[3] = 1.0f;
  g_PSystemGravity[1] = dir[1];
  g_PSystemGravity[2] = dir[2];
}

// 80303ABC
int PSystemGetNumActiveParticles() { return s_activeParticles; }

// ---------------------------------------------------------------------------------------------------
// RParticle ('PART' version 14)

// 8036B2E8
RParticle::RParticle() {}
// 8036B34C
RParticle::~RParticle() { Deallocate(); }

// 8036B3AC
void RParticle::Deallocate() {
  DelRefSubResources();
  delete m_info;
  m_info = nullptr;
}

// 8036B404
void RParticle::DelRefSubResources() {
  if (!m_info) return;
  for (int i = 0; i < m_subCount && i < 8; ++i) {
    if (!m_sub[i]) continue;
    m_sub[i]->DelRef();
    m_sub[i] = nullptr;
    if (i == 0) m_info->m_shader = nullptr;
    m_info->m_models[i] = nullptr;
  }
}

namespace {
// The sub-resources into the description: the sprite shader (and model 0), or the models.
void StoreSubResources(RParticle* r) {
  pemitterinfo* info = r->m_info;
  if ((info->m_flags & 0x200000) || !(info->m_flags & 0x2000000)) {
    if (info->m_flags & 0x200000) info->m_models[0] = reinterpret_cast<ERModel*>(r->m_sub[0]);
    else info->m_shader = reinterpret_cast<ERShader*>(r->m_sub[0]);
  } else {
    for (int i = 0; i < r->m_subCount && i < 8; ++i) info->m_models[i] = reinterpret_cast<ERModel*>(r->m_sub[i]);
  }
}
}  // namespace

// 8036B490
void RParticle::AddRefSubResources() {
  pemitterinfo* info = m_info;
  if (!info) return;
  if (!(info->m_flags & 0x200000) && (info->m_flags & 0x2000000)) {
    for (int i = 0; i < m_subCount && i < 8; ++i)
      m_sub[i] = _modelman.AddRef(static_cast<unsigned int>(std::atoi(info->m_modelDefs[i].m_name)), nullptr, 0);
  } else {
    EResourceManager& mgr = (info->m_flags & 0x200000) ? static_cast<EResourceManager&>(_modelman)
                                                       : static_cast<EResourceManager&>(_shaderman);
    m_sub[0] = mgr.AddRef(static_cast<unsigned int>(std::atoi(info->m_shaderName)), nullptr, 0);
  }
  StoreSubResources(this);
}

// 8036B55C
int RParticle::TryIncrementSubResources() {
  pemitterinfo* info = m_info;
  if (!info) return 1;
  if (!(info->m_flags & 0x200000) && (info->m_flags & 0x2000000)) {
    for (int i = 0; i < m_subCount && i < 8; ++i)
      if (!_modelman.TryIncrementResource(static_cast<unsigned int>(std::atoi(info->m_modelDefs[i].m_name)), &m_sub[i])) {
        for (int j = i - 1; j >= 0; --j) {
          if (m_sub[j]) m_sub[j]->DelRef();
          m_sub[j] = nullptr;
        }
        return 0;
      }
  } else {
    EResourceManager& mgr = (info->m_flags & 0x200000) ? static_cast<EResourceManager&>(_modelman)
                                                       : static_cast<EResourceManager&>(_shaderman);
    if (!mgr.TryIncrementResource(static_cast<unsigned int>(std::atoi(info->m_shaderName)), &m_sub[0])) return 0;
  }
  StoreSubResources(this);
  return 1;
}

namespace {
// The description as read: big-endian words, except the name strings (the shader's and each
// model's), which stay as they are.
void SwapInfo(pemitterinfo* info) {
  unsigned char* b = reinterpret_cast<unsigned char*>(info);
  auto isText = [](unsigned int off) {
    if (0xdc <= off && off < 0xfc) return true;
    if (0x16c <= off && off < 0x58c) return (off - 0x16c) % 0x84 < 0x20;
    return false;
  };
  for (unsigned int off = 0; off < 0x58c; off += 4) {
    if (isText(off)) continue;
    std::swap(b[off], b[off + 3]);
    std::swap(b[off + 1], b[off + 2]);
  }
}
}  // namespace

// 8036BCF8
// The 'PART' header (version 14) and the 0x58C-byte description (an old file without the header:
// its name, then the data), then its sub-resources: the sprite shader, the model (flag 0x200000) or
// the models (tabbed geometry, flag 0x2000000), by their decimal ids.
void RParticle::Load(EFile* file, unsigned int) {
  Deallocate();
  m_info = new pemitterinfo;
  std::memset(static_cast<void*>(m_info), 0, 0x58c);
  EDataHeader header;
  if (!header.Read(*file, 0x50415254 /* 'PART' */, 0xe, 0xe)) {
    SIMMY_STUB("RParticle::Load: headerless particle data (name, then the description)", 0x8036BCF8);
    return;
  }
  const unsigned int size = header.m_dataSize < 0x58c ? header.m_dataSize : 0x58c;
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  file->Read(m_info, size);
  if (header.m_dataSize > size) {
    std::vector<unsigned char> rest(header.m_dataSize - size);
    file->Read(rest.data(), static_cast<unsigned int>(rest.size()));
  }
  SwapInfo(m_info);
  pemitterinfo* info = m_info;
  if (!(info->m_flags & 0x200000) && (info->m_flags & 0x2000000)) {
    m_subCount = info->m_modelCount;
    for (int i = 0; i < m_subCount && i < 8; ++i) {
      const int id = std::atoi(info->m_modelDefs[i].m_name);
      m_sub[i] = id ? _modelman.AddRef(static_cast<unsigned int>(id), nullptr, 0) : nullptr;
    }
  } else {
    m_subCount = 1;
    const int id = std::atoi(info->m_shaderName);
    if (id) {
      EResourceManager& mgr = (info->m_flags & 0x200000) ? static_cast<EResourceManager&>(_modelman)
                                                         : static_cast<EResourceManager&>(_shaderman);
      m_sub[0] = mgr.AddRef(static_cast<unsigned int>(id), nullptr, 0);
    }
  }
  StoreSubResources(this);
}

// 8036BF5C
// Reloads the description in place and puts the sub-resources back.
void RParticle::Refresh(EFile* file) {
  if (!file || !m_info) return;
  EDataHeader header;
  if (!header.Read(*file, 0x50415254, 0xe, 0xe)) return;
  ERShader* shader = m_info->m_shader;
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  file->Read(m_info, header.m_dataSize < 0x58c ? header.m_dataSize : 0x58c);
  SwapInfo(m_info);
  m_info->m_shader = shader;
  StoreSubResources(this);
}

// 8036C130 RParticle::New
// 8036C158 RParticle::Construct
// 8036C184 RParticle::Destruct
// 8036C1A8 RParticle::SafeDelete
// 8036C1E8 RParticle::GetTypeInfo
// 8036C1F4 RParticle::GetTypeName
// 8036C200 RParticle::GetTypeKey
// 8036C20C RParticle::GetTypeVersion
// 8036C218 RParticle::GetTypeInfoStatic
// 8036C224 RParticle::GetReadVersion
SIMMY_STORABLE_IMPL(RParticle, "RParticle", 0, EResource)
// 8036C308 RParticle::GetData (inline in the header)

// 8032B5D8
EResource* ParticleManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* r = new RParticle;
  r->m_id = id;
  r->Load(file, size);
  return r;
}

// 8032B638
// An emitter's post-destroy callback: its description's reference goes.
void ParticleManDelRefCB(void* resource) {
  _particleman.DelRef(static_cast<EResource*>(resource), EResourceManager__DelRefMode{1});
}

// 8032B6D4
EResource* ParticleManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 8032B6F4
EResource* ParticleManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }
// 8032B714
int ParticleManager::CanCache(EResource*) { return 1; }

// ---------------------------------------------------------------------------------------------------
// REffectsAttachment ('EATC')

// 80322400
REffectsAttachment::REffectsAttachment() {}
// 80322450
REffectsAttachment::~REffectsAttachment() {}

// 803224F4
// Version 1: 0x20-byte nodes (type +0x0C, node +0x0D, id +0x1C).
void REffectsAttachment::OldLoad(EFile* file) {
  if (!file) return;
  EDataHeader header;
  if (!header.Read(*file, 0x45415443 /* 'EATC' */, 1, 1)) return;
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  if (!header.m_dataSize) return;
  std::vector<unsigned char> raw(header.m_dataSize);
  file->Read(raw.data(), header.m_dataSize);
  m_nodes.assign(header.m_dataSize >> 5, AttachmentNode{});
  for (size_t i = 0; i < m_nodes.size(); ++i) {
    const unsigned char* s = &raw[i * 0x20];
    m_nodes[i].m_node = s[0xd];
    m_nodes[i].m_id = static_cast<unsigned int>(s[0x1c]) << 24 | static_cast<unsigned int>(s[0x1d]) << 16 |
                      static_cast<unsigned int>(s[0x1e]) << 8 | s[0x1f];
    m_nodes[i].m_type = s[0xc];
    m_nodes[i].m_field02 = 0;
  }
}

// 80322780
// Version 2: the nodes as stored (8 bytes each, big-endian); else the old format.
void REffectsAttachment::Load(EFile* file) {
  if (!file) return;
  EDataHeader header;
  if (!header.Read(*file, 0x45415443, 2, 2)) {
    OldLoad(file);
    return;
  }
  m_flags = header.m_dataSize << 8 | (m_flags & 0xff);
  if (!header.m_dataSize) return;
  std::vector<unsigned char> raw(header.m_dataSize);
  file->Read(raw.data(), header.m_dataSize);
  m_nodes.assign(header.m_dataSize >> 3, AttachmentNode{});
  for (size_t i = 0; i < m_nodes.size(); ++i) {
    const unsigned char* s = &raw[i * 8];
    m_nodes[i].m_type = s[0];
    m_nodes[i].m_node = s[1];
    m_nodes[i].m_field02 = static_cast<short>(s[2] << 8 | s[3]);
    m_nodes[i].m_id = static_cast<unsigned int>(s[4]) << 24 | static_cast<unsigned int>(s[5]) << 16 |
                      static_cast<unsigned int>(s[6]) << 8 | s[7];
  }
}

// 80322A04 REffectsAttachment::New
// 80322A34 REffectsAttachment::Construct
// 80322A54 REffectsAttachment::Destruct
// 80322A78 REffectsAttachment::SafeDelete
// 80322AB8 REffectsAttachment::GetTypeInfo
// 80322AC4 REffectsAttachment::GetTypeName
// 80322AD0 REffectsAttachment::GetTypeKey
// 80322ADC REffectsAttachment::GetTypeVersion
// 80322AE8 REffectsAttachment::GetTypeInfoStatic
// 80322AF4 REffectsAttachment::GetReadVersion
SIMMY_STORABLE_IMPL(REffectsAttachment, "REffectsAttachment", 0, EResource)

// 803287F0
EResource* EffectsAttachmentManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  auto* r = new REffectsAttachment;
  r->m_id = id;
  r->Load(file);
  return r;
}
// 803288B8
EResource* EffectsAttachmentManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 803288D8
EResource* EffectsAttachmentManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }
// 803288F8
int EffectsAttachmentManager::CanCache(EResource*) { return 1; }
