// Port of the WorldMap of game/thesims.cpp: the 3D neighbourhood behind the main menu (see
// worldmap.hpp). Still stubbed: effects/depth-of-field clean-up.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

WorldMap* g_pWorldMap = nullptr;  // 804FF8F4

namespace {
// The world map's day (s_worldMapDayNightLightTable, built by thesims.cpp's static initializer
// 8009A338): eleven times of day from midnight to 8 pm.
LevelLightingEntry s_worldMapDayNightLightTable[11] = {
    {0.0f, {{0.35f, 0.65f, 1.0f}, 0.41f, {0.35f, 0.55f, 1.0f}, 0.25f}, -75.0f, 109.0f, {0.59f, 0.67f, 1.0f}, 0.4f, 0.2f,
     0.0f, {0.52f, 0.57f, 0.87f}, 0.6f, {1.0f, 1.0f, 0.65f}, 0.29f},
    {4.0f, {{0.49f, 0.66f, 0.8f}, 0.44f, {0.4f, 0.6f, 0.89f}, 0.3f}, -75.0f, 109.0f, {0.59f, 0.7f, 1.0f}, 0.4f, 0.29f,
     0.0f, {0.52f, 0.57f, 0.87f}, 0.4f, {1.0f, 1.0f, 0.78f}, 0.24f},
    {4.5f, {{0.6f, 0.5f, 1.0f}, 0.4f, {0.4f, 0.6f, 1.0f}, 0.3f}, -75.0f, 109.0f, {0.4f, 0.6f, 1.0f}, 0.4f, 0.3f,
     0.0f, {0.45f, 0.7f, 1.0f}, 0.4f, {1.0f, 1.0f, 0.8f}, 0.23f},
    {5.0f, {{0.94f, 0.7f, 0.45f}, 0.5f, {0.44f, 0.59f, 0.96f}, 0.35f}, -19.0f, 113.0f, {0.73f, 0.63f, 0.91f}, 0.4f, 0.3f,
     0.0f, {0.99f, 0.89f, 0.76f}, 0.45f, {1.0f, 1.0f, 0.8f}, 0.23f},
    {6.0f, {{1.0f, 0.5f, 0.0f}, 0.75f, {0.45f, 0.65f, 0.84f}, 0.45f}, -30.0f, 106.0f, {0.72f, 0.68f, 1.0f}, 0.6f, 0.3f,
     0.0f, {0.89f, 0.62f, 0.45f}, 0.75f, {1.0f, 1.0f, 0.65f}, 0.29f},
    {7.0f, {{1.0f, 0.8f, 0.4f}, 0.9f, {0.5f, 0.7f, 0.9f}, 0.6f}, -36.0f, 95.0f, {0.8f, 0.8f, 0.7f}, 0.7f, 0.3f,
     0.0f, {1.0f, 0.9f, 0.6f}, 0.9f, {0.8f, 0.8f, 0.9f}, 0.5f},
    {12.0f, {{1.0f, 0.9f, 0.7f}, 1.0f, {0.74f, 0.74f, 1.0f}, 0.85f}, -64.0f, 34.0f, {0.8f, 0.8f, 0.88f}, 0.8f, 0.3f,
     0.0f, {1.0f, 1.0f, 0.8f}, 1.0f, {0.7f, 0.7f, 0.7f}, 0.7f},
    {17.0f, {{1.0f, 0.9f, 0.6f}, 0.96f, {0.7f, 0.7f, 0.88f}, 0.77f}, -56.0f, 0.0f, {0.7f, 0.7f, 1.0f}, 0.8f, 0.3f,
     0.0f, {1.0f, 0.9f, 0.6f}, 1.0f, {0.8f, 0.8f, 1.0f}, 0.34f},
    {18.0f, {{1.0f, 0.6f, 0.2f}, 1.0f, {0.62f, 0.6f, 0.8f}, 0.5f}, -43.0f, -47.0f, {0.77f, 0.62f, 0.9f}, 0.6f, 0.3f,
     0.0f, {1.0f, 0.7f, 0.46f}, 1.0700001f, {0.98f, 0.78f, 0.85f}, 0.5f},
    {19.0f, {{1.0f, 0.5f, 0.0f}, 0.85f, {0.63f, 0.63f, 0.85f}, 0.45f}, -19.0f, -54.0f, {0.87f, 0.87f, 1.0f}, 0.7f, 0.3f,
     0.0f, {1.0f, 0.5f, 0.17f}, 0.77f, {1.0f, 1.0f, 0.8f}, 0.41f},
    {20.0f, {{1.0f, 0.51f, 0.68f}, 0.56f, {0.55f, 0.59f, 0.95f}, 0.29f}, -40.0f, -53.0f, {0.6f, 0.6f, 1.0f}, 0.4f, 0.3f,
     1.4e-45f, {0.7f, 0.7f, 1.0f}, 0.4f, {1.0f, 1.0f, 0.8f}, 0.23f},
};
LevelLighting s_worldMapDayNightLight = {s_worldMapDayNightLightTable, 11};  // 804FDB90

// The logo: top left at 5% of the screen, 0.4 of its size (0.4 x 0.571), white at 60%.
EVec2 s_logoTL{0.05f, 0.05f};           // 804FF8E0
EVec2 s_logoSize{0.4f, 0.5714286f};     // 804FF8E8
float s_logoScale = 0.4f;               // 804FDB98
EVec4 s_logoColor{1.0f, 1.0f, 1.0f, 0.6f};  // 80485D80

CameraDirector* Director() { return _globals.CameraDirectorOf(0); }
}  // namespace

