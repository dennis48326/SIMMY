// @class AwarenessManager after EQuat EVec3
// @class AwarenessManager__eAwarenessAction
// A person's awareness (game/awarenessmanager.cpp; SAnimator2 +0x620): what the sim notices around it
// and how it shows it.
// - Memory awareness: every 5 s of game time a sim not under player control, standing or walking
//   and not busy, looks for a player's sim in its room within 15 tiles and 110 degrees of where it
//   faces (CanSeePlayer). Its strongest memory of that sim (GetDominantMemory, kind 6 then 2, and its
//   level) picks the reaction (SetMemoryAwarenessActionToTry): 1 a memory sprite over its head
//   (StarMemoryAwarenessSprite), 2 an upper-body gesture (track 0x400), 3 turning to face the player
//   and a full-body gesture (track 0x80, the animator's awareness follow, modes 6..8); either way the
//   head turns toward the sim (procedural track 0x4000: head first up to 70 degrees, the torso
//   taking the rest up to 50; or the torso first) and the player's sim becomes aware of it too.
// - Passive influence: standing in an influence's area (stereo, trash, ...), the influence's
//   reaction animation (or the misc animations 0x140..0x142 by its kind) plays over the upper body
//   on track 0x200 and the head turns toward the object (PassiveInfluenceAwarenessCheck).
// - Rain: an adult outside in the rain, not carrying or socialising, holds a newspaper or umbrella
//   prop over its head (auto-carry, track 0x1000 on the left arm).
// - Auto-run: walking more than 5 tiles in the rain (4 under player control) runs.
#pragma once

struct AwarenessManager__eAwarenessAction {
  SIMMY_ENUM_BODY(AwarenessManager__eAwarenessAction)
};

struct AwarenessManager {
  using eAwarenessAction = ::AwarenessManager__eAwarenessAction;
  // m_flags (+0x00).
  enum : unsigned int {
    kPlayerControl = 1,      // the player steers the sim (no memory awareness)
    kTurning = 2,            // the head is turned (the procedural track runs)
    kStartTurning = 4,       // the procedural track is to be set
    kStopTurning = 8,        // turning back to straight ahead, then the track stops
    kMemoryAnimPending = 0x10,  // the gesture is loading
    kAwareSet = 0x20,        // made aware by SetAwareOfObject (a tree or another sim)
    kPassiveAnimLoading = 0x40,
    kPassiveAware = 0x80,    // aware of a passive influence's object
    kPassiveAnimPlaying = 0x100,
    kAutoCarry = 0x200,
    kAutoCarryUmbrella = 0x400,  // the prop is the umbrella (0x1F5AFFFD), else the newspaper
    kAwareKilled = 0x800,    // the object is being destroyed
  };

