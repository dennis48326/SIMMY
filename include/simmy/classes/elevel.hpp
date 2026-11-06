// @class DigestGeomShaderPtr after EResource ERShader EShader ERModel E3DWindow ELights EMat4 EVec3 EDL
// @class EInstance__InstType
// @class EInstance
// @class EIStaticSubModel
// @class EILight
// @class EIDirLight
// @class EIAmbLight
// @class ELightGrid__LightData
// @class ELightGrid__LightWeight
// @class ELightGrid__PortalData
// @class ELightGrid__VertexData
// @class ELightGrid__RoomData
// @class ELightGrid
// @class EOrderTableData
// @class EOrderTableEntry
// @class ELevelDrawData
// @class SkyLights
// @class LevelLightingEntry
// @class LevelLighting
// @class LevelLightingTuning
// @class ERLevel
// Levels (game/e_rlevel.cpp, e_levelman.cpp) and what they hold: instances (game/e_instance.cpp; static
// sub-models game/e_istaticsubmodel.cpp), lights (e_ilight.cpp, e_idirlight.cpp, e_iamblight.cpp) and
// the light grid (e_lightgrid.cpp).
//
// A level resource is an object graph: an ERLevel and its instances (for the neighbourhood world map,
// one EIStaticSubModel per sub-model of the "worldmap" model). The level keeps the instances in an
// array (sky domes first, then terrain, then the rest; objects added at run time go after the loaded
// ones), lights in a list, and per room the instances, floors and draw lists. Drawing culls the
// instances against the 3D window, fades them, and lets each draw itself: opaque geometry at once,
// alpha-sorted geometry into an order table drawn back to front afterwards. The light grid gives each
// room its ambient and sun light (and point/portal lights per grid vertex); outside light follows a
// day/night table (LevelLighting).
#pragma once
#include <utility>
#include <vector>
#include "simmy/enum.hpp"

struct ELevelDrawData;
struct EOrderTableData;

// A sub-model shader's draw record (0x10 bytes): the node moving it, its shader and flags, its strips.
struct DigestGeomShaderPtr {
  void UpdateShaderFlags(bool& alpha);  // 80227530
  short m_node = -1;              // +0x00 the node that moves the sub-model (its field +0x0E), -1 none
  unsigned short m_nodeFlags = 0; // +0x02 8: the node is hidden this frame (animated models)
  ERShader* m_shader = nullptr;   // +0x04
  unsigned int m_flags = 0;       // +0x08 the EShader's surface flags (+0x8000: blended)
  EDL* m_dl = nullptr;            // +0x0C
  unsigned int m_srcShaderId = 0; // PC: the sub-model shader's own id (GC: walked alongside, +0x0C)
};

struct EInstance__InstType {
  SIMMY_ENUM_BODY(EInstance__InstType)
  static constexpr int kStaticModel = 4;
};

// 0x80 bytes. GC: the vtable pointer at +0x00.
struct EInstance : public EStorable {
  using InstType = ::EInstance__InstType;
  enum Flags : unsigned int {
    kUpdated = 0x100,           // in the level's update tree (Update each frame, by GetUpdatePriority)
    kReadByLevel = 0x2000,      // came with the level file
    kFloor = 0x8000,            // registers floor draw records (RegisterFloor)
    kRoomFloor = 0x10000,       // a room's floor: one per room
    kInRoomList = 0x20000,      // listed per room
    kListed = 0x40000,          // kept in the level's extra list instead of the array
    kLight = 0x80000,
  };
  using LocationFn = void (*)(unsigned int, bool, short&, short&);

