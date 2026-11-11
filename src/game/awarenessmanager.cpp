// Port of game/awarenessmanager.cpp: a person's awareness (see awarenessmanager.hpp). It lives in the
// person's animator (SAnimator2 +0x620), which runs AwarenessCheck at the start of each frame's update
// and handleAwarenessAnimations once the frame's animation state is set.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/passiveinfluence.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/classes/spriteslot.hpp"
#include "simmy/detmath.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
const float kAwarenessBlendTime = 0.4f;  // 804FDAF4
const float kCarryBlendTime = 0.6f;      // 804FDAF8
EVec3 zAxis{0.0f, 0.0f, 1.0f};           // 80473508

cXPersonImpl* PersonOf(cXPerson* p) { return p ? static_cast<cXPersonImpl*>(p) : nullptr; }
cXPersonImpl* PersonOf(cXObject* o) {
  cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
  return impl && impl->GetType() == 2 ? static_cast<cXPersonImpl*>(impl) : nullptr;  // TreeSim::_dyncastimpl(2)
}
// The flags of the controller's current main track (0 without one).
unsigned int MainFlags(EAnimController* anim) {
  return anim->m_mainTrack == -1 ? 0u : anim->m_tracks[static_cast<size_t>(anim->m_mainTrack)].m_flags;
}

bool Trace() {
  static const bool on = std::getenv("SIMMY_AWARE_TRACE") != nullptr;  // PC dev trace
  return on;
}
}  // namespace

int AwarenessManager::m_bIsRaining = 0;

// 80011EEC
AwarenessManager::AwarenessManager() {}

// 80011F4C
// For the animator: its head and torso bones, its speed, the person and its instance; the first
// memory check comes after 0..5 seconds.
void AwarenessManager::Init(SAnimator2* animator, EAnimController* anim) {
  m_anim = anim;
  m_animator = animator;
  m_headBone = animator->m_boneIndex[1];
  m_torsoBone = animator->m_boneIndex[3];
  m_timeMult = animator->m_timeMult;
  m_pauseMult = animator->m_pauseMult;
  m_person = animator->m_person;
  m_angle = 0.0f;
  m_sim = animator->m_sim;
  const int r = simmy::sim_rand();  // GC: rand()
  m_category = -1;
  m_flags &= ~static_cast<unsigned int>(kAwareKilled);
  m_memoryTime = static_cast<float>(r) * 4.656613e-10f * 5.0f;
  m_passiveAnim = nullptr;
  m_passiveTime = 0.0f;
  m_action = 0;
  m_awareOf = nullptr;
  m_targetAngle = 0.0f;
}

// 80012034
// Everything let go of (the gesture, the passive influence animation); only the player control flag
// stays.
void AwarenessManager::Reset() {
  MemoryAwarenessClearAction();
  StopPassiveInfluenceAnimation();
  m_angle = 0.0f;
  const int r = simmy::sim_rand();  // GC: rand()
  m_category = -1;
  m_flags &= kPlayerControl;
  m_memoryTime = static_cast<float>(r) * 4.656613e-10f * 5.0f;
  m_passiveAnim = nullptr;
  m_passiveTime = 0.0f;
  m_action = 0;
  m_awareOf = nullptr;
  m_targetAngle = 0.0f;
}

// 800120EC
AwarenessManager::~AwarenessManager() {
  if (!m_animator) return;
  MemoryAwarenessClearAction();
  StopPassiveInfluenceAnimation();
}

void AwarenessManager::Release() {
  if (!m_animator) return;
  MemoryAwarenessClearAction();
  StopPassiveInfluenceAnimation();
  m_animator = nullptr;
  m_anim = nullptr;
}

// 80012140
// A person in the world with its model, the simulator running: the memory check (not under player
// control), the passive influences, the rain and the head's target.
void AwarenessManager::AwarenessCheck(unsigned int animState) {
  cXObjectImpl* obj = m_person ? m_person->AsObject()->GetObjectImplementation() : nullptr;
  if (!obj || !ObjIsInWorld(obj) || !m_sim || !m_sim->m_modelReady) return;
  if (!Globs_pSimulator || Globs_pSimulator->GetSpeedMultiplier() == 0.0) return;
  if (!(m_flags & kPlayerControl)) MemoryAwarenessCheck(animState);
  PassiveInfluenceAwarenessCheck(animState);
  RainAwarenessCheck();
  UpdateAwarenessAngle(animState);
}

// 8001220C
void AwarenessManager::handleAwarenessAnimations(unsigned int animState) {
  handleMemoryAwarenessAnimation(animState);
  handleAwarenessTurningAnimation();
  handlePassiveInfluenceAnimation(animState);
}

// 80012250
void AwarenessManager::SetIsRaining(bool raining) { m_bIsRaining = raining ? 1 : 0; }

// 80012258
int AwarenessManager::AwarenessAutoRunCheck(float dist) {
  if (m_flags & kPlayerControl) return dist < 4.0f ? 0 : 1;
  if (m_bIsRaining != 1) return 0;
  return dist < 5.0f ? 0 : 1;
}

