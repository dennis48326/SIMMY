// Port of game/sanimator2.cpp (partial): a person's animator. Ported: set-up, the per-frame update,
// the anim-state choice, the free-move (direct control) states (idle, turn on the spot, walk, run,
// run stop) with their animations, cross-fades and movement, collision, posing the sim, the animate
// primitive (skills on track 0x10, their loops and 'xevt' animation events) and walking a route (PC
// follow modes, see BeginRoute). Not ported: the GC follow modes (sidesteps, portals), awareness,
// impatient idles, mood overlays, carrying and the fx track, props, costumes, particles, footsteps and
// sounds, the censor, low-motive idles and walks, NPC auto-run.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/roomresolve.hpp"

#include <cmath>
#include <cstdlib>

namespace {
const float kWalkRunBlendTime = 0.1f;      // 804FDAD8
const float kFastRemoveBlendTime = 0.1f;   // 804FDAFC
const float kOtherToTurnBlendTime = 0.1f;  // 804FDAEC
const float kTurnToTurnBlendTime = 0.2f;   // 804FDAE8
const float kDefaultBlendTime = 0.2f;      // 804FDAD0
const float kBlendToDefaultIdle = 0.5f;    // 804FDAD4
const float kTurnToIdleBlendTime = 0.3f;   // 804FDADC
const float kExitIdleOverlayBlend = 0.4f;  // 804FDACC
const float kCarryBlendTime = 0.6f;        // 804FDAF8
const float kPi = 3.1415927f;
const float kTwoPi = 6.2831855f;

float Wrap(float a) {
  while (kPi < a) a -= kTwoPi;
  while (a < -kPi) a += kTwoPi;
  return a;
}

cXPersonImpl* Impl(cXPerson* person) { return static_cast<cXPersonImpl*>(person); }
}  // namespace

// 8005BFD0
// Facing nowhere in particular, standing (free-move idle), the shuffle turns 0.7 s until their
// animations say otherwise.
SAnimator2::SAnimator2() {
  for (auto& row : m_turnTime)
    for (float& t : row) t = 0.7f;
}

// 8005C23C
// The props (removeAllProps) and particles go. GC: also drops a pending load, costumes and the optional
// animations.
SAnimator2::~SAnimator2() {
  removeAllProps();
  cleanupParticlesImmediate(&m_particles);
  cleanupParticlesImmediate(&m_overlayParticles);
}

// 8005C43C
// For `person`: its instance, the bones the camera and props follow (right hand 0x35, head 0x10,
// pelvis 2, torso 0xC; GC: other ones for pets).
int SAnimator2::Initialize(cXPerson* person) {
  m_person = person;
  m_flags &= ~1u;
  cXPersonImpl* impl = Impl(person);
  m_sim = impl ? static_cast<ESim*>(impl->m_instance) : nullptr;
  SIMMY_STUB("SAnimator2::Initialize: the person's destination list (GetDestList), AwarenessManager", 0x8005C43C);
  m_turnStartDir = kNoDir;
  m_pauseMult = 1.0f;
  m_field114 = 0;
  m_dir = 0.0f;
  m_timeMult = 1.0f;
  m_walkBlend = 0.0f;
  m_shuffleDir = 2;
  m_longIdleCount = 0;
  SetIdleInitialized(false);
  m_lowMotiveIdle = 0;
  m_lowMotive = 0;
  m_resetPos = 0;
  m_pendingAnim = 0;
  m_boneIndex[0] = 0x35;
  m_boneIndex[1] = 0x10;
  m_boneIndex[2] = 2;
  m_boneIndex[3] = 0xc;
  return 1;
}

float SAnimator2::ScaledDt() { return _dt * m_timeMult * m_pauseMult; }

unsigned int SAnimator2::MainTrackFlags() {
  EAnimController& anim = m_sim->m_anim;
  return anim.m_mainTrack == -1 ? 0u : anim.m_tracks[anim.m_mainTrack].m_flags;
}

EACTrack* SAnimator2::OutgoingTrack() {
  EAnimController& anim = m_sim->m_anim;
  if (anim.m_blendList.empty() || anim.m_blendList[0] == -1) return nullptr;
  return &anim.m_tracks[anim.m_blendList[0]];
}

// GetMovementVelocityFromStyle (GC 8006A69C)
float SAnimator2::StyleVelocity(int style) {
  switch (style) {
    case 2: return m_lowEnergyVel;
    case 3: return m_lowBladderVel;
    case 1: return m_runVel;
    case 7: case 5: return 24.0f;
    case 8: return 72.16f;
    case 4: case 6: return 0.0f;
    default: return m_walkVel;
  }
}

// 8005CC80
// The frame's speed (the simulator's; 0 while it is paused or stalled), the movement state, the main
// animation's events up to now (looping animations start over), then the animations; the sim posed
// where it stands. GC: also the overlay's events, particles, render models and the censor.
void SAnimator2::Update() {
  if (!m_sim || !m_person) return;
  cSimulatorImpl* sim = Globs_pSimulator;
  m_timeMult = 1.0f;
  if (sim) m_timeMult = (sim->IsStalled() || sim->IsPaused() || sim->IsStopped()) ? 0.0f : static_cast<float>(sim->GetSpeedMultiplier());
  m_field114 = 0;
  adjustAnimationPlayRates();
  m_prevTimeMult = m_timeMult;
  updateMovementState();
  checkParticleCleanup();
  if (m_curAnim) {
    EResource* res = _animman.GetRef(AnimRefId(m_curAnim));
    const int duration = res ? static_cast<ERAnim*>(res)->m_durationMs : 0;
    const float ms = m_eventFraction + _dt * 1000.0f * m_timeMult * m_pauseMult;
    const int whole = static_cast<int>(ms);
    m_eventFraction = ms - static_cast<float>(whole);
    if (whole != 0) {
      processEvents(m_curAnim, m_eventFrame, whole, (m_flags & 0x100) != 0, false);
      m_eventFrame += whole;
    }
    EAnimController& anim = m_sim->m_anim;
    const bool loops = anim.m_mainTrack >= 0 && anim.m_tracks[anim.m_mainTrack].m_endMode == 0;
    if (loops && duration < m_eventFrame) {
      const int over = m_eventFrame + whole - duration;
      m_eventFrame = 0;
      m_eventFraction = static_cast<float>(over);
      processEvents(m_curAnim, 0, over, (m_flags & 0x100) != 0, false);
    }
  }
  // The overlay's notes (its own particle tree); once its track (0x8000) is gone, its particles go.
  if (m_overlayAnim) {
    if (!m_sim->m_anim.IsTrackValid(eTrackFlags{0x8000})) {
      cleanupParticlesDelayed(&m_overlayParticles);
      m_overlayAnim = nullptr;
      m_flags &= ~0x20000u;
    } else {
      const int whole = static_cast<int>(_dt * 1000.0f * m_timeMult * m_pauseMult);
      if (whole != 0) {
        processEvents(m_overlayAnim, m_overlayEventFrame, whole, (m_flags & 0x200) != 0, true);
        m_overlayEventFrame += whole;
      }
    }
  }
  if (m_routing == 0) m_walkBlend = 0.0f;
  updateCarryAnimation();
  updateRenderAnimation();
  updateRenderModels();
  updateCensor();
  UpdateAnimationValidity();
}

// 8005D014
void SAnimator2::adjustAnimationPlayRates() {
  if (m_prevTimeMult != m_timeMult) m_sim->m_anim.SetAllTrackSpeed(m_timeMult);
}

// 8005CC2C
// Along a route, or (under player control) the free-move state; awareness follows (6..) are not
// ported.
void SAnimator2::updateMovementState() {
  if (m_followMode < 6) {
    if (m_routing == 1) moveAnimation();
    else if (m_flags & 1) updateFreeMoveState();
  } else {
    SIMMY_STUB("SAnimator2::awarenessMove", 0x8006067C);
  }
}

// 8005C8E8
// Without a desired direction: the run (or a walk still fading in from one) stops with the run-stop
// animation, then the state becomes the desired one. With one: standing (or stopping), it turns on
// the spot first (a shuffle, state 12); it turns toward the direction (GetTurnRate), and once there
// takes the desired state (walk or run), the direction used up. The person faces where it turned.
void SAnimator2::updateFreeMoveState() {
  const float dt = ScaledDt();
  if (m_desiredDir == kNoDir) {
    if (m_animState == kImpatient) {
      // Still wanted: once the impatient idle has played, standing again, and autonomy takes the sim
      // (GC: also the optional motion and idle animations unloaded).
      if (m_desiredState == kImpatient) {
        if (!(m_flags & 0x1000000u) || !m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{1})) return;
        m_animState = kFreeIdle;
        m_desiredState = kFreeIdle;
        m_flags &= ~0x1000000u;
        if (auto* p = dynamic_cast<cXPersonImpl*>(Impl(m_person))) p->SetIdlePlayerSimAutonomous();
        return;
      }
      m_flags &= ~0x1000000u;
      clearImpatientIdleAnimation();
    }
    EACTrack* outgoing = OutgoingTrack();
    const bool fromWalk = outgoing && (outgoing->m_flags & 2);
    if (m_desiredState == kFreeIdle) {
      if (m_animState == kFreeRun || (m_animState == kFreeWalk && fromWalk)) {
        m_animState = kRunStop;
        m_flags &= ~0x800000u;
        return;
      }
      if (m_animState == kRunStop) {
        if (!m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{1})) return;
        m_animState = kFreeIdle;
        m_flags &= ~0x800000u;
        return;
      }
    }
    // GC: running while carrying or with a low motive walks instead (LoadDCOptionalMotionAnims).
    if (m_desiredState == m_animState) return;
    CheckOptionalMotionAnimStatusOnStateChange(m_desiredState);
    m_animState = m_desiredState;
    return;
  }
  const float delta = static_cast<float>(getDeltaDirFromDesiredDir(m_desiredDir));
  if (delta == 0.0f) {
    SetNextStateFromCompletedTurn();
  } else {
    if (m_turnStartDir == kNoDir) m_turnStartDir = m_dir;
    if (m_animState == kFreeIdle || m_animState == kRunStop) {
      // GC: the person's sounds are quietened (cSoundPlayer::QuietBySourceID).
      initShuffleRotation(delta);
      m_animState = kFreeTurn;
    }
    if (rotateAnimation(dt, m_desiredDir, delta)) {
      m_turnStartDir = kNoDir;
      m_desiredDir = kNoDir;
      SetNextStateFromCompletedTurn();
    }
  }
  setPersonDirection(m_dir);
}

// 80063234
// A finished turn takes the desired state (GC: a run while carrying or low on a motive walks).
void SAnimator2::SetNextStateFromCompletedTurn() {
  if (m_desiredState == kFreeTurn || m_desiredState == kFreeIdle) {
    m_animState = kFreeIdle;
    return;
  }
  if (m_animState == m_desiredState) return;
  CheckOptionalMotionAnimStatusOnStateChange(m_desiredState);
  m_animState = m_desiredState;
}

// 80064C18
// GC: entering a free-move turn, walk or run loads the direct-control motion animations
// (LoadDCOptionalMotionAnims), leaving them unloads them, and leaving an idle unloads the optional
// idle; PC: the animations load when they are played.
void SAnimator2::CheckOptionalMotionAnimStatusOnStateChange(int) {}

// 8006A494
double SAnimator2::getDeltaDirFromDesiredDir(float desired) {
  const float d = Wrap(desired - m_dir);
  return std::fabs(d) < 0.0001f ? 0.0 : d;
}

// 8006A404
// The shuffle for a turn by `delta`: eighths of a turn (positive clockwise, i.e. negative delta),
// rounded away from zero past a sixteenth; a half turn stays 4.
int SAnimator2::getShuffleDirFromDeltaDir(float delta) {
  int dir = static_cast<int>(delta / -0.7853982f);
  if (dir != 4 && dir != -4 && delta != 0.0f) {
    const int half = static_cast<int>(delta / -0.3926991f);
    if (half != 0) dir += half < 1 ? -1 : 1;
    dir += 0.0f <= delta ? -1 : 1;
  }
  return dir;
}

// 800608DC
// Radians a second: instantly when stepping or carrying (2), 3 pi in free-move turns, walks and
// runs, else the shuffle's own rate.
float SAnimator2::GetTurnRate() {
  if (m_followMode == 3 || m_carryType == 2) return kTwoPi;
  if (m_animState != kFreeTurn && static_cast<unsigned int>(m_animState - kFreeWalk) > 1) return m_turnRate;
  return 9.424778f;
}

// 80060928
// One step of the turn toward `desired` (`delta` away): done (1) when this step reaches it, or at
// double speed or more.
int SAnimator2::rotateAnimation(float dt, float desired, float delta) {
  int done = 0;
  const float step = dt * GetTurnRate();
  if (step < std::fabs(delta)) {
    float d = (delta == 0.0f ? 0.0f : (0.0f <= delta ? 1.0f : -1.0f)) * step + m_dir;
    while (kPi < d) d -= kTwoPi;
    while (d <= -kPi) d += kTwoPi;
    m_dir = d;
  } else {
    m_dir = desired;
    done = 1;
    setPersonDirection(desired);
  }
  if (2.0f <= m_timeMult * m_pauseMult) {
    m_dir = desired;
    done = 1;
    setPersonDirection(desired);
  }
  return done;
}

