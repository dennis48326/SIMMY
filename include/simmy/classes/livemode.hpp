// @class DummyMode after EGameState
// @class EGameState after EGameStateId
// @class EGameStateId
// @class EGameStateMan after EGameState
// @class EHouse after ERLevel EVec2
// @class ELiveMode after EGameState
// @class InLevelBaseState after StateMachineState
// @class LiveModeInitState after StateMachineState
// Live mode (game/livemode.cpp, inlevel.cpp, eorhouse.cpp, gamestateman.cpp): the game states the
// app runs while in a level (EGameStateMan: the dummy and live modes), the in-level state of the
// front-end machine that updates them, the staged load of the house (LiveModeInitState) and the house
// itself (EHouse: its level, rooms, objects and lighting).
#pragma once
#include <vector>

struct EGameStateId {
  SIMMY_ENUM_BODY(EGameStateId)
  enum : int { kLive = 1, kDummy = 2 };
};

// A mode of the game (0x0C bytes on GC: id, manager, vtable).
struct EGameState {
  EGameState();  // 80089D78
  virtual ~EGameState();  // 800899CC
  virtual void Init(int arg) = 0;
  virtual void Update() = 0;
  virtual void Draw(ERC* rc) = 0;
  virtual void Reset(int arg) = 0;
  virtual void PauseAllSounds();  // 80089E28
  virtual void ResumeAllSounds();  // 80089E2C
  virtual RET_UNKNOWN InTransportMode();  // 80089E20
  void GetState();  // 80089E10

  int m_id = 0;                      // +0x00 EGameStateId
  EGameStateMan* m_owner = nullptr;  // +0x04
};

struct DummyMode : public EGameState {
  DummyMode();  // 80099834
  ~DummyMode() override;  // 80099864
  void Init(int) override;  // 800998B8
  void Reset(int) override;  // 800998BC
  void Update() override;  // 800998C0
  void Draw(ERC *) override;  // 800998C4
};

// The game states (DAT_8046E680, ESimsApp +0x478): the current one is updated and drawn.
struct EGameStateMan {
  EGameStateMan();  // 80089A14
  ~EGameStateMan();  // 80089A3C
  EGameStateMan(const EGameStateMan&) = delete;
  EGameStateMan& operator=(const EGameStateMan&) = delete;
  void SetState(EGameStateId id);  // 80089A80
  void AddState(EGameState* state);  // 80089B30
  void KillState();  // 80089B68
  void SoftReset();  // 80089BA0
  void DeleteAllStates();  // 80089C10
  void Update();  // 80089CE0
  void Draw(ERC* rc);  // 80089D3C

  EGameState* m_current = nullptr;    // +0x00 (GC: its list node)
  std::vector<EGameState*> m_states;  // +0x04 ENodeList
  int m_ownsStates = 1;               // +0x0C
  static int s_currentId;             // DAT_80475E6C
};

// The house being played (0x128 bytes): its level, rooms (walls), objects and lighting.
struct EHouse {
  EHouse(EVec2& pos, int house, ERLevel* level, bool rooms, bool objects, bool walls, bool outside);  // 8002371C
  ~EHouse();  // 800239A0
  EHouse(const EHouse&) = delete;
  EHouse& operator=(const EHouse&) = delete;
  const char* GetHouseName();  // 800236F8
  void Init();  // 800239EC
  void BuildHouse();  // 80023A20
  void SetWallState(EWallUpDownStateType);  // 80023BAC
  void SetNextWallMode();  // 80023BE0
  void Draw(ERC* rc);  // 80024430
  void Update();  // 800244CC
  void UpdateRoomAmbient(LevelLightingEntry& entry);  // 800246F0
  static void TriggerLightningEffect();  // 80024824
  void WeatherChanging();  // 80024828
  void Fair();  // 800248A8
  void Rain();  // 800248F4
  void UpdateWeather();  // 8002498C
  void Cleanup();  // 80024CFC
  void DestroyWalls();  // 80024DB8
  void ReCalcHouse();  // 80024E04
  void UpdateRoomLights();  // 80024F98
  void SetLightIntensityScaleFromRoom(EILight *);  // 8002510C
  void InitRoomLighting();  // 80025264
  RET_UNKNOWN GetRoomAmbLight(int);  // 8002539C
  void CleanUpRoomLights();  // 800253AC
  void InsertHouseObject(EOrderTableData *);  // 80025464
  static void Refresh();  // 800254A8

  int m_isLot = 0;                  // +0x00 (last constructor argument)
  EIObjectMan* m_objectMan = nullptr;  // +0x04 the objects' 3D instances
  ERoom* m_room = nullptr;          // +0x08 the walls
  int m_updating = 0;               // +0x0C
  int m_field10 = 1;                // +0x10
  int m_built = 0;                  // +0x14 BuildHouse ran (Cleanup first next time)
  int m_ready = 0;                  // +0x18
  ERLevel* m_level = nullptr;       // +0x1C the house's level (also the engine's current level)
  EResource* m_model = nullptr;     // +0x20 the model of the level's id (GetRef: not held)
  int m_wallMode = 1;               // +0x24 EWallUpDownStateType: 0 up, 1 cut-away, 2 down
  EILight** m_roomLights = nullptr; // +0x28 per room (InitRoomLighting): its ambient light, or none
  int m_roomCount = 0;              // +0x2C
  int m_field30 = 0, m_field34 = 0, m_field38 = 0;  // +0x30
  float m_alpha = 1.0f;             // +0x3C the walls' and floors' fade alpha
  float m_minuteOfDay = 0;          // +0x40
  float m_minutesPassed = 0;        // +0x44
  float m_hour = 0;                 // +0x48 the time of day in hours
  float m_posY = 0;                 // +0x4C
  float m_posX = 0;                 // +0x50
  int m_house = 0;                  // +0x54 1..16
  RainEffect* m_rain = nullptr;     // +0x124 the rain while it rains (UpdateWeather)
};

