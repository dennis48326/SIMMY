// Port of game/camera.cpp: the camera tuning, ESimsCam (follow, rotation, zoom, smoothing, the classic
// cursor's area, the motion system, noise) and CameraDirector (camera animations, cuts and
// interpolations, the way back to the game camera, the letterbox, note-track events). See
// docs/camera.md for what is left (debug drawing, paths the release build never reaches).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"
#include "simmy/classes/buildtools.hpp"
#include "simmy/detmath.hpp"

bool PlayerInSocialMode(int player);  // socialmode.cpp
bool IsUsingDirectControlInteractor(int player);  // inlevel.cpp
// PC stand-in for InteractorManager::GetPlayerInteractor(player, -1): the player's classic or direct
// interactor, null for the build/buy manipulators (directinteractor.cpp).
InteractorModule__Interactor* PcPlayerInteractor(int player);

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
int s_camParamNum = 0;  // _camParamNum
}  // namespace

// 804735D8 (constructed by camera.cpp's static initializer, 8001E87C)
SimsCameraParameters ESimsCam::s_params[kMaxPlayers];  // PC mod: 4 players (GC [2])

// 80015BE8
SimsCameraParameters::SimsCameraParameters() {
  m_field04 = 1;
  m_zoomMax = 15.0f;
  m_field030 = 2.25f;
  m_field038 = 6.0f;
  m_field03C = 35.0f;
  m_field008 = 0.2f;
  m_field00C = 1.0f;
  m_field010 = 3.0f;
  m_field014 = 1.0f;
  m_field018 = 2.0f;
  m_field01C = 2.0f;
  m_field020 = 3.0f;
  m_field024 = 3.0f;
  m_zoomMin = 4.0f;
  m_field034 = 4.0f;
  m_field040 = 0.0f;
  m_field048 = 1.4f;
  m_field044 = 0.2f;
  m_field04C = 1.5f;
  m_field050 = 1.3f;
  m_field054 = 1.3f;
  m_field058 = 4.0f;
  m_field05C = 8.0f;
  m_fovMax = 65.0f;
  m_farPlane = 2000.0f;
  m_nearPlaneZoomed = 3.0f;
  m_field074 = 70.0f;
  m_field078 = 100.0f;
  m_field080 = 1.5f;
  m_targetHeight = 1.25f;
  m_field094 = 2.0f;
  m_field098 = 55.0f;
  m_field0A0 = 0.15f;
  m_field0A4 = 0.6f;
  m_fovMin = 55.0f;
  m_nearPlane = 2.0f;
  m_field07C = 0.8f;
  m_field084 = 0.8f;
  m_field088 = 1.0f;
  m_field090 = 10.0f;
  m_field09C = 0.15f;
  m_field0A8 = 0.85f;
  m_field0AC = 0.8f;
  m_field0B0 = 0.0f;
  m_refFrom = EVec3{0.0f, 0.0f, 0.0f};
  m_refTo = EVec3{10.0f, 10.0f, 0.0f};
  m_field0D4 = 0.0f;
  m_field0DC = 1.0f;
  m_field0D8 = 0.0f;
  m_field1C0 = 0;
  m_index = s_camParamNum++;
  m_field0B4 = m_field038;
  m_field0B8 = m_zoomMin;
  // The seven noise settings: none enabled; periods 1 s (0-3) or 0.05 s (4-6), speeds and priorities.
  // (Setting 3 is left all zero.)
  const float speeds[7] = {10.0f, 0.05f, 0.05f, 0.0f, 3.0f, 0.05f, 0.05f};
  const int priorities[7] = {3, 3, 3, 0, 2, 2, 2};
  for (int i = 0; i < 7; ++i) {
    m_noiseEnabled[i] = 0;
    m_noise[i] = Noise{{0.0f, 0.0f, 0.0f}, i == 3 ? 0.0f : 1.0f};
    m_noiseSpeed[i] = speeds[i];
    m_noisePriority[i] = priorities[i];
    m_noiseUseAnim[i] = 0;
  }
}

// 80016010
SimsCameraParameters::~SimsCameraParameters() = default;

// 80016044
// The far zoom at least the near one; the start tilt and distance from the near tilt and zoom.
void SimsCameraParameters::Validate() {
  if (m_zoomMax < m_zoomMin) m_zoomMax = m_zoomMin;
  m_field0B4 = m_field038;
  m_field0B8 = m_zoomMin;
}

// 80473514 (constructed by camera.cpp's static initializer, 8001E87C)
NewControlParms ESimsCam::s_newControlParms;

// 8001E6DC
NewControlParms::NewControlParms() {
  m_followArea = 1;
  m_followRate = 0.6f;
  m_trap[0] = 0.05f;
  m_trap[1] = 0.5f;
  m_trap[2] = 0.9f;
  m_trap[3] = 0.5f;
  m_topWidthNear = 0.5f;
  m_topWidthFar = 0.45f;
  m_depthNear = 3.0f;
  m_depthFar = 5.0f;
  m_centerArea = 1;
  m_fieldA4 = 0;
  m_centerPull = 0.3f;
  m_areaScale = 1.0f;
  m_areaFront = 0.5f;
  m_areaFrontWidth = 1.25f;
  m_areaBack = -0.5f;
  m_areaBackWidth = 1.25f;
}

// 8039DADC
NewControlParms::~NewControlParms() = default;

// 8001E878
void NewControlParms::Validate() {}

// ---- ESimsCam -----------------------------------------------------------------------------------

// 8001606C
CameraMotionSystem::CameraMotionSystem(SimsCameraParameters* params) {
  m_type = 0;
  m_params = params;
}

// 80016080
CameraMotionSystem::~CameraMotionSystem() = default;

// 800160A8
void CameraMotionSystem::SetMotionType(int type) { m_type = type; }
// 800160B0
int CameraMotionSystem::GetMotionType() { return m_type; }

// 800160B8
void CameraMotionSystem::SetRig(EVec3& eye, EVec3& target, EVec3& up) {
  m_rigEye = eye;
  m_rigTarget = target;
  m_rigUp = up;
}

namespace {
EVec3 Normalized(EVec3 v) {
  if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f) {
    const float k = 1.0f / std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    v.x *= k, v.y *= k, v.z *= k;
  }
  return v;
}
}  // namespace

// 80016110
void CameraMotionSystem::SnapCamera() {
  m_eye = m_rigEye;
  m_target = m_rigTarget;
  m_up = m_rigUp;
  m_forward = Normalized(EVec3{m_target.x - m_eye.x, m_target.y - m_eye.y, m_target.z - m_eye.z});
}

// 8001621C
void CameraMotionSystem::MoveCamera(float dt) {
  if (m_type == 1) RubberBandMove(dt);
  else SnapCamera();
}

// 80016258
// `value` moves toward `target` by the tuning's stiffness (+0x50, or +0x54 within +0x4C) x dt of the
// distance, at most all of it (GC: the frame's _dt).
void CameraMotionSystem::RubberBandInterpolate(EVec3* value, EVec3 target, float) {
  const EVec3 d{target.x - value->x, target.y - value->y, target.z - value->z};
  const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  if (len <= 9.999999747378752e-05f) {
    *value = target;
    return;
  }
  const float stiffness = len <= m_params->m_field04C ? m_params->m_field054 : m_params->m_field050;
  float step = stiffness * len * _dt;
  if (len < step) step = len;
  const float k = 1.0f / len;
  value->x += d.x * k * step;
  value->z += d.z * k * step;
  value->y += d.y * k * step;
}

// 8001639C
void CameraMotionSystem::RubberBandMove(float dt) {
  RubberBandInterpolate(&m_eye, m_rigEye, dt);
  RubberBandInterpolate(&m_target, m_rigTarget, dt);
  m_up = m_rigUp;
  m_forward = Normalized(EVec3{m_target.x - m_eye.x, m_target.y - m_eye.y, m_target.z - m_eye.z});
}

// 800164D4
// The camera of `player`, with that player's tuning and motion system; noise animations use the
// "cam_base" character.
ESimsCam::ESimsCam(int player) {
  m_player = player;
  m_filterId = 0;
  m_params = &s_params[player];
  m_motion = new CameraMotionSystem(m_params);
  m_noiseDir = m_noiseStep = m_noiseBase = EVec3{};
  m_noiseTime = 0.0f;
  m_noiseLen = 0.0f;
  m_noiseSetting = -1;
  m_field540 = 1;
  m_roomId = 0;
  m_roomWallsOpaque = 0;
  m_noiseAnim.Init(0xfeace9cb, eAnimatedObjectType{2}, 0);
  m_field470 = 0.0f;
  m_field474 = 1;
  m_field568 = 0;
  m_field46C = 0;
  m_field564 = 0;
}

// 8001669C
ESimsCam::~ESimsCam() {
  delete m_motion;
  m_motion = nullptr;
  m_noiseAnim.Shutdown();
  m_params = nullptr;
}

// 80016728
// Where the zoom is between the tuning's near and far distances (0..1).
float ESimsCam::GetCurZoomRatio() {
  const float r = (m_zoom - m_params->m_zoomMin) / (m_params->m_zoomMax - m_params->m_zoomMin);
  if (r < 0.0f) return 0.0f;
  return r <= 1.0f ? r : 1.0f;
}

// 80016770
// The near plane goes from the zoomed-in value to the tuning's near plane with the zoom. (GC: a
// direct-control interactor whose sim gets close to the camera pulls it in to 0.15; interactors are
// not ported.)
float ESimsCam::GetNearPlane() {
  const float ratio = GetCurZoomRatio();
  float nearIn = m_params->m_nearPlane;
  if (m_field568 || m_field564) nearIn = 0.15f;
  return m_params->m_nearPlaneZoomed * ratio + nearIn * (1.0f - ratio);
}

// 80016990
float ESimsCam::GetFarPlane() { return m_params->m_farPlane; }

// 8001699C
float ESimsCam::GetFov() {
  const float ratio = GetCurZoomRatio();
  return m_params->m_fovMax * ratio + m_params->m_fovMin * (1.0f - ratio);
}

// 800169E8
// The zoom whose field of view is `fov`.
void ESimsCam::SetFov(float fov) {
  const SimsCameraParameters& p = *m_params;
  const float range = p.m_fovMax - p.m_fovMin;
  m_zoom = ((fov - p.m_fovMin) * (p.m_zoomMax - p.m_zoomMin) + p.m_zoomMin * range) / range;
}

// 80016A1C
// In two-player games the camera's window is made full screen.
void ESimsCam::ForceFullScreenViewport() {
  if (_gd.m_fieldEC) {
    SetWinPos(m_win);
    TRect<float> full{0.0f, 0.0f, 1.0f, 1.0f};
    m_win.SetViewport(full);
  }
}

// 80016FFC
// Moves the zoom by `delta`, kept inside the tuning's range.
void ESimsCam::ApplyZoom(float delta) {
  const float zoom = m_zoom - delta;
  m_zoom = zoom;
  if (zoom < m_params->m_zoomMin) m_zoom = m_params->m_zoomMin;
  else if (zoom > m_params->m_zoomMax) m_zoom = m_params->m_zoomMax;
  m_tilt = static_cast<float>(CalcTiltFromZoom(m_zoom));
}

// 80017070
void ESimsCam::SetZoom(float zoom) {
  m_zoom = zoom;
  ApplyZoom(0.0f);
}

