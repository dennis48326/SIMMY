// Port of Create-a-Sim (partial): game/casevent.cpp, caslistener.cpp, casmediator.cpp, cassimstate.cpp,
// cassimrenderer.cpp, cassimrendererdynamic.cpp, casscene.cpp, casscenedefault.cpp and the CAS target
// of game/targets_cas.cpp. Still stubbed: the CAS sub-targets (genetics, morph, body, selection, misc,
// fashion, personal, tattoo, roommate, keyboard) and so the editing of the sim, the Apt commands of
// the CAS target, sounds and ambient score, the mirror, shadows, the stick (turning the sim,
// zooming), and the standing-animation choice.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>
#include <cstring>

extern int _ngcUseWeightBlending;  // 804FF350 (engine/e_ngcrenderer_geom.cpp)

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }

// CasScene's camera positions (m_cameraAngles, 803DFA1C): eye, then target.
const float kCameraAngles[10][6] = {
    {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},   {0.3f, -3.05f, 1.52f, -1.52f, 2.74f, 0.61f},
    {-0.7f, -1.9f, 1.35f, 0.0f, 1.2f, 1.25f},      {0.61f, -2.44f, 0.61f, -1.83f, 3.35f, 0.61f},
    {0.61f, -1.83f, 1.83f, -2.13f, 3.11f, 0.91f},  {0.91f, -1.83f, 0.61f, 0.3f, -0.91f, 0.3f},
    {0.61f, -1.83f, 1.22f, -1.83f, 3.05f, 0.61f},  {0.61f, -1.83f, 1.83f, -2.13f, 3.11f, 0.91f},
    {0.75f, 1.15f, 1.52f, -1.07f, 3.91f, 1.52f},   {-1.18f, -1.29f, 1.74f, -3.96f, -4.88f, 1.22f},
};
// The alternative (C-stick zoom) positions (m_altCameraAngles, 803DFB0C): mostly camera 0's wide shot.
const float kAltCameraAngles[10][6] = {
    {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},  {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},
    {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},  {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},
    {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},  {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},
    {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},  {0.3f, -3.96f, 1.83f, -0.91f, 1.22f, 0.61f},
    {1.83f, -1.22f, 3.96f, -1.22f, 4.27f, -0.61f}, {-0.61f, -1.22f, 3.05f, -3.96f, -4.88f, 1.22f},
};
// The level's sun and ambient per light set (kSunDir, kSunDirColor, kSunAmbientColor (bss, set by
// the static initializer 80166F84), kSunDirIntensity, kSunAmbientIntensity (803DFD1C, 803DFD2C)).
const float kSunDir[4][3] = {{0.57735f, 0.57735f, -0.57735f}, {0.57735f, 0.57735f, -0.57735f},
                             {0.57735f, 0.57735f, -0.57735f}, {1.5f, -0.75f, -0.75f}};
const float kSunDirIntensity[4] = {1.5f, 1.5f, 1.5f, 0.85f};
const float kSunAmbientColor[3] = {0.9f, 0.9f, 1.0f};
const float kSunAmbientIntensity[4] = {0.3f, 0.3f, 0.3f, 0.3f};
// "<scene>_<set>" rows of QDLightSet (s_szLightSetNames).
const char* const kLightSetNames[4] = {"clothing", "roommate", "genetics", "mirror"};
// Object positions (kSimPosDefault, kSimPosGenetics, kSimPosGeneticsPlumbBob, kPosCAR).
const EVec3 kSimPosDefault{0.0f, 0.0f, 0.0f};
const EVec3 kSimPosGenetics{-0.47f, 2.99f, 0.0f};
const EVec3 kSimPosGeneticsPlumbBob{-0.47f, 2.99f, 1.0f};
const EVec3 kPosCAR[4] = {{-3.7f, -6.4f, 0.04f}, {-4.11f, -5.77f, 0.04f}, {-4.52f, -5.16f, 0.04f},
                          {-4.82f, -4.51f, 0.04f}};
// The mirror's corners in the clothing room (mirrorClothingCorners).
const float kMirrorClothingCorners[12] = {-0.989f, 0.119f, 0.235f, -0.563f, 0.532f, 0.235f,
                                          -0.646f, 0.618f, 1.946f, -1.076f, 0.205f, 1.946f};

