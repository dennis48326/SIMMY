// @class Effects after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__Effect__ClientParams after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__FastParticlePacket after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__FastParticleParticleInitData after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__FastParticleQuad after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__FastParticleState after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__Effect after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__EffectsVisitor after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__FastParticleEmitter after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__EffectsManager__EffectEntry after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class Effects__EffectsManager after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class REffectsEmitter after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class EffectsEmitterManager after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class GameEffectsManager after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// @class EffectsPreRenderVisitor after EResource EResourceManager EMat4 EVec3 EVec4 FastAllocPool
// The newer effects system (engine/effect*.cpp, fastparticleemitter.cpp, reffects_emitter.cpp,
// effects_emitterman.cpp; game/gameeffectsmanager.cpp). See src/engine/fastparticleemitter.cpp.
//
// An effects emitter ('EEMT' resource in effectse.arc: REffectsEmitter) describes a burst particle
// emitter: how often it bursts and how many particles, where they are born (a ring / cone about the
// emitter, a box or a sphere), how fast, and how they look over their life (colour, alpha, size,
// spin, sheet frames) with one texture. Effects::EffectsManager keeps the running effects
// (Effects::Effect; the only kind the GC build makes is FastParticleEmitter: the sequencer kind
// returns nothing); GameEffectsManager puts each visible one into the level's order table, where it
// simulates and draws its particles as camera-facing quads (ERC::QuadList).
//
// The random table (Effects::PreComputeRand32Table) is shared with the older particle system
// (psystem.cpp).
#pragma once

#include <vector>

struct Effects {
  using Effect = ::Effects__Effect;
  using EffectsManager = ::Effects__EffectsManager;
  using EffectsSequencer = ::Effects__EffectsSequencer;
  using EffectsVisitor = ::Effects__EffectsVisitor;
  using FastParticleEmitter = ::Effects__FastParticleEmitter;
  using FastParticlePacket = ::Effects__FastParticlePacket;
  using FastParticleParticleInitData = ::Effects__FastParticleParticleInitData;
  using FastParticleQuad = ::Effects__FastParticleQuad;
  using FastParticleState = ::Effects__FastParticleState;
  static void PreComputeRand32Table();  // 803520DC
  static int PMRand();  // 8035214C
  static void SetSeed2(int seed);  // 80352190
  static ERTexture* GetTextureFromID(unsigned int id);  // 803521E0
  static ERC* GetDefaultRenderContext();  // 8035220C
  void GetFaceCameraRotation(ERC *, EMat4 &, EVec4 &, bool);  // 80352214
  static void FreeFastAllocPool(FastAllocPool* pool);  // 80352218
  static int InitializeEffectsSystem();  // 80352258
  static bool ShutdownEffectsSystem();  // 803522A0
  static void InitEffectsMath();  // 80352A10
  static void SinCosf(float a, float& s, float& c);  // 80352A14
  static void Mat4SetEulerAngles34(float x, float y, float z, EMat4& m);  // 80352B68
  static void Mat4Multiply34(EMat4& a, EMat4& b, EMat4& out);  // 80352EA0
  static void Mat4Copy34(EMat4& src, EMat4& dst);  // 8035301C

  static unsigned int gPrecomputedRand32Table[0x80];  // 804DCA00
  static unsigned int gPrecomputedTableIndex;         // 804FF400
  static const unsigned int kPreComputedTableSize = 0x80;  // 80501120
  // The next word of the precomputed table.
  static unsigned int NextRand32() {
    const unsigned int i = gPrecomputedTableIndex & (kPreComputedTableSize - 1);
    gPrecomputedTableIndex = i + 1;
    return gPrecomputedRand32Table[i];
  }
  // [0, 1) from the low 23 bits of a word (the GC's idiom: those bits as the mantissa of a float in
  // [1, 2), minus 1).
  static float UnitFromBits(unsigned int r) {
    union {
      unsigned int u;
      float f;
    } v;
    v.u = (r & 0x7fffff) | 0x3f800000;
    return v.f - 1.0f;
  }
  // A uniform random number in [0, 1) from the precomputed table.
  static float TableRand() { return UnitFromBits(NextRand32()); }

  static FastAllocPool* gFastParticlePacketsPool;    // 804FF410
  static int gsNumberFastParticlePacketsInPool;      // 804FF414
  static float s_minAlpha;                           // 804FF418 (10)
  static unsigned char s_minAlphaTestThreshold;      // 804FF41C (5)
  static float s_minSize;                            // 804FF420 (0.005)
  static float* gpCurrentMatrix1Ptr;                 // the emitter's world matrix while it bursts
};