  EInstance();  // 80228290
  SIMMY_STORABLE_DECL(EInstance)
  virtual ~EInstance();  // 80228370
  virtual void Write(EStream &);  // 802283C8
  virtual void Read(EStream &);  // 80228468
  virtual void Load(EFile& file);  // 802285C0
  void SetBounds(EBound3 &);  // 8022877C
  virtual void GetBoundSphere(EBoundSphere &);  // 802287F4
  void RemoveFromLevel();  // 80228818
  void ShadowDirAtPoint(EVec3 &, EVec3 &, int);  // 802288D8
  void CalcShadowLight(EVec3 &, short);  // 80228A74
  void CalcShadowLightWeighted(EVec3 &, short);  // 80228BA0
  RET_UNKNOWN ShadowDirAtPointWeighted(EVec3 &, EVec3 &, int);  // 80228CB4
  void CalcLights(EVec3& pos, ELights& out, bool otherSide);  // 80228F00
  void CalcLights4(EVec3 &, ELights &, bool);  // 8022907C
  void InsertInOrderTable(EOrderTableData& data);  // 802291F8
  static void SetContext(int context);  // 8022921C
  static int GetContext();  // 80229224
  void SetInstAlpha(float alpha);  // 8022922C
  float GetInstAlpha();  // 80229270
  static void RegisterType(unsigned short);  // 802293E8
  void CreateCopy() const;  // 8022943C
  virtual void Init();  // 8022945C
  virtual void Update();  // 80229460
  virtual void DoAnimation();  // 80229464
  virtual void DoLightingCalculation();  // 80229468
  virtual unsigned int VisibilityTest(E3DWindow& win);  // 8022946C
  virtual void Draw(ELevelDrawData& data);  // 80229474
  virtual void DrawStencil(ELevelDrawData& data);  // 80229478
  virtual void DrawShadow(ELevelDrawData& data);  // 8022947C
  virtual void DrawWireFrame(ERC* rc);  // 80229480
  virtual void SetOrient(EMat4& m);  // 80229484
  virtual void RebuildShaders();  // 80229488
  virtual const char* GetInstName(EInstance__InstType& type);  // 8022948C
  virtual int GetUpdatePriority();  // 802294A0
  virtual void RegisterFloor();  // 802294A8
  void ResetLocation();  // 802294AC
  void GetLocationId(short& room, short& otherSide);  // 80229528
  void GetOtherSide(short &, short &);  // 80229574
  void SetPortalType(EPortalType);  // 802295C4
  RET_UNKNOWN GetPortalType();  // 802295CC
  void RegisterOtherSideFn(unsigned int, void (*)(unsigned int, bool, short &, short &));  // 802295D4
  RET_UNKNOWN GetThisSideData();  // 802295E0
  RET_UNKNOWN GetThisSideFn();  // 802295E8
  RET_UNKNOWN GetOtherSideData();  // 802295F0
  RET_UNKNOWN GetOtherSideFn();  // 802295F8
  void RegisterThisSideFn(unsigned int data, LocationFn fn);  // 80229600
  void RegisterHiddenFn(unsigned int, bool (*)(unsigned int));  // 8022960C
  RET_UNKNOWN IsHidden();  // 80229618
  void SetShadowInterpolated(bool);  // 80229654
  virtual void GetShadowCenter(EVec3 &) const;  // 8022965C
  RET_UNKNOWN GetBounds() const;  // 80229660
  virtual void ReadInstanceData(EStream &, bool);  // 80229668
  virtual RET_UNKNOWN CanReloadInstanceData();  // 8022966C
  virtual ERLevel* GetLevel();  // 80229674
  virtual void GetOwner();  // 8022967C
  virtual RET_UNKNOWN GetNumRoomID();  // 80229680
  virtual RET_UNKNOWN GetRoomID(unsigned int);  // 80229688
  virtual void GetVelocity(EVec3 &);  // 80229690
  virtual void GetPos(EVec3 &);  // 802296A8
  virtual void GetOrient(EMat4 &);  // 802296C0
  virtual int HasModifiableColor();  // 802296E4
  virtual void ModifyColor(unsigned int color);  // 802296EC
  virtual RET_UNKNOWN GetModifiableColor(unsigned int);  // 802296F0
  virtual void AddedToLevel(ERLevel* level);  // 802296F8
  virtual void AboutToBeRemovedFromLevel(ERLevel* level);  // 802296FC
  virtual void RemovedFromLevel(ERLevel* level);  // 80229700
  RET_UNKNOWN IsShadow();  // 80229704
  RET_UNKNOWN GetShadowOwner();  // 8022971C
  RET_UNKNOWN IsWall();  // 80229724
  void ShouldInterestFade(bool);  // 8022972C
  virtual void RealizeShaderTuning();  // 80229740
  // GC: a pure slot (+0x154) the order-table callbacks call for the shader's modifiable colour.
  virtual unsigned int GetModifiableColor() { return 0; }

