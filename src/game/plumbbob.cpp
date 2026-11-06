// Port of game/physics.cpp and game/plumbbob.cpp (PlumbBobStack, PlumbBob; PlumbBobParms is in
// plumbbobmodel.cpp), and the direct-control interactor's plumb bob part (DirectInteractor::UpdatePlumbBob,
// PreDraw, Draw: game/directinteractor.cpp). See siminteractor.hpp.
// Not ported: the other sims' plumb bobs (PlumbBobOwner, ESim::DrawPlumbBob).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/siminteractor.hpp"
#include "simmy/classes/psystem.hpp"

#include <cmath>

namespace {
float Mag(const EVec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
float SquareMag(const EVec3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
void Normalize(EVec3& v) {
  const float l = Mag(v);
  if (l != 0.0f) v.x /= l, v.y /= l, v.z /= l;
}
const PlumbBobParms& Parms() { return g_plumbBobParms; }
// PlumbBob::s_plumbBobParms + player * 0xC: the player's colour (player 1 green, player 2 blue).
EVec3 PlayerColor(int player) {
  const float* c = g_plumbBobParms.m_colorScale + (player & 1) * 3;
  return EVec3{c[0], c[1], c[2]};
}
// The light the bob and the interactor models are lit with: across the view, in the ground plane.
void SetViewLight(ELights& l, float z) {
  ESimsCam* cam = _globals.GetCam();
  if (!cam) return;
  const float dx = cam->m_target.x - cam->m_eye.x, dy = cam->m_target.y - cam->m_eye.y;
  EVec3 d{-dy, dx, z};
  if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f) Normalize(d);
  l.m_dir[0].m_dir[0] = d.x;
  l.m_dir[0].m_dir[1] = d.y;
  l.m_dir[0].m_dir[2] = d.z;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// Physics

// 80058F8C
Physics::Physics() {}
// 80058FD4
Physics::~Physics() {}

// 80058FFC
// One step: the velocity loses its damping share and gains the acceleration, is held to the top speed,
// and moves the point. With a target: when the step would pass it (the line of the step comes within
// 0.1 of it) or ends within 0.1 of it, the point lands on it (1).
int Physics::PhysicsMove(EVec3* target) {
  const float dt = _dt;
  const float keep = 1.0f - m_damping;
  m_vel.x *= keep, m_vel.y *= keep, m_vel.z *= keep;
  m_vel.x += m_accel.x * dt, m_vel.y += m_accel.y * dt, m_vel.z += m_accel.z * dt;
  const float speed = Mag(m_vel);
  if (m_maxSpeed < speed) {
    const float k = m_maxSpeed / speed;
    m_vel.x *= k, m_vel.y *= k, m_vel.z *= k;
  }
  const EVec3 step{m_vel.x * dt, m_vel.y * dt, m_vel.z * dt};
  if (target) {
    EVec3 to{target->x - m_pos.x, target->y - m_pos.y, target->z - m_pos.z};
    if (Mag(to) < Mag(step)) {
      EVec3 dir = step;
      if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) Normalize(dir);
      const float along = to.x * dir.x + to.y * dir.y + to.z * dir.z;
      const EVec3 closest{along * dir.x + m_pos.x, along * dir.y + m_pos.y, along * dir.z + m_pos.z};
      const EVec3 off{target->x - closest.x, target->y - closest.y, target->z - closest.z};
      if (SquareMag(off) < 0.01f) {
        m_pos = *target;
        return 1;
      }
    }
  }
  m_pos.x += step.x, m_pos.y += step.y, m_pos.z += step.z;
  if (target) {
    const EVec3 off{target->x - m_pos.x, target->y - m_pos.y, target->z - m_pos.z};
    if (SquareMag(off) < 0.01f) {
      m_pos = *target;
      return 1;
    }
  }
  return 0;
}

// 800593CC
// Pulled toward the target with m_accelMag, then a step.
int Physics::PhysicsGravitate(EVec3& target) {
  EVec3 d{target.x - m_pos.x, target.y - m_pos.y, target.z - m_pos.z};
  const float l = Mag(d);
  const float inv = l != 0.0f ? 1.0f / l : 1.0f;
  m_accel = EVec3{d.x * inv * m_accelMag, d.y * inv * m_accelMag, d.z * inv * m_accelMag};
  return PhysicsMove(&target);
}

// ---------------------------------------------------------------------------------------------------
// PlumbBobStack

// 8005974C
void PlumbBobStack::Push(PlumbBob* bob) {
  if (3 < m_count) return;
  m_entries[m_count].m_owner = bob->m_owner;
  m_entries[m_count].m_target = bob->m_target;
  ++m_count;
}
// 80059798
void PlumbBobStack::Pop(PlumbBob* bob) {
  if (m_count == 0) return;
  --m_count;
  bob->m_owner = m_entries[m_count].m_owner;
  bob->m_target = m_entries[m_count].m_target;
}
// 800597E0
void PlumbBobStack::Reset() { m_count = 0; }

// ---------------------------------------------------------------------------------------------------
// PlumbBob

PlumbBob* PlumbBob::s_pPlumbBob[2] = {};

// 800597EC
// The shadow, then the draw state forgotten.
void PlumbBob::PlumbBobOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd) {
  // GC: the record is the bob's own (+0x1B4); PC: found by its position pointer.
  for (PlumbBob* bob : s_pPlumbBob)
    if (bob && otd->m_pos == &bob->m_pos.x) bob->DrawShadow(data.m_rc);
  data.m_curLights = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curAnim = nullptr;
  data.m_curShader = nullptr;
}

// 80059838
// Drawn, still, one directional light, the shadow's draw record (sort layer 0x400).
PlumbBob::PlumbBob() {
  m_lights.m_numDir = 1;
  m_lights.m_numPoint = 0;
  m_otd.m_flags = 0x400;
  m_otd.m_pos = &m_pos.x;
  m_otd.m_callback = PlumbBobOrderTableCallback;
  m_flags = (m_flags & ~0xeu) | 0x11;
}

// 800599D4
// Player 1's bob (and player 2's in a two-player game), in the players' colours; then each starts.
void PlumbBob::StartupAll() {
  if (!s_pPlumbBob[0]) {
    s_pPlumbBob[0] = new PlumbBob;
    s_pPlumbBob[0]->SetPlayerColor(0);
  }
  if (!s_pPlumbBob[1] && _gd.m_fieldEC) {  // DAT_80475C70: two players
    s_pPlumbBob[1] = new PlumbBob;
    s_pPlumbBob[1]->SetPlayerColor(1);
  }
  for (PlumbBob* bob : s_pPlumbBob)
    if (bob) bob->Startup();
}

// 80059A84
void PlumbBob::ShutdownAll() {
  for (PlumbBob*& bob : s_pPlumbBob) {
    if (!bob) continue;
    bob->Shutdown();
    delete bob;
    bob = nullptr;
  }
}

// 80059AF4
// The model (0x6D1F0956, the green diamond) at the tuning's height, and its two particle systems.
void PlumbBob::Startup() {
  if (m_model) return;
  m_model = new PlumbBobModel;
  SetModel(0x6d1f0956);
  m_model->m_baseHeight = Parms().m_height;  // DAT_804855C0
  AddParticleSystem(0x73a1b85a);
  AddParticleSystem(0x83a20a04);
}

// 80059B78
// Drawn or not; hidden, the particles' draw state is remembered (4 -> 8) and they stop.
void PlumbBob::EnableDraw(bool on) {
  m_flags = on ? (m_flags | 1) : (m_flags & ~1u);
  if (!on) {
    m_flags = (m_flags & 4) ? (m_flags | 8) : (m_flags & ~8u);
    SetParticleDrawState(false);
  } else {
    SetParticleDrawState(((m_flags >> 3) & 1) != 0);
  }
}

// 80059BEC
void PlumbBob::Shutdown() {
  m_stack.Reset();
  m_owner = 0;
  delete m_model;
  m_model = nullptr;
  DestroyParticleSystems();
}

// 80059C44
PlumbBob::~PlumbBob() { Shutdown(); }

// 80059CA0
// A sprite emitter of the particle description `id` (sparkle, glow) into the global system, its
// description released when it goes; not emitting yet.
void PlumbBob::AddParticleSystem(unsigned int id) {
  auto* res = static_cast<RParticle*>(_particleman.AddRef(id, nullptr, 0));
  if (res) {
    auto* e = new EmitterSpr3d;
    m_emitters.push_back(e);
    e->Create("plumbbob_emitter", nullptr, res->m_info);
    e->EmitterSetPostDestroyCallback(ParticleManDelRefCB, res);
    e->Reg(g_pParticleSystem, true);
  }
  SetParticleDrawState(false);
}

// 80059D90
// Each emitter at the plumb bob (tilted with it), scaled with it (at most 3.5) and raised by the
// grown model's extra height; its description's own matrix on top (its translation with flag
// 0x8000, the plumb bob's rotation with 0x10000).
void PlumbBob::UpdateParticleSystem(int) {
  for (Emitter* emitter : m_emitters) {
    if (!emitter || !emitter->Get()) return;
    pemitterinfo* info = emitter->Get()->m_info;
    EMat4 bob;
    bob.Id();
    if (m_model->m_tilt != 0.0f) bob.PostRotateX(m_model->m_tilt);
    float height = Parms().m_height;  // DAT_804855C0
    const float scale = m_model->m_scale;
    emitter->SetScale(3.5f < scale ? 3.5f : scale);
    if (1.0f < scale && m_model->m_model) {
      ERModel* model = m_model->m_model;
      height += (model->m_bounds2[5] - model->m_bounds2[2]) * model->m_scale * 0.5f * (scale - 1.0f);
    }
    bob.m[3][0] = m_pos.x;
    bob.m[3][1] = m_pos.y;
    bob.m[3][2] = m_pos.z + height;
    EMat4 local = info->m_matrix;
    if (!(info->m_flags & 0x10000)) {
      bob.m[2][2] = 1.0f, bob.m[0][0] = 1.0f, bob.m[0][1] = 0.0f, bob.m[0][2] = 0.0f, bob.m[1][0] = 0.0f;
      bob.m[1][1] = 1.0f, bob.m[1][2] = 0.0f, bob.m[2][0] = 0.0f, bob.m[2][1] = 0.0f;
    }
    if (!(info->m_flags & 0x8000)) local.m[3][2] = local.m[3][0] = local.m[3][1] = 0.0f;
    EMat4 out;
    out.Mult4x4(local, bob);
    emitter->SetMatrix(&out.m[0][0]);
  }
}

// 8005A050
// The emitters die (their particles finish in the system).
void PlumbBob::DestroyParticleSystems() {
  for (Emitter*& e : m_emitters) {
    if (!e) continue;
    e->SetState(0x4000, 1);
    e->SetState(8, 1);
    delete e;
    e = nullptr;
  }
  m_emitters.clear();
}

// 8005A100
// GC (release build): `on` is ignored; the emitters stop emitting and the draw-state flag 4 is
// cleared, so the plumb bob's particles never show.
void PlumbBob::SetParticleDrawState(bool) {
  for (Emitter* e : m_emitters) e->SetState(2, 0);
  m_flags &= ~4u;
}

// 8005A184
void PlumbBob::SetParticleVisibility(bool on) {
  for (Emitter* e : m_emitters) {
    e->SetState(4, on ? 1 : 0);
    m_flags = on ? (m_flags | 0x10) : (m_flags & ~0x10u);
  }
}

// 8005A218
// A new motion state; the bounces (5, 6) start from the target with the tuning's bounce speeds.
void PlumbBob::SetState(int state) {
  if (state == m_state) return;
  m_state = state;
  if (state < 7 && 4 < state) {
    m_target = m_pos;
    m_damping = 0.0f;
    m_vel = EVec3{0.0f, 0.0f, Parms().m_field48[1]};      // DAT_804855EC
    m_maxSpeed = Parms().m_field48[2];                    // DAT_804855F0
    m_accel = EVec3{0.0f, 0.0f, Parms().m_field48[3]};    // DAT_804855F4
    m_bounces = 0;
  }
}

// 8005A2E0
unsigned int PlumbBob::SetModel(unsigned int id) { return m_model ? m_model->SetModel(id) : 0; }
// 8005A314
unsigned int PlumbBob::SetShadow(unsigned int id) { return m_model ? m_model->SetShadow(id) : 0; }

// 8005A348
// The light's colour, and the ambient (the colour times the tuning's 0.3).
void PlumbBob::SetColor(EVec3* color) {
  const float k = Parms().m_field24;  // DAT_804855C4
  m_lights.m_dir[0].m_color[0] = color->x;
  m_lights.m_dir[0].m_color[1] = color->y;
  m_lights.m_dir[0].m_color[2] = color->z;
  m_lights.m_ambient[0] = color->x * k;
  m_lights.m_ambient[1] = color->y * k;
  m_lights.m_ambient[2] = color->z * k;
}

// 8005A3BC
void PlumbBob::SetPlayerColor(int player) {
  EVec3 c = PlayerColor(player);
  SetColor(&c);
}

// 8005A3EC
// Bigger as the camera zooms out (up to the tuning's 2.25), unless the camera director has the camera.
void PlumbBob::SetScaleFromTween(int player, float tween) {
  if (!m_model) return;
  auto* d = static_cast<CameraDirector*>(_globals.CameraDirectorOf(player));
  const bool directed = d && d->m_state != CameraDirector::kIdle && d->m_state != CameraDirector::kGame;
  m_model->m_scale = directed ? 1.0f : (Parms().m_field30[5] - 1.0f) * tween + 1.0f;  // DAT_804855E4
}

// 8005A45C
void PlumbBob::BecomeTransitionIndicator() { m_morph = 1; }
// 8005A468
void PlumbBob::ForceMorphToPlumbbob() {
  if (m_morph != 0) m_morph = 0;
}
// 8005A480
void PlumbBob::StartPositiveRepMomentEffect() {
  if (m_morph != 0) return;
  m_morph = 2;
  m_morphTime = 0.0f;
}
// 8005A4A4
void PlumbBob::StartNegativeRepMomentEffect() {
  if (m_morph != 0) return;
  m_morph = 3;
  m_morphTime = 0.0f;
}

// 8005A4C8
// Once a frame (the interactor resets m_updates): the motion (3: flying to the target under the tuning's
// pull, landing on it -> 4 -> still; 1: jumping there; 2: gravitating; 5/6: bouncing up from the target and
// back, 7 the last bounce), the size from the camera's zoom, the pulse (1: between 0.8 and 1.25), and the
// model by the morph state (0 the plumb bob, 1 the transition indicator, 2/3 reputation moments for three
// seconds): the old model shrinks away and the new one grows in (1.2, then back to 1).
void PlumbBob::Update(int player) {
  const int updates = m_updates;
  m_flags &= ~2u;
  m_updates = updates + 1;
  if (1 < updates + 1) return;
  const PlumbBobParms& p = Parms();
  auto landed = [this, &p] {
    m_landVel = EVec3{m_vel.x * p.m_field30[3], m_vel.y * p.m_field30[3], m_vel.z * p.m_field30[3]};  // DAT_804855DC
    m_vel = EVec3{0.0f, 0.0f, 0.0f};
    SetState(4);
  };
  const int state = m_state;
  if (state == 3) {
    m_accelMag = p.m_field30[0];  // DAT_804855D0
    m_damping = p.m_field30[1];   // DAT_804855D4
    m_maxSpeed = p.m_field30[2];  // DAT_804855D8
    EVec3 dir{m_target.x - m_pos.x, m_target.y - m_pos.y, m_target.z - m_pos.z};
    if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) Normalize(dir);
    const float speed = Mag(m_vel);
    m_vel = EVec3{dir.x * speed, dir.y * speed, dir.z * speed};
    m_accel = EVec3{dir.x * m_accelMag, dir.y * m_accelMag, dir.z * m_accelMag};
    if (PhysicsMove(&m_target)) landed();
  } else if (state < 4) {
    if (state == 1) {
      m_pos = m_target;
      SetState(0);
    } else if (state < 2) {
      m_vel = EVec3{0.0f, 0.0f, 0.0f};
    } else {
      m_damping = p.m_field30[1];
      m_maxSpeed = p.m_field30[2];
      m_accelMag = p.m_field30[0];
      if (PhysicsGravitate(m_target)) landed();
    }
  } else if (state == 4) {
    SetState(0);
  } else if (6 < state) {
    m_vel = EVec3{0.0f, 0.0f, 0.0f};
  } else {
    PhysicsMove(nullptr);
    float z = m_pos.z;
    if (0.0f <= m_vel.z) {
      if (m_target.z < z) z = m_target.z;
    } else {
      const float top = m_target.z + p.m_field48[0];  // DAT_804855E8
      if (m_bounces < p.m_field58 || m_target.z < m_pos.z) {  // DAT_804855F8
        z = m_pos.z;
        if (m_pos.z < top) {
          m_vel.z = -m_vel.z;
          ++m_bounces;
          z = top;
        }
      } else {
        if (m_state == 7) {
          SetState(0);
        } else {
          SetState(7);
          z = m_target.z - p.m_field48[0];
        }
        m_flags |= 2;
      }
    }
    m_pos.z = z;
  }
  ESimsCam* cam = _gd.m_players[player & 1].m_cam;  // DAT_80475C84 + player * 0x10
  SetScaleFromTween(player, cam ? cam->GetCurZoomRatio() : 0.0f);
  if (m_pulse == 0) {
    m_pulseDir = 0;
    m_pulseScale = 1.0f;
  } else if (m_pulse == 1) {
    if (m_pulseDir < 0) {
      m_pulseScale -= _dt * 3.0f;
      if (m_pulseScale < 0.8f) m_pulseDir = 1;
    } else {
      m_pulseScale += _dt * 3.0f;
      if (1.25f < m_pulseScale) m_pulseDir = -1;
    }
    if (m_model) m_model->m_scale *= m_pulseScale;
  }
  const float step = _dt * 6.0f;
  if (3.0f < m_morphTime) m_morph = 0;
  unsigned int model = 0x6d1f0956, particles = 0x83a20a04;
  int pulse = 0;
  bool plain = false;
  if (m_morph == 1) {
    model = 0xcbf05ef5;
    pulse = 1;
    m_morphTime = 0.0f;
    particles = 0xe9111e72;
    m_morph = 0;
  } else if (m_morph < 2) {
    m_morphTime = 0.0f;
    plain = true;
  } else if (m_morph == 2) {
    model = 0x0cd5495d;
    particles = 0xa1f7fa2b;
    m_morphTime += _dt;
  } else if (m_morph == 3) {
    model = 0x0cd5495d;
    particles = 0x94e96649;
    m_morphTime += _dt;
  } else {
    m_morphTime = 0.0f;
    plain = true;
  }
  const unsigned int current = m_model && m_model->m_model ? m_model->m_model->m_id : 0;
  if (current == 0x8af61033 && m_morphScale == 1.0f && m_morph == 3) {
    SetModel(0x8af61033);
    m_pulse = pulse;
    m_morphDir = 0;
  } else if (current == model && m_morphScale == 1.0f) {
    m_pulse = pulse;
    m_morphDir = 0;
  } else {
    if (current == model) {
      if (m_morphDir < 0) {
        m_morphScale -= step;
        if (m_morphScale <= 1.0f) m_morphScale = 1.0f;
      } else {
        m_morphScale += step;
        if (1.2f <= m_morphScale) m_morphDir = -1;
      }
    } else if (m_morphDir < 0) {
      m_morphScale -= step;
      if (m_morphScale <= step) {
        DestroyParticleSystems();
        if (plain) AddParticleSystem(0x73a1b85a);
        AddParticleSystem(particles);
        SetParticleDrawState(true);
        SetModel(model);
        m_morphDir = 1;
      }
    } else {
      m_morphScale += step;
      if (1.2f < m_morphScale) m_morphDir = -1;
    }
    if (m_model) m_model->m_scale *= m_morphScale;
  }
  UpdateParticleSystem(player);
}

