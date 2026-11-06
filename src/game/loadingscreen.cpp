// Port of game/loadingscreen.cpp (CLoadingScreen) and game/loadingscreenstate.cpp (the 'LdSc' state
// machine and its four states).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

extern ENgcRenderer _ngcRenderer;

namespace {
constexpr unsigned int kBackgroundShader = 0x7f5abc2du;  // s2c_bg_gradient
constexpr unsigned int kDiamondModel = 0x6d1f0956u;      // the plumb bob
constexpr unsigned int kGlowModel = 0x6fac867fu;         // plum_glow

// The camera looks along +y at the origin, z up (s_camEye, s_camTarget, s_camUp).
EVec3 s_camEye{0.0f, -1.124f, 0.0f};
EVec3 s_camTarget{0.0f, 0.0f, 0.0f};
EVec3 s_camUp{0.0f, 0.0f, 1.0f};
const EVec3 s_glowColor{0.12f, 0.12f, 0.12f};             // the glow shells' ambient light
const EVec3 s_plumbBobLightColor{0.0f, 1.0f, 0.0f};       // green, from the upper right front
const EVec3 s_plumbBobLightDir{0.577f, 0.577f, -0.577f};
const EVec3 s_plumbBobAmbientColor{0.4f, 0.4f, 0.6f};
const float s_glowScaleRange[2] = {0.9f, 1.2f};           // 804FDA9C
const float s_glowScaleDuration = 0.75f;                  // 804FDAA4
const float s_glowScaleTimeScale[2] = {0.9f, 1.2f};       // 804FDAA8
const float s_glowRotTimeScale[2] = {0.9f, -1.2f};        // 804FDAB0
constexpr float kTwoPi = 6.2831855f;
}  // namespace

int s_bDrawPressStart = 0;  // 804FF7B0

// ---------------------------------------------------------------------------------------------------
// CLoadingScreen

// 80056A64
// The plumb bob spins in place (no bounce) at the origin.
CLoadingScreen::CLoadingScreen(eLoadingScreenMode mode) : m_mode(mode) {
  s_bDrawPressStart = 0;
  m_background = static_cast<ERShader*>(_shaderman.AddRef(kBackgroundShader, nullptr, 0));
  m_diamondModel = static_cast<ERModel*>(_modelman.AddRef(kDiamondModel, nullptr, 0));
  m_glowModel = static_cast<ERModel*>(_modelman.AddRef(kGlowModel, nullptr, 0));
  m_plumbBob = new PlumbBobModel;
  m_plumbBob->SetModel(kDiamondModel);
  m_plumbBob->m_bounce = 0;
  m_plumbBob->m_spin = 1;
  m_plumbBob->m_baseHeight = 0.0f;
  m_plumbBob->m_pos = EVec3{0.0f, 0.0f, 0.0f};
  for (int i = 0; i < 2; ++i) {
    m_glowScaleT[i] = 0.0f;
    m_glowScaleDir[i] = 0;
    m_glowScale[i] = 1.0f;
    m_glowRot[i] = 0.0f;
  }
  if (m_mode >= 0 && m_mode < 6) m_showText = 1;
}

namespace {
// Player 1's camera director's depth of field (DAT_80475EE4: in-game loads only).
DepthOfField* LoadDepthOfField() {
  CameraDirector* d = _globals.CameraDirectorOf(0);
  FrameEffectsManager* fx = d ? d->GetFrameEffectsManager() : nullptr;
  return fx ? fx->DepthOfFieldObject() : nullptr;
}
}  // namespace

// 80056BF0
// Also switches depth of field back off (in-game loads).
CLoadingScreen::~CLoadingScreen() {
  if (m_background) m_background->DelRef();
  if (m_diamondModel) m_diamondModel->DelRef();
  if (m_glowModel) m_glowModel->DelRef();
  m_background = nullptr;
  m_diamondModel = m_glowModel = nullptr;
  delete m_plumbBob;
  m_plumbBob = nullptr;
  if (DepthOfField* dof = LoadDepthOfField())
    if (dof->m_state != kFrameEffectOff) dof->Enable(false);
}

// 80056CE8
// When a camera director exists (in-game loads), a soft grey depth-of-field blur behind the screen.
void CLoadingScreen::InitAsync() {
  m_initialised = 1;
  DepthOfField* dof = LoadDepthOfField();
  if (!dof || dof->m_state != kFrameEffectOff) return;
  dof->m_target.downSampleLevel = 0;
  dof->m_target.tint[0] = 0.7f;
  dof->m_target.tint[1] = 0.7f;
  dof->m_target.tint[2] = 0.7f;
  dof->m_target.blurFactor = 0.3f;
  dof->Enable(true);
}

