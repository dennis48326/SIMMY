// Port of game/thesims.cpp (partial): TheSimsStateMachine, the game's top-level flow.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

StateMachineState* g_pTheSimsMainMenuState = nullptr;  // the main menu state, for direct access
int g_thesims80476244 = 0;  // DAT_80476244, cleared at startup (use not identified yet)

namespace {
// PC: most states are not ported yet. Their generated stand-ins have no (int id) constructor that
// sets the id, so they are built default and given their id here; a ported state is constructed with
// its real constructor instead.
template <class State>
State* NewState(int id) {
  auto* s = new State;
  s->m_id = id;
  return s;
}
}  // namespace

// 8009ACC4
TheSimsStateMachine::TheSimsStateMachine(int id, int machineId) : StateMachine(id, machineId) {}

// 80098DB4
// Compacts every heap, adds the states (see the ids in thesims.hpp; ids 13..19 were added in a
// different order), sets up the script substitution strings and starts at the Nintendo licence
// screen. The flow target handles "goto" requests from the UI.
void TheSimsStateMachine::Startup() {
  for (EAHeap* heap : {RootHeap(), MainHeap(), ResourceHeap(), AudioHeap(), VMHeap(), ShaderHeap(), TextureHeap(),
                       AnimationHeap(), ModelHeap(), FlashesHeap(), AptHeap(), DebugHeap(), CUIHeap()})
    heap->Compact();
  EAHeap::SetNextCheckPoint();
  AddState(new TheSimsNGCLicenseState(kNGCLicense));
  AddState(new TheSimsMemCardCheckState(kMemCardCheck));
  AddState(new TheSimsEAMovieState(kEAMovie));
  AddState(new TheSimsMaxisLogoState(kMaxisLogo));
  AddState(new TheSimsSimsIntroMovieState(kSimsIntroMovie));
  AddState(new TheSimsStartScreenStateMachine(kStartScreen, 0x53545254));  // 'STRT'
  AddState(new TheSimsEnterMainMenuState(kEnterMainMenu));
  g_pTheSimsMainMenuState = new TheSimsMainMenuState(kMainMenu);
  AddState(g_pTheSimsMainMenuState);
  AddState(NewState<PreGameState>(kPreGame));
  AddState(NewState<PreGameFreeplayState>(kPreGameFreeplay));
  AddState(NewState<PreGameCasPlayer1State>(kPreGameCasPlayer1));
  AddState(NewState<PreGameCasPlayer2State>(kPreGameCasPlayer2));
  AddState(NewState<PreGameCreateAFamilyState>(kPreGameCreateAFamily));
  // GC: an InLevelStoryState (an InLevelBaseState with an embedded UIDialog and four BStrings); that
  // class has no functions of its own in the map, so the base stands in until it is recovered.
  AddState(NewState<InLevelBaseState>(kInLevelStory));
  AddState(NewState<UrbzModeNewGameState>(kUrbzModeNewGame));
  AddState(NewState<UrbzCreditsState>(kUrbzCredits));
  AddState(NewState<LiveModeInitState>(kLiveModeInit));
  {
    auto* changeHouse = new LiveModeChangeHouseStateMachine;
    changeHouse->m_id = kLiveModeChangeHouse;
    changeHouse->m_machineId = 0x43484e47;  // 'CHNG'
    AddState(changeHouse);
  }
  AddState(NewState<InLevelCreateASimState>(kInLevelCreateASim));
  AddState(NewState<MoviePlayerEndgameMovieState>(kEndgameMovie));
  EGlobal::SetupSubstitutionStrings();
  g_thesims80476244 = 0;
  SetNextState(kNGCLicense, 0.0f);
  m_flowTarget = new FlowGotoTarget;
}

// 80099670
void TheSimsStateMachine::Shutdown() {
  delete m_flowTarget;
  m_flowTarget = nullptr;
  m_status.ShutdownCurState();
  delete _gd.m_mainMenuTarget;
  _gd.m_mainMenuTarget = nullptr;
}

// 80099708
void TheSimsStateMachine::Update(float dt) {
  if (_gd.SystemPreUpdate()) StateMachine::Update(dt);
  _gd.SystemPostUpdate();
}

