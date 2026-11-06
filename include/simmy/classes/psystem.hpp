// @class PVector4 after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class pemitterinfo after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class particle after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class pemitter after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class psystem after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class RParticle after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class AttachmentNode after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class REffectsAttachment after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class EGEParticleListInfo after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class EGEPackedParticle after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class Emitter after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class EmitterSpr3d after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class EmitterGeom3d after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class EmitterGeom3dTab after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class EParticleEffect after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// @class EParticleObj after EResource DigestGeomShaderPtr EMat4 EVec3 Effects__Effect__ClientParams
// The older particle system (engine/psystem.cpp, engine/rparticle.cpp, engine/particleman.cpp,
// engine/reffects_attachment.cpp, game/emitter.cpp, game/emitterspr3d.cpp, game/iobject.cpp's
// EParticleEffect / EParticleObj). See src/engine/psystem.cpp and src/game/emitter.cpp.
//
// An emitter's description ('PART' resources in emitters.arc, pemitterinfo) says how particles are
// born (a plane, sphere, cone or cylinder about the emitter, at a rate, up to a count), how they live
// (life, speed, drag, gravity, turbulence, spin) and look (four colour keys, three size keys, a
// sprite shader or models). A pemitter (one per running effect) owns its particles; the particle
// systems (psystem: global, weather, models, transitions) hold the emitters. An Emitter is the
// level instance of a pemitter: it is put in the level's order table and draws (and moves) its
// particles there.
#pragma once

#include <cstddef>
#include <vector>

struct ERShader;
struct ERModel;
struct psystem;
struct pemitter;