float Smooth(float t) { return t * 0.0f + t * -2.0f * t * t + t * 3.0f * t; }
EVec3 Lerp(const EVec3& a, const EVec3& b, float t) {
  return EVec3{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), a.z + t * (b.z - a.z)};
}
// The pad CAS's player uses (GC, inlined in CasScene::UpdateCamera and CasSimRendererDynamic::Update):
// the player's mapped pad; unmapped, the active pad unless it is player 1's; and the player's Apt
// button filter.
EController* CasPad(CasMediator* mediator, unsigned int& filter) {
  const unsigned int player = static_cast<unsigned int>(mediator->m_player);
  filter = mediator->GetControllerFilterId(player);
  int index;
  if (!_pCtrlMan->GetPlayerMapped(player)) {
    index = _pCtrlMan->FindActiveController();
    if (index == -1 || index == _pCtrlMan->GetPlayerControllerIndex(0)) return nullptr;
  } else {
    index = _pCtrlMan->GetPlayerControllerIndex(player);
  }
  return _pCtrlMan->GetController(static_cast<unsigned int>(index));
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// CasEvent, CasListener

// 801603B8
CasEvent::~CasEvent() = default;

// 80161B50
CasListener::CasListener() {}
// 80161B6C
CasListener::~CasListener() = default;

// 80161A6C
void CasListener::HandleEvent(CasEvent& event) {
  switch (event.m_type) {
    case CasEvent::kInitSim: HandleEventInitSim(static_cast<CasEventInitSim&>(event)); break;
    case CasEvent::kResetSim: HandleEventResetSim(reinterpret_cast<CasEventResetSim&>(event)); break;
    case CasEvent::kChangeFocus: HandleEventChangeFocus(static_cast<CasEventChangeFocus&>(event)); break;
    case CasEvent::kStoreSim: HandleEventStoreSim(reinterpret_cast<CasEventStoreSim&>(event)); break;
    case CasEvent::kChangeSim: HandleEventChangeSim(reinterpret_cast<CasEventChangeSimS2C&>(event)); break;
    case CasEvent::kMorphSim: HandleEventMorphSim(reinterpret_cast<CasEventMorphSimS2C&>(event)); break;
    case CasEvent::kResetSimDraw: HandleEventResetSimDraw(reinterpret_cast<CasEventResetSimDraw&>(event)); break;
    default: break;
  }
}

// 80161BA0
void CasListener::SetMediator(CasMediator* mediator) { m_mediator = mediator; }
// 80161BA8
CasMediator* CasListener::GetMediator() const { return m_mediator; }
// 80161BB0
void CasListener::HandleEventChangeFocus(CasEventChangeFocus&) {}
// 80161BB4
void CasListener::HandleEventChangeSim(CasEventChangeSimS2C&) {}
// 80161BB8
void CasListener::HandleEventInitSim(CasEventInitSim&) {}
// 80161BBC
void CasListener::HandleEventResetSim(CasEventResetSim&) {}
// 80161BC0
void CasListener::HandleEventStoreSim(CasEventStoreSim&) {}
// 80161BC4
void CasListener::HandleEventMorphSim(CasEventMorphSimS2C&) {}
// 80161BC8
void CasListener::HandleEventResetSimDraw(CasEventResetSimDraw&) {}

// ---------------------------------------------------------------------------------------------------
// CasMediator

// 80161BCC
CasMediator::CasMediator() {}
// 80161C88
CasMediator::~CasMediator() { RemoveAllListeners(); }

// 80161D18
void CasMediator::AddListener(CasListener& listener) {
  m_listeners.insert(&listener);
  listener.SetMediator(this);
}

// 80161D84
void CasMediator::RemoveListener(CasListener& listener) {
  m_listeners.erase(&listener);
  listener.SetMediator(nullptr);
}

// 80161F68
void CasMediator::RemoveAllListeners() {
  for (CasListener* l : m_listeners) l->SetMediator(nullptr);
  m_listeners.clear();
}

// 80162028
int CasMediator::GetNumListeners() const { return static_cast<int>(m_listeners.size()); }

// 80162030
// GC: the event's own virtual (vtable 0x14) runs first; then every listener handles it.
void CasMediator::PostEvent(CasEvent& event) {
  const std::set<CasListener*> listeners = m_listeners;
  for (CasListener* l : listeners) l->HandleEvent(event);
}

// 801620D8
CasSimDescriptionS2C* CasMediator::GetEditSimDescription() { return m_editSim ? &m_editSim->m_desc : nullptr; }

// 8016222C
void CasMediator::DrawSim(ERC* rc) {
  if (m_renderer) m_renderer->Draw(rc);
}

// 8016226C
void CasMediator::SetCasMode(int mode) {
  m_mode = mode;
  UIDB::UIDBSetInt(const_cast<char*>("CAS_mode"), mode);
}

// 80162298
// The scene's camera has finished moving.
int CasMediator::IsTransitionComplete() {
  if (m_renderer && m_scene) return (m_scene->m_t < 1.0f && m_scene->m_delay <= 0.0f) ? 0 : 1;
  return 1;
}

// 801622F4
void CasMediator::SetControllerFilterId(unsigned int id, unsigned int player) { m_filterIds[player] = id; }
// 8016230C
unsigned int CasMediator::GetControllerFilterId(unsigned int player) const { return m_filterIds[player]; }

// ---------------------------------------------------------------------------------------------------
// CasSimState

// 80172C28
CasSimState::CasSimState() {}
// 80172D34
CasSimState::~CasSimState() = default;

// 801747C0
// The new sim becomes the one edited and the one to go back to.
void CasSimState::HandleEventInitSim(CasEventInitSim& event) {
  m_stored = event.m_desc;
  m_desc = event.m_desc;
}

// 80175294
CasSimDescriptionS2C* CasSimState::GetSimDescription() { return &m_desc; }
// 8017529C
CasSimDescriptionS2C* CasSimState::GetStoredSimDescription() { return &m_stored; }

// ---------------------------------------------------------------------------------------------------
// CasSimRenderer

// 8016EEE0
CasSimRenderer::CasSimRenderer() { Init(); }

// 8016EF38
CasSimRenderer::~CasSimRenderer() {
  if (m_shadowModel) {
    m_shadowModel->DelRef();
    m_shadowModel = nullptr;
  }
  if (m_pendingAnim) _animman.DelRefAsync(m_pendingAnim);
}

// 8016F050
void CasSimRenderer::Init() {
  m_model.Reset();
  m_field210 = 0;
  m_pos = EVec3{0.0f, 0.0f, 0.0f};    // _vInitialSimPos
  m_scale = EVec3{1.0f, 1.0f, 1.0f};  // _vInitialSimScale
  m_pendingAnim = 0;
  m_userRotation = 0.0f;
  m_rotation = 0.0f;
}

// 8016F0E8
void CasSimRenderer::SetPosition(EVec3& pos, float rotation) {
  m_pos = pos;
  m_rotation = rotation;
}

// 8016F13C
void CasSimRenderer::UpdateSkin() {
  m_model.CompositeAllSkin();
  m_model.ApplySkinToModels();
}

// 8016F174
// When the CAS target shows this sim: the scene's lights, the skeleton posed where the scene puts the
// sim (turned by the player), then the parts skinned by it.
void CasSimRenderer::Draw(ERC* rc) {
  CASTarget* target = _gd.m_casTarget;
  if (!target || !((target->m_flags >> (m_positionNum % 4)) & 1) || !m_mediator || !m_mediator->m_scene) return;
  rc->Lights(&m_mediator->m_scene->m_lights);
  EVec4 plane{0.0f, 0.0f, 0.0f, 0.0f};
  rc->SetAlphaPlane(1.0f, plane, 0.0f, 1.0f);
  EVec3 rot{0.0f, 0.0f, m_rotation + m_userRotation};
  EMat4 orient;
  EAnimController::CalcOrientMatrix(m_pos, rot, m_scale, orient);
  m_anim.Compute(orient, _framecount);
  const int count = m_anim.NodeCount();
  rc->ModelMatrixList(m_anim.m_matrices, count);
  DrawShadow(rc);
  SimRenderer::Render(rc, m_model.m_parts, nullptr, m_anim.m_matrices, static_cast<unsigned int>(count), true);
}

// 8016F720
// The sim's portrait (SimImageMaker): its gender, the portrait parts of its model, the size and depth
// asked, sepia, a background texture. PC: the texture is finished a few frames later (SimImageMaker).
ETexture* CasSimRenderer::CreateFaceImage(unsigned int width, unsigned int height, unsigned int bpp, bool sepia,
                                          unsigned int background) {
  SimImageMaker maker;
  CasSimDescriptionS2C* desc = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  maker.SetGender(desc && desc->m_male != 0);
  maker.SetPortraitModels(&m_model);
  maker.OverrideImageSize(width, height, bpp);
  maker.m_sepia = sepia ? 1 : 0;
  if (background) maker.OverrideBackground(background);
  return maker.CreateImage();
}

// 8016F304
// The shadow model (posed by the sim's matrices, already loaded) flattened onto the floor (a fixed
// slant of 0.2, 0.01 up) through the view, onto the area the stencil leaves set.
void CasSimRenderer::DrawShadow(ERC* rc) {
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (!m_shadowModel || !win) return;
  EMat4 flat;
  flat.Id();
  flat.m[2][0] = 0.2f;
  flat.m[2][1] = 0.2f;
  flat.m[2][2] = 0.0f;
  EVec3 up{0.0f, 0.0f, 0.01f};
  flat.PostTranslate(up);
  EMat4 view = win->m_lookAt;
  EMat4 proj;
  proj.Mult4x4(flat, view);
  rc->ViewMatrix(&proj, 0, 0.0f);
  m_shadowModel->DrawShadowWithStencilInverted(rc);
  rc->ViewMatrix(&view, 0, 0.0f);
}

// 8016F488
void CasSimRenderer::SetNextAnimation(unsigned int anim) {
  if (!_animman.IsLoaded(anim)) {
    m_pendingAnim = anim;
    _animman.AddRefAsync(anim);
  } else {
    m_anim.SetTrackAnim(eTrackFlags{1}, anim, -1, -1.0f, nullptr);
    m_anim.RestartTrack(eTrackFlags{1});
  }
}

// 8016F50C
// An animation waiting to load starts once it is in.
int CasSimRenderer::CheckPending() {
  if (!m_pendingAnim) return 0;
  if (!_animman.GetRef(m_pendingAnim)) return 1;
  m_anim.SetTrackAnim(eTrackFlags{1}, m_pendingAnim, -1, -1.0f, nullptr);
  m_anim.RestartTrack(eTrackFlags{1});
  _animman.DelRef(m_pendingAnim, EResourceManager__DelRefMode{1});
  m_pendingAnim = 0;
  return 0;
}

// 8016F5A8
// The edited sim's skeleton (by gender), its parts and its skin named after the sim.
void CasSimRenderer::SetupSimCommon() {
  CasSimDescriptionS2C* desc = m_mediator ? m_mediator->GetEditSimDescription() : nullptr;
  if (!desc) return;
  UIDB::UIDBSetInt(const_cast<char*>("CAS_gender"), desc->m_male != 0);
  const unsigned int skeleton = desc->m_male ? 0xffa60350u : 0x1fb80af4u;
  m_anim.Shutdown();
  m_anim.Init(skeleton, eAnimatedObjectType{3}, 0);
  m_anim.m_speed = 1.18f;
  m_anim.m_scale = 1.0f / 4096.0f;
  if (!m_shadowModel) m_shadowModel = static_cast<ERModel*>(_modelman.AddRef(0x6ef2f2dau, nullptr, 0));
  desc->m_precomposited = 0;
  m_model.Init(desc, true);
  SetupSim();
  char first[32], last[32];
  for (int i = 0; i < 32; ++i) {
    first[i] = static_cast<char>(desc->m_firstName[i]);
    last[i] = static_cast<char>(desc->m_lastName[i]);
  }
  first[31] = last[31] = 0;
  std::string name = std::string(first) + "_" + last;
  for (char& c : name)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) c = '_';
  m_model.CreateSkin(name.data());
}

