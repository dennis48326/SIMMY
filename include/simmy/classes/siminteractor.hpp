// @class Physics after EVec2 EVec3 EMat4 InteractorModule__Interactor ELights EOrderTableData PlumbBobModel ERModel ERShader
// @class PlumbBobStack after EVec2 EVec3 EMat4 InteractorModule__Interactor ELights EOrderTableData PlumbBobModel ERModel ERShader
// @class PlumbBob after EVec2 EVec3 EMat4 InteractorModule__Interactor ELights EOrderTableData PlumbBobModel ERModel ERShader Physics PlumbBobStack
// @class FadeSquare after EVec2 EVec3 EMat4 InteractorModule__Interactor ELights EOrderTableData PlumbBobModel ERModel ERShader
// @class InteractorModule__InteractorResourceSet after EVec2 EVec3 EMat4 InteractorModule__Interactor ELights EOrderTableData PlumbBobModel ERModel ERShader
// @class InteractorModule__InteractorVisualizer after EVec2 EVec3 EMat4 InteractorModule__Interactor ELights EOrderTableData PlumbBobModel ERModel ERShader InteractorModule__InteractorResourceSet
// @class InteractorModule__SimInteractor after EVec2 EVec3 EMat4 InteractorModule__Interactor ELights EOrderTableData PlumbBobModel ERModel ERShader FadeSquare Physics
// The classic ("cursor") control mode and what draws the players' interactors:
//  - Physics (game/physics.cpp): a point that moves toward a target with damping and a top speed.
//  - PlumbBob (game/plumbbob.cpp): the player's plumb bob (one per player, PlumbBob::s_pPlumbBob),
//    flown over the selected sim by the player's interactor and drawn by the interactor visualizer.
//  - FadeSquare and InteractorModule::SimInteractor (game/siminteractor.cpp): interactor type 1, the
//    "classic" control. The left stick moves a cursor over the lot (a tile square that leaves fading
//    squares behind, and a beam of light in the player's colour); the objects near it light up and A
//    opens their action menu there; the chosen actions queue up (the HUD's action queue shows). Z goes
//    back to direct control (DirectInteractor, type 0), whose Z comes here; Y brings the cursor back to
//    the sim. The mode is remembered per player in EGlobal +0xB8 (SimInteractor::OnStop sets it,
//    DirectInteractor::OnStop clears it) and taken up again when the interactors are made for the next
//    lot (InteractorModule::InitializeInteractorModule). PreGameState clears it on each game start, so on
//    GC (and here) the mode is not saved with the game; the options keep one camera-rotation inversion per
//    mode (OptionsRecon +0x18 classic, +0x24 direct: ESimsCam::HandleRotation).
//  - InteractorModule::InteractorVisualizer (game/interactorvisualizer.cpp) and InteractorResourceSet
//    (game/interactorresourceset.cpp): per player a draw record put into the house's order table
//    (EHouse::InsertHouseObject) whose callback has the player's interactor draw itself (the plumb bob,
//    the cursor's squares and beam), and per interactor the models, shaders and order-table records it
//    draws with.
// PC: the interactor manager and its input sessions are stand-ins (see the "PC interactor manager"
// functions below and UpdateDirectInteractors); the plumb bob's particle systems (Emitter) are not ported.
#pragma once
#include <utility>
#include <vector>

// 0x30 bytes.
struct Physics {
  Physics();  // 80058F8C
  ~Physics();  // 80058FD4
  int PhysicsMove(EVec3* target);  // 80058FFC
  int PhysicsGravitate(EVec3& target);  // 800593CC

  EVec3 m_pos;              // +0x00
  EVec3 m_vel;              // +0x0C
  EVec3 m_accel;            // +0x18
  float m_damping = 0;      // +0x24 the share of the velocity lost per update
  float m_maxSpeed = 0;     // +0x28
  float m_accelMag = 0;     // +0x2C PhysicsGravitate's pull
};

// Up to four saved plumb bob targets (0x44 bytes).
struct PlumbBobStack {
  void Push(PlumbBob* bob);  // 8005974C
  void Pop(PlumbBob* bob);  // 80059798
  void Reset();  // 800597E0
  int m_count = 0;          // +0x00
  struct Entry {
    EVec3 m_target;
    int m_owner = 0;
  } m_entries[4];           // +0x04
};

