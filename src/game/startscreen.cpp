// Port of game/startscreen.cpp and the start screen state (linkonce/startscreen_*): the "press start"
// screen after the intro movie.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

StartScreenVars* TheSimsStartScreenStateMachine::s_pVars = nullptr;
extern int g_thesims80476244;  // DAT_80476244 (game/thesims.cpp)

namespace {
// s_press_start_color (80485924), set by the file's static initialiser (8009591C).
EVec4 s_press_start_color{0.47f, 0.784f, 0.35f, 1.0f};
StartScreenVars*& Vars() { return TheSimsStartScreenStateMachine::s_pVars; }
// The options' music volume, 0..10 (_globals.m_optionsRecon +0x1D). PC: OptionsRecon is created by
// EGlobal::LoadIntroRequirements, not ported yet; until then its default (10).
int MusicVolumeOption() {
  const auto* options = static_cast<const signed char*>(_globals.m_optionsRecon);
  return options ? options[0x1D] : 10;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// StartScreenVars

// 800954B0
// A window clipped to a 32-pixel strip under the "press start" text. (GC: the vars are not cleared
// before Startup, so the rect came from whatever the heap held; on PC they start at zero.)
void StartScreenVars::Startup() {
  m_window = new EWindow;
  const float half = m_textWidth * 0.5f;
  TRect<float> clip(m_textX - half, m_textY, m_textX + half,
                    m_textY + 32.0f / static_cast<float>(_pGfx->m_screenHeight));
  m_window->SetClip(clip);
}

// 80095570
void StartScreenVars::Shutdown() { delete m_window; }

// 800955B4
void StartScreenVars::Reset() {
  m_timeInStart = 0;
  m_field04 = 15.0f;
  m_showPressStart = 1;
  m_textX = 0.5f;
  m_textY = 0.805f;
  m_textWidth = 0.6f;
  m_field30 = 0;
  m_time = 0;
  m_blinkTime = 0;
  m_field1C = 0;
  m_field24 = 0;
  m_field28 = 0;
  m_field2C = 0;
}

// 80095638
void StartScreenVars::Update(float dt) {
  m_time += dt;
  m_blinkTime += dt;
  if (m_blinkTime < 0.43f) return;
  m_blinkTime -= 0.43f;
  m_showPressStart ^= 1;
}

// 8009567C
// "press_start_ngc" in the game font at size 25, centred at (0.5, 0.775), with a 2-pixel drop shadow.
void StartScreenVars::DrawPressStart(ERC* rc) {
  ERFont* font = _globals.m_font;
  if (!m_showPressStart) return;
  font->SetSize(25.0f, 1.0f, true);
  font->Select(rc);
  font->m_color = s_press_start_color;
  const unsigned short* const* text = _globals.GetNewUIString("press_start_ngc");
  EVec2 pos{0.5f, 0.775f};
  font->DrawDs(rc, const_cast<unsigned short*>(text ? *text : nullptr), pos, EFontAlignX::Center,
               EFontAlignY::Top, nullptr, 2.0f, 1.0f, true);
}

// ---------------------------------------------------------------------------------------------------
// TheSimsStartScreenStateMachine

// 80095970
TheSimsStartScreenStateMachine::TheSimsStartScreenStateMachine(int id, int machineId)
    : StateMachine(id, machineId) {}

// 80095780
// One state (the start state, id 0); the "startscreen" dataset is preloaded and the loading screen
// told that the load in progress (the intro) is finishing.
void TheSimsStartScreenStateMachine::Startup() {
  Vars() = new StartScreenVars;
  Vars()->Startup();
  AddState(new StartScreenStartState);
  _datasetman.PreloadResource(const_cast<char*>("startscreen"));
  LoadingScreenStateMachine::LoadFinishing();
}

// 80095824
void TheSimsStartScreenStateMachine::Shutdown() {
  m_status.ShutdownCurState();
  DeleteAllStates();
  Vars()->Shutdown();
  delete Vars();
  Vars() = nullptr;
}

// 80095874
void TheSimsStartScreenStateMachine::Reset() {
  Vars()->Reset();
  g_thesims80476244 = 1;
  _pCtrlMan->MapPlayerToController(0, 0);
  SetNextState(0, 0.0f);
}

// 800958D4
void TheSimsStartScreenStateMachine::Update(float dt) {
  Vars()->Update(dt);
  StateMachine::Update(dt);
}

// ---------------------------------------------------------------------------------------------------
// StartScreenStartState

// 803A0F54
// Starts the copyright notice fading in on first entry and references the background and logo.
void StartScreenStartState::Startup() {
  // GC: HDDThread::SetPriorityHigh(&g_hddThread) first (the GC HDD streaming thread; none on PC).
  StartScreenVars* vars = Vars();
  const float timeInStart = vars->m_timeInStart;
  vars->m_field2C = 1;
  vars->m_time = 0;
  vars->m_showPressStart = 1;
  vars->m_blinkTime = 0;
  if (timeInStart == 0.0f) _gd.m_copyrightFlags = (_gd.m_copyrightFlags & ~0xEu) | 1;
  m_background = static_cast<ERShader*>(_shaderman.AddRefAsync(0x7f5abc2du));
  m_logo = static_cast<ERShader*>(
      _shaderman.AddRefAsync(static_cast<unsigned int>(GameData::GetLocalizedSims2Logo())));
}

// 803A1000
void StartScreenStartState::Shutdown() {
  _shaderman.DelRefAsync(0x7f5abc2du);
  m_background = nullptr;
  _shaderman.DelRefAsync(static_cast<unsigned int>(GameData::GetLocalizedSims2Logo()));
  m_logo = nullptr;
}

// 803A1060
// After 4 s, start (command 0x26) or the config's auto-start byte asks the router for the main
// menu. When that flow request arrives the title music starts and the loading screen is brought up;
// once it has faded in, TheSimsStateMachine moves on to kEnterMainMenu.
void StartScreenStartState::Update(float) {
  if (!m_background) m_background = static_cast<ERShader*>(_shaderman.GetRef(0x7f5abc2du));
  if (!m_logo)
    m_logo = static_cast<ERShader*>(
        _shaderman.GetRef(static_cast<unsigned int>(GameData::GetLocalizedSims2Logo())));

  if (!LoadingScreenStateMachine::LoadInProgress()) {
    Vars()->m_timeInStart += _dt;
    const int start = _pCtrlMan->GetCmdResultAllCtrl(0x26, &EController::GetBtnPressedMask,
                                                     &EController::GetBtnDownMask, true, nullptr, 0);
    const unsigned char autoStart = _globals.m_config[0x384 - 0x168];
    if ((start || autoStart != 0xff) && Vars()->m_timeInStart >= 4.0f)
      UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MainMenu"));
    if (IsFlowStateRequestPending() && GetFlowStateRequest() == 1) {
      SetFlowStateCurrent();
      UIAUDIO::Instance()->PlaySound(0x374bdceau);
      EPMDesc music(const_cast<char*>("music_48_title_track"), false);
      _pActualAudio->PlayMusic(music);
      _pActualAudio->SetMusicVolume(static_cast<float>(MusicVolumeOption()) / 10.0f);
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{0});
    }
  } else {
    // While the loading screen is up: let the copyright notice start fading out 5 s after it was
    // fully shown, and leave once the loading screen has faded in.
    const unsigned int flags = _gd.m_copyrightFlags;
    if ((flags & 4) && !(flags & 8) && !(flags & 2)) {
      if (static_cast<float>(_pClockMan->GetSec(&_sysclock)) - _gd.m_copyrightShownAt > 5.0f)
        _gd.m_copyrightFlags = (flags & ~0xDu) | 2;
    }
    if (LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn)
      m_owner->OwnerSetNextState(TheSimsStateMachine::kEnterMainMenu, 0.0f);
  }
}

// 803A1328
// The background gradient over the whole screen, the logo in the middle, then "press start" once
// the state has been up for 4 s.
void StartScreenStartState::Draw(ERC* rc) {
  EVec2 uv0{0, 1}, uv1{1, 0};
  EVec4 white{1, 1, 1, 1};
  if (m_background) {
    m_background->GetShader()->Select(rc, 0);
    EVec2 p0{0, 0}, p1{1, 1};
    rc->Rect(p0, p1, uv0, uv1, white, 0.0f);
  }
  if (m_logo) {
    m_logo->GetShader()->Select(rc, 0);
    EVec2 p0{0.3f, 0.2142857f}, p1{0.7f, 0.7857143f};
    rc->Rect(p0, p1, uv0, uv1, white, 0.0f);
  }
  StartScreenVars* vars = Vars();
  if (vars->m_timeInStart >= 4.0f) {
    vars->m_timeInStart = 4.0f;
    vars->DrawPressStart(rc);
  }
}

// 803A1524
int StartScreenStartState::IsFlowStateRequestPending() {
  return static_cast<TheSimsStateMachine*>(m_owner->m_owner)->IsFlowStateRequestPending() != 0;
}

// 803A154C
int StartScreenStartState::GetFlowStateRequest() {
  return static_cast<int>(static_cast<TheSimsStateMachine*>(m_owner->m_owner)->GetFlowStateRequest());
}

// 803A1574
void StartScreenStartState::SetFlowStateCurrent() {
  static_cast<TheSimsStateMachine*>(m_owner->m_owner)->SetFlowStateCurrent();
}