// 80056DB4
// Waits until the shader and both models are loaded (the AddRefs may complete asynchronously).
void CLoadingScreen::Update(float) {
  if (m_resourcesState) return;
  bool ready = true;
  if (!m_background && !(m_background = static_cast<ERShader*>(_shaderman.GetRef(kBackgroundShader)))) ready = false;
  if (!m_diamondModel && !(m_diamondModel = static_cast<ERModel*>(_modelman.GetRef(kDiamondModel)))) ready = false;
  if (!m_glowModel && !(m_glowModel = static_cast<ERModel*>(_modelman.GetRef(kGlowModel)))) ready = false;
  if (ready) {
    InitAsync();
    m_resourcesState = 3;
  }
}

// 80056E94
// Background (alpha-blended over the frame at `alpha`), then in a 3D view (45 degrees horizontal
// field of view, camera 1.124 in front) the two glow shells, lit only by a dim ambient light and
// spinning about y in opposite directions, and the plumb bob, lit green, spinning about z; then
// "Loading, please wait" with a drop shadow. Nothing is drawn while a movie plays.
void CLoadingScreen::DrawTopmost(ERC* rc, float alpha) {
  if (!m_initialised || _app.IsMoviePlaying()) return;
  const float fov = static_cast<float>(_pGfx->m_screenHeight) * 45.0f / static_cast<float>(_pGfx->m_screenWidth);
  SetProjection(fov, _pGfx->GetScreenAspect(), 1.0f, 200.0f);
  SetLookAt(s_camEye, s_camTarget, s_camUp);
  TRect<float> viewport{0.0f, 0.0f, 1.0f, 1.0f};
  SetViewport(viewport);
  Select(rc);

  ERShader* background = m_background ? m_background : _globals.m_globalShader;
  if (EShader* shader = background ? background->GetShader() : nullptr) shader->Select(rc, 0);
  rc->AlphaTest(false, 5, 0.5f, 0);
  rc->SetBlendMode(0, 1, 0, 1, 0.0f, 0);
  rc->EnableRasterModes(0x40, 0);
  EVec2 p0{0, 0}, p1{1, 1}, uv0{0, 1}, uv1{1, 0};
  const float c = m_background ? 1.0f : 0.0f;
  EVec4 color{c, c, c, alpha};
  // PC widescreen: the background covers the whole frame (stretched), not only its 4:3 centre.
  simmy::display_set_hud_anchor(0.0f, 1);
  if (ENgcRenderStateCache* cache = _ngcRenderer.GetRenderStateCache()) cache->m_dirty |= 0x80000600;
  rc->Rect(p0, p1, uv0, uv1, color, 0.0f);
  simmy::display_set_hud_anchor(0.0f, 0);
  if (ENgcRenderStateCache* cache = _ngcRenderer.GetRenderStateCache()) cache->m_dirty |= 0x80000600;

  rc->ZClear(0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  rc->ZTest(true, 2, 0, 0);

  ELights lights;
  EMat4 glowMtx[2];
  for (int i = 0; i < 2; ++i) {
    EMat4& m = glowMtx[i];
    m.Id();
    float t = _dt * s_glowScaleDuration * s_glowScaleTimeScale[i] + m_glowScaleT[i];
    m_glowScaleT[i] = t;
    if (t > 1.0f) {
      m_glowScaleT[i] = 0.0f;
      m_glowScaleDir[i] = 1 - m_glowScaleDir[i];
    }
    const float from = s_glowScaleRange[m_glowScaleDir[i]], to = s_glowScaleRange[1 - m_glowScaleDir[i]];
    m_glowScale[i] = m_glowScaleT[i] * (to - from) + from;
    m.PreScale(m_glowScale[i]);
    float a = _dt * s_glowRotTimeScale[i] + m_glowRot[i];
    m_glowRot[i] = a;
    if (a < 0.0f) a = kTwoPi;
    else if (a > kTwoPi) a = 0.0f;
    m_glowRot[i] = a;
    // RotateY starts from the identity, so the pulse scale above is lost (as in the original).
    m.RotateY(a);
    lights.m_ambient[0] = s_glowColor.x, lights.m_ambient[1] = s_glowColor.y, lights.m_ambient[2] = s_glowColor.z;
    lights.m_numDir = 0;
    lights.m_numPoint = 0;
    m.PreScale(m_glowModel->m_scale);
    rc->SetAlpha(alpha);
    rc->Lights(&lights);
    rc->ModelMatrices(&m, 1);
    m_glowModel->Draw(rc);
  }

  lights.m_dir[0].m_color[0] = s_plumbBobLightColor.x;
  lights.m_dir[0].m_color[1] = s_plumbBobLightColor.y;
  lights.m_dir[0].m_color[2] = s_plumbBobLightColor.z;
  lights.m_dir[0].m_dir[0] = s_plumbBobLightDir.x;
  lights.m_dir[0].m_dir[1] = s_plumbBobLightDir.y;
  lights.m_dir[0].m_dir[2] = s_plumbBobLightDir.z;
  lights.m_ambient[0] = s_plumbBobAmbientColor.x;
  lights.m_ambient[1] = s_plumbBobAmbientColor.y;
  lights.m_ambient[2] = s_plumbBobAmbientColor.z;
  lights.m_numDir = 1;
  lights.m_numPoint = 0;
  rc->SetAlpha(alpha);
  rc->Lights(&lights);
  m_plumbBob->Draw(rc);
  rc->SetAlpha(1.0f);

  // For modes 0..5: "loading_please_wait" in the global font, white at `alpha`, size 16, centred at
  // (0.5, 0.85) with a 2-pixel drop shadow.
  if (m_showText && _globals.m_font) {
    ERFont* font = _globals.m_font;
    font->Select(rc);
    EVec4 white{1, 1, 1, alpha};
    font->SetColor(white);
    font->SetSize(16.0f, 1.0f, true);
    EVec2 pos{0.5f, 0.85f};
    font->DrawDs(rc, const_cast<unsigned short*>(EGlobal::NewUIText("loading_please_wait")), pos,
                 EFontAlignX::Center, EFontAlignY::Center, nullptr, 2.0f, alpha, true);
    rc->SetAlpha(1.0f);
  }
}

// 800578A4
int CLoadingScreen::GetMode() const { return m_mode; }

// ---------------------------------------------------------------------------------------------------
// LoadingScreenStateMachine

LoadingScreenStateMachine* LoadingScreenStateMachine::s_pSM = nullptr;

// 800907CC
LoadingScreenStateMachine::LoadingScreenStateMachine(int id, int machineId) : StateMachine(id, machineId) {
  s_pSM = this;
}

// 80090900
LoadingScreenStateMachine::~LoadingScreenStateMachine() = default;

// 800909A4
void LoadingScreenStateMachine::Startup() {
  AddState(new LoadingScreenWaitingForLoadState(kWaitingForLoad));
  AddState(new LoadingScreenFadeInState(kFadeIn));
  AddState(new LoadingScreenUpdateState(kUpdate));
  AddState(new LoadingScreenFadeOutState(kFadeOut));
  SetNextState(kWaitingForLoad, 0.0f);
}

// 80090AE4
void LoadingScreenStateMachine::Update(float dt) { StateMachine::Update(dt); }

// 80090B04
void LoadingScreenStateMachine::CreateLoadingScreen() {
  if (!m_screen) {
    void* mem = MainHeap()->MallocAligned(sizeof(CLoadingScreen), 0x10, 0, 0);
    m_screen = new (mem) CLoadingScreen(CLoadingScreen__eLoadingScreenMode{m_mode});
  }
}

// 80090B5C
void LoadingScreenStateMachine::DeleteLoadingScreen() {
  if (m_screen) {
    m_screen->~CLoadingScreen();
    MainHeap()->Free(m_screen);
    m_loadRequested = 0;
    m_screen = nullptr;
  }
}

// 80090BB8
int LoadingScreenStateMachine::IsReadyToLoad() { return m_loadRequested ? 1 : 0; }

// 80090BD0
int LoadingScreenStateMachine::IsLoading() { return GetCurStateId() != kWaitingForLoad || m_loadRequested; }

// 80090C1C
// From the waiting state the frame effects are switched off for the load. If the screen is already
// fading out (a new load right after the last one), it turns round and fades back in from where it
// is; if it is fully up, the request is already satisfied.
void LoadingScreenStateMachine::StartLoad(CLoadingScreen__eLoadingScreenMode mode) {
  m_mode = mode;
  m_loadRequested = 1;
  m_loadComplete = 0;
  if (GetCurStateId() == kWaitingForLoad) {
    _pGfx->m_field320 = 0;  // EGraphics::SetEnableFrameEffects(false)
  } else if (m_fadeFlags & kFadingOut) {
    float t = m_fadeDuration - m_fadeRemaining;
    if (t > 0.75f) t = 0.75f;
    if (t == 0.0f) t = 0.01f;
    m_fadeDuration = 0.75f;
    m_fadeRemaining = t;
    m_fadeFlags = kFaderRunning | kFadingIn;
    m_fadeRate = 1.0f / 0.75f;
    SetNextState(kFadeIn, 0.0f);
  } else if (m_fadeFlags & kFadedIn) {
    m_loadRequested = 0;
  }
  CreateLoadingScreen();
}

// 80090D18
void LoadingScreenStateMachine::LoadFinishedNow() {
  DeleteLoadingScreen();
  SetNextState(kWaitingForLoad, 0.0f);
}

// 80090D58
void LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode mode) {
  if (s_pSM) s_pSM->StartLoad(mode);
}
// 80090D8C
void LoadingScreenStateMachine::LoadFinishing() {
  if (s_pSM) s_pSM->m_loadComplete = 1;
}
// 80090DA4
void LoadingScreenStateMachine::LoadFinishNow() {
  if (s_pSM) s_pSM->LoadFinishedNow();
}
// 80090DD0
int LoadingScreenStateMachine::LoadInProgress() { return s_pSM ? s_pSM->IsLoading() : 0; }