  ERLevel* m_level = nullptr;         // +0x04
  unsigned int m_shaderFlags = 0;     // +0x08 a sub-model shader uses blending flag 0x20
  int m_field0C = 0;                  // +0x0C
  int m_wall = 0;                     // +0x10 a wall part (ERLevel::AddWallInstanceToLevel)
  int m_field14 = 0;                  // +0x14
  EInstance* m_owner = nullptr;       // +0x18 the instance it is part of (lit and culled with it)
  LocationFn m_thisSideFn = nullptr;  // +0x1C the room the instance is in
  LocationFn m_otherSideFn = nullptr; // +0x20 (portals: the room on the other side)
  unsigned int m_otherSideData = 0;   // +0x24
  unsigned int m_thisSideData = 0;    // +0x28
  unsigned int m_hiddenData = 0;      // +0x2C RegisterHiddenFn
  bool (*m_hiddenFn)(unsigned int) = nullptr;  // +0x30 says the instance is hidden
  void* m_nameNode = nullptr;         // +0x34 node in the level's update tree
  EStorable* m_boundNode = nullptr;   // +0x38 bound tree node (old level versions)
  float m_bounds[6] = {};             // +0x3C EBound3: min, max
  int m_roomIndex = -1;               // +0x54
  unsigned int m_id = 0;              // +0x58 name id (level id map)
  unsigned int m_flags = 0xf;         // +0x5C Flags
  float m_alpha[2] = {1.0f, 1.0f};    // +0x60 per context (SetContext): fade alpha
  int m_fadeMode = 1;                 // +0x68 1: fade in and out at _dt; 0: never; else fast
  int m_interestFade = 1;             // +0x6C
  int m_cullable = 1;                 // +0x70 test against the view
  int m_field74 = 0;                  // +0x74
  void* m_listNode = nullptr;         // +0x78 node in a level list

  static int s_context;               // _instanceContext
};

// One sub-model of a model as an instance (0xA8 bytes).
struct EIStaticSubModel : public EInstance {
  EIStaticSubModel();  // 8022E360
  SIMMY_STORABLE_DECL(EIStaticSubModel)
  ~EIStaticSubModel() override;  // 8022E3F8
  void Deallocate();  // 8022E45C
  void Write(EStream &) override;  // 8022E47C
  void Read(EStream &) override;  // 8022E508
  void Load(EFile& file) override;  // 8022E5A8
  void GetBoundSphere(EBoundSphere &) override;  // 8022E67C
  void DeallocateModel();  // 8022E6A4
  void SetupModel();  // 8022E738
  void BuildDigests();  // 8022E788
  unsigned int VisibilityTest(E3DWindow& win) override;  // 8022E8AC
  void DrawAsShadow(ELevelDrawData &);  // 8022E8D8
  void Draw(ELevelDrawData& data) override;  // 8022E9EC
  void DrawWireFrame(ERC *) override;  // 8022EF24
  static void SkyOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 8022F0A4
  static void FaceOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 8022F168
  static void OrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 8022F384
  static void FloorOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 8022F448
  int HasModifiableColor() override;  // 8022F50C
  void ModifyColor(unsigned int color) override;  // 8022F588
  unsigned int GetModifiableColor() override;  // 8022F590
  void RebuildShaders() override;  // 8022F598
  void RealizeShaderTuning() override;  // 8022F634
  const char* GetInstName(EInstance__InstType& type) override;  // 8022F668
  static void RegisterType(unsigned short);  // 8022F820
  void CreateCopy() const;  // 8022F874
  RET_UNKNOWN GetBoundSphere();  // 8022F910

  int m_subModel = 0;                 // +0x7C index in the model
  float m_sphere[4] = {};             // +0x80 bound sphere: centre, radius (InstanceData)
  unsigned int m_modelId = 0;         // +0x90
  unsigned int m_color = 0;           // +0x94 modifiable colour
  std::vector<DigestGeomShaderPtr> m_digests;  // +0x98 one per sub-model shader (+0x9C count)
  ERModel* m_model = nullptr;         // +0xA0
  int m_fieldA4 = 0;                  // +0xA4
};

// 0xAC bytes.
struct EILight : public EInstance {
  enum Type { kNone = 0, kAmbient = 1, kDirectional = 2, kPoint = 3, kPortal = 4 };
  EILight();  // 80227984
  SIMMY_STORABLE_DECL(EILight)
  void Write(EStream &) override;  // 80227A2C
  void Read(EStream &) override;  // 80227B40
  void Init() override;  // 80227C5C
  void GetScaledIntColor(float, unsigned int *);  // 80227C90
  EVec3 GetScaledIntensity();  // 80227D64
  RET_UNKNOWN TestOverlap(EInstance *);  // 80227DE0
  static void RegisterType(unsigned short);  // 80228050
  void CreateCopy() const;  // 802280A4
  // The light's direction (or position), range, falloff and colour times intensity.
  virtual void LightingParameters(EVec3& dir, float& range, EVec3& falloff, EVec3& color);  // 802280C4
  virtual void CalcFullIntensityLightOnPoint(EVec3 &, float &);  // 802280C8
  virtual void Setup();  // 802280CC
  virtual int GetLightType();  // 802280D0
  void SetIntensityScale(EVec3 &);  // 802280D8
  void UseScaleIntensity(bool);  // 802280F8
  virtual void GetPosition(EVec3 &);  // 80228100
  void GetIntensityScale();  // 8022813C
  void SetIntensity(float);  // 80228160
  double GetIntensity() const;  // 80228168
  void SetColor(EVec3 &);  // 80228170
  RET_UNKNOWN GetColor() const;  // 80228190
  void Enable(bool);  // 80228198
  virtual RET_UNKNOWN IsEnabled();  // 802281A0
  virtual double GetFalloffEnd();  // 802281A8
  static void SetHaveLightsChanged(bool);  // 802281B4
  static RET_UNKNOWN HaveLightsChanged();  // 802281BC

