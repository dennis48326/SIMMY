// @class InteractorModule__InteractorInputManager__InteractorCommand
// @class InteractorModule__InteractorInfo
// @class InteractorModule__Interactor after EVec2 EVec3 InteractorModule__InteractorInfo
// @class InteractorModule__DirectInteractor after InteractorModule__Interactor
// The player interactors (game/interactor.cpp, game/directinteractor.cpp): what a player's pad
// drives in live mode. Each player has a set of interactors (direct control, build/buy cursors,
// social mode, ...) of which one is active; the interactor manager updates the active ones and their
// input sessions feed them the pad's commands. The direct-control interactor turns the left stick
// into the selected sim's desired anim state and direction (relative to the camera) for its animator
// (SAnimator2) and keeps the camera on the sim. PC: the interactor manager, the input manager and
// the other interactors are not ported; UpdateDirectInteractors stands in for the manager.
#pragma once

// A command the input manager sends (0x0B/0x0C the C stick's x/y, 0x12/0x13 the left stick's).
struct InteractorModule__InteractorInputManager__InteractorCommand {
  SIMMY_ENUM_BODY(InteractorModule__InteractorInputManager__InteractorCommand)
};

// Where the player's interactor is (0x10 bytes): x, y on the ground and its extents.
struct InteractorModule__InteractorInfo {
  float m_x = 0.0f, m_y = 0.0f;  // +0x00
  float m_w = 0.0f, m_h = 0.0f;  // +0x08
};

// 0x5C bytes (vtable at +0x5C).
struct InteractorModule__Interactor {
  InteractorModule__Interactor();  // 802091BC
  void Update(float dt);  // 80209280
  virtual void AttachCamera(ESimsCam* cam);  // 802092A4
  virtual ESimsCam* GetAttachedCamera();  // 802092AC
  virtual void GetInteractorInfo(InteractorModule__InteractorInfo& info);  // 802092B4
  virtual void SetExtents(EVec2& extents);  // 802092D8
  virtual void OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand);  // 802092EC
  virtual void OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand, float);  // 80209354
  virtual void ResetInputState();  // 802093AC
  virtual double GetSelectionRadius();  // 802093F0
  virtual void UpdateObjectHighlights(EIObjectMan *);  // 80209454
  static void NotifyClient(InteractorModule__Interactor__ClientNotificationState, InteractorModule__Interactor__CallbackData &, InteractorModule__Interactor__InteractorParams &);  // 80209480
  virtual ~InteractorModule__Interactor();  // 802094B8
  virtual void OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand, float);  // 802094EC
  void AddIdleTime(float dt);  // 802094F0
  virtual void ResetIdleTime();  // 80209500
  virtual void UpdateOverlapIntersection(InteractorModule__Interactor__OverlapData *);  // 80209510
  virtual void ExecuteObjectActionMenu(cXObject *);  // 80209514
  virtual void StopInObjectMenu();  // 80209518
  virtual RET_UNKNOWN ChooseAction(Interaction *);  // 8020951C
  EVec3& GetPos();  // 80209524
  RET_UNKNOWN GetSnappedPos();  // 8020952C
  virtual void SnapToPos(EVec3 &);  // 80209534
  RET_UNKNOWN EitherStickCentered();  // 8020955C
  RET_UNKNOWN Moved();  // 80209584
  int GetPlayerId();  // 802095CC
  void SetDrawState(int);  // 802095D4
  RET_UNKNOWN GetDrawState();  // 802095DC

  int m_player = 0;               // +0x00
  ESimsCam* m_cam = nullptr;      // +0x04
  EVec3 m_pos;                    // +0x08 where it is (the world's frame)
  EVec3 m_prevPos;                // +0x14 last frame's
  EVec3 m_savedPos;               // +0x20
  int m_restorePos = 0;           // +0x2C put it back at m_savedPos (control status 2)
  int m_actionMenu = 0;           // +0x30 the action menu was asked for
  EVec2 m_leftStick;              // +0x34 the pad's left stick (commands 0x12, 0x13)
  EVec2 m_rightStick;             // +0x3C the C stick (commands 0x0B, 0x0C)
  int m_leftNeutral = 0;          // +0x44
  int m_rightNeutral = 0;         // +0x48
  EVec2 m_extents;                // +0x50
  float m_idleTime = 0.0f;        // +0x58 seconds without input
};