// 800122B0
void AwarenessManager::SetPlayerControl(bool control) {
  if (control) m_flags |= kPlayerControl;
  else m_flags &= ~static_cast<unsigned int>(kPlayerControl);
}

// 800122D8
EQuat AwarenessManager::GetFirstNodeRotation(float step, float limit) {
  const float angle = m_angle, target = m_targetAngle;
  if (angle != target) {
    float d = step * 3.1415927f;
    if (target - angle < 0.0f) d = -d;
    d = angle + d;
    if ((angle < target && target <= d) || (target < angle && d <= target)) m_angle = m_targetAngle;
    else m_angle = d;
  }
  float a = m_angle;
  if (limit < a) a = limit;
  else if (a < -limit) a = -limit;
  EQuat q;
  q.Set(zAxis, a);
  return q;
}

// 800123D4
EQuat AwarenessManager::GetSecondNodeRotation(float limit) {
  float a = 0.0f;
  if (m_angle > limit) a = m_angle - limit;
  else if (m_angle < -limit) a = m_angle + limit;
  EVec3 axis{0.0f, 0.0f, 1.0f};
  EQuat q;
  q.Set(axis, a);
  return q;
}

namespace {
// A node state turning about z only (no translation, unit scale).
EACNodeState TurnState(const EQuat& q) {
  EACNodeState s;
  s.m_flags |= EACNodeState::kAnimated;
  s.m_rot = q;
  s.m_trans = EVec3{0.0f, 0.0f, 0.0f};
  s.m_scale = EVec3{1.0f, 1.0f, 1.0f};
  return s;
}
}  // namespace

// 80012494
// The procedural track's pose (track 0x4000): the head takes the turn (up to 70 degrees) and the
// torso is layered with the rest. Back at the target with flag 8 (turning back), the turn is over.
void AwarenessManager::AwarenessAnimateHeadLeadsMotion(uintptr_t data, EACTrack* track, EMat4&, ERCharacter*,
                                                       EACNodeState* states) {
  auto* self = reinterpret_cast<AwarenessManager*>(data);
  if (!self || !track || !(self->m_flags & kTurning)) return;
  const float step = _dt * self->m_timeMult * self->m_pauseMult;
  EACNodeState head = TurnState(self->GetFirstNodeRotation(step, 1.2217305f));
  EAnimController::Blend(1.0f, states[self->m_headBone], head);
  EACNodeState torso = TurnState(self->GetSecondNodeRotation(1.2217305f));
  EAnimController::Layer(1.0f, states[self->m_torsoBone], torso);
  if (self->m_angle == self->m_targetAngle && (self->m_flags & kStopTurning)) self->m_flags &= ~static_cast<unsigned int>(kTurning);
}

// 80012688
// The torso turns first (up to 50 degrees, layered), the head blended with the rest.
void AwarenessManager::AwarenessAnimateTorsoLeadsMotion(uintptr_t data, EACTrack* track, EMat4&, ERCharacter*,
                                                        EACNodeState* states) {
  auto* self = reinterpret_cast<AwarenessManager*>(data);
  if (!self || !track || !(self->m_flags & kTurning)) return;
  const float step = _dt * self->m_timeMult * self->m_pauseMult;
  EACNodeState torso = TurnState(self->GetFirstNodeRotation(step, 0.87266463f));
  EAnimController::Layer(1.0f, states[self->m_torsoBone], torso);
  EACNodeState head = TurnState(self->GetSecondNodeRotation(0.87266463f));
  EAnimController::Blend(1.0f, states[self->m_headBone], head);
  if (self->m_angle == self->m_targetAngle && (self->m_flags & kStopTurning)) self->m_flags &= ~static_cast<unsigned int>(kTurning);
}

// 8001287C
int AwarenessManager::SetAwarenessTargetAngle(cXObject* object) {
  m_targetAngle = 0.0f;
  if (!object) return 0;
  cXObjectImpl* target = object->GetObjectImplementation();
  if (!target || !ObjIsInWorld(target)) return 0;
  cXObjectImpl* me = m_person->AsObject()->GetObjectImplementation();
  if (!(me->CalcShortDistance(object) < 15.0)) return 0;
  const double delta = GetDeltaAngleToTargetObject(m_awareOf);
  if (!(std::fabs(delta) <= 1.919862151145935)) return 0;
  m_targetAngle = static_cast<float>(delta);
  return 1;
}

// 80012940
// While a passive influence animation plays (0x100): standing (states 0, 7..9) at full body unless
// the person holds something (object data 2: upper body; then only while idle state 0), walking,
// turning, the free-move and the awareness turns on the upper body; it stops in a skill (5, 6), the
// impatient idle (10) and the awareness gesture (0x10).
void AwarenessManager::handlePassiveInfluenceAnimation(unsigned int state) {
  if (!(m_flags & kPassiveAnimPlaying)) return;
  float* factors = SAnimator2::m_UpperBodyIndexArray;
  bool stop = false;
  if (state < 10) {
    if (state < 7 && 4 < state) {
      stop = true;
    } else if (state < 7 && state != 0) {
      // upper body
    } else {
      cXObjectImpl* obj = m_person->AsObject()->GetObjectImplementation();
      if (obj->GetData(2) == 0) factors = nullptr;
      else if (PersonOf(m_person)->GetIdleState() != 0) stop = true;
    }
  } else if (state < 0x10) {
    if (state < 0xc) {
      if (state != 10) return;
      stop = true;
    }
  } else {
    if (state == 0x10) stop = true;
    else if (state != 0x11) return;
  }
  if (stop) {
    StopPassiveInfluenceAnimation();
    return;
  }
  m_anim->SetTrackBlendFactors(eTrackFlags{0x200}, factors);
  m_anim->SetTrackBlendSmooth(eTrackFlags{0x200}, 1.0f, kAwarenessBlendTime, 0.01f);
}