// 8016FB54
unsigned int CasSimRenderer::GetPositionNum() const { return m_positionNum; }
// 8016FB5C
void CasSimRenderer::SetPositionNum(unsigned int index) { m_positionNum = index; }
// 8016FB64
void CasSimRenderer::SetSimRotation(float rotation) { m_userRotation = rotation; }
// 8016FB6C
SimModel* CasSimRenderer::GetSimModel() { return &m_model; }

// ---------------------------------------------------------------------------------------------------
// CasSimRendererDynamic

// 80424600 gCasFemaleSimAnimations
const unsigned int CasSimRendererDynamic::kFemaleAnimations[36] = {
    0xFCCC90BE, 0x84B53623, 0x1DBC6799, 0xD272F32C, 0x275DC4AF, 0x39C8323C, 0xEDE0D290, 0x18CFE513, 0xA2845BE3,
    0xA4A1B266, 0xD0E54FCB, 0xCBF76D0B, 0x52FE3CB1, 0xEBF53F20, 0x602BFD37, 0x9504CAB4, 0x09A06657, 0x18D5E4E4,
    0x496C20AF, 0x7032BC7B, 0x376F944E, 0xAE66C5F4, 0xAA06A2C3, 0x330FF379, 0xE377C600, 0x01229F77, 0x294C766D,
    0x51109DAD, 0xFC8BC4DB, 0xD56F9CA5, 0, 0, 0, 0, 0, 0};