// 80099764
// Machines' DrawTopmost passes only run on frames where the game drew.
void TheSimsStateMachine::Draw(ERC* rc) {
  if (!_gd.SystemPreDraw(rc)) {
    _gd.m_drawTopmost = 0;
  } else {
    _gd.m_drawTopmost = 1;
    StateMachine::Draw(rc);
  }
  _gd.SystemPostDraw(rc);
}

// 800997DC
// The screen the Flash UI asked for ("flowGoto") and not yet taken.
int TheSimsStateMachine::IsFlowStateRequestPending() {
  return m_flowTarget && m_flowTarget->IsStateRequestPending() ? 1 : 0;
}

// 80099804
int TheSimsStateMachine::GetFlowStateRequest() { return m_flowTarget ? m_flowTarget->GetStateRequest() : 0; }

// 8009981C
void TheSimsStateMachine::SetFlowStateCurrent() {
  if (m_flowTarget) m_flowTarget->SetPendingStateCurrent();
}

// ---------------------------------------------------------------------------------------------------
// TheSimsNGCLicenseState

// 8009ADCC
TheSimsNGCLicenseState::TheSimsNGCLicenseState(int id) : StateMachineState(id) {}

// 8009AE18
TheSimsNGCLicenseState::~TheSimsNGCLicenseState() = default;

// 80096B90
void TheSimsNGCLicenseState::Startup() {
  for (EAHeap* heap : {RootHeap(), MainHeap(), ResourceHeap(), AudioHeap(), VMHeap(), ShaderHeap(), TextureHeap(),
                       AnimationHeap(), ModelHeap(), FlashesHeap(), AptHeap(), DebugHeap(), CUIHeap()})
    heap->Compact();
  EAHeap::SetNextCheckPoint();
  GameData::SetUglyGraphicsBackgroundColor();
  m_font = static_cast<ERFont*>(_fontman.AddRef(static_cast<unsigned int>(EGlobal::GetGameFontID()), nullptr, 0));
  m_brightness = 0;
  const char* text = "Licensed by Nintendo";
  for (int i = 0; text[i]; ++i) m_text[i] = static_cast<unsigned char>(text[i]);
}

// 80096CF0
void TheSimsNGCLicenseState::Shutdown() {
  if (m_font) {
    m_font->DelRef();
    m_font = nullptr;
  }
}

// 80096D30
// Fade in (2x frame time per frame) for the first 2 s, then fade out; move on a second later.
void TheSimsNGCLicenseState::Update(float) {
  const float dt = m_owner->m_frameDt;
  if (m_owner->GetTimeInState() < 2.0f) {
    if (m_brightness < 1.0f) m_brightness += dt + dt;
  } else if (m_brightness > 0.0f) {
    m_brightness -= dt + dt;
  } else {
    m_brightness = 0;
    if (m_owner->GetTimeInState() > 3.0f) OwnerSetNextState(TheSimsStateMachine::kMemCardCheck, 0.0f);
  }
}

// 80096DFC
// Black, the text in white (size 24, centred at (0.5, 0.4)), then black over it at 1 - brightness.
void TheSimsNGCLicenseState::Draw(ERC* rc) {
  EShader* shader = _globals.m_globalShader->GetShader();
  EVec2 p0{0, 0}, p1{1, 1}, uv0{0, 1}, uv1{1, 0};
  EVec4 black{0, 0, 0, 1};
  shader->Select(rc, 0);
  rc->Rect(p0, p1, uv0, uv1, black, 0.0f);
  m_font->Select(rc);
  m_font->SetSize(24.0f, 1.0f, true);
  EVec4 white{1, 1, 1, 1};
  m_font->SetColor(white);
  m_font->DoDrawAlign(rc, m_text, true, EVec2{0.5f, 0.4f}, EFontAlignX::Center, EFontAlignY::Center, nullptr,
                      nullptr, EFontMatrixType::Scale, nullptr, true);
  shader->Select(rc, 0);
  EVec4 fade{0, 0, 0, 1.0f - m_brightness};
  rc->Rect(p0, p1, uv0, uv1, fade, 0.0f);
}