// 8006A1A4
// The shuffle for a turn by `delta` and the rate that makes it last as long as its animation (read
// once from the animation, by turn type: from an idle 0, else 1), at least pi/8 a second.
void SAnimator2::initShuffleRotation(float delta) {
  m_flags |= 0x40;
  const int shuffle = getShuffleDirFromDeltaDir(delta);
  delta = std::fabs(delta);
  m_shuffleDir = shuffle;
  const unsigned int main = MainTrackFlags();
  const bool fromIdle = (main & 0x21) || (main & 4);
  const int row = fromIdle ? 0 : 1;
  const int bit = fromIdle ? shuffle + 8 : shuffle + 0x13;
  float* times = m_turnTime[row];
  if (!(m_velocityKnown & (1u << bit))) {
    const AnimRef* ref = nullptr;
    getTurnSkillID(ref, shuffle);
    if (ref) {
      if (auto* anim = static_cast<ERAnim*>(_animman.AddRef(AnimRefId(ref), nullptr, 0))) {
        const float t = static_cast<float>(anim->m_frameCount - 1) / anim->m_fps;
        if (t != 0.0f) {
          times[shuffle + 5] = t;
          times[-shuffle + 5] = t;
        }
        _animman.DelRef(anim, EResourceManager__DelRefMode{1});
      }
    }
    m_velocityKnown |= 1u << bit;
    m_velocityKnown |= 1u << (fromIdle ? -shuffle + 8 : -shuffle + 0x13);
  }
  m_turnRate = delta / times[shuffle + 5];
  if (m_turnRate < 0.3926991f) m_turnRate = 0.3926991f;
  m_flags &= ~0x10u;
}

// 8006AA34
// The animation of a shuffle: walking, routing or in free move the idle-to-walk turns (0x1D..0x24),
// else the standing turns.
void SAnimator2::getTurnSkillID(const AnimRef*& out, int shuffle) {
  int idx = -1;
  if (m_followMode == 1 || m_followMode == 8 || static_cast<unsigned int>(m_animState - kFreeIdle) < 6) {
    switch (shuffle) {
      case -5: idx = 0x24; break;
      case -4: idx = 0x22; break;
      case -3: idx = 0x20; break;
      case -2: case -1: idx = 0x1e; break;
      case 1: case 2: idx = 0x1d; break;
      case 3: idx = 0x1f; break;
      case 4: idx = 0x21; break;
      case 5: idx = 0x23; break;
      default: break;
    }
  } else {
    switch (shuffle) {
      case -5: idx = 5; break;
      case -4: idx = 0x10; break;
      case -3: idx = 7; break;
      case -2: idx = 9; break;
      case -1: idx = 0xd; break;
      case 1: idx = 0xe; break;
      case 2: idx = 8; break;
      case 3: idx = 6; break;
      case 4: idx = 0x11; break;
      case 5: idx = 4; break;
      default: break;
    }
  }
  if (idx >= 0) GetStdAnimRef(m_person, idx, out);
}

// 8006AD80
// The walk or run of the current style (ghosts their own; low-motive walks when loaded).
void SAnimator2::getWalkRunSkillID(const AnimRef*& out) {
  const bool ghost = m_person->IsGhost() != 0;
  if (m_walkStyle == 1 || m_walkStyle == 8) {
    GetStdAnimRef(m_person, ghost ? 0x49 : 0x15, out);
    return;
  }
  if (ghost) {
    GetStdAnimRef(m_person, 0x4a, out);
    return;
  }
  int idx = 0x14;
  if (m_walkStyle == 2 || (m_walkStyle != 3 && m_lowMotive == 5)) idx = 0x12;
  else if (m_walkStyle == 3 || m_lowMotive == 9) idx = 0x3b;
  GetStdAnimRef(m_person, idx, out);
  if (idx != 0x14 && (!out || !_animman.GetRef(AnimRefId(out)))) GetStdAnimRef(m_person, 0x14, out);
}

// 8006697C
// The person's direction (data 1, eighths): 0 radians is direction 2, turning the other way.
void SAnimator2::setPersonDirection(float dir) {
  int d = static_cast<int>(std::floor((-dir + 0.3926991f) / 0.7853982f)) + 2;
  while (d < 0) d += 8;
  if (cXPersonImpl* impl = Impl(m_person)) impl->m_data.WriteVar(1, static_cast<short>(d % 8));
}

// 80069230
double SAnimator2::getPersonDirection() {
  cXPersonImpl* impl = Impl(m_person);
  double d = ((impl ? impl->GetData(1) : 0) - 2.0f) * -0.7853982f;
  if (d < -kPi) d += kTwoPi;
  if (kPi < d) d -= kTwoPi;
  return d;
}

// 8006912C
void SAnimator2::setAnimationDirectionToPersonDirection() { m_dir = static_cast<float>(getPersonDirection()); }

// 8005F8D8
int SAnimator2::getPersonX() { return Impl(m_person)->m_location[1]; }
// 8005F918
int SAnimator2::getPersonY() { return Impl(m_person)->m_location[0]; }

// 80061158
// The anim state for this frame (routing: by follow mode; under player control the free-move state
// stands; simulator-controlled people idle or play their skill), then its animation; the sim is posed
// where it stands (its person's location after SnapToGrid).
void SAnimator2::updateRenderAnimation() {
  int state = m_animState;
  if (m_followMode < 6) {
    if (m_routing == 1) {
      // By follow mode: 1 the starting turn (2), 3 walking (1), 5 the turn at the end (3).
      if (m_followMode == 1) state = 2;
      else if (m_followMode == 3) state = 1;
      else if (m_followMode == 5) state = 3;
    } else if (state == kImpatient) {
      // Seated (idle states 1, 2) there is no impatient idle: autonomy takes the sim at once.
      auto* p = dynamic_cast<cXPersonImpl*>(Impl(m_person));
      if (p && static_cast<unsigned int>(p->GetIdleState() - 1) < 2u) {
        m_animState = kFreeIdle;
        m_desiredState = kFreeIdle;
        p->SetIdlePlayerSimAutonomous();
      }
    } else if (!IsSimulatorControlled()) {
      // GC: object data 2 set means sim-controlled idling (8).
      if (Impl(m_person)->GetData(2) != 0) state = kSimIdle;
      else if (state < kFreeIdle) state = kFreeIdle;
    } else {
      // The current main track (its flag 0x10000) a skill: 5, an algorithmic idle (0x20) 7; idles and
      // anything else 0. GC: socialising (vtable 0x334) 6.
      const unsigned int main = MainTrackFlags();
      if (!(main & eTrackFlags::kMain) || (main & 1)) state = kIdle;
      else state = (main & 0x20) ? 7 : 5;
    }
  } else {
    SIMMY_STUB("SAnimator2::updateRenderAnimation: awareness states", 0x80061158);
  }
  if (m_animState != state) {
    CheckOptionalMotionAnimStatusOnStateChange(state);
    m_animState = state;
  }
  switch (m_animState) {
    case kIdle: case kSimIdle: case kFreeIdle:
      handleIdleAnimation();
      break;
    case kFreeWalk: case kFreeRun:
      stopIdleOverlay();
      handleFreeMoveWalkRunAnimation();
      break;
    case 5: case 6:
      stopIdleOverlay();
      break;
    case 1:
      stopIdleOverlay();
      handleWalkRunAnimation();
      break;
    case 2: case 3:
      stopIdleOverlay();
      handleTurnAnimation();
      break;
    case 7:
      SIMMY_STUB("SAnimator2::handleSkillIdleAnimation", 0x800635DC);
      break;
    case kRunStop:
      stopIdleOverlay();
      handleRunStopAnimation();
      break;
    case kImpatient:
      stopIdleOverlay();
      handleImpatientIdleAnimation();
      break;
    case kFreeTurn:
      stopIdleOverlay();
      handleTurnAnimation();
      break;
    default:
      SIMMY_STUB("SAnimator2::updateRenderAnimation: walking, sidestep, skill, impatient and awareness animations", 0x80061158);
      break;
  }
  SIMMY_STUB("SAnimator2::handleMoodAnimations", 0x80061804);
  if (m_pendingAnim == 0) {
    if (m_resetPos) {
      m_resetPos = 0;
      m_pos[0] = static_cast<float>(getPersonX());
      m_pos[1] = static_cast<float>(getPersonY());
    }
    m_renderPos[0] = m_pos[0];
    m_renderPos[1] = m_pos[1];
    if (m_sim->m_modelReady) positionCharacter(nullptr);
    updateParticles();
  }
}

// 80068ECC
// Not under player control, routing, socialising, in motive failure or busy with an interaction (GC:
// vtable 0x334, 0x354, 0x22C, 0xF4; PC: socialising and motive failure are not ported), or a skill
// track (0x10) still playing.
bool SAnimator2::IsSimulatorControlled() {
  if (!(m_flags & 1) || m_routing == 1) return true;
  // GetCurrentInteractionStackObject (vtable 0xF4): PC, an object's tree on the person's stack.
  if (!Impl(m_person)->m_gosubObjects.empty()) return true;
  return m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{0x10}) == 0;
}

// 80068FBC
// The player steers this person (direct control) or the simulator does; the controller's flag 1
// follows. GC: also AwarenessManager::SetPlayerControl.
void SAnimator2::SetPlayerControl(bool control) {
  if (control) m_flags |= 1;
  else m_flags &= ~1u;
  if (!m_sim) return;
  if (control) m_sim->m_anim.m_flags |= 1;
  else m_sim->m_anim.m_flags &= ~1u;
}

// 80069114
int SAnimator2::GetPlayerControl() { return (m_flags & 1) ? 1 : 0; }
// 80069208
bool SAnimator2::IsDirectControlEnabled() { return static_cast<unsigned int>(m_animState - kFreeIdle) < 6; }
// 80069104
void SAnimator2::SetDesiredAnimState(int state) { m_desiredState = state; }
// 8006910C
void SAnimator2::SetCameraZoomRatio(float ratio) { m_zoomRatio = ratio; }
// 800690C0
double SAnimator2::GetRealDir() { return m_dir; }
// 800690C8
EVec2& SAnimator2::GetPos() { return *reinterpret_cast<EVec2*>(m_pos); }
// 800690D8
cXPerson* SAnimator2::GetPerson() { return m_person; }
// 800690E0
ESim* SAnimator2::GetSim() { return m_sim; }
// 800690E8
int SAnimator2::GetAnimState() { return m_animState; }
// 800689F4
void SAnimator2::SetIdleInitialized(bool initialized) { m_idleInitialized = initialized ? 1 : 0; }
// 8005D124
void SAnimator2::SnapToGrid() { m_resetPos = 1; }
// 8005D130
void SAnimator2::ForceLocation() { SnapToGrid(); }

// 80067758
// 0 the right hand, 1 the head, 2 the pelvis (where positionCharacter last put them).
void SAnimator2::GetBonePos(int bone, EVec3& out) {
  if (bone >= 0 && bone < 3) out = m_bonePos[bone];
}

// 800655A4
void SAnimator2::convertAnimationFormatToEngineFormat(EVec3& anim, EVec3& engine) {
  engine.x = anim.y * 0.0625f;
  engine.z = 0.0f;
  engine.y = anim.x * 0.0625f;
}

// 800655D4
EVec3 SAnimator2::GetEngineFormatPos() {
  EVec3 anim{m_pos[0], m_pos[1], 0.0f}, engine;
  convertAnimationFormatToEngineFormat(anim, engine);
  return engine;
}

// 80065094
// The sim at its render position (in the world's frame), turned to its direction: the controller
// advances (root motion is dropped: the animator moves the sim) and poses it; bounds, the shadow's
// bounds, the instance's orientation and the bones' positions follow. The animation-format position
// (1/16 tiles, a tile's centre at +8) is already the world position: the house's position (0.5, 0.5)
// is NOT added — it only centres the objects, whose models sit at their integer tile
// (ISimsObjectModel::SetObjOrient); adding it put sims half a tile off, e.g. inside the wall behind
// them. PC: the bounds are a tile around the sim,
// 1.8 high (GC: EAnimController::CalcVisibilitySphere of the pose, not ported); a bone's position is
// the translation of its EAnimController::CalcNodeOrient.
void SAnimator2::positionCharacter(EMat4* orient) {
  EMat4 m;
  if (!orient) {
    EVec3 anim{m_renderPos[0], m_renderPos[1], 0.0f}, pos;
    convertAnimationFormatToEngineFormat(anim, pos);
    m.Id();
    m.RotateZ(m_dir);
    m.m[3][0] = pos.x, m.m[3][1] = pos.y, m.m[3][2] = pos.z;
    orient = &m;
  }
  EAnimController& anim = m_sim->m_anim;
  anim.Update(nullptr, nullptr, EVec3{1.0f, 1.0f, 1.0f});
  anim.Compute(*orient, _framecount);
  const float px = orient->m[3][0], py = orient->m[3][1];
  m_sim->m_sphere[0] = px, m_sim->m_sphere[1] = py, m_sim->m_sphere[2] = 0.9f, m_sim->m_sphere[3] = 1.0f;
  float bounds[6] = {px - 0.5f, py - 0.5f, 0.0f, px + 0.5f, py + 0.5f, 1.8f};
  m_sim->SetBounds(*reinterpret_cast<EBound3*>(bounds));
  // The shadow's bounds: three times the sim's sphere across, from the floor up 1.
  if (EIStaticModel* shadow = m_sim->GetShadow()) {
    if (!m_person->IsInvisible()) {
      const float* s = m_sim->m_sphere;
      const float r = s[3] * 3.0f;
      float sb[6] = {s[0] - r, s[1] - r, 0.0f, s[0] + r, s[1] + r, 1.0f};
      shadow->SetBounds(*reinterpret_cast<EBound3*>(sb));
    }
  }
  m_sim->SetOrient(*orient);
  const int nodes = anim.NodeCount();
  for (int b = 0; b < 3; ++b) {
    const int index = m_boneIndex[b];
    if (index < 0 || index >= nodes || !anim.m_matrices) continue;
    EMat4 n;
    anim.CalcNodeOrient(index, n);
    m_bonePos[b] = EVec3{n.m[3][0], n.m[3][1], n.m[3][2]};
  }
  // The carry direction: the z axis of the last bone posed (the pelvis), flattened.
  int last = -1;
  for (int b = 0; b < 3; ++b)
    if (b != 1 && m_boneIndex[b] >= 0 && m_boneIndex[b] < nodes) last = m_boneIndex[b];
  m_carryDir = EVec3{0.0f, 0.0f, 0.0f};
  if (last >= 0 && anim.m_matrices) {
    EMat4 n;
    anim.CalcNodeOrient(last, n);
    m_carryDir = EVec3{n.m[2][0], n.m[2][1], 0.0f};
    const float len = std::sqrt(m_carryDir.x * m_carryDir.x + m_carryDir.y * m_carryDir.y);
    if (len != 0.0f) m_carryDir.x /= len, m_carryDir.y /= len;
  }
}