struct PVector4 {
  float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

// The emitter description, 0x58C bytes as stored (big-endian on disc; host-endian once loaded). The
// GC stored the resolved shader / models over the name strings (+0xDC, +0x16C...); on PC they are kept
// beside the data (m_shader, m_models).
struct pemitterinfo {
  struct Model {                 // 0x84 bytes
    char m_name[0x20];           // +0x00 the model id, as decimal text
    float m_weight;              // +0x20 percent chance of being picked (tabbed geometry)
    unsigned int m_pad[0x18];
  };
  EMat4 m_matrix;                // +0x000 the emitter's own transform
  float m_turbulence[3];         // +0x040
  float m_turbulenceFrames;      // +0x04C the turbulence is applied once per so many frames
  float m_color[4][4];           // +0x050 colour keys (rgba, 0..1): birth, key 1, key 2, death
  float m_colorKey[2];           // +0x090 life fraction of colour keys 1 and 2
  float m_lifeMin, m_lifeMax;    // +0x098 seconds
  float m_speedMin, m_speedMax;  // +0x0A0
  float m_fieldA8;               // +0x0A8
  float m_size[3];               // +0x0AC size keys: birth, middle, death
  float m_sizeKey;               // +0x0B8 life fraction of the middle size
  float m_spin;                  // +0x0BC random spin range (radians per second)
  float m_drag;                  // +0x0C0 velocity gained per second (negative: drag)
  float m_lifetime;              // +0x0C4 the emitter dies after this (flag 0x400000)
  float m_rate;                  // +0x0C8 particles per second
  float m_gravity;               // +0x0CC
  float m_volume[3];             // +0x0D0 birth volume: width, height / radius, spread
  char m_shaderName[0x20];       // +0x0DC the sprite shader id, as decimal text
  float m_maxParticles;          // +0x0FC
  int m_birthType;               // +0x100 0 plane, 1 sphere, 2 cone, 3 cylinder, else the emitter's
  unsigned int m_frames;         // +0x104 sprite frames (flags 0x800 / 0x1000)
  unsigned int m_flags;          // +0x108
  float m_sizeX, m_sizeY, m_sizeZ;  // +0x10C sprite / model scale
  float m_rotation;              // +0x118 base rotation
  float m_tileU, m_tileV;        // +0x11C a frame's size in the sprite sheet
  float m_frameTime;             // +0x124 seconds per frame (flag 0x1000)
  float m_field128[2];           // +0x128
  float m_invColorSeg[3];        // +0x130 1 / length of each colour segment
  float m_invSizeSeg[2];         // +0x13C 1 / length of each size segment
  float m_stopTime;              // +0x144 the emitter stops emitting after this (flag 0x800000)
  float m_boundMin[4];           // +0x148 the emitter's bounds about its position (culling)
  float m_boundMax[4];           // +0x158
  int m_modelCount;              // +0x168
  Model m_modelDefs[8];          // +0x16C
  // PC: the resolved sub-resources (the GC wrote them over m_shaderName / m_modelDefs[i].m_name).
  ERShader* m_shader = nullptr;
  ERModel* m_models[8] = {};
};
static_assert(offsetof(pemitterinfo, m_flags) == 0x108, "pemitterinfo layout");
static_assert(offsetof(pemitterinfo, m_modelDefs) == 0x16C, "pemitterinfo layout");
static_assert(sizeof(pemitterinfo::Model) == 0x84, "pemitterinfo layout");

// One particle (GC: 0x50 bytes after the pool's link words).
struct particle {
  particle* m_next = nullptr;    // +0x00 the emitter's list
  particle* m_prev = nullptr;    // +0x04
  int m_field08[2] = {};
  PVector4 m_pos;                // +0x10 w: life (seconds)
  PVector4 m_vel;                // +0x20 w: age (seconds)
  PVector4 m_last;               // +0x30 last position; w: spin speed
  PVector4 m_last2;              // +0x40 the one before; w: sprite frame / model
  bool m_inUse = false;          // PC: the pool's bookkeeping
};

using ParticleSubmitFn = void (*)(particle*, PVector4*, float, float, PVector4*, void*);
using ParticleBirthFn = void (*)(pemitter*, PVector4*, PVector4*);
using EmitterCallbackFn = void (*)(void*);

// A running emitter (0xC0 bytes).
struct pemitter {
  EMat4 m_matrix;                // +0x00 its place in the world
  PVector4 m_gravity;            // +0x40 the world's gravity this frame
  unsigned int m_flags = 0;      // +0x50 2 emitting, 4 drawn, 8 stopped, 0x2000 local space,
                                 // 0x4000 dying (destroyed once empty), 0x8000000 depth-of-field fg
  // +0x54 the particle list (LLCreate "LclParticles": count +0x84, head +0x88, tail +0x8C, cursor +0x90)
  int m_count = 0;
  particle* m_head = nullptr;
  particle* m_tail = nullptr;
  particle* m_cursor = nullptr;
  float m_age = 0.0f;            // +0x58 seconds since it started
  float m_carry = 0.0f;          // +0x5C fraction of a particle owed
  char m_name[0x1c] = {};        // +0x60
  int m_deathFrames = 3;         // +0x7C frames to wait once dying and empty
  void* m_resource = nullptr;    // +0x80 the sprite shader (ERShader*), model (ERModel*) or the
                                 // models' table (pemitterinfo::Model*)
  float m_frame = 0.0f;          // +0x84 frame counter (turbulence)
  pemitterinfo* m_info = nullptr;  // +0x88
  void* m_node = nullptr;        // +0x8C its node in its system
  ParticleSubmitFn m_submit = nullptr;     // +0x90 per particle
  EmitterCallbackFn m_preProcess = nullptr;  // +0x94
  EmitterCallbackFn m_process = nullptr;   // +0x98
  ParticleBirthFn m_birth = nullptr;       // +0x9C birth type 4+
  void* m_data = nullptr;                  // +0xA0 the callbacks' argument
  EmitterCallbackFn m_postDestroy = nullptr;  // +0xA4
  void* m_postDestroyData = nullptr;       // +0xA8
  EmitterCallbackFn m_interval = nullptr;  // +0xAC when it stops emitting
  void* m_intervalData = nullptr;          // +0xB0
  int m_culled = 0;              // +0xB4 its bounds are out of view this frame
  psystem* m_system = nullptr;   // +0xB8
  float m_scale = 1.0f;          // +0xBC
};

// A particle system: its emitters (GC: an LL list "LocEmitter" nodes, with a cursor).
struct psystem {
  std::vector<pemitter*> m_emitters;
  const char* m_name = nullptr;
};

extern psystem* g_pParticleSystem;            // 804B08F8
extern psystem* g_pParticleWeatherSystem;
extern psystem* g_pParticleModelSystem;
extern psystem* g_pParticleTransitionSystem;
extern psystem* g_pTextParticleSystem;         // 804FF6DC "textpsys": Apt text particles (aptstring_particles.cpp)

int PSysLibOpen();   // 80300B38
int PSysLibClose();  // 80300BF8
void PSystemProcess(float dt, psystem* sys, int submitAll);  // 80302ED0
void PEmitterSubmitParticles(float dt, pemitter* e);           // 80303008
void PSystemClean(psystem* sys);                               // 80303154
int PSystemIsEmpty(psystem* sys);                              // 80303204
void PEmitterReset(pemitter* e);                               // 80303284
int PEmitterCreate(pemitter** out, const char* name, const float* matrix, pemitterinfo* info,
                   ParticleSubmitFn submit, EmitterCallbackFn process, ParticleBirthFn birth, void* data);  // 8030331C
void PEmitterDestroy(pemitter** e);                            // 80303500
void PEmitterReg(psystem* sys, pemitter* e);                   // 803035D4
void PEmitterUnReg(psystem* sys, pemitter* e);                 // 80303620
void PEmitterSetMatrix(pemitter* e, const float* m);           // 8030367C
void PEmitterSetState(pemitter* e, unsigned int bits, int on); // 80303708
int PEmitterGetNumActiveParticles(pemitter* e);                // 80303730
int PSystemCreate(psystem** out, const char* name);            // 8030373C
void PSystemDestroy(psystem** sys);                            // 80303798
int ParticleSysOpen(int systems, int emitters, int particles); // 803037DC
int ParticleSysClose();                                        // 80303A54
void PSystemSetWorldGravityDir(const float* dir);              // 80303A8C
int PSystemGetNumActiveParticles();                            // 80303ABC

// 'PART' resource (0x3C bytes): the description and its sub-resources.
struct RParticle : public EResource {
  RParticle();  // 8036B2E8
  SIMMY_STORABLE_DECL(RParticle)
  ~RParticle() override;  // 8036B34C
  void Deallocate();  // 8036B3AC
  void DelRefSubResources() override;  // 8036B404
  void AddRefSubResources() override;  // 8036B490
  int TryIncrementSubResources() override;  // 8036B55C
  void PrepareData();  // 8036B6BC
  void Load(EFile* file, unsigned int size);  // 8036BCF8
  using EResource::Load;
  void Refresh(EFile* file) override;  // 8036BF5C
  static void RegisterType(unsigned short);  // 8036C230
  void CreateCopy() const;  // 8036C284
  pemitterinfo* GetData() { return m_info; }  // 8036C308

