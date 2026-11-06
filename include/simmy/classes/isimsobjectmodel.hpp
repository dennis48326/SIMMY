// @class ISimInstance after EIGameInstance EAnimNote ObjAnimDef
// @class ISimsObjectModel
// @class ISimsWallObjectModel
// @class ISimsMultiTileObjectModel
// The 3D side of a simulation object (game/isiminstance.cpp, game/iobject.cpp): an EIStaticModel
// with its own EAnimController, tied to its cXObject and house. ISimsObjectModel runs the object's
// animation (ObjAnimDef: model, animation, shader) with the animation's events (sounds, particle
// effects, script events), lights and highlights.
// GC: ISimInstance also derives from IBaseSimInstance (its second vtable at +0x320); the port keeps
// the single EIStaticModel base, and +0x320 is padding.
#pragma once
#include <vector>

// 0x3C8 bytes.
struct ISimInstance : public EIStaticModel {
  virtual void SetPlacementError(bool);  // 800562F4
  virtual void Create(cXObject* xob, EHouse* house);  // 8005631C
  virtual void OrentSubObject(cXObject *);  // 80056320
  virtual void SetObjOrient();  // 80056324 (GC: IBaseSimInstance vtable 0x14)
  virtual void CreateShadow();  // 80056328
  virtual void InsertSubModelsInHouse(ERLevel* level);  // 8005632C
  virtual void RemoveSubModelsFromHouse(ERLevel* level);  // 80056330
  virtual void PropigateFlagsToSubModels();  // 80056334
  virtual void SetOutOfWorld();  // 80056338
  virtual void SetCarryOrient();  // 8005633C
  virtual void StartBurp(int);  // 80056340
  virtual EIStaticModel* GetShadow();  // 80056344
  void SetXOb(cXObject* xob);  // 8005634C
  cXObject* GetXOb();  // 80056354
  ISimInstance();  // 8005635C
  SIMMY_STORABLE_DECL(ISimInstance)
  ~ISimInstance() override;  // 80056424
  void SetHighlight(unsigned int, bool);  // 8005656C
  RET_UNKNOWN GetIsPerson();  // 80056594
  RET_UNKNOWN GetSelector();  // 800565E0
  virtual RET_UNKNOWN HasInteractions(int player);  // 80056628
  virtual RET_UNKNOWN GetObCenter();  // 80056738
  static void RegisterType(unsigned short);  // 800568F4
  void CreateCopy() const;  // 80056948
  virtual void CleanupStuff(ERLevel* level);  // 800569F0
  void SetCursFlags(unsigned int flags);  // 800569F4
  unsigned int GetCursFlags();  // 800569FC
  static void GetSimInstance();  // 80056A04
  virtual RET_UNKNOWN IsMultiTilePart();  // 80056A08
  unsigned int GetFlags();  // 80056A10
  int HasModel();  // 80056A18
  EAnimController* GetAnimController();  // 80056A30

  // m_cursFlags bits.
  enum : unsigned int { kHighlight = 1, kErrorLight = 4, kHighlight2 = 8 };

  int m_field320 = 0;                 // +0x320 (GC: IBaseSimInstance vtable)
  int m_field324 = 0;                 // +0x324
  cXObject* m_xob = nullptr;          // +0x328 the simulation object
  unsigned int m_cursFlags = 0;       // +0x32C highlight / placement-error lighting
  int m_field330 = 0;                 // +0x330
  EAnimController m_anim;             // +0x334
  int m_interactionsFrame[2] = {};    // +0x3AC per player: the frame HasInteractions looks again
  int m_hasInteractions[2] = {};      // +0x3B4 per player: what it found
  EILight* m_light1 = nullptr;        // +0x3BC lights the object owns (lamps)
  EILight* m_light2 = nullptr;        // +0x3C0
};