// 8005F4E0
void SAnimator2::ResetCensorship() {}

// ---------------------------------------------------------------------------------------------------
// Props (cXPersonImpl::TryChangeSuit's dress-up props: models drawn with the sim's bones, ESim's prop
// pass; see proptable.hpp)

// 8005D04C
// After the person's trees reset (cXPersonImpl::Reset): the optional motion and long idle animations
// unloaded, the props taken off, the impatient idle cleared; no animate primitive's animation, events,
// main or overlay animation, facing or costume; the flags but player control cleared; an animation
// still loading given up; the awareness manager reset (+0x620, not ported).
void SAnimator2::Reset() {
  UnloadDCOptionalMotionAnims();
  UnloadSMOptionalMotionAnims();
  UnloadPendingLongIdleAnim();
  removeAllProps();
  clearImpatientIdleAnimation();
  m_skillRunning = 0;
  m_animEvents.clear();
  m_flags &= 1u;
  m_zoomRatio = 0.0f;
  m_eventFrame = 0;
  m_overlayEventFrame = 0;
  m_curAnim = nullptr;
  m_overlayAnim = nullptr;
  m_dir = 0.0f;
  m_lastCostume = 0;
  if (m_pendingAnim != 0) {
    _animman.DelRefAsync(static_cast<unsigned int>(m_pendingAnim));
    m_pendingAnim = 0;  // GC: also m_pendingCount - 1
  }
}

// 8005F4E8
// The adult's model, or (a child, when the prop has one) the child's.
unsigned int SAnimator2::getCorrectId(const PropRef* prop) {
  cXPersonImpl* impl = Impl(m_person);
  if ((impl && impl->IsAdult()) || prop->GetChildModel() == 0) return prop->GetAdultModel();
  return prop->GetChildModel();
}

// 8005F548
// Puts the prop on (any copy worn already comes off first).
void SAnimator2::Dress(const PropRef* prop) {
  const unsigned int id = getCorrectId(prop);
  if (id == 0) return;
  Undress(prop);
  AddProp(id, prop->GetCastsShadow());
}

// 8005F5D0
// Model `id` worn from now on (loaded with EResourceManager::AddRefAsync); `shadow`: also in the shadow.
void SAnimator2::AddProp(unsigned int id, bool shadow) {
  PreloadedModel m;
  m.m_res = id;
  m.m_model = static_cast<ERModel*>(_modelman.AddRefAsync(id));
  m.m_flag = shadow ? 1 : 0;
  m_preloadedModels.push_back(m);
  static const bool trace = std::getenv("SIMMY_SIM_TRACE") != nullptr;  // PC dev trace
  if (trace)
    std::printf("[prop] %s wears %08x%s%s\n", Impl(m_person) ? Impl(m_person)->DebugName() : "?", id,
                shadow ? " (shadow)" : "", m.m_model ? "" : " (no model)");
}

// 8005F758
void SAnimator2::RemoveProp(unsigned int index) {
  if (index >= m_preloadedModels.size()) return;
  static const bool trace = std::getenv("SIMMY_SIM_TRACE") != nullptr;  // PC dev trace
  if (trace)
    std::printf("[prop] %s takes off %08x\n", Impl(m_person) ? Impl(m_person)->DebugName() : "?", m_preloadedModels[index].m_res);
  _modelman.DelRefAsync(m_preloadedModels[index].m_res);
  m_preloadedModels.erase(m_preloadedModels.begin() + static_cast<long>(index));
}

// 8005F7E0
// Its model loaded ahead (EResourceManager::PreloadResource): 1 once it is.
int SAnimator2::PreloadDress(const PropRef* prop) {
  const unsigned int id = getCorrectId(prop);
  if (id == 0) return 0;
  return _modelman.PreloadResource(id) != 0 ? 1 : 0;
}

// 8005F82C
// Takes the prop off, if worn.
void SAnimator2::Undress(const PropRef* prop) {
  const int index = getIndexOfPropID(getCorrectId(prop));
  if (index >= 0) RemoveProp(static_cast<unsigned int>(index));
}

// 80069354
int SAnimator2::getIndexOfPropID(unsigned int id) {
  for (size_t i = 0; i < m_preloadedModels.size(); ++i)
    if (m_preloadedModels[i].m_res == id) return static_cast<int>(i);
  return -1;
}

// 80067ABC
void SAnimator2::removeAllProps() {
  for (const PreloadedModel& m : m_preloadedModels) _modelman.DelRefAsync(m.m_res);
  m_preloadedModels.clear();
}

// 800677B0
// A worn (loaded) prop blends with the frame buffer (ERModel +0x100): the sim's prop pass goes in the
// order table.
bool SAnimator2::PropsHaveAlpha() {
  bool alpha = false;
  for (const PreloadedModel& m : m_preloadedModels)
    if (m.m_model) alpha = alpha || m.m_model->m_hasBlend != 0;
  return alpha;
}

// 8006781C
// Each prop (`flaggedOnly`: those drawn in the shadow too) with the bone matrices set by the caller,
// once loaded (EResourceManager::GetRef). Five models (GC 0xDF85DFA0, 0xF281A334, 0x321DE146,
// 0xBE8F6817, 0xFFC9CA1B) take `shader` instead of their own when there is one: their geometry only
// (ERModel::DrawGeometry), the shader set once for a run of them. Not while the sim's model changes.
void SAnimator2::DrawProps(ERC* rc, bool flaggedOnly, EShader* shader) {
  if (!m_sim || m_sim->HasQueuedOperation()) return;
  bool shaderSet = true;
  for (PreloadedModel& m : m_preloadedModels) {
    if (!m.m_model) m.m_model = static_cast<ERModel*>(_modelman.GetRef(m.m_res));
    if (flaggedOnly && m.m_flag == 0) continue;
    const unsigned int id = m.m_res;
    const bool special = id == 0xdf85dfa0u || id == 0xf281a334u || id == 0x321de146u || id == 0xbe8f6817u || id == 0xffc9ca1bu;
    if (special && shader) {
      if (!shaderSet) {
        shader->Select(rc, 0);  // (the caller selected it for the first run: GC starts with "set")
        shaderSet = true;
      }
      if (m.m_model) m.m_model->DrawGeometry(rc);
      continue;
    }
    if (m.m_model) m.m_model->Draw(rc);
    shaderSet = false;
  }
}

// 800679E8
// The props drawn in the shadow, as shadows (ERModel::DrawAsShadow).
void SAnimator2::DrawPropsShadow(ERC* rc) {
  if (!m_sim || m_sim->HasQueuedOperation()) return;
  for (PreloadedModel& m : m_preloadedModels) {
    if (!m.m_model) m.m_model = static_cast<ERModel*>(_modelman.GetRef(m.m_res));
    if (m.m_flag != 0 && m.m_model) m.m_model->DrawAsShadow(rc);
  }
}

// PC development aid: SIMMY_CENSOR_TEST=<person data 0x1E bits> censors player 1's sim with them.
static int CensorTestFlags(cXPerson* person) {
  static const int test = std::getenv("SIMMY_CENSOR_TEST") ? std::atoi(std::getenv("SIMMY_CENSOR_TEST")) : 0;
  return test && _globals.GetSelectedPerson(0) == person ? test : 0;
}

// 80067B54
// A sim with a full skeleton (over 50 nodes) is censored while any of its person data 0x1E bits are
// set (0x80, 1, 2, 4, 8, 0x10); the instance then stays solid near the camera (+0x68 = 2).
void SAnimator2::updateCensor() {
  const int flags = m_person->GetPersonData(0x1e) | CensorTestFlags(m_person);
  if (m_sim->m_anim.NodeCount() <= 0x32) return;
  m_censor = 0;  // GC: also clears ESim +0x62C (never set: the body is always drawn)
  if (flags & 0x80) m_censor = 1;
  if (flags & 1) m_censor = 1;
  if (flags & 2) m_censor = 1;
  if (flags & 4) m_censor = 1;
  if (flags & 8) m_censor = 1;
  if (flags & 0x10) m_censor = 1;
  m_sim->m_fadeMode = flags == 0 ? 0 : 2;
}

// 80067C4C
// The censor rectangles in screen space, made from the chest (node 0xE), the pelvis (3) and the
// fingers (0x2B, 0x33) projected with the censor's depth offset, sized by the distance (1.1 x the
// projection's y scale / w at the middle of the body), at the nearer of the chest's and pelvis's
// depths. Person data 0x1E picks them (1 the hips, 2 the chest, 4 below the hips, 8 / 0x10 the right /
// left hand); outfit 1 (naked) adds the hips, and the chest for women. An adult (or nothing of 1..4)
// gets each; anyone else one rectangle over the body. The graphics draws them at once (the mosaic),
// and the window is selected again.
void SAnimator2::DrawCensor(ERC* rc) {
  static const float kCensorRectZOffset = -0.5f;  // 804FDAC0 CENSOR_RECT_Z_OFFSET
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  if (m_sim->HasQueuedOperation() || !m_censor || !win) return;
  EAnimController& anim = m_sim->m_anim;
  auto at = [](const EMat4& m, float x, float y, float z) {
    return EVec3{m.m[2][0] * z + m.m[0][0] * x + m.m[1][0] * y + m.m[3][0],
                 m.m[2][1] * z + m.m[0][1] * x + m.m[1][1] * y + m.m[3][1],
                 m.m[2][2] * z + m.m[0][2] * x + m.m[1][2] * y + m.m[3][2]};
  };
  EMat4 m;
  anim.CalcNodeOrient(0xe, m);
  EVec3 chest = at(m, 0.0f, 0.1f, 0.0f), chestS;
  if (!win->TransformToScreen(chest, chestS, kCensorRectZOffset)) return;
  anim.CalcNodeOrient(3, m);
  EVec3 pelvis = at(m, -0.1f, 0.05f, 0.0f), pelvisS;
  if (!win->TransformToScreen(pelvis, pelvisS, kCensorRectZOffset)) return;
  anim.CalcNodeOrient(0x2b, m);
  EVec3 left = at(m, 0.0f, 0.1f, 0.0f), leftS;
  if (!win->TransformToScreen(left, leftS, kCensorRectZOffset)) return;
  anim.CalcNodeOrient(0x33, m);
  EVec3 right = at(m, 0.0f, 0.1f, 0.0f), rightS;
  if (!win->TransformToScreen(right, rightS, kCensorRectZOffset)) return;
  // The size: by the middle of the body's w.
  const EVec3 mid{(chest.x + pelvis.x) * 0.5f, (chest.y + pelvis.y) * 0.5f, (chest.z + pelvis.z) * 0.5f};
  const EMat4& vp = win->m_lookAtProj;
  const float w = mid.x * vp.m[0][3] + mid.y * vp.m[1][3] + mid.z * vp.m[2][3] + vp.m[3][3];
  float scale = (1.0f / w) * 1.1f * win->m_vpIn.m_scale[1];
  if (scale < 0.0f) scale = -scale;
  const float z = pelvisS.z < chestS.z ? pelvisS.z : chestS.z;
  const bool night = Globs_pSimulator && Globs_pSimulator->GetTimeOfDay() == 2;  // GC: vtable 0xF4
  unsigned int flags = static_cast<unsigned int>(m_person->GetPersonData(0x1e) | CensorTestFlags(m_person));
  if (m_person->GetPersonData(8) == 1) {
    if (m_sim->IsFemale()) flags |= 2;  // GC: person vtable 0x20C
    flags |= 1;
  }
  auto rect = [&](const EVec3& c, float hx, float hy, float depth) {
    EVec3 lo{c.x - hx, c.y - hy, depth}, hi{c.x + hx, c.y + hy, depth};
    _pGfx->SetCensorRect(lo, hi, night);
  };
  auto mix = [&](float a, float b) {
    return EVec3{pelvisS.x * a + chestS.x * b, pelvisS.y * a + chestS.y * b, pelvisS.z * a + chestS.z * b};
  };
  cXPersonImpl* impl = dynamic_cast<cXPersonImpl*>(m_person);
  if ((flags & 7) == 0 || (impl && impl->IsAdult())) {  // GC: person vtable 0x214
    if (flags & 1) rect(mix(0.95f, 0.050000012f), scale * 0.39f, scale * 0.36f, z);
    if (flags & 2) rect(mix(0.2f, 0.8f), scale * 0.41f, scale * 0.34f, z);
    if (flags & 4) rect(mix(1.2f, -0.20000005f), scale * 0.67f, scale * 0.67f, z);
    if (flags & 8) rect(rightS, scale * 0.37f, scale * 0.37f, rightS.z);
    if (flags & 0x10) rect(leftS, scale * 0.37f, scale * 0.37f, leftS.z);
  } else {
    rect(mix(0.83f, 0.17000002f), scale * 0.67f, scale * 0.83f, z);
  }
  _pGfx->DrawCensorRects(rc);
  win->Select(rc);
}

// 8006867C
void SAnimator2::AlignCensorToCamera(EVec3&, EVec3&, E3DWindow*) {}

// 8006BFE8
void SAnimator2::stopIdleOverlay() {
  if (!(m_flags & 0x1000)) return;
  m_flags &= ~0x1000u;
  m_sim->m_anim.SetTrackBlendSmooth(eTrackFlags{0x100}, 0.0f, kExitIdleOverlayBlend, 0.01f);
}

