// Port of game/emitter.cpp (Emitter: a pemitter as a level instance, drawn from the order table) and
// game/emitterspr3d.cpp (the sprite emitters' callbacks: the facing state, the particle list, each
// sprite with its frame of the sheet and its trails; the geometry emitters: a model per particle).
// See psystem.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"

#include <cstring>

namespace {
unsigned int g_nSysFlags = 0;          // 804FEA34 SprSysSetState(1): 1 camera-facing, 2 the emitter's
                                       // matrix, 4 cylindrical, 8 the facing matrix as is
EMat4* g_pFacingMat = nullptr;         // 804FEA38
EMat4 g_DefaultFacingMat;              // 8048FCB0
unsigned int gSprCount = 0;            // 804FEA3C
EGEPackedParticle* gSprList = nullptr; // 804FEA44
const float gMulColor = 140.0f;        // 804FEA48 colour 0..1 -> the list's byte
const float gMulApha = 116.0f;         // 804FEA4C alpha 0..1 -> the list's byte (halved range)
EMat4 s_facing;                        // PC: the matrix SprSysSetState loads (GC: on its stack)
}  // namespace

// 80238B38
// The sprite state: 1 sets the facing mode (low byte of the flags), 2 the facing matrix (the
// emitter's), 3 the identity; then model matrix 0 is loaded for ENgcRenderer::ParticleListEnd, whose
// sprites span its first two columns: the camera's right and up (mode 1), the camera's right and
// the facing matrix's y axis (4), the facing matrix's x and z axes (2), or the matrix as is (8).
void SprSysSetState(unsigned int state, unsigned long long value) {
  EMat4* m = g_pFacingMat;
  if (state == 2) {
    m = reinterpret_cast<EMat4*>(static_cast<size_t>(value));
  } else if (state == 1) {
    g_nSysFlags = (g_nSysFlags & 0xffffff00u) | static_cast<unsigned int>(value);
    return;
  } else if (state == 3) {
    g_DefaultFacingMat.Id();
    m = &g_DefaultFacingMat;
  }
  g_pFacingMat = m;
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (g_nSysFlags & 1) {
    g_pRC->ModelMatrices(&win->m_lookAt, 1);
    return;
  }
  const float* f = &g_pFacingMat->m[0][0];
  s_facing = EMat4{};
  if (g_nSysFlags & 4) {
    // The facing matrix's y axis (its rotation only) beside the camera's right.
    const float* v = &win->m_lookAt.m[0][0];
    const float up[3] = {f[4], f[5], f[6]};
    float* o = &s_facing.m[0][0];
    o[0] = v[0], o[4] = v[4], o[8] = v[8];
    o[1] = up[0], o[5] = up[1], o[9] = up[2];
    g_pRC->ModelMatrices(&s_facing, 1);
    return;
  }
  if (g_nSysFlags & 8) {
    g_pRC->ModelMatrices(g_pFacingMat, 1);
    return;
  }
  float* o = &s_facing.m[0][0];
  o[0] = f[0], o[4] = f[4], o[8] = f[8];
  o[1] = f[2], o[5] = f[6], o[9] = f[10];
  g_pRC->ModelMatrices(&s_facing, 1);
}