// 0x4C0 bytes.
struct ISimsObjectModel : public ISimInstance {
  ISimsObjectModel();  // 8004C500
  SIMMY_STORABLE_DECL(ISimsObjectModel)
  ~ISimsObjectModel() override;  // 8004C644
  double GetHeightOffset();  // 8004C7F0
  void InsertSubModelsInHouse(ERLevel* level) override;  // 8004C91C
  void PropigateFlagsToSubModels() override;  // 8004C96C
  void CleanupStuff(ERLevel* level) override;  // 8004C9D0
  void Update() override;  // 8004CBDC
  void UpdateParticle(ObjAnimDef *);  // 8004D598
  void SetupCharacter();  // 8004D6CC
  void InitBulb();  // 8004D8A0
  void UpdateBulb(ObjAnimDef *);  // 8004DCE4
  void SetSOMModel(unsigned int model);  // 8004DD1C
  void UpdateHighlightAnim();  // 8004DDEC
  void AnimStartImmediate(unsigned int anim);  // 8004DF34
  void AnimStartBlendIn(unsigned int anim);  // 8004DF84
  void KillAllAnimTracks();  // 8004DFD4
  void UpdateTracks();  // 8004E028
  void UpdateAnimTrack(ObjAnimDef* def, bool restart);  // 8004E15C
  void UpdateAnim(ObjAnimDef* def);  // 8004E538
  void RemoveSubModelsFromHouse(ERLevel* level) override;  // 8004E55C
  void ChageShader(unsigned int, unsigned int);  // 8004E618
  short GetAdjacentRoom(bool find);  // 8004E7DC
  void SetWallForPortal();  // 8004E868
  void CreateShadow() override;  // 8004EA54
  void SetPosStatic(EVec3 &, float);  // 8004EB6C
  void CalcOrient();  // 8004EB70
  void CalcDynamicOrient();  // 8004EC78
  void SetOutOfWorld() override;  // 8004ED20
  void StartBurp(int) override;  // 8004ED24
  // The object plays animation state `anim` (its data 0) to its end: 2 while playing, 1 done; with
  // `event`, the next script event of the animation is handed out first (0, the event in *event).
  int TryAnimateObject(short anim, short* event);  // 8004ED3C
  // The animation of state `anim` loaded ahead: 1 when it is in.
  int PreloadAnimation(short anim);  // 8004EF58
  void BlendHighlights(ELights *, bool *);  // 8004F02C
  void CalcLightsFromPos(ELights *, bool);  // 8004F658
  void DoLightingCalculation() override;  // 8004F8D8
  unsigned int VisibilityTest(E3DWindow& win) override;  // 8004FBA0
  int IsVisible();  // 8004FC24
  void UpdateAnimController();  // 8004FC8C
  void DoAnimation() override;  // 8004FD38
  void Draw(ELevelDrawData& data) override;  // 8004FE28
  void AnimateLights();  // 80050288
  void DrawBounds(ERC *);  // 80050418
  ObjAnimDef* GetAnimDef(int state, bool warn);  // 800504B8
  int processAnimEvents(ERAnim* anim, int from, int count, bool reverse);  // 80050630
  int animEventHandler(ERAnim* anim, EAnimNote& note, int index);  // 800507E8
  void SetInitalObjectState();  // 80050B00
  void Create(cXObject* xob, EHouse* house) override;  // 80050CA0
  void SetObjOrient() override;  // 80050D44
  void OrientSubObjects();  // 80050EB4
  virtual void OrentSubObject();  // 80050F14
  RET_UNKNOWN IsMultiTilePart() override;  // 80052DEC
  void ApplyMatrix(float angle, EVec3& pos, EVec3& scale);  // 80052E3C
  static void RegisterType(unsigned short);  // 80053D28
  void CreateCopy() const;  // 80053D7C
  EIStaticModel* GetShadow() override;  // 80053D9C
  RET_UNKNOWN GetDynamic();  // 80053DA4
  void SetDynamic(bool);  // 80053DB0
  RET_UNKNOWN GetPos();  // 80053DD8
  using EIStaticModel::GetPos;
  void SetPos(EVec3 &);  // 80053DE0
  RET_UNKNOWN GetDir();  // 80053E00
  void SetDir(EVec3 &);  // 80053E08
  double GetRot();  // 80053E28
  void SetRot(float);  // 80053E30
  RET_UNKNOWN GetILight();  // 80053E38
  RET_UNKNOWN ShouldAnimStartBlendIn(unsigned int);  // 80053E40
  RET_UNKNOWN ShouldAnimStartImmediate(unsigned int);  // 80053E60
  bool ShouldKillAllAnimTracks(unsigned int);  // 80053E74
  void UpdateModel(ObjAnimDef *);  // 800544D0
  void UpdateShader(ObjAnimDef *);  // 8005450C
  void StartNewAnimTrack(ObjAnimDef* def, bool restart);  // 8005455C
  void ProcessPropertyEventTags(ObjAnimDef *, float);  // 80054704

  // m_trackMode: how UpdateTracks runs the controller.
  enum { kTracksOff = 0, kTracksRunning = 1, kTracksBlending = 2 };

