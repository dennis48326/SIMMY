// @class CLoadingScreen__eLoadingScreenMode after StateMachine StateMachineState EShader ERShader E3DWindow PlumbBobModel ERModel
// @class CLoadingScreen after CLoadingScreen__eLoadingScreenMode
// @class LoadingScreenStateMachine after StateMachine CLoadingScreen
// @class LoadingScreenWaitingForLoadState after StateMachineState
// @class LoadingScreenFadeInState after StateMachineState
// @class LoadingScreenUpdateState after StateMachineState
// @class LoadingScreenFadeOutState after StateMachineState
// The loading screen (game/loadingscreen.cpp, game/loadingscreenstate.cpp). Code that starts a load
// calls LoadingScreenStateMachine::StartingLoad; the machine ('LdSc') fades a CLoadingScreen in, keeps
// it up while the load runs and fades it out after LoadFinishing. CLoadingScreen draws the background
// gradient, a spinning plumb bob in a 3D window and "Loading, please wait".
#pragma once
#include "simmy/enum.hpp"

struct CLoadingScreen__eLoadingScreenMode {
  SIMMY_ENUM_BODY(CLoadingScreen__eLoadingScreenMode)
};

// A 3D window (E3DWindow) the plumb bob is drawn in, over the background gradient.
struct CLoadingScreen : E3DWindow {
  using eLoadingScreenMode = ::CLoadingScreen__eLoadingScreenMode;
  explicit CLoadingScreen(eLoadingScreenMode mode);  // 80056A64
  ~CLoadingScreen() override;  // 80056BF0
  void InitAsync();  // 80056CE8
  void Update(float dt);  // 80056DB4
  void DrawTopmost(ERC* rc, float alpha);  // 80056E94
  int GetMode() const;  // 800578A4

  int m_initialised = 0;               // +0x350 set once the resources are in (InitAsync)
  int m_mode;                          // +0x354
  ERShader* m_background = nullptr;    // +0x358 s2c_bg_gradient
  ERModel* m_diamondModel = nullptr;   // +0x35C the plumb bob ("Diamond", 0x6d1f0956)
  ERModel* m_glowModel = nullptr;      // +0x360 the glow shell around it ("plum_glow", 0x6fac867f)
  PlumbBobModel* m_plumbBob = nullptr; // +0x364 draws the diamond, spinning
  float m_glowScaleT[2] = {};          // +0x368 per glow shell: 0..1 through the pulse
  int m_glowScaleDir[2] = {};          // +0x370 pulse direction
  float m_glowScale[2] = {1, 1};       // +0x378
  float m_glowRot[2] = {};             // +0x380 spin about y
  int m_showText = 0;                  // +0x388 "loading_please_wait" (modes 0..5)
  int m_resourcesState = 0;            // +0x38C 3 once all resources are referenced
};

struct LoadingScreenStateMachine : StateMachine {
  enum : int { kWaitingForLoad = 0, kFadeIn = 1, kUpdate = 2, kFadeOut = 3 };
  enum : unsigned int { kFadingIn = 1, kFadingOut = 2, kFadedIn = 4, kFadedOut = 8, kFaderRunning = 0x10 };

  LoadingScreenStateMachine(int id, int machineId);  // 800907CC
  ~LoadingScreenStateMachine() override;  // 80090900
  void Startup() override;  // 800909A4
  void Update(float dt) override;  // 80090AE4
  void CreateLoadingScreen();  // 80090B04
  void DeleteLoadingScreen();  // 80090B5C
  int IsReadyToLoad();  // 80090BB8
  int IsLoading();  // 80090BD0
  void StartLoad(CLoadingScreen__eLoadingScreenMode mode);  // 80090C1C
  void LoadFinishedNow();  // 80090D18
  static void StartingLoad(CLoadingScreen__eLoadingScreenMode mode);  // 80090D58
  static void LoadFinishing();  // 80090D8C
  static void LoadFinishNow();  // 80090DA4
  static int LoadInProgress();  // 80090DD0
  CLoadingScreen* GetLoadingScreen();  // 80090E04
  void LoadStarted();  // 80090E0C
  int IsLoadComplete();  // 80090E18
  void FaderStart(float seconds, bool fadeOut);  // 80090E20
  void FaderStop();  // 80090E88
  void FaderUpdate(float dt);  // 80090E94
  float FaderGetFadeOpacity();  // 80090F00
  static LoadingScreenStateMachine* GetSingleton();  // 80090F74
  static int IsFadingIn();  // 80090F7C
  static int IsFadedIn();  // 80090FA4
  static int IsFadingOut();  // 80090FB4
  static int IsFadedOut();  // 80090FC4
  void LoadFinished();  // 80090FD4

  static LoadingScreenStateMachine* s_pSM;

  CLoadingScreen* m_screen = nullptr;  // +0x98
  int m_mode = 0;                      // +0x9C
  int m_loadRequested = 0;             // +0xA0
  int m_loadComplete = 0;              // +0xA4
  float m_fadeDuration = 0;            // +0xA8
  float m_fadeRate = 0;                // +0xAC 1 / duration
  float m_fadeRemaining = 0;           // +0xB0
  unsigned int m_fadeFlags = 0;        // +0xB4
};

struct LoadingScreenWaitingForLoadState : StateMachineState {
  explicit LoadingScreenWaitingForLoadState(int id);  // 80090FE0
  void Update(float dt) override;  // 800901A4
  void FaderStart(float seconds);  // 80091018
  int IsReadyToLoad();  // 80091054
  LoadingScreenStateMachine* Owner() { return static_cast<LoadingScreenStateMachine*>(m_owner); }
};
struct LoadingScreenFadeInState : StateMachineState {
  explicit LoadingScreenFadeInState(int id);  // 80091098
  void Update(float dt) override;  // 8009027C
  void DrawTopmost(ERC* rc) override;  // 8009038C
  void FaderUpdate(float dt);  // 800910D0
  float FaderGetFadeOpacity();  // 80091140
  int IsLoadComplete();  // 800911B8
  LoadingScreenStateMachine* Owner() { return static_cast<LoadingScreenStateMachine*>(m_owner); }
};
struct LoadingScreenUpdateState : StateMachineState {
  explicit LoadingScreenUpdateState(int id);  // 800911EC
  void Update(float dt) override;  // 80090454
  void DrawTopmost(ERC* rc) override;  // 80090544
  void FaderStart(float seconds);  // 80091224
  float FaderGetFadeOpacity();  // 80091264
  int IsLoadComplete();  // 800912DC
  LoadingScreenStateMachine* Owner() { return static_cast<LoadingScreenStateMachine*>(m_owner); }
};
struct LoadingScreenFadeOutState : StateMachineState {
  explicit LoadingScreenFadeOutState(int id);  // 80091310
  void Update(float dt) override;  // 8009060C
  void DrawTopmost(ERC* rc) override;  // 80090704
  void FaderUpdate(float dt);  // 80091348
  float FaderGetFadeOpacity();  // 800913B8
  int IsLoadComplete();  // 80091430
  LoadingScreenStateMachine* Owner() { return static_cast<LoadingScreenStateMachine*>(m_owner); }
};
