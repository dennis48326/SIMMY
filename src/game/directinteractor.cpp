// Port of game/interactor.cpp and game/directinteractor.cpp (partial): the player interactor base and
// the direct-control interactor, which steers the selected sim with the left stick. Ported: the base's
// state, the direct interactor's start, update, control status and the free-move input to the sim's
// animator, and the object highlights (UpdateObjectHighlights, from the interaction influence map) that
// the A menu offers. Not ported: the input manager's sessions (PC: Update reads the pad's sticks itself,
// the commands OnCommandUpdate would get), cancelling queued actions, the plumb bob (PlumbBob), the idle timeout to autonomy (the input
// manager adds the idle time), social mode, and the interactor visualizer.
#include "simmy/types_gen.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"
#include "simmy/classes/influencemap.hpp"
#include "simmy/classes/collisionmap.hpp"
#include "simmy/classes/placement.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace {
const float kInvalidPos = -1000.0f;            // DirectInteractor::s_invalidPos (and the next two words)
const float kSelectionConeAngle = 0.34906584f;  // 804FFBF8 c_SelectionConeAngle (20 degrees)
const float kWalkCutoff = 0.9604f;              // 80500688 c_DirectInteractorWalkCutoff (0.98 squared)
// The 120 s without input before autonomy takes a player's sim (DirectInteractor::Update). PC
// development aid: SIMMY_AUTONOMY_IDLE=<seconds>.
float kAutonomyIdleTime() {
  static const float t = std::getenv("SIMMY_AUTONOMY_IDLE") ? static_cast<float>(std::atof(std::getenv("SIMMY_AUTONOMY_IDLE"))) : 120.0f;
  return t;
}
const float kStickNeutral = 0.0f;               // 80473560
const float kNoDir = SAnimator2::kNoDir;

float Wrap(float a) {
  while (3.1415927f < a) a -= 6.2831855f;
  while (a < -3.1415927f) a += 6.2831855f;
  return a;
}

SAnimator2* AnimatorOf(cXPerson* person) { return person ? person->GetSAnimator() : nullptr; }

// InteractorModule::IsCameraDirectorInControl: (GC 80213700) the player's director runs a camera of its
// own (a state other than idle and the game camera).
bool IsCameraDirectorInControl(int player) {
  auto* d = static_cast<CameraDirector*>(_globals.CameraDirectorOf(player));
  return d && d->m_state != CameraDirector::kIdle && d->m_state != CameraDirector::kGame;
}

// InteractorModule::IsPosOutsideWorld: (GC 8021109C) off the lot or on its edge row or column.
bool IsPosOutsideWorld(const EVec3& p) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w) return true;
  return p.x < 1.0f || static_cast<float>(w->m_ySize) - 1.0f < p.x || p.y < 1.0f ||
         static_cast<float>(w->m_xSize) - 1.0f < p.y;
}

// InteractorModule::GetCameraRelativeStickAngle (GC 80210804)
// The stick's direction turned into the world by the camera's heading (eye to target): pushed up is
// away from the camera. The angle is the animator's (0 along the tiles' +x, i.e. the world's +y).
float GetCameraRelativeStickAngle(EVec2& stick, ESimsCam* cam, float* stickAngle) {
  float hx = cam->m_target.x - cam->m_eye.x, hy = cam->m_target.y - cam->m_eye.y;
  if (hx != 0.0f || hy != 0.0f) {
    const float l = std::sqrt(hx * hx + hy * hy);
    hx /= l, hy /= l;
  }
  const float heading = std::atan2(hy, hx);
  float sx = stick.x, sy = stick.y;
  if (sx != 0.0f || sy != 0.0f) {
    const float l = std::sqrt(sx * sx + sy * sy);
    sx /= l, sy /= l;
  }
  const float a = std::atan2(-sy, -sx);
  if (stickAngle) *stickAngle = a;
  return Wrap(a + heading);
}

// PC: the interactor manager's per-player direct-control interactors.
InteractorModule__DirectInteractor* s_direct[2] = {};

cXPersonImpl* PersonOf(cXPerson* p) {
  cXObject* o = p ? p->AsObject() : nullptr;
  return o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
}

// cXObjectImpl / cXMTObjectImpl::GetAverageLocation (800E2A68 / 800C7884): its location; a multi-tile
// object's the mean of its parts'.
void AverageLocationOf(cXObjectImpl* o, int out[2]) {
  out[0] = o->m_location[0], out[1] = o->m_location[1];
  PlacementGroup* g = o->IsMultiTile() ? PlacementGroupOf(o) : nullptr;
  if (!g || g->parts.empty()) return;
  int s0 = 0, s1 = 0, n = 0;
  for (auto& part : g->parts) {
    s0 += part.object->m_location[0];
    s1 += part.object->m_location[1];
    ++n;
  }
  out[0] = s0 / n, out[1] = s1 / n;
}
}  // namespace

// 803070A8
// Whether `p` is right of the line from `a` toward `b`.
bool IsOnRightOf(EVec2 p, EVec2 a, EVec2 b) { return (p.y - a.y) * (b.x - a.x) - (p.x - a.x) * (b.y - a.y) < 0.0f; }

// 803070F0
// Whether the rectangle lo..hi touches the cone from `centre` along `dir` (`angle` each side): the centre
// inside it, or some corner in front of the centre right of the cone's left edge and some corner in
// front left of its right edge.
RET_UNKNOWN IsRectTouchingArc(EVec2 centre, EVec2 dir, float angle, EVec2 lo, EVec2 hi) {
  if (!(centre.x < lo.x || centre.y < lo.y || hi.x < centre.x || hi.y < centre.y)) return 1;
  const EVec2 corners[4] = {{lo.x, lo.y}, {hi.x, hi.y}, {lo.x, hi.y}, {hi.x, lo.y}};
  const float c = std::cos(angle), s = std::sin(angle);
  const EVec2 left{centre.x + (c * dir.x - s * dir.y), centre.y + (s * dir.x + c * dir.y)};
  const EVec2 right{centre.x + (c * dir.x - -s * dir.y), centre.y + (-s * dir.x + c * dir.y)};
  bool rightOfLeft = false, leftOfRight = false;
  for (const EVec2& p : corners) {
    if (!(0.0 < static_cast<double>(dir.x * (p.x - centre.x) + dir.y * (p.y - centre.y)))) continue;
    if (!rightOfLeft && IsOnRightOf(p, centre, left)) rightOfLeft = true;
    if (!leftOfRight && IsOnRightOf(p, right, centre)) leftOfRight = true;
  }
  return rightOfLeft && leftOfRight ? 1 : 0;
}

