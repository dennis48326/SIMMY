// @class TheSimsStateMachine after StateMachine
// @class MUStateMachine after StateMachine
// @class TheSimsNGCLicenseState after StateMachineState ERFont
// @class TheSimsMemCardCheckState after StateMachineState
// @class TheSimsEAMovieState after StateMachineState
// @class TheSimsSimsIntroMovieState after StateMachineState
// @class TheSimsMaxisLogoState after StateMachineState
// @class TheSimsEnterMainMenuState after StateMachineState
// @class TheSimsMainMenuState after StateMachineState
// The game's top-level state machine ('SIMS', game/thesims.cpp): boot screens, intro movies, start
// screen, menus, pre-game, live mode. MUStateMachine ('MUST', game/mustates.cpp) runs memory-card
// ("memory unit") requests beside it.
#pragma once

struct TheSimsStateMachine : StateMachine {
  // State ids (TheSimsStateMachine::Startup).
  enum : int {
    kNGCLicense = 0, kMemCardCheck = 1, kEAMovie = 2, kMaxisLogo = 3, kSimsIntroMovie = 4, kStartScreen = 5,
    kEnterMainMenu = 6, kMainMenu = 7, kPreGame = 8, kPreGameFreeplay = 9, kPreGameCasPlayer1 = 10,
    kPreGameCasPlayer2 = 11, kPreGameCreateAFamily = 12, kInLevelStory = 13, kInLevelCreateASim = 14,
    kEndgameMovie = 15, kUrbzModeNewGame = 16, kUrbzCredits = 17, kLiveModeInit = 18, kLiveModeChangeHouse = 19,
  };
    void Startup() override;  // 80098DB4
    void Shutdown() override;  // 80099670
    void Update(float dt) override;  // 80099708
    void Draw(ERC* rc) override;  // 80099764
    int IsFlowStateRequestPending();  // 800997DC
    int GetFlowStateRequest();  // 80099804
    void SetFlowStateCurrent();  // 8009981C
    TheSimsStateMachine(int id, int machineId);  // 8009ACC4

  FlowGotoTarget* m_flowTarget = nullptr;  // +0x98
};

struct MUStateMachine : StateMachine {
    using eMUDisplayOk = ::MUStateMachine__eMUDisplayOk;
    using eMUInfo = ::MUStateMachine__eMUInfo;
    using eMUQuery = ::MUStateMachine__eMUQuery;
    using eMURequest = ::MUStateMachine__eMURequest;
  // Requests (MURequest) and the states that run them (MUStatesProcessRequests::Update).
  enum : int {
    kRequestLoadGame = 1, kRequestLoadHouse = 2, kRequestAutoSaveExisting = 3, kRequestAutoSaveNew = 4,
    kRequestSaveExistingGame = 5, kRequest6 = 6, kRequestTransitionAutoSave = 7, kRequestLoadConfig = 8,
  };
  // State ids (Startup). Results (MUPollForResult): 0 pending, 1 done, 2 failed or cancelled, 3 refused
  // (a quit to the main menu is pending). The states themselves return 2 for done and 1 for failed.
  enum : int {
    kStateProcessRequests = 0, kStateFormat = 1, kStateLoadGame = 2, kStateLoadHouse = 3, kStateSlotSelectSave = 4,
    kStateAutoSaveNew = 5, kStateSaveNewGame = 6, kStateAutoSaveExisting = 7, kStateSaveExistingGame = 8,
    kStateTransitionAutoSaveExisting = 9, kStateTransitionSaveExistingGame = 10, kStateSlotSelectSaveNoSkip = 11,
    kStateLoadConfig = 13, kStateDisplay = 14,
  };
    void Startup() override;  // 80093294
    void Shutdown() override;  // 80093684
    void Reset() override;  // 800936C0
    void Update(float dt) override;  // 80093798
    void Draw(ERC* rc) override;  // 800937B8
    static int MURequest(int request);  // 800937E8
    static int MUPollForResult(int request);  // 8009381C
    void MUWrapperStartup();  // 80093838
    void MUWrapperShutdown();  // 80093958
    static int GetCurRequest();  // 800939C0
    static void ClearResult();  // 800939C8
    static void SetHouseNumber(int house);  // 800939D4
    static void SetupQuitToMainMenu();  // 800939DC
    MUStateMachine(int id, int machineId);  // 80093A30
    ~MUStateMachine() override;  // 80093B3C
    static int GetHouseNumber();  // 80093BCC
    MUWrapper* GetMUWrapper();  // 80093BD4

  static int m_pendingRequest;  // the request waiting for MUStatesProcessRequests
  static int m_resultRequest;   // the request being run (or last run)
  static int m_result;          // its result
  static int m_houseNumber;
  // The rest of the MUSlotStatus object (804858D8, 0x4C bytes; the int MUSlotStatus, the card slot
  // being used, is its first word).
  static int s_curPlayer;                     // 804858DC
  static int s_playerCard[2];                 // 804858E0 the card slot each player saves to
  static const unsigned short* s_strings[4];  // 804858E8 two per player (SimsMemCardWrap)
  static int s_field8F8[2];                   // 804858F8
  static int s_field900;                      // 80485900
  static int s_field904;                      // 80485904
  static int s_field908;                      // 80485908 2: player 1 ... (SetupQuitToMainMenu)
  static int s_field90C;                      // 8048590C 2: player 2 ...
  static int s_field910;                      // 80485910
  static int s_field914;                      // 80485914
  static int s_field918;                      // 80485918
  static int s_cardUsed[2];                   // 8048591C set when a dialog for that player's card is shown