// 80238D38
// Before a sprite emitter's particles: culled when its bounds about it are out of view (unless flag
// 0x1000000); the facing mode (0x80 the emitter's plane, 0x40 the identity as is, 0x400 cylindrical,
// else the camera); the view matrix; the list's description; the sprite shader; z test without
// writes; and the list, room for each particle and its trails (0x10, 0x20).
void EmitterSpr3dPreProcessCB(void* data) {
  auto* e = static_cast<pemitter*>(data);
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  pemitterinfo* info = e->m_info;
  if (!(info->m_flags & 0x1000000)) {
    float b[6] = {info->m_boundMin[0] + e->m_matrix.m[3][0], info->m_boundMin[1] + e->m_matrix.m[3][1],
                  info->m_boundMin[2] + e->m_matrix.m[3][2], info->m_boundMax[0] + e->m_matrix.m[3][0],
                  info->m_boundMax[1] + e->m_matrix.m[3][1], info->m_boundMax[2] + e->m_matrix.m[3][2]};
    if (win->Test(*reinterpret_cast<EBound3*>(b)) == 1) {
      e->m_culled = 1;
      return;
    }
  }
  e->m_culled = 0;
  int count = e->m_count;
  if (info->m_flags & 0x10) count *= 2;
  if (info->m_flags & 0x20) count += e->m_count;
  if (info->m_flags & 0x80) {
    SprSysSetState(1, 2);
    SprSysSetState(2, reinterpret_cast<size_t>(&e->m_matrix));
  } else if (info->m_flags & 0x40) {
    SprSysSetState(1, 8);
    SprSysSetState(3, 0);
  } else if (!(info->m_flags & 0x400)) {
    SprSysSetState(1, 1);
    SprSysSetState(3, 0);
  } else {
    SprSysSetState(1, 4);
    SprSysSetState(2, reinterpret_cast<size_t>(&e->m_matrix));
  }
  gSprCount = 0;
  g_pRC->ViewMatrix(&win->m_lookAt, 2, 0.0f);
  EGEParticleListInfo li{};
  float* d = li.m_data;
  d[0] = -info->m_colorKey[0], d[1] = -info->m_colorKey[1], d[2] = -info->m_sizeKey;
  d[3] = static_cast<float>(e->m_flags);
  d[4] = info->m_invColorSeg[0], d[5] = info->m_invColorSeg[1], d[6] = info->m_invColorSeg[2], d[7] = 0.0f;
  d[8] = info->m_invSizeSeg[0], d[9] = info->m_invSizeSeg[1];
  d[10] = info->m_sizeX * e->m_scale, d[11] = info->m_sizeY * e->m_scale;
  std::memcpy(&d[12], info->m_color, sizeof(info->m_color));
  d[28] = info->m_size[0], d[29] = info->m_size[1], d[30] = info->m_size[2], d[31] = info->m_rotation;
  if (e->m_flags & 0x2000) d[32] = e->m_matrix.m[3][0], d[33] = e->m_matrix.m[3][1], d[34] = e->m_matrix.m[3][2];
  d[35] = info->m_gravity;
  d[36] = e->m_gravity.x, d[37] = e->m_gravity.y, d[38] = e->m_gravity.z, d[39] = 0.5f;
  g_pRC->ParticleListInfo(&li);
  auto* shader = static_cast<ERShader*>(e->m_resource);
  if (EShader* s = shader ? shader->GetShader() : nullptr) s->Select(g_pRC, 0);
  g_pRC->ZTest(true, 2, 1, 0);
  void* list = nullptr;
  if (e->m_count) {
    const unsigned int type = (info->m_flags & 0x80000) ? 1 : (info->m_flags & 0x100000) ? 2 : 0;
    list = g_pRC->ParticleListBegin(static_cast<unsigned int>(count), type);
  }
  gSprList = static_cast<EGEPackedParticle*>(list);
}