// ---------------------------------------------------------------------------------------------------
// InteractorModule::Interactor

// 802091BC
InteractorModule__Interactor::InteractorModule__Interactor() {}
// 802094B8
InteractorModule__Interactor::~InteractorModule__Interactor() {}
// 80209280
void InteractorModule__Interactor::Update(float) { m_prevPos = m_pos; }
// 802092A4
void InteractorModule__Interactor::AttachCamera(ESimsCam* cam) { m_cam = cam; }
// 802092AC
ESimsCam* InteractorModule__Interactor::GetAttachedCamera() { return m_cam; }
// 802092B4
void InteractorModule__Interactor::GetInteractorInfo(InteractorModule__InteractorInfo& info) {
  info.m_x = m_pos.x;
  info.m_y = m_pos.y;
  info.m_w = m_extents.x;
  info.m_h = m_extents.y;
}
// 802092D8
void InteractorModule__Interactor::SetExtents(EVec2& extents) { m_extents = extents; }
// 802093AC
void InteractorModule__Interactor::ResetInputState() {
  m_leftNeutral = 1;
  m_rightStick = EVec2{0.0f, 0.0f};
  m_leftStick = EVec2{0.0f, 0.0f};
  m_rightNeutral = 1;
}
// 802094F0
void InteractorModule__Interactor::AddIdleTime(float dt) { m_idleTime += dt; }
// 80209500
void InteractorModule__Interactor::ResetIdleTime() { m_idleTime = 0.0f; }
// 80209524
EVec3& InteractorModule__Interactor::GetPos() { return m_pos; }
// 802095CC
int InteractorModule__Interactor::GetPlayerId() { return m_player; }

// ---------------------------------------------------------------------------------------------------
// InteractorModule::DirectInteractor

// 80203D70
InteractorModule__DirectInteractor::InteractorModule__DirectInteractor() { m_coneAngle = kSelectionConeAngle; }
// 80203DD0
InteractorModule__DirectInteractor::~InteractorModule__DirectInteractor() {}

// 80203E6C
// For `player`: the selected person leaves social mode (person data 0x28 cleared), the input session
// opens, the camera follows the sim, the interactor's position waits for the sim's, the sim stands.
// GC: also the player's plumb bob is shown over the sim.
void InteractorModule__DirectInteractor::OnStart(int player) {
  m_player = player;
  cXPerson* person = _globals.GetSelectedPerson(player);
  if (person) {
    if (auto* p = person->AsObject() ? dynamic_cast<cXPersonImpl*>(person->AsObject()->GetObjectImplementation()) : nullptr) {
      p->SetFirstPlayerInSocialMode(false);
      p->SetPersonData(0x28, 0);
    }
    SIMMY_STUB("DirectInteractor::OnStart: the input session, the plumb bob", 0x80203E6C);
  }
  SetupCamera();
  m_posInitialized = 0;
  m_pos = EVec3{kInvalidPos, kInvalidPos, kInvalidPos};
  m_person = person;
  ResetInputState();
  // The player's plumb bob, bouncing and spinning.
  m_plumbBob = static_cast<unsigned int>(player) < 2 ? PlumbBob::s_pPlumbBob[player] : nullptr;
  if (auto* bob = static_cast<PlumbBob*>(m_plumbBob))
    if (bob->m_model) bob->m_model->m_bounce = bob->m_model->m_spin = 1;
  ResetIdleTime();
  m_paused = 0;
  ClearCancelTimer();
  if (m_person) SendNeutralFreeMoveInput();
}

// 802040D4
// GC: the player's selected-person slot (DAT_80475E84) is cleared, the input session closes and the
// plumb bob goes back to its idle state.
void InteractorModule__DirectInteractor::OnStop() {
  _globals.m_usedSimInteractor[m_player & 1] = 0;  // EGlobal +0xB8: the player left direct control
  m_plumbBob = static_cast<unsigned int>(m_player) < 2 ? PlumbBob::s_pPlumbBob[m_player] : nullptr;
  if (m_plumbBob) static_cast<PlumbBob*>(m_plumbBob)->SetState(3);
}

// 8020415C
// Each stick is neutral when both its axes are within kStickNeutral.
void InteractorModule__DirectInteractor::ParseControls() {
  m_rightNeutral = 0;
  m_leftNeutral = 0;
  if (std::fabs(m_leftStick.x) < kStickNeutral && std::fabs(m_leftStick.y) < kStickNeutral) m_leftNeutral = 1;
  if (std::fabs(m_rightStick.x) < kStickNeutral && std::fabs(m_rightStick.y) < kStickNeutral) m_rightNeutral = 1;
}

// 802041CC
// Pushed all the way (past kWalkCutoff) runs (14), less walks (13), else standing (9).
int InteractorModule__DirectInteractor::GetDesiredAnimState(bool neutral, EVec2 stick) {
  const float m = stick.x * stick.x + stick.y * stick.y;
  if (!neutral && 0.0f < m) return kWalkCutoff < m ? SAnimator2::kFreeRun : SAnimator2::kFreeWalk;
  return SAnimator2::kFreeIdle;
}

// 80204220
// The interactor takes the person's location once it is on the lot. GC: the plumb bob's particles
// show the first time (with a tile check against the world, vtable 0x6C).
bool InteractorModule__DirectInteractor::InitPlayerPos() {
  cXPersonImpl* impl = static_cast<cXPersonImpl*>(m_person);
  if (!impl) return false;
  const EVec3 pos{impl->m_location[0] * 0.0625f, impl->m_location[1] * 0.0625f, 0.0f};
  const bool outside = IsPosOutsideWorld(pos);
  if (!outside) {
    m_posInitialized = 1;
    m_pos = pos;
    m_restorePos = 0;
  }
  return !outside;
}