// 80012A74
// The pending gesture starts (once loaded) in the states it can play in: standing (0, 7..9), walking
// and turning (1..4), the awareness gesture (0x10).
void AwarenessManager::handleMemoryAwarenessAnimation(unsigned int state) {
  if (state != 7) {
    if (state < 8) {
      if (4 < state) return;
      if (state == 0) return;
    } else {
      if (state < 0x10) return;
      if (state != 0x10) return;
    }
  }
  if (m_flags & kMemoryAnimPending) StartMemoryAwarenessAnimation(m_action, m_category);
}

// 80012ADC
// The head turn's procedural track (0x4000) is set when a turn starts (the head leads for the sprite
// reaction, else the torso) and stopped once a turn back is over.
void AwarenessManager::handleAwarenessTurningAnimation() {
  unsigned int f = m_flags;
  if (!(f & kTurning)) {
    if (!(f & kStopTurning)) return;
    m_anim->StopTrack(eTrackFlags{0x4000});
    f = m_flags & ~static_cast<unsigned int>(kStopTurning);
  } else {
    if (!(f & kStartTurning)) return;
    if (m_action == 1)
      m_anim->SetProceduralTrack(eTrackFlags{0x4000}, nullptr, AwarenessAnimateHeadLeadsMotion, reinterpret_cast<uintptr_t>(this));
    else
      m_anim->SetProceduralTrack(eTrackFlags{0x4000}, nullptr, AwarenessAnimateTorsoLeadsMotion, reinterpret_cast<uintptr_t>(this));
    f = m_flags & ~static_cast<unsigned int>(kStartTurning);
  }
  m_flags = f;
}

// 80012B8C
int AwarenessManager::SetAwareOfObject(cXObject* object) {
  if (m_awareOf || !object) return 0;
  m_awareOf = object;
  m_action = 1;
  m_flags = (m_flags & ~static_cast<unsigned int>(kAwareKilled)) | kAwareSet;
  if (Trace())
    std::printf("[aware] %s: made aware of #%d\n", PersonOf(m_person)->DebugName(), object->GetObjectImplementation()->m_id);
  return 1;
}

// 80012BD0
int AwarenessManager::SetAwareOfObjectKilled(cXObject* object) {
  if (!m_awareOf || m_awareOf != object) return 0;
  m_flags |= kAwareKilled;
  return 1;
}

// 80012C04
int AwarenessManager::ClearAwareOfObject(cXObject* object) {
  if (object != m_awareOf) return 0;
  if (m_flags & kAwareSet) {
    m_awareOf = nullptr;
    m_flags = (m_flags & ~static_cast<unsigned int>(kAwareSet)) | kStopTurning;
    m_targetAngle = 0.0f;
  }
  return 1;
}

// PC helper (inlined three times): the sim is doing something that rules awareness out: a skill
// (state 5), socialising, the second player's social, an interaction while not routing, or no
// usable main track (track rootFlags 0x200).
bool AwarenessManager::BusyForAwareness(unsigned int state) {
  cXPersonImpl* p = PersonOf(m_person);
  if (state == 5 || p->IsInSocialMode() == 1 || p->IsSecondPlayerInSocialMode() == 1) return true;
  if (p->GetCurrentInteractionStackObject() != 0 && m_animator->m_routing != 1) return true;
  if (m_anim->m_mainTrack < 0) return true;
  return (m_anim->m_tracks[static_cast<size_t>(m_anim->m_mainTrack)].m_rootFlags & 0x200) != 0;
}

// 80012C4C
// Every 5 s of game time, while aware of nothing and not busy.
int AwarenessManager::IsTimeToCheckMemoryAwareness(unsigned int state) {
  const float dt = _dt * m_timeMult * m_pauseMult;
  if (dt == 0.0f || m_awareOf) return 0;
  m_memoryTime += dt;
  if (m_memoryTime <= 5.0f) return 0;
  m_memoryTime = 0.0f;
  return BusyForAwareness(state) ? 0 : 1;
}

