// @class CASTarget after UIObjectBase UIScreenManager DigestGeomShaderPtr CasSimDescriptionS2C EACNodeState EXPORT_DIRECTORY E3DWindow ISimInstance UIDialog
// @class CasEvent
// @class CasEventChangeFocus
// @class CasEventInitSim
// @class CasEventResetSim
// @class CasEventStoreSim
// @class CasEventResetSimDraw
// @class CasListener
// @class CasMediator
// @class CasScene
// @class CasScene__RoomId
// @class CasSceneDefault
// @class CasSimRenderer
// @class CasSimRendererDynamic
// @class CasSimState
// Create-a-Sim (game/targets_cas.cpp, casmediator.cpp, caslistener.cpp, casevent.cpp, casscene*.cpp,
// cassimstate.cpp, cassimrenderer*.cpp). The CAS target owns a mediator that passes events (a new
// sim, a change of the sim, a change of the UI focus, ...) to its listeners: the scene (the CAS room
// level, its light sets and the camera, which moves with the UI focus), the current sim's state (the
// description being edited) and the current sim's renderer (its SimModel and animation). Up to four
// sims (a family) have a state and a renderer each.
#pragma once
#include <deque>
#include <set>
#include <vector>
#include <string>

// An event (0x08 bytes + its data): m_type picks the listener's handler.
struct CasEvent {
  enum : int {
    kInitSim = 0, kResetSim = 1, kChangeFocus = 2, kStoreSim = 3, kChangeSim = 4, kMorphSim = 5, kResetSimDraw = 6
  };
  explicit CasEvent(int type = 0) : m_type(type) {}
  virtual ~CasEvent();  // 801603B8
  int m_type = 0;  // +0x00 (GC: the vtable follows at +0x04)
};

struct CasEventChangeFocus : public CasEvent {
  CasEventChangeFocus() : CasEvent(kChangeFocus) {}
  int m_eventFocus = 0;      // +0x08 ConvertUIFocusToEventFocus
  int m_uiFocus = 0;         // +0x0C GetCurrentUIFocus
  int m_prevFocus = 0;       // +0x10
  int m_prevPartFocus = 0;   // +0x14
};

struct CasEventInitSim : public CasEvent {
  CasEventInitSim() : CasEvent(kInitSim) {}
  CasSimDescriptionS2C m_desc;  // +0x08
};

// Back to the stored sim (the gender is the one to show).
struct CasEventResetSim : public CasEvent {
  CasEventResetSim() : CasEvent(kResetSim) {}
  int m_male = 0;  // +0x08
};
// The sim as it is becomes the stored one.
struct CasEventStoreSim : public CasEvent {
  CasEventStoreSim() : CasEvent(kStoreSim) {}
};
// The renderer redraws the sim (rebuilt when the gender changed).
struct CasEventResetSimDraw : public CasEvent {
  CasEventResetSimDraw() : CasEvent(kResetSimDraw) {}
  int m_genderChanged = 0;  // +0x08
};

struct CasListener {
  CasListener();  // 80161B50
  virtual ~CasListener();  // 80161B6C
  virtual void HandleEvent(CasEvent& event);  // 80161A6C
  virtual void SetMediator(CasMediator* mediator);  // 80161BA0
  CasMediator* GetMediator() const;  // 80161BA8
  virtual void HandleEventChangeFocus(CasEventChangeFocus& event);  // 80161BB0
  virtual void HandleEventChangeSim(CasEventChangeSimS2C &);  // 80161BB4
  virtual void HandleEventInitSim(CasEventInitSim& event);  // 80161BB8
  virtual void HandleEventResetSim(CasEventResetSim &);  // 80161BBC
  virtual void HandleEventStoreSim(CasEventStoreSim &);  // 80161BC0
  virtual void HandleEventMorphSim(CasEventMorphSimS2C &);  // 80161BC4
  virtual void HandleEventResetSimDraw(CasEventResetSimDraw &);  // 80161BC8

  CasMediator* m_mediator = nullptr;  // +0x00 (GC: the vtable at +0x04)
};