// 80424690 gCasMaleSimAnimations
const unsigned int CasSimRendererDynamic::kMaleAnimations[36] = {
    0x042C6B49, 0x2E42AD0F, 0xB74BFCB5, 0x5249786C, 0x6865FF42, 0xB9F3B97C, 0x6DDB59D0, 0x57F7DEFE, 0x22BFD0A3,
    0xD2FDC87D, 0x0E11E935, 0x41E4F89D, 0xD8EDA927, 0x9DA9453B, 0xE0107677, 0xDA3CF159, 0x899BED17, 0xE502978A,
    0xB4BB53C1, 0x3F0A8796, 0x4133EE55, 0xD83ABFEF, 0x2A3D2983, 0xB3347839, 0x952BBC1B, 0x81191437, 0x585A8D24,
    0x48F6B849, 0x7698514D, 0xDEBB326B, 0, 0, 0, 0, 0, 0};

// 8016FBA0
CasSimRendererDynamic::CasSimRendererDynamic() {}
// 8016FC14
CasSimRendererDynamic::~CasSimRendererDynamic() = default;

// 8016FC44
// The gender's animations; the first (the standing idle) plays.
void CasSimRendererDynamic::SetupSim() {
  m_anims = m_model.m_desc && m_model.m_desc->m_male ? kMaleAnimations : kFemaleAnimations;
  m_anim.SetTrackAnim(eTrackFlags{1}, m_anims[0], -1, -1.0f, nullptr);
  m_anim.RestartTrack(eTrackFlags{1});
}