// 80012D9C
// A player's sim in my room, closer than 15 tiles and within 110 degrees of my facing (player 2's when
// nearer than player 1's).
int AwarenessManager::CanSeePlayer() {
  cXPersonImpl* me = PersonOf(m_person);
  cXObjectImpl* meObj = me;
  double nearest = 100000.0;
  int player = -1;
  if (cXPersonImpl* p = PersonOf(_globals.GetSelectedPerson(0))) {  // DAT_80475E88
    if (me->GetCurrentRoom() == p->GetCurrentRoom()) {
      nearest = meObj->CalcShortDistance(p);
      if (nearest < 15.0 && std::fabs(GetDeltaAngleToTargetObject(p)) <= 1.919862151145935) player = 0;
    }
  }
  if (_gd.m_fieldEC != 0) {  // DAT_80475C70: two players
    if (cXPersonImpl* p = PersonOf(_globals.GetSelectedPerson(1))) {
      if (me->GetCurrentRoom() == p->GetCurrentRoom()) {
        const double d = meObj->CalcShortDistance(p);
        if (d < 15.0 && d < nearest && std::fabs(GetDeltaAngleToTargetObject(p)) <= 1.919862151145935) player = 1;
      }
    }
  }
  return player;
}

// 80012F50
// My strongest memory of the person `object` (kind 6, else kind 2) and its level; -1 none.
void AwarenessManager::GetActiveMemoryCategory(cXObject* object, int& category, int& level) {
  level = -1;
  category = -1;
  cXPersonImpl* other = PersonOf(object);
  if (!other) return;
  cXPersonImpl* me = PersonOf(m_person);
  category = me->GetDominantMemory(other, 6);
  if (category == -1) {
    category = me->GetDominantMemory(other, 2);
    if (category == -1) return;
  }
  level = static_cast<int>(me->GetSimMemory(other, category));
}

// 8001303C
// The category's sprite (0x387..0x38E) over my head in a balloon, popping up, until cleared; unless
// the slot already shows something.
void AwarenessManager::StarMemoryAwarenessSprite() {
  cXObjectImpl* obj = m_person->AsObject()->GetObjectImplementation();
  auto* slot = obj ? reinterpret_cast<SpriteSlot*>(obj->GetSpriteSlot()) : nullptr;
  if (!slot || slot->m_ticks != 0) return;
  int sprite = 0;
  switch (m_category) {
    case 0: sprite = 0x387; break;
    case 1: sprite = 0x388; break;
    case 2: sprite = 0x389; break;
    case 3: sprite = 0x38a; break;
    case 4: sprite = 0x38b; break;
    case 5: sprite = 0x38c; break;
    case 6: sprite = 0x38d; break;
    case 7: sprite = 0x38e; break;
    default: break;
  }
  if (!sprite) return;
  slot->m_priority = 2;
  slot->Reset();
  slot->m_options = SpriteSlot::kPop;
  slot->m_popSize = 1.2f;
  slot->SetSprite(1, sprite, 1, false);
  // IconGroup::GetBalloonSpriteID(0): group 1's icon 0.
  IconGroupImpl balloons;
  balloons.Init(1);
  slot->m_balloon = balloons.GetSpriteID(0);
  slot->m_overlay = 0;
  slot->ActivateForTicks(-1);
  obj->ClearIdleStatus();
  if (Trace()) std::printf("[aware] %s: memory sprite %x (category %d)\n", PersonOf(m_person)->DebugName(), sprite, m_category);
}

// 800131C4
bool AwarenessManager::SetPendingMemoryAwarenessAnim(int action, int category) {
  const AnimRef* ref = nullptr;
  getAwarenessSkillID(action, category, ref);
  if (ref) {
    _animman.AddRefAsync(AnimRefId(ref));
    m_flags |= kMemoryAnimPending;
  }
  return ref != nullptr;
}

// 80013228
// The gesture once loaded (loading it first): action 3 on the main gesture track 0x80 (smooth,
// held), else over the upper body on track 0x400, faded in; the sprite shows with it.
void AwarenessManager::StartMemoryAwarenessAnimation(int action, int category) {
  const AnimRef* ref = nullptr;
  getAwarenessSkillID(action, category, ref);
  if (!ref) return;
  if (!_animman.GetRef(AnimRefId(ref))) {
    if (!(m_flags & kMemoryAnimPending)) {
      _animman.AddRefAsync(AnimRefId(ref));
      m_flags |= kMemoryAnimPending;
    }
    return;
  }
  if (m_flags & kMemoryAnimPending) _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
  StarMemoryAwarenessSprite();
  m_flags &= ~static_cast<unsigned int>(kMemoryAnimPending);
  if (action == 3) {
    if (EACTrack* t = m_anim->SetTrackAnim(eTrackFlags{0x80}, AnimRefId(ref), 2, kAwarenessBlendTime, nullptr)) {
      m_anim->SetTrackSpeed(t, m_timeMult);
      t->m_endMode = 1;
    }
  } else if (EACTrack* t = m_anim->SetTrackAnim(eTrackFlags{0x400}, AnimRefId(ref), -1, -1.0f, nullptr)) {
    m_anim->SetTrackSpeed(t, m_timeMult);
    t->m_endMode = 1;
    m_anim->SetTrackIntensity(t, 0.0001f, true);
    m_anim->SetTrackBlendSmooth(t, 1.0f, kAwarenessBlendTime, 0.01f);
    m_anim->SetTrackBlendFactors(t, SAnimator2::m_UpperBodyIndexArray);
  }
  if (Trace())
    std::printf("[aware] %s: memory gesture %08x (action %d, category %d)\n", PersonOf(m_person)->DebugName(),
                AnimRefId(ref), action, category);
}