// 80017490
// Full-window viewport, the projection (the field of view is horizontal: scaled by height / width),
// the look-at, the room the eye is in (that room's lighting group, GC: Room +0x02) and whether its walls
// and roof are opaque; then, unless a CameraDirector shot is playing, the ground area around the target
// (+0x390: s_newControlParms' front and back offsets along the view, each that wide across it).
void ESimsCam::UpdateWin() {
  TRect<float> full{0.0f, 0.0f, 1.0f, 1.0f};
  m_win.SetViewport(full);
  const float fov = GetFov() * static_cast<float>(_pGfx->m_screenHeight) / static_cast<float>(_pGfx->m_screenWidth);
  m_win.SetProjection(fov, _pGfx->GetScreenAspect(), GetNearPlane(), GetFarPlane());
  SetWinPos(m_win);
  m_roomId = CalRoomId(m_eye);
  if (Globs_pRoomManager && m_roomId < 0xfffb)
    if (Room* room = Globs_pRoomManager->GetRoom(m_roomId)) m_roomId = room->m_lightingGroup;
  if (!_globals.m_house) {
    m_roomWallsOpaque = 0;
  } else {
    ERoom* walls = _globals.m_house->m_room;
    m_roomWallsOpaque = walls && walls->IsRoomWallsOpaque(m_roomId, m_player) && walls->IsRoomRoofOpaque(m_roomId, m_player) ? 1 : 0;
  }
  auto* director = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player));
  if (director && director->IsActive()) return;
  const NewControlParms& c = s_newControlParms;
  const EVec3 t{m_target.x, m_target.y, 0.0f};
  EVec3 d{m_eye.x - t.x, m_eye.y - t.y, 0.0f};
  if (d.x != 0.0f || d.y != 0.0f) d = Normalized(d);
  EVec3* q = m_cursorQuad.m_points;
  const float front = c.m_areaFront * c.m_areaScale, back = c.m_areaBack * c.m_areaScale;
  q[0] = q[1] = EVec3{t.x + d.x * front, t.y + d.y * front, t.z + d.z * front};
  q[2] = q[3] = EVec3{t.x + d.x * back, t.y + d.y * back, t.z + d.z * back};
  const float half = c.m_areaScale * 0.5f;
  EVec3 side{d.y * c.m_areaFrontWidth * half, -d.x * c.m_areaFrontWidth * half, d.z * c.m_areaFrontWidth * half};
  q[1].x -= side.x, q[1].z -= side.z, q[1].y -= side.y;
  q[0].x += side.x, q[0].z += side.z, q[0].y += side.y;
  side = EVec3{d.y * c.m_areaBackWidth * half, d.z * c.m_areaBackWidth * half, -d.x * c.m_areaBackWidth * half};
  q[2].x -= side.x, q[2].z -= side.y, q[2].y -= side.z;
  q[3].x += side.x, q[3].z += side.y, q[3].y += side.z;
}

// 800187E8
void ESimsCam::SnapActualCameraParamsToTarget() {
  m_actualTarget = m_target;
  m_actualZoom = m_zoom;
  m_actualTilt = m_tilt;
  m_actualRotAngle = m_rotAngle;
}

// 80018E1C
void ESimsCam::GetPos(EVec3& eye, EVec3& target, EVec3& up) {
  eye = m_eye;
  target = m_target;
  up = m_up;
}

// 80018FE4
// Places the camera: eye, target and up, the rotation (degrees from the tuning's reference direction)
// and tilt it implies, and the distance as the zoom; then snaps to it.
void ESimsCam::SetPos(EVec3& eye, EVec3& target, EVec3& up) {
  m_eye = eye;
  EVec3 dir{target.x - eye.x, target.y - eye.y, target.z - eye.z};
  const float pitch = CalcPitch(dir);
  const float theta = CalcZAxisTheta(dir);
  EVec3 ref{m_params->m_refTo.x - m_params->m_refFrom.x, m_params->m_refTo.y - m_params->m_refFrom.y,
            m_params->m_refTo.z - m_params->m_refFrom.z};
  float angle = theta - CalcZAxisTheta(ref);
  if (angle < 0.0f) angle += 6.2831855f;
  m_rotAngle = angle * 180.0f / 3.1415927f;
  m_tilt = pitch * 180.0f / 3.1415927f;
  m_zoom = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  m_target = target;
  m_up = up;
  SnapActualCameraParamsToTarget();
  SetWinPos(m_win);
}

namespace {
// The tile coordinate of a lot-relative position: truncated, then up from .5.
int RoundTile(float v) {
  int i = static_cast<int>(v);
  if (0.5f <= v - static_cast<float>(i)) ++i;
  return i;
}
}  // namespace

// 80019628
// The room id of the floor tile at a position (relative to the house's origin; x is the tile's y);
// 0xFFFB off the lot's first row/column, 0 without a house.
unsigned short ESimsCam::CalRoomId(EVec3& pos) {
  EHouse* house = _globals.m_house;
  if (!house) return 0;
  const int x = RoundTile(pos.y - house->m_posX);
  const int y = RoundTile(pos.x - house->m_posY);
  if (x < 1 || y < 1 || !Globs_pFixedWorld) return 0xfffb;
  CTilePt tile(x, y, 1);
  return Globs_pFixedWorld->GetRoom(tile);
}

// 8001976C
// The room id there when the four tiles around it are in the same room; else 0xFFFB.
unsigned short ESimsCam::GetContainingRoomId(EVec3& pos) {
  EHouse* house = _globals.m_house;
  if (!house) return 0;
  const int x = RoundTile(pos.y - house->m_posX);
  const int y = RoundTile(pos.x - house->m_posY);
  if (0 < x && 0 < y && Globs_pFixedWorld) {
    cFixedWorldImpl* w = Globs_pFixedWorld;
    CTilePt t0(x, y, 1), t1(x, y + 1, 1), t2(x, y - 1, 1), t3(x + 1, y, 1), t4(x - 1, y, 1);
    const unsigned short r = w->GetRoom(t0);
    if (r == w->GetRoom(t1) && r == w->GetRoom(t2) && r == w->GetRoom(t3) && r == w->GetRoom(t4)) return r;
  }
  return 0xfffb;
}

// 80019A00
RET_UNKNOWN ESimsCam::CursorMoved(int, EVec3&) { return 0; }

// 80019A04
// While a CameraDirector shot plays, the window looks from the eye (plus noise) at the target.
// Otherwise the rig: the eye from the eased camera parameters (the target at the tuning's height,
// except while the director plays the game camera), handed to the motion system, which snaps to it
// (m_snap, set by ResetPos / ForcePosition) or moves toward it; the window then looks from where the
// motion system's camera is (plus noise) along its forward direction. (GC: animation modes 1-4 follow
// objects; SetAnimationMode is never called.)
void ESimsCam::SetWinPos(E3DWindow& win) {
  auto* director = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player));
  if (director && director->IsActive()) {
    SnapActualCameraParamsToTarget();
    EVec3 eye = ApplyNoise(m_eye, m_actualTarget, m_up);
    win.SetLookAt(eye, m_actualTarget, m_up);
    return;
  }
  EVec3 eye;
  if (m_animMode == 4) {
    SnapActualCameraParamsToTarget();
    m_motion->SetRig(m_eye, m_actualTarget, m_up);
    m_snap = 1;
  } else {
    if (m_animMode >= 1 && m_animMode <= 3)
      SIMMY_STUB("ESimsCam::SetWinPos: camera animation modes on objects (GetObjectPosition)", 0x80019A04);
    if (!director || director->m_state != CameraDirector::kGame) m_target.z = m_params->m_targetHeight;
    InterpolateActualCameraParamsToTarget(_dt);
    ESimsCam__CameraParameters actual;
    actual.m_target = m_actualTarget;
    actual.m_zoom = m_actualZoom;
    actual.m_rotAngle = m_actualRotAngle;
    actual.m_tilt = m_actualTilt;
    CalcEyePosition(eye, actual);
    m_motion->SetRig(eye, m_actualTarget, m_up);
  }
  if (!m_snap) {
    m_motion->MoveCamera(_dt);
  } else {
    m_snap = 0;
    m_motion->SnapCamera();
  }
  m_eye = m_motion->m_eye;
  const EVec3 fwd = m_motion->m_forward;
  EVec3 at{m_eye.x + fwd.x, m_eye.y + fwd.y, m_eye.z + fwd.z};
  EVec3 noisy = ApplyNoise(m_eye, at, m_motion->m_up);
  EVec3 look{noisy.x + fwd.x, noisy.y + fwd.y, noisy.z + fwd.z};
  win.SetLookAt(noisy, look, m_motion->m_up);
}

// 80019DFC
// The angle between the direction and the ground plane (2 pi minus it when pointing up).
float ESimsCam::CalcPitch(EVec3& dir) {
  const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  if (len < 1e-5f) return 0.0f;
  const float flat = std::sqrt(dir.x * dir.x + dir.y * dir.y);
  float a = simmy::dm::acos(flat / len);
  if (dir.z > 0.0f) a = 6.2831855f - a;
  return a;
}

// 80019EA8
// The direction's angle about z from +x (0..2 pi).
float ESimsCam::CalcZAxisTheta(EVec3& dir) {
  float x = dir.x, y = dir.y;
  if (x != 0.0f || y != 0.0f) {
    const float inv = 1.0f / std::sqrt(x * x + y * y);
    x *= inv, y *= inv;
  }
  if (x > 1.0f) x = 1.0f;
  if (x < -1.0f) x = -1.0f;
  float a = simmy::dm::acos(x);
  if (y < 0.0f) a = 6.2831855f - a;
  return a;
}

// 8001A328
// A new noise setting takes over unless the running one has a higher priority (none while in author
// mode, -1 stops it); one that plays an animation starts it on the noise controller.
void ESimsCam::SetActiveNoiseSetting(int setting) {
  const int current = m_noiseSetting;
  if (setting == current) return;
  if (setting == -1 || _gd.m_fieldF0) {
    m_noiseSetting = -1;
  } else if (current == -1 || m_params->m_noisePriority[current] <= m_params->m_noisePriority[setting]) {
    m_noiseSetting = setting;
    if (m_params->m_noiseUseAnim[setting] && m_noiseAnims[setting]) {
      if (m_noiseAnim.GetTrackAnimId(eTrackFlags{1}) != m_noiseAnims[m_noiseSetting]) {
        m_noiseAnim.StopAllTracks();
        m_noiseAnim.SetTrackAnim(eTrackFlags{1}, m_noiseAnims[m_noiseSetting], -1, -1.0f, nullptr);
      }
    }
  }
}

namespace {
// 802CB890 Rndf / 802CB8E4 SignedRndf: the engine's own LCG (_urandseed), -1..1. PC: a copy for the
// camera noise (the generated Rndf is a stub; with the shipped tuning the noise never draws).
unsigned int s_urandseed = 0;
float CamSignedRndf() {
  s_urandseed = s_urandseed * 0x41c64e6fu + 0x303bu;
  const float r = static_cast<float>((s_urandseed >> 16) & 0x7fff) * 3.0517578e-05f;
  return (r + r) - 1.0f;
}
}  // namespace