// 8005AD18
void PlumbBob::DrawShadow(ERC* rc) {
  if (!m_model) return;
  rc->Lights(&m_lights);
  m_model->m_pos = m_pos;
  m_model->DrawShadow(rc);
}

// 8005AD8C
// Not while the owner's sim hides its plumb bob (ESim +0x3EC states 1, 2, 5, 6, 8, 9), nor undrawn or
// without a model. The light falls across the view; the shadow is left out while the camera director runs
// a shot.
void PlumbBob::Draw(ERC* rc) {
  if (cXPerson* person = _globals.GetSelectedPerson(s_pPlumbBob[0] != this ? 1 : 0)) {
    cXObjectImpl* impl = person->AsObject() ? person->AsObject()->GetObjectImplementation() : nullptr;
    if (ESim* sim = impl ? dynamic_cast<ESim*>(impl->m_instance) : nullptr) {
      const unsigned int s = static_cast<unsigned int>(sim->m_plumbBobState);
      if (s < 7) {
        if (4 < s) return;
        if (s < 3 && s != 0) return;
      } else if (s < 10 && 7 < s) {
        return;
      }
    }
  }
  if (!(m_flags & 1) || !m_model || !m_model->m_model) return;
  SetViewLight(m_lights, m_pos.z);
  CameraDirector* d = _globals.m_cameraDirector;  // DAT_80475EAC
  if (!(d && d->m_state != CameraDirector::kIdle && d->m_state != CameraDirector::kGame)) DrawShadow(rc);
  rc->Lights(&m_lights);
  m_model->m_pos = m_pos;
  m_model->Draw(rc);
}