namespace {
void CompactAllHeaps() {
  for (EAHeap* heap : {RootHeap(), MainHeap(), ResourceHeap(), AudioHeap(), VMHeap(), ShaderHeap(), TextureHeap(),
                       AnimationHeap(), ModelHeap(), FlashesHeap(), AptHeap(), DebugHeap(), CUIHeap()})
    heap->Compact();
  EAHeap::SetNextCheckPoint();
}
// The movie states set the music volume (_pActualAudio vtable 0x60 / 0x68) to 1 and fade it on a skip.
// (The THP player's own volume is separate and stays at full.)
void SetMovieVolume(float volume) { _pActualAudio->SetMusicVolume(volume); }
float GetMovieVolume() { return _pActualAudio->GetMusicVolume(); }
// Debug switches in EGlobal's config block that skip the boot movies.
bool SkipEAMovie() { return _globals.ConfigInt(0x43C) != 0; }
bool SkipIntroMovie() { return _globals.ConfigInt(0x3A8) != 0; }
}  // namespace

// ---------------------------------------------------------------------------------------------------
// TheSimsMemCardCheckState

// 8009AE4C
TheSimsMemCardCheckState::TheSimsMemCardCheckState(int id) : StateMachineState(id) {}
// 8009AE84
TheSimsMemCardCheckState::~TheSimsMemCardCheckState() = default;

// 80097090
void TheSimsMemCardCheckState::Startup() {
  // GC: AptViewer::AllowControllerReading(_globals.m_aptViewer, true) (Apt not ported yet).
  OwnerSetNextState(TheSimsStateMachine::kEAMovie, 0.0f);
}
// 800970E8
void TheSimsMemCardCheckState::Shutdown() {}
// 800970EC
void TheSimsMemCardCheckState::Update(float) {}

// 800970F0
// Black over the whole screen.
void TheSimsMemCardCheckState::Draw(ERC* rc) {
  _globals.m_globalShader->GetShader()->Select(rc, 0);
  EVec2 p0{0, 0}, p1{1, 1}, uv0{0, 1}, uv1{1, 0};
  EVec4 black{0, 0, 0, 1};
  rc->Rect(p0, p1, uv0, uv1, black, 0.0f);
}

// ---------------------------------------------------------------------------------------------------
// TheSimsEAMovieState

// 8009AEB8
TheSimsEAMovieState::TheSimsEAMovieState(int id) : StateMachineState(id) {}
// 8009AEF4
TheSimsEAMovieState::~TheSimsEAMovieState() = default;

// 800971C8
void TheSimsEAMovieState::Startup() {
  CompactAllHeaps();
  SetMovieVolume(1.0f);
  if (!SkipEAMovie()) {
    gbDrawUI = gbUpdateUI = 0;
    m_playing = 1;
    _app.PlayMovie(static_cast<unsigned int>(GameData::GetLocalizedEAMovie()), -1, -1);
  } else {
    OwnerSetNextState(TheSimsStateMachine::kMaxisLogo, 0.0f);
  }
  GameData::SetBlackGraphicsBackgroundColor();
}

// 800972E8
void TheSimsEAMovieState::Update(float) {
  if (!_app.IsMovieStarting() && !_app.IsMoviePlaying()) OwnerSetNextState(TheSimsStateMachine::kMaxisLogo, 0.0f);
}

// 80097348
void TheSimsEAMovieState::Shutdown() {
  if (m_playing) {
    gbDrawUI = gbUpdateUI = 1;
    m_playing = 0;
  }
  GameData::SetUglyGraphicsBackgroundColor();
}

// ---------------------------------------------------------------------------------------------------
// TheSimsSimsIntroMovieState

// 8009AFB8
TheSimsSimsIntroMovieState::TheSimsSimsIntroMovieState(int id) : StateMachineState(id) {}
// 8009AFF4
TheSimsSimsIntroMovieState::~TheSimsSimsIntroMovieState() = default;

// 80097BF8
void TheSimsSimsIntroMovieState::Startup() {
  CompactAllHeaps();
  SetMovieVolume(1.0f);
  if (!SkipEAMovie() && !SkipIntroMovie()) {
    gbDrawUI = gbUpdateUI = 0;
    m_playing = 1;
    _app.PlayMovie(static_cast<unsigned int>(GameData::GetLocalizedSimsIntroMovie()), -1, -1);
    OwnerSetNextState(TheSimsStateMachine::kStartScreen, 10.0f);
  } else {
    OwnerSetNextState(TheSimsStateMachine::kStartScreen, 0.0f);
  }
  m_skipping = 0;
  GameData::SetBlackGraphicsBackgroundColor();
}

// 80097D44
void TheSimsSimsIntroMovieState::Shutdown() {
  if (m_playing) {
    gbDrawUI = gbUpdateUI = 1;
    m_playing = 0;
  }
  GameData::SetUglyGraphicsBackgroundColor();
}