// What the client of an effect hands over (0x50 bytes; EParticleEffect starts with one): where it is,
// how it sorts.
struct Effects__Effect__ClientParams {
  EMat4 m_matrix;                // +0x00 the parent transform
  short m_sortLayer = 0;         // +0x40 the order table's sort key
  float* m_sortPos = nullptr;    // +0x44 where it sorts (null: the emitter's own position)
  int m_dofForeground = 0;       // +0x48 drawn in the depth-of-field foreground list
  int m_sortByEmitter = 0;       // +0x4C sort at the emitter's position
};

// Base of the running effects (engine/effect.cpp). GC: the vtable at +0x144.
struct Effects__Effect : public Effects__Effect__ClientParams {
  using ClientParams = ::Effects__Effect__ClientParams;
  enum Flags : unsigned int {
    kDone = 0x1,             // finished (its client lets go once it sees this)
    kInfinite = 0x2,         // never ends by itself
    kKeepParentRotation = 0x4,
    kKeepLocalOffset = 0x100,  // the local matrix's translation counts
    kStopped = 0x200,
    kUpdate = 0x400,         // updated by the manager
    kRender = 0x800,         // drawn
    kReleased = 0x1000,      // its client let go: delete once done
    kDelete = 0x2000,        // the manager deletes it
  };
  Effects__Effect();  // 80351E20
  explicit Effects__Effect(ClientParams& params);  // 80351E7C
  virtual ~Effects__Effect() {}  // GC vtable +0x08
  void DefaultInit();  // 80351F84
  void SetParentTransform(EMat4& m);  // 80352014
  virtual void Update(float dt) = 0;  // GC vtable +0x10
  virtual void Render(ERC* rc) = 0;  // +0x18
  virtual void Stop() = 0;  // +0x20
  virtual int Accept(Effects__EffectsVisitor& v) = 0;  // +0x28

  EMat4 m_local;               // +0x50 the emitter's own rotation (and offset)
  EMat4 m_world;               // +0x90 m_local * m_parent (+0xC0: the emitter's position)
  EVec4 m_colorScale;          // +0xD0
  EVec3 m_fieldE0;             // +0xE0
  float m_fieldEC = 0;         // +0xEC the birth radius at start
  EVec4 m_prevPos;             // +0xF0 the position last frame
  EMat4 m_parent;              // +0x100
  unsigned int m_flags = 0;    // +0x140
};

// Visits the effects by kind (Effects::EffectsManager::ForEach*Effect). 0 stops the walk.
struct Effects__EffectsVisitor {
  virtual ~Effects__EffectsVisitor() {}
  virtual int Visit(Effects__Effect&) { return 1; }
  virtual int Visit(Effects__FastParticleEmitter&) { return 1; }
  virtual int Visit(Effects__EffectsSequencer&) { return 1; }
};

// An effects emitter's description, after the 0x90-byte birth data in the resource (0xF0 bytes;
// big-endian in the file, swapped field by field at load).
struct Effects__FastParticleEmitterInitData {
  float m_life;                // +0x00 a particle's life (s); <= 0: until the emitter goes
  float m_spin;                // +0x04 spin speed (rad/s)
  float m_size;                // +0x08 size at birth
  float m_sizeRandom;          // +0x0C
  float m_burstInterval;       // +0x10 seconds between bursts (< 0: one burst)
  float m_duration;            // +0x14 emits for this long (<= 0: for ever)
  float m_speed;               // +0x18
  float m_speedRandom;         // +0x1C
  float m_radius;              // +0x20 birth distance from the emitter
  float m_radiusRandom;        // +0x24
  float m_elevation;           // +0x28 the birth direction above the emitter's xy plane
  float m_elevationRandom;     // +0x2C
  float m_rotZ;                // +0x30 the emitter's own rotation (z, y), and its speed
  float m_rotY;                // +0x34
  float m_tiltX;               // +0x38
  float m_rotZSpeed;           // +0x3C
  float m_rotYSpeed;           // +0x40
  short m_maxBursts;           // +0x44 < 0: no limit
  short m_maxParticles;        // +0x46
  unsigned int m_flags;        // +0x48
  unsigned int m_flags2;       // +0x4C (sign bit: the emitter rotates)
  float m_box[3];              // +0x50 box / sphere dispersion extents
  float m_field5C;             // +0x5C
  float m_killTime;            // +0x60 every particle dies after this long (<= 0: never)
  short m_burstMin;            // +0x64 particles per burst
  short m_burstRange;          // +0x66
  float m_alphaRateRandom;     // +0x68
  float m_rotation;            // +0x6C a particle's rotation at birth
  float m_offset[3];           // +0x70 the emitter's offset from its parent
  float m_azimuthRange;        // +0x7C birth directions about the z axis
  float m_color[3];            // +0x80 a particle's colour at birth
  float m_rotationRandom;      // +0x8C
  float m_colorRange[3];       // +0x90
  float m_burstIntervalRandom; // +0x9C
  unsigned int m_fieldA0;      // +0xA0
  float m_inheritVelocity;     // +0xA4
  unsigned char m_fieldA8;     // +0xA8
  unsigned char m_fieldA9, m_fieldAA, m_fieldAB, m_fieldAC;
  unsigned char m_flipU;       // +0xAD 1: sheet frames mirrored (with a colour mode)
  unsigned char m_alphaTest;   // +0xAE the alpha test threshold (0..255)
  unsigned char m_fieldAF;
  float m_frameTime;           // +0xB0 DieOnLastFrame: one sheet frame's time
  float m_framesPerSecond;     // +0xB4
  float m_radiusSpeed;         // +0xB8 the birth radius grows at this rate...
  float m_radiusSpeedTime;     // +0xBC ...slowing to 0 over this long
  unsigned int m_textureId;    // +0xC0
  unsigned char m_tail[0x2C];  // +0xC4 (tool data, unused)
};