// 8020450C
// 3 while the camera director has the camera (or there is none), 2 to put the interactor back, else
// 1 (steering) unless the sim is out of the free-move states (0).
void InteractorModule__DirectInteractor::UpdateControlStatus() {
  m_controlStatus = 1;
  if (!m_cam || IsCameraDirectorInControl(m_player)) {
    m_controlStatus = 3;
    return;
  }
  if (m_restorePos) {
    m_controlStatus = 2;
    return;
  }
  if (SAnimator2* anim = AnimatorOf(m_person))
    if (static_cast<unsigned int>(anim->m_animState - SAnimator2::kFreeIdle) > 5) m_controlStatus = 0;
}

// 802045C4
// The sim moves faster the further the camera is zoomed out.
void InteractorModule__DirectInteractor::SetVelocityModifiers() {
  if (SAnimator2* anim = AnimatorOf(m_person))
    if (m_cam) anim->m_zoomRatio = m_cam->GetCurZoomRatio();
}

// 80204620
// No stick: the sim's desired state is standing, without a direction (a turn under way more than a
// sixteenth of a turn from it forgets where it started).
void InteractorModule__DirectInteractor::SendNeutralFreeMoveInput() {
  SAnimator2* anim = AnimatorOf(m_person);
  if (!anim) return;
  if (anim->m_desiredDir != kNoDir && 0.3926991f < std::fabs(Wrap(kNoDir - anim->m_desiredDir)))
    anim->m_turnStartDir = kNoDir;
  anim->m_desiredDir = kNoDir;
  anim->m_desiredState = SAnimator2::kFreeIdle;
}

// 802046FC
// The stick to the sim's desired state and its direction relative to the camera.
void InteractorModule__DirectInteractor::InterpretFreeMoveInput() {
  SAnimator2* anim = AnimatorOf(m_person);
  if (!anim) return;
  const int state = GetDesiredAnimState(m_leftNeutral != 0, m_leftStick);
  const float dir = state == SAnimator2::kFreeIdle || !m_cam ? kNoDir : GetCameraRelativeStickAngle(m_leftStick, m_cam, nullptr);
  if (anim->m_desiredDir != kNoDir && 0.3926991f < std::fabs(Wrap(dir - anim->m_desiredDir)))
    anim->m_turnStartDir = kNoDir;
  anim->m_desiredState = state;
  anim->m_desiredDir = dir;
}

// 80204838
// Unless the simulator is paused: the controls, then (with a selected person whose position is known)
// by the control status the stick steers the sim or lets it stand, the interactor at the sim. Then,
// paused or not, the action menu A asked for (GC: while AllPlayersActiveInputInteractors; PC: always).
// First the social-mode entry checks: a selected sim waiting to join the other player's social mode, or
// put in social mode by its own Socialize, that runs no interaction any more leaves social mode.
// GC: also 120 s without input hands the sim to autonomy (desired state 10), cancelling queued actions
// (B held) and the plumb bob.
void InteractorModule__DirectInteractor::Update(float dt) {
  if (cXPerson* sel = _globals.GetSelectedPerson(m_player))  // DAT_80475E88
    if (auto* p = sel->AsObject() ? dynamic_cast<cXPersonImpl*>(sel->AsObject()->GetObjectImplementation()) : nullptr) {
      if (p->IsWaitingForSocialMode() == 1) p->CheckSecondPlayerForFailedSocialModeEntry();  // vtables 0x35C/0x37C
      if (p->IsFirstPlayerInSocialMode() == 1) p->CheckFirstPlayerForFailedSocialModeEntry();  // vtables 0x34C/0x374
    }
  ParseControls();
  InteractorModule__Interactor::Update(dt);
  m_person = _globals.GetSelectedPerson(m_player);
  if (!m_person) {
    ResetIdleTime();
    return;
  }
  // Free will off (global 0x1E) keeps the player's sim; else after 120 s without input the sim
  // plays an impatient idle (desired state 10), and autonomy takes it when that ends
  // (SAnimator2 -> cXPersonImpl::SetIdlePlayerSimAutonomous).
  if (!Globs_pSimulator || Globs_pSimulator->GetGlobal(0x1e) == 0) ResetIdleTime();
  if (m_autonomous == 0 && kAutonomyIdleTime() < m_idleTime) {
    if (SAnimator2* anim = AnimatorOf(m_person)) anim->m_desiredState = SAnimator2::kImpatient;
    m_controlStatus = 0;
  }
  if (!m_posInitialized && !InitPlayerPos()) return;
  if (m_plumbBob) static_cast<PlumbBob*>(m_plumbBob)->m_updates = 0;
  if (!IsSimulatorPaused()) {
    if (CheckCancelTimer()) {
      ClearCancelTimer();
      CancelQueuedActions();
    }
    if (!m_paused && !(kAutonomyIdleTime() < m_idleTime)) {
      UpdateControlStatus();
      SAnimator2* anim = AnimatorOf(m_person);
      switch (m_controlStatus) {
        case 1:
          if (anim) m_pos = anim->GetEngineFormatPos();
          InterpretFreeMoveInput();
          SetVelocityModifiers();
          break;
        case 0:
          if (anim) m_pos = anim->GetEngineFormatPos();
          SendNeutralFreeMoveInput();
          break;
        case 2:
          m_restorePos = 0;
          m_pos = m_savedPos;
          SendNeutralFreeMoveInput();
          break;
        default:
          break;
      }
    }
  }
  if (m_actionMenu) ExecuteActionMenu();
  if (m_plumbBob) UpdatePlumbBob();
}