  pemitterinfo* m_info = nullptr;  // +0x14
  int m_subCount = 0;              // +0x18
  EResource* m_sub[8] = {};        // +0x1C the shader or models
};

// One effect of an attachment (8 bytes): its kind (0 a particle emitter, 1 an effects emitter;
// 0x80: sort by the emitter's position), the model node it follows (0x80: none, the object's
// matrix), and the resource id.
struct AttachmentNode {
  static const unsigned char kUseEmitterPosForAlphaSort = 0x80;  // 8050110C
  unsigned char m_type = 0;   // +0x00
  unsigned char m_node = 0;   // +0x01
  short m_field02 = 0;        // +0x02
  unsigned int m_id = 0;      // +0x04
};

// 'EATC' resource: the effects attached to an object state or an animation event.
struct REffectsAttachment : public EResource {
  REffectsAttachment();  // 80322400
  SIMMY_STORABLE_DECL(REffectsAttachment)
  ~REffectsAttachment() override;  // 80322450
  void OldLoad(EFile* file);  // 803224F4
  void Load(EFile* file);  // 80322780
  using EResource::Load;
  void Refresh(EFile *) override;  // 803228AC
  static void RegisterType(unsigned short);  // 80322B00
  void CreateCopy() const;  // 80322B54

  std::vector<AttachmentNode> m_nodes;  // +0x14
};

// What a particle list is drawn with (0xA0 bytes; ignored by the GC renderer, ENgcRC::ParticleListInfo
// is empty): -colour key 1, -colour key 2, -size key, the emitter's flags; 1 / the colour segments
// (and 0); 1 / the size segments; the sprite size x scale; the four colour keys; the three sizes; the
// rotation; the emitter's position (local space) or 0, the gravity factor; the world's gravity; 0.5.
struct EGEParticleListInfo {
  float m_data[40];
};
// One sprite as handed to the renderer (0x40 bytes).
struct EGEPackedParticle {
  float m_pos[3];      // +0x00
  float m_rot;         // +0x0C
  float m_half[2];     // +0x10 half width, half height
  unsigned int m_flip; // +0x18
  float m_pad1C;
  unsigned int m_color[4];  // +0x20 rgba (0..255; the renderer takes the low bytes, alpha halved)
  float m_uv[4];       // +0x30 u0, v0, u1, v1
};
static_assert(sizeof(EGEPackedParticle) == 0x40, "EGEPackedParticle");

// The level instance of a pemitter (0xC4 bytes).
struct Emitter : public EInstance {
  Emitter();  // 802386A4
  ~Emitter() override;  // 8023872C
  int Create(const char* name, const float* matrix, pemitterinfo* info, ParticleSubmitFn submit,
             EmitterCallbackFn process, ParticleBirthFn birth, void* data);  // 802383EC
  void Destroy();  // 80238480
  void Reg(psystem* sys, bool level);  // 802384B0
  void UnReg(psystem* sys, bool level);  // 8023850C
  void SetMatrix(const float* m);  // 80238570
  void SetState(int bits, int on);  // 80238608
  int GetNumActiveParticles();  // 8023867C
  pemitter* Get();  // 8023869C
  void SetCallbacks(ParticleSubmitFn submit, EmitterCallbackFn preProcess, EmitterCallbackFn process,
                    ParticleBirthFn birth, void* data, EmitterCallbackFn postDestroy, void* postDestroyData,
                    EmitterCallbackFn interval, void* intervalData, unsigned int keep = 0);  // 8023878C
  // SetCallbacks' `keep`: the arguments to leave as they are (GC: passed as 0xFFFF), bit 0 the first.
  void EmitterSetPostDestroyCallback(EmitterCallbackFn fn, void* data);  // 8023884C
  void EmitterSetIntervalCallback(EmitterCallbackFn fn, void* data);  // 802388B4
  void Reset();  // 80238928
  void SetSortPos(EVec3& pos);  // 80238954
  void SetScale(float scale);  // 80238974
  static void PEmitterSubmitParticlesCB(ELevelDrawData& data, EOrderTableData* otd);  // 80238980
  unsigned int VisibilityTest(E3DWindow& win) override;  // 802389D4
  void Draw(ELevelDrawData& data) override;  // 802389DC
  const char* GetInstName(EInstance__InstType& type) override;  // 80238B08