// 8016FF08
// While shown and the scene is up: placed where the scene puts it (the genetics or mirror spot, or its
// family position), turned with the stick, animated.
void CasSimRendererDynamic::Update(float) {
  if (!m_mediator) return;
  CASTarget* target = m_mediator->m_target;
  if (!target || !((target->m_flags >> (m_positionNum % 4)) & 1) || !m_mediator->m_scene) return;
  unsigned int object;
  if (target->IsViewingMirror()) object = 2;
  else if (target->IsViewingGenetics()) object = 0;
  else object = m_positionNum + 3;
  m_mediator->m_scene->GetObjectPosition(object, m_pos, m_rotation);
  if (!m_rotationLocked) {
    // The C-stick's x turns the sim, 90 degrees a second at full deflection.
    unsigned int filter = 0;
    EController* pad = CasPad(m_mediator, filter);
    const double x = pad ? pad->GetStick(1, 0, filter) : 0.0;
    if (x != 0.0) {
      float deg = static_cast<float>(x + x) * 45.0f * _dt + m_userRotation * 57.295776f;
      if (deg > 360.0f) deg -= 360.0f;
      else if (deg < 0.0f) deg += 360.0f;
      m_userRotation = deg * 0.017453292f;
    }
  }
  if (!CheckPending()) {
    if (m_anim.IsTrackAnimComplete(eTrackFlags{eTrackFlags::kMain})) SelectNextStandingAnimation();
    m_anim.Update(nullptr, nullptr, EVec3{1.0f, 1.0f, 1.0f});
  }
}

// 80170394 CasSimRendererDynamic::SelectNextStandingAnimation: cassimstate.cpp

// 80172A70
// The edited sim's parts and skin; the playing animation switches to the gender's version.
void CasSimRendererDynamic::HandleEventInitSim(CasEventInitSim&) {
  SetupSimCommon();
  m_model.SetAllModels();
  UpdateSkin();
  m_model.ApplyAllLatticeMorphing();
  m_model.ApplyAllMorphTargets();
  for (int i = 0; i < 36; ++i) {
    if (kFemaleAnimations[i] == m_curAnim || kMaleAnimations[i] == m_curAnim) {
      m_curAnim = m_anims[i];
      break;
    }
  }
}

// 80172C20
void CasSimRendererDynamic::LockSimRotation(bool lock) { m_rotationLocked = lock ? 1 : 0; }

// ---------------------------------------------------------------------------------------------------
// CasScene

// 8016502C
// GC: also sets the camera bloom (_pGfx vtable 0x1AC: 1, 10, 0, 0).
CasScene::CasScene() {
  m_quickdata = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(0x2a2af469u, nullptr, 0));
  m_win = new E3DWindow();  // GC: 0x350 bytes, 16-aligned, from the main heap
}

// 80165250
CasScene::~CasScene() {
  if (m_level) {
    if (_pEngine->m_currentLevel == m_level) _pEngine->m_currentLevel = nullptr;
    m_level->DelRef();
    m_level = nullptr;
  }
  if (m_mirrorRoom) {
    m_mirrorRoom->DelRef();
    m_mirrorRoom = nullptr;
  }
  delete static_cast<EMirrorPortal*>(m_mirror);
  m_mirror = nullptr;
  if (m_quickdata) {
    m_quickdata->DelRef();
    m_quickdata = nullptr;
  }
  delete m_win;
  m_win = nullptr;
}

// 801653FC
// GC: also starts the ambient score (AmbientScorePlayer, GetAmbientScoreName).
void CasScene::Init() {
  Reload();
  InitCamera(0);
  SIMMY_STUB("CasScene::Init: AmbientScorePlayer", 0x801653FC);
}

// 801654A4
// The room level (current in the engine, one room), the four light sets of the scene, the room seen
// in the mirror and the mirror.
void CasScene::Reload() {
  const char* scene = GetSceneName();
  if (m_level) {
    m_level->DelRef();
    m_level = nullptr;
  }
  if (m_mirrorRoom) {
    m_mirrorRoom->DelRef();
    m_mirrorRoom = nullptr;
  }
  delete static_cast<EMirrorPortal*>(m_mirror);
  m_mirror = nullptr;
  m_level = static_cast<ERLevel*>(_levelman.AddRef(0x94a667afu, nullptr, 0));
  if (m_level) {
    _pEngine->m_currentLevel = m_level;
    m_level->m_dayNight = nullptr;
    m_level->m_rainInfo = nullptr;
    m_level->m_sunYawOffset = 0.0f;
    m_level->ResetLightLocations();
    m_level->InitializeLightingGrid();
    m_level->SetRoomCount(1);
  }
  for (int i = 0; i < 4; ++i) {
    char name[0x40];
    std::snprintf(name, sizeof name, "%s_%s", scene, kLightSetNames[i]);
    LoadLights(name, i);
  }
  m_mirrorRoom = static_cast<ERModel*>(_modelman.AddRef(0xca9378d8u, nullptr, 0));
  EVec3 corners[4];
  if (GetMirrorCorners(CasScene__RoomId{0}, corners)) {
    // The mirror on the room's mirror corners, moved 0.006 off the wall along its normal.
    auto* mirror = new EMirrorPortal;
    m_mirror = mirror;
    mirror->SetNumCorners(4);
    mirror->SetMirrorShader(0xab5fdccc);
    const EVec3 e1{corners[1].x - corners[0].x, corners[1].y - corners[0].y, corners[1].z - corners[0].z};
    const EVec3 e2{corners[2].x - corners[0].x, corners[2].y - corners[0].y, corners[2].z - corners[0].z};
    EVec3 n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
    const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len != 0.0f) n = EVec3{n.x / len, n.y / len, n.z / len};
    for (int i = 0; i < 4; ++i) {
      EVec3 c{corners[i].x + n.x * 0.006f, corners[i].y + n.y * 0.006f, corners[i].z + n.z * 0.006f};
      mirror->SetCorner(i, c);
    }
  }
}