struct CasMediator {
  CasMediator();  // 80161BCC
  ~CasMediator();  // 80161C88
  CasMediator(const CasMediator&) = delete;
  CasMediator& operator=(const CasMediator&) = delete;
  void AddListener(CasListener& listener);  // 80161D18
  void RemoveListener(CasListener& listener);  // 80161D84
  void RemoveAllListeners();  // 80161F68
  int GetNumListeners() const;  // 80162028
  void PostEvent(CasEvent& event);  // 80162030
  CasSimDescriptionS2C* GetEditSimDescription();  // 801620D8
  unsigned int GetNumBodyModels(eBodyPartS2C) const;  // 801620F4
  unsigned int GetNumTattooTextures(eTattooTextureTypeS2C) const;  // 80162128
  unsigned int GetIconTextureId(eBodyPartS2C, unsigned int) const;  // 8016215C
  unsigned int GetIconTattooTextureId(eTattooTextureTypeS2C, unsigned int) const;  // 80162190
  int GetIndexes(eBodyPartS2C, unsigned char *, unsigned char *);  // 801621C4
  int GetIndexes(eTattooTextureTypeS2C, unsigned char *, unsigned char *);  // 801621F8
  void DrawSim(ERC* rc);  // 8016222C
  void SetCasMode(int mode);  // 8016226C
  int IsTransitionComplete();  // 80162298
  void SetControllerFilterId(unsigned int id, unsigned int player);  // 801622F4
  unsigned int GetControllerFilterId(unsigned int player) const;  // 8016230C

  std::set<CasListener*> m_listeners;    // +0x00 (an rb-tree)
  CasSimState* m_editSim = nullptr;      // +0x10 the current sim's state
  CasSimRenderer* m_renderer = nullptr;  // +0x14 the current sim's renderer
  CasScene* m_scene = nullptr;           // +0x18
  CASTarget* m_target = nullptr;         // +0x1C
  unsigned int m_filterIds[2] = {};      // +0x20 Apt button filters per player
  int m_player = 0;                      // +0x28
  int m_mode = 0;                        // +0x2C the CAS mode (0: new game)
};

// 0x250 bytes.
struct CasSimState : public CasListener {
  CasSimState();  // 80172C28
  CasSimState(CasSimDescriptionS2C &);  // 80172C78
  ~CasSimState() override;  // 80172D34
  int GetAdjacentIndex(unsigned char, bool);  // 80172D68
  bool GetMoreThanOneOption(unsigned char);  // 8017348C
  int GetNextAvailableSleeveStyle(eBodyPartS2C, signed char, bool, CasSimPartsS2C &);  // 80173B54
  int GetNextSleeveStyle(eBodyPartS2C, Sim__SleeveStyleS2C, bool);  // 80173E5C
  int GetNextAvailableMidLayerTorsoStyle(signed char, bool, CasSimPartsS2C &);  // 80173F28
  int GetNextMidLayerTorsoStyle(Sim__MidLayerTorsoStyleS2C, bool);  // 80173FF8
  int GetNextAvailableMidLayerCollarStyle(signed char, CasSimPartsS2C &);  // 801740A4
  int GetNextAvailableOuterLayerTorsoStyle(signed char, CasSimPartsS2C &);  // 80174178
  int GetNextAvailableOuterLayerCollarStyle(signed char, bool, CasSimPartsS2C &);  // 8017424C
  int GetNextAvailableLowerBodyStyle(signed char, bool, CasSimPartsS2C &);  // 80174324
  int GetNextAvailableHairStyle(signed char, bool, CasSimPartsS2C &);  // 801743FC
  int GetNextAvailableHatStyle(signed char, bool, CasSimPartsS2C &);  // 801745CC
  int GetNextAvailableArmAccessoryStyle(signed char, bool, CasSimPartsS2C &);  // 801746A4
  int GetNextBodyPartStyle(int, int, bool);  // 8017477C
  void HandleEventInitSim(CasEventInitSim& event) override;  // 801747C0
  void HandleEventResetSim(CasEventResetSim &) override;  // 8017486C
  void HandleEventStoreSim(CasEventStoreSim &) override;  // 80174984
  void HandleEventChangeSim(CasEventChangeSimS2C &) override;  // 801749DC
  void HandleEventMorphSim(CasEventMorphSimS2C &) override;  // 80175180
  void SetSimName(unsigned short *, unsigned short *);  // 80175228
  CasSimDescriptionS2C* GetSimDescription();  // 80175294
  CasSimDescriptionS2C* GetStoredSimDescription();  // 8017529C