  AwarenessManager();  // 80011EEC
  void Init(SAnimator2* animator, EAnimController* anim);  // 80011F4C
  void Reset();  // 80012034
  ~AwarenessManager();  // 800120EC
  void AwarenessCheck(unsigned int animState);  // 80012140
  void handleAwarenessAnimations(unsigned int animState);  // 8001220C
  static void SetIsRaining(bool raining);  // 80012250
  // Walking `dist` tiles: run (player control: more than 4; else in the rain: more than 5).
  int AwarenessAutoRunCheck(float dist);  // 80012258
  void SetPlayerControl(bool control);  // 800122B0
  // The head's (first node's) turn about z this frame: toward the target by up to `step` radians
  // (times pi), clamped to +-limit.
  EQuat GetFirstNodeRotation(float step, float limit);  // 800122D8
  // What is left beyond `limit` for the second node.
  EQuat GetSecondNodeRotation(float limit);  // 800123D4
  static void AwarenessAnimateHeadLeadsMotion(uintptr_t data, EACTrack* track, EMat4& root, ERCharacter* character,
                                              EACNodeState* states);  // 80012494
  static void AwarenessAnimateTorsoLeadsMotion(uintptr_t data, EACTrack* track, EMat4& root, ERCharacter* character,
                                               EACNodeState* states);  // 80012688
  // `object` close enough (15 tiles) and in view (110 degrees): its angle becomes the target. 1: it is.
  int SetAwarenessTargetAngle(cXObject* object);  // 8001287C
  void handlePassiveInfluenceAnimation(unsigned int animState);  // 80012940
  void handleMemoryAwarenessAnimation(unsigned int animState);  // 80012A74
  void handleAwarenessTurningAnimation();  // 80012ADC
  int SetAwareOfObject(cXObject* object);  // 80012B8C
  int SetAwareOfObjectKilled(cXObject* object);  // 80012BD0
  int ClearAwareOfObject(cXObject* object);  // 80012C04
  int IsTimeToCheckMemoryAwareness(unsigned int animState);  // 80012C4C
  // The player (0, 1) whose sim this one sees, -1 none.
  int CanSeePlayer();  // 80012D9C
  void GetActiveMemoryCategory(cXObject* object, int& category, int& level);  // 80012F50
  void StarMemoryAwarenessSprite();  // 8001303C
  bool SetPendingMemoryAwarenessAnim(int action, int category);  // 800131C4
  void StartMemoryAwarenessAnimation(int action, int category);  // 80013228
  void SetMemoryAwarenessActionToTry(cXObject* object);  // 800133B8
  void MemoryAwarenessClearAction();  // 800134F8
  void UpdateAwarenessAngle(unsigned int animState);  // 800137C0
  void MemoryAwarenessCheck(unsigned int animState);  // 80013960
  void StopPassiveInfluenceAnimation();  // 80013AAC
  int StartPassiveInfluenceAnimation(const AnimRef* anim);  // 80013BA4
  void RainAwarenessCheck();  // 80013CEC
  void PassiveInfluenceAwarenessCheck(unsigned int animState);  // 80013D4C
  cXObject* GetPlayerObject(int player);  // 80014028
  double GetDeltaAngleToTargetObject(cXObject* object);  // 80014050
  int GetAwarenessAction();  // 80014248
  cXObject* GetAwareOfObject();  // 80014250
  int IsMemoryAwarenessAnimPending();  // 80014258
  int IsAutoCarryActive();  // 80014264
  void SetTimeMultiplier(float mult);  // 80014270
  void SetPauseMultiplier(float mult);  // 80014278
  unsigned int GetStateFlags();  // 80014280
  int ShouldAwarenessBeActive(unsigned int animState);  // 80014288
  int IsMemoryAwarenessAnimDone();  // 80014378
  void getPassiveInfluenceSkillID(signed char kind, const AnimRef*& out);  // 800143E0
  void getAwarenessSkillID(int action, int category, const AnimRef*& out);  // 80014450
  int shouldAutoCarry();  // 800145B4
  void startAutoCarry();  // 8001471C
  void endAutoCarry();  // 80014A3C

  // PC: the person busy as the memory checks count it (IsTimeToCheckMemoryAwareness,
  // UpdateAwarenessAngle and ShouldAwarenessBeActive inline the same test).
  bool BusyForAwareness(unsigned int animState);

  static int m_bIsRaining;  // 804FD968

  unsigned int m_flags = 0;          // +0x00
  SAnimator2* m_animator = nullptr;  // +0x04
  EAnimController* m_anim = nullptr; // +0x08 the sim's controller
  float m_angle = 0.0f;              // +0x0C the head's turn now
  float m_memoryTime = 0.0f;         // +0x10 seconds since the last memory check (starts at 0..5)
  int m_category = -1;               // +0x14 the memory category (0..7) shown
  int m_action = 0;                  // +0x18 eAwarenessAction: 0 none, 1 sprite, 2 gesture, 3 face and gesture
  cXObject* m_awareOf = nullptr;     // +0x1C
  float m_targetAngle = 0.0f;        // +0x20
  int m_headBone = 0;                // +0x24 (the animator's head bone)
  int m_torsoBone = 0;               // +0x28
  float m_timeMult = 1.0f;           // +0x2C the animator's (SAnimator2 +0xFC)
  float m_pauseMult = 1.0f;          // +0x30
  cXPerson* m_person = nullptr;      // +0x34
  ESim* m_sim = nullptr;             // +0x38
  float m_passiveTime = 0.0f;        // +0x3C how long a kind-4 influence's animation waited
  const AnimRef* m_passiveAnim = nullptr;  // +0x40 the passive influence animation loading
};