// 80204BEC
// While the simulator's speed is 0 (paused, or a tick not finished) or a player's interactor is not
// taking input (EIObjectMan::AllPlayersActiveInputInteractors; PC: they always are). PC: also while the
// pause menu holds the game (H2DTarget::IncPause stops the simulator on GC).
bool InteractorModule__DirectInteractor::IsSimulatorPaused() {
  if (H2DTarget::m_nPauseRefCount != 0) return true;
  return Globs_pSimulator && Globs_pSimulator->GetSpeedMultiplier() == 0.0;
}

// 80204EDC
// GC: where the plumb bob is (over the sim); PC: the interactor's position (the plumb bob is not
// ported), the sim's.
void InteractorModule__DirectInteractor::GetInteractorInfo(InteractorModule__InteractorInfo& info) {
  info.m_x = m_pos.x;
  info.m_y = m_pos.y;
  info.m_w = m_extents.x;
  info.m_h = m_extents.y;
}

// 80204F24
void InteractorModule__DirectInteractor::ClearCancelTimer() {
  m_cancelTimerOn = 0;
  m_cancelTime = 0.0f;
}
// 80204FD8
// Half a second after StartCancelTimer (B held: not ported).
int InteractorModule__DirectInteractor::CheckCancelTimer() {
  if (m_cancelTimerOn != 1) return 0;
  m_cancelTime += _dt;
  return 0.5f < m_cancelTime ? 1 : 0;
}

// 80205748
void InteractorModule__DirectInteractor::OnCommandUpdate(InteractorModule__InteractorInputManager__InteractorCommand command,
                                                          float value) {
  switch (static_cast<int>(command)) {
    case 0x12: m_leftStick.x = value; break;
    case 0x13: m_leftStick.y = value; break;
    case 0x0b: m_rightStick.x = value; break;
    case 0x0c: m_rightStick.y = value; break;
    default: break;
  }
}

// 8020506C
// A (command 0) asks for the action menu, run by the next Update. The selected sim's queued actions
// are cancelled by command 4 (B; person vtable 0xCC). Commands 0, 0x11, 0xD and 0xE are ignored
// while the selected sim is in a social mode, its own or the other player's (vtables 0x334/0x354).
// 0xD/0xE switch to the next / previous sim of the family.
void InteractorModule__DirectInteractor::OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand command,
                                                           float) {
  const int cmd = static_cast<int>(command);
  if (cmd == 0 || cmd == 0x11 || cmd == 0xd || cmd == 0xe)
    if (cXPerson* sel = _globals.GetSelectedPerson(m_player))  // DAT_80475E88
      if (auto* p = sel->AsObject() ? dynamic_cast<cXPersonImpl*>(sel->AsObject()->GetObjectImplementation()) : nullptr)
        if (p->IsInSocialMode() || p->IsSecondPlayerInSocialMode()) return;
  switch (cmd) {
    case 0:
      m_actionMenu = 1;
      break;
    case 4:
      if (cXPerson* sel = _globals.GetSelectedPerson(m_player))
        if (auto* p = dynamic_cast<cXPersonImpl*>(sel->AsObject()->GetObjectImplementation()))
          std::printf("[sim] player %d cancels: %d\n", m_player + 1, p->CancelAllActions());
      break;
    case 0x11: {
      // Z: the classic (cursor) control, unless the camera director has the camera; the sim stands.
      CameraDirector* cd = _globals.CameraDirectorOf(m_player);  // DAT_80475EE4
      if (cd && cd->m_state != CameraDirector::kIdle && cd->m_state != CameraDirector::kGame) break;
      m_paused = 1;
      if (SAnimator2* anim = AnimatorOf(_globals.GetSelectedPerson(m_player))) anim->m_desiredState = SAnimator2::kFreeIdle;
      ChangeActiveInteractor(m_player, 1);
      if (HUDTarget* hud = _gd.m_hudTarget) hud->CloseMenu(m_player);
      std::printf("[ctl] player %d: classic control\n", m_player + 1);
      break;
    }
    case 0xd:
    case 0xe: {
      // Next (0xD) / previous (0xE) sim of the family: unless the camera director holds the camera
      // (it lets go when it may), with the HUD up. The sim left stands (its default idle unless the
      // simulator steers it), the interactor jumps to the new one (SnapToPos), and unless it joins a
      // social mode the half-second cancel timer starts (Update then cancels its queued actions).
      CameraDirector* cd = _globals.CameraDirectorOf(m_player);  // DAT_80475EE4
      if (cd && cd->m_state != CameraDirector::kIdle && cd->m_state != CameraDirector::kGame) {
        if (cd->m_cancelMode == 0) break;
        cd->ReleaseCurrentCamera();
      }
      if (!_gd.m_hudTarget) break;
      if (static_cast<int>(command) == 0xe) _globals.ReverseSelectedPerson(m_player);
      else _globals.AdvanceSelectedPerson(m_player);
      cXPerson* next = _globals.GetSelectedPerson(m_player);
      if (!next || next == m_person) break;
      if (m_person) {
        SAnimator2* anim = AnimatorOf(m_person);
        if (anim && !anim->IsSimulatorControlled()) anim->StartDefaultIdle();
        SendNeutralFreeMoveInput();
      }
      m_person = next;
      cXObjectImpl* impl = next->AsObject() ? next->AsObject()->GetObjectImplementation() : nullptr;
      EVec3 pos{impl ? static_cast<float>(impl->m_location[0]) * 0.0625f : 0.0f,
                impl ? static_cast<float>(impl->m_location[1]) * 0.0625f : 0.0f, 0.0f};
      SnapToPos(pos);
      if (InteractorModule::CheckNewSimInSocialMode(static_cast<unsigned int>(m_player))) ClearCancelTimer();
      else StartCancelTimer();
      std::printf("[ctl] player %d: %s sim\n", m_player + 1, static_cast<int>(command) == 0xe ? "previous" : "next");
      break;
    }
    default:
      break;
  }
}

// 80205788
void InteractorModule__DirectInteractor::ResetInputState() {
  InteractorModule__Interactor::ResetInputState();
  m_actionMenu = 0;
}

