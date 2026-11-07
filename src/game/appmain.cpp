// Port of game/appmain.cpp (partial): the Sims application object.
// Still stubbed: UpdateReset, the game sound player in UpdateAudio, big screenshots.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

namespace simmy {
bool overlay_pauses_game();  // platform/pc_overlay.cpp: the F1 options overlay is open
}

ESimsApp _app;  // the application (global constructor keyed to gRebootTimer)

// 80004340
ESimsApp::ESimsApp() = default;

// 80004398
ESimsApp::~ESimsApp() = default;

char ESimsApp_m_nameOfNgh[0x11] = "DEFAULT.NGH";  // 804FB1D0 ESimsApp::m_nameOfNgh
static char rebootName[0x100];                   // 8046E6B0, argv[0] (used to reboot)
static int s_startLot = 0;                       // 804FD930
int bIsAutoTestingInitialized = 0;               // 804FD92C

// The game's controller command map (g_simsCtrlCommands, 59 entries): command -> EA button mask.
static EBtnToCmdAssoc g_simsCtrlCommands[] = {
    {0x27, 0x000000}, {0x28, 0x000000}, {0x29, 0x000000}, {0x12, 0x0000B0}, {0x13, 0x000004},
    {0x14, 0x000008}, {0x17, 0x000004}, {0x17, 0x000008}, {0x1C, 0x000040}, {0x1D, 0x004000},
    {0x1E, 0x001000}, {0x1F, 0x004000}, {0x20, 0x008000}, {0x21, 0x002000}, {0x1A, 0x000800},
    {0x1B, 0x000900}, {0x22, 0x000004}, {0x23, 0x000008}, {0x24, 0x800000}, {0x25, 0x200000},
    {0x26, 0x000800}, {0x01, 0x001000}, {0x02, 0x004000}, {0x03, 0x008000}, {0x04, 0x002000},
    {0x01, 0x010000}, {0x02, 0x040000}, {0x03, 0x080000}, {0x04, 0x020000}, {0x2A, 0x001000},
    {0x2B, 0x004000}, {0x2C, 0x008000}, {0x2D, 0x002000}, {0x0F, 0x100000}, {0x0F, 0x400000},
    {0x0F, 0x800000}, {0x0F, 0x200000}, {0x07, 0x000800}, {0x30, 0x000002}, {0x31, 0x000002},
    {0x10, 0x000040}, {0x11, 0x000001}, {0x15, 0x000010}, {0x16, 0x000020}, {0x19, 0x002000},
    {0x18, 0x000010}, {0x11, 0x000010}, {0x2E, 0x000020}, {0x2F, 0x000010}, {0x05, 0x000040},
    {0x06, 0x000080}, {0x08, 0x000002}, {0x09, 0x000020}, {0x0A, 0x000010}, {0x0B, 0x000004},
    {0x0C, 0x000008}, {0x0D, 0x000001}, {0x0E, 0x000010}, {0x7FFFFFFF, 0x000000},
};
static int g_simsCtrlNumCommands = 59;  // 804FD984

// Every heap compacted, then a new allocation checkpoint (an inline helper in the original).
static void CompactHeaps() {
  for (EAHeap* heap : {RootHeap(), MainHeap(), ResourceHeap(), AudioHeap(), VMHeap(), ShaderHeap(), TextureHeap(),
                       AnimationHeap(), ModelHeap(), FlashesHeap(), AptHeap(), DebugHeap(), CUIHeap()})
    heap->Compact();
  EAHeap::SetNextCheckPoint();
}

// 80003FEC
void ESimsApp::SetNghName(char* name) {
  if ((std::strstr(name, ".ngh") || std::strstr(name, ".NGH")) && std::strlen(name) > 4) {
    std::strncpy(ESimsApp_m_nameOfNgh, name, 0x10);
    ESimsApp_m_nameOfNgh[0xF] = 0;
  }
}