// The live mode game state (0x58 bytes, DAT_80475C48).
struct ELiveMode : public EGameState {
  ELiveMode();  // 8008BAAC
  ~ELiveMode() override;  // 8008BB08
  void Init(int arg) override;  // 8008BB68
  void Reset(int arg) override;  // 8008CA58
  static void ChangeHouse();  // 8008DC0C
  void GotoStartMode();  // 8008DC40
  void QuitToMainMenu();  // 8008DC90
  int IsReadyForUpdate();  // 8008DD70
  void UpdateIntroCamera();  // 8008DF28
  void Update() override;  // 8008E4C0
  void Draw(ERC* rc) override;  // 8008EA14
  void PauseAllSounds() override;  // 8008EF14
  void ResumeAllSounds() override;  // 8008EF18
  void DrawSplitScreenMask(ERC *, E3DWindow &, int);  // 8008EF1C
  void InitPlayerXWindow(ERC* rc, E3DWindow& win, int player);  // 8008F1B8
  void DrawSplitScreenDivider(ERC *);  // 8008F470
  void DrawMain(ERC* rc);  // 8008F7AC
  void TriggerIntroCamera(CameraDirector *);  // 8008FBD0
  RET_UNKNOWN StartIntroCamera(CameraDirector *, ESimsCam *);  // 8008FBFC
  RET_UNKNOWN EndIntroCamera(CameraDirector *, ESimsCam *);  // 8008FE28
  RET_UNKNOWN InTransportMode() override;  // 80090038
  bool IsIntroCameraTriggered();  // 80090040
  void ResetIntroCamera();  // 80090054
  void SetIntroCameraState(IntroCameraStartState);  // 80090060
  RET_UNKNOWN GetIntroCameraState();  // 80090068
  RET_UNKNOWN IsRunningIntroCamera();  // 80090070
  void SetGoingToNeighborhoodMode(bool);  // 80090088
  void SetInitialized(bool initialized);  // 80090090
  int IsInitialized();  // 80090098
  void SetDontDraw(bool dontDraw);  // 800900A0

  int m_introCamera = 0;          // +0x0C the intro camera runs
  int m_field10 = 0;              // +0x10
  float m_field14 = 0;            // +0x14
  float m_field18 = 0;            // +0x18
  int m_storyStart = 0;           // +0x1C 1: "press start" before the story starts
  int m_field20 = 0;              // +0x20
  int m_leaving = 0;              // +0x24 going to the start mode / main menu
  int m_field28 = 0;              // +0x28
  int m_paused = 0;               // +0x2C (the cameras are not updated)
  int m_initialized = 0;          // +0x30 LiveModeInitState is done
  int m_dontDraw = 1;             // +0x34
  int m_arg = 0;                  // +0x38 Init's argument
  int m_backgroundSet = 0;        // +0x3C
  ERShader* m_shader = nullptr;   // +0x44 0x8e4c2d0b
  int m_field50 = 0;              // +0x50
  int m_field54 = -1;             // +0x54
};

// The front-end machine's state while in a level (0x2C bytes): runs the game states and the UI's
// in-game flow requests (pause menus, HUD).
struct InLevelBaseState : public StateMachineState {
  explicit InLevelBaseState(int id = -1);  // 8008B5B0
  ~InLevelBaseState() override;  // 8008B6F8
  void Reset() override;  // 80089E94
  void Shutdown() override;  // 80089EB0
  void Update(float) override;  // 80089F2C
  static void HandleExitToMainMenu();  // 8008B380
  void Draw(ERC* rc) override;  // 8008B498
  static void SavePlayerInteractorState();  // 8008B4D0
  void dialog_selection_callback(int);  // 8008B55C
  static void dialog_zero_input_callback();  // 8008B5AC
  void IsFlowStateRequestPending();  // 8008B758
  void GetFlowStateRequest();  // 8008B77C
  void SetFlowStateCurrent();  // 8008B7A0

  int m_pauseMenu = 0;        // +0x1C
  int m_paused = 0;           // +0x20 HUDTarget::IncPause was called
  int m_directControl[2] = {};  // +0x24
};

// The staged load of the house (0x20 bytes): datasets, the house and its level, the sims, cameras.
struct LiveModeInitState : public StateMachineState {
  explicit LiveModeInitState(int id = -1);  // 8008FEDC
  void Shutdown() override;  // 8008BC24
  void Reset() override;  // 8008BC28
  void Update(float) override;  // 8008BD2C
  void Draw(ERC *) override;  // 8008C9BC
  void DatasetGetRef();  // 8008C9C0
  void DatasetFinishAddRefAsync();  // 8008CA18
  RET_UNKNOWN DatasetPendingAddRefAsync();  // 8008FF18

  unsigned int m_dataset = 0;  // +0x1C the house preload dataset being loaded
};