// 8001A424
// The eye with the active camera noise added. A setting enabled in the tuning (+0xE0) is made active
// (its animation started). None active, or the simulator paused: no noise. A setting that plays an
// animation adds its root's translation; otherwise a random walk: every period a new random offset
// (scaled by the setting's amplitudes) to head for, approached at its speed. (With the shipped tuning
// no setting is enabled and every amplitude is zero, so the eye is unchanged.)
EVec3 ESimsCam::ApplyNoise(EVec3 eye, EVec3, EVec3) {
  for (int i = 0; i < 7; ++i) {
    if (m_params->m_noiseEnabled[i]) {
      m_noiseSetting = i;
      if (m_params->m_noiseUseAnim[i] && m_noiseAnims[i] && m_noiseAnim.GetTrackAnimId(eTrackFlags{1}) != m_noiseAnims[i]) {
        m_noiseAnim.StopAllTracks();
        m_noiseAnim.SetTrackAnim(eTrackFlags{1}, m_noiseAnims[i], -1, -1.0f, nullptr);
      }
      break;
    }
  }
  auto none = [&]() {
    m_noiseDir = m_noiseStep = m_noiseBase = EVec3{};
    m_noiseLen = 0.0f;
    m_noiseTime = 0.0f;
    return eye;
  };
  if (m_noiseSetting == -1 || (Globs_pSimulator && Globs_pSimulator->IsPaused())) return none();
  const int n = m_noiseSetting;
  EVec3 offset{0.0f, 0.0f, 0.0f};
  if (!m_params->m_noiseUseAnim[n]) {
    const SimsCameraParameters::Noise& noise = m_params->m_noise[n];
    if (noise.amp[0] == 0.0f && noise.amp[1] == 0.0f && noise.amp[2] == 0.0f && noise.period == 1.0f) return none();
    if (noise.period <= m_noiseTime) {
      m_noiseBase.x += m_noiseDir.x * m_noiseLen;
      m_noiseBase.y += m_noiseDir.y * m_noiseLen;
      m_noiseBase.z += m_noiseDir.z * m_noiseLen;
      const float rx = CamSignedRndf() * noise.amp[0];
      const float ry = CamSignedRndf() * noise.amp[1];
      const float rz = CamSignedRndf() * noise.amp[2];
      m_noiseStep = EVec3{rx - m_noiseBase.x, ry - m_noiseBase.y, rz - m_noiseBase.z};
      m_noiseLen = std::sqrt(m_noiseStep.x * m_noiseStep.x + m_noiseStep.y * m_noiseStep.y + m_noiseStep.z * m_noiseStep.z);
      m_noiseStep = Normalized(m_noiseStep);
      m_noiseTime = 0.0f;
    }
    const float speed = m_params->m_noiseSpeed[n];
    m_noiseDir = Normalized(EVec3{m_noiseDir.x + m_noiseStep.x * speed, m_noiseDir.y + m_noiseStep.y * speed,
                                  m_noiseDir.z + m_noiseStep.z * speed});
    m_noiseTime += _dt;
    float k = m_noiseTime / noise.period;
    if (1.0f < k) k = 1.0f;
    offset = EVec3{m_noiseBase.x + m_noiseDir.x * m_noiseLen * k, m_noiseBase.y + m_noiseDir.y * m_noiseLen * k,
                   m_noiseBase.z + m_noiseDir.z * m_noiseLen * k};
  } else if (m_noiseAnim.GetTrackAnimId(eTrackFlags{1}) != 0) {
    EMat4 id;
    id.Id();
    m_noiseAnim.Update(nullptr, nullptr, EVec3{1.0f, 1.0f, 1.0f});
    m_noiseAnim.Compute(id, _framecount);
    if (m_noiseAnim.m_matrices) offset = EVec3{m_noiseAnim.m_matrices[0].m[3][0], m_noiseAnim.m_matrices[0].m[3][1], m_noiseAnim.m_matrices[0].m[3][2]};
  }
  return EVec3{eye.x + offset.x, eye.y + offset.y, eye.z + offset.z};
}

// 8001EA6C
E3DWindow* ESimsCam::GetWin() { return &m_win; }
// 8001EAA4
int ESimsCam::GetPlayerId() { return m_player; }

namespace {
// this = m * *attach * *attach2 for the attached matrices there are (EMat4::Mult4x4(out, a, b): out = a b).
void ApplyAttached(EMat4& m, EMat4* attach, EMat4* attach2) {
  EMat4 t;
  if (attach) t.Mult4x4(m, *attach), m = t;
  if (attach2) t.Mult4x4(m, *attach2), m = t;
}
float SecondsNow() { return static_cast<float>(_pClockMan->GetSec(&_sysclock)); }
}  // namespace

// ---- CameraDirector -----------------------------------------------------------------------------

// 8001AE60
CameraDirector::CameraDirector() {
  m_field120.Id();
  m_controllerFilter = 0;
  m_field324 = 0;
  m_field328 = 0;
  m_cam = nullptr;
  InitAnim(0xfeace9cb);
  m_frameEffects = new FrameEffectsManager;
  m_frameEffects->Init();
  Reset();
}

// 8001AEE4
CameraDirector::~CameraDirector() {
  m_cam = nullptr;
  m_anim.Shutdown();
  m_frameEffects->Shutdown();
  delete m_frameEffects;
  m_frameEffects = nullptr;
  Reset();
}

// 8001AF60
void CameraDirector::Reset() {
  m_flags &= ~1u;
  EnableControls();
  m_state = kIdle;
  m_nextState = kIdle;
  m_transform = 0;
  m_transformB.Id();
  m_transformA.Id();
  m_cam = nullptr;
  m_interpolating = 0;
  m_field194 = 0;
  m_field190 = 0;
  m_animBase.Id();
  m_blurTime = 0.0f;
  m_flags = 0;
  m_fov = 0.0f;
  m_attach = nullptr;
  m_attach2 = nullptr;
  m_field1A4 = 0;
  m_animRef = nullptr;
  m_eventTime = 0.0f;
  m_eventFrame = 0.0f;
  m_startFrame = 0.0f;
  m_cancelMode = 0;
  m_field23C = 0;
  m_effectFlags = 0;
  m_bloomTime = 0.0f;
  m_field234 = 1;
  m_resetBase = 1;
  m_anim.StopAllTracks();
  ForceDisableHud(false);
}

// 8001B038
void CameraDirector::RegisterCurrentCamera(ESimsCam* cam, bool force) {
  if ((cam && m_cam != cam) || force) {
    m_cam = cam;
    InitCurrentCamera();
  }
}

// 8001B078
// Remembers the game camera and the tuning the director will change (field of view range, near
// planes, tilt), and pulls the near planes in for shots.
void CameraDirector::InitSimsCamera() {
  EVec3 eye;
  m_cam->GetPos(eye, m_simsCamTarget, m_simsCamUp);
  m_simsCam.LookAtPos(eye, m_simsCamTarget, m_simsCamUp);
  m_cam->m_field564 = 0;
  m_cam->m_field568 = 0;
  m_savedFov = m_cam->GetFov() * 0.017453292f;
  SimsCameraParameters& p = *m_cam->m_params;
  m_savedFovMax = p.m_fovMax;
  m_savedFovMin = p.m_fovMin;
  m_savedNearZoomed = p.m_nearPlaneZoomed;
  m_savedNear = p.m_nearPlane;
  m_field18C = 0.5f;
  m_savedTilt = m_cam->m_tilt;
  p.m_nearPlaneZoomed = 0.6f;
  p.m_nearPlane = 0.5f;
}

// 8001B198
void CameraDirector::InitCurrentCamera() {
  InitSimsCamera();
  m_animBase.Id();
  m_resetBase = 1;
  m_eventFrame = 0.0f;
  m_animRef = nullptr;
  m_fov = 0.0f;
  m_attach = nullptr;
  m_attach2 = nullptr;
  m_startFrame = 0.0f;
  m_eventTime = 0.0f;
}

// 8001B34C
// While a shot plays, the camera's player's left stick is masked (a controller filter, priority 100).
void CameraDirector::DisableControls() {
  if (m_controllerFilter || !m_cam) return;
  if (!_pCtrlMan->GetPlayerMapped(m_cam->m_player)) return;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(m_cam->m_player)));
  if (!pad) return;
  m_controllerFilter = pad->AddFilter(0xf0000, const_cast<char*>("CameraDirector: mask LEFT stick control"), 100);
  m_filterPlayer = m_cam->m_player;
}

// 8001B3EC
void CameraDirector::EnableControls() {
  if (!m_controllerFilter || !m_cam) return;
  if (!_pCtrlMan->GetPlayerMapped(m_filterPlayer)) return;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(m_filterPlayer)));
  if (!pad) return;
  pad->RemoveFilter(m_controllerFilter);
  m_filterPlayer = 0;
  m_controllerFilter = 0;
}

// 8001CAF8
FrameEffectsManager* CameraDirector::GetFrameEffectsManager() { return m_frameEffects; }

// 8001CD20
// A shot the stick may cancel (and not disabled) is cancelled when the camera's player moves either
// stick by 0.2 (through the director's own filter).
int CameraDirector::CheckCancelled() {
  if (m_cancelMode != 1 || (m_flags & 1)) return 0;
  EController* pad = nullptr;
  if (_pCtrlMan->GetPlayerMapped(m_cam->m_player))
    pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(m_cam->m_player)));
  if (pad && (pad->StickMoved(0, 0.2f, m_controllerFilter) || pad->StickMoved(1, 0.2f, m_controllerFilter))) return 1;
  return 0;
}

// 8001D2DC
// Widens the camera's field of view range to include `radians` and sets it.
void CameraDirector::SetFOV(float radians) {
  const float deg = radians * 57.295776f;
  SimsCameraParameters& p = *m_cam->m_params;
  if (p.m_fovMax < deg) p.m_fovMax = deg;
  if (deg < p.m_fovMin) p.m_fovMin = deg;
  m_cam->SetFov(deg);
}

// 8001D33C
void CameraDirector::InitAnim(unsigned int character) { m_anim.Init(character, eAnimatedObjectType{2}, 0); }

// 8001D368
// Plays a camera animation on the registered camera (unless the director is disabled or cancelling):
// from the start frame, relative to the identity (or the attached matrices); the director then holds
// the last frame (or, with `releaseAfter`, the shot ends timed).
int CameraDirector::StartAnim(unsigned int anim, bool flag, bool releaseAfter) {
  if (!m_cam || m_cancelMode == 2 || (m_flags & 1)) return 0;
  if (!m_anim.NodeCount()) return 0;
  m_anim.StopAllTracks();
  m_animRef = nullptr;
  if (!m_anim.SetTrackAnim(eTrackFlags{1}, anim, -1, -1.0f, nullptr)) return 0;
  m_anim.SetTrackFrame(eTrackFlags{eTrackFlags::kMain}, m_startFrame, true);
  EVec3 eye, target, up;
  m_cam->GetPos(eye, target, up);
  m_animBase.LookAtPos(eye, target, up);
  if (m_resetBase) {
    m_animBase.Id();
    ApplyAttached(m_animBase, m_attach, m_attach2);
  }
  m_field1A4 = flag;
  m_state = kAnim;
  m_nextState = releaseAfter ? kHoldTimed : kHold;
  m_eventFrame = 0.0f;
  m_holdTime = 5.0f;
  m_eventTime = 0.0f;
  DisableControls();
  return 1;
}

// 8001D644
// Per frame: a cancelled shot interpolates back; an animation moves the camera until it completes
// (then holds, stopping the animation unless it holds its last frame); a hold keeps following the
// animation and switches to the queued state; a timed hold releases the camera when its time is up;
// cuts restore the camera from m_animBase; interpolations run Interp.
void CameraDirector::Update() {
  if (IsActive() && CheckCancelled() && m_state != kCancel) {
    m_cancelMode = 2;
    InterpToCancelCamera(5.0f, CamDirInterpType{}, false);
  }
  // The shot's base again from the attached matrices (when a shot resets it).
  auto resetBase = [this]() {
    m_animBase.Id();
    EMat4 t;
    if (m_attach) t.Mult4x4(m_animBase, *m_attach), m_animBase = t;
    if (m_attach2) t.Mult4x4(m_animBase, *m_attach2), m_animBase = t;
  };
  switch (m_state) {
    case kCut:
      if (m_cam) {
        EVec3 eye, target, up;
        EMat4 base = m_animBase;
        base.GetLookAtPos(eye, target, up);
        m_cam->SetPos(eye, target, up);
        SetFOV(m_fov);
        m_state = kHold;
        if (m_resetBase) resetBase();
      }
      break;
    case kCancel:
      if (m_cam) m_cam->SetActiveNoiseSetting(0);
      if (m_field23C) CalcCancelCam();
      [[fallthrough]];
    case kInterp:
      if (m_interpolating == 0) ReleaseCurrentCamera();
      else m_interpolating = Interp();
      break;
    case kInterpToGame:
      if (m_interpolating == 0) {
        m_state = kHold;
        if (m_resetBase) resetBase();
      } else {
        m_interpolating = Interp();
      }
      break;
    case kHold:
      if (m_anim.IsTrackValid(eTrackFlags{eTrackFlags::kMain})) UpdateCameraPosAndFOV();
      if (m_nextState) {
        m_state = m_nextState;
        m_nextState = kIdle;
      }
      break;
    case kHoldTimed:
      if (m_holdTime > 0.0f) {
        // GC: not while Globs::pSimulator (not ported) is paused.
        m_holdTime -= _dtlim15;
        if (m_holdTime < 0.0f) {
          ReleaseCurrentCamera();
          m_state = kIdle;
        }
      }
      break;
    case kAnim:
      UpdateCameraPosAndFOV();
      if (m_anim.IsTrackAnimComplete(eTrackFlags{eTrackFlags::kMain})) {
        m_state = kHold;
        if (m_anim.m_mainTrack >= 0 && m_anim.m_tracks[m_anim.m_mainTrack].m_endMode != 0) m_anim.StopAllTracks();
        m_animRef = nullptr;
        m_startFrame = 0.0f;
      }
      break;
    default:
      break;
  }
  if (m_state != kIdle) UpdateAnimNoteTrack();
}