// 800998C8
// Takes the level (the loading screen stays up until it is in), the scenery's model, animation and
// character and the logo, and makes the camera: full screen, frustum type 1, 45 degrees, zoomed out.
WorldMap::WorldMap() {
  m_level = static_cast<ERLevel*>(_levelman.AddRefAsync(kLevel));
  if (!m_level) {
    LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{0});
    m_waitingForLevel = 1;
  }
  m_sceneryModel = static_cast<ERModel*>(_modelman.AddRefAsync(kScenery));
  m_sceneryAnim = static_cast<ERAnim*>(_animman.AddRefAsync(kSceneryAnim));
  m_sceneryCharacter = static_cast<ERCharacter*>(_characterman.AddRefAsync(kScenery));
  m_logo = static_cast<ERShader*>(_shaderman.AddRefAsync(static_cast<unsigned int>(GameData::GetLocalizedSims2Logo())));
  m_cam = new ESimsCam(0);  // GC: 0x570 bytes, 16-aligned, from the main heap
  m_cam->ForceFullScreenViewport();
  m_cam->m_win.SetFrustumType(1);
  m_cam->SetFov(45.0f);
  m_cam->SetZoom(m_cam->m_params->m_zoomMax);
  m_cam->UpdateWin();
  if (m_level) {
    Reset();
    // PC: loads complete at once, so the level is already in; end the loading screen as Update does
    // when it arrives.
    LoadingScreenStateMachine::LoadFinishing();
  }
  EInstance::SetContext(0);
}

// 80099AA8
WorldMap::~WorldMap() {
  if (m_level) {
    Effects__EffectsManager::GetSingleton()->DeleteAll();
    if (m_scenery) {
      m_scenery->SafeDelete();  // GC: out of the level by its destructor
      m_scenery = nullptr;
    }
    _pEngine->m_currentLevel = nullptr;
    m_level = nullptr;
    if (CameraDirector* director = Director()) director->m_flags |= 1;
    // The current director's depth of field off.
    FrameEffectsManager* fx = _globals.m_cameraDirector ? _globals.m_cameraDirector->GetFrameEffectsManager() : nullptr;
    if (DepthOfField* dof = fx ? fx->DepthOfFieldObject() : nullptr)
      if (dof->m_state != kFrameEffectOff) dof->Enable(false);
  }
  m_sceneryModel = nullptr;
  _modelman.DelRefAsync(kScenery);
  m_sceneryAnim = nullptr;
  _animman.DelRefAsync(kSceneryAnim);
  m_sceneryCharacter = nullptr;
  _characterman.DelRefAsync(kScenery);
  m_logo = nullptr;
  _shaderman.DelRefAsync(static_cast<unsigned int>(GameData::GetLocalizedSims2Logo()));
  delete m_cam;
  m_cam = nullptr;
  _levelman.DelRefAsync(kLevel);
}

// 80099C44
// Makes the level current with the world map's day/night table, a default sun and ambient light, one
// room (the outside), lighting for the current hour, and starts the camera animation on the player's
// director.
void WorldMap::Reset() {
  m_waitingForLevel = 0;
  _pEngine->m_currentLevel = m_level;
  m_level->m_dayNight = &s_worldMapDayNightLight;
  m_level->m_rainInfo = nullptr;
  m_level->m_sunYawOffset = 0.0f;
  m_level->m_sun.m_dir = EVec3{0.57735f, 0.57735f, -0.57735f};
  m_level->m_sun.m_color = EVec3{1.0f, 1.0f, 1.0f};
  m_level->m_sun.m_intensity = 1.5f;
  m_level->m_sunAmbient.m_color = EVec3{0.9f, 0.9f, 1.0f};
  m_level->m_sunAmbient.m_intensity = 0.3f;
  m_level->m_shadowAlpha = 1.0f;
  m_level->SetRoomCount(1);
  LevelLightingEntry entry;
  m_level->OutsideLightsInit(m_hour, entry);
  m_level->ResetLightLocations();
  m_level->InitializeLightingGrid();
  m_level->Update();
  if (CameraDirector* director = Director()) {
    _globals.SetCameraDirector(director);
    director->RegisterCurrentCamera(m_cam, true);
    director->m_flags &= ~1u;
    director->StartAnim(kCameraAnim, false, false);
  }
}