  int m_enabled = 1;                  // +0x7C
  int m_field80 = 0;                  // +0x80
  int m_field84 = 0;                  // +0x84
  int m_field88 = 1;                  // +0x88
  float m_intensity = 1.0f;           // +0x8C
  EVec3 m_color{1.0f, 1.0f, 1.0f};    // +0x90
  EVec3 m_intensityScale{1.0f, 1.0f, 1.0f};  // +0x9C
  int m_useIntensityScale = 1;        // +0xA8

  static int m_haveLightsChanged;     // EILight::m_haveLightsChanged
};

// The sun and other directional lights (0xB8 bytes).
struct EIDirLight : public EILight {
  EIDirLight();  // 802270AC
  SIMMY_STORABLE_DECL(EIDirLight)
  void Write(EStream &) override;  // 80227108
  void Read(EStream &) override;  // 80227144
  void LightingParameters(EVec3& dir, float& range, EVec3& falloff, EVec3& color) override;  // 80227190
  void Setup() override;  // 80227238
  static void RegisterType(unsigned short);  // 80227460
  void CreateCopy() const;  // 802274B4
  int GetLightType() override;  // 802274D4
  void SetDir(EVec3 &);  // 802274DC
  RET_UNKNOWN GetDir() const;  // 802274FC

  EVec3 m_dir{0.0f, 0.0f, -1.0f};     // +0xAC the direction the light shines in
};

struct EIAmbLight : public EILight {
  SIMMY_STORABLE_DECL(EIAmbLight)
  virtual void CalcLightOnPoint(EVec3 &, EVec3 &);  // 80226B98
  void LightingParameters(EVec3& dir, float& range, EVec3& falloff, EVec3& color) override;  // 80226C58
  void Setup() override;  // 80226D08
  static void RegisterType(unsigned short);  // 80226F64
  void CreateCopy() const;  // 80226FB8
  int GetLightType() override;  // 80226FD8
};

// A light as the grid evaluates it (0x20 bytes).
struct ELightGrid__LightData {
  void Clear();  // 80230F8C
  EVec3 m_dir;                        // +0x00 direction (or position)
  float m_range = 0;                  // +0x0C
  EVec3 m_color;                      // +0x10 colour times intensity
  EILight* m_light = nullptr;         // +0x1C
};

// A light's weight at a point (8 bytes): its index in the light table.
struct ELightGrid__LightWeight {
  short m_index = -1;                 // +0x00
  float m_weight = -1.0f;             // +0x04
};

// The (up to eight) lights nearest a portal light in the room beyond it (0x20 bytes).
struct ELightGrid__PortalData {
  ELightGrid__LightData* m_lights[8] = {};
};

// A vertex of the lighting grid (0x24 bytes): the rooms touching it and, per room, its four
// nearest lights (indices into the light table, -1 ended).
struct ELightGrid__VertexData {
  signed char m_rooms[4];             // +0x00 -1: none
  short m_lights[4][4];               // +0x04
};

// One room's lights (0x54 bytes).
struct ELightGrid__RoomData {
  void Flush();  // 8022FBC4
  void FindClosestLights(EVec3& pos, int first, int end, ELightGrid__LightData* lights, ELightGrid__LightWeight* out, int count);  // 802301AC
  void EvaluateAmbientAndDirectionalLights();  // 8023073C
  ELightGrid__LightData m_ambient;    // +0x00
  ELightGrid__LightData m_sun;        // +0x20
  float m_portalScale = 0;            // +0x40
  int m_portalFirst = 0, m_portalEnd = 0x100;  // +0x44 its portal lights in the light table
  int m_pointFirst = 0, m_pointEnd = 0x100;    // +0x4C and point lights
};