// 80097D8C
// Command 0x1A (skip) on any pad starts a fade of the movie audio (2x frame time per frame); the movie
// stops when the volume runs out. The start screen follows when the movie is over.
void TheSimsSimsIntroMovieState::Update(float) {
  if (_pCtrlMan->GetCmdResultAllCtrl(0x1a, &EController::GetBtnPressedMask, &EController::GetBtnDownMask, false,
                                     nullptr, 0xffffffffu))
    m_skipping = 1;
  if (m_skipping) {
    float volume = GetMovieVolume() + m_owner->m_frameDt * -2.0f;
    bool done = false;
    if (volume > 1.0f) {
      volume = 1.0f;
      done = true;
    } else if (volume < 0.0f) {
      volume = 0.0f;
      done = true;
    }
    SetMovieVolume(volume);
    if (done) _app.StopMovie();
  }
  if (!_app.IsMovieStarting() && !_app.IsMoviePlaying()) OwnerSetNextState(TheSimsStateMachine::kStartScreen, 0.0f);
}

// ---------------------------------------------------------------------------------------------------
// TheSimsMaxisLogoState

// 8009AF28
TheSimsMaxisLogoState::TheSimsMaxisLogoState(int id) : StateMachineState(id) {}
// 8009AF84
TheSimsMaxisLogoState::~TheSimsMaxisLogoState() = default;

// 80097390
// A Wrapper (game/wrapper.cpp) with the default font and one pane: the background shader 0x2a07c8b5
// under one text item. The debug switch that skips the intro movies skips this screen too.
void TheSimsMaxisLogoState::Startup() {
  CompactAllHeaps();
  m_wrapper = new Wrapper;
  m_wrapper->WrapperStartup(nullptr);
  Wrapper::s_pDefaultFont = _globals.m_font ? _globals.m_font
                                            : static_cast<ERFont*>(_fontman.AddRef(
                                                  static_cast<unsigned int>(EGlobal::GetGameFontID()), nullptr, 0));
  m_pane = new BackgroundPane(0, 1);
  m_wrapper->AddPane(m_pane);
  m_pane->SetBackgroundShader(0x2a07c8b5);
  if (SkipIntroMovie()) OwnerSetNextState(TheSimsStateMachine::kSimsIntroMovie, 0.0f);
}

// 80097800
void TheSimsMaxisLogoState::Shutdown() {
  m_wrapper->WrapperShutdown();
  delete m_wrapper;
  m_pane = nullptr;
  m_wrapper = nullptr;
}

// 80097864
// The brand line (NewUIStrings "maxis_logo_brand"): size 12, white with a shadow, centred at
// (0.5, 0.88); the pane starts transparent and fades in over 1 s.
void TheSimsMaxisLogoState::Reset() {
  m_wrapper->WrapperReset();
  m_pane->m_flags |= WrapperPaneBase::kDrawEnabled;
  m_pane->ShowEnabledButtons();
  const unsigned short* const* text = _globals.GetNewUIString("maxis_logo_brand");
  m_pane->SetItemText(ItemType::Text, 0, text ? *text : nullptr);
  m_pane->SetItemFontSize(ItemType::Text, 0, 12.0f);
  m_pane->SetItemStyle(ItemType::Text, 0, ItemStyle::WhiteShadow);
  EVec2 pos{0.5f, 0.88f};
  m_pane->SetItemPos(ItemType::Text, 0, pos);
  m_fadeFlags = kRunning | kFadingIn;
  m_fadeRemaining = 1.0f;
  m_fadeDuration = 1.0f;
  m_fadeRate = 1.0f;
  m_pane->m_alpha = 0.0f;
}