// 8001DB94
// The note-track events, then a running bloom or blur interpolation (by simulated time).
void CameraDirector::UpdateAnimNoteTrack() {
  ProcessPropertyEventTags();
  if (m_effectFlags & 1) {
    const float dt = _dt * static_cast<float>(Globs_pSimulator ? Globs_pSimulator->GetSpeedMultiplier() : 1.0);
    if (0.0f < dt) {
      m_bloomTime += dt;
      BloomInterp();
    }
  }
  if (m_effectFlags & 2) {
    const float dt = _dt * static_cast<float>(Globs_pSimulator ? Globs_pSimulator->GetSpeedMultiplier() : 1.0);
    if (0.0f < dt) {
      m_blurTime += dt;
      BlurInterp();
    }
  }
}

// 8001E1FC
// The events of a shot started from an AnimRef: whole milliseconds of simulated time since the last
// frame (the fraction carried), from the event clock on.
void CameraDirector::ProcessPropertyEventTags() {
  if (!m_animRef) return;
  const double speed = Globs_pSimulator ? Globs_pSimulator->GetSpeedMultiplier() : 1.0;
  const float ms = static_cast<float>(static_cast<double>(_dt * 1000.0f) * speed + static_cast<double>(m_eventTime));
  const int whole = static_cast<int>(ms);
  m_eventTime = ms - static_cast<float>(whole);
  if (whole != 0) {
    ProcessAnimEvents(m_animRef, static_cast<int>(m_eventFrame), whole);
    m_eventFrame = static_cast<float>(static_cast<double>(m_eventFrame) + static_cast<double>(whole));
  }
}

// 8001E2E0
// Each note of the animation from `from` for `length` ms (length < 0: to the end) goes to the handler;
// returns the earliest handled time (from + length when none).
RET_UNKNOWN CameraDirector::ProcessAnimEvents(AnimRef* ref, int from, int length) {
  auto* anim = static_cast<ERAnim*>(_animman.GetRef(AnimRefId(ref)));
  std::vector<EAnimNote> notes;
  if (anim) notes = anim->m_notes;
  const int end = from + length;
  int first = end;
  for (int i = 0; i < static_cast<int>(notes.size()); ++i) {
    const int t = static_cast<int>(notes[i].m_time);
    if (from <= t && (length < 0 || t < end) && AnimEventHandler(ref, notes[i], i) && t < first)
      first = t;
  }
  return first;
}

// 8001E3E0
// Note kinds: 5 a sound (no source), 6 a number (101-103: DAT_80476230 = -1, 104-105: a loading
// screen starts), 0xD-0xF frame effects for the camera's player (HandleEffectNote).
RET_UNKNOWN CameraDirector::AnimEventHandler(AnimRef*, EAnimNote& note, int) {
  const int kind = static_cast<int>(note.m_field04);
  if (kind < 0xd) {
    if (kind == 5) {
      if (Globs_pSound) Globs_pSound->PlayBySource(const_cast<char*>(note.m_text.c_str()), 0);
    } else if (kind == 6) {
      const int n = std::atoi(note.m_text.c_str());
      if (100 < n) {
        if (n < 0x68) SIMMY_STUB("CameraDirector::AnimEventHandler: event 101-103 (DAT_80476230 = -1)", 0x8001E3E0);
        else if (n < 0x6a) LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{1});
      }
    }
  } else if (kind <= 0xf) {
    anon::HandleEffectNote(m_cam, note, anim__PropKind{kind});
  }
  return 0;
}

// 8001E4C0
// From the camera bloom now to the element's over 2 s of simulated time.
void CameraDirector::BeginCameraBloomInterp(CameraBloomDataElement* element) {
  _pGfx->GetCameraBloom(m_bloomFrom[0], m_bloomFrom[1], m_bloomFrom[2], m_bloomFrom[3]);
  std::memcpy(m_bloomTo, element, sizeof(m_bloomTo));
  m_bloomTime = 0.0f;
  m_effectFlags |= 1;
}

// 8001E540
void CameraDirector::BloomInterp() {
  float k = m_bloomTime * 0.5f;
  if (1.0f <= k) {
    m_effectFlags &= ~1u;
    k = 1.0f;
  }
  const float j = 1.0f - k;
  _pGfx->SetCameraBloom(j * m_bloomFrom[0] + k * m_bloomTo[0], j * m_bloomFrom[1] + k * m_bloomTo[1],
                        j * m_bloomFrom[2] + k * m_bloomTo[2], j * m_bloomFrom[3] + k * m_bloomTo[3]);
}

// 8001E5F0
void CameraDirector::BlurInterp() {
  float k = m_blurTime * 0.5f;
  if (1.0f <= k) {
    m_effectFlags &= ~2u;
    k = 1.0f;
  }
  const float j = 1.0f - k;
  _pGfx->SetMotionBlur(j * m_blurFrom[0] + k * m_blurTo[0], j * m_blurFrom[1] + k * m_blurTo[1],
                       j * m_blurFrom[2] + k * m_blurTo[2], j * m_blurFrom[3] + k * m_blurTo[3]);
}

// 8001DC68
// Samples the camera animation relative to m_animBase: node 0's matrix is the camera (eye, target,
// up; optionally transformed by m_transformA/B) and node 0's third scalar channel the field of view
// (45 degrees when zero). Without a camera the animation is stopped.
void CameraDirector::UpdateCameraPosAndFOV() {
  if (!m_cam) {
    if (m_anim.IsTrackValid(eTrackFlags{eTrackFlags::kMain})) m_anim.StopAllTracks();
    return;
  }
  // In live mode the shot runs at the simulator's speed.
  if (_gd.m_field90 && Globs_pSimulator) {
    m_anim.m_speed = static_cast<float>(Globs_pSimulator->GetSpeedMultiplier());
  } else {
    m_anim.m_speed = 1.0f;
  }
  EMat4 base = m_animBase;
  m_anim.Update(nullptr, nullptr, EVec3{1.0f, 1.0f, 1.0f});
  m_anim.Compute(base, _framecount);
  if (m_anim.m_matrices) {
    EVec3 eye, target, up;
    m_anim.m_matrices[0].GetLookAtPos(eye, target, up);
    if (m_transform) {
      // A mirrored shot (SetCameraCutAnim): each of eye, target and up into the base's space, x
      // negated, and back.
      auto mirror = [this](EVec3& v, float w) {
        EVec4 in{v.x, v.y, v.z, w};
        EVec4 local = in * m_transformA;
        local.x = -local.x;
        EVec4 out = local * m_transformB;
        v = EVec3{out.x, out.y, out.z};
      };
      mirror(eye, 1.0f);
      mirror(target, 1.0f);
      mirror(up, 0.0f);
    }
    m_cam->SetPos(eye, target, up);
  }
  if (m_anim.m_states) {
    float fov = m_anim.m_states[0].m_scalar2;
    if (fov == 0.0f) fov = 0.7853982f;
    SetFOV(fov);
  }
}

// 8001B1FC
// Gives the camera back: restores the tuning, clears the shot, and turns off motion blur.
void CameraDirector::ReleaseCurrentCamera() {
  m_flags &= ~1u;
  EnableControls();
  if (m_cam) {
    SimsCameraParameters& p = *m_cam->m_params;
    p.m_fovMax = m_savedFovMax;
    p.m_fovMin = m_savedFovMin;
    p.m_nearPlaneZoomed = m_savedNearZoomed;
    p.m_nearPlane = m_savedNear;
    m_cam->SetActiveNoiseSetting(-1);
  }
  m_state = kIdle;
  m_nextState = kIdle;
  m_transform = 0;
  m_cam = nullptr;
  m_field194 = 0;
  m_field190 = 0;
  m_interpolating = 0;
  m_startFrame = 0.0f;
  m_animRef = nullptr;
  m_eventTime = 0.0f;
  m_eventFrame = 0.0f;
  m_resetBase = 1;
  m_animBase.Id();
  m_fov = 0.0f;
  m_attach = nullptr;
  m_attach2 = nullptr;
  _pGfx->SetCameraBloom(0.0f, 0.0f, 0.0f, 0.0f);
  _pGfx->SetMotionBlur(0.0f, 0.0f, 1.0f, 0.15f);
  m_filterPlayer = 0;
  m_effectFlags &= ~3u;
  ForceDisableHud(false);
}

// 8001DFC8
void CameraDirector::LetterBox(bool on) {
  if (m_flags & 1) return;
  m_field234 = on ? 1 : 0;
}

// 8001B478
// Shots are placed relative to `dummy` (GC: an ENDummy, its first point's matrix) times `mat` (none:
// the identity at +0x120); neither: not attached. The next shot resets its base from them.
void CameraDirector::AttachDummy(EMat4* dummy, EMat4* mat) {
  if (m_cancelMode == 2 || (m_flags & 1)) return;
  if (!dummy) {
    if (!mat) {
      m_attach2 = nullptr;
      m_attach = nullptr;
      return;
    }
    m_attach = &m_field120;
  } else {
    m_attach = dummy;
  }
  m_attach2 = mat ? mat : &m_field120;
  m_resetBase = 1;
  m_startFrame = 0.0f;
  m_animRef = nullptr;
  DisableControls();
}

// 8001B524
// A cut to `camera` (its matrix and field of view, relative to the attached matrices); none: the base is
// the identity at a 50 degree field of view and the director stays as it is.
void CameraDirector::SetCameraCut(ENCamera* camera) {
  if (m_cancelMode == 2 || (m_flags & 1)) return;
  m_resetBase = 0;
  m_startFrame = 0.0f;
  if (!camera) {
    m_animBase.Id();
    m_fov = 0.87266463f;
    return;
  }
  SIMMY_STUB("CameraDirector::SetCameraCut: an ENCamera's matrix (ENCamera +0x48)", 0x8001B524);
}