  CasSimDescriptionS2C m_desc;    // +0x08 being edited
  CasSimDescriptionS2C m_stored;  // +0x12C as it was
};

// 0x240 bytes (with CasSimRendererDynamic).
struct CasSimRenderer : public CasListener {
  CasSimRenderer();  // 8016EEE0
  ~CasSimRenderer() override;  // 8016EF38
  void Init();  // 8016F050
  void SetPosition(EVec3& pos, float rotation);  // 8016F0E8
  RET_UNKNOWN GetNumActiveBodyModels();  // 8016F10C
  void UpdateSkin();  // 8016F13C
  virtual void Draw(ERC* rc);  // 8016F174
  void DrawShadow(ERC* rc);  // 8016F304
  void SetNextAnimation(unsigned int anim);  // 8016F488
  int CheckPending();  // 8016F50C
  void SetupSimCommon();  // 8016F5A8
  ETexture* CreateFaceImage(unsigned int width, unsigned int height, unsigned int bpp, bool sepia,
                            unsigned int background);  // 8016F720
  unsigned int GetNumBodyModels(eBodyPartS2C);  // 8016F7D0
  unsigned int GetNumTattooTextures(eTattooTextureTypeS2C);  // 8016F7F4
  unsigned int GetIconTextureId(eBodyPartS2C, unsigned int);  // 8016F818
  unsigned int GetIconTattooTextureId(eTattooTextureTypeS2C, unsigned int);  // 8016F83C
  void SetFaceImage(ETexture *);  // 8016F860
  void DrawFaceImage(ERC *);  // 8016F90C
  void UpdateRepShaders(int);  // 8016FA20
  void GetBonePos(unsigned int, EVec3 &);  // 8016FAB0
  unsigned int GetPositionNum() const;  // 8016FB54
  void SetPositionNum(unsigned int index);  // 8016FB5C
  void SetSimRotation(float rotation);  // 8016FB64
  SimModel* GetSimModel();  // 8016FB6C
  // GC: vtable 0x5C / 0x6C (CasSimRendererDynamic's).
  virtual void Update(float dt) {}
  virtual void SetupSim() {}

  SimModel m_model;                 // +0x08
  ERModel* m_shadowModel = nullptr; // +0x170 0x6ef2f2da
  EAnimController m_anim;           // +0x174 the sim's skeleton
  EVec3 m_pos;                      // +0x1EC
  EVec3 m_scale{1.0f, 1.0f, 1.0f};  // +0x1F8
  unsigned int m_pendingAnim = 0;   // +0x204 waits to be loaded
  float m_rotation = 0.0f;          // +0x208 radians, from the scene
  float m_userRotation = 0.0f;      // +0x20C turned by the player
  int m_field210 = 0;               // +0x210
  unsigned int m_positionNum = 0;   // +0x214 which sim (0..3)
};

struct CasSimRendererDynamic : public CasSimRenderer {
  CasSimRendererDynamic();  // 8016FBA0
  ~CasSimRendererDynamic() override;  // 8016FC14
  void SetupSim() override;  // 8016FC44
  int GetIndexes(eBodyPartS2C, unsigned char *, unsigned char *);  // 8016FD00
  int GetIndexes(eTattooTextureTypeS2C, unsigned char *, unsigned char *);  // 8016FE04
  void Update(float dt) override;  // 8016FF08
  bool IsPersonalityAnim(unsigned int anim);  // 80170154
  bool IsShirtIdleAnim(unsigned int anim);  // 801701DC
  bool IsShoeIdleAnim(unsigned int anim);  // 8017021C
  bool IsPantIdleAnim(unsigned int anim);  // 80170244
  bool IsRegularIdleAnim(unsigned int anim);  // 80170278
  unsigned int SetNextRandomShirtIdle();  // 801702AC
  unsigned int SetNextRandomRegularIdle();  // 80170320
  void SelectNextStandingAnimation();  // 80170394
  void PlayPersonalityAnim(short);  // 80170704
  void DrawDebugInfo(ERC *);  // 80170824
  void HandleEventChangeSim(CasEventChangeSimS2C &) override;  // 80171D78
  void HandleEventMorphSim(CasEventMorphSimS2C &) override;  // 801725DC
  void HandleEventResetSimDraw(CasEventResetSimDraw &) override;  // 801729AC
  void HandleEventInitSim(CasEventInitSim& event) override;  // 80172A70
  void HandleEventChangeFocus(CasEventChangeFocus &) override;  // 80172B18
  void LockSimRotation(bool lock);  // 80172C20