// 0x1FC bytes.
struct PlumbBob : public Physics {
  static void PlumbBobOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 800597EC
  PlumbBob();  // 80059838
  static void StartupAll();  // 800599D4
  static void ShutdownAll();  // 80059A84
  void Startup();  // 80059AF4
  void EnableDraw(bool on);  // 80059B78
  void Shutdown();  // 80059BEC
  ~PlumbBob();  // 80059C44
  void AddParticleSystem(unsigned int id);  // 80059CA0
  void UpdateParticleSystem(int player);  // 80059D90
  void DestroyParticleSystems();  // 8005A050
  void SetParticleDrawState(bool on);  // 8005A100
  void SetParticleVisibility(bool on);  // 8005A184
  void SetState(int state);  // 8005A218 (PlumbBob_MotionState)
  unsigned int SetModel(unsigned int id);  // 8005A2E0
  unsigned int SetShadow(unsigned int id);  // 8005A314
  void SetColor(EVec3* color);  // 8005A348
  void SetPlayerColor(int player);  // 8005A3BC
  void SetScaleFromTween(int player, float tween);  // 8005A3EC
  void BecomeTransitionIndicator();  // 8005A45C
  void ForceMorphToPlumbbob();  // 8005A468
  void StartPositiveRepMomentEffect();  // 8005A480
  void StartNegativeRepMomentEffect();  // 8005A4A4
  void Update(int player);  // 8005A4C8
  void DrawShadow(ERC* rc);  // 8005AD18
  void Draw(ERC* rc);  // 8005AD8C
  void ResetMomentum(bool keepZ);  // 8005AF94

  static PlumbBob* s_pPlumbBob[kMaxPlayers];  // 80500574: player 1's and player 2's (two players only; PC mod: 4 players: [2..3])

  // Physics                        +0x00
  ELights m_lights;                 // +0x30 the player colour (ambient = colour * 0.3) and one light
  EVec3 m_target;                   // +0x128 where it flies to
  EVec3 m_landVel;                  // +0x134 the speed it landed with (x 0.05)
  float m_morphScale = 1.0f;        // +0x140 grows and shrinks while the model changes
  float m_pulseScale = 1.0f;        // +0x144
  int m_pulseDir = 0;               // +0x148 the pulse's direction (0 up, -1 down)
  int m_morphDir = 0;               // +0x14C
  float m_morphTime = 0;            // +0x150 seconds in a reputation moment
  unsigned int m_flags = 0;         // +0x154 1 drawn, 2 bounced this update, 4/8 particle draw state,
                                    //        0x10 particles visible
  int m_state = 0;                  // +0x158 PlumbBob_MotionState: 0 still, 1 jump to target, 2 gravitate,
                                    //        3 fly to target, 4 landed, 5/6 bounce, 7 last bounce
  int m_pulse = 0;                  // +0x15C 1: the model pulses
  int m_morph = 0;                  // +0x160 0 plumb bob, 1 transition indicator, 2/3 rep moments
  PlumbBobModel* m_model = nullptr; // +0x164
  int m_bounces = 0;                // +0x168
  int m_owner = 0;                  // +0x16C
  PlumbBobStack m_stack;            // +0x170
  EOrderTableData m_otd;            // +0x1B4 (the shadow's draw record)
  std::vector<Emitter*> m_emitters; // +0x1EC its particle emitters, count +0x1F4
  int m_updates = 0;                // +0x1F8 updates since the interactor's last reset (one a frame)
};

// A tile square drawn under the classic cursor (0x1C bytes): when the cursor leaves a tile its square
// fades out, shrinking (or, fading in, growing from four times its size).
struct FadeSquare {
  FadeSquare();  // 8021CE00
  ~FadeSquare();  // 8021CE4C
  FadeSquare(const FadeSquare&) = delete;
  FadeSquare& operator=(const FadeSquare&) = delete;
  void StartDraw(EVec3& pos, bool fadeIn);  // 8021CEB0
  void StartFade();  // 8021CEE4
  void StopDraw();  // 8021CEF0
  void Update(float dt);  // 8021CF08
  bool ShouldDraw();  // 8021CFB0
  void Draw(ERC* rc, EVec3& color);  // 8021CFCC

  EVec3 m_pos;               // +0x00
  ERModel* m_model = nullptr;  // +0x0C 0x967660EA
  float m_time = 0;          // +0x10 0.5 when started, counts down while fading
  int m_fading = 0;          // +0x14
  int m_fadeIn = 0;          // +0x18
};