// 8001B7AC
// A cut to the first frame of camera animation `anim`, relative to the attached matrices (m_transformB,
// its inverse m_transformA). A shot on the other side of the base's x axis from the game camera's eye
// is mirrored across it (m_transform; UpdateCameraPosAndFOV mirrors the animation the same way). The
// animation then holds from that frame (m_startFrame).
void CameraDirector::SetCameraCutAnim(int anim) {
  if (!m_cam || m_cancelMode == 2 || (m_flags & 1)) return;
  if (!m_anim.NodeCount()) return;
  m_anim.StopAllTracks();
  m_animRef = nullptr;
  if (!m_anim.SetTrackAnim(eTrackFlags{1}, static_cast<unsigned int>(anim), -1, -1.0f, nullptr)) return;
  m_animBase.Id();
  if (m_attach) m_animBase = *m_attach;
  if (m_attach2) {
    EMat4 t;
    t.Mult4x4(m_animBase, *m_attach2);
    m_animBase = t;
  }
  m_transformB = m_animBase;
  m_transformA.Invert(m_transformB);
  EMat4 base = m_animBase;
  m_anim.Update(nullptr, nullptr, EVec3{1.0f, 1.0f, 1.0f});
  m_anim.Compute(base, _framecount);
  if (!m_anim.m_matrices) return;
  m_animBase = m_anim.m_matrices[0];
  EVec3 eye, target, up;
  m_cam->GetPos(eye, target, up);
  const float eyeX = eye.x * m_transformA.m[0][0] + eye.y * m_transformA.m[1][0] + eye.z * m_transformA.m[2][0] + m_transformA.m[3][0];
  EVec4 pos{m_animBase.m[3][0], m_animBase.m[3][1], m_animBase.m[3][2], m_animBase.m[3][3]};
  EVec4 posLocal = pos * m_transformA;
  const bool mirror = (0.0f < posLocal.x) != (0.0f < eyeX);
  if (mirror) {
    posLocal.x = -posLocal.x;
    EVec4 p = posLocal * m_transformB;
    m_animBase.m[3][0] = p.x, m_animBase.m[3][1] = p.y, m_animBase.m[3][2] = p.z;
    for (int row = 1; row <= 2; ++row) {
      EVec4 r{m_animBase.m[row][0], m_animBase.m[row][1], m_animBase.m[row][2], m_animBase.m[row][3]};
      EVec4 l = r * m_transformA;
      l.x = -l.x;
      EVec4 back = l * m_transformB;
      m_animBase.m[row][0] = back.x, m_animBase.m[row][1] = back.y, m_animBase.m[row][2] = back.z, m_animBase.m[row][3] = back.w;
    }
    const float* r1 = m_animBase.m[1];
    const float* r2 = m_animBase.m[2];
    const float x = r1[1] * r2[2] - r1[2] * r2[1];
    const float y = r1[2] * r2[0] - r1[0] * r2[2];
    const float z = r1[0] * r2[1] - r1[1] * r2[0];
    m_animBase.m[0][0] = x, m_animBase.m[0][1] = y, m_animBase.m[0][2] = z;
  }
  m_transform = mirror ? 1 : 0;
  if (m_anim.m_states) {
    m_fov = m_anim.m_states[0].m_scalar2;
    if (m_fov == 0.0f) m_fov = 0.7853982f;
  }
  m_startFrame = m_anim.GetTrackFrame(eTrackFlags{eTrackFlags::kMain});
  m_state = kCut;
  m_resetBase = 1;
  DisableControls();
}

// 8001BDB0
// An interpolation to `camera` over `time` (CamDirInterpType); none: the base is the identity at 50
// degrees and the director stays as it is.
void CameraDirector::SetCameraInterp(ENCamera* camera, float time, CamDirInterpType type) {
  if (m_cancelMode == 2 || (m_flags & 1)) return;
  m_resetBase = 0;
  m_startFrame = 0.0f;
  if (!camera) {
    m_animBase.Id();
    m_fov = 0.87266463f;
    return;
  }
  (void)time;
  (void)type;
  SIMMY_STUB("CameraDirector::SetCameraInterp: an ENCamera's matrix (ENCamera +0x48)", 0x8001BDB0);
}

// 8001C058
// "Interpolate" to a camera animation: the game camera eases to look at the attached point instead
// (the tuning's offset +0x40 through the attached matrices becomes its target, its zoom range the
// shot range +0x30/+0x34, the old one kept at +0x324/+0x328 for ReleaseToGameCamera), under motion
// blur; the director gives the camera to the game (kGame). The animation and times are not used; the
// out flag is never set.
void CameraDirector::SetCameraInterpAnim(int, float, CamDirInterpType, float, bool*) {
  if (!m_cam || m_cancelMode == 2 || (m_flags & 1)) return;
  if (!m_anim.NodeCount()) return;
  SimsCameraParameters& p = *m_cam->m_params;
  float zoomMax = p.m_zoomMax, zoomMin = p.m_zoomMin;
  std::memcpy(&m_field328, &zoomMax, 4);
  std::memcpy(&m_field324, &zoomMin, 4);
  p.m_zoomMin = p.m_field030;
  p.m_zoomMax = p.m_field034;
  m_cam->SetZoom(m_cam->m_zoom);
  EMat4 m;
  m.Id();
  if (m_attach) m = *m_attach;
  if (m_attach2) {
    EMat4 t;
    t.Mult4x4(m, *m_attach2);
    m = t;
  }
  const float ox = p.m_field040, oy = p.m_field044, oz = p.m_field048;
  m_cam->m_target = EVec3{oz * m.m[2][0] + ox * m.m[0][0] + oy * m.m[1][0] + m.m[3][0],
                          oz * m.m[2][1] + ox * m.m[0][1] + oy * m.m[1][1] + m.m[3][1],
                          oz * m.m[2][2] + ox * m.m[0][2] + oy * m.m[1][2] + m.m[3][2]};
  anon::TurnOnMotionBlur(GetFrameEffectsManager());
  m_cam->m_field564 = 1;
  m_state = kGame;
}

// 8001C310
// The interpolation starts from the camera as it is now (+0x40, its field of view +0x19C). Type 0:
// `time` is a speed, the length is the larger of the eye's move and the rotation's (quaternion
// distance), or the field of view's change, over it; type 1: `time` is the length.
void CameraDirector::SetUpInterpCamera(float time, CamDirInterpType type) {
  EVec3 eye, target, up;
  m_cam->GetPos(eye, target, up);
  m_field040.LookAtPos(eye, target, up);
  m_field040.Normalize();
  const float fov = m_fov;
  m_startFov = m_cam->GetFov() * 0.017453292f;
  if (type == 0) {
    EMat4 dst = m_animBase;
    EQuat a, b;
    a.FromMat4(m_field040);
    b.FromMat4(dst);
    const float dq = std::sqrt((a.w - b.w) * (a.w - b.w) + (a.z - b.z) * (a.z - b.z) + (a.x - b.x) * (a.x - b.x) +
                               (a.y - b.y) * (a.y - b.y));
    EVec3 d{m_field040.m[3][0] - dst.m[3][0], m_field040.m[3][1] - dst.m[3][1], m_field040.m[3][2] - dst.m[3][2]};
    float move = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (move <= dq) move = dq;
    m_field194 = move * (1.0f / time);
    const float fovLength = std::fabs(m_startFov - fov) * (1.0f / time);
    if (fovLength >= m_field194) m_field194 = fovLength;
  } else {
    m_field194 = time;
  }
  m_field190 = SecondsNow();
  m_interpolating = 1;
}

// 8001C830
void CameraDirector::SetHoldState(float time) {
  m_holdTime = time;
  m_state = kHoldTimed;
}

// 8001C840
// Unless disabled: back to the game camera by interpolation (CalcCancelCam works out where; with
// `again` it is worked out again each frame).
void CameraDirector::InterpToCancelCamera(float time, CamDirInterpType type, bool again) {
  if (!m_cam || (m_flags & 1)) return;
  CalcCancelCam();
  m_field23C = again ? 1 : 0;
  SetUpInterpCamera(time, type);
  m_startFrame = 0.0f;
  DisableControls();
}

namespace {
// The common part of CalcSimsCam / CalcCancelCam: the game camera reset onto the selected sim from the
// direction it had when the shot took it (m_simsCam, with the director's tuning restored), kept at the
// shot's eye; the result (the camera's look-at matrix) becomes the shot's base, and the camera goes back
// where the shot has it.
void CalcGameCameraView(CameraDirector* d, bool cancel) {
  ESimsCam* cam = d->m_cam;
  EVec3 eye0, target0, up0;
  cam->GetPos(eye0, target0, up0);
  const float fov = cam->GetFov() * 0.017453292f;
  EVec3 eye1, target1, up1;
  d->m_simsCam.GetLookAtPos(eye1, target1, up1);
  cam->SetPos(eye1, d->m_simsCamTarget, d->m_simsCamUp);
  SimsCameraParameters& p = *cam->m_params;
  p.m_fovMax = d->m_savedFovMax;
  p.m_fovMin = d->m_savedFovMin;
  p.m_nearPlaneZoomed = d->m_savedNearZoomed;
  p.m_nearPlane = d->m_savedNear;
  cam->m_field540 = 0;
  cam->ResetPos(3.4e38f);
  cam->m_field540 = 1;
  EVec3 eye2, target2, up2;
  cam->GetPos(eye2, target2, up2);
  cam->SetPosAndReset(eye0, target2, up2);
  cam->Update();
  if (cancel) {
    d->InitCurrentCamera();
    d->m_state = CameraDirector::kCancel;
  } else {
    d->InitSimsCamera();
  }
  cam->SetPos(eye0, target0, up0);
  d->SetFOV(fov);
  cam->Update();
  d->m_animBase = d->m_simsCam;
  d->m_fov = d->m_savedFov;
}
}  // namespace

// 8001C8D4
// Where the game camera will be once the shot lets go (the shot's base and field of view).
RET_UNKNOWN CameraDirector::CalcSimsCam() {
  const int state = m_state;
  m_state = kIdle;
  ESimsCam* cam = m_cam;
  cam->Reset();
  CalcGameCameraView(this, false);
  m_state = state;
  return 0;
}

// 8001CB00
// The same for a cancelled shot (its interpolation runs as kCancel; the transform is dropped).
void CameraDirector::CalcCancelCam() {
  m_state = kIdle;
  m_transform = 0;
  CalcGameCameraView(this, true);
}

// 8001CDF4
// One step of the interpolation from +0x40 to the base: the view turns (quaternion blend) about the
// point each camera looks at on the plane z = +0x18C, that point and the distance to it moving
// linearly, and the field of view with them. True while not there.
int CameraDirector::Interp() {
  const float t = (SecondsNow() - m_field190) / m_field194;
  if (!m_cam) return 0;
  EMat4 dst = m_animBase;
  dst.Normalize();
  EMat4 cur;
  int more = 0;
  if (1.0f <= t) {
    cur = dst;
  } else {
    // The focus points: along each camera's view (-row 2) from its eye (row 3) to the plane.
    const EVec3 eye0{m_field040.m[3][0], m_field040.m[3][1], m_field040.m[3][2]};
    const EVec3 fwd0{-m_field040.m[2][0], -m_field040.m[2][1], -m_field040.m[2][2]};
    float k0 = 1.0f;
    if (0.001f < std::fabs(fwd0.z)) k0 = (m_field18C - eye0.z) / fwd0.z;
    const EVec3 focus0{k0 * fwd0.x + eye0.x, k0 * fwd0.y + eye0.y, k0 * fwd0.z + eye0.z};
    const EVec3 eye1{dst.m[3][0], dst.m[3][1], dst.m[3][2]};
    const EVec3 fwd1{-dst.m[2][0], -dst.m[2][1], -dst.m[2][2]};
    const float k1 = std::fabs(fwd1.z) <= 0.001f ? 1.0f : (m_field18C - eye1.z) / fwd1.z;
    const EVec3 focus1{k1 * fwd1.x + eye1.x, k1 * fwd1.y + eye1.y, k1 * fwd1.z + eye1.z};
    more = 1;
    const EVec3 focus{focus0.x + (focus1.x - focus0.x) * t, focus0.y + (focus1.y - focus0.y) * t,
                      focus0.z + (focus1.z - focus0.z) * t};
    cur.BlendQuat(t, m_field040, dst);
    const float k = (k1 - k0) * t + k0;
    cur.m[3][0] = focus.x - k * -cur.m[2][0];
    cur.m[3][1] = focus.y - k * -cur.m[2][1];
    cur.m[3][2] = focus.z - k * -cur.m[2][2];
  }
  float fov = m_fov;
  if (t < 1.0f && m_startFov != fov) {
    more = 1;
    fov = (fov - m_startFov) * t + m_startFov;
  }
  EVec3 eye, target, up;
  cur.GetLookAtPos(eye, target, up);
  m_cam->SetPos(eye, target, up);
  SetFOV(fov);
  return more;
}

// 8001D5F8
bool CameraDirector::StartAnim(AnimRef* ref, bool flag, bool releaseAfter) {
  const int r = StartAnim(AnimRefId(ref), flag, releaseAfter);
  if (r) m_animRef = ref;
  return r != 0;
}