  static const unsigned int kFemaleAnimations[36];  // gCasFemaleSimAnimations (80424600)
  static const unsigned int kMaleAnimations[36];    // gCasMaleSimAnimations (80424690)

  unsigned int m_curAnim = 0;            // +0x218
  int m_rotationLocked = 0;              // +0x21C
  int m_field220 = 0;                    // +0x220 the occasional special idle (anims[3]) may play
  const unsigned int* m_anims = kFemaleAnimations;  // +0x224 the gender's animation table
  int m_topChanged = 0;                  // +0x228 a top changed (the next idle shows it off)
  int m_bottomChanged = 0;               // +0x22C
  int m_shoesChanged = 0;                // +0x230
  int m_playPersonality = 0;             // +0x234 m_personalityAnim plays after the next regular idle
  unsigned int m_personalityAnim = 0;    // +0x238
  int m_focus = 0;                       // +0x23C the last event focus
};

struct CasScene__RoomId {
  SIMMY_ENUM_BODY(CasScene__RoomId)
};

// One of the scene's four light sets (0x260 bytes): "<scene>_clothing/roommate/genetics/mirror".
struct CasSceneLightSet {
  ELights m_lights;                 // +0x00 what the sim is lit with
  float m_sunIntensity = 0.0f;      // +0x184 the level's sun and ambient (kSun* tables)
  EVec3 m_sunColor;                 // +0x188
  EVec3 m_sunDir;                   // +0x1A4
  float m_ambientIntensity = 0.0f;  // +0x23C
  EVec3 m_ambientColor;             // +0x240
  float m_shadowAlpha = 0.0f;       // +0x25C
};

// 0xB20 bytes.
struct CasScene : public CasListener {
  CasScene();  // 8016502C
  ~CasScene() override;  // 80165250
  void Init();  // 801653FC
  virtual void Reload();  // 801654A4
  void SetMediator(CasMediator* mediator) override;  // 801657D8
  virtual void Update(float dt);  // 801657E0
  virtual void Draw(ERC* rc);  // 80165824
  void DrawRoom(ERC* rc, CasScene__RoomId room, EMat4& m);  // 80165990
  virtual void DrawRoomClothing(ERC* rc, EMat4& m);  // 801659D4
  void SetCameraParams(float fov, float nearPlane, float farPlane);  // 80165BCC
  void LoadLights(char* name, int set);  // 80165BDC
  void SetLights(int set);  // 80165F04
  void SetSceneLighting(int set, bool tweak);  // 80166088
  void RepositionCamera(unsigned int camera, float duration, float delay);  // 801660D0
  void InitCamera(unsigned int camera);  // 80166478
  void UpdateCamera(float dt);  // 801665A8
  void SetUpWindow(ERC* rc);  // 80166A9C
  void HandleEventChangeFocus(CasEventChangeFocus& event) override;  // 80166BB0
  virtual int GetObjectPosition(unsigned int object, EVec3& pos, float& rotation);  // 80166E94
  void GetCameraAngle(EVec3 *, EVec3 *);  // 80167238
  RET_UNKNOWN IsCameraMoving();  // 80167274
  ELights* GetLights();  // 801672B0
  E3DWindow* Get3DWindow() const;  // 801672B8
  virtual int GetRoomFromCameraAngle(unsigned int) const;  // 801672C0
  virtual const char* GetAmbientScoreName() const;  // 801672C8
  virtual int GetMirrorCorners(CasScene__RoomId room, EVec3* corners) const = 0;
  virtual const char* GetSceneName() const = 0;
  virtual unsigned int GetRoomModelId(CasScene__RoomId room) const = 0;