// 80097988
// Runs the fader; the pane's alpha follows it. Six seconds in, fade out; once faded out, play the
// Sims intro movie.
void TheSimsMaxisLogoState::Update(float dt) {
  unsigned int flags = m_fadeFlags;
  if ((flags & kRunning) && m_fadeRemaining != 0.0f) {
    m_fadeRemaining -= dt;
    if (m_fadeRemaining <= 0.0f) {
      m_fadeRemaining = 0;
      if (flags & kFadingIn)
        m_fadeFlags = (flags & ~kFadingIn) | kFadedIn;
      else if (flags & kFadingOut)
        m_fadeFlags = (flags & ~kFadingOut) | kFadedOut;
    }
  }
  flags = m_fadeFlags;
  float alpha;
  if (flags & kFadingIn) {
    alpha = (m_fadeDuration - m_fadeRemaining) * m_fadeRate;
  } else if (flags & kFadingOut) {
    alpha = m_fadeRemaining * m_fadeRate;
  } else if (flags & kFadedOut) {
    OwnerSetNextState(TheSimsStateMachine::kSimsIntroMovie, 0.0f);
    return;
  } else {
    if (m_owner->GetTimeInState() < 6.0f) return;
    m_fadeRemaining = m_fadeDuration = m_fadeRate = 1.0f;
    m_fadeFlags = kRunning | kFadingOut;
    return;
  }
  m_pane->m_alpha = alpha;
}

// 80097AF4
void TheSimsMaxisLogoState::Draw(ERC* rc) {
  _globals.m_globalShader->GetShader()->Select(rc, 0);
  EVec2 p0{0, 0}, p1{1, 1}, uv0{0, 1}, uv1{1, 0};
  EVec4 black{0, 0, 0, 1};
  rc->Rect(p0, p1, uv0, uv1, black, 0.0f);
  m_wrapper->WrapperDraw(rc);
}

// ---------------------------------------------------------------------------------------------------
// TheSimsEnterMainMenuState

// 8009B028
TheSimsEnterMainMenuState::TheSimsEnterMainMenuState(int id) : StateMachineState(id) {}
// 8009B064
TheSimsEnterMainMenuState::~TheSimsEnterMainMenuState() = default;

// 80097EF0
void TheSimsEnterMainMenuState::Reset() {
  CompactAllHeaps();
  if (_globals.m_vibrate) _globals.m_vibrate->StopAllVibration();
  // PC: the players' model cache emptied on the way to the main menu (GC: ELiveMode::QuitToMainMenu does
  // it after the people are killed; this also covers the lot left for the ending movie, which never
  // empties it, so no cached model outlives the user selectors GamePlayShutdown deletes).
  ESim::DeletePlayerCachedModel();
  m_stage = 0;
  // GC: GameData::StageInitTiming(0, 7) (the loading meter's stage timing: not ported, nothing reads it).
  _gd.m_fieldE4 = 0;  // DAT_80475C68
  _gd.m_fieldE8 = 0;  // DAT_80475C6C free play
  _gd.SetTwoPlayerMode(false);
  // GC: DAT_80475F2C = 0 (never read).
  _gd.m_field84 = 0;  // DAT_80475C08
  _gd.m_fieldF0 = 0;  // DAT_80475C74 author mode
}

// 80097FDC
void TheSimsEnterMainMenuState::Shutdown() {}

// 80098058
void TheSimsEnterMainMenuState::Startup() {
  CompactAllHeaps();
  CompactAllHeaps();
  SIMMY_STUB("TheSimsEnterMainMenuState::Startup: _pResLoader vtable 0xE4 (flush pending loads)", 0x80098058);
  _datasetman.AddRefAsyncIfNotLoaded(const_cast<char*>("mainmenu"));
}