// 801657D8
void CasScene::SetMediator(CasMediator* mediator) { m_mediator = mediator; }

// 801657E0
void CasScene::Update(float dt) {
  UpdateCamera(dt);
  if (m_level) m_level->Update();
}

// 80165824
// The window and default render state, then the room of the current camera.
void CasScene::Draw(ERC* rc) {
  SetUpWindow(rc);
  rc->Texture(nullptr, 0);
  rc->SetGeometryModes(0);
  rc->SetRasterModes(0x48, 0);
  rc->SetBlendMode(0, 2, 2, 2, 1.0f, 0);
  rc->ZTest(true, 1, 0, 0);
  rc->AlphaTest(false, 5, 0.5f, 0);
  rc->SetCombineMode(0, 0);
  EMat4 id;
  id.Id();
  DrawRoom(rc, CasScene__RoomId{GetRoomFromCameraAngle(m_camera)}, id);
}

// 80165990
void CasScene::DrawRoom(ERC* rc, CasScene__RoomId room, EMat4& m) {
  if (room == 0) DrawRoomClothing(rc, m);
}

// 801659D4
// In the mirror view the room and the sim are drawn through the mirror portal first (reflected); then
// the light set of the current view and the room level, and in the mirror view the mirror's model.
void CasScene::DrawRoomClothing(ERC* rc, EMat4& m) {
  CASTarget* target = m_mediator ? m_mediator->m_target : nullptr;
  auto* mirror = static_cast<EMirrorPortal*>(m_mirror);
  const bool reflect = target && target->IsViewingMirror() && mirror && m_win;
  if (reflect) {
    // The room and the sim through the mirror first, in light set 3.
    mirror->SetFieldOfView(m_fov);
    mirror->StartRendering(*m_win, rc);
    EMat4 id;
    id.Id();
    rc->ModelMatrices(&id, 1);
    SetSceneLighting(3, false);
    rc->Lights(&m_lights);
    if (m_level) m_level->Draw(rc);
  }
  SetSceneLighting(m_lightSet, true);
  if (reflect) {
    m_mediator->DrawSim(rc);
    mirror->StopRendering(*m_win, rc);
  }
  EMat4 id;
  id.Id();
  rc->ModelMatrices(&id, 1);
  rc->Lights(&m_lights);
  if (m_level) m_level->Draw(rc);
  // The mirror's frame.
  if (m_mirrorRoom && reflect) {
    EMat4 scaled;
    scaled.Mult4x4(*m_mirrorRoom->GetScaleMatrix(), m);
    rc->ModelMatrices(&scaled, 1);
    m_mirrorRoom->Draw(rc);
  }
}

// 80165BCC
void CasScene::SetCameraParams(float fov, float nearPlane, float farPlane) {
  m_far = farPlane;
  m_fov = fov;
  m_near = nearPlane;
}

// 80165BDC
// A light set from its QDLightSet row: ambient, four directional and four point lights; the level's
// sun and ambient for it come from tables.
void CasScene::LoadLights(char* name, int set) {
  if (!m_quickdata) return;
  const void* table = m_quickdata->getTable("QDLightSet");
  const void* row = table ? m_quickdata->getRow(table, name) : nullptr;
  if (!row) return;
  CasSceneLightSet& s = m_lightSets[set];
  ELights& L = s.m_lights;
  L.m_numDir = ERQuickdata::U8(row, 0xc4);
  L.m_numPoint = ERQuickdata::U8(row, 0xc5);
  for (int i = 0; i < 3; ++i) L.m_ambient[i] = ERQuickdata::F32(row, i * 4);
  for (int d = 0; d < 4; ++d) {
    ELightsDir& dir = L.m_dir[d];
    for (int i = 0; i < 3; ++i) {
      dir.m_color[i] = ERQuickdata::F32(row, (d * 6 + 3 + i) * 4);
      dir.m_dir[i] = ERQuickdata::F32(row, (d * 6 + 6 + i) * 4);
    }
    const float len = std::sqrt(dir.m_dir[0] * dir.m_dir[0] + dir.m_dir[1] * dir.m_dir[1] + dir.m_dir[2] * dir.m_dir[2]);
    if (len != 0.0f)
      for (float& f : dir.m_dir) f /= len;
  }
  for (int p = 0; p < 4; ++p) {
    ELightsPoint& pt = L.m_point[p];
    for (int i = 0; i < 3; ++i) {
      pt.m_color[i] = ERQuickdata::F32(row, (p * 7 + 0x15 + i) * 4);
      pt.m_pos[i] = ERQuickdata::F32(row, (p * 7 + 0x18 + i) * 4);
    }
    pt.m_range = ERQuickdata::F32(row, (p * 7 + 0x1b) * 4);
  }
  s.m_ambientColor = EVec3{kSunAmbientColor[0], kSunAmbientColor[1], kSunAmbientColor[2]};
  s.m_ambientIntensity = kSunAmbientIntensity[set];
  s.m_sunDir = EVec3{kSunDir[set][0], kSunDir[set][1], kSunDir[set][2]};
  s.m_sunColor = EVec3{1.0f, 1.0f, 1.0f};
  s.m_sunIntensity = kSunDirIntensity[set];
}