// 80090E04
CLoadingScreen* LoadingScreenStateMachine::GetLoadingScreen() { return m_screen; }
// 80090E0C
void LoadingScreenStateMachine::LoadStarted() { m_loadRequested = 0; }
// 80090E18
int LoadingScreenStateMachine::IsLoadComplete() { return m_loadComplete; }

// 80090E20
void LoadingScreenStateMachine::FaderStart(float seconds, bool fadeOut) {
  m_fadeFlags = kFaderRunning | (fadeOut ? kFadingOut : kFadingIn);
  m_fadeRemaining = seconds;
  m_fadeDuration = seconds;
  m_fadeRate = 1.0f / seconds;
}

// 80090E88
void LoadingScreenStateMachine::FaderStop() { m_fadeFlags = 0; }

// 80090E94
void LoadingScreenStateMachine::FaderUpdate(float dt) {
  const unsigned int flags = m_fadeFlags;
  if (!(flags & kFaderRunning) || m_fadeRemaining == 0.0f) return;
  m_fadeRemaining -= dt;
  if (m_fadeRemaining > 0.0f) return;
  m_fadeRemaining = 0;
  if (flags & kFadingIn)
    m_fadeFlags = (flags & ~kFadingIn) | kFadedIn;
  else if (flags & kFadingOut)
    m_fadeFlags = (flags & ~kFadingOut) | kFadedOut;
}