// 80068B8C
// The motive (energy 5, bladder 9, hunger 7, hygiene 8, social 0xE, fun 0xF) whose idle a standing sim
// plays: the lowest one under -75; 0 none (or carrying a held object, or flag 0x8000000).
int SAnimator2::shouldUseLowMotiveIdle() {
  if (!m_person || (m_flags & 0x8000000)) return 0;
  cXPersonImpl* p = dynamic_cast<cXPersonImpl*>(m_person);
  if (!p || p->IsCarryingDCObject()) return 0;
  int motive = 5;
  double low = p->GetMotive(5);
  for (int m : {9, 7, 8, 0xe, 0xf}) {
    const double v = p->GetMotive(m);
    if (v < low) low = v, motive = m;
  }
  return low < -75.0 ? motive : 0;
}

// 800632EC
// The next standing idle: a low motive's own idle (shouldUseLowMotiveIdle: energy 0x3C, bladder 0x3A,
// hunger 0x3E, hygiene 0x3F, social 0x40, fun 0x3D); after a turn or walk the base idle (0x46);
// otherwise every fifth idle or so one of the long ones (am_idle_fidget01..03: 0x43, 0x44, 0x45; men
// carrying 0x44), loaded in the background and played once it is in, else the base idle.
// GC: the low-motive idles are optional animations loaded on demand (Load/UnloadOptionalIdleAnim);
// PC: GetStdAnimRef's animation is loaded when played.
int SAnimator2::selectRandomIdle() {
  if (const int low = shouldUseLowMotiveIdle()) {
    if (m_longIdle) m_longIdle = 0;  // GC: UnloadPendingLongIdleAnim
    m_lowMotiveIdle = static_cast<unsigned char>(low);
    switch (low) {
      case 8: return 0x3f;
      case 5: return 0x3c;
      case 7: return 0x3e;
      case 0xe: return 0x40;
      case 9: return 0x3a;
      case 0xf: return 0x3d;
      default: return 3;
    }
  }
  m_lowMotiveIdle = 0;  // GC: UnloadOptionalIdleAnim
  m_flags &= ~0x4000000u;
  if (!(MainTrackFlags() & 0x21)) {
    m_longIdle = 0;  // GC: UnloadPendingLongIdleAnim
    return 0x46;
  }
  const AnimRef* ref = nullptr;
  if (m_longIdle == 0) {
    if (m_longIdleCount < 4) {
      ++m_longIdleCount;
      return 0x46;
    }
    const int pick = (simmy::sim_rand() & 0xff) % 3;
    m_longIdle = static_cast<unsigned char>(pick == 1 ? 0x44 : pick == 0 ? 0x43 : 0x45);
    GetStdAnimRef(m_person, m_longIdle, ref);
  } else {
    GetStdAnimRef(m_person, static_cast<char>(m_longIdle), ref);
    if (ref && _animman.GetRef(AnimRefId(ref))) {
      if (m_flags & 0x80000) _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
      const int idle = m_longIdle;
      m_longIdle = 0;
      m_longIdleCount = 0;
      m_flags = (m_flags & ~0x80000u) | 0x4000000u;
      return idle;
    }
  }
  if (ref && !(m_flags & 0x80000)) {
    _animman.AddRefAsync(AnimRefId(ref));
    m_flags |= 0x80000;
  }
  ++m_longIdleCount;
  return 0x46;
}

// 800696C0
// Straight into the idle selectRandomIdle picks, blending over kBlendToDefaultIdle, looping. GC:
// also the low-motive idles and an idle overlay (selectIdleOverlay).
void SAnimator2::StartDefaultIdle() {
  const int idle = selectRandomIdle();
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, idle, ref);
  if (!ref) return;
  if (EACTrack* t = m_sim->m_anim.SetTrackAnim(eTrackFlags{1}, AnimRefId(ref), -1, kBlendToDefaultIdle, nullptr)) {
    m_sim->m_anim.SetTrackSpeed(t, m_timeMult);
    t->m_endMode = 0;
  }
  m_eventFrame = 0;
  m_curAnim = ref;
  m_eventFraction = 0.0f;
  m_flags |= 0x10000;
}

// 8006BD5C
// Half the time, unless something else plays over the idle (flag 0x2000), a pet, a sim in the walk-over
// state (0x10), a low-motive idle, flag 0x4000000 or an overlay already playing: an upper-body fidget
// on track 0x100 (stock anims 0x13, 0x18, 0x19, 0x47, 0x48 weighted 20 each, the first 40 unless
// carrying or flags 0x800 / aware 0x200), blended in from nothing over kDefaultBlendTime with the
// upper-body factors.
void SAnimator2::selectIdleOverlay() {
  cXPersonImpl* p = Impl(m_person);
  if ((m_flags & 0x2000) || !p || p->IsPet() == 1 || m_animState == 0x10 || shouldUseLowMotiveIdle() ||
      (m_flags & 0x4000000) || (m_flags & 0x1000) || simmy::sim_rand() % 100 >= 0x32)
    return;
  m_flags |= 0x1000;
  int weights[5];
  weights[0] = 0x14;
  weights[1] = (p->IsCarrying() || (m_flags & 0x800) || (m_awareFlags & 0x200)) ? 0x14 : 0x28;
  weights[2] = weights[1] + 0x14;
  weights[3] = weights[1] + 0x28;
  weights[4] = weights[1] + 0x3c;
  const int r = simmy::sim_rand() % weights[4];
  int i = 0;
  while (weights[i] < r && ++i < 5) {
  }
  static const int kOverlays[5] = {0x13, 0x18, 0x19, 0x47, 0x48};
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, kOverlays[i < 5 ? i : 4], ref);
  if (!ref) return;
  EAnimController& anim = m_sim->m_anim;
  if (EACTrack* t = anim.SetTrackAnim(eTrackFlags{0x100}, AnimRefId(ref), -1, -1.0f, nullptr)) {
    anim.SetTrackIntensity(t, 0.0001f, true);
    anim.SetTrackBlendFactors(t, m_UpperBodyIndexArray);
    anim.SetTrackBlendSmooth(t, 1.0f, kDefaultBlendTime, 0.01f);
    t->m_endMode = 1;
  }
}

// 80064750
// The low-motive idle of the motive (+0x5FA: energy 0x3C, hunger 0x3E, hygiene 0x3F, bladder 0x3A,
// social 0x40, fun 0x3D); unless one is pending (0x100000), it is loaded (pending until then) or marks
// itself loaded (0x8000000). Null: no such motive.
RET_UNKNOWN SAnimator2::LoadOptionalIdleAnim() {
  int id = 0;
  switch (m_lowMotiveIdle) {
    case 5: id = 0x3c; break;
    case 7: id = 0x3e; break;
    case 8: id = 0x3f; break;
    case 9: id = 0x3a; break;
    case 0xe: id = 0x40; break;
    case 0xf: id = 0x3d; break;
    default: return 0;
  }
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, id, ref);
  if (ref && !(m_flags & 0x100000)) {
    if (!_animman.GetRef(AnimRefId(ref))) {
      _animman.AddRefAsync(AnimRefId(ref));
      m_flags |= 0x100000;
    } else {
      m_flags |= 0x8000000;
    }
  }
  return reinterpret_cast<RET_UNKNOWN>(ref);
}

// 80063A1C
// Standing: the sim is drawn; a fidget overlay that has played out blends away. The person's idle state
// (GetIdleState) picks what follows when the idle playing has looped: 0 a random idle (selectRandomIdle)
// and maybe a fidget overlay (selectIdleOverlay's choice), 1 / 2 the sitting / lying idles 1 / 2, 3
// (impatient) the random idle with an overlay for three seconds, then the impatient idle 0x42. A pending
// low-motive idle (flag 0x100000) plays once loaded. The idle blends in from a turn over
// kTurnToIdleBlendTime, looping.
void SAnimator2::handleIdleAnimation() {
  cXPersonImpl* p = Impl(m_person);
  const int idleState = p ? p->GetIdleState() : 0;  // vtable 0x16C
  m_sim->DrawSim(true);
  m_flags &= ~0x2000u;
  if (!m_idleInitialized || m_idleState != idleState) SetIdleInitialized(true);
  EAnimController& anim = m_sim->m_anim;
  if ((m_flags & 0x1000) && anim.IsTrackAnimComplete(eTrackFlags{0x100})) {
    m_flags &= ~0x1000u;
    anim.SetTrackBlendSmooth(eTrackFlags{0x100}, 0.0f, kExitIdleOverlayBlend, 0.01f);
  }
  bool impatientNow = false;
  m_idleState = p ? p->GetIdleState() : 0;
  const bool looped = anim.IsTrackAnimComplete(eTrackFlags{0x21}) != 0;
  if (m_idleState == 3) {
    m_impatientTime += _dt;
    if (3.0f < m_impatientTime && m_impatientTime - _dt <= 3.0f) {
      impatientNow = true;
      stopIdleOverlay();
    }
  }
  auto play = [&](const AnimRef* ref) {
    m_eventFrame = 0;
    m_eventFraction = 0.0f;
    m_curAnim = ref;
    m_flags |= 0x10000;
    const float blend = (MainTrackFlags() & 0xc) ? kTurnToIdleBlendTime : -1.0f;
    if (EACTrack* t = anim.SetTrackAnim(eTrackFlags{1}, AnimRefId(ref), -1, blend, nullptr)) {
      anim.SetTrackSpeed(t, m_timeMult);
      t->m_endMode = 0;
    }
  };
  if (m_flags & 0x100000) {
    // The low-motive idle being loaded: it plays once it is in.
    const AnimRef* ref = reinterpret_cast<const AnimRef*>(LoadOptionalIdleAnim());
    if (!ref || !_animman.GetRef(AnimRefId(ref))) return;
    m_flags = (m_flags & ~0x100000u) | 0x8000000;
    play(ref);
    _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
    return;
  }
  if (!looped && !impatientNow) return;
  int idle = -1;
  switch (m_idleState) {
    case 0:
      idle = selectRandomIdle();
      selectIdleOverlay();
      m_impatientTime = 0.0f;
      break;
    case 1:
    case 2:
      idle = m_idleState;
      m_impatientTime = 0.0f;
      break;
    case 3:
      if (m_impatientTime <= 3.0f) {
        if (looped) {
          idle = selectRandomIdle();
          selectIdleOverlay();
        }
      } else {
        idle = 0x42;
      }
      break;
    default: break;
  }
  if (idle == -1) return;
  if (m_lowMotiveIdle != 0) {
    LoadOptionalIdleAnim();
    if (m_flags & 0x100000) return;
  }
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, idle, ref);
  if (ref) play(ref);
}

// 80064C84
// The shuffle's animation, once per turn (flag 0x10): from a walk on the turn track 8 (else 4),
// blended from another turn over kTurnToTurnBlendTime (else kOtherToTurnBlendTime), held at its end,
// played so it lasts as long as the turn (free-move turns rotate at 3 pi a second). GC: not while
// carrying.
void SAnimator2::handleTurnAnimation() {
  if (m_flags & 0x10) return;
  if (m_carryType == 2 || (m_carryType == 1 && m_animState == 3)) return;
  const AnimRef* ref = nullptr;
  getTurnSkillID(ref, m_shuffleDir);
  unsigned int flags = 0;
  if (m_animState == 2 || m_animState == 0x11) flags = 4;
  else if (m_animState == 3 || m_animState == 0xf) flags = 8;
  else if (static_cast<unsigned int>(m_animState - kFreeIdle) < 6) flags = (MainTrackFlags() & 2) ? 8 : 4;
  m_flags |= 0x10;
  if (!ref) return;
  const float blend = (MainTrackFlags() & flags) ? kTurnToTurnBlendTime : kOtherToTurnBlendTime;
  m_eventFrame = 0;
  m_flags |= 0x10000;
  m_curAnim = ref;
  m_eventFraction = 0.0f;
  EAnimController& anim = m_sim->m_anim;
  EACTrack* t = anim.SetTrackAnim(eTrackFlags{static_cast<int>(flags)}, AnimRefId(ref), -1, blend, nullptr);
  if (!t) return;
  anim.SetTrackPos(t, 0.0f, true);
  t->m_endMode = 1;
  if (m_animState == kFreeTurn) anim.SetTrackSpeed(t, m_timeMult * m_pauseMult * (9.424778f / m_turnRate));
  else anim.SetTrackSpeed(t, m_timeMult * m_pauseMult);
}

// 800630DC
// Once: the run-stop for the foot that is down (by the run's phase), held at its end.
void SAnimator2::handleRunStopAnimation() {
  if (m_flags & 0x800000) return;
  EAnimController& anim = m_sim->m_anim;
  const double phase = anim.GetTrackPos(eTrackFlags{eTrackFlags::kMain});
  const bool left = !(0.25 <= phase && phase < 0.75);
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, left ? 0x31 : 0x32, ref);
  m_flags |= 0x800000;
  if (!ref) return;
  m_eventFrame = 0;
  m_flags |= 0x10000;
  m_curAnim = ref;
  m_eventFraction = 0.0f;
  EACTrack* t = anim.SetTrackAnim(eTrackFlags{1}, AnimRefId(ref), -1, kDefaultBlendTime, nullptr);
  if (!t) return;
  t->m_endMode = 1;
  anim.SetTrackPos(t, 0.0f, true);
  anim.SetTrackSpeed(t, m_timeMult * m_pauseMult);
}

// 80064434
// The impatient idle (stock animation 0x41) once, on the main track, when it has loaded (an async
// reference while it loads, flag 0x40000); flag 0x1000000 says it plays.
void SAnimator2::handleImpatientIdleAnimation() {
  if (m_flags & 0x1000000u) return;
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, 0x41, ref);
  if (!ref) return;
  if (!_animman.GetRef(AnimRefId(ref))) {
    if (!(m_flags & 0x40000u)) {
      _animman.AddRefAsync(AnimRefId(ref));
      m_flags |= 0x40000u;
    }
    return;
  }
  if (m_flags & 0x40000u) _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
  EAnimController& anim = m_sim->m_anim;
  EACTrack* t = anim.SetTrackAnim(eTrackFlags{1}, AnimRefId(ref), -1, kDefaultBlendTime, nullptr);
  m_flags = (m_flags & ~0x40000u) | 0x1000000u;
  if (!t) return;
  t->m_endMode = 1;
  anim.SetTrackPos(t, 0.0f, true);
  anim.SetTrackSpeed(t, m_timeMult * m_pauseMult);
}