// 800133B8
// The reaction for my memory of `object` (levels below 2 or no category: none): 2..3 the sprite;
// 4..5 a gesture (the sprite with a low-motive idle or walk); 6..7 turning to face the player with a
// gesture when walking a route or standing (idle state 0 or 3), else a gesture (the sprite when low
// on a motive); higher: none.
void AwarenessManager::SetMemoryAwarenessActionToTry(cXObject* object) {
  int level = 0;
  GetActiveMemoryCategory(object, m_category, level);
  if (level < 2 || m_category == -1) return;
  int action = 0;
  if (level < 6) {
    if (3 < level) action = (m_animator->shouldUseLowMotiveIdle() || m_animator->shouldUseLowMotiveWalk()) ? 1 : 2;
    else action = 1;
  } else if (level < 8) {
    cXPersonImpl* p = PersonOf(m_person);
    if (m_animator->shouldUseLowMotiveIdle() || m_animator->shouldUseLowMotiveWalk()) action = 1;
    else if (m_animator->m_routing == 1 || p->GetIdleState() == 0 || p->GetIdleState() == 3) action = 3;
    else action = 2;
  }
  m_action = action;
}

// 800134F8
// The reaction ends: a gesture still loading is given back, an awareness follow ends, the upper-body
// gesture fades, the head turns back; the other sim is told (it forgets me) unless made aware by a
// tree or influence; the sprite fades out.
void AwarenessManager::MemoryAwarenessClearAction() {
  if (m_flags & kMemoryAnimPending) {
    const AnimRef* ref = nullptr;
    getAwarenessSkillID(m_action, m_category, ref);
    if (ref) {
      if (!_animman.GetRef(AnimRefId(ref))) _animman.DelRefAsync(AnimRefId(ref));
      else _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
    }
    m_flags &= ~static_cast<unsigned int>(kMemoryAnimPending);
  }
  SAnimator2* a = m_animator;
  // PC fix: the GC's test read `a` before checking it (null for a person whose animator is not made
  // yet: a load killing the missing object a sleeping sim's bed was, mods.md)
  if (a && (a->m_followMode == 6 || a->m_followMode == 7)) a->setAwarenessFollowEnd();
  if (m_anim && !m_anim->m_tracks.empty() && m_anim->IsTrackValid(eTrackFlags{0x400}))
    m_anim->SetTrackBlendSmooth(eTrackFlags{0x400}, 0.0f, kAwarenessBlendTime, 0.01f);
  if (m_flags & kTurning) {
    m_flags |= kStopTurning;
    m_targetAngle = 0.0f;
  }
  cXObjectImpl* meObj = m_person->AsObject()->GetObjectImplementation();
  const bool killed = (meObj->GetMiscFlag(cXObject__MiscFlag{0x40}) || meObj->GetMiscFlag(cXObject__MiscFlag{0x800}));
  if (killed && !(m_flags & (kPassiveAware | kAwareKilled))) {
    if (!m_awareOf) goto done;
    if (cXPersonImpl* other = PersonOf(m_awareOf)) other->SetAwareOfObjectKilled(m_person->AsObject());
  }
  if (m_awareOf && !(m_flags & (kAwareKilled | kPassiveAware | kAwareSet))) {
    if (cXPersonImpl* other = PersonOf(m_awareOf)) other->ClearAwareOfObject(m_person->AsObject());
  }
done:
  m_flags &= ~static_cast<unsigned int>(kAwareSet | kPassiveAware);
  m_category = -1;
  m_action = 0;
  m_targetAngle = 0.0f;
  m_awareOf = nullptr;
  if (auto* slot = reinterpret_cast<SpriteSlot*>(meObj->GetSpriteSlot()))
    if (slot->m_ticks != 0) slot->m_options |= SpriteSlot::kFade | SpriteSlot::kEndWithFade;
}

// 800137C0
// While aware of something: unless being destroyed or busy, the head keeps turning toward it while it
// is near and in view and no gesture has finished; otherwise the reaction ends.
void AwarenessManager::UpdateAwarenessAngle(unsigned int state) {
  if (!m_awareOf) {
    m_targetAngle = 0.0f;
    return;
  }
  // PC: an object killed without telling me (GC reads the freed object): forgotten as if killed.
  if (!(m_flags & kAwareKilled) && Globs_pObjectModule && !Globs_pObjectModule->IsValidObject(m_awareOf)) {
    m_flags |= kAwareKilled;
    m_awareOf = nullptr;
    MemoryAwarenessClearAction();
    StopPassiveInfluenceAnimation();
    return;
  }
  if (!(m_flags & kAwareKilled) && !BusyForAwareness(state)) {
    if (!IsMemoryAwarenessAnimDone() && SetAwarenessTargetAngle(m_awareOf)) {
      if (m_flags & kTurning) return;
      m_flags |= kTurning | kStartTurning;
      return;
    }
  }
  MemoryAwarenessClearAction();
  StopPassiveInfluenceAnimation();
}