// How the particles change over their life (the first 0x90 bytes of the resource; 36 floats).
struct Effects__FastParticleParticleInitData {
  float m_accel[3];            // +0x00 drift velocity (the velocity decays towards it)
  float m_frameRandom;         // +0x0C (flag 0x20000000: per-particle frame offset)
  float m_colorRate[3];        // +0x10 colour change per second
  float m_alphaRate;           // +0x1C alpha change per second
  float m_colorMin[4];         // +0x20 clamps (0..255; alpha 0..128)
  float m_colorMax[4];         // +0x30
  float m_color0[4];           // +0x40 colour at birth
  float m_scaleU;              // +0x50 quad scale (+1) in u, v
  float m_scaleV;              // +0x54
  float m_spinTime;            // +0x58 spin slows over this long...
  float m_spinDamp;            // +0x5C ...by this much
  float m_drag;                // +0x60
  float m_sizeMax;             // +0x64
  float m_sizeRate;            // +0x68
  float m_velocityScale;       // +0x6C
  float m_frameRateU;          // +0x70 sheet frames per second
  float m_frameRateV;          // +0x74
  float m_fadeRate;            // +0x78 alpha limit: fadeRate * age + fadeBase
  float m_fadeBase;            // +0x7C
  float m_framesU;             // +0x80 sheet frames across, down
  float m_framesV;             // +0x84
  float m_frameU;              // +0x88 one frame's width, height in uv
  float m_frameV;              // +0x8C
};

// One particle's motion (0x20 bytes).
struct Effects__FastParticleState {
  float m_pos[3];              // +0x00 (flag 0x20000000: x's low 12 bits are a per-particle seed)
  float m_rot;                 // +0x0C rotation at birth (1024: slot free, flag 0x1000)
  float m_vel[3];              // +0x10
  float m_spin;                // +0x1C
};

// 16 particles (0x2E0 bytes, from the shared pool).
struct Effects__FastParticlePacket {
  float m_age[16];             // +0x000
  float m_size[16];            // +0x040 size at birth
  float m_alphaRate[16];       // +0x080
  Effects__FastParticleState m_state[16];  // +0x0C0
  unsigned short m_color[16];  // +0x2C0 RGB 5:5:5 x16 (bit 15: flip u)
};

// One quad being drawn (only the uvs and its slot are read).
struct Effects__FastParticleQuad {
  float m_corners[4][4];       // +0x00
  float m_field40[12];         // +0x40
  float m_u0, m_u1;            // +0x70
  float m_v0, m_v1;            // +0x78
  int m_index;                 // +0x80 the quad's number this frame
};