// 80064538
// An impatient idle still loading gives its reference back.
void SAnimator2::clearImpatientIdleAnimation() {
  if (!(m_flags & 0x40000u)) return;
  m_flags &= ~0x40000u;
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, 0x41, ref);
  if (!ref) return;
  if (!_animman.GetRef(AnimRefId(ref))) _animman.DelRefAsync(AnimRefId(ref));
  else _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
}

// 800627C0
// Walking or running under direct control: the style's cycle on the walk track (flag 2), cross-faded
// over kWalkRunBlendTime (a running start begins half way, on the other foot); the cycle's speed is
// read once from the animation's root motion. The sim moves along its direction at the style's
// velocity (faster zoomed out, blended with the previous style while the cross-fade runs), stopped or
// slid by walls and objects (CheckCollision); the person's location follows (GC: CanPlace / Place,
// vtable 0x134 / 0x13C; PC: any tile on the lot).
void SAnimator2::handleFreeMoveWalkRunAnimation() {
  EAnimController& anim = m_sim->m_anim;
  const unsigned int playing = anim.GetTrackAnimId(eTrackFlags{eTrackFlags::kMain});
  const float dt = ScaledDt();
  int style = 8;
  if (m_animState == kFreeWalk) style = 7;
  else if (m_animState == kFreeRun && m_lowMotive != 0) SIMMY_STUB("SAnimator2: low-motive runs", 0x800627C0);
  if (style != m_walkStyle) {
    m_prevWalkStyle = m_walkStyle;
    m_walkStyle = style;
  }
  EVec2 from{m_pos[0], m_pos[1]};
  stopIdleOverlay();
  const AnimRef* ref = nullptr;
  getWalkRunSkillID(ref);
  if (ref && AnimRefId(ref) != playing) {
    if (m_skillOverlay && !(m_skillOverlay->m_flags & 0x80000)) {
      anim.SetTrackBlendSmooth(m_skillOverlay, 0.0f, kFastRemoveBlendTime, 0.01f);
      m_skillOverlay = nullptr;
    }
    float* velocity;
    unsigned int bit;
    if (m_walkStyle == 2) velocity = &m_lowEnergyVel, bit = 4;
    else if (m_walkStyle == 3) velocity = &m_lowBladderVel, bit = 8;
    else if (m_walkStyle == 1 || m_walkStyle == 8) velocity = &m_runVel, bit = 2;
    else velocity = &m_walkVel, bit = 1;
    if (!(m_velocityKnown & bit) && _animman.GetRef(AnimRefId(ref))) {
      // The cycle's speed: its root's forward (y) distance over its length.
      if (auto* a = static_cast<ERAnim*>(_animman.AddRef(AnimRefId(ref), nullptr, 0))) {
        const float frames = static_cast<float>(a->m_frameCount - 1);
        const float length = (frames / a->m_fps) * (static_cast<float>(a->m_frameCount - 2) / frames);
        if (length != 0.0f) {
          EACTrack t;
          t.m_anim = a;
          anim.CreateStreams(&t);
          t.m_pos = static_cast<float>(a->m_frameCount - 2);
          const EVec3 root = anim.GetAnimRootNodeTrans(&t);
          if (0.2f < root.y) *velocity = root.y * 16.0f / length;
          anim.DestroyStreams(&t);
          t.m_anim = nullptr;
        }
        _animman.DelRef(a, EResourceManager__DelRefMode{1});
        m_velocityKnown |= bit;
      }
    }
    EACTrack* t = anim.SetTrackAnim(eTrackFlags{2}, AnimRefId(ref), 2, kWalkRunBlendTime, nullptr);
    if (m_shuffleDir < 0) anim.SetTrackPos(t, 0.5f, true);
    m_eventFrame = 0;
    m_eventFraction = 0.0f;
    m_curAnim = ref;
    m_flags |= 0x10000;
  }
  float v = StyleVelocity(m_walkStyle);
  v = v * m_zoomRatio * 0.5f + v;
  if (m_walkStyle == 7) anim.SetTrackSpeed(eTrackFlags{eTrackFlags::kMain}, m_timeMult * v / m_walkVel);
  else if (m_walkStyle == 8) anim.SetTrackSpeed(eTrackFlags{eTrackFlags::kMain}, m_timeMult * v / m_runVel);
  m_walkBlend = 0.0f;
  if (EACTrack* out = OutgoingTrack()) {
    m_walkBlend = 1.0f - out->m_intensity;
    const float prev = (out->m_flags & 2) ? StyleVelocity(m_prevWalkStyle) : 0.0f;
    v = (v - prev) * m_walkBlend + prev;
  }
  const float c = std::cos(-m_dir), s = std::sin(-m_dir);
  m_pos[0] += c * dt * v;
  m_pos[1] += s * dt * v;
  EVec2 to{m_pos[0], m_pos[1]};
  CheckCollision(from, to);
  m_pos[0] = to.x, m_pos[1] = to.y;
  cXPersonImpl* impl = Impl(m_person);
  CTilePt tile(static_cast<int>(m_pos[0] + 0.5f) >> 4, static_cast<int>(m_pos[1] + 0.5f) >> 4, impl->m_level);
  if (!Globs_pFixedWorld || Globs_pFixedWorld->OutOfBounds(tile)) {
    m_pos[0] = from.x, m_pos[1] = from.y;
  } else {
    impl->m_location[1] = static_cast<int>(m_pos[0] + 0.5f);
    impl->m_location[0] = static_cast<int>(m_pos[1] + 0.5f);
    UpdateObjectRoomID(impl);  // GC: cXObjectImpl::SetLocation
  }
}

// 800625A0
// The move from `from` to `to` (animation format) as a circle of radius 0.24 tiles in the world's
// frame: slid along the walls it meets (and, if that slide hits a wall, stopped there); then, unless
// a ghost, the same against objects, the slide checked once more against walls and objects.
void SAnimator2::CheckCollision(EVec2& from, EVec2& to) {
  const float r = 0.24f;
  EVec2 a{from.y * 0.0625f, from.x * 0.0625f};
  EVec2 b{to.y * 0.0625f, to.x * 0.0625f};
  EVec2 hit;
  const bool ghost = m_person->IsGhost() != 0;
  if (CollisionMap::DoesCollideEnvironment(a, b, r, true, ghost, hit)) {
    b = hit;
    if (CollisionMap::DoesCollideEnvironment(a, b, r, false, ghost, hit)) b = hit;
  }
  if (!ghost && CollisionMap::DoesCollideObject(a, b, r, true, hit)) {
    b = hit;
    if (CollisionMap::DoesCollideEnvironment(a, b, r, false, ghost, hit)) b = hit;
    if (CollisionMap::DoesCollideObject(a, b, r, false, hit)) b = hit;
  }
  // PC: two people steered at once (two-player) do not walk through each other: a move into another
  // standing person's circle (both radii) slides round its edge. GC: DoesCollideObject skips people
  // (IsPerson) and nothing else stops them; the routes plan round people (XRoute's penalty rects).
  static const bool collide = !std::getenv("SIMMY_SIM_COLLIDE") || std::atoi(std::getenv("SIMMY_SIM_COLLIDE")) != 0;
  cXObjectImpl* self = Impl(m_person);
  if (collide && !ghost && self && Globs_pObjectModule) {
    ObjectModuleImpl* module = Globs_pObjectModule;
    for (int id = 1; id <= module->GetNumObjects(); ++id) {
      cXObject* x = module->GetObjectFromID(id);
      auto* o = x ? dynamic_cast<cXPersonImpl*>(x->GetObjectImplementation()) : nullptr;
      if (!o || o == self || o->GetType() != 2 || o->m_level != self->m_level || o->GetContainer()) continue;
      const EVec2 p{o->m_location[0] * 0.0625f, o->m_location[1] * 0.0625f};
      const float dx = b.x - p.x, dy = b.y - p.y, d2 = dx * dx + dy * dy, min = 2.0f * r;
      const float ax = a.x - p.x, ay = a.y - p.y;
      if (d2 >= min * min || d2 >= ax * ax + ay * ay) continue;  // clear, or moving away
      const float d = std::sqrt(d2);
      if (d < 1e-4f) {
        b = a;
      } else {
        b.x = p.x + dx / d * min;
        b.y = p.y + dy / d * min;
      }
      if (CollisionMap::DoesCollideEnvironment(a, b, r, false, ghost, hit)) b = hit;
    }
  }
  to.x = b.y * 16.0f;
  to.y = b.x * 16.0f;
}

// ---------------------------------------------------------------------------------------------------
// Skills: the animate primitive (SimAntics 0x2C) and animation events

namespace {
const float kIdleToSkillBlendTime = 0.3f;  // 804FDAE0
}  // namespace

// 8005D394
// Operand +0 the animation (with flag 4 the index is param N of the frame, of the frame's stack
// object), +4 its table: 0 the stack object's own (GetObjectAnimRef), 1 global, 2 person stock, 3 misc.
int SAnimator2::resolveSkillForPrimitive(StackElem* frame, const unsigned char* params, const AnimRef*& out) {
  out = nullptr;
  cXPersonImpl* impl = Impl(m_person);
  int index = static_cast<short>(simmy::be16(params));
  cXObject* object = nullptr;
  if (params[5] & 4) {
    if (index < 0 || index >= frame->m_numParams) {
      impl->m_treeSim->SetError(8);
      impl->Error(8);
      return -1;
    }
    index = frame->GetParams()[index];
    object = impl->GetObjectFromID(frame->m_stackObject);
  } else {
    // The object whose tree the person runs (cXPerson::GetControllingObject, vtable 0x2EC).
    object = impl->GetControllingObject();
  }
  switch (static_cast<signed char>(params[4])) {
    case 0: {
      if (!object) {
        impl->m_treeSim->SetError(0x17);
        impl->Error(0x17);
        return -1;
      }
      int kind = params[3];
      if (kind == 0) kind = ((params[5] & 1) ? 1 : 0) | ((params[5] & 0x10) ? 2 : 0);
      else kind += 3;
      return GetObjectAnimRef(object, m_person, index, kind == 0, out) == -1 ? -1 : 1;
    }
    case 1: return GetGlobalAnimRef(m_person, index, out) == -1 ? -1 : 1;
    case 2: return GetPersonStockAnimRef(m_person, index, out) == -1 ? -1 : 1;
    case 3: return GetMiscAnimRef(m_person, index, out) == -1 ? -1 : 1;
    default: return 1;
  }
}

// 8005D164
int SAnimator2::resolveIdleSkillForPrimitive(StackElem* frame, const unsigned char* params, const AnimRef*& out) {
  out = nullptr;
  cXPersonImpl* impl = Impl(m_person);
  int index = static_cast<short>(simmy::be16(params));
  cXObject* object = nullptr;
  if (params[5] & 2) {
    if (index < 0 || index >= frame->m_numParams) {
      impl->m_treeSim->SetError(8);
      impl->Error(8);
      return -1;
    }
    index = frame->GetParams()[index];
    object = impl->GetObjectFromID(frame->m_stackObject);
  } else {
    object = impl->GetControllingObject();
  }
  switch (static_cast<signed char>(params[4])) {
    case 0:
      if (!object) {
        impl->m_treeSim->SetError(0x17);
        impl->Error(0x17);
        return -1;
      }
      return GetObjectAnimRef(object, m_person, index, true, out) == -1 ? -1 : 1;
    case 1: return GetGlobalAnimRef(m_person, index, out) == -1 ? -1 : 1;
    case 2: return GetPersonStockAnimRef(m_person, index, out) == -1 ? -1 : 1;
    case 3: return GetMiscAnimRef(m_person, index, out) == -1 ? -1 : 1;
    default: return 1;
  }
}

// 8005D6D4
// IdleAnimateParam: +0 the animation (0 with flags 2 and 4 clear: the current animation keeps playing as
// an idle, main track flags 0x10020), +2 an object to sync with, +4 the table, +5 flags (1 backwards, 2
// the animation from a param, 4 an algorithmic idle when the skill track is done, 8 with an idle
// overlay when standing), +6 how the caller counts (2: loops). The animation plays on the algorithmic
// idle track (0x20). PC: syncing objects (vtable 0xC4) is not ported.
int SAnimator2::TryIdleAnimate(StackElem* frame, const unsigned char* params) {
  if (m_pendingAnim != 0) {
    if (!_animman.GetRef(static_cast<unsigned int>(m_pendingAnim))) return 2;
    _animman.DelRef(static_cast<unsigned int>(m_pendingAnim), EResourceManager__DelRefMode{1});
    m_pendingAnim = 0;
  }
  if (!m_sim || !m_sim->m_modelReady) return 2;
  EAnimController& ctl = m_sim->m_anim;
  const int anim = static_cast<short>(simmy::be16(params));
  const unsigned char flags = params[5];
  int r;
  const AnimRef* ref = nullptr;
  if (anim == 0 && !(flags & 2) && !(flags & 4)) {
    if (ctl.m_mainTrack != -1) {
      EACTrack& t = ctl.m_tracks[static_cast<size_t>(ctl.m_mainTrack)];
      t.m_flags = (t.m_flags & 0xffffff00u) | 0x10020u;
    }
    m_flags |= 0x200000u;
    r = 1;
  } else {
    m_flags &= ~0x200000u;
    if (!(flags & 4)) {
      r = resolveIdleSkillForPrimitive(frame, params, ref);
      if (r == 1 && ref && AnimRefId(ref) == ctl.GetTrackAnimId(eTrackFlags{0x10000})) return 1;
      if (r == 1) r = loadSkillAnim(ref);
    } else {
      if (!ctl.IsTrackAnimComplete(eTrackFlags{0x10000})) return 1;
      r = SelectAlgorithmicIdle(ref);
    }
    if (params[2] > 0) SIMMY_STUB("SAnimator2::TryIdleAnimate: syncing an object (vtable 0xC4)", 0x8005D6D4);
    if (r == -1) return -1;
    if (r == 1 && ref) {
      const bool restart = !(m_skillSuspended == 1 && m_suspendedSkill == AnimRefId(ref));
      stopCurAnim(true);
      if (!startSkill(ref, (flags & 1) != 0, 0x20)) return 2;
      if (params[6] == 2 && ctl.m_mainTrack != -1) ctl.m_tracks[static_cast<size_t>(ctl.m_mainTrack)].m_endMode = 0;
      if (restart) m_loopCount = 0;
    }
  }
  if (r == 1) {
    cXPersonImpl* p = Impl(m_person);
    const int idle = p ? p->GetIdleState() : 0;
    if (!(flags & 8) || (idle != 0 && idle != 3)) {
      stopIdleOverlay();
      m_flags |= 0x2000u;
    } else {
      m_flags &= ~0x2000u;
      selectIdleOverlay();
    }
  }
  return r;
}