namespace {
// One sprite into the list: its place, colour and alpha bytes, frame of the sheet, half size and
// rotation folded into -90..90 degrees (the flip flag marks a half turn).
void PackSprite(const float* pos, const PVector4* color, float rot, float size, const float uv[4]) {
  EGEPackedParticle* s = gSprList;
  s->m_pos[0] = pos[0], s->m_pos[1] = pos[1], s->m_pos[2] = pos[2];
  s->m_color[0] = static_cast<unsigned int>(static_cast<int>(gMulColor * color->x)) & 0xff;
  s->m_color[1] = static_cast<unsigned int>(static_cast<int>(gMulColor * color->y)) & 0xff;
  s->m_color[2] = static_cast<unsigned int>(static_cast<int>(gMulColor * color->z)) & 0xff;
  s->m_color[3] = static_cast<unsigned int>(static_cast<int>(gMulApha * color->w)) & 0xff;
  s->m_uv[0] = uv[0], s->m_uv[1] = uv[1], s->m_uv[2] = uv[2], s->m_uv[3] = uv[3];
  s->m_flip = 0;
  s->m_rot = rot;
  s->m_half[1] = size * 0.5f;
  s->m_half[0] = size * 0.5f;
  while (3.1415927f < s->m_rot) s->m_rot -= 6.2831855f;
  while (s->m_rot < -3.1415927f) s->m_rot += 6.2831855f;
  if (s->m_rot < -1.5707964f) {
    s->m_flip = s->m_flip == 0;
    s->m_rot += 3.1415927f;
  } else if (1.5707964f < s->m_rot) {
    s->m_flip = s->m_flip == 0;
    s->m_rot -= 3.1415927f;
  }
  ++gSprCount;
  ++gSprList;
}
}  // namespace

// 80239230
// One particle as sprites: its frame (flag 0x1000: by its age, looping; 0x800/0x1000: its cell of the
// sheet), its place (local-space emitters: plus the emitter's), and with flag 0x40000 its trails at
// its last positions (0x10: 3/4 size, 0x20: 1/2), each 5% more transparent.
void EmitterSpr3dSubmitCB(particle* p, PVector4* pos, float rot, float size, PVector4* color, void* data) {
  auto* e = static_cast<pemitter*>(data);
  pemitterinfo* info = e->m_info;
  if (info->m_flags & 0x1000) {
    const unsigned int f = static_cast<unsigned int>(p->m_vel.w / info->m_frameTime);
    p->m_last2.w = static_cast<float>(f - (f / info->m_frames) * info->m_frames);
  }
  float uv[4];
  if (!(info->m_flags & 0x1800)) {
    uv[0] = 0.0f, uv[2] = 1.0f, uv[1] = 0.0f, uv[3] = 1.0f;
  } else {
    unsigned int perRow = static_cast<unsigned int>(1.0f / info->m_tileU);
    if (perRow == 0) perRow = 1;
    const unsigned int row = static_cast<unsigned int>(p->m_last2.w / static_cast<float>(static_cast<int>(perRow)));
    const int col = static_cast<int>(p->m_last2.w - static_cast<float>(static_cast<int>(row * perRow)));
    uv[0] = static_cast<float>(col) * info->m_tileU;
    uv[2] = uv[0] + info->m_tileU;
    uv[1] = static_cast<float>(row) * info->m_tileV;
    uv[3] = uv[1] + info->m_tileV;
  }
  float here[3] = {pos->x, pos->y, pos->z};
  float last[3] = {p->m_last.x, p->m_last.y, p->m_last.z};
  float last2[3] = {p->m_last2.x, p->m_last2.y, p->m_last2.z};
  if (e->m_flags & 0x2000) {
    for (int i = 0; i < 3; ++i) {
      here[i] += e->m_matrix.m[3][i];
      last[i] += e->m_matrix.m[3][i];
      last2[i] += e->m_matrix.m[3][i];
    }
  }
  const float sx = info->m_sizeX, sy = info->m_sizeY;
  auto put = [&](const float* at, float s) {
    PackSprite(at, color, rot, 1.0f, uv);
    EGEPackedParticle* q = gSprList - 1;
    q->m_half[1] = s * sy * 0.5f;
    q->m_half[0] = s * sx * 0.5f;
  };
  if (gSprList) put(here, size);
  if (!(info->m_flags & 0x40000)) return;
  if (info->m_flags & 0x10) {
    color->w *= 0.95f;
    if (gSprList) put(last, size * 0.75f);
  }
  if (!(info->m_flags & 0x20)) return;
  color->w *= 0.95f;
  if (gSprList) put(last2, size * 0.5f);
}