// GC: lives at ERLevel +0x18 (0x29C60 bytes: the light table, rooms, a 63x63 vertex grid of point
// light weights, and the outside light set).
struct ELightGrid {
  using LightData = ::ELightGrid__LightData;
  using LightWeight = ::ELightGrid__LightWeight;
  using PortalData = ::ELightGrid__PortalData;
  using RoomData = ::ELightGrid__RoomData;
  using VertexData = ::ELightGrid__VertexData;
  static constexpr int kMaxRooms = 0x28;
  static constexpr int kGridSize = 0x3f;  // vertices per side
  ELightGrid();  // 8022FA6C
  void Flush();  // 8022FAF4
  void AddRoomToVertex(int x, int y, int room);  // 8022FC14
  void Update(std::vector<EILight*>& lights, EInstance** instances, int instanceCount, int roomCount);  // 8022FCAC
  float ConnectPortalLights(ELightGrid__RoomData* room);  // 8022FFE0
  int EvaluatePointLights(int first, int end);  // 802302B4
  void AddLightsToVertices();  // 802303D4
  void EvaluateLights();  // 80230608
  void EvaluateOuterLotLights();  // 8023065C
  void EvaluateAmbientAndDirectionalLights();  // 802306E4
  void EvaluatePortalLights(int first, int end);  // 802307CC
  void EvaluatePortalLight(ELightGrid__LightData* light, ELightGrid__PortalData* portal);  // 80230838
  void GetLightWeightsAtLocation(EVec3& pos, int room, ELightGrid__LightWeight* out);  // 80230C2C
  const short* GetLightsAtLocation(EVec3& pos, int room) const;  // 80230E28
  static short m_lightVertexDummy[4];  // no point lights (all -1)
  int IsRoomAtLocation(EVec3& pos, int room);  // 80230EF0
  void EvaluateLamps();  // 80230FFC
  void EvaluateLampsOn();  // 80231040

  int m_dirty = 1;                    // +0x00 rebuild the rooms' light lists
  int m_stable = 0;                   // +0x04
  int m_rebuilds = 0;                 // +0x08
  int m_framesSince = 0;              // +0x0C
  int m_portalFirst = 0, m_portalEnd = 0;     // +0x10 portal lights in the light table
  int m_pointFirst = 0x100, m_pointEnd = 0x100;  // +0x18 point lights
  LightData m_lights[0x200];          // +0x20 0..0xFF portal lights, 0x100..0x1FF point lights
  PortalData m_portals[0x100];        // +0x4020 per portal light: the lights beyond it
  int m_roomCount = 0;                // +0x6020
  RoomData m_rooms[kMaxRooms];        // +0x6024
  VertexData m_vertices[kGridSize][kGridSize];  // +0x6D44 [x][y]
  ELights m_outside;                  // +0x29B68 the outer lot's light set
};

// One draw record of the order table (0x38 bytes).
struct EOrderTableData {
  using Callback = void (*)(ELevelDrawData&, EOrderTableData*);
  unsigned int m_flags = 0;           // +0x00 the shader's surface flags
  int m_sortKey = 0;                  // +0x04 the shader's sort layer
  float* m_pos = nullptr;             // +0x08 where it is (distance sorting)
  EMat4* m_matrix = nullptr;          // +0x0C model matrix
  ELights* m_lights = nullptr;        // +0x10
  Callback m_callback = nullptr;      // +0x14 draws it
  EDL* m_dl = nullptr;                // +0x18
  EAnimController* m_animController = nullptr;  // +0x1C animated: its node matrices (GC: also in
                                      // m_matrix, tagged with bit 0 when skinned)
  int m_skinned = 0;                  // PC: that tag bit
  ERShader* m_shader = nullptr;       // +0x20
  EInstance* m_instance = nullptr;    // +0x24
  EOrderTableData* m_next = nullptr;  // +0x28
  float m_distance = 0;               // +0x2C from the eye, in the ground plane
  float m_key = 0;                    // +0x30 m_sortKey
  unsigned int m_field34 = 0;         // +0x34 1: lit by the outside radiosity
};

struct EOrderTableEntry {
  EOrderTableEntry();  // 8023122C
  EOrderTableData* m_head = nullptr;  // +0x00
  int m_count = 0;                    // +0x04
};

// What an instance draws with (ERLevel +0x2A1A0). The "current" fields avoid redundant renderer calls.
struct ELevelDrawData {
  EMat4 m_skyMatrix;                  // +0x00 sky domes: scale 0.03 about the eye
  EMat4 m_shadowProj;                 // +0x40 projects onto the ground along the sun
  EVec3 m_eye;                        // +0x80
  EVec3 m_sunDir;                     // +0x8C
  ERC* m_rc = nullptr;                // +0x98
  E3DWindow* m_win = nullptr;         // +0x9C
  int m_noOrderTable = 0;             // +0xA0 draw blended geometry at once
  int m_fieldA4 = 0;                  // +0xA4
  ERShader* m_curShader = nullptr;    // +0xA8
  EMat4* m_curMatrix = nullptr;       // +0xAC
  EMat4* m_fieldB0 = nullptr;         // +0xB0 the view matrix loaded (the window's or m_shadowProj)
  EAnimController* m_curAnim = nullptr;  // +0xB4 whose node matrices are loaded
  ELights* m_curLights = nullptr;     // +0xB8
};

