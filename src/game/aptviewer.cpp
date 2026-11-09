#include <cstdio>
#include <cstdlib>
// Port of game/aptviewer.cpp: the Flash UI player. Pad reading and the button filters are not
// ported yet (they wait on the Apt input queue).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/mp4.hpp"  // PC mod: 4 players

#include <cstdarg>
#include <cmath>
#include <cstring>

int PcPrivateScreenPlayer();  // uiscreens.cpp (PC co-op private screens)
bool PcSharedDialogShown();   // g2dtarget.cpp

AptViewer* _gpAptViewer = nullptr;
extern int iUseStencil;
int gAptOptFlags = 0;

namespace {
int s_aptTicks = 0;  // DAT_8047623C: milliseconds of Apt time so far
float s_repeatSeconds = 0;   // repeatSeconds: time since the previous UpdateAll
float s_lastUpdate = 0;      // DAT_804FF6EC
float s_repeatTime = 0;      // timeAcc_4980
int s_repeatCount[2][4];     // repeatCount (per player, direction)
int s_repeatRate[2][4];      // repeatRate
}  // namespace

// 80010038
AptViewer* CreateAptViewer() {
  _gpAptViewer = new AptViewer;
  return _gpAptViewer;
}

// 80010078
void _freeFontTable() {}

// 8001007C
void closeFunc() {
  AptAuxPCEorGL_Shutdown();
  _freeFontTable();
  AptShutdown(0);
}

// 800100A8
// Apt advances by the frame's milliseconds, at most 66 per frame.
void updateFunc() {
  AptAuxPCEorGL_TimerStartFrame();
  int ticks = AptAuxPCEorGL_TimerGetTicks();
  if (ticks < 0) ticks = 0;
  if (ticks > 0x42) ticks = 0x42;
  AptUpdate(ticks);
  s_aptTicks += ticks;
}

// 80010108
// GC: AptInitialize took the pool sizes (0x200 objects, 0x200 strings, ...) and the XML
// implementor was installed; the PC runtime sizes itself and builds XML objects natively.
void StartItAll() {
  AptAuxPCEorGL_Initialize(_pGfx->m_screenWidth, _pGfx->m_screenHeight, nullptr);
  AptInitialize(nullptr, false);
  AptAuxPCEorGL_TimerInit();
}

// 800101F0
AptViewer::AptViewer() {
  m_clock = new EClock;
  _pClockMan->Start(m_clock);
  gAptOptFlags |= 2;
}

// 800102E0
AptViewer::~AptViewer() {
  UnLoad();
  closeFunc();
  delete m_clock;
  m_clock = nullptr;
}

// 80010358
// Empty in this build (the release game calls the movie through NewCallFunction).
void AptViewer::CallFunction(char*, char*, char*, int, ...) {}

// 80010394
// Up to five string arguments.
void AptViewer::NewCallFunction(char* function, char* unused, char* target, int argc, ...) {
  if (!m_loaded) return;
  char* argv[5] = {};
  va_list ap;
  va_start(ap, argc);
  for (int i = 0; i < argc && i < 5; ++i) argv[i] = va_arg(ap, char*);
  va_end(ap);
  NewCallFunction2(function, unused, target, argc, argv);
}

// 800104B0
void AptViewer::NewCallFunction2(char* function, char* unused, char* target, int argc, char** argv) {
  if (argc < 0 || argc > 5) return;
  AptCallFunction(function, unused, target, argc, argv);
}

// 800105B4
void AptViewer::Shutdown() {
  UnLoad();
  closeFunc();
}

// 800105D8
void AptViewer::Init(UIObjectBase* handler) {
  if (!m_started) StartItAll();
  m_started = 1;
  AptAuxPCEorGL_SetUiObject(handler);
  m_filterTop[0] = m_filterTop[1] = 0;
}

// 80010640
void AptViewer::Update() {}

// 80010644
void AptViewer::UpdateAll() {
  if (!m_loaded) return;
  s_repeatSeconds = static_cast<float>(_pClockMan->GetSec(&_sysclock)) - s_lastUpdate;
  m_mutex.Acquire(0xffffffffu);
  UIUpdateTable::Instance()->ExecuteUpdatesPreFlash();
  updateFunc();
  UIUpdateTable::Instance()->ExecuteUpdatesPostFlash();
  m_mutex.Release();
  s_lastUpdate = static_cast<float>(_pClockMan->GetSec(&_sysclock));
}