// 8005DA24
// The idle for the person's idle state; with a low-motive idle (+0x5FA) that one (LoadOptionalIdleAnim).
int SAnimator2::SelectAlgorithmicIdle(const AnimRef*& out) {
  out = nullptr;
  if (m_pendingAnim != 0) {
    if (!_animman.GetRef(static_cast<unsigned int>(m_pendingAnim))) return 0;
    _animman.DelRef(static_cast<unsigned int>(m_pendingAnim), EResourceManager__DelRefMode{1});
    m_pendingAnim = 0;
  }
  cXPersonImpl* p = Impl(m_person);
  m_idleState = p ? p->GetIdleState() : 0;
  int idle = -1;
  if (m_idleState == 1) idle = 1;
  else if (m_idleState == 2) idle = 2;
  else if (m_idleState == 0 || m_idleState == 3) idle = selectRandomIdle();
  if (idle != -1) m_impatientTime = 0.0f;
  if (idle == -1) return 0;
  if (m_lowMotiveIdle != 0) {
    // The low-motive idle instead (loaded asynchronously: false either way, as GC).
    out = reinterpret_cast<const AnimRef*>(LoadOptionalIdleAnim());
    return 0;
  }
  GetStdAnimRef(m_person, idle, out);
  return loadSkillAnim(out);
}

// 8005D5FC
int SAnimator2::loadSkillAnim(const AnimRef* ref) {
  const unsigned int id = AnimRefId(ref);
  if (!ref || id == 0 || _animman.IsLoaded(id)) return 1;
  m_pendingAnim = static_cast<int>(id);
  PreloadEvents(const_cast<AnimRef*>(ref));
  _animman.AddRefAsync(id);
  return 2;
}

// 8005DBA0
// Operands: +0 the animation (0: stop), +2 the local for events, +3 the kind (0: by flags), +4 the
// table, +5 flags (1 idle track, 2 backwards, 4 animation from a param, 0x10 carry, 0x20 events into
// the local, else param 0; 0x80 preload only), +6 loops, +7 an object to sync with. The kind: 0 a
// skill, 1 the idle track, 2 / 5 carry, 3 a skill over a carry, 4 the fx track (0x8000), 6 an
// algorithmic idle. A skill plays on track 0x10 until it ends (true), handing each animation event
// (>= 100) and each loop to the tree as false with its number. PC: carrying and the fx track are not
// ported (logged, true), nor objects syncing (SyncObjectIsReady, vtable 0xC4).
int SAnimator2::TryAnimate(StackElem* frame, const unsigned char* params) {
  int kind = params[3];
  if (kind == 0) kind = ((params[5] & 1) ? 1 : 0) | ((params[5] & 0x10) ? 2 : 0);
  else kind += 3;
  if (!m_sim || !m_sim->m_modelReady) return 2;
  if (m_pendingAnim != 0) {
    if (!_animman.GetRef(static_cast<unsigned int>(m_pendingAnim))) return 2;
    _animman.DelRef(static_cast<unsigned int>(m_pendingAnim), EResourceManager__DelRefMode{1});
    m_pendingAnim = 0;
  }
  const AnimRef* ref = nullptr;
  if (params[5] & 0x80) {
    if (resolveSkillForPrimitive(frame, params, ref) == -1) return 1;
    PreloadEvents(const_cast<AnimRef*>(ref));
    if (ref && AnimRefId(ref)) _animman.AddRefAsync(AnimRefId(ref));
    return 1;
  }
  int anim = static_cast<short>(simmy::be16(params));
  // GC: ESim::HasQueuedOperation / a model change in flight (+0x630) waits.
  m_flags &= ~0x2000u;
  if (params[5] & 4) anim = frame->GetParams()[0];
  m_sim->DrawSim(true);
  EAnimController& ctl = m_sim->m_anim;
  int result = 1;
  if (kind == 1) {
    if (anim == 0) {
      if (ctl.IsTrackValid(eTrackFlags{1})) SIMMY_STUB("SAnimator2::TryAnimate: stop the idle track (vtable 0x114)", 0x8005DBA0);
    } else if (ctl.IsTrackAnimComplete(eTrackFlags{0x10000})) {
      const int idle = selectRandomIdle();
      GetStdAnimRef(m_person, idle, ref);
      if (loadSkillAnim(ref) != 1) return 2;
      m_eventFrame = 0;
      m_flags |= 0x10000;
      m_eventFraction = 0.0f;
      m_curAnim = ref;
      if (EACTrack* t = ctl.SetTrackAnim(eTrackFlags{1}, AnimRefId(ref), -1, -1.0f, nullptr)) {
        ctl.SetTrackSpeed(t, m_timeMult);
        t->m_endMode = 0;
      }
    }
  } else if (kind == 4) {
    // The fx track (0x8000): the animation on the fx node only, at full weight.
    if (anim != 0) {
      const int r = resolveSkillForPrimitive(frame, params, ref);
      if (r == -1) {
        UpdateAnimationValidity();
        return -1;
      }
      result = r == 1 ? loadSkillAnim(ref) : r;
      if (result == 1 && ref) {
        if (m_overlayAnim) m_flags |= 0x20000;
        EACTrack* t = ctl.SetTrackAnim(eTrackFlags{0x8000}, AnimRefId(ref), 0, -1.0f, nullptr);
        if (t) {
          ctl.SetTrackIntensity(t, 1.0f, true);
          ctl.SetTrackBlendFactors(t, m_FXNodeIndexArray);
          if (!(params[5] & 2)) {
            m_flags &= ~0x200u;
            ctl.SetTrackSpeed(t, m_timeMult);
          } else {
            m_flags |= 0x200;
            ctl.SetTrackSpeed(t, -m_timeMult);
            ctl.SetTrackPos(t, 1.0f, true);
          }
        }
        m_overlayEventFrame = 0;
        m_overlayAnim = ref;
      }
      UpdateAnimationValidity();
      return result;
    }
    if (ctl.IsTrackValid(eTrackFlags{0x8000})) {
      ctl.SetTrackIntensity(eTrackFlags{0x8000}, 0.0f);
      m_flags |= 0x20000;
    }
    m_overlayAnim = nullptr;
  } else if (kind == 2 || kind == 5) {
    // The carry track (0x800): the animation on the right arm's nodes (both arms for the hands-up
    // carry), faded in over kCarryBlendTime for kind 5; animation 0 fades it out.
    const float blend = kind == 5 ? kCarryBlendTime : 0.0f;
    if (anim != 0) {
      const int r = resolveSkillForPrimitive(frame, params, ref);
      if (r == -1) {
        UpdateAnimationValidity();
        return -1;
      }
      result = r == 1 ? loadSkillAnim(ref) : r;
      if (result == 1 && ref) {
        EACTrack* t = ctl.SetTrackAnim(eTrackFlags{0x800}, AnimRefId(ref), -1, -1.0f, nullptr);
        if (t) {
          ctl.SetTrackIntensity(t, 0.0001f, true);
          ctl.SetTrackBlendSmooth(t, 1.0f, blend, 0.01f);
          if (AnimRefId(ref) == 0xf5548783u) lockHandsUpCarryNodes();
          else lockCarryArmNodes(m_RightArmCarryIndexArray, t);
          if (!(params[5] & 2)) {
            ctl.SetTrackSpeed(t, m_timeMult);
          } else {
            ctl.SetTrackSpeed(t, -m_timeMult);
            ctl.SetTrackPos(t, 1.0f, true);
          }
        }
        m_flags = (m_flags & ~0x800u) | 0x4000;
      }
      UpdateAnimationValidity();
      return result;
    }
    if (ctl.IsTrackValid(eTrackFlags{0x800})) ctl.SetTrackBlendSmooth(eTrackFlags{0x800}, 0.0f, blend, 0.01f);
    if (m_flags & 0x800) m_flags = (m_flags & ~0x800u) | 0x8000;
    else m_flags &= ~0x4000u;
  } else if (anim != 0) {
    m_animatePrim = 1;
    if (!m_skillRunning) {
      int r;
      if (kind != 6) {
        r = resolveSkillForPrimitive(frame, params, ref);
        if (r == -1) return -1;
        if (r == 1) r = loadSkillAnim(ref);
      } else {
        r = SelectAlgorithmicIdle(ref);
      }
      if (r != 1) return r;
      if (!ref) return 1;
      const bool restart = !(m_skillSuspended == 1 && m_suspendedSkill == AnimRefId(ref));
      stopCurAnim(restart);
      if (!startSkill(ref, (params[5] & 2) != 0, kind == 6 ? 0x20 : 0x10)) return 2;
      if (kind == 3) {
        // A skill over a carry: the carry pose fades out while the skill blends in (twice the
        // outgoing track's blend time) and comes back when it ends.
        m_flags |= 0x400;
        float time = 0.0f;
        if (ctl.m_blendCount != 0 && !ctl.m_blendList.empty() && ctl.m_blendList[0] >= 0)
          time = ctl.m_tracks[static_cast<size_t>(ctl.m_blendList[0])].m_blendTime * 2.0f;
        ctl.SetTrackBlendSmooth(eTrackFlags{0x800}, 0.0001f, time, 0.01f);
      }
      m_skillRunning = 1;
      if (restart) m_loopCount = 0;
    }
    const int loops = static_cast<signed char>(params[6]);
    int event = 0;
    if (!DequeueAnimEvent(&event)) {
      if (!isAnimationDone()) return 2;
      if (loops <= m_loopCount) {
        m_skillRunning = 0;
        resumeCarryAfterSkill();
        Impl(m_person)->SetPersonData(0x13, 0);
        return 1;
      }
      event = m_loopCount;
      if (params[5] & 2) event = loops - event - 1;
      ++m_loopCount;
    } else if (event < 100) {
      // A loop marker: only the next loop's.
      int expect = (params[5] & 2) ? loops - m_loopCount - 1 : m_loopCount;
      if (loops <= expect || expect < 0) expect = -1;
      if (event != expect || expect < 0) return 2;
      ++m_loopCount;
      event = expect;
    }
    if (params[5] & 0x20) {
      if (params[2] >= frame->m_numLocals) {
        Impl(m_person)->m_treeSim->SetError(8);
        Impl(m_person)->Error(8);
        return -1;
      }
      frame->GetLocals()[params[2]] = static_cast<short>(event);
    } else {
      if (frame->m_numParams == 0) {
        Impl(m_person)->m_treeSim->SetError(8);
        Impl(m_person)->Error(8);
        return -1;
      }
      frame->GetParams()[0] = static_cast<short>(event);
    }
    return 0;
  } else {
    m_skillRunning = 0;
    stopCurAnim(true);
    Impl(m_person)->SetPersonData(0x13, 0);
    resumeCarryAfterSkill();
  }
  UpdateAnimationValidity();
  return result;
}

// PC helper (inlined twice in TryAnimate): a skill played over a carry (flag 0x400) ended; the carry
// track resumes and fades back in.
void SAnimator2::resumeCarryAfterSkill() {
  if (!(m_flags & 0x400)) return;
  m_flags &= ~0x400u;
  EAnimController& ctl = m_sim->m_anim;
  ctl.ResumeTrack(eTrackFlags{0x800});
  ctl.SetTrackBlendSmooth(eTrackFlags{0x800}, 1.0f, kDefaultBlendTime, 0.01f);
}

// 80066490
int SAnimator2::startSkill(const AnimRef* ref, bool backwards, unsigned int track) {
  if (!m_sim->m_modelReady) return 0;
  m_flags |= 0x10000;
  const unsigned int id = AnimRefId(ref);
  EResource* res = (m_skillSuspended == 1 && m_suspendedSkill == id) ? _animman.AddRef(m_suspendedSkill, nullptr, 0)
                                                                      : _animman.GetRef(id);
  const int duration = res ? static_cast<ERAnim*>(res)->m_durationMs : 0;
  float resume = 0.0f;
  m_curAnim = ref;
  if (m_skillSuspended == 1 && m_suspendedSkill == id) {
    if (duration != 0) resume = std::min(1.0f, std::max(0.0f, static_cast<float>(m_eventFrame) / duration));
    _animman.DelRef(id, EResourceManager__DelRefMode{1});
  } else {
    m_eventFrame = 0;
    m_eventFraction = 0.0f;
  }
  m_suspendedSkill = 0;
  m_skillSuspended = 0;
  m_skillAnimId = id;
  EAnimController& ctl = m_sim->m_anim;
  const float blend = (MainTrackFlags() & 0x21) ? kIdleToSkillBlendTime : -1.0f;
  EACTrack* t = ctl.SetTrackAnim(eTrackFlags{static_cast<int>(track)}, id, -1, blend, nullptr);
  if (t) {
    if (backwards) {
      ctl.SetTrackSpeed(t, -m_timeMult);
      ctl.SetTrackPos(t, 1.0f, true);
    } else {
      ctl.SetTrackSpeed(t, m_timeMult);
      ctl.SetTrackPos(t, resume, true);
    }
  }
  if (backwards) m_flags |= 0x100;
  else m_flags &= ~0x100u;
  m_dir = static_cast<float>(getPersonDirection());
  return 1;
}