// 80239A60
// After the particles: the list drawn, z writes back on, the view matrix restored.
void EmitterSpr3dProcessCB(void* data) {
  auto* e = static_cast<pemitter*>(data);
  if (e->m_culled) return;
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  g_pRC->ParticleListEnd(gSprCount);
  g_pRC->ZTest(true, 2, 0, 0);
  g_pRC->ViewMatrix(&win->m_lookAt, 0, 0.0f);
}

// 80239B04
// Rain about the camera: the emitter 8 above the eye.
void EmitterSpr3dCameraXYZProcessCB(void* data) {
  auto* e = static_cast<pemitter*>(data);
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  e->m_matrix.m[3][0] = win->m_lookAtPos.m[3][0];
  e->m_matrix.m[3][1] = win->m_lookAtPos.m[3][1];
  e->m_matrix.m[3][2] = win->m_lookAtPos.m[3][2] + 8.0f;
  EmitterSpr3dProcessCB(data);
}

// 80239B50
// Ground splashes about the camera's look-at point: a 20 x 20 area, sprites 2 across.
void EmitterSpr3dCamaraXYProcessCB(void* data) {
  auto* e = static_cast<pemitter*>(data);
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  e->m_matrix.m[3][0] = win->m_lookPos.x;
  e->m_matrix.m[3][1] = win->m_lookPos.y;
  e->m_matrix.m[3][2] = 0.06f;
  e->m_info->m_volume[1] = 20.0f;
  e->m_info->m_volume[0] = 20.0f;
  e->m_info->m_sizeY = 2.0f;
  e->m_info->m_sizeX = 2.0f;
  EmitterSpr3dProcessCB(data);
}

// 80239BC4
int EmitterSpr3d::Create(const char* name, const float* matrix, pemitterinfo* info) {
  const int r = Emitter::Create(name, matrix, info, nullptr, nullptr, nullptr, nullptr);
  if (r != -1) {
    SetCallbacks(EmitterSpr3dSubmitCB, EmitterSpr3dPreProcessCB, EmitterSpr3dProcessCB, nullptr, Get(), nullptr,
                 nullptr, nullptr, nullptr);
    pemitter* e = Get();
    e->m_resource = info->m_shader;
    SetMatrix(&e->m_info->m_matrix.m[0][0]);
  }
  return r;
}

// 80239C78
// Rain: follows the camera, ballistic (0x100000), no trails, in world space.
int EmitterSpr3d::CreateCameraFollowXYZ(const char* name, const float* matrix, pemitterinfo* info) {
  const int r = Create(name, matrix, info);
  if (r != -1) {
    SetCallbacks(EmitterSpr3dSubmitCB, EmitterSpr3dPreProcessCB, EmitterSpr3dCameraXYZProcessCB, nullptr, Get(),
                 nullptr, nullptr, nullptr, nullptr);
    pemitter* e = Get();
    e->m_info->m_flags |= 0x1000000;
    e->m_info->m_flags |= 0x100000;
    e->m_info->m_flags &= ~0x30u;
    e->m_flags &= ~0x2000u;
  }
  return r;
}

// 80239D40
int EmitterSpr3d::CreateCameraFollowXY(const char* name, const float* matrix, pemitterinfo* info) {
  const int r = Create(name, matrix, info);
  if (r != -1) {
    SetCallbacks(EmitterSpr3dSubmitCB, EmitterSpr3dPreProcessCB, EmitterSpr3dCamaraXYProcessCB, nullptr, Get(),
                 nullptr, nullptr, nullptr, nullptr);
    pemitter* e = Get();
    e->m_info->m_flags |= 0x1000000;
    e->m_info->m_flags |= 0x100000;
    e->m_info->m_flags &= ~0x30u;
    e->m_flags &= ~0x2000u;
  }
  return r;
}

// 80239E08
void EmitterSpr3d::Destroy() { Emitter::Destroy(); }