// 80010778
// The first draw sets an orthographic projection over the stage (pixels, y down) seen from z = 35.
// GC: on screens wider than 640 the stage was scaled with SetOrthoProjectionForUIHD.
void AptViewer::Draw(ERC* rc) {
  rc->ProcessFrameEffects(0, 0);
  if (!m_loaded) return;
  static_cast<ENgcRC*>(rc)->TextureCacheInvalidate();
  m_mutex.Acquire(0xffffffffu);
  UIDrawTable::Instance()->ExecuteDrawsPreFlash(rc);
  AptAuxPCEorGL_SetRC(rc);
  int w = 0, h = 0;
  AptGetAnimationSize(&w, &h);
  if (w && h && !m_projectionSet) {
    m_stageRect[0] = 0;
    m_stageRect[1] = 0;
    m_stageRect[2] = w;
    m_stageRect[3] = h;
    if (w < static_cast<int>(_pGfx->GetActualScreenXSize()))
      SIMMY_STUB("AptViewer::Draw: SetOrthoProjectionForUIHD (screens wider than the stage)", 0x80010778);
    SetOrthoProjection(0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f, 1000.0f);
    EVec3 eye{0.0f, 0.0f, 35.0f}, target{0.0f, 0.0f, 0.0f}, up{0.0f, 1.0f, 0.0f};
    SetLookAt(eye, target, up);
    m_projectionSet = 1;
  }
  Select(rc);
  iUseStencil = 0;
  rc->StencilClear(0.0f, 0.0f, 1.0f, 1.0f, 0);
  AptAuxPCEorGL_VertexMatrixId();
  rc->DisableGeometryModes(6);
  AptRender();
  UIDrawTable::Instance()->ExecuteDrawsPostFlash(rc);
  m_mutex.Release();
}

// 80010C14
void AptViewer::Load(char* name, bool wait) {
  if (!name || !*name) return;
  if (m_loaded) UnLoad();
  AptAuxPCEorGL_LoadFile(name);
  if (wait) AptAuxPCEorGL_WaitOn(name);
  std::strncpy(m_movieName, name, sizeof m_movieName - 1);
  m_loaded = 1;
  AptSetValidFocusButton();
}

// 80010CF0
// Unloads _level0 and lets the player settle.
void AptViewer::UnLoad() {
  if (!m_loaded) return;
  AptLoadAnimation("", "_level0");
  for (int i = 0; i < 5; ++i) AptUpdate(500);
  m_loaded = 0;
}

// 80011DCC
void AptViewer::NewSetMessageHandler(UIObjectBase* handler) { AptAuxPCEorGL_SetUiObject(handler); }
// 80011DF0
void AptViewer::NewRemoveMessageHandler(UIObjectBase* handler) { AptAuxPCEorGL_RemoveUiObject(handler); }

// 80011E1C
bool AptViewer::AllowControllerReading(bool allow) {
  const bool previous = m_readController != 0;
  m_readController = allow;
  return previous;
}

// 80010D68
int AptViewer::ReadController(int, bool, int, bool) { return 0; }

// 80010D70
void AptViewer::RepeatClear(int player, int direction) {
  s_repeatCount[player][direction] = 0;
  s_repeatRate[player][direction] = 0x16;
}

// 80010DAC
// A held direction repeats: every 1/30 s counts, the first repeat after 22 counts, then 3 fewer each
// time down to 12.
bool AptViewer::RepeatCheck(int player, int direction) {
  s_repeatTime += s_repeatSeconds;
  int steps = static_cast<int>(s_repeatTime / 0.033333335f);
  s_repeatTime = std::fmod(s_repeatTime, 0.125f);
  if (steps > 4) steps = 4;
  int& count = s_repeatCount[player][direction];
  int& rate = s_repeatRate[player][direction];
  count += steps;
  if (count < rate) return false;
  count = 0;
  rate -= 3;
  if (rate < 12) rate = 12;
  return true;
}