// The light of a time of day (SkyLights: 0x20 bytes; LevelLightingEntry: 0x64).
struct SkyLights {
  EVec3 m_dirColor;                   // +0x00
  float m_dirIntensity = 0;           // +0x0C
  EVec3 m_ambientColor;               // +0x10
  float m_ambientIntensity = 0;       // +0x1C
};
struct LevelLightingEntry {
  float m_time = 0;                   // +0x00 hour of the day
  SkyLights m_sky;                    // +0x04 sky dome lighting
  float m_sunPitch = 0;               // +0x24 degrees
  float m_sunYaw = 0;                 // +0x28 degrees
  EVec3 m_ambientColor;               // +0x2C
  float m_ambientIntensity = 0;       // +0x38
  float m_shadowAlpha = 0;            // +0x3C
  float m_field40 = 0;                // +0x40
  EVec3 m_sunColor;                   // +0x44
  float m_sunIntensity = 0;           // +0x50
  EVec3 m_field54;                    // +0x54
  float m_field60 = 0;                // +0x60
};
struct LevelLighting {
  LevelLightingEntry* m_entries = nullptr;  // +0x00 by time of day
  int m_count = 0;                    // +0x04
};

// Debug overrides of the outside lighting (0xC0 bytes; ERLevel::s_levelLightingTuning).
struct LevelLightingTuning {
  LevelLightingTuning();  // 80237A7C
  void ProcessOverrides(LevelLightingEntry& entry);  // 80237D04
  float m_field00[17] = {};           // +0x00
  int m_override = 0;                 // +0x44 use the values below instead of the table's
  SkyLights m_sky;                    // +0x48 (the table's values are copied here otherwise)
  float m_sunPitch = 0, m_sunYaw = 0; // +0x68
  float m_field70 = 0;                // +0x70
  EVec3 m_sunColor;                   // +0x74
  float m_sunIntensity = 0;           // +0x80
  EVec3 m_ambientColor;               // +0x84
  float m_ambientIntensity = 0;       // +0x90
  EVec3 m_field94;                    // +0x94
  float m_fieldA0 = 0;                // +0xA0
  float m_shadowAlpha = 0;            // +0xA4
  float m_fieldA8[6] = {};            // +0xA8
};