// 80239E28
// Before model particles: lit by the sun alone.
void EmitterGeomdPreProcessCB(void*) {
  ERLevel* level = _pEngine ? _pEngine->GetCurrentLevel() : nullptr;
  if (!level) return;
  static ELights lights;  // PC: static (handed to the RC by pointer)
  EVec3 ambient;
  EDirLight sun;
  level->GetSunLight(ambient, sun);
  lights = ELights{};
  lights.m_ambient[0] = ambient.x, lights.m_ambient[1] = ambient.y, lights.m_ambient[2] = ambient.z;
  lights.m_dir[0].m_color[0] = sun.m_color.x, lights.m_dir[0].m_color[1] = sun.m_color.y;
  lights.m_dir[0].m_color[2] = sun.m_color.z;
  lights.m_dir[0].m_dir[0] = sun.m_dir.x, lights.m_dir[0].m_dir[1] = sun.m_dir.y, lights.m_dir[0].m_dir[2] = sun.m_dir.z;
  lights.m_numDir = 1;
  lights.m_numPoint = 0;
  g_pRC->Lights(&lights);
}

// 80239F10
void EmitterGeomProcessCB(void*) { g_pRC->SetAlpha(1.0f); }

namespace {
// A model particle: the model's scale matrix, sized, spun about all three axes and turned half
// round, turned with the emitter (flag 0x80), at the particle; drawn with the particle's alpha when
// its bounding sphere is in view.
void DrawModelParticle(ERModel* model, pemitterinfo* info, pemitter* e, PVector4* pos, float rot, float size,
                       PVector4* color) {
  if (!model) return;
  EMat4 m = *model->GetScaleMatrix();
  float sphere[4] = {pos->x, pos->y, pos->z, model->m_boundSphere[3] * m.m[0][0]};
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (!win->Test(*reinterpret_cast<EBoundSphere*>(sphere))) {
    EVec3 s{size * info->m_sizeX, size * info->m_sizeY, size * info->m_sizeZ};
    m.PreScale(s);
    m.PreRotateX(rot);
    m.PreRotateY(rot);
    m.PreRotateZ(rot);
    m.PreRotateZ(3.1415927f);
    if (info->m_flags & 0x80) {
      EMat4 em = e->m_matrix;
      em.m[3][0] = em.m[3][1] = em.m[3][2] = 0.0f;
      EMat4 local = m;
      m.Mult4x4(em, local);
    }
    m.m[3][0] = pos->x, m.m[3][1] = pos->y, m.m[3][2] = pos->z;
    g_pRC->SetAlpha(color->w);
    g_pRC->ModelMatrices(&m, 1);
    model->Draw(g_pRC);
  }
}
}  // namespace

// 80239F50
void EmitterGeom3dSubmitCB(particle*, PVector4* pos, float rot, float size, PVector4* color, void* data) {
  auto* e = static_cast<pemitter*>(data);
  DrawModelParticle(static_cast<ERModel*>(e->m_resource), e->m_info, e, pos, rot, size, color);
}

// 8023A134
int EmitterGeom3d::Create(const char* name, const float* matrix, pemitterinfo* info) {
  const int r = Emitter::Create(name, matrix, info, nullptr, nullptr, nullptr, nullptr);
  if (r != -1) {
    SetCallbacks(EmitterGeom3dSubmitCB, EmitterGeomdPreProcessCB, EmitterGeomProcessCB, nullptr, Get(), nullptr,
                 nullptr, nullptr, nullptr);
    pemitter* e = Get();
    e->m_resource = info->m_models[0];
    SetMatrix(&e->m_info->m_matrix.m[0][0]);
    e->m_info->m_flags |= 0x200000;
  }
  return r;
}

// 8023A1F8
// As EmitterGeom3dSubmitCB with the particle's own model of the table.
void EmitterGeom3dTabSubmitCB(particle* p, PVector4* pos, float rot, float size, PVector4* color, void* data) {
  auto* e = static_cast<pemitter*>(data);
  const int i = static_cast<int>(p->m_last2.w);
  DrawModelParticle(0 <= i && i < 8 ? e->m_info->m_models[i] : nullptr, e->m_info, e, pos, rot, size, color);
}