// 80165F04
// The set's lights become the scene's; the level takes the set's sun, ambient and shadow alpha.
void CasScene::SetLights(int set) {
  if (set < 0) set = 0;
  else if (set > 3) set = 3;
  const CasSceneLightSet& s = m_lightSets[set];
  m_lights = s.m_lights;
  if (!m_level) return;
  m_level->m_sun.m_dir = s.m_sunDir;
  m_level->m_sun.m_color = s.m_sunColor;
  m_level->m_sun.m_intensity = s.m_sunIntensity;
  m_level->m_sunAmbient.m_color = s.m_ambientColor;
  m_level->m_sunAmbient.m_intensity = s.m_ambientIntensity;
  m_level->m_shadowAlpha = s.m_shadowAlpha;
  m_level->Update();
}

// 80166088
// GC: with `tweak`, the CAS tweak tool may override the lights (CasTweakTool::GetLights).
void CasScene::SetSceneLighting(int set, bool) { SetLights(set); }

// 801660D0
// Moves the camera to `camera` (and its zoomed-out alternative) over `duration` seconds after `delay`,
// from wherever it is (half way back when a move was under way); instantly for no duration. The
// destination goes into the tweak tool; a timed move resets the stick's zoom and blurs.
void CasScene::RepositionCamera(unsigned int camera, float duration, float delay) {
  if (camera >= 10) return;
  const bool moving = m_t < 1.0f && m_delay <= 0.0f;
  if (!moving) m_delay = delay;
  m_prevCamera = m_camera;
  m_camera = camera;
  const float* a = kCameraAngles[camera];
  const float* alt = kAltCameraAngles[camera];
  EVec3 eye{a[0], a[1], a[2]}, target{a[3], a[4], a[5]};
  const EVec3 eyeAlt{alt[0], alt[1], alt[2]}, targetAlt{alt[3], alt[4], alt[5]};
  _gCasTweakTool.SetCamera(eye, target);
  if (duration == 0.0f) {
    m_eyeTo = m_eye = eye;
    m_eyeAlt = eyeAlt;
    m_targetAlt = targetAlt;
    m_targetTo = m_target = target;
    m_t = 1.0f;
    return;
  }
  float t = m_t;
  if (moving) {
    if (t > 0.5f) t = 1.0f - t;
  } else {
    t = 0.0f;
  }
  m_t = t;
  m_eyeFrom = m_eye;
  m_targetFrom = m_target;
  m_eyeTo = eye;
  m_eyeAlt = eyeAlt;
  m_targetAlt = targetAlt;
  m_targetTo = target;
  m_duration = duration;
  m_stickT = 0.0f;
  _pGfx->SetMotionBlur(0.5f, 0.9f, 1.0f, 0.15f);
}

// 80166478
void CasScene::InitCamera(unsigned int camera) {
  if (camera >= 10) return;
  m_camera = camera;
  const float* a = kCameraAngles[camera];
  EVec3 eye{a[0], a[1], a[2]}, target{a[3], a[4], a[5]};
  _gCasTweakTool.SetCamera(eye, target);
  m_eyeTo = m_eye = eye;
  m_targetTo = m_target = target;
  m_t = 1.0f;
}

// 801665A8
// After the delay the camera eases from where it was to where it goes. At rest the C-stick's y zooms
// it between that spot and the camera's alternative (up: out), with motion blur while it moves; let go,
// it stays where the tweak tool holds it.
void CasScene::UpdateCamera(float dt) {
  if (m_delay > 0.0f) {
    m_delay -= dt;
    if (m_delay > 0.0f) return;
    m_delay = 0.0f;
  }
  if (m_t < 1.0f) {
    m_t += dt / m_duration;
    if (m_t > 1.0f) m_t = 1.0f;
    const float s = Smooth(m_t);
    m_eye = Lerp(m_eyeFrom, m_eyeTo, s);
    m_target = Lerp(m_targetFrom, m_targetTo, s);
    if (m_t < 1.0f) return;
    m_eyeFrom = m_eye;
    m_targetFrom = m_target;
    return;
  }
  m_prevCamera = m_camera;
  unsigned int filter = 0;
  EController* pad = m_mediator ? CasPad(m_mediator, filter) : nullptr;
  const double y = pad ? pad->GetStick(1, 1, filter) : 0.0;
  if (y == 0.0) {
    _gCasTweakTool.GetCamera(m_eye, m_target);
    _pGfx->SetMotionBlur(0.0f, 0.0f, 1.0f, 0.15f);
    return;
  }
  _pGfx->SetMotionBlur(0.5f, 0.9f, 1.0f, 0.15f);
  m_stickT = static_cast<float>(-y + -y) * _dt + m_stickT;
  double t = m_stickT;
  if (t < 0.0) t = 0.0;
  else if (t > 1.0) t = 1.0;
  m_stickT = static_cast<float>(t);
  const float s = static_cast<float>(t * 0.0 + static_cast<float>(static_cast<float>(t * -2.0) * t) * t +
                                     static_cast<float>(static_cast<float>(t * 3.0) * t));
  m_eye = Lerp(m_eyeFrom, m_eyeAlt, s);
  m_target = Lerp(m_targetFrom, m_targetAlt, s);
  _gCasTweakTool.SetCamera(m_eye, m_target);
}