// 80205C04
// The player's camera, following the sim.
void InteractorModule__DirectInteractor::SetupCamera() {
  Player* players = _gd.m_players;
  m_cam = players ? players[m_player & 1].m_cam : nullptr;
  if (m_cam) m_cam->SetMode(ESimsCam__CameraMode{1});
}

// 80205FB4
// GC: also closes the player's HUD menu (HUDTarget::CloseMenu).
void InteractorModule__DirectInteractor::SetAutonomous() { m_autonomous = 1; }

// 80205FF4
// Input takes the sim back from autonomy (its actions cancelled, vtable 0xCC) and ends an impatient
// idle.
void InteractorModule__DirectInteractor::ResetIdleTime() {
  m_idleTime = 0.0f;
  if (m_autonomous != 0) {
    if (cXPerson* p = _globals.GetSelectedPerson(m_player))
      if (auto* impl = dynamic_cast<cXPersonImpl*>(p->AsObject()->GetObjectImplementation())) {
        impl->CancelAllActions();
        std::printf("[auto] player %d takes %s back\n", m_player + 1, impl->DebugName());
      }
    m_autonomous = 0;
  }
  if (SAnimator2* anim = AnimatorOf(_globals.GetSelectedPerson(m_player)))
    if (anim->m_animState == SAnimator2::kImpatient) anim->m_desiredState = SAnimator2::kFreeIdle;
}

// 80206118
int InteractorModule__DirectInteractor::GetControlStatus() { return m_controlStatus; }
// 8020612C InteractorModule::DirectInteractor::IsAutonomous (inline in the header)

// 80206134
bool InteractorModule__DirectInteractor::CameraDirectorPermitsDirectControl() {
  return m_cam && !IsCameraDirectorInControl(m_player);
}

// 802057C8
// A: the player's pie menu for the objects the interactor highlights (EIObjectMan::GetObjectsInRect, in
// the object manager's order; the one the sim holds first), or the refuse sound when the HUD's menu is
// not available (or the sim's idle state is 3) or nothing is highlighted.
void InteractorModule__DirectInteractor::ExecuteActionMenu() {
  HUDTarget* hud = _gd.m_hudTarget;
  cXPersonImpl* me = PersonOf(m_person);
  if (!hud || !hud->IsMenuAvailable(m_player) || (me && me->GetIdleState() == 3)) {
    UIAUDIO::Instance()->PlaySound(0x3804219f);  // GC: unless the camera director hides the HUD (+0x234)
    m_actionMenu = 0;
    return;
  }
  std::vector<cXObjectImpl*> objects;
  if (EHouse* house = _globals.m_house)  // DAT_80475E94
    if (house->m_objectMan) {
      std::vector<ISimInstance*> lit;
      house->m_objectMan->GetObjectsInRect(m_player, lit);
      for (ISimInstance* inst : lit)
        if (cXObjectImpl* o = inst->m_xob ? inst->m_xob->GetObjectImplementation() : nullptr) objects.push_back(o);
    }
  // The object the selected sim holds (object slot 0: carried trash, a newspaper, a plate) always heads
  // the list, highlighted or not (out of it first if it was there): the held object's own interactions
  // ("Throw Away", "Drop", ...) are offered wherever the sim stands.
  if (cXPerson* sel = _globals.GetSelectedPerson(m_player))  // DAT_80475E88
    if (cXObjectImpl* self = sel->AsObject() ? sel->AsObject()->GetObjectImplementation() : nullptr)
      if (cXObjectImpl* held = self->GetContainedObject(0)) {
        objects.erase(std::remove(objects.begin(), objects.end(), held), objects.end());
        objects.insert(objects.begin(), held);
      }
  if (objects.empty()) {
    UIAUDIO::Instance()->PlaySound(0x3804219f);
  } else if (FlashPiMenu* pi = _gd.m_players[m_player & 1].m_piMenu) {
    if (pi->CreateObjectMenuFromObjList(objects)) UIAUDIO::Instance()->PlaySound(0x374bdcea);
  }
  ResetInputState();
}

// 80204C5C
// Whether `object` lights up for this player: not the player's own sim (unless its motive 0xB is
// negative); its (average) location within `radius` of the interactor; its model's bounds touching the
// selection cone (c_SelectionConeAngle each side) along the sim's facing; and it has something for the
// player to do (ISimInstance::HasInteractions).
RET_UNKNOWN InteractorModule__DirectInteractor::ShouldHighlightObject(cXObject* object, float radius) {
  cXPerson* sel = _globals.GetSelectedPerson(m_player);  // DAT_80475E88
  if (!sel || !object) return 0;
  if (object == sel->AsObject() && sel->GetMotive(0xb) >= 0.0) return 0;
  cXObjectImpl* impl = object->GetObjectImplementation();
  if (!impl) return 0;
  int at[2];
  AverageLocationOf(impl, at);
  const float dx = static_cast<float>(at[0]) * 0.0625f - m_pos.x, dy = static_cast<float>(at[1]) * 0.0625f - m_pos.y,
              dz = 0.0f - m_pos.z;
  if (radius * radius <= dx * dx + dy * dy + dz * dz) return 0;
  ISimInstance* inst = impl->m_instance;
  SAnimator2* anim = sel->GetSAnimator();
  if (!inst || !anim) return 0;
  const float a = static_cast<float>(anim->GetRealDir() + 1.5707963705062866);
  EVec2 centre{m_pos.x, m_pos.y};
  EVec2 dir{std::cos(a), std::sin(a)};
  EVec2 lo{inst->m_bounds[0], inst->m_bounds[1]};
  EVec2 hi{inst->m_bounds[3], inst->m_bounds[4]};
  if (!IsRectTouchingArc(centre, dir, m_coneAngle, lo, hi)) return 0;
  return inst->HasInteractions(m_player);
}