// 8023A3F4
int EmitterGeom3dTab::Create(const char* name, const float* matrix, pemitterinfo* info) {
  const int r = Emitter::Create(name, matrix, info, nullptr, nullptr, nullptr, nullptr);
  if (r != -1) {
    SetCallbacks(EmitterGeom3dTabSubmitCB, EmitterGeomdPreProcessCB, EmitterGeomProcessCB, nullptr, Get(), nullptr,
                 nullptr, nullptr, nullptr);
    pemitter* e = Get();
    e->m_resource = info->m_modelDefs;
    SetMatrix(&e->m_info->m_matrix.m[0][0]);
    e->m_info->m_flags |= 0x2000000;
    e->m_info->m_flags |= 0x800;
    e->m_info->m_frames = static_cast<unsigned int>(info->m_modelCount);
  }
  return r;
}

// ---------------------------------------------------------------------------------------------------
// Emitter

// 802383EC
int Emitter::Create(const char* name, const float* matrix, pemitterinfo* info, ParticleSubmitFn submit,
                    EmitterCallbackFn process, ParticleBirthFn birth, void* data) {
  const int r = PEmitterCreate(&m_emitter, name, matrix, info, submit, process, birth, data);
  m_alpha[1] = 1.0f;     // never culled or faded; sort position 0
  m_sortPos[0] = 0.0f;
  m_cullable = 0;
  m_alpha[0] = 1.0f;
  m_fadeMode = 0;
  m_interestFade = 0;
  m_sortPos[2] = 0.0f;
  m_sortPos[1] = 0.0f;
  return r < 0 ? -1 : 0;
}

// 80238480
void Emitter::Destroy() {
  if (m_emitter) PEmitterDestroy(&m_emitter);
}

// 802384B0
// Into a particle system, and (`level`) the current level as an instance.
void Emitter::Reg(psystem* sys, bool level) {
  if (!m_emitter) return;
  PEmitterReg(sys, m_emitter);
  if (level && _pEngine && _pEngine->GetCurrentLevel()) _pEngine->GetCurrentLevel()->InsertInstance(this, nullptr);
}

// 8023850C
void Emitter::UnReg(psystem* sys, bool level) {
  if (!m_emitter) return;
  PEmitterUnReg(sys, m_emitter);
  if (level && m_level && _pEngine && _pEngine->GetCurrentLevel()) _pEngine->GetCurrentLevel()->RemoveInstance(this);
}

// 80238570
void Emitter::SetMatrix(const float* m) {
  if (m_emitter) std::memcpy(&m_emitter->m_matrix, m, sizeof(EMat4));
}

// 80238608
// The emitter's state bits; dying (0x4000) takes it out of the level (the system finishes it).
void Emitter::SetState(int bits, int on) {
  pemitter* e = m_emitter;
  if (!e) return;
  if (on) e->m_flags |= static_cast<unsigned int>(bits);
  else e->m_flags &= ~static_cast<unsigned int>(bits);
  if ((bits & 0x4000) && on && m_level && _pEngine && _pEngine->GetCurrentLevel())
    _pEngine->GetCurrentLevel()->RemoveInstance(this);
}

// 8023867C
int Emitter::GetNumActiveParticles() { return m_emitter ? m_emitter->m_count : 0; }
// 8023869C
pemitter* Emitter::Get() { return m_emitter; }

// 802386A4
Emitter::Emitter() {}

// 8023872C
// A dying emitter belongs to its system (destroyed once its particles are gone).
Emitter::~Emitter() {
  if (m_emitter && !(m_emitter->m_flags & 0x4000)) Destroy();
}