// 800667F0
// GC: also done under player control when an algorithmic idle plays and actions are queued.
int SAnimator2::isAnimationDone() {
  if (!m_curAnim) return 0;
  if (!m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{0x30})) return 0;
  EResource* res = _animman.GetRef(AnimRefId(m_curAnim));
  const int duration = res ? static_cast<ERAnim*>(res)->m_durationMs : 0;
  if (m_eventFrame <= duration) {
    processEvents(m_curAnim, m_eventFrame, -1, (m_flags & 0x100) != 0, false);
    m_eventFrame = duration + 1;
  }
  m_skillAnimId = 0;
  return m_animEvents.empty() ? 1 : 0;
}

// 80066940
void SAnimator2::stopCurAnim(bool reset) {
  if (reset) {
    m_eventFrame = 0;
    m_eventFraction = 0.0f;
  }
  m_curAnim = nullptr;
  m_animEvents.clear();
  m_flags &= ~0x100u;
}

// 8005EDBC
int SAnimator2::DequeueAnimEvent(int* event) {
  if (m_animEvents.empty()) return 0;
  *event = m_animEvents.front();
  m_animEvents.erase(m_animEvents.begin());
  return 1;
}

// 80065668
// A note's time (ms; footsteps, type 5, 150 ms early) inside the window is handled; backwards the
// window runs from the end.
void SAnimator2::processEvents(const AnimRef* ref, int from, int length, bool backwards, bool overlay) {
  EResource* res = ref ? _animman.GetRef(AnimRefId(ref)) : nullptr;
  if (overlay) m_flags |= 0x80;
  else m_flags &= ~0x80u;
  if (!res) return;
  ERAnim* anim = static_cast<ERAnim*>(res);
  const int count = static_cast<int>(anim->m_notes.size());
  auto timeOf = [&](const EAnimNote& n) { return std::max(0, static_cast<int>(n.m_time) - (n.m_field04 == 5 ? 0x96 : 0)); };
  if (!backwards) {
    for (int i = 0; i < count; ++i) {
      const int t = timeOf(anim->m_notes[i]);
      if (from <= t && (length < 0 || t < from + length)) eventHandler(anim->m_notes[i], i);
    }
  } else {
    const int end = anim->m_durationMs - from;
    for (int i = count - 1; i >= 0; --i) {
      const int t = timeOf(anim->m_notes[i]);
      if (t <= end && (length < 0 || end - length < t)) eventHandler(anim->m_notes[i], i);
    }
  }
}

bool IsUsingDirectControlInteractor(int player);  // inlevel.cpp (the same test as 8005BDA4)

namespace {
// 8005BD6C
int GetSimIndex(cXPerson* person) {
  if (_globals.GetSelectedPerson(0) == person) return 0;
  if (_globals.GetSelectedPerson(1) == person) return 1;
  return -1;
}

// 8005BE08
// A frame-effect note of a player's selected sim, while it is in social mode, goes to that player's
// camera director; in two-player mode also to the other player's when its sim is the social target
// and it is in direct control.
void HandleEffectNote(cXPerson* person, EAnimNote& note, int kind) {
  const int player = person ? GetSimIndex(person) : -1;
  if (player < 0) return;
  auto* impl = dynamic_cast<cXPersonImpl*>(_globals.GetSelectedPerson(player));
  if (!impl || !impl->IsInSocialMode()) return;
  auto handle = [&](int p) {
    CameraDirector* d = _globals.CameraDirectorOf(p);
    FrameEffectsManager* fx = d ? d->GetFrameEffectsManager() : nullptr;
    if (kind == 0xe) HandleMotionBlurEvent(note, fx);
    else if (kind == 0xf) HandleDepthOfFieldEvent(note, fx);
    else HandleBloomEvent(note, fx);
  };
  handle(player);
  if (_gd.m_fieldEC) {
    const int other = (player + 1) % 2;
    auto* targetImpl = reinterpret_cast<cXPersonImpl*>(impl->GetSocialModeTarget());
    cXPerson* otherSim = _globals.GetSelectedPerson(other);
    if (targetImpl && otherSim == static_cast<cXPerson*>(targetImpl) && IsUsingDirectControlInteractor(other)) handle(other);
  }
}
}  // namespace

// 80065818
// Note kinds: 1 a footstep, 5 a sound, 6 an event for the tree ('xevt', its number), 7 a rumble, 8
// nothing, 9 the interaction's object told (vtable 0x9C), 0xA a particle effect, 0xD..0xF effects.
// PC: events, sounds, footsteps, particles and frame effects are ported (rumble and kind 9 are logged).
void SAnimator2::eventHandler(EAnimNote& note, int) {
  switch (note.m_field04) {
    case 6: m_animEvents.push_back(std::atoi(note.m_text.c_str())); break;
    case 0: case 8: break;
    case 1: footstepEvent(const_cast<char*>(note.m_text.c_str())); break;
    case 5:  // a sound, for the sim
      if (Globs_pSound && m_person && m_person->AsObject())
        Globs_pSound->PlayBySource(const_cast<char*>(note.m_text.c_str()), static_cast<short>(m_person->AsObject()->GetID()));
      break;
    case 7: SIMMY_STUB("SAnimator2::eventHandler: rumble notes (playRumble)", 0x80065818); break;
    case 0xd: case 0xe: case 0xf: HandleEffectNote(m_person, note, static_cast<int>(note.m_field04)); break;
    case 0xa:  // a particle effect: the attachment by name, toggled on the sim's bones (boneparticle.cpp)
      if (EResource* att = g_effectsAttachmentMan.AddRef(const_cast<char*>(note.m_text.c_str()), nullptr, 0)) {
        if (!static_cast<REffectsAttachment*>(att)->m_nodes.empty()) _handleParticleEvent(static_cast<REffectsAttachment*>(att));
        att->DelRef();
      }
      break;
    default: SIMMY_STUB("SAnimator2::eventHandler: notes kind 9 (the interaction's object, vtable 0x9C)", 0x80065818); break;
  }
}

// 800689FC
// GC: the instance's "animation valid" flag (ESim +0x394) follows the main track.
void SAnimator2::UpdateAnimationValidity() {}

// 8005ED9C
int SAnimator2::IsFollowing() { return m_routing != 0 ? 1 : 0; }

// 8005EDB4
// A walk can always be interrupted (TryGotoRoutingSlot asks when the person should give way).
RET_UNKNOWN SAnimator2::IsInterruptable() { return 1; }

// ---------------------------------------------------------------------------------------------------
// Routing: walking a path

namespace {
// The facing (radians) of the move from `a` to `b` (animation format): moving along +x is 0.
float HeadingTo(const EVec2& a, const EVec2& b) { return -std::atan2(b.y - a.y, b.x - a.x); }
}  // namespace

// PC (GC 8005E7CC BeginFollow: the person's destination list, the first follow mode by the first
// leg (1 a turn on the spot first, 2 a side step for a short one), the walk style (low motives walk
// slowly, long routes run: StartAutoRun), the start at the person's location).
// Follow modes used: 1 turning toward the first leg, 3 walking, 5 turning to the end facing.
void SAnimator2::BeginRoute(const std::vector<EVec2>& points, float endDir) {
  if (m_pendingAnim != 0) {
    _animman.DelRef(static_cast<unsigned int>(m_pendingAnim), EResourceManager__DelRefMode{1});
    m_pendingAnim = 0;
  }
  m_sim->DrawSim(true);
  m_route = points;
  m_routeNode = 1;
  m_routeEndDir = endDir;
  m_routeResult = 0;
  m_dir = static_cast<float>(getPersonDirection());
  m_pos[0] = static_cast<float>(getPersonX());
  m_pos[1] = static_cast<float>(getPersonY());
  m_resetPos = 0;
  m_routeIndex = 0;
  if (m_walkStyle != 0) {
    m_prevWalkStyle = m_walkStyle;
    m_walkStyle = 0;
  }
  m_routing = 1;
  m_followMode = 1;
  m_flags &= ~0x50u;
  if (m_route.size() < 2) m_followMode = 5;
}

void SAnimator2::AbortRoute() {
  if (m_routing != 1) return;
  m_routing = 0;
  m_followMode = 0;
  m_routeResult = -1;
  m_route.clear();
  setPersonDirection(m_dir);
}

// 8006028C
// One frame along the route (GC: the follow modes' updates, sidesteps, portals, awareness): turn on
// the spot toward the leg (a shuffle), walk it at the walk style's velocity turning into each new leg
// as it goes, and at the end turn to the goal's facing; the person's location follows.
void SAnimator2::moveAnimation() {
  const float dt = ScaledDt();
  cXPersonImpl* impl = Impl(m_person);
  auto finish = [&] {
    m_routing = 0;
    m_followMode = 0;
    m_routeResult = 1;
    m_route.clear();
    m_flags &= ~0x50u;
    setPersonDirection(m_dir);
  };
  auto place = [&] {
    impl->m_location[1] = static_cast<int>(m_pos[0] + 0.5f);
    impl->m_location[0] = static_cast<int>(m_pos[1] + 0.5f);
    UpdateObjectRoomID(impl);  // GC: cXObjectImpl::SetLocation
  };
  if (m_followMode == 1) {
    const float desired = HeadingTo(EVec2{m_pos[0], m_pos[1]}, m_route[m_routeNode]);
    const float delta = static_cast<float>(getDeltaDirFromDesiredDir(desired));
    if (std::fabs(delta) < 0.3926991f) {
      m_dir = desired;
      m_followMode = 3;
      m_flags &= ~0x50u;
    } else {
      if (!(m_flags & 0x40)) initShuffleRotation(delta);
      if (rotateAnimation(dt, desired, delta)) {
        m_followMode = 3;
        m_flags &= ~0x50u;
      }
      return;
    }
  }
  if (m_followMode == 3) {
    float remaining = dt * StyleVelocity(m_walkStyle);
    if (EACTrack* out = OutgoingTrack()) remaining *= 1.0f - out->m_intensity * 0.5f;
    while (remaining > 0.0f && m_routeNode < m_route.size()) {
      const EVec2& target = m_route[m_routeNode];
      const float dx = target.x - m_pos[0], dy = target.y - m_pos[1];
      const float dist = std::sqrt(dx * dx + dy * dy);
      if (dist <= remaining) {
        m_pos[0] = target.x, m_pos[1] = target.y;
        remaining -= dist;
        ++m_routeNode;
        continue;
      }
      m_pos[0] += dx / dist * remaining;
      m_pos[1] += dy / dist * remaining;
      remaining = 0.0f;
    }
    if (m_routeNode < m_route.size()) {
      // Turn into the leg while walking (at the turn rate a walk turns: 3 pi a second).
      const float desired = HeadingTo(EVec2{m_pos[0], m_pos[1]}, m_route[m_routeNode]);
      const float delta = Wrap(desired - m_dir);
      const float step = dt * 9.424778f;
      m_dir = std::fabs(delta) <= step ? desired : Wrap(m_dir + (delta > 0.0f ? step : -step));
    } else {
      m_followMode = 5;
      m_flags &= ~0x50u;
    }
    place();
    setPersonDirection(m_dir);
    if (m_followMode == 3) return;
  }
  if (m_followMode == 5) {
    place();
    if (m_routeEndDir == kNoDir) {
      finish();
      return;
    }
    const float delta = static_cast<float>(getDeltaDirFromDesiredDir(m_routeEndDir));
    if (delta == 0.0f) {
      finish();
      return;
    }
    if (!(m_flags & 0x40)) initShuffleRotation(delta);
    if (rotateAnimation(dt, m_routeEndDir, delta)) finish();
  }
}

// 800620AC
// Walking a route (follow mode 3): the style's walk (or run) cycle on the walk track (flag 2),
// cross-faded over kWalkRunBlendTime, at the simulator's speed. GC: also low-motive walks, a
// slowed-down end of the route and the walk's events.
void SAnimator2::handleWalkRunAnimation() {
  if (m_followMode != 3) return;
  EAnimController& anim = m_sim->m_anim;
  const AnimRef* ref = nullptr;
  getWalkRunSkillID(ref);
  if (!ref) return;
  if (AnimRefId(ref) != anim.GetTrackAnimId(eTrackFlags{eTrackFlags::kMain})) {
    EACTrack* t = anim.SetTrackAnim(eTrackFlags{2}, AnimRefId(ref), 2, kWalkRunBlendTime, nullptr);
    if (t) t->m_endMode = 0;
    m_eventFrame = 0;
    m_eventFraction = 0.0f;
    m_curAnim = ref;
    m_flags |= 0x10000;
  }
  anim.SetTrackSpeed(eTrackFlags{eTrackFlags::kMain}, m_timeMult * m_pauseMult);
}

// ---------------------------------------------------------------------------------------------------
// Carrying

float SAnimator2::m_RightArmCarryIndexArray[0x3A];
float SAnimator2::m_LeftArmCarryIndexArray[0x3A];
float SAnimator2::m_DualArmCarryIndexArray[0x3A];
float SAnimator2::m_HeadIndexArray[0x3A];
float SAnimator2::m_FXNodeIndexArray[0x3A];
float SAnimator2::m_UpperBodyIndexArray[0x3A];