// 80205C4C
// Every frame: the player's highlights off, then on for the objects listed at the sim's tile in the
// interaction influence map that pass ShouldHighlightObject within 5 tiles, and the people within 1.5
// tiles that pass it with no wall between the player's sim and them.
void InteractorModule__DirectInteractor::UpdateObjectHighlights(EIObjectMan* objects) {
  if (!objects) return;
  objects->TurnOffAllHighlights(static_cast<unsigned int>(m_player));
  InfluenceMap* map = Globs_pFixedWorld ? Globs_pFixedWorld->GetInfluenceMap() : nullptr;
  ObjectList* list = map ? map->GetObjectList(static_cast<int>(m_pos.y), static_cast<int>(m_pos.x)) : nullptr;
  if (!list) return;
  const unsigned int flag = m_player == 0 ? 1u : 8u;
  for (cXObject* object : list->m_objects) {
    if (!ShouldHighlightObject(object, 5.0f)) continue;
    cXObjectImpl* impl = object->GetObjectImplementation();
    if (ISimInstance* inst = impl ? impl->m_instance : nullptr) {
      inst->SetHighlight(flag, true);
      inst->PropigateFlagsToSubModels();
    }
  }
  // GC: the module's people list (GetNumPeople / GetPeople); PC: the people by id.
  cXPerson* sel = _globals.GetSelectedPerson(m_player);
  cXObjectImpl* selImpl = sel && sel->AsObject() ? sel->AsObject()->GetObjectImplementation() : nullptr;
  if (!Globs_pObjectModule || !selImpl) return;
  for (int id = 1; id <= Globs_pObjectModule->GetNumObjects(); ++id) {
    cXObject* x = Globs_pObjectModule->GetObjectFromID(static_cast<short>(id));
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    if (!o || o->GetType() != 2 || !dynamic_cast<cXPersonImpl*>(o)) continue;
    if (!ShouldHighlightObject(o, 1.5f)) continue;
    EVec2 to{static_cast<float>(o->m_location[0]) * 0.0625f, static_cast<float>(o->m_location[1]) * 0.0625f};
    EVec2 from{static_cast<float>(selImpl->m_location[0]) * 0.0625f, static_cast<float>(selImpl->m_location[1]) * 0.0625f};
    EVec2 hit;
    if (CollisionMap::DoesCollideEnvironment(from, to, 0.0625f, false, false, hit)) continue;
    if (ISimInstance* inst = o->m_instance) {
      inst->SetHighlight(flag, true);
      inst->PropigateFlagsToSubModels();
    }
  }
}

// 80206180
void InteractorModule__DirectInteractor::UpdateInteractorPos() {
  if (SAnimator2* anim = AnimatorOf(m_person)) m_pos = anim->GetEngineFormatPos();
}

// ---------------------------------------------------------------------------------------------------
// PC: the interactor manager's part

// PC development aid: the SIMMY_INTERACT_NEAR entries used (padgate.cpp skips one for a dropped press).
int g_pcInteractNearPresses = 0;

namespace {
// The object within two tiles of player `player`'s sim (nearest first) whose interaction it can do.
void InteractWithNearestObject(int player) {
  cXPerson* selected = _globals.GetSelectedPerson(player);
  cXObject* self = selected ? selected->AsObject() : nullptr;
  auto* person = self ? dynamic_cast<cXPersonImpl*>(self->GetObjectImplementation()) : nullptr;
  if (!person || !Globs_pObjectModule) return;
  float px = static_cast<float>(person->m_location[1]) / 16.0f, py = static_cast<float>(person->m_location[0]) / 16.0f;
  // SIMMY_INTERACT_NEAR=x,y: the objects near that tile instead (the sim walks there; story starts).
  // "x,y;x,y;...": the n-th A press takes the n-th place (the last one from then on); "#id" an object's.
  if (const char* at = std::getenv("SIMMY_INTERACT_NEAR")) {
    int& presses = g_pcInteractNearPresses;
    for (int i = 0; i < presses; ++i)
      if (const char* next = std::strchr(at, ';')) at = next + 1;
    ++presses;
    // "#id": where object (or person) id stands now.
    cXObject* target = at[0] == '#' ? Globs_pObjectModule->GetObjectFromID(std::atoi(at + 1)) : nullptr;
    if (at[0] == 'g') {  // "g<guid>": where the first object of that GUID stands
      const unsigned int guid = static_cast<unsigned int>(std::strtoul(at + 1, nullptr, 16));
      for (cXObjectImpl* o = Globs_pObjectModule->m_first; o && !target; o = o->m_nextObject)
        if (o->m_def && objdef::GUID(o->m_def) == guid) target = o;
    }
    if (cXObjectImpl* t = target ? target->GetObjectImplementation() : nullptr) {
      px = static_cast<float>(t->m_location[1]) / 16.0f, py = static_cast<float>(t->m_location[0]) / 16.0f;
    } else if (at[0] == '>') {
      // ">x,y": the sim is put on that tile first (a walk the script cannot steer, e.g. to the bin).
      std::sscanf(at + 1, "%f,%f", &px, &py);
      person->SetLocation(px + 0.5f, py + 0.5f, person->m_level ? person->m_level : 1, person->GetData(1) & 7);
      if (person->m_animator) person->m_animator->SnapToGrid();
      std::printf("[sim] player %d's sim put at %.2f, %.2f (SIMMY_INTERACT_NEAR)\n", player + 1, px + 0.5f, py + 0.5f);
    } else {
      std::sscanf(at, "%f,%f", &px, &py);
    }
  }
  std::vector<std::pair<float, cXObjectImpl*>> near;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) {
    if (o->GetType() == 2 || o->m_level != person->m_level) continue;
    const CTilePt t = o->GetCTilePt();
    const float dx = static_cast<float>(t.GetX()) + 0.5f - px, dy = static_cast<float>(t.GetY()) + 0.5f - py;
    const float d2 = dx * dx + dy * dy;
    if (d2 <= 4.0f) near.emplace_back(d2, o);
  }
  for (int id = 1; id <= Globs_pObjectModule->GetNumObjects(); ++id) {  // other people (by id)
    cXObject* x = Globs_pObjectModule->GetObjectFromID(id);
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    if (!o || o == person || o->GetType() != 2 || !dynamic_cast<cXPersonImpl*>(o)) continue;
    const float dx = static_cast<float>(o->m_location[1]) / 16.0f - px, dy = static_cast<float>(o->m_location[0]) / 16.0f - py;
    if (dx * dx + dy * dy <= 4.0f) near.emplace_back(dx * dx + dy * dy, o);
  }
  std::sort(near.begin(), near.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  // What the sim holds comes first (GC ExecuteActionMenu: its slot 0 object heads the menu's list).
  if (cXObjectImpl* held = person->GetContainedObject(0)) {
    near.erase(std::remove_if(near.begin(), near.end(), [held](const auto& n) { return n.second == held; }), near.end());
    near.insert(near.begin(), std::make_pair(0.0f, held));
  }
  std::printf("[sim] player %d presses A at %.2f, %.2f: %d objects near\n", player + 1, px, py, static_cast<int>(near.size()));
  for (const auto& n : near)
    if (PushInteraction(person, n.second)) return;
}
}  // namespace