// 8023878C
void Emitter::SetCallbacks(ParticleSubmitFn submit, EmitterCallbackFn preProcess, EmitterCallbackFn process,
                           ParticleBirthFn birth, void* data, EmitterCallbackFn postDestroy, void* postDestroyData,
                           EmitterCallbackFn interval, void* intervalData, unsigned int keep) {
  pemitter* e = m_emitter;
  if (!e) return;
  if (!(keep & 1)) e->m_submit = submit;
  if (!(keep & 4)) e->m_process = process;
  if (!(keep & 0x10)) e->m_data = data;
  if (!(keep & 8)) e->m_birth = birth;
  if (!(keep & 0x20)) e->m_postDestroy = postDestroy;
  if (!(keep & 0x40)) e->m_postDestroyData = postDestroyData;
  if (!(keep & 2)) e->m_preProcess = preProcess;
  if (!(keep & 0x100)) e->m_intervalData = intervalData;
  if (!(keep & 0x80)) e->m_interval = interval;
}

// 8023884C
// GC: SetCallbacks(0xFFFF x5, fn, data, 0, 0): the interval callback is cleared too.
void Emitter::EmitterSetPostDestroyCallback(EmitterCallbackFn fn, void* data) {
  if (m_emitter) SetCallbacks(nullptr, nullptr, nullptr, nullptr, nullptr, fn, data, nullptr, nullptr, 0x1f);
}
// 802388B4
void Emitter::EmitterSetIntervalCallback(EmitterCallbackFn fn, void* data) {
  if (m_emitter) SetCallbacks(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, fn, data, 0x7f);
}

// 80238928
void Emitter::Reset() {
  if (m_emitter) PEmitterReset(m_emitter);
}

// 80238954
void Emitter::SetSortPos(EVec3& pos) {
  m_sortPos[0] = pos.x;
  m_sortPos[2] = pos.z;
  m_sortPos[1] = pos.y;
}

// 80238974
void Emitter::SetScale(float scale) { m_emitter->m_scale = scale; }

// 80238980
// The order table's callback: the emitter's particles moved and drawn now; the draw state the level
// tracks (shader, matrices, lights) is no longer what is loaded.
void Emitter::PEmitterSubmitParticlesCB(ELevelDrawData& data, EOrderTableData* otd) {
  auto* self = static_cast<Emitter*>(otd->m_instance);
  if (!self->m_emitter) return;
  PEmitterSubmitParticles(_gamedt, self->m_emitter);
  data.m_curLights = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curAnim = nullptr;
  data.m_curShader = nullptr;
}

// 802389D4
unsigned int Emitter::VisibilityTest(E3DWindow&) { return 0; }

// 802389DC
// Into the order table (blended sprites sort with their shader's layer, by the emitter's place or,
// on a layer, the sort position; models by the emitter's place), or the depth-of-field foreground.
void Emitter::Draw(ELevelDrawData&) {
  pemitter* e = m_emitter;
  if (!e) return;
  m_otd.m_instance = this;
  m_otd.m_callback = PEmitterSubmitParticlesCB;
  if (!(e->m_flags & 0x2200000)) {
    auto* res = static_cast<ERShader*>(e->m_resource);
    EShader* shader = res ? res->GetShader() : nullptr;
    m_otd.m_flags = shader ? shader->m_surface & 0x3e00 : 0;
    const short layer = shader ? static_cast<short>(shader->m_field02) : 0;
    m_otd.m_sortKey = layer;
    m_otd.m_pos = layer == 0 ? &e->m_matrix.m[3][0] : m_sortPos;
  } else {
    m_otd.m_pos = &e->m_matrix.m[3][0];
  }
  if (!(e->m_flags & 0x8000000)) {
    InsertInOrderTable(m_otd);
  } else {
    m_otd.m_flags = 0x4000;
    GetLevel()->InsertInDOFForegroundObjectList(m_otd);
  }
}

// 80238B08
const char* Emitter::GetInstName(EInstance__InstType& type) {
  type = EInstance__InstType{5};
  return m_emitter->m_resource ? "N/A" : "Unnamed Emitter";
}