// 80013960
// When it is time, a player's sim in view: I become aware of it, pick my reaction and (if it is in
// range) the player's sim becomes aware of me; then the sprite, the gesture, or the awareness follow
// (turning to face it) with the gesture loading.
void AwarenessManager::MemoryAwarenessCheck(unsigned int state) {
  if (!IsTimeToCheckMemoryAwareness(state)) return;
  const int player = CanSeePlayer();
  if (player == -1) return;
  cXObject* seen = GetPlayerObject(player);
  m_awareOf = seen;
  m_flags &= ~static_cast<unsigned int>(kAwareKilled);
  SetMemoryAwarenessActionToTry(seen);
  if (m_action != 0) {
    if (!SetAwarenessTargetAngle(m_awareOf)) {
      MemoryAwarenessClearAction();
      return;
    }
    if (cXPersonImpl* p = PersonOf(_globals.GetSelectedPerson(player))) p->SetAwareOfObject(m_person->AsObject());
    if (Trace())
      std::printf("[aware] %s: sees player %d's sim, action %d category %d angle %.2f\n", PersonOf(m_person)->DebugName(),
                  player, m_action, m_category, m_targetAngle);
    switch (m_action) {
      case 2: StartMemoryAwarenessAnimation(2, m_category); return;
      case 1: StarMemoryAwarenessSprite(); return;
      case 3:
        if (SetPendingMemoryAwarenessAnim(3, m_category)) {
          m_animator->setAwarenessFollowStart(m_targetAngle);
          return;
        }
        break;
      default: return;
    }
  }
  MemoryAwarenessClearAction();
}

// 80013AAC
// The passive influence animation: a load in flight given back, the playing one faded out; an
// awareness of the influence's object ends (the head turns back).
void AwarenessManager::StopPassiveInfluenceAnimation() {
  if (m_flags & kPassiveAnimLoading) {
    const AnimRef* ref = m_passiveAnim;
    m_flags &= ~static_cast<unsigned int>(kPassiveAnimLoading);
    if (ref) {
      if (!_animman.GetRef(AnimRefId(ref))) _animman.DelRefAsync(AnimRefId(ref));
      else _animman.DelRef(AnimRefId(ref), EResourceManager__DelRefMode{1});
    }
  }
  if ((m_flags & kPassiveAnimPlaying) && m_anim && !m_anim->m_tracks.empty() && m_anim->IsTrackValid(eTrackFlags{0x200}))
    m_anim->SetTrackBlendSmooth(eTrackFlags{0x200}, 0.0f, kAwarenessBlendTime, 0.01f);
  const unsigned int f = m_flags;
  m_passiveAnim = nullptr;
  m_flags = f & ~static_cast<unsigned int>(kPassiveAnimPlaying);
  if (f & kPassiveAware) {
    m_flags = (f & ~static_cast<unsigned int>(kPassiveAnimPlaying | kPassiveAware)) | kStopTurning;
    m_awareOf = nullptr;
    m_targetAngle = 0.0f;
  }
}

// 80013BA4
// The influence's animation on track 0x200 (upper body, faded in) once loaded; 1 when it started.
int AwarenessManager::StartPassiveInfluenceAnimation(const AnimRef* anim) {
  if ((m_flags & kPassiveAnimPlaying) || m_anim->IsTrackValid(eTrackFlags{0x200}) || !anim) return 0;
  unsigned int f;
  int started = 0;
  if (!_animman.GetRef(AnimRefId(anim))) {
    if (m_flags & kPassiveAnimLoading) return 0;
    _animman.AddRefAsync(AnimRefId(anim));
    f = m_flags | kPassiveAnimLoading;
  } else {
    EACTrack* t = m_anim->SetTrackAnim(eTrackFlags{0x200}, AnimRefId(anim), -1, -1.0f, nullptr);
    if (m_flags & kPassiveAnimLoading) _animman.DelRef(AnimRefId(anim), EResourceManager__DelRefMode{1});
    if (t) {
      t->m_endMode = 0;
      m_anim->SetTrackIntensity(t, 0.0001f, true);
      m_anim->SetTrackBlendSmooth(t, 1.0f, kAwarenessBlendTime, 0.01f);
      m_anim->SetTrackBlendFactors(t, SAnimator2::m_UpperBodyIndexArray);
    }
    started = 1;
    f = (m_flags & ~static_cast<unsigned int>(kPassiveAnimLoading)) | kPassiveAnimPlaying;
    m_passiveTime = 0.0f;
    if (Trace())
      std::printf("[aware] %s: passive influence animation %08x\n", PersonOf(m_person)->DebugName(), AnimRefId(anim));
  }
  m_flags = f;
  return started;
}

// 80013CEC
void AwarenessManager::RainAwarenessCheck() {
  if (!shouldAutoCarry()) {
    if (m_flags & kAutoCarry) endAutoCarry();
  } else if (!(m_flags & kAutoCarry)) {
    startAutoCarry();
  }
}