  MUWrapper* m_wrapper = nullptr;  // +0x98 never created in this build (MUWrapperStartup has no callers)
};

// "Licensed by Nintendo" on black for about 3 s, faded in and out (state 0).
struct TheSimsNGCLicenseState : StateMachineState {
  explicit TheSimsNGCLicenseState(int id);  // 8009ADCC
  ~TheSimsNGCLicenseState() override;  // 8009AE18
  void Startup() override;  // 80096B90
  void Shutdown() override;  // 80096CF0
  void Update(float dt) override;  // 80096D30
  void Draw(ERC* rc) override;  // 80096DFC

  ERFont* m_font = nullptr;          // +0x1C
  unsigned short m_text[22] = {};    // +0x20 UTF-16
  float m_brightness = 0;            // +0x4C 0..1, fades in over 0.5 s and out after 2 s
};

// Memory card check (state 1). In this build it only lets Apt read the pads and moves straight on.
struct TheSimsMemCardCheckState : StateMachineState {
  explicit TheSimsMemCardCheckState(int id);  // 8009AE4C
  ~TheSimsMemCardCheckState() override;  // 8009AE84
  void Startup() override;  // 80097090
  void Shutdown() override;  // 800970E8
  void Update(float dt) override;  // 800970EC
  void Draw(ERC* rc) override;  // 800970F0
};

// The EA logo movie (state 2).
struct TheSimsEAMovieState : StateMachineState {
  explicit TheSimsEAMovieState(int id);  // 8009AEB8
  ~TheSimsEAMovieState() override;  // 8009AEF4
  void Startup() override;  // 800971C8
  void Update(float dt) override;  // 800972E8
  void Shutdown() override;  // 80097348

  int m_playing = 0;  // +0x1C the movie was started (UI drawing is off meanwhile)
};

// The Sims 2 intro movie (state 4); any "skip" press fades the audio out and stops it.
struct TheSimsSimsIntroMovieState : StateMachineState {
  explicit TheSimsSimsIntroMovieState(int id);  // 8009AFB8
  ~TheSimsSimsIntroMovieState() override;  // 8009AFF4
  void Startup() override;  // 80097BF8
  void Shutdown() override;  // 80097D44
  void Update(float dt) override;  // 80097D8C

  int m_field1C = 0;   // +0x1C
  int m_skipping = 0;  // +0x20
  int m_playing = 0;   // +0x24
};

// The Maxis brand screen (state 3): "maxis_logo_brand" in a Wrapper text pane on black, faded in over
// 1 s, held until 6 s, faded out over 1 s.
struct TheSimsMaxisLogoState : StateMachineState {
  enum : unsigned int { kFadingIn = 1, kFadingOut = 2, kFadedIn = 4, kFadedOut = 8, kRunning = 0x10 };
  explicit TheSimsMaxisLogoState(int id);  // 8009AF28
  ~TheSimsMaxisLogoState() override;  // 8009AF84
  void Startup() override;  // 80097390
  void Shutdown() override;  // 80097800
  void Reset() override;  // 80097864
  void Update(float dt) override;  // 80097988
  void Draw(ERC* rc) override;  // 80097AF4

  Wrapper* m_wrapper = nullptr;    // +0x1C
  BackgroundPane* m_pane = nullptr;  // +0x20 one TextItem over the background shader 0x2a07c8b5
  float m_fadeDuration = 0;        // +0x24
  float m_fadeRate = 0;            // +0x28
  float m_fadeRemaining = 0;       // +0x2C
  unsigned int m_fadeFlags = 0;    // +0x30
};

// Loading what the main menu needs (state 6), behind the loading screen: the front-end sample
// banks and dataset 0x5012E600, the background loader, then the "mainmenu" dataset.
struct TheSimsEnterMainMenuState : StateMachineState {
  explicit TheSimsEnterMainMenuState(int id);  // 8009B028
  ~TheSimsEnterMainMenuState() override;  // 8009B064
  void Reset() override;  // 80097EF0
  void Shutdown() override;  // 80097FDC
  void Startup() override;  // 80098058
  void Update(float dt) override;  // 8009817C
  void Draw(ERC* rc) override;  // 800985F8
  void DrawTopmost(ERC* rc) override;  // 800985FC

  int m_entered = 0;  // +0x1C 0 on the first entry ("Startup"), then 1 ("Restart")
};

// The main menu (state 7): the Flash UI is updated and drawn, the MMU target answers the menu movie,
// and the menu's flow requests (credits, slot select, freeplay, CAS...) move the game on.
struct TheSimsMainMenuState : StateMachineState {
  explicit TheSimsMainMenuState(int id);  // 8009B098
  ~TheSimsMainMenuState() override;  // 8009B0D4
  void Startup() override;  // 80098600
  void Shutdown() override;  // 800986A0
  void Reset() override;  // 80098728
  void Update(float dt) override;  // 80098838
  static void LeavingMainMenuCleanup();  // 80098C6C
  void Draw(ERC* rc) override;  // 80098D00
  int IsFlowStateRequestPending();  // 8009B11C
  int GetFlowStateRequest();  // 8009B140
  void SetFlowStateCurrent();  // 8009B164
};