  ELights m_lights;                      // +0x08 the current light set's
  CasSceneLightSet m_lightSets[4];       // +0x100
  ERLevel* m_level = nullptr;            // +0xA80 the room (0x94a667af)
  ERModel* m_mirrorRoom = nullptr;       // +0xA84 the room seen in the mirror (0xca9378d8)
  void* m_mirror = nullptr;              // +0xA88 EMirrorPortal
  ERQuickdata* m_quickdata = nullptr;    // +0xA8C 0x2a2af469 (QDLightSet)
  EVec3 m_eye;                           // +0xA90
  EVec3 m_target;                        // +0xA9C
  float m_t = 1.0f;                      // +0xAA8 camera move, 0..1
  float m_delay = 0.0f;                  // +0xAAC before the move starts
  float m_stickT = 0.0f;                 // +0xAB0 the stick's zoom
  unsigned int m_camera = 0;             // +0xAB4
  unsigned int m_prevCamera = 0;         // +0xAB8
  float m_fov = 45.0f;                   // +0xABC
  void* m_score = nullptr;               // +0xAC0 AmbientScorePlayer
  EVec3 m_eyeFrom;                       // +0xAC4
  EVec3 m_eyeTo;                         // +0xAD0
  EVec3 m_eyeAlt;                        // +0xADC zoomed (m_altCameraAngles)
  EVec3 m_targetAlt;                     // +0xAE8
  EVec3 m_targetFrom;                    // +0xAF4
  EVec3 m_targetTo;                      // +0xB00
  float m_duration = 0.25f;              // +0xB0C
  float m_near = 0.25f;                  // +0xB10
  float m_far = 50.0f;                   // +0xB14
  E3DWindow* m_win = nullptr;            // +0xB18
  int m_lightSet = 0;                    // +0xB1C
};

struct CasSceneDefault : public CasScene {
  CasSceneDefault();  // 801754D4
  ~CasSceneDefault() override;  // 80175510
  int GetMirrorCorners(CasScene__RoomId room, EVec3* corners) const override;  // 80175540
  const char* GetSceneName() const override;  // 801755A4
  unsigned int GetRoomModelId(CasScene__RoomId room) const override;  // 801755AC
};

// 0x1638 bytes.
struct CASTarget : public UIObjectBase {
  enum { kMaxSims = 4 };
  // m_flags bits: the low four say which sims are shown.
  enum : unsigned int { kMediatorStarted = 0x10, kMediatorChanged = 0x20, kNewGame = 0x40 };

  explicit CASTarget(bool skipUI);  // 80190EA0
  ~CASTarget() override;  // 80191A00
  void Init(bool skipUI);  // 80191C94
  void Shutdown();  // 80192298
  void SaveSimToFamilySlot(int);  // 80192360
  int SaveCASDataToNeighborhood();  // 80192658
  int IsShutdown();  // 8019282C
  void StartupMediator();  // 80192ACC
  void ShutdownMediator();  // 80192C50
  void ChangeMediator(int sim);  // 80192DB8
  CasScene* CreateScene(int mode);  // 801930DC
  static int GetLotNumberFromMode(int mode);  // 8019314C
  void SetVariable(char* name, char* value) override;  // 80193174
  unsigned short* GetLocalizable(char* name) override;  // 80193334
  char* GetVariable(char* name) override;  // 80193928
  void Update() override;  // 80193B78
  void Draw(ERC* rc) override;  // 80193D70
  int UnloadLastScreen();  // 80193F2C
  void SetVariableNav(char* value);  // 80193FB0
  void SetVariableSubnav(char* value);  // 801942B8
  int GetCurrentUIFocus();  // 80194428
  void BroadcastNewFocus();  // 801949EC
  int ConvertUIFocusToEventFocus(int focus);  // 80194A78
  void HandleNav(int nav);  // 80194CD0
  void GotoScreen(int screen);  // 80194DA4
  void GotoInitialScreen();  // 80194E4C
  void SetHandleInput(bool on);  // 80194EBC
  void GotoGame();  // 80194EC4
  void GotoMainMenu();  // 80194F08
  void GotoBase();  // 80194F8C
  static void ReturnForwardToKeyboard();  // 80194FB0
  void SaveNameEnterCBA();  // 80195000
  static void DontSaveNameEnterCBA();  // 801950C4
  void HandleConditionalNav(int nav);  // 80195154
  int CFCShouldGotoBodyScreenNext();  // 80195C1C
  int ShowAllCFAOptions();  // 80195C40
  void ConvertPartUIFocusToModList(char* out);  // 80195C80
  void CreateModStrFromValues(char* out, unsigned int flags);  // 80195D48
  void SpawnYesNoDialog(unsigned int nav, const unsigned short* body, const unsigned short* title, int preselect);  // 80195DC4
  void DialogYesNoSelectionCallback(int selection);  // 80196040
  void DialogNoSelectionCallback();  // 801960F4
  void SpawnOKDialog(const unsigned short* body, const unsigned short* title);  // 801960F8
  void DialogOKSelectionCallback(int selection);  // 8019632C
  bool IsViewingCAR();  // 80196364
  int IsViewingMirror();  // 80196378
  int IsViewingGenetics();  // 801963CC
  void ResetSimRotations();  // 80196420
  void DeallocateMorphResources(int sim);  // 80196474
  int IsShuttingdown();  // 801A07F8
  int GetPrevUIFocus();  // 801A0800
  int GetCurPartUIFocus();  // 801A0808
  int GetCurrentSimNumber();  // 801A0810
  int HasSimInSlot(int slot) const;  // 801A0818
  void SetHasSimInSlot(int slot, bool has);  // 801A0840
  CasSimRendererDynamic* GetSimRenderer(int sim);  // 801A0890
  CASGeneticsTarget* GetGeneticsTarget();  // 801A08A8
  CasNpcEditor* GetNPCEditor();  // 801A08B0