// 80010E7C
// Each pad goes to Apt as the player it is mapped to, or (unmapped) as the viewer's default player.
int AptViewer::NewReadController(int, bool, int, bool) {
  for (int c = 0; c < 4; ++c) {
    if (_pCtrlMan->GetPlayerMapped(0) && _pCtrlMan->GetPlayerControllerIndex(0) == c)
      OnePlayerReadController(0, c);
    else if (_pCtrlMan->GetPlayerMapped(1) && _pCtrlMan->GetPlayerControllerIndex(1) == c)
      OnePlayerReadController(1, c);
    else if (Mp4PadKeptFromUi(c))
      continue;  // PC mod: 4 players: an extra player's pad, or one that may join (its Start joins)
    else if (m_field3AC != -1)
      OnePlayerReadController(m_field3AC, c);
  }
  return 0;
}

// 80010F40
// The pad's commands become Apt input events of controller player+2: the d-pad (commands 1-4, or
// 0x2A-0x2D when analog navigation is off) as up 0x0E, down 0x0F, left 1, right 2 on press and
// with auto-repeat while held; then the buttons as key codes (select 'x', cancel 't', ...).
int AptViewer::OnePlayerReadController(int player, int controller) {
  if (!m_loaded || !m_readController || LoadingScreenStateMachine::LoadInProgress()) return 0;
  const int apt = player + 2;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(controller));
  const unsigned int filter = GetCurrentAptButtonFilter(player);
  const bool noAnalog = UIDB::UIDBGetInt(const_cast<char*>("DisableAnalogNavigation")) != 0;
  bool navigate = false, worldView = false;
  if (_gd.m_casTarget) {
    navigate = worldView = true;
  } else if (_gd.m_h2dTarget || false /* G2DTarget::IsG2DActive: G2DTarget not ported */ || _gd.m_field4C || !_gd.m_field90 ||
             _gd.m_field40) {
    navigate = true;
  }
  auto pressed = [&](unsigned int cmd) {
    return pad->GetCmdResult(cmd, pad->GetBtnPressedMask(filter), pad->GetBtnDownMask(filter), false) != 0;
  };
  // PC co-op private screens: the movie gives its input to the screen on top (food creation), so the
  // other player's HUD buttons (the H2D screen: the d-pad's side panels, B's cancel, A in cancel
  // mode, Start) would never reach it. Their presses go to H2DTarget directly, as the movie would
  // send them ("<player>" as the value); the movie gets none of them. Not while the dialog box shows
  // a dialog for both players (it takes their input as on the GC).
  const int privateOwner = PcPrivateScreenPlayer();  // uiscreens.cpp
  if (privateOwner >= 0 && player != privateOwner && pad && _gd.m_h2dTarget && !PcSharedDialogShown()) {
    const char value[2] = {static_cast<char>('0' + player), 0};
    static const struct {
      unsigned int command;
      const char* name;
    } kH2D[] = {{0x2A, "H2D_DPAD_UP"}, {0x2B, "H2D_DPAD_DOWN"}, {0x2C, "H2D_DPAD_LEFT"}, {0x2D, "H2D_DPAD_RIGHT"},
                {7, "H2D_PAUSE"},     {6, "H2D_CANCEL"},       {5, "H2D_SELECT"}};
    for (const auto& b : kH2D)
      if (pressed(b.command)) _gd.m_h2dTarget->SetVariable(const_cast<char*>(b.name), const_cast<char*>(value));
    return 0;
  }
  auto down = [&](unsigned int cmd) {
    return pad->GetCmdResult(cmd, pad->GetBtnDownMask(filter), pad->GetBtnDownMask(filter), false) != 0;
  };
  static const int kEvent[4] = {0x0E, 0x0F, 1, 2};  // up, down, left, right
  if (!navigate) {
    for (int d = 0; d < 4; ++d) RepeatClear(player, d);
  } else {
    const unsigned int base = noAnalog ? 0x2A : 1;
    // Pressed this frame: up/down win over each other in order, a diagonal sends both.
    bool dir[4];
    for (int d = 0; d < 4; ++d) dir[d] = pressed(base + static_cast<unsigned int>(d));
    bool send[4] = {};
    send[0] = dir[0];
    send[1] = !dir[0] && dir[1];
    send[2] = dir[2] && !dir[3];
    send[3] = dir[3];
    for (int d = 0; d < 4; ++d)
      if (send[d]) AptAddToInputQueue(kEvent[d], 0, apt);
    // Held: auto-repeat.
    for (int d = 0; d < 4; ++d) dir[d] = down(base + static_cast<unsigned int>(d));
    for (int d = 0; d < 4; ++d)
      if (!dir[d]) RepeatClear(player, d);
    bool hold[4] = {};
    hold[0] = dir[0];
    hold[1] = !dir[0] && dir[1];
    hold[2] = dir[2] && !dir[3];
    hold[3] = dir[3];
    for (int d = 0; d < 4; ++d)
      if (hold[d] && RepeatCheck(player, d)) AptAddToInputQueue(kEvent[d], 0, apt);
  }
  static const struct {
    unsigned int command;
    int event;
  } kButtons[] = {{8, 300}, {7, 0x12D}, {5, 0x78}, {6, 0x74}, {0xB, 0x132}, {0xC, 0x133},
                  {0xD, 0x134}, {0xE, 0x135}, {9, 99}};
  for (const auto& b : kButtons)
    if (pressed(b.command)) AptAddToInputQueue(b.event, 0, apt);
  if (pressed(10)) AptAddToInputQueue(0x6F, 0, apt);
  else if (down(10) && worldView) AptAddToInputQueue(0x75, 0, apt);
  return 0;
}