// 80013D4C
// The influences on my tile move my motives (PassiveInfluenceMap::NotifySimulator). Unless in the
// awareness gesture (0x10): the strongest one's reaction (its own animation, else by its kind: 2
// 0x141, 3 0x142, 4 0x140 which waits 3 s between plays) plays for kinds 2.. unless carrying a
// direct-control object or low on a motive, and its object becomes what I am aware of.
void AwarenessManager::PassiveInfluenceAwarenessCheck(unsigned int state) {
  PassiveInfluenceMap* map = Globs_pFixedWorld ? Globs_pFixedWorld->GetPassiveInfluenceMap() : nullptr;
  PassiveInfluenceItem* item = nullptr;
  if (!map) {
    StopPassiveInfluenceAnimation();
  } else {
    cXObjectImpl* obj = m_person->AsObject()->GetObjectImplementation();
    const int y = obj->m_location[0] >> 4, x = obj->m_location[1] >> 4;
    // GC: the fixed world's bounds checks of x and y (vtable 0x30 / 0x38), results unused.
    item = PassiveInfluenceMap::NotifySimulator(m_person, x, y);
    if (state == 0x10) return;
    if (!item) {
      StopPassiveInfluenceAnimation();
    } else {
      auto* influence = static_cast<PassiveInfluencePerson*>(item);  // NotifySimulator returns person-influences
      const AnimRef* anim = influence->m_skill;
      const signed char kind = influence->m_skillKind;
      if (!anim) {
        getPassiveInfluenceSkillID(kind, anim);
        if (kind == 4 && anim) {
          if (AnimRefId(anim) == m_anim->GetTrackAnimId(eTrackFlags{0x200})) {
            if (m_anim->IsTrackAnimComplete(eTrackFlags{0x200})) {
              StopPassiveInfluenceAnimation();
              return;
            }
          } else {
            m_passiveTime += _dt;
            if (m_passiveTime < 3.0f) return;
          }
        }
      }
      if (kind < 2 || PersonOf(m_person)->IsCarryingDCObject() || m_animator->shouldUseLowMotiveIdle()) {
        StopPassiveInfluenceAnimation();
      } else if (!(m_flags & kPassiveAnimLoading)) {
        m_passiveAnim = anim;
        StartPassiveInfluenceAnimation(anim);
      } else if (anim && m_passiveAnim != anim) {
        StopPassiveInfluenceAnimation();
      } else {
        StartPassiveInfluenceAnimation(m_passiveAnim);  // GC passes its register's stale value: the pending one
      }
      if (!m_awareOf || m_awareOf != item->GetObject()) {
        m_awareOf = item->GetObject();
        m_action = 1;
        m_flags = (m_flags & ~static_cast<unsigned int>(kAwareKilled)) | kPassiveAware;
      }
    }
  }
  if (m_awareOf && (m_flags & kPassiveAware)) {
    if (!Globs_pObjectModule || !Globs_pObjectModule->IsValidObject(m_awareOf)) StopPassiveInfluenceAnimation();
  }
}

// 80014028
cXObject* AwarenessManager::GetPlayerObject(int player) {
  cXPerson* p = _globals.GetSelectedPerson(player);  // DAT_80475E88[player]
  return p ? p->AsObject() : nullptr;
}

// 80014050
// The turn from my facing to `object` (its tile from my position), or, for something I hold, the
// direction of my hand.
double AwarenessManager::GetDeltaAngleToTargetObject(cXObject* object) {
  const EVec2& pos = m_animator->GetPos();
  const float px = pos.x, py = pos.y;
  cXObjectImpl* target = object ? object->GetObjectImplementation() : nullptr;
  double delta = 0.0;
  if (!target) return 0.0;
  cXObjectImpl* me = m_person->AsObject()->GetObjectImplementation();
  cXObject* holder = target->GetContainer();  // IsContained / GetContainerID
  if (holder && holder->GetObjectImplementation() && holder->GetObjectImplementation()->m_id == me->m_id) {
    EVec3 hand;
    m_animator->GetBonePos(0, hand);
    delta = simmy::dm::atan2(-hand.x, hand.y);
  } else {
    const float ty = static_cast<float>(target->m_location[0]), tx = static_cast<float>(target->m_location[1]);
    const float a = simmy::dm::atan2(-(ty - py), tx - px);
    delta = m_animator->getDeltaDirFromDesiredDir(a);
  }
  if (std::fabs(delta) < 9.999999747378752e-05) delta = 0.0;
  return delta;
}

// 80014248
int AwarenessManager::GetAwarenessAction() { return m_action; }
// 80014250
cXObject* AwarenessManager::GetAwareOfObject() { return m_awareOf; }
// 80014258
int AwarenessManager::IsMemoryAwarenessAnimPending() { return (m_flags >> 4) & 1; }
// 80014264
int AwarenessManager::IsAutoCarryActive() { return (m_flags >> 9) & 1; }
// 80014270
void AwarenessManager::SetTimeMultiplier(float mult) { m_timeMult = mult; }
// 80014278
void AwarenessManager::SetPauseMultiplier(float mult) { m_pauseMult = mult; }
// 80014280
unsigned int AwarenessManager::GetStateFlags() { return m_flags; }

// 80014288
int AwarenessManager::ShouldAwarenessBeActive(unsigned int state) { return BusyForAwareness(state) ? 0 : 1; }