  CasListener m_listener;                 // +0x84 (GC: CASTarget's own listener part)
  int m_handleInput = 1;                  // +0x8C nav commands are acted on
  int m_screen = 0;                       // +0x90 the UI screen shown
  int m_lastScreen = 0;                   // +0x94 being unloaded
  int m_field98 = 0;                      // +0x98
  int m_subnav = 0;                       // +0x9C
  int m_prevFocus = 0;                    // +0xA0
  int m_prevPartFocus = 0;                // +0xA4
  std::deque<int> m_prevOptions;          // +0xA8 the options chosen on the way (CAS_previousOptNum)
  CasMediator m_mediator;                 // +0xD8
  // +0x100 the player and +0x104 the CAS mode (UIDB "CAS_mode"; 0: a new family). GC: these are the
  // mediator's own words (+0xD8 + 0x28 / 0x2C), so CasMediator::SetCasMode changes the target's mode.
  int& m_player = m_mediator.m_player;
  int& m_mode = m_mediator.m_mode;
  CasSimState m_simStates[kMaxSims];      // +0x108
  CasSimRendererDynamic m_renderers[kMaxSims];  // +0xA48
  unsigned int m_flags = 0;               // +0x1348
  int m_curSim = 0;                       // +0x134C
  CasScene* m_scene = nullptr;            // +0x1350
  CasNpcEditor* m_npcEditor = nullptr;    // +0x1354 (developer builds)
  unsigned int m_filters[2] = {};         // +0x1358 Apt button filters
  int m_uiPlayer = 0;                     // +0x1360
  int m_initFrames = 0;                   // +0x1364
  float m_genderTime = 0;                 // +0x136C
  // The sub-targets (null while not ported).
  CASGeneticsTarget* m_genetics = nullptr;   // +0x1370
  CASMorphTarget* m_morph = nullptr;         // +0x1374
  CASBodyTarget* m_body = nullptr;           // +0x1378
  CASSelectionTarget* m_selection = nullptr; // +0x137C
  CASMiscTarget* m_misc = nullptr;           // +0x1380
  CASFashionTarget* m_fashion = nullptr;     // +0x1384
  CASPersonalTarget* m_personal = nullptr;   // +0x1388
  CASTattooTarget* m_tattoo = nullptr;       // +0x138C
  CASRoommateTarget* m_roommate = nullptr;   // +0x1390
  K2YTarget* m_keyboard = nullptr;           // +0x1398
  UIDialog m_dialog;                         // +0x139C yes/no and OK prompts
  int m_pendingNav = 0;                      // +0x1424 the conditional nav a yes/no dialog answers
  std::vector<unsigned short> m_familyName;  // +0x1428 StringBuffer2
  int m_shuttingDown = 0;                 // +0x1630
  int m_shutDown = 0;                     // +0x1634
};
