// Port of game/sanimator2.cpp: a person's animator. Set-up, the per-frame update, the anim-state
// choice, the free-move (direct control) states (idle, turn on the spot, walk, run, run stop) with
// their animations, cross-fades and movement, collision, posing the sim, the animate primitive
// (skills on track 0x10, their loops and 'xevt' animation events), following the person's path (the
// GC follow modes: turns, side steps, walking with slow-down at the end, door portal mode, NPC
// auto-run, the awareness follow), low-motive walks and idles, skill idles, the mood on the head,
// carrying, props, costumes, particles, footsteps, sounds, rumble notes and the censor. The awareness
// manager is game/awarenessmanager.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/roomresolve.hpp"
#include "simmy/classes/placement.hpp"

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
const float kSidestepBlendTime = 0.2f;     // 804FDAF0
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
// A pending animation load is given back, the props and particles go, then the animations it still
// holds (an impatient idle, a long idle, the optional low-motive walks) and, while in a costume, the
// person's own description is put back. (Before these were all ported only the props and particles went:
// every person killed kept its held animations referenced, and a sim quit in a costume leaked its own
// description.)
SAnimator2::~SAnimator2() {
  if (m_pendingAnim != 0) {
    _animman.DelRefAsync(static_cast<unsigned int>(m_pendingAnim));
    m_pendingAnim = 0;  // GC: m_pendingCount - 1 (PC: loads are synchronous)
  }
  removeCostume();
  removeAllProps();
  cleanupParticlesImmediate(&m_particles);
  cleanupParticlesImmediate(&m_overlayParticles);
  clearImpatientIdleAnimation();
  UnloadPendingLongIdleAnim();
  UnloadDCOptionalMotionAnims();
  UnloadSMOptionalMotionAnims();
  if (m_ownClothes) {
    if (CasSimDescriptionS2C* desc = m_person ? m_person->GetSimDescription() : nullptr) *desc = *m_ownClothes;
    delete m_ownClothes;
    m_ownClothes = nullptr;
  }
}

// 800676E4
// Empty in the release build.
RET_UNKNOWN SAnimator2::removeCostume() { return 1; }