  EHouse* m_house = nullptr;          // +0x3C8
  EIWallPart* m_portalWall = nullptr; // +0x45C a window's wall part (SetWallForPortal; its other side's room)
  unsigned int m_objFlags = 0;        // +0x3CC bit 31: the controller is not re-enabled this frame
  int m_state = -1;                   // +0x3E4 the object's state (data 0) its ObjAnimDef is for
  int m_trackMode = kTracksOff;       // +0x3E8
  float m_trackTime = 0.0f;           // +0x3EC seconds since the track started
  float m_rot = 0.0f;                 // +0x3F0 heading (radians)
  float m_field3F4 = 60.0f;           // +0x3F4
  float m_hlTime[2] = {};             // +0x3F8 how long each player's highlight has pulsed
  int m_dataDir = 0;                  // +0x400 the object's direction (data 1) as of the last Update
  float m_position[3] = {};           // +0x404 where ApplyMatrix put it
  ERShader* m_stateShader = nullptr;  // +0x41C the shader ChageShader put on (referenced)
  unsigned int m_curModel = 0;        // +0x420 ObjAnimDef model
  unsigned int m_curAttach = 0;       // +0x424 ObjAnimDef effects attachment
  unsigned int m_curAnim = 0;         // +0x428 ObjAnimDef animation
  unsigned int m_curShaderSlot = 0;   // +0x42C
  unsigned int m_curShader = 0;       // +0x430
  std::vector<EParticleObj*> m_particles;  // +0x440 ENodeList of EParticleObj (owning)
  EIStaticModel* m_shadow = nullptr;  // +0x44C itself once CreateShadow made it cast a shadow
  std::vector<EInstance*> m_subModels;  // +0x450 inserted into the level with the object
  std::vector<EILight*> m_animLights; // +0x464 TArray
  float m_matrixAngle = 0.0f;         // +0x470 ApplyMatrix's angle
  float m_scale[3] = {1.0f, 1.0f, 1.0f};  // +0x480 ApplyMatrix's scale
  float m_animMsFraction = 0.0f;      // +0x48C
  int m_animMs = 0;                   // +0x490 animation time (ms; event times are in ms)
  int m_animatePrim = 0;              // +0x494 TryAnimateObject has started the animation
  int m_eventsLoop = 1;               // +0x498 end-of-animation handling still due
  int m_animDone = 0;                 // +0x49C
  int m_paused = 0;                   // +0x4A0 time does not run for the animation
  std::vector<int> m_scriptEvents;    // +0x4A4 the animation's script events (note type 6)
  unsigned int m_preloadAnim = 0;     // +0x4B4 an animation it holds a reference to
  EILight* m_bulb = nullptr;          // +0x460 its lamp (InitBulb): an EIPointLight or EISpotLight
  int m_bulbState = 0;                // +0x434 the state's bulb setting (ObjAnimDef +0x14: 1 on)
  void PlaceBulb(EMat4& m);           // PC: the lamp's light at the object (ApplyMatrix, wall SetObjOrient)
  ObjAnimDef m_animDefCopy;           // PC: GetAnimDef's result (GC: a pointer into the quickdata)
  int m_field4B8 = -1;                // +0x4B8
};

// An object hung on a wall (pictures, wall lamps): its tile's position pushed 0.45 toward the wall it
// faces, not scaled (0x4C0 bytes).
struct ISimsWallObjectModel : public ISimsObjectModel {
  ISimsWallObjectModel();  // 80051170
  SIMMY_STORABLE_DECL(ISimsWallObjectModel)
  ~ISimsWallObjectModel() override;  // 800511B8
  void CreateShadow() override;  // 80051218
  void Create(cXObject* xob, EHouse* house) override;  // 8005121C
  void SetObjOrient() override;  // 8005127C
  static void RegisterType(unsigned short);  // 80053FA0
  void CreateCopy() const;  // 80053FF4
};

// An object over several tiles (beds, tables, doors and windows): placed at the middle of its tiles
// (portals: at their own tile) (0x4C0 bytes).
struct ISimsMultiTileObjectModel : public ISimsObjectModel {
  ISimsMultiTileObjectModel();  // 80051A8C
  SIMMY_STORABLE_DECL(ISimsMultiTileObjectModel)
  ~ISimsMultiTileObjectModel() override;  // 80051AD4
  void Create(cXObject* xob, EHouse* house) override;  // 80051B34
  void SetObjOrient() override;  // 80051B94
  static void RegisterType(unsigned short);  // 80054134
  void CreateCopy() const;  // 80054188
};