// 80090F00
float LoadingScreenStateMachine::FaderGetFadeOpacity() {
  const unsigned int flags = m_fadeFlags;
  if (flags & kFadingIn) return (m_fadeDuration - m_fadeRemaining) * m_fadeRate;
  if (flags & kFadingOut) return m_fadeRemaining * m_fadeRate;
  if (flags & kFadedIn) return 1.0f;
  return 0.0f;
}

// 80090F74
LoadingScreenStateMachine* LoadingScreenStateMachine::GetSingleton() { return s_pSM; }
// 80090F7C
int LoadingScreenStateMachine::IsFadingIn() { return (s_pSM->m_fadeFlags & kFadingIn) || s_pSM->m_loadRequested; }
// 80090FA4
int LoadingScreenStateMachine::IsFadedIn() { return (s_pSM->m_fadeFlags >> 2) & 1; }
// 80090FB4
int LoadingScreenStateMachine::IsFadingOut() { return (s_pSM->m_fadeFlags >> 1) & 1; }
// 80090FC4
int LoadingScreenStateMachine::IsFadedOut() { return (s_pSM->m_fadeFlags >> 3) & 1; }
// 80090FD4
void LoadingScreenStateMachine::LoadFinished() { m_loadComplete = 1; }

// ---------------------------------------------------------------------------------------------------
// States. Fades are 0.75 s, or 0.25 s for loading-screen mode 0.

namespace {
float FadeSeconds() {
  CLoadingScreen* s = LoadingScreenStateMachine::s_pSM->m_screen;
  return s && s->m_mode == 0 ? 0.25f : 0.75f;
}
void DrawScreen(LoadingScreenStateMachine* owner, ERC* rc) {
  if (CLoadingScreen* s = LoadingScreenStateMachine::s_pSM->m_screen) s->DrawTopmost(rc, owner->FaderGetFadeOpacity());
}
}  // namespace