bool PlayerInSocialMode(int player);  // socialmode.cpp
bool H2DRewardMomentActive();         // rmdtarget.cpp
bool PcModalDialogWaiting();          // objectsim.cpp

namespace {
void UpdateDirectInteractorsOf(int count, float dt);
}  // namespace

// The players' interactors (below), then, as InteractorManager::UpdateActivePlayerInteractors does after
// each active interactor's update, the camera's cursor area (ESimsCam::ValidateCursorPosition).
void UpdateDirectInteractors(int count, float dt) {
  UpdateDirectInteractorsOf(count, dt);
  for (int player = 0; player < count && player < 2; ++player) {
    ESimsCam* cam = _gd.m_players[player].m_cam;
    InteractorModule__InteractorInfo info;
    if (!cam || !GetPlayerInteractorInfo(player, info)) continue;
    EVec3 pos{info.m_x, info.m_y, 0.0f};
    cam->ValidateCursorPosition(player, &pos);
  }
}

namespace {
void UpdateDirectInteractorsOf(int count, float dt) {
  EController* pads[2] = {};
  for (int player = 0; player < count && player < 2; ++player) {
    if (UpdatePlaceManipulator(player, dt)) continue;  // the player's active interactor is the place manipulator
    if (UpdateSimInteractor(player, dt)) continue;     // ... or the classic control (siminteractor.cpp)
    InteractorModule__DirectInteractor*& d = s_direct[player];
    if (!d) {
      if (!_globals.GetSelectedPerson(player)) continue;
      d = new InteractorModule__DirectInteractor;
      d->OnStart(player);
    }
    // In social mode the player's interactor is the SocialModeInteractor (7, socialmode.cpp), which
    // takes no commands: the social menu has the pad. Its highlights are the base interactor's (all off).
    if (PlayerInSocialMode(player)) {
      if (EHouse* house = _globals.m_house)
        if (house->m_objectMan) house->m_objectMan->TurnOffAllHighlights(static_cast<unsigned int>(player));
      continue;
    }
    // The pad's left stick (and C stick) as the input manager's stick commands would give them.
    pads[player] = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(player)));
    if (EController* pad = pads[player]) {
      const unsigned int filter = d->m_cam ? d->m_cam->m_filterId : 0u;
      d->OnCommandUpdate(0x12, static_cast<float>(pad->GetStick(0, 0, filter)));
      d->OnCommandUpdate(0x13, static_cast<float>(pad->GetStick(0, 1, filter)));
      d->OnCommandUpdate(0x0b, static_cast<float>(pad->GetStick(1, 0, filter)));
      d->OnCommandUpdate(0x0c, static_cast<float>(pad->GetStick(1, 1, filter)));
    }
    // InteractorInputManager::InstanceData::Update (8020AAF0): any stick or button resets the idle
    // time, else it grows.
    if (EController* pad = pads[player]) {
      const unsigned int filter = d->m_cam ? d->m_cam->m_filterId : 0u;
      const bool moved = std::fabs(pad->GetStick(0, 0, filter)) > 0.1 || std::fabs(pad->GetStick(0, 1, filter)) > 0.1 ||
                         std::fabs(pad->GetStick(1, 0, filter)) > 0.1 || std::fabs(pad->GetStick(1, 1, filter)) > 0.1;
      if (moved || pad->GetBtnDownMask(filter) != 0) d->ResetIdleTime();
      else d->AddIdleTime(dt);
    }
    // A (controller command 5; the input manager's command 0) and B (controller command 6, the input
    // manager's EICMD_CANCEL = 4) as the input session delivers them, before the update (GC:
    // InteractorInputManager::InstanceData::Update -> OnCommandPressed). PC development aid: with
    // SIMMY_INTERACT_PICK the nearest object's named (or first) interaction starts at once instead.
    if (EController* pad = pads[player]) {
      const unsigned int filter = d->m_cam ? d->m_cam->m_filterId : 0u;
      // SIMMY_INTERACT_PICK_PLAYER=<n> (1 or 2): only that player's A presses pick; the other player's
      // A opens the action menu as usual (co-op tests).
      static const bool pickAny = std::getenv("SIMMY_INTERACT_PICK") != nullptr;
      static const int pickPlayer = std::getenv("SIMMY_INTERACT_PICK_PLAYER") ? std::atoi(std::getenv("SIMMY_INTERACT_PICK_PLAYER")) : 0;
      const bool pick = pickAny && (pickPlayer <= 0 || pickPlayer == player + 1);
      if (pad->GetCmdResult(5, pad->GetBtnPressedMask(filter), pad->GetBtnDownMask(filter), false)) {
        if (pick) {
          // A reward moment or a modal dialog has the pad.
          if (!H2DRewardMomentActive() && !PcModalDialogWaiting()) InteractWithNearestObject(player);
        }
        else d->OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand{0}, 1.0f);
      }
      if (pad->GetCmdResult(6, pad->GetBtnPressedMask(filter), pad->GetBtnDownMask(filter), false))
        d->OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand{4}, 1.0f);
      // Z (controller command 0x31) -> 0x11 (803FCA60).
      if (pad->GetCmdResult(0x31, pad->GetBtnPressedMask(filter), pad->GetBtnDownMask(filter), false)) {
        PcSetMouseClassic(player, false);
        d->OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand{0x11}, 1.0f);
      } else if (player == KbmPlayer() && (simmy::kbm_tick().flags & simmy::kKbmCursor) && d->m_cam &&
                 !pad->IsStickFiltered(0, 0, filter) && !IsCameraDirectorInControl(player)) {
        // PC keyboard and mouse (pc_kbm.cpp): the mouse moved over the 3D view takes the player to the
        // classic control, as Z does; the cursor then follows the mouse.
        PcSetMouseClassic(player, true);
        d->OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand{0x11}, 1.0f);
      }
      // Controller commands 0xE (Y) -> 0xD (next sim) and 0xD -> 0xE (previous sim) (803FCA60).
      if (pad->GetCmdResult(0xe, pad->GetBtnPressedMask(filter), pad->GetBtnDownMask(filter), false))
        d->OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand{0xd}, 1.0f);
      if (pad->GetCmdResult(0xd, pad->GetBtnPressedMask(filter), pad->GetBtnDownMask(filter), false))
        d->OnCommandPressed(InteractorModule__InteractorInputManager__InteractorCommand{0xe}, 1.0f);
    }
    d->Update(dt);
    // InteractorManager::UpdateActivePlayerInteractors: then the interactor's highlights (vtable 0x5C).
    if (EHouse* house = _globals.m_house)  // DAT_80475E94
      if (house->m_objectMan) {
        d->UpdateObjectHighlights(house->m_objectMan);
        // PC dev trace: SIMMY_HL_TRACE=1 prints the objects the player's direct control highlights every
        // 60 frames.
        static const bool hlTrace = std::getenv("SIMMY_HL_TRACE") != nullptr;
        if (hlTrace && _framecount % 60 == 0) {
          std::vector<ISimInstance*> lit;
          house->m_objectMan->GetObjectsInRect(player, lit);
          InfluenceMap* map = Globs_pFixedWorld ? Globs_pFixedWorld->GetInfluenceMap() : nullptr;
          ObjectList* l = map ? map->GetObjectList(static_cast<int>(d->m_pos.y), static_cast<int>(d->m_pos.x)) : nullptr;
          int total = 0;
          if (map)
            for (ObjectList& ol : map->m_lists) total += static_cast<int>(ol.m_objects.size());
          std::printf("[hl] f%d p%d at %.2f %.2f: %d listed (map %dx%d, %d in all), %d lit:", _framecount, player + 1, d->m_pos.x,
                      d->m_pos.y, l ? static_cast<int>(l->m_objects.size()) : -1, map ? map->m_xSize : 0, map ? map->m_ySize : 0, total,
                      static_cast<int>(lit.size()));
          for (ISimInstance* inst : lit)
            if (cXObjectImpl* o = inst->m_xob ? inst->m_xob->GetObjectImplementation() : nullptr) std::printf(" %s", o->DebugName());
          if (l) {
            std::printf(" | listed:");
            for (cXObject* x : l->m_objects)
              if (cXObjectImpl* o = x->GetObjectImplementation()) std::printf(" %s", o->DebugName());
          }
          if (cXPersonImpl* me = PersonOf(d->m_person))
            std::printf(" | sim loc %d %d dir %.2f", me->m_location[0], me->m_location[1],
                        me->GetSAnimator() ? me->GetSAnimator()->GetRealDir() : 0.0);
          std::printf("\n");
        }
      }
    // PC dev trace: SIMMY_COOP_TRACE=1 prints each player's sim, camera and stick every 60 frames.
    static const bool trace = std::getenv("SIMMY_COOP_TRACE") != nullptr;
    if (trace && _framecount % 60 == 0) {
      SAnimator2* a = AnimatorOf(_globals.GetSelectedPerson(player));
      EVec3 p = a ? a->GetEngineFormatPos() : EVec3{};
      ESimsCam* c = d->m_cam;
      std::printf("[coop] f%d p%d sim %.2f %.2f %.2f cam eye %.2f %.2f %.2f tgt %.2f %.2f %.2f stick %.2f %.2f\n",
                  _framecount, player, p.x, p.y, p.z, c ? c->m_eye.x : 0, c ? c->m_eye.y : 0, c ? c->m_eye.z : 0,
                  c ? c->m_target.x : 0, c ? c->m_target.y : 0, c ? c->m_target.z : 0, d->m_leftStick.x, d->m_leftStick.y);
    }
  }
}
}  // namespace