// 80004074
// Options: -lot <1..16> (starting lot), -ngh <file.ngh> (neighborhood), -test_listen / -test_NGC.
// An argument may also hold several space-separated words.
void ESimsApp::parseCommandLine() {
  const int argc = m_argc;
  char** argv = m_argv;
  rebootName[0] = 0;
  if (!argv || argc <= 0 || !argv[0]) return;
  std::strcpy(rebootName, argv[0]);
  for (int i = 0; i < argc;) {
    char* word = argv[i++];
    const bool haveNext = i < argc;
    int consumed = 0;
    while (word) {
      char* space = std::strchr(word, ' ');
      char* nextSpace = nullptr;
      if (space) {
        *space = 0;
        nextSpace = std::strchr(space + 1, ' ');
        if (nextSpace) *nextSpace = 0;
      }
      consumed = 0;
      if (*word == '-') {
        const char* opt = word + 1;
        char* value = haveNext ? argv[i] : nullptr;
        if (space) value = space + 1;
        if (value && !*value) value = nullptr;
        if (!strcasecmp(opt, "test_listen") || !strcasecmp(opt, "test_NGC")) {
          bIsAutoTestingInitialized = 1;
        } else if (!strncasecmp(opt, "lot", 3)) {
          const int lot = value ? std::atoi(value) : 0;
          if (static_cast<unsigned int>(lot - 1) < 16) s_startLot = lot;
          consumed = 1;
        } else if (!strncasecmp(opt, "ngh", 3)) {
          consumed = 1;
          if (value) SetNghName(value);
        }
      }
      word = space;
      if (consumed) {
        if (!space) break;
        consumed = 0;
        word = nextSpace;
      }
    }
    if (consumed) i += consumed;
  }
}

// 800044C0
RET_UNKNOWN ESimsApp::GetDefaultLanguage() { return 0; }

// 80004B8C
void ESimsApp::ShowInitialDisplay() {
  CompactHeaps();
  CompactHeaps();
}

// 800044C8
RET_UNKNOWN ESimsApp::Init() {
  const int language = static_cast<int>(GetDefaultLanguage());
  _quickdataman.FreeUnreferencedResources();
  g_quickdatamanLanguage = language;
  _shaderman.AddRef(0x7f5abc2du, nullptr, 0);  // s2c_bg_gradient
  const auto logo = static_cast<unsigned int>(GameData::GetLocalizedSims2Logo());
  _shaderman.AddRef(logo, nullptr, 0);
  ShowInitialDisplay();
  _shaderman.DelRef(logo, EResourceManager__DelRefMode::Cache);
  _levelman.Init(const_cast<char*>("levels"));
  CompactHeaps();
  _datasetman.AddRef(const_cast<char*>("introload"), nullptr, 0);
  _modelman.AddRef(0x6d1f0956u, nullptr, 0);
  _modelman.AddRef(0x6fac867fu, nullptr, 0);
  for (unsigned int anim : {0x7afe5eeeu, 0x3e07f4bfu, 0xcb815f53u, 0x5ccffda6u, 0xb5b49290u, 0x330210c3u,
                            0x3725ec30u, 0xc63f9c7du})
    _animman.AddRef(anim, nullptr, 0);
  g_effectsAttachmentMan.AddRefAll();
  g_soundtrackdataman.AddRefAll();
  SetNghName(const_cast<char*>("DEFAULT.NGH"));
  _pClockMan->Start(&g_playerCheats);
  if (!_globals.m_globalShader) _globals.m_globalShader = static_cast<ERShader*>(_shaderman.AddRef(0x1a18ca65u, nullptr, 0));
  parseCommandLine();
  // BString2::s_empty = "" (BString2 is not ported yet)
  CompactHeaps();
  _datasetman.AddRef(static_cast<unsigned int>(EGlobal::GetGameFontID()), nullptr, 0);
  CompactHeaps();
  _globals.LoadIntroRequirements();
  if (_globals.m_config[0x1A4 - 0x168] && !bIsAutoTestingInitialized)  // the auto-test script name
    bIsAutoTestingInitialized = 1;
  _pCtrlMan->MapControllerCommands(g_simsCtrlCommands, g_simsCtrlNumCommands);
  if (!Globs_pNghResFile) {
    Globs_pNghResFile = new NghResFile;
    if (Globs_pNghResFile) {
      if (!SerializeUIGameData_m_bInitialized)
        SerializeUIGameData_m_bInitialized = static_cast<int>(Globs_pNghResFile->RegisterSection(
            NghResFile__RegistryID{0x55697878}, SerializeUIGameData::Save, SerializeUIGameData::Load,
            SerializeUIGameData::SaveComplete));
      SerializeUIGameData::AllocateLoadBuffers();
    }
  }
  EyeToyClient::Init();
  auto* states = new EGameStateMan;
  m_gameStates = states;
  states->AddState(new DummyMode);
  states->AddState(new ELiveMode);
  GameData::SetBlackGraphicsBackgroundColor();
  m_field498 = 0;
  auto* window = new EWindow;
  m_window = window;
  if (window) {
    TRect<float> clip{0, 0, 1, 1};
    window->SetClip(clip);
  }
  initContinue();
  std::fprintf(stderr, "[ctgDump] Allocating CTGDump object memory\n");
  _datasetman.DelRef(const_cast<char*>("introload"), EResourceManager__DelRefMode::Cache);
  return 0;
}