// 0x8C bytes.
struct InteractorModule__DirectInteractor : public InteractorModule__Interactor {
  InteractorModule__DirectInteractor();  // 80203D70
  ~InteractorModule__DirectInteractor() override;  // 80203DD0
  virtual void OnCreate();  // 80203E04
  virtual void OnDestroy();  // 80203E38
  virtual void OnStart(int player);  // 80203E6C
  virtual void OnStop();  // 802040D4
  void ParseControls();  // 8020415C
  int GetDesiredAnimState(bool neutral, EVec2 stick);  // 802041CC
  bool InitPlayerPos();  // 80204220
  void UpdatePlumbBob();  // 80204438
  void UpdateControlStatus();  // 8020450C
  void SetVelocityModifiers();  // 802045C4
  void SendNeutralFreeMoveInput();  // 80204620
  void InterpretFreeMoveInput();  // 802046FC
  virtual void Update(float dt);  // 80204838
  bool IsSimulatorPaused();  // 80204BEC
  RET_UNKNOWN ShouldHighlightObject(cXObject *, float);  // 80204C5C
  void GetInteractorInfo(InteractorModule__InteractorInfo& info) override;  // 80204EDC
  void StartCancelTimer();  // 80204F0C
  void ClearCancelTimer();  // 80204F24
  void CancelQueuedActions();  // 80204F3C
  int CheckCancelTimer();  // 80204FD8
  virtual void PreDraw(InteractorModule__InteractorVisualizer &);  // 80205014
  virtual void Draw(InteractorModule__InteractorVisualizer &);  // 80205040
  virtual void OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand, float);  // 8020506C
  virtual void OnCommandReleased(InteractorModule__InteractorInputManager__InteractorCommand);  // 802056D8
  void OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand command, float value) override;  // 80205748
  void ResetInputState() override;  // 80205788
  virtual double GetSelectionRadius();  // 802057BC
  void ExecuteActionMenu();  // 802057C8
  virtual void ExecuteObjectActionMenu(cXObject *);  // 80205A60
  virtual void StopInObjectMenu();  // 80205B78
  virtual RET_UNKNOWN ChooseAction(Interaction *);  // 80205BB0
  void SetupCamera();  // 80205C04
  virtual void UpdateObjectHighlights(EIObjectMan *);  // 80205C4C
  // Autonomy takes the sim (TryCheckObject kind 0 and TryFindBestAction stop seeing a player).
  void SetAutonomous();  // 80205FB4
  void ResetIdleTime() override;  // 80205FF4
  int GetControlStatus();  // 80206118
  void ExitDirectControl();  // 80206120
  int IsAutonomous() { return m_autonomous; }  // 8020612C
  bool CameraDirectorPermitsDirectControl();  // 80206134
  void UpdateInteractorPos();  // 80206180

  int m_controlStatus = 1;        // +0x64 0 neutral, 1 steering, 2 restore, 3 camera director's
  int m_posInitialized = 0;       // +0x68
  int m_paused = 0;               // +0x6C
  cXPerson* m_person = nullptr;   // +0x70 the player's selected person
  float m_coneAngle = 0.0f;       // +0x74 c_SelectionConeAngle
  void* m_plumbBob = nullptr;     // +0x78 the player's PlumbBob (not ported)
  int m_plumbBobShown = 0;        // +0x7C
  int m_autonomous = 0;           // +0x80
  int m_cancelTimerOn = 0;        // +0x84
  float m_cancelTime = 0.0f;      // +0x88
};

// PC stand-in for InteractorModule::InteractorManager::Update (GC: from ELiveMode::Update before the
// cameras move): each playing player's direct-control interactor, started on its selected person,
// reads the pad and updates. `count`: the players playing.
void UpdateDirectInteractors(int count, float dt);
// PC stand-in for InteractorManager::GetPlayerInteractorInfo: the player's interactor's info; false
// without one.
bool GetPlayerInteractorInfo(int player, InteractorModule__InteractorInfo& info);
// PC stand-in for InteractorManager::GetPlayerInteractor(player, 0): the player's direct interactor or
// null.
InteractorModule__DirectInteractor* PlayerDirectInteractor(int player);
// PC: the interactors go with the house (GC: ShutdownInteractorModule).
void ShutdownDirectInteractors();