  pemitter* m_emitter = nullptr;   // +0x7C
  float m_sortPos[3] = {};         // +0x80
  EOrderTableData m_otd;           // +0x8C
};

struct EmitterSpr3d : public Emitter {
  int Create(const char* name, const float* matrix, pemitterinfo* info);  // 80239BC4
  int CreateCameraFollowXYZ(const char* name, const float* matrix, pemitterinfo* info);  // 80239C78
  int CreateCameraFollowXY(const char* name, const float* matrix, pemitterinfo* info);  // 80239D40
  void Destroy();  // 80239E08
};
struct EmitterGeom3d : public Emitter {
  int Create(const char* name, const float* matrix, pemitterinfo* info);  // 8023A134
};
struct EmitterGeom3dTab : public Emitter {
  int Create(const char* name, const float* matrix, pemitterinfo* info);  // 8023A3F4
};

// One effect of an object or animation (0x70 bytes): a particle Emitter or an Effects::Effect,
// loaded asynchronously and placed each frame.
// Starts with the client parameters an effects emitter is made with (its matrix, the node's sort
// layer, the sort point, whether it sorts by the emitter).
struct EParticleEffect : public Effects__Effect__ClientParams {
  EParticleEffect(AttachmentNode& node, EMat4& m);  // 8004B8E0
  ~EParticleEffect();  // 8004BC6C
  int UpdateParticleEffectLoad();  // 8004BA38
  void SetPos(EMat4& m, EVec3* sortPos);  // 8004BDAC
  void SetAlphaFade(float alpha);  // 8004BFE4

  Emitter* m_emitter = nullptr;   // +0x58
  Effects__Effect* m_effect = nullptr;  // +0x5C
  unsigned int m_id = 0;          // +0x60
  int m_type = 0;                 // +0x64 0 particle emitter, 1 effects emitter
  RParticle* m_particle = nullptr;  // +0x68
  REffectsEmitter* m_effectsEmitter = nullptr;  // +0x6C
};

// The effects of one attachment (0x10 bytes).
struct EParticleObj {
  EParticleObj();  // 8004C130
  ~EParticleObj();  // 8004C150
  int CreateEffects(EMat4& m, float alpha, ObjAnimDef* def);  // 8004C1F8
  void UpdateEffectPosAndAlphaFade(EAnimController& anim, float rot, EMat4& m, EVec3* sortPos, float alpha);  // 8004C350

  std::vector<EParticleEffect*> m_effects;  // +0x00 ENodeList
  int m_owns = 1;                          // +0x08
  REffectsAttachment* m_attachment = nullptr;  // +0x0C
};
void ParticleManDelRefCB(void* resource);  // 8032B638