// The models, shaders and textures an interactor draws with (sorted by id) and its draw records.
struct InteractorModule__InteractorResourceSet {
  InteractorModule__InteractorResourceSet();  // 8020C09C
  ~InteractorModule__InteractorResourceSet();  // 8020C14C
  InteractorModule__InteractorResourceSet(const InteractorModule__InteractorResourceSet&) = delete;
  InteractorModule__InteractorResourceSet& operator=(const InteractorModule__InteractorResourceSet&) = delete;
  ERModel* AddModel(unsigned int id);  // 8020C354
  int RemoveModel(unsigned int id);  // 8020CCE0
  ERModel* GetModel(unsigned int id);  // 8020CE44
  ERShader* AddShader(unsigned int id);  // 8020D6A4
  int RemoveShader(unsigned int id);  // 8020E038
  ERShader* GetShader(unsigned int id);  // 8020E19C
  RET_UNKNOWN AddTexture(unsigned int);  // 8020EA0C
  RET_UNKNOWN RemoveTexture(unsigned int);  // 8020F3A0
  RET_UNKNOWN GetTexture(unsigned int);  // 8020F504
  void ClearSet();  // 8020FD74
  void AddOrderTableData(EOrderTableData& data);  // 8020FEF4
  EOrderTableData* GetOrderTableData(unsigned int index);  // 80210208
  int GetOrderTableDataCount();  // 80210240

  std::vector<std::pair<unsigned int, ERModel*>> m_models;    // +0x00
  std::vector<std::pair<unsigned int, ERShader*>> m_shaders;  // +0x14
  std::vector<unsigned int> m_textures;                       // +0x28 (not used by the ported interactors)
  std::vector<EOrderTableData> m_otds;                        // +0x6C
};

// 0x114 bytes (InteractorManager::s_pVisualizer).
struct InteractorModule__InteractorVisualizer {
  InteractorModule__InteractorVisualizer();  // 80213D8C
  ~InteractorModule__InteractorVisualizer();  // 80213DE0
  InteractorModule__InteractorVisualizer(const InteractorModule__InteractorVisualizer&) = delete;
  InteractorModule__InteractorVisualizer& operator=(const InteractorModule__InteractorVisualizer&) = delete;
  void Initialize();  // 80213E74
  void Shutdown();  // 80213F54
  bool CreateResources(InteractorModule__FloorPainter& in);  // 80213F98
  void DestroyResources(InteractorModule__FloorPainter& in);  // 802140E0
  void PreDraw(InteractorModule__FloorPainter& in);  // 80214100
  void Draw(InteractorModule__FloorPainter& in);  // 8021414C
  bool DrawFloorRectPreview(ERC* rc, EVec3& anchor, EVec3& cursor);  // 802147E4
  int DrawFloorRoomPreview(ERC* rc, EVec3& cursor);  // 80214B04
  bool CreateResources(InteractorModule__WallPainter& in);  // 80214E80
  void DestroyResources(InteractorModule__WallPainter& in);  // 80214F18
  void PreDraw(InteractorModule__WallPainter& in);  // 80214F38
  void Draw(InteractorModule__WallPainter& in);  // 80214F94
  void DrawWallpaperRoomPreview(InteractorModule__WallPainter& in);  // 80215218
  int DrawWallpaperPreviewOnAffectedWalls(InteractorModule__WallPainter& in);  // 802153F4
  bool CreateResources(InteractorModule__WallManipulator& in);  // 802155E8
  int DrawWallpaperRectPreview(ERShader* shader, EVec3& from, EVec3& to, float height);  // 8021577C (GC: EVec2s)
  int DestroyResources(InteractorModule__WallManipulator& in);  // 80215C14
  void PreDraw(InteractorModule__WallManipulator& in);  // 80215C34
  void Draw(InteractorModule__WallManipulator& in);  // 80215C90
  void DrawWallRectPreview(InteractorModule__WallManipulator& in);  // 80215F08
  void DrawVerticalRect(ERShader* shader, EVec3& from, EVec3& to, float height, int texGen, float uvStep);  // 80215FDC (GC: EVec2s)
  void DrawRoomPreview(InteractorModule__WallManipulator& in);  // 80216488
  int DrawPreviewOnAffectedWalls(InteractorModule__WallManipulator& in);  // 802166D0
  RET_UNKNOWN CreateResources(InteractorModule__ObjectManipulator &);  // 80216C90
  void DestroyResources(InteractorModule__ObjectManipulator &);  // 80216CE8
  void PreDraw(InteractorModule__ObjectManipulator &);  // 80216D08
  void Draw(InteractorModule__ObjectManipulator &);  // 80216D64
  bool CreateResources(InteractorModule__SimInteractor& in);  // 80216E98
  void DestroyResources(InteractorModule__SimInteractor& in);  // 80216ED8
  void PreDraw(InteractorModule__SimInteractor& in);  // 80216EF8
  void Draw(InteractorModule__SimInteractor& in);  // 80216F54
  int CreateResources(InteractorModule__DirectInteractor& in);  // 80217130
  int DestroyResources(InteractorModule__DirectInteractor& in);  // 80217138
  void PreDraw(InteractorModule__DirectInteractor& in);  // 80217140
  void Draw(InteractorModule__DirectInteractor& in);  // 8021719C
  RET_UNKNOWN CreateResources(InteractorModule__SocialModeInteractor &);  // 8021728C
  RET_UNKNOWN DestroyResources(InteractorModule__SocialModeInteractor &);  // 80217294
  void PreDraw(InteractorModule__SocialModeInteractor &);  // 8021729C
  void Draw(InteractorModule__SocialModeInteractor &);  // 802172F8
  static void InteractorOrderTableCallback(ELevelDrawData& data, EOrderTableData* otd);  // 802172FC
  void SetupDrawCallback(EHouse* house, int player);  // 80217388
  RET_UNKNOWN CreateSimpleResourceSet(InteractorModule__Interactor &, int);  // 802173E8
  RET_UNKNOWN DrawSimpleResourceSet(InteractorModule__Interactor &, int, EVec3 &, float);  // 80217438
  bool SelectShader(InteractorModule__Interactor& in, unsigned int id);  // 80217550
  void SetDefaultLights();  // 803C06D4 (inline in the header)
  RET_UNKNOWN Draw(InteractorModule__PlacementObject &, EVec3 &);  // 802175EC
  void DrawResource(InteractorModule__Interactor& in, unsigned int model, EVec3& color, EMat4* m);  // 80217B30
  InteractorModule__InteractorResourceSet* CreateResourceSet(InteractorModule__Interactor& in);  // 80217D00
  static RET_UNKNOWN AddModelToResourceSet(InteractorModule__Interactor &, int);  // 80218AD8
  RET_UNKNOWN AddOrderTableToResourceSet(InteractorModule__Interactor &, EOrderTableData &);  // 80218B8C
  int DestroyResourceSet(InteractorModule__Interactor& in);  // 80218C44
  InteractorModule__InteractorResourceSet* GetInteractorResourceSet(InteractorModule__Interactor* in);  // 80218DE4
  void DrawLitModel(ERModel* model, EVec3& color, EMat4* m);  // PC: DrawResource for a model held elsewhere