// 8009817C
// Stages: 0/1 front-end sample banks and dataset 0x5012E600 (2 waits for it), 3 the background
// loader's defaults, 4 the background loader done, 5 the "mainmenu" dataset; then the main menu.
// The background loader runs EGlobal::SetDefaults (stage 3).
void TheSimsEnterMainMenuState::Update(float) {
  const int stage = m_stage;
  bool done = false;
  switch (stage) {
    case 0:
    case 1: {
      if (stage == 0) ++m_stage;
      for (unsigned int id : {0x64ac15u, 0x374bdceau, 0x48ae94fu, 0xcb2fa7cdu, 0x5d6996bbu, 0x2aaff358u, 0x212edd24u,
                              0xae80f928u, 0xcd34abdcu, 0x76d81c2au})
        _pAudiosampleman->AddRefAsync(id);  // the front end's UI samples
      if (!_datasetman.GetRef(0x5012e600u)) {
        _datasetman.AddRefAsync(0x5012e600u);
        ++m_stage;
      } else {
        // GC: with the fixed world (Globs::pFixedWorld) already built the background steps are skipped
        ++m_stage;
      }
      break;
    }
    case 2:
      if (!_datasetman.GetRef(0x5012e600u)) break;
      ++m_stage;
      [[fallthrough]];
    case 3:
      if (!BG::s_bBGComplete) break;
      BGCall_SetDefaults();
      ++m_stage;
      break;
    case 4:
      if (!BG::s_bBGComplete) break;
      if (_globals.ConfigInt(0x434) && Globs_pSound && _globals.m_optionsRecon)
        Globs_pSound->SetOldFXVolume(static_cast<const signed char*>(_globals.m_optionsRecon)[0x1C]);
      SIMMY_STUB("TheSimsEnterMainMenuState::Update: EGraphics::DisplayTiming", 0x8009817C);
      ++m_stage;
      break;
    case 5:
      if (!_datasetman.GetRef(const_cast<char*>("mainmenu"))) break;
      _datasetman.DelRef(const_cast<char*>("mainmenu"), EResourceManager__DelRefMode{1});
      ++m_stage;
      done = true;
      OwnerSetNextState(TheSimsStateMachine::kMainMenu, 0.0f);
      ++m_stage;
      break;
    case 6:
      done = true;
      OwnerSetNextState(TheSimsStateMachine::kMainMenu, 0.0f);
      ++m_stage;
      break;
    default:
      return;
  }
  // The copyright notice starts fading out 5 s after it was fully shown.
  const unsigned int flags = _gd.m_copyrightFlags;
  if ((flags & 4) && !(flags & 8) && !(flags & 2) &&
      static_cast<float>(_pClockMan->GetSec(&_sysclock)) - _gd.m_copyrightShownAt > 5.0f)
    _gd.m_copyrightFlags = (flags & ~0xDu) | 2;
  if (done) {
    m_entered = 1;
    if (Globs_pSound) Globs_pSound->SetGameMode(cSoundPlayer::kFrontEnd);
  }
}

// 800985F8
void TheSimsEnterMainMenuState::Draw(ERC*) {}
// 800985FC
void TheSimsEnterMainMenuState::DrawTopmost(ERC*) {}

// ---------------------------------------------------------------------------------------------------
// TheSimsMainMenuState

// 8009B098
TheSimsMainMenuState::TheSimsMainMenuState(int id) : StateMachineState(id) { g_pTheSimsMainMenuState = this; }
// 8009B0D4
TheSimsMainMenuState::~TheSimsMainMenuState() = default;

// 80098600
void TheSimsMainMenuState::Startup() {
  SIMMY_STUB("TheSimsMainMenuState::Startup: HDDThread::SetPriorityHigh", 0x80098600);
  CompactAllHeaps();
  _globals.m_playerCheats = 0;  // DAT_80475E68: the button-code cheats are off again at the main menu
}

// 800986A0
void TheSimsMainMenuState::Shutdown() {
  if (auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer)) viewer->m_field3AC = -1;
  delete _gd.m_mainMenuTarget;
  _gd.m_mainMenuTarget = nullptr;
  delete g_pWorldMap;
  g_pWorldMap = nullptr;
}

// 80098728
// Game-play state reset, both players unmapped, Apt allowed to read the pads, and the Flash UI
// updated and drawn from now on.
void TheSimsMainMenuState::Reset() {
  SIMMY_STUB("TheSimsMainMenuState::Reset: front-end flags, NghResFile reset", 0x80098728);
  if (_globals.m_vibrate) _globals.m_vibrate->StopAllVibration();
  _gd.GamePlayReset();
  _pCtrlMan->UnmapPlayer(0);
  _pCtrlMan->UnmapPlayer(1);
  if (auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer)) {
    viewer->m_field3A8 = 0;
    viewer->AllowControllerReading(true);
    viewer->m_field3AC = 0;
  }
  g_thesims80476244 = 0;
  if (!_gd.m_mainMenuTarget) _gd.m_mainMenuTarget = new MMUTarget;
  if (!g_pWorldMap) {
    g_pWorldMap = new WorldMap();
  } else {
    g_pWorldMap->m_drawLogo = 1;
    g_pWorldMap->m_running = 1;
  }
  gbUpdateUI = 1;
  gbDrawUI = 1;
}