// 80099E00
// Waits for the level (then ends the loading screen and resets); takes the scenery resources as they
// arrive and puts the scenery in the level once all are in; runs its animation and the camera
// director (both stopped while time is not running); advances the time of day and updates the outside
// lights and the level.
void WorldMap::Update() {
  if (!m_level) {
    if (m_waitingForLevel) {
      m_level = static_cast<ERLevel*>(_levelman.GetRef(kLevel));
      if (!m_level) return;
      LoadingScreenStateMachine::LoadFinishing();
      Reset();
    }
    if (!m_level) return;
  }
  if (!m_sceneryModel) m_sceneryModel = static_cast<ERModel*>(_modelman.GetRef(kScenery));
  if (!m_sceneryAnim) m_sceneryAnim = static_cast<ERAnim*>(_animman.GetRef(kSceneryAnim));
  if (!m_sceneryCharacter) m_sceneryCharacter = static_cast<ERCharacter*>(_characterman.GetRef(kScenery));
  if (!m_logo) m_logo = static_cast<ERShader*>(_shaderman.GetRef(static_cast<unsigned int>(GameData::GetLocalizedSims2Logo())));
  if (m_sceneryModel && m_sceneryAnim && m_sceneryCharacter && !m_scenery) {
    // An object without a cXObject: model and character "scenery_outerlot" playing its animation
    // (looping) at the model's scale.
    m_scenery = new ISimsObjectModel();  // GC: 0x4C0 bytes, 16-aligned, from the main heap
    m_level->InsertInstance(m_scenery, nullptr);
    m_scenery->InsertSubModelsInHouse(m_level);
    EAnimController* anim = &m_scenery->m_anim;
    m_scenery->SetModel(kScenery, false, nullptr, anim);
    m_scenery->Create(nullptr, nullptr);
    anim->Init(kScenery, eAnimatedObjectType{1}, 0);
    anim->m_scale = m_sceneryModel->m_scale;
    anim->SetTrackAnim(eTrackFlags{eTrackFlags::kMainMask}, kSceneryAnim, -1, -1.0f, nullptr);
    anim->m_computeCallback = nullptr;
    anim->m_computeCallbackData = 0;  // GC: the WorldMap (unused without a callback)
    anim->m_speed = 1.0f;
  }
  if (m_scenery) {
    _gamedt = _dt;
    m_scenery->m_anim.SetAllTrackSpeed(m_running ? 1.0f : 0.0f);
    m_scenery->UpdateAnim(&m_sceneryAnimDef);
  }
  if (CameraDirector* director = Director()) {
    director->m_anim.SetAllTrackSpeed(m_running ? 1.0f : 0.0f);
    director->Update();
  }
  const float step = m_running ? _dt * 0.00027777778f * m_hoursPerHour : 0.0f;
  float hour = m_hour + step;
  m_hourStep = step;
  m_hour = hour;
  if (hour >= 24.0f) {
    do hour -= 24.0f;
    while (hour >= 24.0f);
    m_hour = hour;
  }
  m_fieldD0 = 0;
  m_hourInt = static_cast<int>(m_hour);
  m_minute = static_cast<int>((m_hour - static_cast<float>(m_hourInt)) * 60.0f);
  LevelLightingEntry entry;
  m_level->OutsideLightsUpdate(m_hour, entry);
  m_level->Update();
}

// 8009A1D0
// The camera's window, the level, and (when the menu asks) the logo at the top left.
void WorldMap::Draw(ERC* rc) {
  if (!m_level || !m_cam) return;
  m_cam->UpdateWin();
  m_cam->m_win.Select(rc);
  m_level->Draw(rc);
  if (m_drawLogo && m_logo) {
    if (EShader* shader = m_logo->GetShader()) shader->Select(rc, 0);
    EVec2 br{s_logoTL.x + s_logoScale * s_logoSize.x, s_logoTL.y + s_logoScale * s_logoSize.y};
    EVec2 uv0{0.0f, 1.0f}, uv1{1.0f, 0.0f};
    rc->Rect(s_logoTL, br, uv0, uv1, s_logoColor, 0.0f);
  }
}