// A level (0x2E460 bytes on the GameCube).
struct ERLevel : public EResource {
  static constexpr int kMaxInstances = 0x800;
  ERLevel();  // 80231348
  SIMMY_STORABLE_DECL(ERLevel)
  ~ERLevel() override;  // 80231700
  void GetSunLight(EVec3 &, EDirLight &);  // 80231894
  void Write(EStream &) override;  // 802318F0
  void AddInstancesFromList(TNodeList<EInstance *> &);  // 80231B90
  void AddInstancesFromBoundTree(EStorable *);  // 80231BD0
  void Read(EStream &) override;  // 80231C7C
  void Load(EFile& file) override;  // 80232334
  void Refresh(EFile* file) override;  // 80232BB0
  using EResource::Refresh;
  static ERLevel* AllocAndLoadLevel(EFile& file);  // 80232D80
  void ClearJustReadByLevelFlags();  // 80232DF8
  void DestroyInstancesWithoutJustReadByLevelFlags();  // 80232E3C
  void DestroyInstancesOriginallyReadByLevel();  // 80232F84
  void Deallocate();  // 80233000
  void DeallocateSub();  // 80233094
  void InsertInstance(EInstance* instance, EInstance* after);  // 802331BC
  bool IsInstanceListKindaFull();  // 802331F8
  void InsertWall(EInstance *, bool);  // 80233210
  void RemoveWall(EInstance *);  // 8023326C
  void InsertLight(EILight* light);  // 802332C4
  void RemoveLight(EILight* light);  // 80233308
  void ValidateInstanceInLevel(EInstance* instance);  // 80233364
  void InsertSkydomeInstance(EInstance *);  // 80233368
  void AddInstanceToLevel(EInstance* instance);  // 80233454
  void AddInstanceToLevelHead(EInstance *);  // 8023353C
  void AddWallInstanceToLevel(EInstance *);  // 8023362C
  static void GroupWallInstances();  // 802337D0
  void PrepareInstance(EInstance* instance);  // 802337D4
  void AddInstanceToRoom(EInstance* instance);  // 80233824
  void RemoveInstanceFromRoom(EInstance* instance);  // 802338C8
  int GetRoomIndex(EInstance* instance) const;  // 80233964
  void NotifyInstanceMoved(EInstance *);  // 802339DC
  void FixInstances();  // 80233A38
  void FixInstanceList(TNodeList<EInstance *> &);  // 80233B44
  void AddInstanceToIdMap(EInstance* instance);  // 80233B9C
  void RemoveInstanceFromIdMap(EInstance* instance);  // 80233BD8
  void FindInstance(char *);  // 80233C50
  RET_UNKNOWN FindInstance(unsigned int);  // 80233C90
  void FindFirstInstance(unsigned int, EInstance **);  // 80233CCC
  void FindNextInstance(HTIteratorPtrType *, EInstance **);  // 80233CF4
  void Init() override;  // 80233D1C
  void UnregisterFloorOTDs(EInstance *);  // 80233EA4
  void RemoveInstance(EInstance* instance);  // 80233F10
  void SetBounds(EInstance* instance, EBound3& bounds);  // 80234090
  void AddBounds(EBound3 &, EBound3 &, bool &);  // 802341B0
  RET_UNKNOWN CalcBounds();  // 80234260
  void UpdateLightGrid();  // 80234368
  void Update();  // 802343A0
  static void CalcRadiusFromViewParams(float, float, float);  // 80234438
  void DrawObjectShadowsOutside(std::vector<EInstance*>& list);  // 80234508
  void DrawObjectShadowsInside(std::vector<EInstance*>& list);  // 80234740
  void DrawOrderTableSlot(EOrderTableData* head);  // 80234A70
  void DrawOrderTableSlotFast(EOrderTableData *);  // 80234AE4
  void DrawSortedOrderTable(EOrderTableData *);  // 80234BA4
  void Draw(ERC* rc);  // 80234C98
  int ObjectShouldBeOccluded(EInstance* instance);  // 80235DA4
  bool ObjectShouldBeOccluded(short room, float z);  // 80235F14
  void DrawSimShadow(int);  // 80235F58
  void UpdatePerRoomData();  // 80236268
  EOrderTableData* AllocAndCopyOTD(EOrderTableData& data);  // 80236288
  void RegisterFloor(EOrderTableData& data);  // 8023637C
  void InsertInOrderTable(EOrderTableData& data);  // 802363E0
  void InsertInDOFForegroundObjectList(EOrderTableData &);  // 80236488
  void DrawDOFForegroundObjects();  // 80236570
  void DrawOrderTableEntry(EOrderTableData* data, ERC* rc);  // 80236598
  void DrawOrderTable();  // 802367B8
  static RET_UNKNOWN IntersectBoundBox(EBound3 &, EVec3 &, EVec3 &);  // 80236930
  void DrawWireFrame(ERC *);  // 80236BEC
  void SetRoomCount(int count);  // 80236CA8
  void UpdateLightsIntensityScale(int, EVec3);  // 80236E88
  int IsRoomAt(EVec3& pos, int room);  // 80236F0C
  void InitializeLightingGrid();  // 80236F50
  void ResetLightLocations();  // 80236F74
  void OutsideLightsInit(float hour, LevelLightingEntry& entry);  // 80237784
  void OutsideLightsUpdate(float hour, LevelLightingEntry& entry);  // 802377AC
  void UpdateFloorLighting();  // 80237918
  void InsertHouseObject(EOrderTableData *);  // 802379A0
  void GetLightsAtLocation(EVec3& pos, int room, ELights& out);  // 802379B4
  static void RegisterType(unsigned short);  // 8023813C
  void CreateCopy() const;  // 80238190
  EIDirLight* GetSun();  // 80238214
  EIAmbLight* GetSunAmbient();  // 80238220
  void SetDayNightLightTable(LevelLighting* table);  // 8023822C
  void SetRainInfoTable(levelraininfo* info);  // 80238238
  void SetSunYawOffset(float degrees);  // 80238244
  void SetWallFadeState(EWallUpDownStateType);  // 80238250
  RET_UNKNOWN WallFadeState();  // 8023825C
  static void SetIdMapTableSize(int);  // 80238268
  static int IsDrawingOrderTable();  // 80238290
  RET_UNKNOWN IsInUse();  // 80238298
  int GetRoomCount();  // 802382A4
  void UpdateRooms(int);  // 802382B0
  void SetRainBlend(float);  // 802382E0
  RET_UNKNOWN GetStorableVersion() const;  // 802382EC
  void RefreshRoomAssignment(EInstance *);  // 802382F8
  void ClearWallIndexSaves();  // 80238334
  int GetNumInstances();  // 80238348
  EInstance* GetInstance(int index);  // 80238354
  RET_UNKNOWN GetNumVisibleInstances();  // 80238368
  RET_UNKNOWN GetVisInstance(int);  // 80238374
  // PC: the wall list is a std::vector (GC: TNodeList<EInstance*>).
  using DrawWallShadowsFn = void (*)(std::vector<EInstance*>&, EVec3&, ELevelDrawData&);
  void SetDrawWallShadowsFunc(DrawWallShadowsFn fn);  // 80238388