  // Draw state the PreDraw functions leave for the callback: the player and the interactor drawn.
  EOrderTableData* m_otd[kMaxPlayers] = {};   // +0x00 one draw record per player (PC mod: 4 players; GC [2])
  // +0x08 vector<pair<Interactor*, InteractorResourceSet>> sorted by interactor.
  std::vector<std::pair<InteractorModule__Interactor*, InteractorModule__InteractorResourceSet*>> m_sets;
  ELights m_lights;                 // +0x1C what DrawResource lights the model with

  static ERC* s_pERC;               // the render context the callback draws with
};

struct InteractorModule__SimInteractor : public InteractorModule__Interactor {
  InteractorModule__SimInteractor();  // 8021D250
  ~InteractorModule__SimInteractor() override;  // 8021D364
  virtual void OnCreate();  // 8021D3F4
  virtual void OnDestroy();  // 8021D428
  virtual void OnStart(int player);  // 8021D45C (InteractorParams: the player)
  virtual void OnStop();  // 8021D6DC
  void ParseControls();  // 8021D764
  virtual void Update(float dt);  // 8021D7D4
  virtual double GetBeamScale();  // 8021DF90
  void GetInteractorInfo(InteractorModule__InteractorInfo& info) override;  // 8021DFF4
  virtual void PreDraw(InteractorModule__InteractorVisualizer& v);  // 8021E018
  virtual void Draw(InteractorModule__InteractorVisualizer& v);  // 8021E044
  void OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand command, float value) override;  // 8021E070
  void OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand command) override;  // 8021E4CC
  void OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand command, float value) override;  // 8021E598
  void ResetInputState() override;  // 8021E5D8
  void ReturnPlumbBobToOwner();  // 8021E610
  void ImmediatelyCutCursorAndCameraToPos(EVec3& pos);  // 8021E6E0
  void TryCutToSim();  // 8021E72C
  virtual void ResetSnapTimer();  // 8021E890
  double GetSelectionRadius() override;  // 8021E8A8
  void UpdateOverlapIntersection(InteractorModule__Interactor__OverlapData* data) override;  // 8021E8E8
  void ExecuteActionMenu();  // 8021EA98
  void ReturnPlumbobToSim(PlumbBob* bob, EVec3& pos);  // 8021ECB0
  int IsSimulatorPaused();  // 8021ECBC
  void UpdateObjectHighlights(EIObjectMan* objects) override;  // 8021ED2C
  void SetupCamera();  // 8021EFC8
  RET_UNKNOWN ChooseAction(Interaction* action) override;  // 8021F00C
  void SetupCutToSim();  // 8021F0AC
  FadeSquare* GetFadeSquare(int i);  // 8021F0B8
  void SetPlumbBobShadowSave(unsigned int id);  // 8021F0C8
  unsigned int GetPlumbBobShadowSave();  // 8021F0D8

  // Interactor                      +0x00..+0x5C (vtable +0x5C)
  FadeSquare m_squares[10];          // +0x64
  int m_square = 0;                  // +0x17C the newest square
  float m_snapTime = 0;              // +0x180 seconds since the start (the beam grows in)
  float m_snapTimer = 0;             // +0x184 ResetSnapTimer
  int m_cutToSim = 0;                // +0x188 Y: the cursor (and camera) back to the sim
  int m_cut = 0;                     // +0x18C ImmediatelyCutCursorAndCameraToPos ran
  unsigned int m_shadowSave = ~0u;   // +0x190 the plumb bob's shadow model while it is away
  int m_particlesOn = 0;             // +0x194 the plumb bob's particles were started
  int m_snapped = 0;                 // +0x198
  PlumbBob* m_plumbBob = nullptr;    // +0x19C the player's
  Physics m_physics;                 // +0x1A0
  EVec3 m_overlapCentre;             // +0x1D0 the nearest overlapping object's centre (z: its top)
  float m_overlapDist = 3.4e38f;     // +0x1DC its squared distance
  float m_overlapRect[6] = {};       // +0x1E0 its bounds
  EVec3 m_prevOverlapCentre;         // +0x1F8
  float m_field204[3] = {};          // +0x204
  int m_state = 0;                   // +0x214 0 start, 1 moving the cursor, 2 in the action menu
  int m_field218[3] = {};            // +0x218
  int m_overlapId = 0;               // +0x224
  int m_field228 = 0;                // +0x228

  static float s_invalidPos;         // 80487E34 (and the next two words): -1000
};