// 80098838
// A called state's return value restarts the menu (1) or goes back to the front end (state 6);
// otherwise the menu's flow requests are taken: credits (0x18), main menu (1), slot select (0x17),
// the slots' results (0x2E load accepted, 0x2F load cancelled, 0x31 boot check cancelled),
// boot check (0x30), free play / CAS (8, 3) behind the loading screen.
void TheSimsMainMenuState::Update(float) {
  const unsigned int flags = _gd.m_copyrightFlags;
  if ((flags & 4) && !(flags & 8) && !(flags & 2) &&
      static_cast<float>(_pClockMan->GetSec(&_sysclock)) - _gd.m_copyrightShownAt > 5.0f)
    _gd.m_copyrightFlags = (flags & ~0xDu) | 2;
  if (g_pWorldMap) {
    g_pWorldMap->Update();
    g_pWorldMap->m_drawLogo = 1;
  }
  if (_gd.m_field9C) _gd.m_field9C = 0;  // DAT_80475C20: a quit request left over
  // GC: DAT_804761A0 (never set in this build) would set DAT_80475C38.
  auto* owner = static_cast<TheSimsStateMachine*>(m_owner);
  if (const int ret = owner->m_returnVal) {
    owner->m_returnVal = 0;
    if (ret == 1) {
      Reset();
      return;
    }
    _gd.GamePlayShutdown();
    OwnerSetNextState(TheSimsStateMachine::kEnterMainMenu, 0.0f);
    return;
  }
  if (!owner->IsFlowStateRequestPending()) return;
  const int request = owner->GetFlowStateRequest();
  bool leaving = false;  // GC: the menu is left for the game; the world map goes
  if (request == FlowGotoTarget::kCredits) {
    owner->SetFlowStateCurrent();
    OwnerCallState(TheSimsStateMachine::kUrbzCredits);
  } else if (request == FlowGotoTarget::kMainMenu) {
    owner->SetFlowStateCurrent();
  } else if (request == FlowGotoTarget::kSlotSelect) {
    // the memory card slots (load, or the new game's boot check)
    owner->SetFlowStateCurrent();
    _gd.m_fieldD8 = 1;  // DAT_80475C5C
    if (!_gd.m_field54) {
      UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), 0);
      _gd.m_field54 = new M2MTarget;  // DAT_80475BD8
    }
  } else if (request == 0x2e || request == 0x2f || request == 0x31) {
    // MainMenuLoadAccept / MainMenuLoadCancel / MainMenuBootCheckCancel: the slots are done
    owner->SetFlowStateCurrent();
    delete static_cast<M2MTarget*>(_gd.m_field54);
    _gd.m_field54 = nullptr;
    if (auto* mmu = static_cast<MMUTarget*>(_gd.m_mainMenuTarget)) mmu->OnStartGameComplete(request == 0x2e);
  } else if (request == 0x30 || request == 3 || request == 8) {
    if (!LoadingScreenStateMachine::LoadInProgress()) {
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{1});
    } else if (LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn) {
      leaving = true;
      owner->SetFlowStateCurrent();
      LeavingMainMenuCleanup();
      OwnerCallState(request == 8 ? TheSimsStateMachine::kUrbzModeNewGame : TheSimsStateMachine::kPreGame);
    }
  }
  // GC: other requests are left pending.
  if (leaving) {
    if (!g_pWorldMap) return;
    delete g_pWorldMap;
    g_pWorldMap = nullptr;
  }
  if (g_pWorldMap) g_pWorldMap->m_running = 1;
}

// 80098C6C
void TheSimsMainMenuState::LeavingMainMenuCleanup() {
  _gd.m_fieldD8 = 1;  // DAT_80475C5C
  delete _gd.m_mainMenuTarget;
  _gd.m_mainMenuTarget = nullptr;
  delete static_cast<M2MTarget*>(_gd.m_field54);
  _gd.m_field54 = nullptr;
}

// 80098D00
void TheSimsMainMenuState::Draw(ERC* rc) {
  if (g_pWorldMap) g_pWorldMap->Draw(rc);
}

// 8009B11C
int TheSimsMainMenuState::IsFlowStateRequestPending() {
  return static_cast<TheSimsStateMachine*>(m_owner)->IsFlowStateRequestPending();
}
// 8009B140
int TheSimsMainMenuState::GetFlowStateRequest() { return static_cast<TheSimsStateMachine*>(m_owner)->GetFlowStateRequest(); }
// 8009B164
void TheSimsMainMenuState::SetFlowStateCurrent() { static_cast<TheSimsStateMachine*>(m_owner)->SetFlowStateCurrent(); }