  static int m_fillingOrderTable;     // 804FEA18
  static int m_drawingOrderTable;
  static LevelLightingTuning s_levelLightingTuning;  // 80488B6C

  float m_shadowAlpha = 1.0f;         // +0x14
  ELightGrid m_lightGrid;             // +0x18
  ELights m_skyLights;                // +0x29C78 lights sky domes are drawn with
  int m_field29D70 = 0;               // +0x29D70
  int m_skyCount = 0;                 // +0x29D74 the first instances: sky domes
  int m_terrainCount = 0;             // +0x29D78 then terrain
  int m_count29D7C = 0;               // +0x29D7C then these (drawn after the room floors)
  std::vector<EInstance*> m_fadeInstances;  // +0x29D80 count, +0x29D84 instances (fade with the eye)
  std::vector<EILight*> m_levelLights;      // +0x29E4C count, +0x29E50 lights read with the level
  std::vector<EInstance*> m_instances5;     // +0x29FE0 count, +0x29FE4
  std::vector<EILight*> m_lights;           // +0x2A174 TNodeList<EILight*>: every light
  EOrderTableData* m_orderTable = nullptr;  // +0x2A180
  int m_orderTableCount = 0;                // +0x2A184
  float m_field2A188 = 0;                   // +0x2A188
  float m_depthScale = 0;                   // +0x2A18C
  float m_depthMin = 0;                     // +0x2A190
  float m_depthMax = 100.0f;                // +0x2A194
  int m_readVersion = 0;                    // +0x2A198
  ELevelDrawData m_drawData;                // +0x2A1A0
  int m_drawingWindow = 0;                  // +0x2A244 set once Draw has run
  int m_field2A258 = 0;                     // +0x2A258
  // +0x2A264 EHashTable (id -> instances), +0x2A274 ERedBlackTree (update priority -> instance),
  // kept sorted by priority (equal keys in insertion order)
  std::vector<std::pair<unsigned int, EInstance*>> m_idMap;
  std::vector<std::pair<unsigned int, EInstance*>> m_updateMap;
  std::vector<std::vector<EInstance*>> m_roomInstances;  // +0x2A280 per room (+1 for outside)
  std::vector<EInstance*> m_roomFloors;                  // +0x2A284
  std::vector<EOrderTableData*> m_roomOrderTables;       // +0x2A288
  std::vector<EInstance*> m_wallList;                    // +0x2A298 TNodeList
  std::vector<EInstance*> m_listed;                      // +0x2A2A4 TNodeList (kListed)
  int m_roomCount = 0;                                   // +0x2A2B4
  std::vector<EInstance*> m_instances;                   // +0x2A2B8 (count +0x2C2B8)
  int m_wallFirst = -1, m_wallLast = -1;                 // +0x2C2BC
  int m_firstObject = 0;                                 // +0x2C2C4 instances added after loading start here
  std::vector<EInstance*> m_visible;                     // +0x2C2CC (count +0x2E2CC) this frame's
  EOrderTableData* m_houseObjects = nullptr;             // +0x2E2D0
  int m_wallFadeState = 0;                               // +0x2E2D4 EWallUpDownStateType; 2: walls down
  DrawWallShadowsFn m_drawWallShadows = nullptr;         // +0x2E2D8
  EDL* m_shadowDL = nullptr;                             // +0x2E2DC
  ERShader* m_shadowSquare = nullptr;                    // +0x2E2E0
  ERShader* m_shadowCircle = nullptr;                    // +0x2E2E4
  float m_lightAlpha = 0;                                // +0x2E2E8
  EOrderTableData* m_dofForeground = nullptr;            // +0x2E2EC
  EIDirLight m_sun;                                      // +0x2E2F0
  EIAmbLight m_sunAmbient;                               // +0x2E3A8
  LevelLighting* m_dayNight = nullptr;                   // +0x2E454
  levelraininfo* m_rainInfo = nullptr;                   // +0x2E458
  float m_sunYawOffset = 0;                              // +0x2E45C
};