// 8005C610
// The nodes each partial track moves (the adult skeleton's indices): the right arm 47..54, the left
// arm 39..43, both arms, the head 16, the fx node 57, the upper body (spine 12..16 and both arms).
void SAnimator2::InitStaticAnimationElements() {
  float* const arrays[] = {m_RightArmCarryIndexArray, m_LeftArmCarryIndexArray, m_DualArmCarryIndexArray,
                           m_HeadIndexArray, m_FXNodeIndexArray, m_UpperBodyIndexArray};
  for (float* a : arrays)
    for (int i = 0; i < 0x3A; ++i) a[i] = 0.0f;
  for (int i = 47; i <= 54; ++i) m_RightArmCarryIndexArray[i] = 1.0f;
  for (int i = 39; i <= 43; ++i) m_LeftArmCarryIndexArray[i] = 1.0f;
  for (int i = 39; i <= 43; ++i) m_DualArmCarryIndexArray[i] = 1.0f;
  for (int i = 47; i <= 51; ++i) m_DualArmCarryIndexArray[i] = 1.0f;
  m_HeadIndexArray[16] = 1.0f;
  m_FXNodeIndexArray[57] = 1.0f;
  for (int i = 12; i <= 16; ++i) m_UpperBodyIndexArray[i] = 1.0f;
  for (int i = 39; i <= 43; ++i) m_UpperBodyIndexArray[i] = 1.0f;
  for (int i = 47; i <= 51; ++i) m_UpperBodyIndexArray[i] = 1.0f;
}

// 80066F00
void SAnimator2::lockHandsUpCarryNodes() {
  m_sim->m_anim.SetTrackBlendFactors(eTrackFlags{0x800}, m_DualArmCarryIndexArray);
}

// 80066F40
void SAnimator2::lockCarryArmNodes(float* factors, EACTrack* track) {
  m_sim->m_anim.SetTrackBlendFactors(track, factors);
}

// 80066F80
void SAnimator2::lockCarryArmNodes(float* factors, int flags) {
  m_sim->m_anim.SetTrackBlendFactors(eTrackFlags{flags}, factors);
}

// 800676EC
// The hand's position and the way a held object faces (positionCharacter keeps both), and the hand
// bone's matrix at its joint.
void SAnimator2::GetCarryHandPosAndDir(EVec3& pos, EVec3& dir, EMat4& hand) {
  pos = m_bonePos[0];
  dir = m_carryDir;
  m_sim->m_anim.CalcNodeOrientWithoutLocalRot(m_boneIndex[0], hand);
}

// 80066A20
// Once a frame: a skill over the carry that faded it out suspends it. While the person holds
// something (in slot 0) and no carry animation was asked for (flag 0x800 / 0x4000 / 0x8000), the
// standard carry pose (std anim 0x26) holds the right arm, and the held object does not fade with
// the walls; once it is put down the pose fades out over kCarryBlendTime. Under player control a
// held direct-control object (definition +0x0A 0x23..0x25) takes its own carry animation.
void SAnimator2::updateCarryAnimation() {
  EAnimController& ctl = m_sim->m_anim;
  if ((m_flags & 0x400) && ctl.GetTrackIntensity(eTrackFlags{0x800}) == static_cast<double>(0.0001f))
    ctl.SuspendTrack(eTrackFlags{0x800});
  cXPersonImpl* person = Impl(m_person);
  auto heldModel = [](short id) -> EInstance* {
    cXObject* o = id && Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(id) : nullptr;
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    return impl ? impl->m_instance : nullptr;
  };
  unsigned int flags = m_flags;
  if (!(flags & 0x800)) {
    if (person->IsCarrying()) {
      if (m_flags & 0x8000) return;
      cXObjectImpl* held = person->GetContainedObject(0);
      m_carryObject = held ? held->m_id : 0;
      if (m_carryObject)
        if (EInstance* model = heldModel(m_carryObject)) model->m_interestFade = 0;
      if (ctl.IsTrackValid(eTrackFlags{0x800})) return;
      m_flags |= 0x800;
      const AnimRef* ref = nullptr;
      GetStdAnimRef(m_person, 0x26, ref);
      if (!ref) return;
      EACTrack* t = ctl.SetTrackAnim(eTrackFlags{0x800}, AnimRefId(ref), -1, -1.0f, nullptr);
      if (!t) return;
      ctl.SetTrackIntensity(t, 1.0f, true);
      ctl.SetTrackSpeed(t, m_timeMult);
      lockCarryArmNodes(m_RightArmCarryIndexArray, t);
      if (m_flags & 0x400) ctl.SuspendTrack(t);
      return;
    }
    flags = m_flags & ~0x8000u;
  } else {
    if ((flags & 1) && !(flags & 0x4000) && person->IsCarryingDCObject()) {
      unsigned int id = 0;
      cXObjectImpl* held = person->GetContainedObject(0);
      switch (held && held->m_def ? ERQuickdata::S16(held->m_def, 0x0a) : 0) {
        case 0x23: id = 0xaf5d68a9u; break;
        case 0x24: id = 0xa3cdf677u; break;
        case 0x25: id = 0x8baf558au; break;
        default: break;
      }
      if (id) ctl.SetTrackAnim(eTrackFlags{0x800}, id, 0, -1.0f, nullptr);
      m_flags |= 0x4000;
    }
    if (person->IsCarrying()) return;
    if (m_carryObject) {
      if (EInstance* model = heldModel(m_carryObject)) model->m_interestFade = 1;
      m_carryObject = 0;
    }
    if (!(m_flags & 0x400)) ctl.SetTrackBlendSmooth(eTrackFlags{0x800}, 0.0f, kCarryBlendTime, 0.01f);
    else ctl.SetTrackIntensity(eTrackFlags{0x800}, 0.0f);
    if (m_flags & 0x400) {
      m_flags &= ~0x400u;
      ctl.ResumeTrack(eTrackFlags{0x800});
      ctl.SetTrackBlendSmooth(eTrackFlags{0x800}, 1.0f, kDefaultBlendTime, 0.01f);
    }
    flags = m_flags & ~0x800u;
  }
  m_flags = flags;
}

// ---------------------------------------------------------------------------------------------------
// Outfits

// 8005C790
int SAnimator2::TryChangeSuit() {
  if (!m_sim || !m_sim->m_modelReady) return 2;
  if (m_person->GetServiceNPC()) {
    m_sim->m_changingOutfit = 0;
    return 1;
  }
  if (m_lastCostume == m_person->GetPersonData(8) && !m_sim->HasQueuedOperation() && m_sim->m_skinChange == -1) {
    m_sim->m_changingOutfit = 0;
    return 1;
  }
  m_sim->m_changingOutfit = 1;
  return 2;
}

// 80066FC0
void SAnimator2::updateRenderModels() {
  if (!m_person || m_person->GetServiceNPC()) return;
  const int outfit = m_person->GetPersonData(8);
  if (m_lastCostume == outfit) return;
  if (m_sim && (m_sim->m_skinChange != -1 || m_sim->HasQueuedOperation())) return;
  int changed;
  if (outfit == 0) changed = wearNormal();
  else if (outfit == 3) changed = setJobModel();
  else if (outfit > 0 && outfit < 0xb) changed = setNewModel(GetCostumeName(outfit), false, true);
  else changed = wearNormal();
  if (changed) m_lastCostume = outfit;
}

// 800670E0
// The person's job's uniform (career data 0x38, level 0x39: the level's +0x68 costume name), else the
// own clothes.
int SAnimator2::setJobModel() {
  if (auto* careers = static_cast<CareersImpl*>(Globs_pCareers)) {
    if (const Career* c = careers->GetCareerByID(m_person->GetPersonData(0x38))) {
      const int level = m_person->GetPersonData(0x39);
      if (level >= 0 && level < career::JobCount(c)) {
        const char* uniform = ERQuickdata::String(career::JobAt(c, level), 0x68);
        if (uniform && *uniform) return setNewModel(uniform, false, true);
      }
    }
  }
  return wearNormal();
}

// 800671C0
// The own clothes back (when in a costume), and the change asked for.
int SAnimator2::wearNormal() {
  CasSimDescriptionS2C* desc = m_person->GetSimDescription();
  if (m_ownClothes && desc) {
    *desc = *m_ownClothes;
    desc->Verify();
    delete m_ownClothes;
    m_ownClothes = nullptr;
  }
  return m_sim ? m_sim->CreateSkinAsync() : 0;
}

// 80067288
int SAnimator2::setNewModel(const char* name, bool, bool) {
  auto* costumes = static_cast<CasCostumes*>(_globals.m_casCostumes);
  if (!costumes || !m_sim || !name) return 0;
  CasSimDescriptionS2C* costume = costumes->GetSimDescription(m_sim->IsMale() != 0, name);
  CasSimDescriptionS2C* desc = m_person->GetSimDescription();
  if (!costume || !desc) return 0;
  if (!m_ownClothes) m_ownClothes = new CasSimDescriptionS2C(*desc);
  *desc = *costume;
  RestoreNonCostumeRelatedSimDescriptionItems(desc, m_ownClothes, desc->m_head != 0);
  desc->Verify();
  return m_sim->CreateSkinAsync();
}

// 800673FC
// The person's identity and (unless the costume brings its own head) face and body: the mark and
// version, names, gender, body type, skin, head, hair, facial feature, tattoos, the body morphs; then
// the personality and zodiac.
void SAnimator2::RestoreNonCostumeRelatedSimDescriptionItems(CasSimDescriptionS2C* d, CasSimDescriptionS2C* own,
                                                             bool costumeHasHead) {
  d->m_magic = own->m_magic;
  d->m_version = own->m_version;
  for (int i = 0; i < 32; ++i) {
    d->m_firstName[i] = own->m_firstName[i];
    d->m_lastName[i] = own->m_lastName[i];
  }
  d->m_male = own->m_male;
  if (!costumeHasHead) {
    d->m_bodyType = own->m_bodyType;
    d->m_skinColor = own->m_skinColor;
    d->m_head = own->m_head;
    d->m_hair = own->m_hair;
    d->m_hairTexture = own->m_hairTexture;
    d->m_hairStyle = own->m_hairStyle;
    d->m_facialFeature = own->m_facialFeature;
    for (int i = 0; i < 3; ++i) d->m_tattoos[i] = own->m_tattoos[i];
    d->m_morphBody = own->m_morphBody;
    d->m_morphArms = own->m_morphArms;
    d->m_morphLegs = own->m_morphLegs;
    d->m_morphHead = own->m_morphHead;
    d->m_morph108 = own->m_morph108;
    d->m_morph10C = own->m_morph10C;
    d->m_morph110 = own->m_morph110;
    d->m_morph114 = own->m_morph114;
  }
  for (int i = 0; i < 5; ++i) d->m_personality[i] = own->m_personality[i];
  d->m_zodiac = own->m_zodiac;
}

// 800675A0
const char* SAnimator2::GetCostumeName(int outfit) {
  switch (outfit) {
    case 1: return "naked";
    case 2: {
      const int r = simmy::sim_rand() % 3;
      return r == 1 ? "swimsuit2" : r == 2 ? "swimsuit3" : "swimsuit";
    }
    case 4: return "marriage";
    case 5: return "PJ";
    case 6: return "workout";
    case 7: return "sci_fi_jumpsuit";
    case 8: return "yeti";
    case 9: return "panda";
    case 10: return "robot";
    default: return nullptr;
  }
}

// 8005EE34
void SAnimator2::ReconStream(ReconBuffer* b, int version) {
  const bool reading = b->m_mode == ReconBuffer__Mode::kRead;
  if (version < 0x40) {
    // The old model state: two ints, five pairs of names, a counted list of pairs, three names, the
    // model kind (read: unknown kinds become 3) and nine floats; all dropped.
    int unused = 0, count = 0, kind = 0;
    b->ReconInt(&unused, 1);
    b->ReconInt(&unused, 1);
    std::string name;
    for (int i = 0; i < 5; ++i) {
      b->ReconString(name);
      b->ReconString(name);
    }
    b->Recon32(&count, 1);
    for (int i = 0; i < count; ++i) {
      b->ReconString(name);
      b->ReconString(name);
    }
    for (int i = 0; i < 3; ++i) b->ReconString(name);
    b->ReconInt(&kind, 1);
    float f = 0.0f;
    for (int i = 0; i < 9; ++i) b->ReconFloat(&f, 1);
  } else if (reading) {
    short count = 0;
    b->Recon16(&count, 1);
    for (int i = 0; i < count; ++i) {
      PreloadedModel m;
      b->ReconBool(&m.m_flag);
      b->Recon32(reinterpret_cast<int*>(&m.m_res), 1);
      // Kept (and loaded with EResourceManager::AddRefAsync) when the model manager knows the id.
      if (_modelman.IsValid(m.m_res)) {
        m.m_model = static_cast<ERModel*>(_modelman.AddRefAsync(m.m_res));
        m_preloadedModels.push_back(m);
      }
    }
    int running = 0, frame = 0, loops = 0;
    float fraction = 0.0f;
    b->ReconBool(&running);
    b->ReconFloat(&fraction, 1);
    b->ReconInt(&frame, 1);
    b->Recon32(reinterpret_cast<int*>(&m_suspendedSkill), 1);
    b->ReconInt(&loops, 1);
    if (!running) {
      m_suspendedSkill = 0;
    } else {
      m_eventFraction = fraction;
      m_eventFrame = frame;
      m_skillSuspended = 1;
      m_loopCount = loops;
    }
    if (version > 0x60) {
      int ownClothes = m_ownClothes != nullptr;
      b->ReconBool(&ownClothes);
      if (ownClothes) {
        delete m_ownClothes;
        m_ownClothes = new CasSimDescriptionS2C;
        m_ownClothes->DoStream(b, 2);
      }
    }
  } else {
    short count = static_cast<short>(m_preloadedModels.size());
    b->Recon16(&count, 1);
    for (PreloadedModel& m : m_preloadedModels) {
      b->ReconBool(&m.m_flag);
      b->Recon32(reinterpret_cast<int*>(&m.m_res), 1);
    }
    b->ReconBool(&m_skillRunning);
    b->ReconFloat(&m_eventFraction, 1);
    b->ReconInt(&m_eventFrame, 1);
    b->Recon32(reinterpret_cast<int*>(&m_skillAnimId), 1);
    b->ReconInt(&m_loopCount, 1);
    int ownClothes = m_ownClothes != nullptr;
    b->ReconBool(&ownClothes);
    if (ownClothes) m_ownClothes->DoStream(b, 2);
  }
  if (reading) m_reconstituted = 1;
}