// 8005AF94
void PlumbBob::ResetMomentum(bool keepZ) {
  const float vz = keepZ ? m_vel.z : 0.0f;
  m_vel = EVec3{0.0f, 0.0f, vz};
  const float az = keepZ ? m_vel.z : 0.0f;
  m_accel = EVec3{0.0f, 0.0f, az};
}

// ---------------------------------------------------------------------------------------------------
// InteractorModule::DirectInteractor's plumb bob

// 802104A8
// The person's pelvis (its animator's bone 2), with z from its head (bone 1) when `ground` is false
// (`highest`: only if above), else 0; its tile's centre while the animator has no position yet.
void InteractorModule::GetPersonPosition(EVec3* out, cXPerson* person, bool ground, bool highest) {
  if (!person) return;
  SAnimator2* anim = person->GetSAnimator();
  if (anim) {
    anim->GetBonePos(2, *out);
    if (!ground) {
      EVec3 head;
      anim->GetBonePos(1, head);
      if (!highest || out->z < head.z) out->z = head.z;
      // PC: SAnimator2's bone positions come out at floor height (world side: m_bonePos), so the sim's
      // bounds' top (positionCharacter: 1.8) stands in for the head while they do.
      if (out->z < 0.5f)
        if (cXObject* o = person->AsObject())
          if (cXObjectImpl* impl = o->GetObjectImplementation())
            if (impl->m_instance && 0.5f < impl->m_instance->m_bounds[5]) out->z = impl->m_instance->m_bounds[5];
    } else {
      out->z = 0.0f;
    }
    if (0.5f <= SquareMag(*out)) return;
  }
  cXObject* o = person->AsObject();
  const CTilePt t = o ? o->GetCTilePt() : CTilePt();
  out->x = static_cast<float>(t.GetY()) + 0.5f;
  out->y = static_cast<float>(t.GetX()) + 0.5f;
  EVec3 head{0.0f, 0.0f, 0.0f};
  if (!ground && anim) anim->GetBonePos(1, head);
  out->z = ground ? 0.0f : head.z;
}
// 802119C0
RET_UNKNOWN InteractorModule::GetPlumbBob(int player) {
  return reinterpret_cast<RET_UNKNOWN>(static_cast<unsigned int>(player) < 2 ? PlumbBob::s_pPlumbBob[player] : nullptr);
}

// 80204438
// The bob flies over the sim's head; the interactor's extents are the bob's.
void InteractorModule__DirectInteractor::UpdatePlumbBob() {
  auto* bob = static_cast<PlumbBob*>(m_plumbBob);
  if (!bob) return;
  EVec3 p;
  InteractorModule::GetPersonPosition(&p, m_person, false, true);
  bob->SetState(3);
  bob->m_target = p;
  bob->Update(m_player);
  if (bob->m_model) bob->m_model->GetExtents(m_extents);
  if (!(bob->m_flags & 0x10)) {
    if (!IsSimulatorPaused()) bob->SetParticleVisibility(true);
  } else if (IsSimulatorPaused()) {
    bob->SetParticleVisibility(false);
  }
}

// 80205014
void InteractorModule__DirectInteractor::PreDraw(InteractorModule__InteractorVisualizer& v) { v.PreDraw(*this); }
// 80205040
void InteractorModule__DirectInteractor::Draw(InteractorModule__InteractorVisualizer& v) { v.Draw(*this); }