namespace {
// The common head of ReleaseToGameCamera / InterpToGameCamera: social-mode effects off, the zoom
// range a shot changed restored (+0x324 / +0x328), no letterbox; a shot that moved the game camera
// (+0x320) puts it back where it was (CalcSimsCam).
void ReturnToGameCamera(CameraDirector* d) {
  anon::TurnOffSocialModeEffects(d->GetFrameEffectsManager());
  float zoomMin, zoomMax;
  std::memcpy(&zoomMin, &d->m_field324, 4);
  std::memcpy(&zoomMax, &d->m_field328, 4);
  if (d->m_cam && zoomMin > 0.0f) {
    d->m_cam->m_params->m_zoomMax = zoomMax;
    d->m_cam->m_params->m_zoomMin = zoomMin;
    d->m_field324 = 0;
    d->m_field328 = 0;
  }
  d->LetterBox(false);
  if (d->m_field320 && d->m_cam) {
    // The game camera's own view is worked out (CalcSimsCam), then the camera is put back where the
    // shot has it, looking straight on (up +z) and zoomed in to the near distance, from where it eases
    // to the game camera.
    EVec3 eye, target, up;
    d->m_cam->GetPos(eye, target, up);
    up = EVec3{0.0f, 0.0f, 1.0f};
    d->CalcSimsCam();
    d->m_cam->SetPos(eye, target, up);
    d->m_cam->SetZoom(d->m_cam->m_params->m_zoomMin);
    d->m_cam->m_field568 = 1;
    d->m_field320 = 0;
  }
}
}  // namespace

// 8001C540
// Unless cancelling or disabled: back to the game camera at once.
RET_UNKNOWN CameraDirector::ReleaseToGameCamera() {
  if (m_cancelMode == 2 || (m_flags & 1)) return 0;
  ReturnToGameCamera(this);
  ReleaseCurrentCamera();
  return 1;
}

// 8001C69C
// Unless cancelling or disabled: back to the game camera, which eases there from the shot (its zoom
// set again) under motion blur; ESimsCam::InterpolateActualCameraParamsToTarget turns the blur off
// when it arrives.
RET_UNKNOWN CameraDirector::InterpToGameCamera(float, CamDirInterpType) {
  if (!m_cam || m_cancelMode == 2 || (m_flags & 1)) return 0;
  ReturnToGameCamera(this);
  m_cam->SetZoom(m_cam->m_zoom);
  anon::TurnOnMotionBlur(GetFrameEffectsManager());
  m_cam->m_field564 = 1;
  ReleaseCurrentCamera();
  m_state = kIdle;
  return 1;
}

// 8001DFE0
// While a shot plays with the letterbox on, black bars over the top and bottom fifths of the view.
void CameraDirector::Draw(ERC* rc) {
  if (!IsActive() || !m_field234) return;
  rc->Texture(nullptr, 0);
  rc->Texture(nullptr, 1);
  rc->DisableGeometryModes(0x20);
  rc->SetAlpha(1.0f);
  rc->SetRasterModes(0, 0);
  rc->ZTest(false, 2, 0, 0);
  EVec2 uv0{0.0f, 1.0f}, uv1{1.0f, 0.0f};
  EVec4 black{0.0f, 0.0f, 0.0f, 1.0f};
  EVec2 p0{0.0f, 0.0f}, p1{1.0f, 0.2f};
  rc->Rect(p0, p1, uv0, uv1, black, 0.0f);
  EVec2 p2{0.0f, 0.8f}, p3{1.0f, 1.0f};
  rc->Rect(p2, p3, uv0, uv1, black, 0.0f);
}

// 80015A50
// A shot's frame-effect note (bloom 0xD, motion blur 0xE, depth of field 0xF) for the camera's player
// (while it has a selected sim); in two-player games also for the other player when that player's sim
// is the social-mode target of this one's.
void anon::HandleEffectNote(ESimsCam* cam, EAnimNote& note, anim__PropKind kind) {
  if (!cam || cam->m_player < 0) return;
  const int player = cam->m_player;
  auto handle = [&](int p) {
    CameraDirector* d = _globals.CameraDirectorOf(p);
    FrameEffectsManager* fx = d ? d->GetFrameEffectsManager() : nullptr;
    if (kind == 0xd) HandleBloomEvent(note, fx);
    else if (kind == 0xe) HandleMotionBlurEvent(note, fx);
    else if (kind == 0xf) HandleDepthOfFieldEvent(note, fx);
  };
  cXPerson* sim = _globals.GetSelectedPerson(player);
  if (sim) handle(player);
  if (_gd.m_fieldEC && sim) {
    const int other = (player + 1) % 2;
    auto* impl = dynamic_cast<cXPersonImpl*>(sim);
    auto* target = impl ? reinterpret_cast<cXPersonImpl*>(impl->GetSocialModeTarget()) : nullptr;
    if (target && static_cast<cXPerson*>(target) == _globals.GetSelectedPerson(other)) handle(other);
  }
}

// 8001E68C
void CameraDirector::ForceDisableHud(bool disable) { m_hudDisabled = disable && !_gd.m_fieldEC ? 1 : 0; }
// 8001E6B8
bool CameraDirector::IsForceDisableHud() { return m_hudDisabled != 0; }

// ---------------------------------------------------------------------------------------------------
// ESimsCam: the live-mode camera's controls

// 804FD974
int ESimsCam::m_modeDef = 1;

// 80016A8C
void ESimsCam::Init() {
  m_animObject2 = nullptr;
  m_prevMode = m_modeDef;
  m_fullScreen = 0;
  m_field3D4 = 0;
  m_snap = 0;
  m_animMode = 0;
  m_animObject = nullptr;
  m_mode = m_modeDef;
}

// 80016AB8
// The tuning's distance, field of view and near plane back to their defaults, then the position.
void ESimsCam::Reset() {
  m_fullScreen = 0;
  SimsCameraParameters defaults;
  m_params->m_fovMax = defaults.m_fovMax;
  m_params->m_fovMin = defaults.m_fovMin;
  m_params->m_nearPlaneZoomed = defaults.m_nearPlaneZoomed;
  m_params->m_nearPlane = defaults.m_nearPlane;
  m_params->m_zoomMin = defaults.m_zoomMin;
  m_params->m_zoomMax = defaults.m_zoomMax;
  ResetPos(3.4e38f);
}

// 80016B54
// The HUD's panel state sets the mode: states 0, 3 and 4 follow the sim in direct control (else the
// cursor), 8 the cursor.
void ESimsCam::SetState(int panelState) {
  if (panelState < 5) {
    if (2 < panelState || panelState == 0) SetMode(ESimsCam__CameraMode{IsUsingDirectControlInteractor(m_player) ? 1 : 0});
  } else if (panelState == 8) {
    SetMode(ESimsCam__CameraMode{0});
  }
}

// 80016BD4
// Not while the director runs a camera animation; 2 goes back to the previous mode.
void ESimsCam::SetMode(ESimsCam__CameraMode mode) {
  auto* director = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player));
  if (director && director->IsActive()) return;
  int m = mode;
  if (m == 2) m = m_prevMode;
  m_prevMode = m_mode;
  m_mode = m;
}

// 80016C34
// The cursor (+0x430) follows the player's interactor; when it leaves the area it may move in without
// moving the camera (+0x360, ValidateCursorPosition), the target (and the eased target with it) moves a
// share (s_newControlParms +0x7C) of the way it is outside.
void ESimsCam::FollowPlayerInteractor() {
  InteractorModule__InteractorInfo info;
  if (!GetPlayerInteractorInfo(m_player, info)) return;
  EVec3 cursor{info.m_x, info.m_y, 0.0f};
  if (!m_cursorLimit.IsInQuad(&cursor)) {
    EVec3 pinned = cursor;
    m_cursorLimit.PinToQuad(&pinned);
    const EVec3 d{cursor.x - pinned.x, cursor.y - pinned.y, cursor.z - pinned.z};
    if (0.0f < std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z)) {
      const float k = s_newControlParms.m_followRate;
      m_target.x += d.x * k;
      m_target.y += d.y * k;
      m_target.z += d.z * k;
      m_actualTarget = m_target;
    }
  }
  m_field430 = cursor;
}

// 80017EEC
// Following the selected sim: with a smoothing camera (tuning +0x04) the target goes to the sim's
// pelvis (its animator's bone 2) unless, while the target is nearly still, the sim is within the dead
// zone; with the camera turned by nobody (+0x474) the angle follows the target from where the eye is,
// so the camera swings round behind the sim. Not while the director plays the game camera (8). GC:
// without smoothing the target is pulled along the interactor's line through a circle (+0x448 / 5,
// LineCircleIntersect), not ported.
void ESimsCam::FollowSimStiff() {
  cXPerson* person = _globals.GetSelectedPerson(m_player);
  InteractorModule__InteractorInfo info;
  if (!person || !GetPlayerInteractorInfo(m_player, info)) return;
  if (!m_params->m_field04) {
    SIMMY_STUB("ESimsCam::FollowSimStiff: the unsmoothed camera (LineCircleIntersect)", 0x80017EEC);
    return;
  }
  auto* director = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player));
  if (director && director->m_state == CameraDirector::kGame) return;
  float x = info.m_x, y = info.m_y;
  if (SAnimator2* anim = person->GetSAnimator()) {
    EVec3 pelvis;
    anim->GetBonePos(2, pelvis);
    x = pelvis.x, y = pelvis.y;
  }
  if (m_field470 < 0.1f) {
    EVec3 p{x, y, 0.0f};
    if (IsInDeadZone(p)) return;
  }
  m_target.y = y;
  m_target.x = x;
  if (m_field474) {
    EVec3 d{x - m_eye.x, m_target.y - m_eye.y, m_target.z - m_eye.z};
    float a = CalcZAxisTheta(d) * 57.29578f + 315.0f;
    if (a > 360.0f) a -= 360.0f;
    else if (a < 0.0f) a += 360.0f;
    m_rotAngle = a;
  }
}

// 800183F4
// Within the dead zone: closer to the target (in x, y) than the tuning's radius, which grows from
// +0x08 at the nearest zoom to +0x0C at the farthest (squared distances).
bool ESimsCam::IsInDeadZone(EVec3& p) const {
  const SimsCameraParameters& t = *m_params;
  float k = t.m_zoomMax != t.m_zoomMin ? (m_zoom - t.m_zoomMin) / (t.m_zoomMax - t.m_zoomMin) : 0.0f;
  k = k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k);
  const float zone = t.m_field008 + (t.m_field00C - t.m_field008) * k;
  const float dx = p.x - m_target.x, dy = p.y - m_target.y;
  return dx * dx + dy * dy < zone;
}

// 80016DB8
// The camera noise for the cursor (5 while the player's sticks are neutral, else 6) and for a routing
// sim (4); then, unless the director runs a camera animation (only the window is placed), following
// the cursor (mode 0) or the selected sim (1), turned and zoomed with the C stick, and the window placed.
void ESimsCam::Update() {
  m_params->Validate();
  SetActiveNoiseSetting(CursorNotActive() ? 5 : 6);
  if (cXPerson* person = _globals.GetSelectedPerson(m_player)) {
    SAnimator2* anim = person->GetSAnimator();
    cXObject* object = person->AsObject();
    const int data = object ? object->GetData(0x11) : 0;
    if (data && anim && anim->m_routing == 1) SetActiveNoiseSetting(4);
    else if (m_noiseSetting == 4) SetActiveNoiseSetting(-1);
  }
  auto* director = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player));
  if (director && director->IsActive()) {
    UpdateWin();
    return;
  }
  if (!director || director->m_state != CameraDirector::kGame) {
    if (m_mode == 1) {
      FollowSimStiff();
    } else if (m_mode == 0 && !PcMouseClassic(m_player)) {
      // (PC: not while the mouse leads the cursor: it points at what is on screen, so the camera stays.)
      FollowPlayerInteractor();
    }
  }
  if (!m_animMode && !m_animObject) {
    const int rotated = HandleRotation();
    const int zoomed = HandleZoom();
    const int mouse = PcKeyboardMouseCamera();  // PC
    m_moved = (rotated || zoomed || mouse) ? 1 : 0;
  } else {
    m_moved = 0;
  }
  UpdateWin();
}