// 8005C43C
// For `person`: its instance and destination list (its path), the bones the camera, props and the
// awareness follow (right hand, head, pelvis, torso: dogs and cats 2, 0x17, 6, 0; monkeys 0x17, 0x10,
// 2, 0xE; people 0x35, 0x10, 2, 0xC), then the awareness manager.
int SAnimator2::Initialize(cXPerson* person) {
  m_person = person;
  m_flags &= ~1u;
  cXPersonImpl* impl = Impl(person);
  m_sim = impl ? static_cast<ESim*>(impl->m_instance) : nullptr;
  m_destList = impl ? &impl->m_path : nullptr;  // GetDestList (vtable 0x184)
  m_turnStartDir = kNoDir;
  m_pauseMult = 1.0f;
  m_useSpeed = 0.0f;
  m_dir = 0.0f;
  m_timeMult = 1.0f;
  m_walkBlend = 0.0f;
  m_shuffleDir = 2;
  m_longIdleCount = 0;
  SetIdleInitialized(false);
  m_carryObject = 0;
  m_lowMotiveIdle = 0;
  m_moodAnim = 0;
  m_skillAnimId = 0;
  m_suspendedSkill = 0;
  m_skillSuspended = 0;
  m_lowMotive = 0;
  m_resetPos = 0;
  m_pendingAnim = 0;
  if (impl && (impl->IsDog() || impl->IsCat())) {
    m_boneIndex[0] = 2, m_boneIndex[3] = 0, m_boneIndex[1] = 0x17, m_boneIndex[2] = 6;
  } else if (impl && impl->IsMonkey()) {
    m_boneIndex[0] = 0x17, m_boneIndex[3] = 0xe, m_boneIndex[1] = 0x10, m_boneIndex[2] = 2;
  } else {
    m_boneIndex[0] = 0x35, m_boneIndex[3] = 0xc, m_boneIndex[1] = 0x10, m_boneIndex[2] = 2;
  }
  if (m_sim) m_aware.Init(this, &m_sim->m_anim);
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
// A person read from a save in a costume skips the changing curtain. The frame's speed (the
// simulator's; 0 while it is paused or stalled), the awareness check, the movement state, the main
// animation's events up to now (looping animations start over), then the animations; the sim posed
// where it stands. GC: also nothing moves (speed 0) while an animation loads outside a follow
// (m_pendingCount; PC: loads are synchronous).
void SAnimator2::Update() {
  if (!m_sim || !m_person) return;
  if (m_reconstituted == 1) {
    m_reconstituted = 0;
    if (m_person->GetPersonData(8) != 0) m_sim->m_field424 = 1;
  }
  cSimulatorImpl* sim = Globs_pSimulator;
  m_timeMult = 1.0f;
  if (sim) m_timeMult = (sim->IsStalled() || sim->IsPaused() || sim->IsStopped()) ? 0.0f : static_cast<float>(sim->GetSpeedMultiplier());
  m_aware.SetTimeMultiplier(m_timeMult);
  adjustAnimationPlayRates();
  m_prevTimeMult = m_timeMult;
  m_aware.AwarenessCheck(static_cast<unsigned int>(m_animState));
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
// Along a follow, or (under player control) the free-move state; the awareness follows (6..) turn
// the sim toward what it is aware of.
void SAnimator2::updateMovementState() {
  if (m_followMode < 6) {
    if (m_routing == 1) moveAnimation();
    else if (m_flags & 1) updateFreeMoveState();
  } else {
    awarenessMove();
  }
}

// 8005C8E8
// Without a desired direction: the run (or a walk still fading in from one) stops with the run-stop
// animation, then the state becomes the desired one (a run while carrying a direct-control object or
// low on energy or bladder walks: the low-motive walk loads). With one: standing (or stopping), it
// turns on the spot first (a shuffle, state 12; the sim's sounds are quietened); it turns toward the
// direction (GetTurnRate), and once there takes the desired state, the direction used up. The person
// faces where it turned.
void SAnimator2::updateFreeMoveState() {
  const float dt = ScaledDt();
  if (m_desiredDir == kNoDir) {
    if (m_animState == kImpatient) {
      // Still wanted: once the impatient idle has played, standing again, and autonomy takes the sim.
      if (m_desiredState == kImpatient) {
        if (!(m_flags & 0x1000000u) || !m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{1})) return;
        m_animState = kFreeIdle;
        m_desiredState = kFreeIdle;
        m_flags &= ~0x1000000u;
        UnloadDCOptionalMotionAnims();
        UnloadOptionalIdleAnim();
        UnloadPendingLongIdleAnim();
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
        UnloadDCOptionalMotionAnims();
        return;
      }
      if (m_animState == kRunStop) {
        if (!m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{1})) return;
        m_animState = kFreeIdle;
        m_flags &= ~0x800000u;
        UnloadDCOptionalMotionAnims();
        return;
      }
    }
    cXPersonImpl* p = Impl(m_person);
    if (m_desiredState == kFreeRun && (p->IsCarryingDCObject() || shouldUseLowMotiveWalk())) {
      LoadDCOptionalMotionAnims();
      m_animState = kFreeWalk;
      return;
    }
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
      if (m_animState == kFreeIdle && Globs_pSound && m_person->AsObject())
        Globs_pSound->QuietBySourceID(static_cast<short>(m_person->AsObject()->GetID()));
      initShuffleRotation(delta);
      LoadDCOptionalMotionAnims();
      UnloadOptionalIdleAnim();
      UnloadPendingLongIdleAnim();
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
// A finished turn takes the desired state: turning or standing stands (the direct-control walks
// unloaded); a run while carrying a direct-control object or low on a motive walks.
void SAnimator2::SetNextStateFromCompletedTurn() {
  if (m_desiredState == kFreeTurn || m_desiredState == kFreeIdle) {
    UnloadDCOptionalMotionAnims();
    m_animState = kFreeIdle;
    return;
  }
  cXPersonImpl* p = Impl(m_person);
  if (m_desiredState == kFreeRun && (p->IsCarryingDCObject() || shouldUseLowMotiveWalk())) {
    LoadDCOptionalMotionAnims();
    m_animState = kFreeWalk;
    return;
  }
  if (m_animState == m_desiredState) return;
  CheckOptionalMotionAnimStatusOnStateChange(m_desiredState);
  m_animState = m_desiredState;
}

// 80064C18
// Entering a free-move turn, walk or run loads the direct-control low-motive walk, any other state
// unloads it; leaving a standing idle (0, 9) for another state unloads the low-motive idle.
void SAnimator2::CheckOptionalMotionAnimStatusOnStateChange(int state) {
  if (static_cast<unsigned int>(state - kFreeTurn) < 3) LoadDCOptionalMotionAnims();
  else UnloadDCOptionalMotionAnims();
  if ((m_animState == kFreeIdle || m_animState == kIdle) && state != kFreeIdle) UnloadOptionalIdleAnim();
}

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
// Radians a second: instantly when walking a follow or in a door (portal mode 2), 3 pi in free-move
// turns, walks and runs, else the shuffle's own rate.
float SAnimator2::GetTurnRate() {
  if (m_followMode == 3 || m_portalMode == 2) return kTwoPi;
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
  float d = (static_cast<float>(impl ? impl->GetData(1) : 0) - 2.0f) * -0.7853982f;
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
// The anim state for this frame. Following: by follow mode (1 the starting turn 2, 2 a side step 4, 3
// walking 1, 4 the turn at the end 3, 5 turning to face 3 until it faces the person's direction); the
// awareness follows 0xF, 0x10, 0x11. Otherwise: the impatient idle seated becomes standing (autonomy
// takes the sim); under player control the free-move state stands (object data 2: holding, idle 8);
// simulator controlled the main track says: a skill 5 (socialising 6), an algorithmic idle 7, else 0.
// Then the state's animation, the mood on the head, and (not loading) the sim posed where it stands
// with the awareness animations and particles.
void SAnimator2::updateRenderAnimation() {
  int state = m_animState;
  if (m_followMode < 6) {
    if (m_routing == 1) {
      switch (m_followMode) {
        case 3: state = 1; break;
        case 1: state = 2; break;
        case 2: state = 4; break;
        case 4: state = 3; break;
        case 5:
          if (m_dir != static_cast<float>(getPersonDirection())) state = 3;
          break;
        default: break;
      }
    } else if (state == kImpatient) {
      // Seated (idle states 1, 2) there is no impatient idle: autonomy takes the sim at once.
      cXPersonImpl* p = Impl(m_person);
      if (static_cast<unsigned int>(p->GetIdleState() - 1) < 2u) {
        m_animState = kFreeIdle;
        m_desiredState = kFreeIdle;
        p->SetIdlePlayerSimAutonomous();
      }
    } else if (!IsSimulatorControlled()) {
      if (Impl(m_person)->GetData(2) != 0) state = kSimIdle;
      else if (state < kFreeIdle) state = kFreeIdle;
    } else {
      const unsigned int main = MainTrackFlags();
      if (!(main & eTrackFlags::kMain) || (main & 1)) {
        state = kIdle;
        if (Impl(m_person)->IsInSocialMode()) state = 6;
      } else if (!(main & 0x20)) {
        state = 5;
      } else {
        state = 7;
        if (Impl(m_person)->IsInSocialMode()) state = 6;
      }
    }
  } else if (m_followMode == 7) {
    state = 0x10;
  } else if (m_followMode == 6) {
    state = 0xf;
  } else if (m_followMode == 8) {
    state = 0x11;
  }
  if (m_animState != state) {
    CheckOptionalMotionAnimStatusOnStateChange(state);
    m_animState = state;
  }
  switch (m_animState) {
    case kIdle: case kSimIdle: case kFreeIdle:
      handleIdleAnimation();
      break;
    case 1:
      stopIdleOverlay();
      handleWalkRunAnimation();
      break;
    case 2: case 3: case kFreeTurn: case 0xf: case 0x11:
      stopIdleOverlay();
      handleTurnAnimation();
      break;
    case 4:
      stopIdleOverlay();
      handleSidestepAnimation();
      break;
    case 5: case 6:
      stopIdleOverlay();
      break;
    case 7:
      handleSkillIdleAnimation();
      break;
    case kImpatient:
      stopIdleOverlay();
      handleImpatientIdleAnimation();
      break;
    case kRunStop:
      stopIdleOverlay();
      handleRunStopAnimation();
      break;
    case kFreeWalk: case kFreeRun:
      stopIdleOverlay();
      handleFreeMoveWalkRunAnimation();
      break;
    case 0x10:
      stopIdleOverlay();
      break;
    default: break;
  }
  handleMoodAnimations();
  if (m_pendingAnim == 0) {
    if (m_resetPos) {
      m_resetPos = 0;
      m_pos[0] = static_cast<float>(getPersonX());
      m_pos[1] = static_cast<float>(getPersonY());
    }
    m_renderPos[0] = m_pos[0];
    m_renderPos[1] = m_pos[1];
    m_aware.handleAwarenessAnimations(static_cast<unsigned int>(m_animState));
    if (m_sim->m_modelReady) positionCharacter(nullptr);
    updateParticles();
  }
}

// 80068ECC
// Not under player control, following, socialising (or the second player's social), in motive failure
// or busy with an interaction (GC: vtable 0x334, 0x354, 0x22C, 0xF4), or a skill track (0x10) still
// playing.
bool SAnimator2::IsSimulatorControlled() {
  if (!(m_flags & 1) || m_routing == 1) return true;
  cXPersonImpl* p = Impl(m_person);
  if (p->IsInSocialMode() || p->IsSecondPlayerInSocialMode() || p->IsInMotiveFailure()) return true;
  if (p->GetCurrentInteractionStackObject() != 0) return true;
  return m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{0x10}) == 0;
}

// 80068FBC
// The player steers this person (direct control) or the simulator does; the controller's flag 1 and
// the awareness manager follow.
void SAnimator2::SetPlayerControl(bool control) {
  if (control) m_flags |= 1;
  else m_flags &= ~1u;
  if (m_sim) {
    if (!control) m_sim->m_anim.m_flags &= ~1u;
    else m_sim->m_anim.m_flags |= 1;
  }
  m_aware.SetPlayerControl(control);
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
// them. The bound sphere (lighting, sorting) is the pose's EAnimController::CalcVisibilitySphere and the
// bounds the box around it (EBound3::Compute(EBoundSphere&) 802C5980, inlined); a bone's position is
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
  anim.CalcVisibilitySphere(*orient, *reinterpret_cast<EBoundSphere*>(m_sim->m_sphere));
  {
    const float* c = m_sim->m_sphere;
    float bounds[6] = {c[0] - c[3], c[1] - c[3], c[2] - c[3], c[0] + c[3], c[1] + c[3], c[2] + c[3]};
    m_sim->SetBounds(*reinterpret_cast<EBound3*>(bounds));
  }
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
// still loading given up; the awareness manager reset.
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
    m_pendingAnim = 0;
  }
  m_aware.Reset();
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
  weights[1] = (p->IsCarrying() || (m_flags & 0x800) || (m_aware.m_flags & 0x200)) ? 0x14 : 0x28;
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
  if (m_portalMode == 2 || (m_portalMode == 1 && m_animState == 3)) return;
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
  if (m_animState == kFreeWalk) {
    style = 7;
  } else if (m_animState == kFreeRun && m_lowMotive != 0) {
    // A low-motive run walks once its low-motive walk is in.
    const AnimRef* low = nullptr;
    if (m_lowMotive == 5) GetStdAnimRef(m_person, 0x12, low);
    else if (m_lowMotive == 9) GetStdAnimRef(m_person, 0x3b, low);
    if (low && _animman.GetRef(AnimRefId(low))) style = 7;
  }
  if (style != m_walkStyle) {
    m_prevWalkStyle = m_walkStyle;
    m_walkStyle = style;
  }
  EVec2 from{m_pos[0], m_pos[1]};
  stopIdleOverlay();
  const AnimRef* ref = nullptr;
  getWalkRunSkillID(ref);
  if (ref && AnimRefId(ref) != playing) {
    if (m_monitoredTrack && !(m_monitoredTrack->m_flags & 0x80000)) {
      anim.SetTrackBlendSmooth(m_monitoredTrack, 0.0f, kFastRemoveBlendTime, 0.01f);
      m_monitoredTrack = nullptr;
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
  // The person goes where the sim walked if it can be placed there (CanPlace / Place, vtable 0x134 /
  // 0x13C: its tile and room, the tile map); else the sim steps back.
  cXPersonImpl* impl = Impl(m_person);
  const int pos[2] = {static_cast<int>(m_pos[1] + 0.5f), static_cast<int>(m_pos[0] + 0.5f)};
  if (!ObjCanPlace(impl, pos, impl->m_level, nullptr, 0)) {
    m_pos[0] = from.x, m_pos[1] = from.y;
  } else {
    impl->PlacePerson(pos);
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
    case 7: playRumble(const_cast<char*>(note.m_text.c_str())); break;
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

// 800661D8
// A rumble note: its RumbleDataElement (ObjectFolder::GetRumbleByName).
void SAnimator2::playRumble(char* name) {
  ObjectFolderImpl* folder = Globs_pObjectFolder;
  if (const void* rumble = folder ? folder->GetRumbleByName(name) : nullptr)
    playRumble(static_cast<RumbleDataElement*>(const_cast<void*>(rumble)));
}

// 8006622C
// The sim of a player (its instance this one) rumbles that player's pad, when rumble is on in the
// options and the rumble lasts: motor one at +0, motor two at +2 / 255, both for +4 seconds.
void SAnimator2::playRumble(RumbleDataElement* rumble) {
  int port = -1;
  cXPerson* p0 = _globals.GetSelectedPerson(0);
  if (p0 && p0->AsObject() && p0->AsObject()->GetObjectImplementation()->m_instance == static_cast<ISimInstance*>(m_sim))
    port = _pCtrlMan->GetPlayerControllerIndex(0);
  cXPerson* p1 = _globals.GetSelectedPerson(1);
  if (p1 && p1->AsObject() && p1->AsObject()->GetObjectImplementation()->m_instance == static_cast<ISimInstance*>(m_sim))
    port = _pCtrlMan->GetPlayerControllerIndex(1);
  auto* options = static_cast<OptionsRecon*>(_globals.m_optionsRecon);  // DAT_80475F14
  const float seconds = ERQuickdata::F32(rumble, 4);
  static const bool trace = std::getenv("SIMMY_RUMBLE_TRACE") != nullptr;  // PC dev trace
  if (trace)
    std::printf("[rumble] %s: %u %u %.2f s (port %d)\n", Impl(m_person)->DebugName(), ERQuickdata::U16(rumble, 0),
                ERQuickdata::U16(rumble, 2), seconds, port);
  if (port == -1 || !options || options->m_opt04 == 0 || !(0.0010000000474974513 < seconds) || !_globals.m_vibrate) return;
  _globals.m_vibrate->VibrateAll(static_cast<unsigned char>(port), static_cast<float>(ERQuickdata::U16(rumble, 0)),
                                 static_cast<float>(ERQuickdata::U16(rumble, 2)) / 255.0f, seconds, seconds);
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
// Following the person's destination list (its path: the person's spot, the way points, and a last
// point a tile on in the direction to face). Points are 1/16 tiles, p[1] the animation format's x and
// p[0] its y. The follow modes: 1 turn on the spot toward the first leg, 2 a side step (a short
// three-point list), 3 walk the legs (turning into each at 2 pi a second), 4 turn (to the nearest
// eighth) along the last leg, 5 turn to the person's direction; then done (m_routing 2) until
// FollowOneStep places the person.

TileList& SAnimator2::DestList() { return *m_destList; }
int SAnimator2::DestCount() { return m_destList ? static_cast<int>(m_destList->m_points.size()) : 0; }

void SAnimator2::dropMonitoredTrack() {
  if (!m_monitoredTrack) return;
  m_sim->m_anim.SetTrackBlendSmooth(m_monitoredTrack, 0.0f, kFastRemoveBlendTime, 0.01f);
  m_monitoredTrack = nullptr;
}

// 8006A69C
double SAnimator2::GetMovementVelocityFromStyle(int style) { return StyleVelocity(style); }

// 80069A14
// An adult's walk 19.84, a child's 17.28; runs 64.96 (1/16 tiles a second).
double SAnimator2::defaultVelocity(int style) {
  if (style == 1 || style == 8) return 64.95999908447266;
  return m_sim->IsAdult() ? 19.84000015258789 : 17.280000686645508;
}

// 80069A8C
double SAnimator2::getAnimDuration(ERAnim* anim) {
  return anim ? static_cast<float>(anim->m_frameCount - 1) / anim->m_fps : 0.0f;
}

// 8006A3AC
int SAnimator2::getIsLeftFootUp() {
  const double phase = m_sim->m_anim.GetTrackPos(eTrackFlags{eTrackFlags::kMain});
  return (phase < 0.25 || 0.75 <= phase) ? 1 : 0;
}

// 80069D10
// The walk/run style's velocity, once, from its cycle (getWalkRunSkillID) when loaded: the root's
// forward distance at its second-to-last frame over that time, if over 0.2.
void SAnimator2::setMovementVelocityFromAnimation() { readWalkVelocity(); }

void SAnimator2::readWalkVelocity() {
  float* velocity;
  unsigned int bit;
  if (m_walkStyle == 2) velocity = &m_lowEnergyVel, bit = 4;
  else if (m_walkStyle == 3) velocity = &m_lowBladderVel, bit = 8;
  else if (m_walkStyle == 1 || m_walkStyle == 8) velocity = &m_runVel, bit = 2;
  else velocity = &m_walkVel, bit = 1;
  if (m_velocityKnown & bit) return;
  const AnimRef* ref = nullptr;
  getWalkRunSkillID(ref);
  if (!ref || !_animman.GetRef(AnimRefId(ref))) return;
  auto* a = static_cast<ERAnim*>(_animman.AddRef(AnimRefId(ref), nullptr, 0));
  if (!a) return;
  const float frames = static_cast<float>(a->m_frameCount - 1);
  const float length = (frames / a->m_fps) * (static_cast<float>(a->m_frameCount - 2) / frames);
  if (length == 0.0f) {
    _animman.DelRef(a, EResourceManager__DelRefMode{1});
  } else {
    EAnimController& anim = m_sim->m_anim;
    EACTrack t;
    t.m_anim = a;
    anim.CreateStreams(&t);
    t.m_pos = static_cast<float>(a->m_frameCount - 2);
    const EVec3 root = anim.GetAnimRootNodeTrans(&t);
    if (0.2f < root.y) *velocity = root.y * 16.0f / length;
    anim.DestroyStreams(&t);
    t.m_anim = nullptr;
    _animman.DelRef(a, EResourceManager__DelRefMode{1});
  }
  m_velocityKnown |= bit;
}

// 80069ADC
// The side step's velocity (+0x0A4), once, from the side step animation: the root's sideways (x)
// distance half way through over 0.45 of half its length.
void SAnimator2::setSideStepSpeedFromAnimation(const AnimRef* ref) {
  if ((m_velocityKnown & 0x10) || !ref) return;
  auto* a = static_cast<ERAnim*>(_animman.AddRef(AnimRefId(ref), nullptr, 0));
  if (!a) return;
  const float time = (static_cast<float>(a->m_frameCount - 1) / a->m_fps) * 0.45f * 0.5f;
  if (time == 0.0f) {
    _animman.DelRef(a, EResourceManager__DelRefMode{1});
  } else {
    EAnimController& anim = m_sim->m_anim;
    EACTrack t;
    t.m_anim = a;
    anim.CreateStreams(&t);
    t.m_pos = static_cast<float>(a->m_frameCount - 1) * 0.5f;
    const EVec3 root = anim.GetAnimRootNodeTrans(&t);
    if (0.2f < std::fabs(root.y)) m_sideVel = std::fabs(root.y) * 16.0f / time;
    anim.DestroyStreams(&t);
    t.m_anim = nullptr;
    _animman.DelRef(a, EResourceManager__DelRefMode{1});
  }
  m_velocityKnown |= 0x10;
}

// 80069FBC
// The shuffle's rate from its animation's length (as initShuffleRotation, without marking the turn).
void SAnimator2::setRotationRateFromShuffleDir(int shuffle, float delta) {
  delta = std::fabs(delta);
  const unsigned int main = MainTrackFlags();
  const bool fromIdle = (main & 0x21) || (main & 4);
  const int row = fromIdle ? 0 : 1;
  const int bit = fromIdle ? shuffle + 8 : shuffle + 0x13;
  float* times = m_turnTime[row];
  if (!(m_velocityKnown & (1u << bit))) {
    const AnimRef* ref = nullptr;
    getTurnSkillID(ref, shuffle);
    if (auto* anim = ref ? static_cast<ERAnim*>(_animman.AddRef(AnimRefId(ref), nullptr, 0)) : nullptr) {
      const float t = static_cast<float>(anim->m_frameCount - 1) / anim->m_fps;
      if (t != 0.0f) {
        times[shuffle + 5] = t;
        times[-shuffle + 5] = t;
      }
      _animman.DelRef(anim, EResourceManager__DelRefMode{1});
    }
    m_velocityKnown |= 1u << bit;
    m_velocityKnown |= 1u << (fromIdle ? -shuffle + 8 : -shuffle + 0x13);
  }
  m_turnRate = delta / times[shuffle + 5];
  if (m_turnRate < 0.3926991f) m_turnRate = 0.3926991f;
}

// 8006A508
// The direction of the leg walked (from the point before the current one; the first leg at the
// start); along the last leg (mode 4) to the nearest eighth; on the spot the sim's own.
double SAnimator2::getDesiredDir() {
  const std::vector<TileListPoint>& p = DestList().m_points;
  const int i = m_routeIndex;
  float dx, dy;
  if (i == 0) {
    dx = static_cast<float>(p[1].p[1] - p[0].p[1]);
    dy = static_cast<float>(p[1].p[0] - p[0].p[0]);
  } else {
    dx = static_cast<float>(p[i].p[1] - p[i - 1].p[1]);
    dy = static_cast<float>(p[i].p[0] - p[i - 1].p[0]);
  }
  if (dx <= -0.001f || 0.001f <= dx || dy <= -0.001f || 0.001f <= dy) {
    double d = std::atan2(-dy, dx);
    if (m_followMode == 4) d = std::floor(static_cast<float>(d + 0.39269909262657166) / 0.7853982f) * 0.7853981852531433;
    return static_cast<float>(d);
  }
  return m_dir;
}

// 8006982C
void SAnimator2::updateDesiredAndDeltaDir(float& desired, float& delta) {
  desired = static_cast<float>(getDesiredDir());
  delta = static_cast<float>(getDeltaDirFromDesiredDir(desired));
}

// 8006952C
// A side step for a short (under a tile) three-point list unless in a door's portal mode; else the turn.
int SAnimator2::determineFirstFollowMode(TileList*) {
  UpdatePortalMode();
  const std::vector<TileListPoint>& p = DestList().m_points;
  if (m_portalMode == 0 && p.size() == 3) {
    const int dy = p[1].p[0] - p[0].p[0], dx = p[1].p[1] - p[0].p[1];
    if (dx * dx + dy * dy < 0x101) return 2;
  }
  return 1;
}

// PC helper (inlined in BeginFollow, determineWalkRunStyle, exitAwarenessFollow): the follow's walk
// style: the low-motive walk (not pets: energy 2, bladder 3), else object data 0x11; plain walks run
// in a side step (4: none) or by AwarenessAutoRunCheck.
int SAnimator2::followWalkStyle(float dist) {
  cXPersonImpl* p = Impl(m_person);
  int style;
  if (m_lowMotive == 0 || p->IsPet()) style = p->GetData(0x11);
  else if (m_lowMotive == 5) style = 2;
  else if (m_lowMotive == 9) style = 3;
  else style = 0;
  if (style == 0) style = m_followMode == 2 ? 4 : (StartAutoRun(dist) ? 1 : 0);
  return style;
}

// 800695B8
void SAnimator2::determineWalkRunStyle(float dist) {
  UnloadDCOptionalMotionAnims();
  UnloadSMOptionalMotionAnims();
  LoadSMOptionalMotionAnims();
  const int style = followWalkStyle(dist);
  if (style != m_walkStyle) {
    m_prevWalkStyle = m_walkStyle;
    m_walkStyle = style;
  }
}

// 80069870
// Following; unless in an awareness follow, from `mode` with the outgoing main tracks faded out.
void SAnimator2::setFirstFollowMode(int mode) {
  m_routing = 1;
  if (m_followMode < 6) {
    m_followMode = mode;
    m_flags &= ~0x7eu;
    m_sim->m_anim.BlendAllOutgoingTracks(0.0f, 0.1f, true);
    m_monitoredTrack = nullptr;
  }
}

// 800698E4
void SAnimator2::setFollowMiddle() {
  m_followMode = 3;
  m_flags &= ~0x5eu;
  setMovementVelocityFromAnimation();
}

// 80069920
void SAnimator2::setFollowEnd() {
  m_followMode = 4;
  m_useSpeed = 0.0f;
  m_flags &= ~0x5eu;
  dropMonitoredTrack();
}

// 80069998
void SAnimator2::setFollowDone() {
  m_followMode = 5;
  m_useSpeed = 0.0f;
  m_flags = (m_flags & ~0x5eu) | 0x20;
  dropMonitoredTrack();
}

// 8006C850
// Long enough (AwarenessAutoRunCheck) to run: the person's data 0x11 says so (1) until EndAutoRun.
bool SAnimator2::StartAutoRun(float dist) {
  m_autoRun = m_aware.AwarenessAutoRunCheck(dist);
  if (m_autoRun == 1) Impl(m_person)->SetData(0x11, 1);
  return m_autoRun == 1;
}

// 800692E4
bool SAnimator2::EndAutoRun() {
  const bool was = m_autoRun == 1;
  if (was) {
    Impl(m_person)->SetData(0x11, 0);
    m_autoRun = 0;
  }
  return was;
}

// 8005E7CC
// Follows the destination list: a load in flight given up; the first follow mode (a side step for a
// short three-point list, else the turn), the direction the person faces, the simulator's low-motive
// walk loaded, the walk style (followWalkStyle); the walk starts at the person's spot.
void SAnimator2::BeginFollow(float dist) {
  if (m_pendingAnim != 0) {
    _animman.DelRefAsync(static_cast<unsigned int>(m_pendingAnim));
    m_pendingAnim = 0;
  }
  m_sim->DrawSim(true);
  UpdatePortalMode();
  int mode = 1;
  const std::vector<TileListPoint>& p = DestList().m_points;
  if (m_portalMode == 0 && p.size() == 3) {
    const int dy = p[1].p[0] - p[0].p[0], dx = p[1].p[1] - p[0].p[1];
    if (dx * dx + dy * dy < 0x101) mode = 2;
  }
  m_dir = static_cast<float>(getPersonDirection());
  setFirstFollowMode(mode);
  UnloadDCOptionalMotionAnims();
  UnloadSMOptionalMotionAnims();
  LoadSMOptionalMotionAnims();
  const int style = followWalkStyle(dist);
  if (style != m_walkStyle) {
    m_prevWalkStyle = m_walkStyle;
    m_walkStyle = style;
  }
  m_routeIndex = 0;
  m_pos[0] = static_cast<float>(getPersonX());
  m_resetPos = 0;
  m_pos[1] = static_cast<float>(getPersonY());
  m_pathStartsHere = (!p.empty() && p.size() > 1 && p[0].p[1] == p[1].p[1] && p[0].p[0] == p[1].p[0]) ? 1 : 0;
  static const bool trace = std::getenv("SIMMY_WALK_TRACE") != nullptr;  // PC dev trace
  if (trace)
    std::printf("[walk] %s: follow %d points from %d,%d, mode %d style %d dist %.1f\n", Impl(m_person)->DebugName(),
                static_cast<int>(p.size()), getPersonX(), getPersonY(), mode, m_walkStyle, dist);
}

// 8005EAB8
// 2 while walking (or the model changes); arrived (2): the person goes to the nearest point of its path
// to where the sim stands if it fits there (1), else stays where it is (0, the sim snapped back); it
// faces where the sim does. Blocked or not following: 0.
int SAnimator2::FollowOneStep() {
  m_sim->DrawSim(true);
  if (m_sim->HasQueuedOperation()) return 2;
  if (m_routing == 1) return 2;
  if (m_routing != 2) return 0;
  cXPersonImpl* p = Impl(m_person);
  int pos[2] = {static_cast<int>(m_pos[1] + 0.5f), static_cast<int>(m_pos[0] + 0.5f)};
  DestList().FindNearestPoint(pos, -1);
  const int x = getPersonX(), y = getPersonY();
  const int ok = ObjCanPlace(p, pos, p->m_level, nullptr, 0);
  if (!ok) {
    pos[0] = y;
    pos[1] = x;
    SnapToGrid();
  }
  p->PlacePerson(pos);
  setPersonDirection(m_dir);
  return ok ? 1 : 0;
}

// 8005EC74
// The follow ends: the portal mode and the animate flag cleared, the default idle unless the main
// track idles already, the monitored track faded, the turn to face done, the outgoing tracks faded,
// the simulator's low-motive walk unloaded, an auto run undone. True unless it was still walking.
bool SAnimator2::EndFollow() {
  m_portalModeSet = 0;
  m_animatePrim = 0;
  if (!(MainTrackFlags() & 0x21)) StartDefaultIdle();
  if (m_monitoredTrack) DetachMonitoredTrack();
  const int old = m_routing;
  if (m_followMode != 5) setFollowDone();
  m_routing = 0;
  m_sim->m_anim.BlendAllOutgoingTracks(0.0f, 0.1f, false);
  UnloadSMOptionalMotionAnims();
  if (m_autoRun == 1) {
    Impl(m_person)->SetData(0x11, 0);
    m_autoRun = 0;
  }
  return old != 1;
}

// 800697D8
void SAnimator2::DetachMonitoredTrack() {
  m_sim->m_anim.SetTrackBlendSmooth(m_monitoredTrack, 0.0f, kFastRemoveBlendTime, 0.01f);
  m_monitoredTrack = nullptr;
}

// 8005F958
// The follow is done: standing without a main track (a door's portal mode aside), a random idle (the
// low-motive one when it is in) with maybe an overlay; the sim at the path's second-to-last point (the
// person's spot for a short list).
int SAnimator2::endFollowDone() {
  const int count = DestCount();
  if (m_portalMode == 0 && !m_sim->m_anim.IsTrackValid(eTrackFlags{1})) {
    m_longIdleCount = 0;
    const int idle = selectRandomIdle();
    if (m_lowMotiveIdle != 0) LoadOptionalIdleAnim();
    if (!(m_flags & 0x100000)) {
      const AnimRef* ref = nullptr;
      GetStdAnimRef(m_person, idle, ref);
      EACTrack* t = ref ? m_sim->m_anim.SetTrackAnim(eTrackFlags{1}, AnimRefId(ref), -1, -1.0f, nullptr) : nullptr;
      m_eventFrame = 0;
      m_eventFraction = 0.0f;
      m_curAnim = ref;
      m_flags |= 0x10000;
      if (t) {
        m_sim->m_anim.SetTrackSpeed(t, m_timeMult);
        t->m_endMode = 0;
      }
    }
    selectIdleOverlay();
  }
  m_routing = 2;
  m_flags &= ~0x60u;
  if (count < 2) {
    m_pos[0] = static_cast<float>(getPersonX());
    m_pos[1] = static_cast<float>(getPersonY());
  } else {
    const TileListPoint& q = DestList().m_points[static_cast<size_t>(count - 2)];
    m_pos[0] = static_cast<float>(q.p[1]);
    m_pos[1] = static_cast<float>(q.p[0]);
  }
  return 0;
}

// 8005FB50
int SAnimator2::followStandToTurnUpdate(float& desired, float& delta, float dt) {
  if (delta == 0.0f) return 1;
  if (!(m_flags & 0x40)) initShuffleRotation(delta);
  return rotateAnimation(dt, desired, delta);
}

// 8005FBD0
// A side step along the legs (at normal speed or less, its style follows the side step's phase: 4
// stepping, 5 moving, 6 ending), the angle of the step kept in +0x110 for its animation. 1: done.
int SAnimator2::followSidestepUpdate(float& desired, float& delta, float dt) {
  int done = 0;
  float time = dt;
  if (m_flags & 2) {
    int style = 5;
    bool set = true;
    if (m_timeMult * m_pauseMult < 2.0f) {
      set = false;
      const double phase = m_sim->m_anim.GetTrackPos(eTrackFlags{eTrackFlags::kMain});
      if (m_walkStyle == 4 && 0.25 < phase) style = 5, set = true;
      else if (m_walkStyle == 5 && 0.699999988079071 < phase) style = 6, set = true;
      else if (m_walkStyle == 6 && !(0.25 < phase)) style = 4, set = true;
    }
    if (set) m_walkStyle = style;
  }
  if (m_routeIndex == 0) m_routeIndex = 1;
  const int last = DestCount() - 1;
  if (0.0f < time && m_routeIndex < last) {
    do {
      if (m_pathStartsHere < m_routeIndex) sidestepAlongNode(time);
      if (0.0f < time) {
        ++m_routeIndex;
        updateDesiredAndDeltaDir(desired, delta);
      }
    } while (0.0f < time && m_routeIndex < last);
  }
  float angle;
  if (m_routeIndex < last) {
    angle = delta;
  } else {
    done = 1;
    angle = 0.0f;
  }
  m_walkBlend = angle;
  return done;
}

// 8005FDBC
// Along the legs by this frame's time; 1 once at the last point.
int SAnimator2::followMiddleUpdate(float& desired, float& delta, float dt) {
  float time = dt;
  const int count = DestCount();
  if (0.0f < dt) {
    if (count - 1 <= m_routeIndex) return 1;
    do {
      if (m_pathStartsHere < m_routeIndex) advanceAlongNode(time);
      if (0.0f < time) {
        ++m_routeIndex;
        updateDesiredAndDeltaDir(desired, delta);
      }
    } while (0.0f < time && m_routeIndex < count - 1);
  }
  return count - 1 <= m_routeIndex ? 1 : 0;
}

// 8005FEB8
int SAnimator2::followMoveToTurnUpdate(float& desired, float& delta, float dt) {
  if (!(m_flags & 0x40)) initShuffleRotation(delta);
  return rotateAnimation(dt, desired, delta);
}

// 8005FF1C
int SAnimator2::followDoneUpdate(float& desired, float& delta, float dt) {
  if (!(m_flags & 0x40)) initShuffleRotation(delta);
  return rotateAnimation(dt, desired, delta);
}

// 8006028C
// One frame of the follow, going on through the modes while each finishes within the frame: 1 the
// starting turn (in a door: none) then 3 the walk (velocity from its animation), 2 the side step, 3
// walking the legs turning into each (unless the starting turn ran this frame) then 4 the turn along
// the last leg, then 5 turning to face the person's direction and done (endFollowDone); then the
// person placed (endMoveAnimation).
void SAnimator2::moveAnimation() {
  const float dt = ScaledDt();
  bool turned = false;
  float desired = static_cast<float>(getDesiredDir());
  float delta = static_cast<float>(getDeltaDirFromDesiredDir(desired));
  int r = 0;
  do {
    const int mode = m_followMode;
    if (mode == 3) {
      m_portalModeSet = 0;
      if (m_portalMode == 2) m_portalMode = 0;
      r = followMiddleUpdate(desired, delta, dt);
      if (!turned) {
        if (delta == 0.0f) m_dir = desired;
        else rotateAnimation(dt, desired, delta);
        updateDesiredAndDeltaDir(desired, delta);
      }
      if (turned) r = 0;
      if (r != 1) break;
      m_followMode = 4;
      m_flags &= ~0x5eu;
      r = 1;
      m_useSpeed = 0.0f;
      dropMonitoredTrack();
    } else if (mode == 1) {
      UpdatePortalMode();
      if (m_portalMode == 2) {
        r = 1;
      } else if (delta == 0.0f) {
        r = 1;
        m_dir = desired;
      } else {
        r = followStandToTurnUpdate(desired, delta, dt);
        turned = true;
      }
      if (r != 1) break;
      updateDesiredAndDeltaDir(desired, delta);
      m_followMode = 3;
      m_flags &= ~0x5eu;
      setMovementVelocityFromAnimation();
      r = 1;
    } else if (mode == 2) {
      r = followSidestepUpdate(desired, delta, dt);
      if (r != 1) break;
      updateDesiredAndDeltaDir(desired, delta);
      m_followMode = 4;
      m_flags &= ~0x5eu;
      r = 1;
      m_useSpeed = 0.0f;
      dropMonitoredTrack();
    } else if (mode == 4) {
      if (delta == 0.0f) {
        r = 1;
        m_dir = desired;
      } else {
        r = followMoveToTurnUpdate(desired, delta, dt);
      }
      if (r != 1) break;
      m_followMode = 5;
      m_flags = (m_flags & ~0x5eu) | 0x20;
      r = 1;
      m_useSpeed = 0.0f;
      dropMonitoredTrack();
    } else if (mode == 5) {
      desired = static_cast<float>(getPersonDirection());
      delta = static_cast<float>(getDeltaDirFromDesiredDir(desired));
      if (delta == 0.0f) {
        r = 1;
        m_dir = desired;
      } else {
        r = followDoneUpdate(desired, delta, dt);
      }
      if (r != 1) break;
      r = endFollowDone();
    } else {
      break;
    }
  } while (r == 1);
  endMoveAnimation();
}

// 8005FF80
// The person placed (CanPlace / Place) at the point of its path nearest the sim (at the end: the
// second-to-last point); where it cannot be, the follow is blocked (m_routing 3). It faces the sim's
// way.
void SAnimator2::endMoveAnimation() {
  cXPersonImpl* p = Impl(m_person);
  const int count = DestCount();
  int pos[2];
  if (count < 2) {
    // PC: a list without its facing point (GC reads before the list).
    pos[1] = getPersonX();
    pos[0] = getPersonY();
  } else if (m_routeIndex < count - 1) {
    pos[1] = static_cast<int>(m_pos[0] + 0.5f);
    pos[0] = static_cast<int>(m_pos[1] + 0.5f);
    DestList().FindNearestPoint(pos, -1);
  } else {
    const TileListPoint& q = DestList().m_points[static_cast<size_t>(count - 2)];
    pos[1] = q.p[1];
    pos[0] = q.p[0];
  }
  if (!ObjCanPlace(p, pos, p->m_level, nullptr, 0)) m_routing = 3;
  else p->PlacePerson(pos);
  setPersonDirection(m_dir);
}

// 800600E0
// Once per leg: a route through a door (more than one route on the stack) whose last tile is outside
// the person's room enters portal mode 1; then (back in the room, or no door) mode 1 becomes 2 (and an
// NPC's auto-run is checked again) and 2 becomes 0.
void SAnimator2::UpdatePortalMode() {
  if (m_portalModeSet != 0) return;
  cXPersonImpl* p = Impl(m_person);
  const int count = DestCount();
  bool advance = false;
  if (static_cast<int>(p->GetRouteStackSize()) < 2 || count < 3) {
    advance = true;
  } else {
    const TileListPoint& q = DestList().m_points[static_cast<size_t>(count - 2)];
    CTilePt tile(q.p[1] >> 4, q.p[0] >> 4, p->m_level);  // GC: CTilePt(FTilePt&, level)
    Room* room = RoomManager::GetRoomManager() ? RoomManager::GetRoomManager()->GetRoom(p->GetCurrentRoom()) : nullptr;
    if (room) {
      if (!room->IsTileInRoom(tile)) m_portalMode = 1;
      else advance = true;
    }
  }
  if (advance && m_portalMode != 0) {
    if (m_portalMode == 1) {
      m_portalMode = 2;
      if (!(m_flags & 1)) UpdateNPCAutoRun();
    } else {
      m_portalMode = 0;
    }
  }
  m_portalModeSet = 1;
}

// 8006AF28
int SAnimator2::UpdateNPCAutoRunState() {
  if (m_autoRun == 0 || m_lowMotive != 0) return 0;
  if (Impl(m_person)->GetCurrentRoom() == 0) {
    if (m_walkStyle != 1) {
      m_walkStyle = 1;
      return 1;
    }
  } else if (m_walkStyle != 0) {
    m_walkStyle = 0;
    return 1;
  }
  return 0;
}

// 8006AFC4
// An auto-running NPC runs outside (room 0) and walks inside: on a change the cycle switches (its
// velocity read once) keeping its phase.
void SAnimator2::UpdateNPCAutoRun() {
  if (!UpdateNPCAutoRunState()) return;
  setMovementVelocityFromAnimation();
  const AnimRef* ref = nullptr;
  getWalkRunSkillID(ref);
  if (!ref) return;
  EAnimController& anim = m_sim->m_anim;
  const float phase = static_cast<float>(anim.GetTrackPos(eTrackFlags{eTrackFlags::kMain}));
  anim.SetTrackAnim(eTrackFlags{2}, AnimRefId(ref), 2, kWalkRunBlendTime, nullptr);
  anim.SetTrackPos(eTrackFlags{eTrackFlags::kMain}, phase, true);
  m_eventFrame = 0;
  m_flags |= 0x10000;
  m_eventFraction = 0.0f;
  m_curAnim = ref;
}

// 80060A84
// Toward the current point at the side step's style velocity for `time`, the time left over kept.
void SAnimator2::sidestepAlongNode(float& time) {
  const float v = StyleVelocity(m_walkStyle);
  const TileListPoint& q = DestList().m_points[static_cast<size_t>(m_routeIndex)];
  const float tx = static_cast<float>(q.p[1]), ty = static_cast<float>(q.p[0]);
  const float dx = tx - m_pos[0], dy = ty - m_pos[1];
  const float dist = std::sqrt(dx * dx + dy * dy);
  if (0.0001f <= dist) {
    const float step = v * time;
    if (step <= dist || step <= 0.0f) {
      time = 0.0f;
      m_pos[0] = m_pos[0] + dx * (1.0f / dist) * step;
      m_pos[1] = m_pos[1] + dy * (1.0f / dist) * step;
    } else {
      time = time * ((step - dist) / step);
      m_pos[1] = ty;
      m_pos[0] = tx;
    }
  } else {
    m_pos[0] = tx;
    m_pos[1] = ty;
  }
}

// 80060C20
// A leg longer than a sixteenth of a tile or so is walked (moveTowardsDestination).
void SAnimator2::advanceAlongNode(float& time) {
  const std::vector<TileListPoint>& p = DestList().m_points;
  const TileListPoint& q = p[static_cast<size_t>(m_routeIndex)];
  const TileListPoint& o = p[static_cast<size_t>(m_routeIndex - 1)];
  const int dx = q.p[1] - o.p[1], dy = q.p[0] - o.p[0];
  if (3 < dx * dx + dy * dy) {
    EVec2 to{static_cast<float>(q.p[1]), static_cast<float>(q.p[0])};
    moveTowardsDestination(time, to);
  }
}

// 80060CE0
// The speed this frame: on the last legs, slowing (over `decel` seconds to stop) as the end comes
// near (pets and doors at 30% at least, a door at full speed), else picking up from +0x114 by the
// frame's acceleration; at most `velocity`.
double SAnimator2::getUseSpeed(float velocity, float decel, float dist, float time) {
  const double v = velocity;
  const float accel = static_cast<float>(-v / decel);
  const float stopDist = static_cast<float>(static_cast<float>(static_cast<float>(accel * 0.5f) * decel) * decel +
                                            static_cast<float>(v * decel));
  double speed;
  if (stopDist <= dist || m_routeIndex < DestCount() - 2) {
    speed = static_cast<float>(-accel * time + m_useSpeed);
  } else if (Impl(m_person)->GetData(9) == 0) {
    const float root = std::sqrt(static_cast<float>(v * v + static_cast<double>(static_cast<float>(accel + accel) * static_cast<float>(stopDist - dist))));
    speed = static_cast<float>(v + static_cast<float>(-v + root)) * 1.001;
  } else {
    float f = std::sqrt(static_cast<float>(dist / stopDist));
    if (f < 0.30000001192092896f) f = 0.30000001192092896f;
    speed = v;
    if (m_portalMode != 1) speed = static_cast<float>(f * v);
  }
  if (v < speed) speed = v;
  return speed;
}

// 80060E30
// On the last leg when the stopping distance covers what is left (not in a door): the walk fades
// into the idle (flag 8).
void SAnimator2::EnableWalkFade(unsigned int& flags, float dist, float velocity, float decel) {
  const int lastLeg = DestCount() - 2;
  if (lastLeg <= m_routeIndex &&
      (lastLeg < m_routeIndex ||
       static_cast<float>(dist / static_cast<float>(static_cast<float>(decel * decel) * static_cast<float>(-velocity / decel) * 0.5f +
                                                   static_cast<float>(velocity * decel))) < 1.0f)) {
    if (m_portalMode != 1) flags |= 8;
    return;
  }
  flags &= ~8u;
}

// 80060EBC
// Toward `to` for `time` at the speed getUseSpeed gives (decelerating over 0.5 s of game time), the
// time left over kept; the walk's share of the walk cycle (+0x110) while walking (mode 3).
void SAnimator2::moveTowardsDestination(float& time, EVec2& to) {
  const float velocity = StyleVelocity(m_walkStyle);
  const float dy = to.y - m_pos[1];
  const float dx = to.x - m_pos[0];
  const float dist = std::sqrt(dx * dx + dy * dy);
  if (9.999999747378752e-05 <= dist) {
    const double inv = 1.0 / dist;
    const float decel = m_timeMult == 0.0f ? 1e+12f : 0.5f / (m_timeMult * m_pauseMult);
    const double speed = getUseSpeed(velocity, decel, dist, time);
    EnableWalkFade(m_flags, dist, velocity, decel);
    m_useSpeed = static_cast<float>(speed);
    const double step = static_cast<float>(speed * time);
    if (step <= dist || step <= 0.0) {
      time = 0.0f;
      m_pos[0] = m_pos[0] + static_cast<float>(static_cast<double>(dx * static_cast<float>(inv)) * step);
      m_pos[1] = m_pos[1] + static_cast<float>(static_cast<double>(dy * static_cast<float>(inv)) * step);
    } else {
      time = static_cast<float>(time * static_cast<double>(static_cast<float>(static_cast<float>(step - dist) / step)));
      m_pos[0] = to.x;
      m_pos[1] = to.y;
    }
    m_walkBlend = static_cast<float>(speed / velocity);
    if (m_followMode != 3) m_walkBlend = 0.0f;
  } else {
    dropMonitoredTrack();
    m_walkBlend = 0.0f;
    m_flags &= ~8u;
  }
}

// 800620AC
// Walking the legs (mode 3): the style's cycle on the walk track (flag 2) once per follow (an NPC
// auto-run picks run outside, walk inside; ghosts their own; low-motive walks once loaded), fading in
// over the outgoing track (4), a running start on the other foot; at the end (8) the base idle fades
// in under it as the walk slows (+0x110). Played at the simulator's speed.
void SAnimator2::handleWalkRunAnimation() {
  if (m_followMode != 3) {
    m_walkStarted = 0;
    return;
  }
  EAnimController& anim = m_sim->m_anim;
  if (!(m_flags & 2)) {
    if (!(m_flags & 1) && m_autoRun != 0 && m_lowMotive == 0) {
      if (Impl(m_person)->GetCurrentRoom() == 0) {
        if (m_walkStyle != 1) m_walkStyle = 1;
      } else if (m_walkStyle != 0) {
        m_walkStyle = 0;
      }
    }
    const AnimRef* ref = nullptr;
    getWalkRunSkillID(ref);
    EACTrack* outgoing = nullptr;
    if (ref) anim.SetTrackAnim(eTrackFlags{2}, AnimRefId(ref), 4, -1.0f, &outgoing);
    if (m_shuffleDir < 0) anim.SetTrackPos(eTrackFlags{eTrackFlags::kMain}, 0.5f, true);
    m_eventFrame = 0;
    m_curAnim = ref;
    m_flags |= 0x10000;
    m_eventFraction = 0.0f;
    if (outgoing) {
      dropMonitoredTrack();
      m_monitoredTrack = outgoing;
      m_flags |= 4;
    }
    m_flags |= 2;
  }
  if (m_flags & 4) {
    if (m_monitoredTrack) {
      float i = 1.0f - m_walkBlend;
      if (i < 9.999999747378752e-05f) i = 0.0f;
      anim.SetTrackIntensity(m_monitoredTrack, i, false);
      if (i == 0.0f) {
        m_monitoredTrack = nullptr;
        m_flags &= ~4u;
      }
    }
  } else if (m_flags & 8) {
    if (!(MainTrackFlags() & 1)) {
      const AnimRef* idle = nullptr;
      GetStdAnimRef(m_person, 3, idle);
      EACTrack* outgoing = nullptr;
      if (idle) anim.SetTrackAnim(eTrackFlags{1}, AnimRefId(idle), 4, -1.0f, &outgoing);
      m_eventFrame = 0;
      m_flags |= 0x10000;
      m_curAnim = idle;
      m_eventFraction = 0.0f;
      if (outgoing) {
        dropMonitoredTrack();
        m_monitoredTrack = outgoing;
      }
    }
    if (m_monitoredTrack) {
      float i = m_walkBlend;
      if (i < 9.999999747378752e-05f) i = 0.0f;
      anim.SetTrackIntensity(m_monitoredTrack, i, false);
      if (i == 0.0f) {
        m_monitoredTrack = nullptr;
        m_flags &= ~8u;
      }
    }
  }
  anim.SetTrackSpeed(eTrackFlags{eTrackFlags::kMain}, m_timeMult * m_pauseMult);
  m_walkStarted = 1;
}

// 8006AC98
// The side step (std anims 0x27 right / 0x28 left) and the walk it blends with (0x29 / 0x2A forward,
// 0x2B / 0x2C backward) for a step at `angle` from the facing.
void SAnimator2::getSidestepSkillIDs(const AnimRef*& side, const AnimRef*& walk, float angle) {
  const bool forward = !(angle < -1.5707963705062866f || 1.5707963705062866f < angle);
  if (0.0f < angle) {
    GetStdAnimRef(m_person, 0x27, side);
    GetStdAnimRef(m_person, forward ? 0x29 : 0x2b, walk);
  } else {
    GetStdAnimRef(m_person, 0x28, side);
    GetStdAnimRef(m_person, forward ? 0x2a : 0x2c, walk);
  }
}

// 8006B4D0
double SAnimator2::getSidestepIntensity() {
  const float s = std::sin(m_walkBlend);
  double i = s * s;
  if (9.999999747378752e-05 <= i) {
    if (0.9998999834060669 < i) i = 1.0;
  } else {
    i = 0.0;
  }
  return i;
}

// 8006B52C
// The side step: its velocity read once; straight sideways the side step alone (track 0x40),
// otherwise the side step as a static blend phase-locked to the walk (track 2) at the step's sine.
void SAnimator2::initSidestep() {
  dropMonitoredTrack();
  const AnimRef* side = nullptr;
  const AnimRef* walk = nullptr;
  getSidestepSkillIDs(side, walk, m_walkBlend);
  setSideStepSpeedFromAnimation(walk);
  EAnimController& anim = m_sim->m_anim;
  const float intensity = static_cast<float>(getSidestepIntensity());
  if (intensity == 1.0f) {
    if (EACTrack* t = side ? anim.SetTrackAnim(eTrackFlags{0x40}, AnimRefId(side), 2, kSidestepBlendTime, nullptr) : nullptr)
      t->m_endMode = 0;
  } else {
    EACTrack* s = side ? anim.SetTrackAnim(eTrackFlags{0x40}, AnimRefId(side), 2, kSidestepBlendTime, nullptr) : nullptr;
    m_monitoredTrack = s;
    if (s) {
      s->m_flags &= ~static_cast<unsigned int>(eTrackFlags::kStarted);
      s->m_endMode = 0;
    }
    EACTrack* w = walk ? anim.SetTrackAnim(eTrackFlags{2}, AnimRefId(walk), 5, -1.0f, nullptr) : nullptr;
    if (w) w->m_endMode = 0;
    anim.SetTrackPhaseLock(m_monitoredTrack, w, 0.0f);
    if (m_monitoredTrack) anim.SetTrackIntensity(m_monitoredTrack, intensity, false);
  }
  anim.SetTrackSpeed(eTrackFlags{eTrackFlags::kMain}, (m_timeMult * 24.0f) / m_sideVel);
  m_flags |= 2;
}

// 80061C1C
// A side step (mode 2), once: initSidestep's animations.
void SAnimator2::handleSidestepAnimation() {
  if (m_followMode != 2 || (m_flags & 2)) return;
  initSidestep();
}

// 8006B9A4
// The walk cycle (handleWalkRunAnimation's start, on its own).
void SAnimator2::initWalkRun() {
  if (!(m_flags & 1) && m_autoRun != 0 && m_lowMotive == 0) {
    if (Impl(m_person)->GetCurrentRoom() == 0) {
      if (m_walkStyle != 1) m_walkStyle = 1;
    } else if (m_walkStyle != 0) {
      m_walkStyle = 0;
    }
  }
  const AnimRef* ref = nullptr;
  getWalkRunSkillID(ref);
  EACTrack* outgoing = nullptr;
  EAnimController& anim = m_sim->m_anim;
  if (ref) anim.SetTrackAnim(eTrackFlags{2}, AnimRefId(ref), 4, -1.0f, &outgoing);
  if (m_shuffleDir < 0) anim.SetTrackPos(eTrackFlags{eTrackFlags::kMain}, 0.5f, true);
  m_eventFrame = 0;
  m_curAnim = ref;
  m_flags |= 0x10000;
  m_eventFraction = 0.0f;
  if (outgoing) {
    dropMonitoredTrack();
    m_monitoredTrack = outgoing;
    m_flags |= 4;
  }
  m_flags |= 2;
}

// 8006BC94
// The base idle (std anim 3) under the walk, the walk monitored as it fades.
void SAnimator2::initWalkRunFadeOut() {
  const AnimRef* idle = nullptr;
  GetStdAnimRef(m_person, 3, idle);
  EACTrack* outgoing = nullptr;
  if (idle) m_sim->m_anim.SetTrackAnim(eTrackFlags{1}, AnimRefId(idle), 4, -1.0f, &outgoing);
  m_eventFrame = 0;
  m_flags |= 0x10000;
  m_curAnim = idle;
  m_eventFraction = 0.0f;
  if (outgoing) {
    dropMonitoredTrack();
    m_monitoredTrack = outgoing;
  }
}

// ---------------------------------------------------------------------------------------------------
// The awareness follow (the awareness manager's reaction 3): turn toward what the sim is aware of
// (mode 6), its gesture plays (7), then back to the follow's way or standing (8).

// 80068AB4
void SAnimator2::setAwarenessFollowStart(float angle) {
  m_followMode = 6;
  m_flags &= ~0x7eu;
  m_sim->m_anim.BlendAllOutgoingTracks(0.0f, 0.1f, true);
  m_monitoredTrack = nullptr;
  m_desiredDir = Wrap(angle + m_dir);
}

// 8006C664
// The gesture: an idle overlay fades out.
void SAnimator2::setAwarenessFollowMiddle() {
  m_followMode = 7;
  if (m_flags & 0x1000) {
    m_flags &= ~0x1000u;
    m_sim->m_anim.SetTrackBlendSmooth(eTrackFlags{0x100}, 0.0f, kExitIdleOverlayBlend, 0.01f);
  }
  m_flags &= ~0x5eu;
}

// 8006C490
// Back: following, toward the leg walked (as getDesiredDir); else the sim's facing.
void SAnimator2::setAwarenessFollowEnd() {
  m_followMode = 8;
  m_flags &= ~0x5eu;
  m_useSpeed = 0.0f;
  if (m_routing == 1) {
    const std::vector<TileListPoint>& p = DestList().m_points;
    const int i = m_routeIndex;
    float dx, dy;
    if (i == 0) {
      dx = static_cast<float>(p[1].p[1] - p[0].p[1]);
      dy = static_cast<float>(p[1].p[0] - p[0].p[0]);
    } else {
      dx = static_cast<float>(p[i].p[1] - p[i - 1].p[1]);
      dy = static_cast<float>(p[i].p[0] - p[i - 1].p[0]);
    }
    float d;
    if (dx <= -0.001f || 0.001f <= dx || dy <= -0.001f || 0.001f <= dy) d = std::atan2(-dy, dx);
    else d = m_dir;
    m_desiredDir = d;
  } else {
    m_desiredDir = m_dir;
  }
}

// 8006C6DC
// The awareness follow is over: no desired direction; following, the walk goes on (mode 3, the walk
// style chosen again); else standing with the default idle.
void SAnimator2::exitAwarenessFollow() {
  m_desiredDir = kNoDir;
  m_useSpeed = 0.0f;
  m_flags &= ~0x5eu;
  if (m_routing == 1) {
    m_followMode = 3;
    setMovementVelocityFromAnimation();
    UnloadDCOptionalMotionAnims();
    UnloadSMOptionalMotionAnims();
    LoadSMOptionalMotionAnims();
    const int style = followWalkStyle(0.0f);
    if (style != m_walkStyle) {
      const int old = m_walkStyle;
      m_walkStyle = style;
      m_prevWalkStyle = old;
    }
  } else {
    m_followMode = 0;
    StartDefaultIdle();
  }
}

// 8006067C
// The awareness follow's frame: 6 turning toward the object (then 7), 7 while the gesture plays (it
// starts once loaded; done: 8), 8 turning back (then exitAwarenessFollow); the person placed where the
// sim stands if it can be, facing its way.
void SAnimator2::awarenessMove() {
  const float dt = ScaledDt();
  bool again = false;
  do {
    const int mode = m_followMode;
    if (mode == 7) {
      again = false;
      if (m_aware.m_action == 3) {
        if (!(m_aware.m_flags & AwarenessManager::kMemoryAnimPending)) {
          if (m_aware.IsMemoryAwarenessAnimDone()) {
            again = true;
            setAwarenessFollowEnd();
          }
        } else {
          StartDefaultIdle();
        }
      } else {
        again = true;
        setAwarenessFollowEnd();
      }
    } else if (mode == 6) {
      float delta = static_cast<float>(getDeltaDirFromDesiredDir(m_desiredDir));
      int r;
      if (delta == 0.0f) {
        r = 1;
        m_dir = m_desiredDir;
      } else {
        r = followMoveToTurnUpdate(m_desiredDir, delta, dt);
      }
      if (r != 1) break;
      setAwarenessFollowMiddle();
      again = true;
    } else if (mode == 8) {
      float delta = static_cast<float>(getDeltaDirFromDesiredDir(m_desiredDir));
      int r;
      if (delta == 0.0f) {
        r = 1;
        m_dir = m_desiredDir;
      } else {
        r = followStandToTurnUpdate(m_desiredDir, delta, dt);
      }
      if (r != 1) break;
      again = false;
      exitAwarenessFollow();
    } else {
      break;
    }
  } while (again);
  cXPersonImpl* p = Impl(m_person);
  const int pos[2] = {static_cast<int>(m_pos[1] + 0.5f), static_cast<int>(m_pos[0] + 0.5f)};
  if (ObjCanPlace(p, pos, p->m_level, nullptr, 0)) p->PlacePerson(pos);
  setPersonDirection(m_dir);
}

// 80068A40
cXObject* SAnimator2::GetAwareOfObject() { return m_aware.m_awareOf; }
// 80068A48
void SAnimator2::SetAwareOfObject(cXObject* object) { m_aware.SetAwareOfObject(object); }
// 80068A6C
void SAnimator2::SetAwareOfObjectKilled(cXObject* object) { m_aware.SetAwareOfObjectKilled(object); }
// 80068A90
void SAnimator2::ClearAwareOfObject(cXObject* object) { m_aware.ClearAwareOfObject(object); }
// 8005C76C
void SAnimator2::SetIsRaining(bool raining) { AwarenessManager::SetIsRaining(raining); }
// 80069024
double SAnimator2::SetPauseMultiplier(float mult) {
  const float old = m_pauseMult;
  m_aware.SetPauseMultiplier(mult);
  m_pauseMult = mult;
  return old;
}
// 800690D0
double SAnimator2::GetTimeMultiplier() { return m_timeMult; }
// 8006918C
double SAnimator2::GetPauseMultiplier() { return m_pauseMult; }
// 800690F0
int SAnimator2::GetFollowMode() { return m_followMode; }
// 800690F8
int SAnimator2::GetPortalMode() { return static_cast<signed char>(m_portalMode); }
// 8005D6A0
int SAnimator2::IsSkillIdleAnimRunning() { return (MainTrackFlags() >> 5) & 1; }
// 8005E7C8
void SAnimator2::SetAnimDisplacements(float, float, float) {}
// 8005C78C
void SAnimator2::Render(int) {}
// 8005D120
void SAnimator2::ResetSuits() {}
// 8005F4E4
void SAnimator2::SetPixelated(int) {}

// 80068DD0
// Energy or bladder (the lower; bladder on a tie) under -75, unless carrying a direct-control object.
int SAnimator2::shouldUseLowMotiveWalk() {
  cXPersonImpl* p = Impl(m_person);
  if (p->IsCarryingDCObject()) return 0;
  int motive = 5;
  double low = p->GetMotive(5);
  if (p->GetMotive(9) <= low) {
    motive = 9;
    low = p->GetMotive(9);
  }
  return low < -75.0 ? motive : 0;
}

namespace {
// The low-motive walk of a motive (energy std anim 0x12, bladder 0x3B), or null.
const AnimRef* LowMotiveWalkRef(cXPerson* person, int motive) {
  const AnimRef* ref = nullptr;
  if (motive == 5) GetStdAnimRef(person, 0x12, ref);
  else if (motive == 9) GetStdAnimRef(person, 0x3b, ref);
  return ref;
}
void ReleaseAnim(const AnimRef* ref) {
  if (!ref) return;
  if (!_animman.GetRef(AnimRefId(ref))) _animman.DelRefAsync(AnimRefId(ref));
  else _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
}
}  // namespace

// 800645C8
// For a follow (the simulator's): the low-motive walk (shouldUseLowMotiveWalk, +0x5F9) loaded in the
// background (flag 0x2000000); the direct-control one given back first.
void SAnimator2::LoadSMOptionalMotionAnims() {
  if (m_flags & 0x400000) UnloadDCOptionalMotionAnims();
  if (m_flags & 0x2000000) return;
  m_lowMotive = static_cast<unsigned char>(shouldUseLowMotiveWalk());
  if (m_lowMotive == 0) return;
  m_flags |= 0x2000000;
  if (const AnimRef* ref = LowMotiveWalkRef(m_person, m_lowMotive)) _animman.AddRefAsync(AnimRefId(ref));
}

// 8006468C
// For direct control (flag 0x400000), as LoadSMOptionalMotionAnims.
void SAnimator2::LoadDCOptionalMotionAnims() {
  if (m_flags & 0x2000000) UnloadSMOptionalMotionAnims();
  if (m_flags & 0x400000) return;
  m_lowMotive = static_cast<unsigned char>(shouldUseLowMotiveWalk());
  if (m_lowMotive == 0) return;
  m_flags |= 0x400000;
  if (const AnimRef* ref = LowMotiveWalkRef(m_person, m_lowMotive)) _animman.AddRefAsync(AnimRefId(ref));
}

// 80064A40
void SAnimator2::UnloadDCOptionalMotionAnims() {
  if (!(m_flags & 0x400000)) return;
  m_flags &= ~0x400000u;
  ReleaseAnim(LowMotiveWalkRef(m_person, m_lowMotive));
  m_lowMotive = 0;
}

// 80064B2C
void SAnimator2::UnloadSMOptionalMotionAnims() {
  if (!(m_flags & 0x2000000)) return;
  m_flags &= ~0x2000000u;
  ReleaseAnim(LowMotiveWalkRef(m_person, m_lowMotive));
  m_lowMotive = 0;
}

// 80064858
// A long idle loading (flag 0x80000) is given back.
void SAnimator2::UnloadPendingLongIdleAnim() {
  m_longIdleCount = 0;
  if (!(m_flags & 0x80000)) {
    m_longIdle = 0;
    return;
  }
  if (m_longIdle == 0) return;
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, m_longIdle, ref);
  ReleaseAnim(ref);
  m_longIdle = 0;
  m_flags &= ~0x80000u;
}

// 8006491C
// The low-motive idle loading (flag 0x100000) given back; no low-motive idle (and not loaded, 0x8000000).
void SAnimator2::UnloadOptionalIdleAnim() {
  if (m_flags & 0x100000) {
    int id = -1;
    switch (m_lowMotiveIdle) {
      case 8: id = 0x3f; break;
      case 5: id = 0x3c; break;
      case 7: id = 0x3e; break;
      case 0xe: id = 0x40; break;
      case 9: id = 0x3a; break;
      case 0xf: id = 0x3d; break;
      default: break;
    }
    if (id != -1) {
      const AnimRef* ref = nullptr;
      GetStdAnimRef(m_person, id, ref);
      ReleaseAnim(ref);
    }
    m_flags &= ~0x100000u;
  }
  m_lowMotiveIdle = 0;
  m_flags &= ~0x8000000u;
}

// 800617BC
// A motive's value (-100..100) as 0..1.
double GetMotiveMag(float value) {
  const double m = static_cast<float>(value + 100.0f) / 200.0f;
  if (!(0.0 <= m)) return 0.0;
  return 1.0 < m ? 1.0 : m;
}

// 80061804
// The mood on the face (not pets): the happy (0x6EE972F7) or sad (0xF4E342DB) pose on the head's
// track 0x2000, held still, as strong as the mood is far from neutral.
void SAnimator2::handleMoodAnimations() {
  cXPersonImpl* p = Impl(m_person);
  if (p->IsPet()) return;
  unsigned int id = 0x6ee972f7u;
  double mag = static_cast<float>(GetMotiveMag(static_cast<float>(p->GetMotive(3))) * 2.0) - 1.0;
  if (mag < 0.0) {
    mag = -mag;
    id = 0xf4e342dbu;
  }
  EAnimController& anim = m_sim->m_anim;
  if (id != m_moodAnim) {
    m_moodAnim = id;
    anim.StopTrack(eTrackFlags{0x2000});
    if (id != 0) {
      EACTrack* t = anim.SetTrackAnim(eTrackFlags{0x2000}, id, -1, -1.0f, nullptr);
      anim.SetTrackPos(t, 0.0f, true);
      anim.SetTrackSpeed(t, 0.0f);
      anim.SetTrackBlendFactors(t, m_HeadIndexArray);
    }
  }
  if (anim.IsTrackValid(eTrackFlags{0x2000})) anim.SetTrackIntensity(eTrackFlags{0x2000}, static_cast<float>(mag));
}

// 800635DC
// A skill's idle (track 0x20, state 7): the fidget overlay as selectIdleOverlay (half the time,
// unless something plays over the idle, a pet, the awareness gesture, a low-motive idle or one
// playing); a low-motive idle loading plays once loaded (unless the idle-animate primitive keeps the
// skill playing as its idle, 0x200000).
void SAnimator2::handleSkillIdleAnimation() {
  m_sim->DrawSim(true);
  EAnimController& anim = m_sim->m_anim;
  if ((m_flags & 0x1000) && anim.IsTrackAnimComplete(eTrackFlags{0x100}) && (m_flags & 0x1000)) {
    m_flags &= ~0x1000u;
    anim.SetTrackBlendSmooth(eTrackFlags{0x100}, 0.0f, kExitIdleOverlayBlend, 0.01f);
  }
  cXPersonImpl* p = Impl(m_person);
  if (!(m_flags & 0x2000) && p->IsPet() != 1 && m_animState != 0x10 && !shouldUseLowMotiveIdle() &&
      !(m_flags & 0x4000000) && !(m_flags & 0x1000) && simmy::sim_rand() % 100 < 0x32) {
    m_flags |= 0x1000;
    int weights[5];
    weights[0] = 0x14;
    weights[1] = (p->IsCarrying() || (m_flags & 0x800) || (m_aware.m_flags & 0x200)) ? 0x14 : 0x28;
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
    if (ref) {
      if (EACTrack* t = anim.SetTrackAnim(eTrackFlags{0x100}, AnimRefId(ref), -1, -1.0f, nullptr)) {
        anim.SetTrackIntensity(t, 0.0001f, true);
        anim.SetTrackBlendFactors(t, m_UpperBodyIndexArray);
        anim.SetTrackBlendSmooth(t, 1.0f, kDefaultBlendTime, 0.01f);
        t->m_endMode = 1;
      }
    }
  }
  if (m_flags & 0x200000) return;
  if (!(m_flags & 0x100000)) return;
  int id = 3;
  switch (m_lowMotiveIdle) {
    case 8: id = 0x3f; break;
    case 5: id = 0x3c; break;
    case 7: id = 0x3e; break;
    case 0xe: id = 0x40; break;
    case 9: id = 0x3a; break;
    case 0xf: id = 0x3d; break;
    default: break;
  }
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, id, ref);
  if (!ref || !_animman.GetRef(AnimRefId(ref))) return;
  _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
  m_flags = (m_flags & ~0x100000u) | 0x8000000u;
  EACTrack* t = anim.SetTrackAnim(eTrackFlags{0x20}, AnimRefId(ref), -1, -1.0f, nullptr);
  m_eventFrame = 0;
  m_eventFraction = 0.0f;
  m_curAnim = ref;
  m_flags |= 0x10000;
  if (t) {
    anim.SetTrackSpeed(t, m_timeMult);
    t->m_endMode = 0;
  }
}

// 8006C040
bool SAnimator2::skillIsIdle() { return Impl(m_person)->IsIdle() != 0; }

// 8006C094
// The skill track has played out, or (player control) an algorithmic idle plays and actions wait.
int SAnimator2::ShouldEndSkillTrack() {
  if (m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{0x30})) return 1;
  if (!(m_flags & 1)) return 0;
  if (!(MainTrackFlags() & 0x20)) return 0;
  return Impl(m_person)->CountActions(false) > 0 ? 1 : 0;
}

// 800693B8
void SAnimator2::SetSkillName(const AnimRef* ref) {
  m_eventFrame = 0;
  m_flags |= 0x10000;
  m_eventFraction = 0.0f;
  m_curAnim = ref;
}

// 800693E0
// A new desired direction (direct control): a change of more than a sixteenth of a turn restarts
// the turn's start.
void SAnimator2::SetDesiredFacing(float dir) {
  if (m_desiredDir != kNoDir) {
    float d = dir - m_desiredDir;
    while (kPi < d) d -= kTwoPi;
    while (d < -kPi) d += kTwoPi;
    if (0.3926991f < std::fabs(d)) m_turnStartDir = kNoDir;
  }
  m_desiredDir = dir;
}

// 80069470
void SAnimator2::clearSuspendedCarry() { resumeCarryAfterSkill(); }

// 800694E0
void SAnimator2::SetIdleAnimPlaybackParameters(EACTrack* track) {
  if (!track) return;
  m_sim->m_anim.SetTrackSpeed(track, m_timeMult);
  track->m_endMode = 0;
}

// 8006C148
// The held object's model stops fading with the walls; the standard carry pose (std anim 0x26) on the
// right arm unless a carry track plays.
void SAnimator2::startCarry() {
  cXPersonImpl* person = Impl(m_person);
  cXObjectImpl* held = person->GetContainedObject(0);
  m_carryObject = held ? held->m_id : 0;
  if (m_carryObject && held->m_instance) held->m_instance->m_interestFade = 0;
  EAnimController& ctl = m_sim->m_anim;
  if (ctl.IsTrackValid(eTrackFlags{0x800})) return;
  m_flags |= 0x800;
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, 0x26, ref);
  EACTrack* t = ref ? ctl.SetTrackAnim(eTrackFlags{0x800}, AnimRefId(ref), -1, -1.0f, nullptr) : nullptr;
  if (!t) return;
  ctl.SetTrackIntensity(t, 1.0f, true);
  ctl.SetTrackSpeed(t, m_timeMult);
  lockCarryArmNodes(m_RightArmCarryIndexArray, t);
  if (m_flags & 0x400) ctl.SuspendTrack(t);
}

// 8006C320
// The held object fades with the walls again; the carry pose fades out (or goes at once over a skill,
// which then resumes the track).
void SAnimator2::stopCarry() {
  if (m_carryObject) {
    cXObject* o = Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(m_carryObject) : nullptr;
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    if (impl && impl->m_instance) impl->m_instance->m_interestFade = 1;
    m_carryObject = 0;
  }
  EAnimController& ctl = m_sim->m_anim;
  if (!(m_flags & 0x400)) ctl.SetTrackBlendSmooth(eTrackFlags{0x800}, 0.0f, kCarryBlendTime, 0.01f);
  else ctl.SetTrackIntensity(eTrackFlags{0x800}, 0.0f);
  resumeCarryAfterSkill();
  m_flags &= ~0x800u;
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