// 80090FE0
LoadingScreenWaitingForLoadState::LoadingScreenWaitingForLoadState(int id) : StateMachineState(id) {}

// 800901A4
void LoadingScreenWaitingForLoadState::Update(float) {
  if (Owner()->IsReadyToLoad()) {
    Owner()->FaderStart(FadeSeconds(), false);
    OwnerSetNextState(LoadingScreenStateMachine::kFadeIn, 0.0f);
  }
}

// 80091018
void LoadingScreenWaitingForLoadState::FaderStart(float seconds) { Owner()->FaderStart(seconds, false); }
// 80091054
int LoadingScreenWaitingForLoadState::IsReadyToLoad() { return Owner()->IsReadyToLoad(); }

// 80091098
LoadingScreenFadeInState::LoadingScreenFadeInState(int id) : StateMachineState(id) {}

// 8009027C
// Once faded in (or the load already finished) the loader may start: the request is consumed and
// frame effects stay off while the screen is up.
void LoadingScreenFadeInState::Update(float dt) {
  Owner()->FaderUpdate(_dtlim20);
  LoadingScreenStateMachine* sm = LoadingScreenStateMachine::s_pSM;
  if (Owner()->m_loadComplete || (sm->m_fadeFlags & LoadingScreenStateMachine::kFadedIn)) {
    sm->m_loadRequested = 0;
    _pGfx->m_field320 = 0;
    OwnerSetNextState(LoadingScreenStateMachine::kUpdate, 0.0f);
  }
  if (sm->m_screen) sm->m_screen->Update(dt);
}

// 8009038C
void LoadingScreenFadeInState::DrawTopmost(ERC* rc) { DrawScreen(Owner(), rc); }
// 800910D0
void LoadingScreenFadeInState::FaderUpdate(float dt) { Owner()->FaderUpdate(dt); }
// 80091140
float LoadingScreenFadeInState::FaderGetFadeOpacity() { return Owner()->FaderGetFadeOpacity(); }
// 800911B8
int LoadingScreenFadeInState::IsLoadComplete() { return Owner()->m_loadComplete; }

// 800911EC
LoadingScreenUpdateState::LoadingScreenUpdateState(int id) : StateMachineState(id) {}

// 80090454
void LoadingScreenUpdateState::Update(float dt) {
  LoadingScreenStateMachine* sm = LoadingScreenStateMachine::s_pSM;
  if (sm->m_screen) sm->m_screen->Update(dt);
  if (Owner()->m_loadComplete) {
    Owner()->FaderStart(FadeSeconds(), true);
    OwnerSetNextState(LoadingScreenStateMachine::kFadeOut, 0.0f);
  }
}

// 80090544
void LoadingScreenUpdateState::DrawTopmost(ERC* rc) { DrawScreen(Owner(), rc); }
// 80091224
void LoadingScreenUpdateState::FaderStart(float seconds) { Owner()->FaderStart(seconds, true); }
// 80091264
float LoadingScreenUpdateState::FaderGetFadeOpacity() { return Owner()->FaderGetFadeOpacity(); }
// 800912DC
int LoadingScreenUpdateState::IsLoadComplete() { return Owner()->m_loadComplete; }

// 80091310
LoadingScreenFadeOutState::LoadingScreenFadeOutState(int id) : StateMachineState(id) {}

// 8009060C
// When the fade-out ends the screen is deleted, frame effects come back on and the machine waits.
void LoadingScreenFadeOutState::Update(float dt) {
  Owner()->FaderUpdate(_dtlim20);
  LoadingScreenStateMachine* sm = LoadingScreenStateMachine::s_pSM;
  if (sm->m_fadeFlags & LoadingScreenStateMachine::kFadedOut) {
    sm->DeleteLoadingScreen();
    _pGfx->m_field320 = 1;
    sm->m_fadeFlags = 0;
    OwnerSetNextState(LoadingScreenStateMachine::kWaitingForLoad, 0.0f);
  }
  if (sm->m_screen) sm->m_screen->Update(dt);
}

// 80090704
void LoadingScreenFadeOutState::DrawTopmost(ERC* rc) { DrawScreen(Owner(), rc); }
// 80091348
void LoadingScreenFadeOutState::FaderUpdate(float dt) { Owner()->FaderUpdate(dt); }
// 800913B8
float LoadingScreenFadeOutState::FaderGetFadeOpacity() { return Owner()->FaderGetFadeOpacity(); }
// 80091430
int LoadingScreenFadeOutState::IsLoadComplete() { return Owner()->m_loadComplete; }