// 80004C80
// After the engine is up: the simulator globals, then the state machines that run the game: 'SIMS'
// (the game flow), 'MUST' (memory-card requests) and 'LdSc' (the loading screen), and the memory card
// banner comments.
void ESimsApp::initContinue() {
  CompactHeaps();
  LoadSimulatorGlobs();
  StateMachineManager::Startup();
  StateMachineManager* manager = StateMachineManager::s_pStateMachineManager;
  auto* sims = new TheSimsStateMachine(0, 0x53494d53);  // 'SIMS'
  manager->AddMachine(sims);
  sims->SetFlag(1);
  sims->SetFlag(2);
  manager->AddMachine(new MUStateMachine(0, 0x4d555354));  // 'MUST'
  manager->AddMachine(new LoadingScreenStateMachine(0, 0x4c645363));  // 'LdSc'
  ENgcMemoryCard::SetComments(const_cast<unsigned short*>(EGlobal::NewUIText("ngc_ipl_banner_1")),
                              const_cast<unsigned short*>(EGlobal::NewUIText("ngc_ipl_banner_2")));
}

// ---- Per-frame update ----------------------------------------------------------------------------

// 80005448
// Player 1's pad: a frame with a new press goes to the cheat reader.
void ESimsApp::UpdateCheats() {
  if (!_pCtrlMan) return;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(0)));
  if (pad && pad->GetBtnPressedMask(0xffffffffu) != 0) g_playerCheats.Capture(pad);
}

// ---- PlayerCheats (the button-code reader) -------------------------------------------------------

// 80005D40
// A press of one button that some code uses joins the ring (cleared first after a wrong button or
// when a second has passed since the last press; the time is this stopwatch's, i.e. the game clock).
// Then every code whose buttons are all in the ring is looked for, as a run starting at each older
// slot; a whole run toggles that cheat (EGlobal::TogglePlayerCheat) and clears the ring. Returns 1 when
// a code was entered.
int PlayerCheats::Capture(EController* pad) {
  bool entered = false;
  const unsigned short pressed = static_cast<unsigned short>(pad->GetBtnPressedMask(0xffffffffu));
  if (!IsSingleButton(pressed) || (pressed & _globals.m_cheatButtons) == 0) {
    m_purge = 1;
    return 0;
  }
  const float nowMs = static_cast<float>(_pClockMan->GetSec(this) * 1000.0);
  const float last = m_lastPressMs;
  m_lastPressMs = nowMs;
  if (last + 1000.0f <= nowMs) m_purge = 1;
  if (m_purge) PurgeBtnMemory();
  m_buttons[m_index] = pressed;
  const unsigned short held = CreateBtnMask();
  for (int code = 0; code < 11; ++code) {
    const unsigned short mask = _globals.m_cheatCodeMask[code];
    if ((held & mask) != mask) continue;
    int start = m_index;
    while (GetNextIndex(start) != m_index) {
      if (m_buttons[start] == 0 || _globals.m_cheatCodes[code][0] == 0) continue;
      int at = start;
      bool match = false;
      for (int i = 0; i < 7; ++i) {
        // GC read code[i] before the i == 6 test (the next code's first button); the result is the same.
        if (i == 6 || _globals.m_cheatCodes[code][i] == 0) {
          match = true;
          break;
        }
        if (_globals.m_cheatCodes[code][i] != m_buttons[at]) break;
        GetNextIndex(at);
      }
      if (match) {
        entered = true;
        _globals.TogglePlayerCheat(code);
      }
    }
  }
  if (entered) {
    PurgeBtnMemory();
    return 1;
  }
  GetNextIndex(m_index);
  return 0;
}

// 80005F60
int PlayerCheats::GetNextIndex(int& index) {
  if (++index > 5) index = 0;
  return index;
}

// 80005F84
// Exactly one of the 16 low bits is set.
int PlayerCheats::IsSingleButton(unsigned short buttons) {
  int seen = 0;
  for (int bit = 0; bit < 16; ++bit) {
    if (!(buttons >> bit & 1)) continue;
    if (seen) return 0;
    seen = 1;
  }
  return seen;
}

// 80005FD4
void PlayerCheats::PurgeBtnMemory() {
  m_index = 0;
  m_purge = 0;
  for (unsigned short& b : m_buttons) b = 0;
}

// 80005FFC
unsigned short PlayerCheats::CreateBtnMask() {
  unsigned short mask = 0;
  for (unsigned short b : m_buttons) mask = static_cast<unsigned short>(mask | b);
  return mask;
}

// 800054BC
// The pads are read for Apt, then the Flash UI is updated.
void ESimsApp::UpdateApt() {
  if (!gbUpdateUI || !_globals.m_aptViewer) return;
  auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer);
  viewer->NewReadController(0, false, 0, true);
  viewer->UpdateAll();
}