// engine/fastparticleemitter.cpp (0x1A8 bytes on GC).
struct Effects__FastParticleEmitter : public Effects__Effect {
  using InitData = ::Effects__FastParticleEmitterInitData;
  using BirthData = ::Effects__FastParticleParticleInitData;
  using Packet = ::Effects__FastParticlePacket;
  using State = ::Effects__FastParticleState;
  using Quad = ::Effects__FastParticleQuad;
  enum StateFlags : unsigned int {
    kNoBurst = 0x4,          // the burst interval is negative: bursts once
    kEmpty = 0x8,            // no particles and no more coming
    kBursting = 0x10,        // has burst at least once
    kSuspended = 0x800,      // no more bursts
    kMoved = 0x10000,
  };
  static void InitPlatform();  // 80353090
  static void InitModule();  // 80353094
  Effects__FastParticleEmitter();  // 803530C4
  explicit Effects__FastParticleEmitter(ClientParams& params);  // 8035312C
  ~Effects__FastParticleEmitter() override;  // 80353194
  void FreeParticlePackets();  // 80353298
  void SharedInitPart1();  // 80353344
  int SharedInitPart2();  // 80353398
  int ReallocateParticles(int count);  // 80353930
  int Init(REffectsEmitter& res);  // 80353C84
  float UpdateBurst(int count, float birth, int* added);  // 80353D08
  int AddBurst(int count, float birth);  // 80353EF4
  int InitResourceData();  // 80354838
  void RotateEmitter(float dt);  // 803548D4
  static void InitParticlePool();  // 80354978
  static int GetTotalPacketsInPool();  // 803549E0
  void ComputeCombinedMatrix();  // 803549E8
  void Update(float dt) override;  // 80354A5C
  void Stop() override;  // 80354FE0
  void ComputeDispersionPositionAndVelocity(State* s, float t, float dt, float birth);  // 8035504C
  float DieOnLastFrame_Lifetime(Packet* p, unsigned int i);  // 80355A38
  void Render(ERC* rc) override;  // 80355B34
  void DoSimulationAndRender(ERC* rc);  // 80355B9C
  int Accept(Effects__EffectsVisitor& v) override;  // 803562D0
  int ComputeMaxParticles();  // 8035630C
  float GetAlphaScale();  // 80356654
  void SetAlphaScale(float a);  // 8035665C
  void SuspendBurst();  // 80356664
  bool PreRollParticles();  // 80356680
  int AllocateParticlePackets(int count);  // 80356870
  void RenderQuad(ERC* rc, Quad& q, float angle, EVec4& color, float size, EVec4& pos, BirthData& bd);  // 80356954

  Packet* PacketOf(int slot) { return m_packets[slot >> 4]; }

  unsigned int m_state = 0;      // +0x148
  float m_alphaScale = 1.0f;     // +0x14C
  float m_lastDt = 0;            // +0x150
  float m_burstTimer = 0;        // +0x154 till the next burst
  float m_time = 0;              // +0x158
  int m_bursts = 0;              // +0x15C particles born so far
  int m_deaths = 0;              // +0x160
  short m_packetCount = 0;       // +0x164
  unsigned char m_field166 = 0;  // +0x166
  short m_maxParticles = 0;      // +0x168
  float m_field16C = 0;          // +0x16C
  float m_field170 = 0;          // +0x170
  float m_rotZ = 0, m_rotY = 0;  // +0x174
  float m_burstInterval = 0;     // +0x17C
  float m_rateScale = 1.0f;      // +0x180
  float m_radius = 0;            // +0x184 birth radius now
  float m_radiusSpeed = 0;       // +0x188
  InitData* m_init = nullptr;    // +0x18C
  BirthData* m_birth = nullptr;  // +0x190
  ERTexture* m_texture = nullptr;  // +0x194
  unsigned short m_count = 0;    // +0x198 live particles
  unsigned short m_head = 0;     // +0x19A the oldest
  Packet** m_packets = nullptr;  // +0x19C
  REffectsEmitter* m_resource = nullptr;  // +0x1A0
  EShader* m_shader = nullptr;   // +0x1A4

  static int s_renderBufferIndex;            // 804FF424
  static float s_vertArray[256][4];          // 804DCC50
  static float s_texCoordArray[256][2];      // 804DDC50
  static unsigned char s_colorArray[256][4]; // 804FD2BC (PC: RGBA bytes)
};

// A running effect in one of the manager's lists (0x0C bytes): frames since it was deleted.
struct Effects__EffectsManager__EffectEntry {
  int m_framesDeleted = 0;          // +0x00
  int m_deleted = 0;                // +0x04 scheduled for deletion
  Effects__Effect* m_effect = nullptr;  // +0x08
};