// 80011A48
// A new filter on the player's pad (hiding everything until commands are added to it).
unsigned int AptViewer::PushAptButtonFilter(int player, char* name) {
  if (!m_field3A8) return 0;
  if (player > 1) player = 0;
  int top = m_filterTop[player];
  if (top >= 10) return 0;
  EController* pad = GetPlayersController(player);
  if (!pad) return 0;
  if (m_filters[player][top].id) ++top;
  if (top >= 10) return 0;
  m_filterTop[player] = top;
  const unsigned int id = pad->AddFilter(0, name, 100);
  m_filters[player][top] = {id, name};
  static const bool trace = std::getenv("SIMMY_UI_TRACE") != nullptr;  // PC dev trace
  if (trace) std::printf("[filter] p%d push %s -> %u (top %d)\n", player + 1, name ? name : "?", id, top);
  return id;
}

// 80011B4C
void AptViewer::PopAptButtonFilter(int player, unsigned int) {
  if (!m_field3A8) return;
  if (player > 1) player = 0;
  const int top = m_filterTop[player];
  if (top >= 10 || !m_filters[player][top].id) return;
  EController* pad = GetPlayersController(player);
  if (!pad) return;
  static const bool trace = std::getenv("SIMMY_UI_TRACE") != nullptr;  // PC dev trace
  if (trace) std::printf("[filter] p%d pop %u (top %d)\n", player + 1, m_filters[player][top].id, top);
  pad->RemoveFilter(m_filters[player][top].id);
  m_filters[player][top] = ButtonFilter();
  if (top - 1 >= 0) m_filterTop[player] = top - 1;
}

// 80011C44
void AptViewer::AddCmdToAptButtonFilter(int player, unsigned int id, int command) {
  if (!m_field3A8) return;
  if (player > 1) player = 0;
  if (command < 0x32)
    if (EController* pad = GetPlayersController(player)) pad->AddCmdToFilter(id, static_cast<unsigned int>(command));
}

// 80011CA8
void AptViewer::RemoveCmdFromAptButtonFilter(int player, unsigned int id, int command) {
  if (!m_field3A8) return;
  if (player > 1) player = 0;
  if (command < 0x32)
    if (EController* pad = GetPlayersController(player))
      pad->RemoveCmdFromFilter(id, static_cast<unsigned int>(command));
}

// 80011D0C
unsigned int AptViewer::GetCurrentAptButtonFilter(int player) {
  if (!m_field3A8) return 0xffffffffu;
  if (player > 1) player = 0;
  return m_filters[player][m_filterTop[player]].id;
}

// 80011D50
// The player's mapped pad, or (no players mapped yet) the pad in use.
EController* AptViewer::GetPlayersController(int player) {
  int index;
  if (!_pCtrlMan->GetPlayerMapped(static_cast<unsigned int>(player))) {
    index = _pCtrlMan->FindActiveController();
    if (index == -1) return nullptr;
  } else {
    index = _pCtrlMan->GetPlayerControllerIndex(static_cast<unsigned int>(player));
  }
  return _pCtrlMan->GetController(static_cast<unsigned int>(index));
}

// 80011E14
int AptViewer::UIOn() { return 1; }
// 80011E2C
bool AptViewer::IsWaitingForPlayerInput() { return m_loaded && m_readController; }
// 80011E58
bool AptViewer::IsReadControllerActive() { return m_readController != 0; }