// PC stand-in for InteractorManager::GetPlayerInteractor(player, 0) != 0: the player's active
// interactor is direct control (not classic, social mode or a build/buy manipulator).
bool PlayerUsesDirectInteractor(int player) {
  if (player < 0 || player > 1 || !s_direct[player]) return false;
  InteractorModule__InteractorInfo info;
  return PlayerInteractorType(player) != 1 && !PlayerInSocialMode(player) && !PlaceManipulatorInfo(player, info);
}

InteractorModule__DirectInteractor* PlayerDirectInteractor(int player) {
  return PlayerInteractorType(player) == 1 ? nullptr : s_direct[player & 1];  // GetPlayerInteractor(player, 0)
}

InteractorModule__Interactor* PcPlayerInteractor(int player) {
  InteractorModule__InteractorInfo info;
  if (PlaceManipulatorInfo(player, info)) return nullptr;  // (the build/buy manipulators are PC structs)
  if (PlayerInteractorType(player) == 1) return PlayerSimInteractor(player);
  return s_direct[player & 1];
}

bool GetPlayerInteractorInfo(int player, InteractorModule__InteractorInfo& info) {
  if (PlaceManipulatorInfo(player, info)) return true;
  if (SimInteractorInfo(player, info)) return true;
  InteractorModule__DirectInteractor* d = s_direct[player & 1];
  if (!d) return false;
  d->GetInteractorInfo(info);
  return true;
}

void ShutdownDirectInteractors() {
  for (auto*& d : s_direct) {
    if (d) d->OnStop();
    delete d;
    d = nullptr;
  }
  ShutdownSimInteractors();
}