// 80166A9C
// The scene's window: its field of view (widened to the screen's aspect), near and far planes, and
// the camera looking from the eye at the target, z up.
void CasScene::SetUpWindow(ERC* rc) {
  const float fovY = m_fov * static_cast<float>(_pGfx->m_screenHeight) / static_cast<float>(_pGfx->m_screenWidth);
  m_win->SetProjection(fovY, _pGfx->GetScreenAspect(), m_near, m_far);
  EVec3 up{0.0f, 0.0f, 1.0f};
  m_win->SetLookAt(m_eye, m_target, up);
  m_win->Select(rc);
}

// 80166BB0
// The camera, light set and listener spot of the UI focus: the genetics screens (2..8) look at the
// genetics spot, the roommates (0x42..0x46) at their spots, the mirror (0x34) at the default spot;
// the body and clothing screens use cameras 1..6.
void CasScene::HandleEventChangeFocus(CasEventChangeFocus& event) {
  const int f = event.m_uiFocus;
  int camera = -1;
  if (f >= 2 && f < 9) {
    RepositionCamera(8, 1.0f, 0.4f);
    m_lightSet = 2;
    return;
  }
  if (f == 0x34) {
    RepositionCamera(7, 1.0f, 0.4f);
    m_lightSet = 0;
    return;
  }
  if (f > 0x41 && f <= 0x46 && f != 0x42) {
    RepositionCamera(9, 1.0f, 0.4f);
    m_lightSet = 1;
    return;
  }
  if (f < 2) return;
  if (f < 0x12) camera = 1;
  else if (f < 0x15) camera = 4;
  else if (f == 0x15) camera = 1;
  else if (f == 0x16 || f == 0x1a || f == 0x20) return;
  else if (f == 0x17 || f == 0x18) camera = 2;
  else if (f == 0x19) camera = 3;
  else if (f < 0x20) camera = 4;
  else if (f < 0x26) camera = 1;
  else if (f < 0x2a) camera = 4;
  else if (f == 0x2a || f == 0x2e || f == 0x2f || f == 0x31) return;
  else if (f < 0x2e) camera = 2;
  else if (f == 0x30) camera = 5;
  else if (f == 0x32) camera = 2;
  else if (f == 0x33) camera = 6;
  else if (f == 0x35) camera = 1;
  else if (f == 0x3f || f == 0x40) camera = 2;
  else if (f == 0x41) camera = 3;
  else return;
  RepositionCamera(static_cast<unsigned int>(camera), 1.0f, 0.4f);
  m_lightSet = 0;
}

// 80166E94
// Where an object stands and which way it faces: 0 the genetics spot, 1 above it (plumb bob), 2 the
// mirror spot, 3..6 the family positions.
int CasScene::GetObjectPosition(unsigned int object, EVec3& pos, float& rotation) {
  if (object == 0) {
    pos = kSimPosGenetics;
    rotation = 3.926991f;
  } else if (object == 1) {
    pos = kSimPosGeneticsPlumbBob;
    rotation = 3.926991f;
  } else if (object == 2) {
    pos = kSimPosDefault;
    rotation = 3.1415927f;
  } else if (object <= 6) {
    pos = kPosCAR[object - 3];
    rotation = -0.7853982f;
  } else {
    return 0;
  }
  return 1;
}

// 801672B0
ELights* CasScene::GetLights() { return &m_lights; }
// 801672B8
E3DWindow* CasScene::Get3DWindow() const { return m_win; }
// 801672C0
int CasScene::GetRoomFromCameraAngle(unsigned int) const { return 0; }
// 801672C8
const char* CasScene::GetAmbientScoreName() const { return "cas_scene"; }

// 801754D4
CasSceneDefault::CasSceneDefault() {}
// 80175510
CasSceneDefault::~CasSceneDefault() = default;

// 80175540
int CasSceneDefault::GetMirrorCorners(CasScene__RoomId room, EVec3* corners) const {
  if (room != 0) return 0;
  for (int i = 0; i < 4; ++i) corners[i] = EVec3{kMirrorClothingCorners[i * 3], kMirrorClothingCorners[i * 3 + 1],
                                                 kMirrorClothingCorners[i * 3 + 2]};
  return 1;
}

// 801755A4
const char* CasSceneDefault::GetSceneName() const { return "default"; }

// 801755AC
unsigned int CasSceneDefault::GetRoomModelId(CasScene__RoomId room) const { return room == 0 ? 0x94a667afu : 0u; }