// 80016FB8
void ESimsCam::ApplyPan(float degrees) {
  const float a = m_rotAngle + degrees;
  m_rotAngle = a;
  if (a > 360.0f) m_rotAngle = a - 360.0f;
  else if (a < 0.0f) m_rotAngle = a + 360.0f;
}

// While the camera turns or zooms in with the C stick and the cursor is off screen (but within 1.5 eye
// heights of the target), the target is pulled toward the cursor by `rate` x the step (GC: the part of
// HandleRotation / HandleZoom after ApplyPan / ApplyZoom; only with the snapping motion system and not
// while the director plays the game camera).
void ESimsCam::PullTargetToCursor(float rate) {
  EVec3 cursor = GetCursorPos();
  if (!PointOffScreen(cursor)) return;
  auto* director = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player));
  if (director && director->m_state == CameraDirector::kGame) return;
  if (m_motion->GetMotionType() != 0) return;
  cursor = GetCursorPos();
  const EVec3 d{m_target.x - cursor.x, m_target.y - cursor.y, m_target.z - cursor.z};
  if (std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) / m_eye.z < 1.5f) {
    m_target.x -= rate * d.x;
    m_target.z -= rate * d.z;
    m_target.y -= rate * d.y;
  }
}

// 8001709C
// The C stick's x turns the camera (squared response, faster past the tuning's threshold; inverted
// by the options), pulling the target toward an off-screen cursor by 1% of the step.
int ESimsCam::HandleRotation() {
  const float stick = ReadControllerRotation();
  const float response = stick * (stick < 0.0f ? -stick : stick);
  float speed = m_params->m_field074;
  if (m_params->m_field07C * m_params->m_field07C < std::fabs(response)) speed = m_params->m_field078;
  // Inverted by the options: OptionsRecon +0x24 in direct control (and social mode), +0x18 in the classic.
  if (const auto* o = static_cast<const OptionsRecon*>(_globals.m_optionsRecon)) {
    InteractorModule__InteractorInfo info;  // (the build/buy manipulators count as not direct)
    const bool direct = (PlayerInteractorType(m_player) == 0 && !PlaceManipulatorInfo(m_player, info)) || PlayerInSocialMode(m_player);
    if (direct ? o->m_opt24 : o->m_opt18) speed = -speed;
  }
  const float delta = response * _dt * speed;
  if (delta == 0.0f) {
    m_field474 = 1;
    return 0;
  }
  m_field474 = 0;
  ApplyPan(delta);
  PullTargetToCursor(std::fabs(delta) * 0.01f);
  return 1;
}

// 800172E8
// The C stick's y zooms; zooming in pulls the target toward an off-screen cursor by 10% (delta_8199) of
// the step.
int ESimsCam::HandleZoom() {
  const float delta = ReadControllerZoom() * m_params->m_field088;
  if (delta == 0.0f) return 0;
  ApplyZoom(delta);
  if (0.0f < delta) PullTargetToCursor(delta * 0.1f);
  return 1;
}

// PC keyboard and mouse (pc_kbm.cpp, the frame's simmy::KbmTick): for the player whose controller is
// port 1, while the C stick reaches the camera (no menu has it), the mouse's turn (degrees) and zoom (tenths
// of the zoom range) go through ApplyPan / ApplyZoom as the C stick's do; while the mouse leads the classic
// cursor, the pan keys and screen edges move the camera's target over the lot at the cursor's speed.
int ESimsCam::PcKeyboardMouseCamera() {
  if (m_player != KbmPlayer() || !m_params) return 0;
  const simmy::KbmTick& t = simmy::kbm_tick();
  if (t.rot == 0.0f && t.zoom == 0.0f && t.panX == 0.0f && t.panY == 0.0f) return 0;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(m_player)));
  if (!pad || pad->IsStickFiltered(1, 0, m_filterId)) return 0;
  int moved = 0;
  if (t.rot != 0.0f) {
    m_field474 = 0;  // turned by the player (HandleRotation)
    ApplyPan(t.rot);
    moved = 1;
  }
  if (t.zoom != 0.0f) {
    ApplyZoom(t.zoom * 0.1f * (m_params->m_zoomMax - m_params->m_zoomMin));
    moved = 1;
  }
  if ((t.panX != 0.0f || t.panY != 0.0f) && PcMouseClassic(m_player)) {
    EVec3 p{m_target.x, m_target.y, 0.0f};
    EVec2 stick{t.panX, t.panY};
    ibuild::MoveCursorAlongCameraTargetVector(static_cast<float>(GetCursorSpeed()), p, stick, this);
    if (cFixedWorldImpl* w = Globs_pFixedWorld) {
      p.x = std::min(std::max(p.x, 1.0f), static_cast<float>(w->m_ySize) - 1.0f);
      p.y = std::min(std::max(p.y, 1.0f), static_cast<float>(w->m_xSize) - 1.0f);
    }
    m_target.x = p.x, m_target.y = p.y;
    moved = 1;
  }
  return moved;
}

// 80017A90
double ESimsCam::CalcTiltFromZoom(float zoom) {
  const SimsCameraParameters& p = *m_params;
  return (zoom - p.m_zoomMin) / (p.m_zoomMax - p.m_zoomMin) * (p.m_field03C - p.m_field038) + p.m_field038;
}

// 80017ABC
// The eye: from the target, `zoom` away along the tuning's reference direction tilted by `tilt` and
// turned by `rotAngle` about z.
void ESimsCam::CalcEyePosition(EVec3& eye, ESimsCam__CameraParameters& p) {
  const SimsCameraParameters& t = *m_params;
  const EVec3 d{t.m_refTo.x - t.m_refFrom.x, t.m_refTo.y - t.m_refFrom.y, t.m_refTo.z - t.m_refFrom.z};
  EVec3 axis{-d.y, d.x, 0.0f};
  if (axis.x != 0.0f || axis.y != 0.0f) {
    const float l = std::sqrt(axis.x * axis.x + axis.y * axis.y);
    axis.x /= l, axis.y /= l;
  }
  EMat4 m;
  m.Rotate(axis, p.m_tilt * 0.017453292f);
  m.PostRotateZ(p.m_rotAngle * 0.017453292f);
  const EVec3 v{-d.x, -d.y, 0.0f};
  EVec3 r{v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0] + t.m_refFrom.x + d.x,
          v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1] + t.m_refFrom.y + d.y,
          v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2] + t.m_refFrom.z + d.z};
  EVec3 dir{r.x - t.m_refTo.x, r.y - t.m_refTo.y, r.z - t.m_refTo.z};
  const float l = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  if (l != 0.0f) dir.x /= l, dir.y /= l, dir.z /= l;
  eye = EVec3{p.m_target.x + dir.x * p.m_zoom, p.m_target.y + dir.y * p.m_zoom, p.m_target.z + dir.z * p.m_zoom};
}

// 80017DC0
// The tuning's start position (target, up, angles) unless `rotAngle` is given, the start distance,
// the cursor on the selected sim, the window, then centred on the sim.
void ESimsCam::ResetPos(float rotAngle) {
  m_snap = 1;
  if (rotAngle == 3.4e38f) {
    m_target = m_params->m_refTo;
    m_up = EVec3{m_params->m_field0D4, m_params->m_field0D8, m_params->m_field0DC};
    m_tilt = m_params->m_field0B4;
    m_rotAngle = m_params->m_field0B0;
  } else {
    m_tilt = m_params->m_field0B4;
    m_rotAngle = rotAngle;
  }
  SetZoom(m_params->m_field0B8);
  if (InteractorModule__Interactor* in = PcPlayerInteractor(m_player)) {
    EVec3 pos;
    InteractorModule::GetPersonPosition(&pos, _globals.GetSelectedPerson(m_player), true, false);
    in->SnapToPos(pos);
  }
  UpdateWin();
  CenterOnSelectedSim();
}

// 80018824
// The camera onto the player's selected sim (its pelvis, bone 2; its tile when that is at the origin).
// With +0x540 clear (CalcSimsCam's reset) the target goes there only while the sim is on the lot (else
// the sim mode falls back), snapping when it is off screen. Otherwise, with no director shot and the
// live mode running: the cursor (+0x430) takes the interactor's place and, when outside the cursor's area
// (+0x360), the target is pulled a share (+0xA8) of the way (kept on the lot); without an interactor the
// target is pulled into the area around it (+0x390). During a shot or before live mode runs, the target
// is the sim. Then the window.
void ESimsCam::CenterOnSelectedSim() {
  cXPerson* person = _globals.GetSelectedPerson(m_player);
  if (!person) return;
  InteractorModule__InteractorInfo info;
  if (!GetPlayerInteractorInfo(m_player, info)) return;
  EVec3 pos{0.0f, 0.0f, 0.0f};
  if (SAnimator2* anim = person->GetSAnimator()) anim->GetBonePos(2, pos);
  pos.z = 0.0f;
  if (pos.x * pos.x + pos.y * pos.y + pos.z * pos.z < 0.5f) {
    if (cXObject* object = person->AsObject()) {
      const CTilePt tile = object->GetCTilePt();
      pos = EVec3{static_cast<float>(tile.GetY()), static_cast<float>(tile.GetX()), 0.0f};
    }
  }
  cFixedWorldImpl* world = Globs_pFixedWorld;
  const NewControlParms& c = s_newControlParms;
  if (!m_field540) {
    if (world && (pos.x < 1.0f || static_cast<float>(world->m_ySize - 1) < pos.x || pos.y < 1.0f ||
                  static_cast<float>(world->m_xSize - 1) < pos.y)) {
      if (m_mode == 1) m_mode = m_prevMode;
      return;
    }
    if (PointOffScreen(pos)) m_snap = 1;
    m_target = EVec3{pos.x, pos.y, m_params->m_targetHeight};
    UpdateWin();
    return;
  }
  auto* director = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player));
  if ((director && director->IsActive()) || !_gd.m_field90) {
    m_target = pos;
    UpdateWin();
    return;
  }
  bool noInteractor = true;
  if (c.m_followArea) {
    pos = EVec3{info.m_x, info.m_y, 0.0f};
    if (!m_cursorLimit.IsInQuad(&pos)) {
      EVec3 pinned = pos;
      if (m_cursorLimit.PinToQuad(&pinned)) {
        m_target.x += (pos.x - pinned.x) * c.m_centerPull;
        m_target.y += (pos.y - pinned.y) * c.m_centerPull;
        m_target.z += (pos.z - pinned.z) * c.m_centerPull;
        if (world) {
          const float maxX = static_cast<float>(world->m_ySize) - 1.0f, maxY = static_cast<float>(world->m_xSize) - 1.0f;
          m_target.x = m_target.x < 1.0f ? 1.0f : (maxX < m_target.x ? maxX : m_target.x);
          m_target.y = m_target.y < 1.0f ? 1.0f : (maxY < m_target.y ? maxY : m_target.y);
        }
      }
    }
    noInteractor = false;
    m_field430 = pos;
  }
  if (noInteractor && c.m_centerArea) {
    if (!m_cursorQuad.IsInQuad(&pos)) {
      EVec3 pinned = pos;
      m_cursorQuad.PinToQuad(&pinned);
      const EVec3 d{pos.x - pinned.x, pos.y - pinned.y, pos.z - pinned.z};
      if (0.0f < std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z)) {
        m_target.x += d.x * c.m_centerPull;
        m_target.y += d.y * c.m_centerPull;
        m_target.z += d.z * c.m_centerPull;
      }
    }
    m_field430 = pos;
  }
  UpdateWin();
}