// ---- PC interactor manager (stand-ins for InteractorModule::InteractorManager's active-interactor
// bookkeeping; the direct-control interactors themselves are made by UpdateDirectInteractors) ----

// The player's active interactor type: 0 direct control, 1 classic (SimInteractor), -1 none yet.
int PlayerInteractorType(int player);
// InteractorManager::ChangeActiveInteractor (inline on GC: the request bit at +0x3C and the pending
// pair at +0x2C): `type` takes over at the next update.
void ChangeActiveInteractor(int player, int type);
// PC keyboard and mouse (simmy::KbmTick, platform.hpp): the player whose controller is port 1 (-1 none),
// whose controls the mouse's record is for.
int KbmPlayer();
// The player's classic control is led by the mouse (it took the player there, or moved its cursor):
// the camera stops following the cursor (the mouse points where it wants), the stick goes back to
// direct control.
bool PcMouseClassic(int player);
void PcSetMouseClassic(int player, bool on);
// The floor point the mouse puts the classic cursor on this frame (taken once; SimInteractor::Update).
bool PcTakeMouseCursor(int player, EVec3& pos);
// The classic interactor's part of InteractorManager::UpdateActivePlayerInteractors: applies a pending
// change (OnStop / OnStart) and, when the player's interactor is the SimInteractor, feeds it the pad (its
// input session: 803FDC74 buttons, 803FDC9C sticks) and updates it. True when the classic interactor
// ran (the direct interactor then stays out).
bool UpdateSimInteractor(int player, float dt);
// The SimInteractor's cursor (InteractorManager::GetPlayerInteractorInfo while it is active).
bool SimInteractorInfo(int player, InteractorModule__InteractorInfo& info);
InteractorModule__SimInteractor* PlayerSimInteractor(int player);
// The visualizer (InteractorManager::s_pVisualizer), made with the first interactor.
InteractorModule__InteractorVisualizer* InteractorVisualizer();
// PC: the classic interactors, the visualizer and the plumb bobs go with the house.
void ShutdownSimInteractors();
// PC: the build/buy manipulators' draw record (InteractorVisualizer::PreDraw(ObjectManipulator&)) and
// drawing (Draw(ObjectManipulator&), Draw(PlacementObject&, EVec3&)): placemanipulator.cpp.
bool PlaceManipulatorPreDraw(int player, InteractorModule__InteractorVisualizer& v);
bool PlaceManipulatorDraw(int player, EOrderTableData* otd, InteractorModule__InteractorVisualizer& v);