// 80014378
// The gesture (action 2 or 3) has played out.
int AwarenessManager::IsMemoryAwarenessAnimDone() {
  const int track = m_action == 3 ? 0x80 : 0x400;
  return (1 < m_action && !(m_flags & kMemoryAnimPending) && m_anim->IsTrackAnimComplete(eTrackFlags{track})) ? 1 : 0;
}

// 800143E0
void AwarenessManager::getPassiveInfluenceSkillID(signed char kind, const AnimRef*& out) {
  int index = 0;
  if (kind == 2) index = 0x141;
  else if (kind < 3) {
    if (-1 < kind) {
      out = nullptr;
      return;
    }
  } else if (kind == 3) index = 0x142;
  else if (kind == 4) index = 0x140;
  GetMiscAnimRef(m_person, index, out);
}

// 80014450
// The memory gesture of a category (actions 2 and 3 alike): 0 0x135, 1 0x137, 2 0x138, 3 0x13A, 4
// 0x134, 5 0x139, 6 0x133, 7 0x136.
void AwarenessManager::getAwarenessSkillID(int action, int category, const AnimRef*& out) {
  out = nullptr;
  if (action != 2 && action != 3) return;
  int index = 0;
  switch (category) {
    case 0: index = 0x135; break;
    case 1: index = 0x137; break;
    case 2: index = 0x138; break;
    case 3: index = 0x13a; break;
    case 4: index = 0x134; break;
    case 5: index = 0x139; break;
    case 6: index = 0x133; break;
    case 7: index = 0x136; break;
    default: break;  // GC: misc animation 0
  }
  GetMiscAnimRef(m_person, index, out);
}

// 800145B4
// In the rain, outside (room 0), not walking through a door (portal mode 1), no skill playing, an
// adult not a ghost, not socialising, and not busy unless routing.
int AwarenessManager::shouldAutoCarry() {
  if (!m_bIsRaining) return 0;
  cXPersonImpl* p = PersonOf(m_person);
  if (p->GetCurrentRoom() != 0 || m_animator->m_portalMode == 1) return 0;
  if (MainFlags(m_anim) & 0x10) return 0;
  if (m_sim->IsAdult() != 1 || p->IsGhost() || p->IsInSocialMode() || p->IsSecondPlayerInSocialMode()) return 0;
  if (p->GetCurrentInteractionStackObject() != 0 && !p->IsRouting()) return 0;
  return 1;
}

// 8001471C
// The prop already worn (the newspaper 0x8919F4C7, std anim 0xF; else the umbrella 0x1F5AFFFD, std
// anim 0xB), else one of them at random, put on; its carry pose on the left arm (track 0x1000).
void AwarenessManager::startAutoCarry() {
  const unsigned int kNewspaper = 0x8919f4c7u, kUmbrella = 0x1f5afffdu;
  int anim, worn;
  unsigned int prop;
  worn = m_animator->getIndexOfPropID(kNewspaper);
  if (worn != -1) {
    anim = 0xf, prop = kNewspaper;
  } else {
    worn = m_animator->getIndexOfPropID(kUmbrella);
    if (worn != -1) {
      anim = 0xb, prop = kUmbrella;
    } else if (simmy::sim_rand() & 1) {  // GC: rand()
      worn = m_animator->getIndexOfPropID(kNewspaper);
      anim = 0xf, prop = kNewspaper;
    } else {
      worn = m_animator->getIndexOfPropID(kUmbrella);
      anim = 0xb, prop = kUmbrella;
    }
  }
  if (worn < 0) m_animator->AddProp(prop, true);
  const AnimRef* ref = nullptr;
  GetStdAnimRef(m_person, anim, ref);
  if (ref) {
    if (EACTrack* t = m_anim->SetTrackAnim(eTrackFlags{0x1000}, AnimRefId(ref), -1, -1.0f, nullptr)) {
      m_anim->SetTrackIntensity(t, 0.0001f, true);
      m_anim->SetTrackSpeed(t, m_timeMult);
      m_anim->SetTrackBlendSmooth(t, 1.0f, kCarryBlendTime, 0.01f);
      m_animator->lockCarryArmNodes(SAnimator2::m_LeftArmCarryIndexArray, t);
    }
  }
  m_flags |= kAutoCarry;
  if (anim == 0xb) m_flags |= kAutoCarryUmbrella;
  if (Trace()) std::printf("[aware] %s: auto-carry %s in the rain\n", PersonOf(m_person)->DebugName(), anim == 0xb ? "umbrella" : "newspaper");
}

// 80014A3C
void AwarenessManager::endAutoCarry() {
  const int worn = m_animator->getIndexOfPropID((m_flags & kAutoCarryUmbrella) ? 0x1f5afffdu : 0x8919f4c7u);
  if (worn >= 0) m_animator->RemoveProp(static_cast<unsigned int>(worn));
  if (m_anim->IsTrackValid(eTrackFlags{0x1000})) m_anim->SetTrackBlendSmooth(eTrackFlags{0x1000}, 0.0f, kCarryBlendTime, 0.01f);
  m_flags &= ~static_cast<unsigned int>(kAutoCarry | kAutoCarryUmbrella);
}