// The newer effects system's manager (engine/effectsmanager.cpp, 0x1C bytes): the active effects
// and those waiting to be deleted (three frames).
struct Effects__EffectsManager {
  using EffectEntry = ::Effects__EffectsManager__EffectEntry;
  using List = std::vector<EffectEntry*>;  // GC: TNodeList (ENodeList)
  Effects__EffectsManager();  // 803522E4
  ~Effects__EffectsManager();  // 80352318
  static Effects__EffectsManager* GetSingleton();  // 80352364
  int Initialize();  // 803523A0
  static int Shutdown();  // 803523C8
  void Update(float dt);  // 803523D0
  void Render(ERC* rc);  // 80352534
  int DeleteAll();  // 803525C0
  Effects__Effect* CreateEffect(REffectsEmitter& res, Effects__Effect__ClientParams* params);  // 80352634
  Effects__Effect* CreateEffect(REffectsSequencer& res, Effects__Effect__ClientParams* params);  // 80352710
  int DeleteEffect(Effects__Effect& effect);  // 80352718
  void ScheduleEffectForDeletion(EffectEntry* e);  // 80352798
  int AddEffect(Effects__Effect* effect);  // 803527D0
  bool AddEntryToList(EffectEntry* e, List& list);  // 80352860
  int RemoveEntryFromList(EffectEntry* e, List& list, size_t* at);  // 80352894
  EffectEntry* FindEffectInList(Effects__Effect* effect, List& list, size_t* at);  // 80352910
  int ForEachActiveEffect(Effects__EffectsVisitor& v);  // 80352950
  int ForEachDeletedEffect(Effects__EffectsVisitor& v);  // 80352974
  int ForEachEffectInList(Effects__EffectsVisitor& v, List& list);  // 80352998

  static Effects__EffectsManager* s_pSingleton;
  int m_initialized = 0;  // +0x00
  List m_active;          // +0x04
  List m_deleted;         // +0x10
};

// 'EEMT' resource (0x24 bytes): the birth data (0x90 bytes) then the description, in one block.
struct REffectsEmitter : public EResource {
  using InitData = ::Effects__FastParticleEmitterInitData;
  using BirthData = ::Effects__FastParticleParticleInitData;
  REffectsEmitter();  // 8036A7B4
  SIMMY_STORABLE_DECL(REffectsEmitter)
  ~REffectsEmitter() override;  // 8036A804
  void Deallocate();  // 8036A860
  void DelRefSubResources() override;  // 8036A8C4
  void AddRefSubResources() override;  // 8036A904
  int TryIncrementSubResources() override;  // 8036A964
  void Load(EFile* file);  // 8036A9D0
  using EResource::Load;
  void Refresh(EFile* file) override;  // 8036AAC4
  static void RegisterType(unsigned short);  // 8036AC78
  void CreateCopy() const;  // 8036ACCC
  static void Alloc(unsigned long);  // 8036ACEC
  static void AllocAligned(unsigned long, unsigned long);  // 8036AD1C
  static void Free(void *);  // 8036AD4C
  InitData* GetEmitterInitData() { return m_init; }  // 8036AD78
  BirthData* GetParticleBirthData() { return m_birth; }  // 8036AD80
  ERTexture* GetTexture() { return m_texture; }  // 8036AD88
  int Refreshed() { return m_refreshed; }  // 8036AD90
  void RefreshClear() { m_refreshed = 0; }  // 8036AD98

  InitData* m_init = nullptr;      // +0x14 the block + 0x90
  BirthData* m_birth = nullptr;    // +0x18 the block
  ERTexture* m_texture = nullptr;  // +0x1C
  int m_refreshed = 0;             // +0x20
};

struct EffectsEmitterManager : public EResourceManager {
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;  // 80328958
  EResource* AddRef(unsigned int id, EFile* file, int pos);  // 80328A18
  EResource* AddRef(char* name, EFile* file, int pos);  // 80328A38
};

// game/gameeffectsmanager.cpp: the game's side of the effects (the order table).
struct GameEffectsManager {
  GameEffectsManager();  // 8003E008
  static GameEffectsManager* GetSingleton();  // 8003E00C
  static void PreRender();  // 8003E048
  static void RenderCB(ELevelDrawData& dd, EOrderTableData* otd);  // 8003E094
  static GameEffectsManager* s_pSingleton;
};

// Puts each drawn effect into the level's order table (or the depth-of-field foreground list).
struct EffectsPreRenderVisitor : public Effects__EffectsVisitor {
  EffectsPreRenderVisitor();  // 8003E140
  ~EffectsPreRenderVisitor() override;  // 8003E154
  int Visit(Effects__FastParticleEmitter& e) override;  // 8003DED4
  int Visit(Effects__Effect&) override;  // 8003E188
  int Visit(Effects__EffectsSequencer&) override;  // 8003E190
  static EInstance s_DaveRemoveTheSuck;  // the order table's instance for effects (alpha 1)
};