// 80018E74
// Places the camera at `eye` looking at `target` from the tuning's start tilt and distance (the
// rotation from the direction), snapped.
void ESimsCam::SetPosAndReset(EVec3& eye, EVec3& target, EVec3& up) {
  m_eye = eye;
  EVec3 dir{target.x - eye.x, target.y - eye.y, target.z - eye.z};
  const float theta = CalcZAxisTheta(dir);
  EVec3 ref{m_params->m_refTo.x - m_params->m_refFrom.x, m_params->m_refTo.y - m_params->m_refFrom.y,
            m_params->m_refTo.z - m_params->m_refFrom.z};
  float angle = theta - CalcZAxisTheta(ref);
  if (angle < 0.0f) angle += 6.2831855f;
  m_tilt = m_params->m_field038;
  m_rotAngle = angle * 180.0f / 3.1415927f;
  m_zoom = m_params->m_field0B8;
  m_target = target;
  m_up = up;
  SnapActualCameraParamsToTarget();
  SetWinPos(m_win);
}

// 8001916C
// SetPos with the motion system snapping to it.
void ESimsCam::ForcePosition(EVec3& eye, EVec3& target, EVec3& up) {
  m_eye = eye;
  EVec3 dir{target.x - eye.x, target.y - eye.y, target.z - eye.z};
  const float pitch = CalcPitch(dir);
  const float theta = CalcZAxisTheta(dir);
  EVec3 ref{m_params->m_refTo.x - m_params->m_refFrom.x, m_params->m_refTo.y - m_params->m_refFrom.y,
            m_params->m_refTo.z - m_params->m_refFrom.z};
  float angle = theta - CalcZAxisTheta(ref);
  if (angle < 0.0f) angle += 6.2831855f;
  m_snap = 1;
  m_rotAngle = angle * 180.0f / 3.1415927f;
  m_tilt = pitch * 180.0f / 3.1415927f;
  m_zoom = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  m_target = target;
  m_up = up;
  SnapActualCameraParamsToTarget();
  SetWinPos(m_win);
}

// 800192FC
// The camera at once on `target`, from its current angles and distance.
void ESimsCam::ForceTarget(EVec3& target) {
  m_target = target;
  ESimsCam__CameraParameters p;
  p.m_target = m_target;
  p.m_zoom = m_zoom;
  p.m_rotAngle = m_rotAngle;
  p.m_tilt = m_tilt;
  EVec3 eye;
  CalcEyePosition(eye, p);
  ForcePosition(eye, target, m_up);
}

// 80019F6C
// The player's interactor's position at the tuning's target height (the origin without one).
EVec3 ESimsCam::GetCursorPos() {
  InteractorModule__InteractorInfo info;
  EVec3 p{0.0f, 0.0f, m_params->m_targetHeight};
  if (GetPlayerInteractorInfo(m_player, info)) p.x = info.m_x, p.y = info.m_y;
  return p;
}

// 80019364
// Outside the tuning's screen rectangle (+0xA0..+0xAC; all zero in the shipped tuning, so any point
// whose x or y is negative on screen).
int ESimsCam::PointOffScreen(EVec3& p) {
  const SimsCameraParameters& t = *m_params;
  return PointOutsideScreenRect(p, t.m_field0A0, t.m_field0A4, t.m_field0A8, t.m_field0AC);
}

// 80019398
// Bits: 1 left of, 2 right of, 4 below `top`, 8 above `bottom` (the window's input coordinates).
int ESimsCam::PointOutsideScreenRect(EVec3& p, float left, float top, float right, float bottom) {
  EVec2 s{0.0f, 0.0f};  // (GC: left as it was when the point is behind the eye)
  m_win.TransformToScreen(p, s);
  int bits = s.x < left ? 1 : 0;
  if (right < s.x) bits |= 2;
  if (s.y < top) bits |= 4;
  if (bottom < s.y) bits |= 8;
  return bits;
}

// 8001942C
// The ground under the screen trapezoid the classic cursor may move in without moving the camera
// (s_newControlParms +0x80: its top narrows with the zoom); its far corners (directions, as the top of
// the screen sees past the ground's edge) are put the zoom's depth beyond the near ones. With +0x2C
// clear the cursor would be pinned to it (the shipped value lets the camera follow instead).
void ESimsCam::ValidateCursorPosition(int, EVec3* pos) {
  NewControlParms& c = s_newControlParms;
  c.m_trap[1] = GetCursorLimitTopWidth();
  m_win.Get3DQuadFromScreenTrapAtZ(0.0f, c.m_trap, &m_cursorLimit);
  EVec3* q = m_cursorLimit.m_points;
  if (q[0].z < 0.0f && q[1].z < 0.0f) {
    q[0].z = 0.0f;
    q[1].z = 0.0f;
    float depth = GetCursorLimitDepth();
    q[0] = EVec3{q[3].x + depth * q[0].x, q[3].y + depth * q[0].y, q[3].z + depth * q[0].z};
    depth = GetCursorLimitDepth();
    q[1] = EVec3{q[2].x + depth * q[1].x, q[2].y + depth * q[1].y, q[2].z + depth * q[1].z};
  }
  if (!c.m_followArea && pos) m_cursorLimit.PinToQuad(pos);
}

// 8001A03C
float ESimsCam::GetCursorLimitTopWidth() {
  const float k = (m_zoom - m_params->m_zoomMin) / (m_params->m_zoomMax - m_params->m_zoomMin);
  return k * (s_newControlParms.m_topWidthFar - s_newControlParms.m_topWidthNear) + s_newControlParms.m_topWidthNear;
}

// 8001A074
float ESimsCam::GetCursorLimitDepth() {
  const float k = (m_zoom - m_params->m_zoomMin) / (m_params->m_zoomMax - m_params->m_zoomMin);
  return k * (s_newControlParms.m_depthFar - s_newControlParms.m_depthNear) + s_newControlParms.m_depthNear;
}

// 8001A2B8
// The player's interactor has both sticks neutral (InteractorModule::Interactor +0x44 / +0x48).
int ESimsCam::CursorNotActive() {
  InteractorModule__Interactor* in = PcPlayerInteractor(m_player);
  return in && (in->m_leftNeutral || in->m_rightNeutral) ? 1 : 0;
}

namespace {
// 80015834 InterpolateSmooth<float> (and 8039D65C InterpolateSmooth<EVec3>)
// `value` moves toward `target` by stiffness x dt of the distance (at most all of it); within 0.0001
// it arrives.
void SmoothTo(float* value, float& target, float stiffness, float dt) {
  const float d = target - *value;
  if (std::fabs(d) > 9.999999747378752e-05f) {
    float step = d * stiffness * dt;
    if (std::fabs(d) < std::fabs(step)) step = d;
    *value += step;
    return;
  }
  *value = target;
}
void SmoothTo(EVec3* value, EVec3& target, float stiffness, float dt) {
  const EVec3 d{target.x - value->x, target.y - value->y, target.z - value->z};
  const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  if (len <= 9.999999747378752e-05f) {
    *value = target;
    return;
  }
  float step = len * stiffness * dt;
  if (len < step) step = len;
  const float k = 1.0f / len;
  value->x += step * d.x * k;
  value->y += step * d.y * k;
  value->z += step * d.z * k;
}

// 8039D620
// a at x0 to b at x1 (held beyond).
float InterpolateLinear(float a, float b, float x0, float x1, float x) {
  if (x1 < x) return b;
  if (x0 <= x) return a + ((b - a) * (x - x0)) / (x1 - x0);
  return a;
}
}  // namespace

// 80015880
// The social-mode depth of field off (unless already off).
void anon::TurnOffSocialModeEffects(FrameEffectsManager* fx) {
  DepthOfField* dof = fx ? fx->DepthOfFieldObject() : nullptr;
  if (dof && dof->m_state != kFrameEffectOff) dof->Enable(false);
}

// 800158C4
// Motion blur on at once: colour offset 0.12, half the previous frame, no down-sampling.
void anon::TurnOnMotionBlur(FrameEffectsManager* fx) {
  MotionBlur* blur = fx ? fx->MotionBlurObject() : nullptr;
  if (!blur) return;
  blur->m_fadeInTime = 0.0f;
  blur->m_target.colorOffset = 0.12f;
  blur->m_target.downSampleLevel = 0;
  blur->m_target.previousFrameBlend = 0.5f;
  blur->Enable(true);
}

// 80015958
// Motion blur off over 0.3 s.
void anon::TurnOffMotionBlur(FrameEffectsManager* fx) {
  MotionBlur* blur = fx ? fx->MotionBlurObject() : nullptr;
  if (!blur) return;
  blur->m_fadeOutTime = 0.3f;
  blur->Enable(false);
}

// 800184A0
// The tuning's stiffnesses by the zoom (from the nearest, +0x28, to the farthest, +0x2C): translation
// +0x10..+0x14, rotation +0x18..+0x1C, zoom +0x20..+0x24.
float ESimsCam::CalcTranslationStiffness() const {
  const SimsCameraParameters& t = *m_params;
  return InterpolateLinear(t.m_field010, t.m_field014, t.m_zoomMin, t.m_zoomMax, m_zoom);
}
// 800184D8
float ESimsCam::CalcRotationStiffness() const {
  const SimsCameraParameters& t = *m_params;
  return InterpolateLinear(t.m_field018, t.m_field01C, t.m_zoomMin, t.m_zoomMax, m_zoom);
}
// 80018510
float ESimsCam::CalcZoomStiffness() const {
  const SimsCameraParameters& t = *m_params;
  return InterpolateLinear(t.m_field020, t.m_field024, t.m_zoomMin, t.m_zoomMax, m_zoom);
}

// 80018548
// Eased toward the target with the tuning's stiffnesses when it smooths (+0x04; else snapped): the
// target point, the zoom, the angle and the tilt (the angles the short way round, kept in 0..360).
// Once a director's interpolation back to the game camera (+0x564 / +0x568) has the zoom there, its
// motion blur goes off. The actual target's speed (+0x470, x and y) is kept for FollowSimStiff's dead
// zone.
void ESimsCam::InterpolateActualCameraParamsToTarget(float dt) {
  if (!m_field46C) {
    SnapActualCameraParamsToTarget();
    m_field46C = 1;
    m_field470 = 0.0f;
    return;
  }
  const float x0 = m_actualTarget.x, y0 = m_actualTarget.y;
  if (!m_params->m_field04) {
    SnapActualCameraParamsToTarget();
  } else {
    SmoothTo(&m_actualTarget, m_target, CalcTranslationStiffness(), dt);
    SmoothTo(&m_actualZoom, m_zoom, CalcZoomStiffness(), dt);
    auto turn = [&](float& actual, float& target) {
      if (actual - target > 180.0f) actual -= 360.0f;
      else if (target - actual > 180.0f) actual += 360.0f;
      SmoothTo(&actual, target, CalcRotationStiffness(), dt);
      if (actual > 360.0f) actual -= 360.0f;
      else if (actual < 0.0f) actual += 360.0f;
    };
    turn(m_actualRotAngle, m_rotAngle);
    turn(m_actualTilt, m_tilt);
  }
  if ((m_field564 || m_field568) && std::fabs(m_actualZoom - m_zoom) < 0.0001f) {
    m_field568 = 0;
    m_field564 = 0;
    auto* director = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player));
    anon::TurnOffMotionBlur(director ? director->GetFrameEffectsManager() : nullptr);
  }
  const float dx = m_actualTarget.x - x0, dy = m_actualTarget.y - y0;
  m_field470 = dt > 0.0f ? std::sqrt(dx * dx + dy * dy) / dt : 0.0f;
}

// 8001ADB8
float ESimsCam::ReadControllerZoom() {
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(m_player)));
  return pad ? static_cast<float>(pad->GetStick(1, 1, m_filterId)) : 0.0f;
}

// 8001AE0C
float ESimsCam::ReadControllerRotation() {
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(m_player)));
  return pad ? static_cast<float>(pad->GetStick(1, 0, m_filterId)) : 0.0f;
}