// 80005514
void ESimsApp::UpdateGame() {
  // (The original skipped the state machines while a background save was in progress.)
  StateMachineManager::UpdateMachines(_dt);
  _gd.CopyrightUpdate(_dt);
}

// 800056F0
// The game's sound player (cSoundPlayer, Globs::pSound) while no movie plays, then the UI sounds.
void ESimsApp::UpdateAudio() {
  if (m_movieState == 0 && Globs_pSound) Globs_pSound->Update();
  UIAUDIO::Instance()->Update();
}

// 80005554
// Advances animated shaders. With the simulator running, shader time follows the simulation speed
// (paused/fast-forward); multi-state shaders take the clock and weather from the simulator and house,
// or from the world map.
void ESimsApp::UpdateShaders(float dt) {
  cSimulatorImpl* sim = Globs_pSimulator;
  if (dt == 0.0f) dt = _dt;
  int hour = 0, minute = 0, weather = 0;
  float stateDt = dt;
  if (!sim || sim->GetMode() != 1) {
    if (g_pWorldMap) {
      weather = g_pWorldMap->m_fieldD0;
      stateDt = g_pWorldMap->m_hourStep;
      hour = g_pWorldMap->m_hourInt;
      minute = g_pWorldMap->m_minute;
    }
  } else {
    dt = (!sim->IsPausedAtAll() && !sim->IsStopped()) ? static_cast<float>(dt * sim->GetSpeedMultiplier()) : 0.0f;
    stateDt = dt;
  }
  if (sim && _globals.m_house) {
    hour = sim->GetGlobal(0);
    minute = sim->GetGlobal(5);
    weather = _globals.m_house->m_field30 & 0xff;  // the byte at +0x33
  }
  EShader::UpdateAll(dt);
  ERShader::UpdateAll(stateDt, hour, minute, weather);
}

// 8000574C
// The state machines, then (on frames where the game drew) GameData's world view and the Apt UI.
void DrawGame(ERC* rc) {
  if (_pGfx->FrameBufferClearFlag()) StateMachineManager::DrawMachines(rc);
  if (_gd.m_drawTopmost) {
    if (_gd.m_casTarget) _gd.m_casTarget->Draw(rc);
    if (gbDrawUI && _globals.m_aptViewer) static_cast<AptViewer*>(_globals.m_aptViewer)->Draw(rc);
  }
}

// 80005808
void ESimsApp::UpdateDraw() {
  ERC* rc = _pGfx->Open(RCMode::Immediate);
  m_drawRC = rc;
  DrawGame(rc);
  StateMachineManager::DrawTopmostMachines(rc);
  _gd.CopyrightDraw(rc);
  simmy::dev_draw_overlay(rc);  // PC: optional renderer test pattern (--render-test)
  _pGfx->Close(rc);
  m_drawRC = nullptr;
  // m_bigScreenshot (TakeBigScreenshot) re-rendered the frame as a grid of tiles saved to
  // screenshot-x%dy%d.raw; not ported.
}

// 80005CAC
// Before a movie plays (EApp::PlayMovie): the scratch heap (EGlobal::SetupScratchHeap, empty on GC too).
void ESimsApp::SetupForMovie() { _globals.SetupScratchHeap(); }

// 80005CD4
// After it: EGlobal::FreeScratchHeap (empty on GC too).
void ESimsApp::CleanupAfterMovie() { _globals.FreeScratchHeap(); }

// 80005BA4
void ESimsApp::Update() {
  // Players 1 and 2 whose controller is unmapped are mapped to the first free pad pressing start
  // when m_field48C / m_field490 ask for it (the front end's "press start").
  auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer);
  if (m_field48C && !_pCtrlMan->GetPlayerMapped(0) && _globals.TryAutoMapPlayer(0) != -1 && viewer)
    viewer->m_field3AC = -1;
  if (m_field490 && !_pCtrlMan->GetPlayerMapped(1) && _globals.TryAutoMapPlayer(1) != -1 && viewer) {
    viewer->m_field3AC = -1;
    viewer->m_field3A8 = 1;
  }
  // PC: while the options overlay (F1) is open the game stands still; it is still drawn every frame.
  const bool paused = simmy::overlay_pauses_game();
  if (!paused) {
    UpdateCheats();
    UpdateApt();
    UpdateGame();
  }
  UpdateAudio();
  if (!paused) UpdateShaders(0.0f);
  SimImageMaker::ServiceDeferredImages();  // PC: portraits whose pixels come back from the GPU later
  UpdateDraw();
  UpdateReset();
}
