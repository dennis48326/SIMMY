// @class SAnimator2 after EVec2 EVec3 EMat4 AnimRef EAnimNote StackElem
// A person's animator (game/sanimator2.cpp): it owns where the sim stands and faces (in the
// animation format: 1/16 tiles, x and y as the tiles have them; the world's frame swaps them) and
// which animations the ESim's controller plays. Each frame Update picks the animation state
// (updateRenderAnimation) and plays it with cross-fades: standing idles with fidget overlays, the
// free-move walk and run cycles of direct control (the stick sets the desired state and direction:
// SetDesiredAnimState / +0xB4), turning on the spot (shuffles) and the run-stop; then poses the sim
// (positionCharacter). The simulator side: skills from the animate primitive (TryAnimate) with their
// animation events, carrying (the carry track 0x800 on the arm nodes, the hand's position for the
// held object), and walking a route (BeginRoute / moveAnimation, PC follow modes). Not ported:
// awareness, props, costumes. Bone particles: src/game/boneparticle.cpp.
#pragma once
#include <map>
#include <vector>

struct ESim;
struct SAnimator2 {
  SAnimator2();  // 8005BFD0
  virtual ~SAnimator2();  // 8005C23C
  virtual int Initialize(cXPerson* person);  // 8005C43C
  static void InitStaticAnimationElements();  // 8005C610
  void SetIsRaining(bool);  // 8005C76C
  virtual void Render(int);  // 8005C78C
  // The outfit in the person's data 8 is on (1), or still being changed (2: the ESim's change asked for).
  virtual int TryChangeSuit();  // 8005C790
  void checkParticleCleanup();  // 8005C87C
  void updateFreeMoveState();  // 8005C8E8
  void updateMovementState();  // 8005CC2C
  virtual void Update();  // 8005CC80
  void adjustAnimationPlayRates();  // 8005D014
  virtual void Reset();  // 8005D04C
  virtual void ResetSuits();  // 8005D120
  virtual void SnapToGrid();  // 8005D124
  virtual void ForceLocation();  // 8005D130
  // The idle-animate primitive's animation (IdleAnimateParam: +0 the animation, flag 2 the param it
  // names of the stack object's, +4 its table as for TryAnimate).
  int resolveIdleSkillForPrimitive(StackElem* frame, const unsigned char* params, const AnimRef*& out);  // 8005D164
  // The animation an animate primitive names (`params`: its AnimateNewParam operands). 1: found, -1: an
  // error (the tree is told).
  int resolveSkillForPrimitive(StackElem* frame, const unsigned char* params, const AnimRef*& out);  // 8005D394
  // 1: `ref`'s animation is in; 2: its load started (the primitive waits).
  int loadSkillAnim(const AnimRef* ref);  // 8005D5FC
  RET_UNKNOWN IsSkillIdleAnimRunning();  // 8005D6A0
  // Primitive 0x40's animation (cXPersonImpl::TryIdleAnimate): 1 playing, 2 loading, -1 error.
  virtual int TryIdleAnimate(StackElem* frame, const unsigned char* params);  // 8005D6D4
  // The idle for the person's idle state (sitting 1, 2; standing or lying a random one), loaded: 1.
  int SelectAlgorithmicIdle(const AnimRef*& out);  // 8005DA24
  // The animate-sim primitive (0x2C): 1 done, 0 an event or a loop (its number in param 0 or a local),
  // 2 still playing, -1 an error.
  virtual int TryAnimate(StackElem* frame, const unsigned char* params);  // 8005DBA0
  virtual void SetAnimDisplacements(float, float, float);  // 8005E7C8
  virtual void BeginFollow(float);  // 8005E7CC
  // PC (GC: BeginFollow over the person's destination list): walk `points` (animation format, the
  // person's spot first), then turn to `endDir` (kNoDir: as it arrives). m_routeResult says how it ended.
  void BeginRoute(const std::vector<EVec2>& points, float endDir);
  // PC: the walk stops where it is (m_routeResult -1).
  void AbortRoute();
  // Each simulator tick of a route: 2 still walking, 1 arrived (the person placed at the nearest point
  // of its path where it fits), 0 not following or it did not fit (put back where it stands).
  virtual int FollowOneStep();  // 8005EAB8
  virtual bool EndFollow();  // 8005EC74
  virtual int IsFollowing();  // 8005ED9C
  virtual RET_UNKNOWN IsInterruptable();  // 8005EDB4
  // The oldest animation event (an 'xevt' note's number) not yet handed to the tree. 1: there was one.
  virtual int DequeueAnimEvent(int* event);  // 8005EDBC
  // Version 0x40 on: the models it preloads (+0x168: {resource, flag}), the skill animation playing
  // (whether, its event fraction and frame, its id, its loops: read, it is kept to resume), from 0x61 the
  // person's own clothes while in a costume (+0x604). Older saves hold a model state now dropped.
  virtual void ReconStream(ReconBuffer* buffer, int version);  // 8005EE34
  virtual void ResetCensorship();  // 8005F4E0
  virtual void SetPixelated(int);  // 8005F4E4
  unsigned int getCorrectId(const PropRef* prop);  // 8005F4E8
  virtual void Dress(const PropRef* prop);  // 8005F548
  virtual void AddProp(unsigned int, bool);  // 8005F5D0
  virtual void RemoveProp(unsigned int);  // 8005F758
  virtual int PreloadDress(const PropRef* prop);  // 8005F7E0
  virtual void Undress(const PropRef* prop);  // 8005F82C
  int getPersonX();  // 8005F8D8
  int getPersonY();  // 8005F918
  RET_UNKNOWN endFollowDone();  // 8005F958
  RET_UNKNOWN followStandToTurnUpdate(float &, float &, float);  // 8005FB50
  RET_UNKNOWN followSidestepUpdate(float &, float &, float);  // 8005FBD0
  RET_UNKNOWN followMiddleUpdate(float &, float &, float);  // 8005FDBC
  void followMoveToTurnUpdate(float &, float &, float);  // 8005FEB8
  RET_UNKNOWN followDoneUpdate(float &, float &, float);  // 8005FF1C
  void endMoveAnimation();  // 8005FF80
  void UpdatePortalMode();  // 800600E0
  void moveAnimation();  // 8006028C
  void awarenessMove();  // 8006067C
  float GetTurnRate();  // 800608DC
  int rotateAnimation(float dt, float desired, float delta);  // 80060928
  void sidestepAlongNode(float &);  // 80060A84
  void advanceAlongNode(float &);  // 80060C20
  void getUseSpeed(float, float, float, float);  // 80060CE0
  RET_UNKNOWN EnableWalkFade(unsigned int &, float, float, float);  // 80060E30
  RET_UNKNOWN moveTowardsDestination(float &, EVec2 &);  // 80060EBC
  void updateRenderAnimation();  // 80061158
  void updateParticles();  // 80061658
  void handleMoodAnimations();  // 80061804
  void playFootprint(char *);  // 800619BC
  RET_UNKNOWN getFootSound(char *);  // 80061B3C
  void handleSidestepAnimation();  // 80061C1C
  void handleWalkRunAnimation();  // 800620AC
  void CheckCollision(EVec2& from, EVec2& to);  // 800625A0
  void handleFreeMoveWalkRunAnimation();  // 800627C0
  void handleRunStopAnimation();  // 800630DC
  void SetNextStateFromCompletedTurn();  // 80063234
  int selectRandomIdle();  // 800632EC
  void handleSkillIdleAnimation();  // 800635DC
  void handleIdleAnimation();  // 80063A1C
  void handleImpatientIdleAnimation();  // 80064434
  void clearImpatientIdleAnimation();  // 80064538
  void LoadSMOptionalMotionAnims();  // 800645C8
  void LoadDCOptionalMotionAnims();  // 8006468C
  RET_UNKNOWN LoadOptionalIdleAnim();  // 80064750
  void UnloadPendingLongIdleAnim();  // 80064858
  void UnloadOptionalIdleAnim();  // 8006491C
  void UnloadDCOptionalMotionAnims();  // 80064A40
  void UnloadSMOptionalMotionAnims();  // 80064B2C
  void CheckOptionalMotionAnimStatusOnStateChange(int state);  // 80064C18
  void handleTurnAnimation();  // 80064C84
  void positionCharacter(EMat4* orient);  // 80065094
  virtual void GetBonePosAndDirForParticle(unsigned int, EMat4 &);  // 800653F8
  virtual void AttachParticleEffect(unsigned int, unsigned int, int);  // 80065420
  virtual void DetachParticleEffect(unsigned int, unsigned int, int);  // 800654F0
  virtual RET_UNKNOWN StartParticleEffectFromEdithPrimitive(unsigned int, unsigned int);  // 8006559C
  void convertAnimationFormatToEngineFormat(EVec3& anim, EVec3& engine);  // 800655A4
  EVec3 GetEngineFormatPos();  // 800655D4
  // The notes of `ref`'s animation from `from` ms on for `length` ms (-1: to the end; backwards: counted
  // from the end), each to eventHandler.
  void processEvents(const AnimRef* ref, int from, int length, bool backwards, bool overlay);  // 80065668
  void eventHandler(EAnimNote& note, int index);  // 80065818
  void PreloadEvents(AnimRef *);  // 80065B68
  void PreloadBoneParticleEvent(REffectsAttachment *);  // 80065C70
  void _handleParticleEvent(REffectsAttachment *);  // 80065CE4
  void procBoneParticleEvt(AnimParticleData *, bool);  // 80065DE8
  // The bone particles (boneparticle.hpp): bone -> effect id -> particle (GC TRedBlackTree<unsigned int,
  // TRedBlackTree<unsigned int, EBoneParticle *> *>).
  using BoneParticleTree = std::map<unsigned int, std::map<unsigned int, EBoneParticle*>*>;
  void cleanupParticlesDelayed(BoneParticleTree* tree);  // 80065FF4
  void cleanupParticlesImmediate(BoneParticleTree* tree);  // 80066054
  void cleanupParticles(BoneParticleTree* tree);  // 800660BC
  static void playRumble(char *);  // 800661D8
  void playRumble(RumbleDataElement *);  // 8006622C
  static void triggerCameraBloom(char *);  // 8006634C
  void triggerCameraBloom(CameraBloomDataElement *);  // 800663A0
  static void triggerCameraBlur(char *);  // 800663E8
  void triggerCameraBlur(CameraBlurDataElement *);  // 8006643C
  void footstepEvent(char *);  // 80066470
  // `ref` on skill track `track` (0x10, algorithmic idles 0x20), forwards or backwards; the sim faces
  // the person's direction. 0: no model yet.
  int startSkill(const AnimRef* ref, bool backwards, unsigned int track);  // 80066490
  // The skill animation reached its end (its last events handled) and no event waits.
  int isAnimationDone();  // 800667F0
  void stopCurAnim(bool);  // 80066940
  void setPersonDirection(float dir);  // 8006697C
  void updateCarryAnimation();  // 80066A20
  void lockHandsUpCarryNodes();  // 80066F00
  void lockCarryArmNodes(float *, EACTrack *);  // 80066F40
  void lockCarryArmNodes(float *, int);  // 80066F80
  // The outfit in the person's data 8 (SAnimator::OutfitS2C: 0 normal, 1 naked, 2 swimsuit, 3 the job's
  // uniform, 4 marriage, 5 PJ, 6 workout, 7 sci-fi, 8 yeti, 9 panda, 10 robot) put on when it changed.
  void updateRenderModels();  // 80066FC0
  int setJobModel();  // 800670E0
  int wearNormal();  // 800671C0
  // The costume's description copied over the person's (its own clothes kept at +0x604 the first time),
  // the face and body put back, and the ESim asked to change. 1: asked.
  int setNewModel(const char* name, bool, bool);  // 80067288
  void RestoreNonCostumeRelatedSimDescriptionItems(CasSimDescriptionS2C* desc, CasSimDescriptionS2C* own, bool costumeHasHead);  // 800673FC
  static const char* GetCostumeName(int outfit);  // 800675A0
  static RET_UNKNOWN removeCostume();  // 800676E4
  virtual void GetCarryHandPosAndDir(EVec3 &, EVec3 &, EMat4 &);  // 800676EC
  virtual void GetBonePos(int bone, EVec3& out);  // 80067758
  bool PropsHaveAlpha();  // 800677B0
  virtual void DrawProps(ERC *, bool, EShader *);  // 8006781C
  void DrawPropsShadow(ERC *);  // 800679E8
  void removeAllProps();  // 80067ABC
  void updateCensor();  // 80067B54
  virtual void DrawCensor(ERC *);  // 80067C4C
  void AlignCensorToCamera(EVec3 &, EVec3 &, E3DWindow *);  // 8006867C
  RET_UNKNOWN GetFootSound(CTilePt &, char *);  // 80068680
  virtual void SetIdleInitialized(bool initialized);  // 800689F4
  void UpdateAnimationValidity();  // 800689FC
  virtual RET_UNKNOWN GetAwareOfObject();  // 80068A40
  virtual void SetAwareOfObject(cXObject *);  // 80068A48
  virtual void SetAwareOfObjectKilled(cXObject *);  // 80068A6C
  virtual void ClearAwareOfObject(cXObject *);  // 80068A90
  void setAwarenessFollowStart(float);  // 80068AB4
  int shouldUseLowMotiveIdle();  // 80068B8C
  RET_UNKNOWN shouldUseLowMotiveWalk();  // 80068DD0
  bool IsSimulatorControlled();  // 80068ECC
  void SetPlayerControl(bool control);  // 80068FBC
  double SetPauseMultiplier(float);  // 80069024
  virtual double GetRealDir();  // 800690C0
  virtual EVec2& GetPos();  // 800690C8
  double GetTimeMultiplier();  // 800690D0
  cXPerson* GetPerson();  // 800690D8
  ESim* GetSim();  // 800690E0
  int GetAnimState();  // 800690E8
  RET_UNKNOWN GetFollowMode();  // 800690F0
  RET_UNKNOWN GetPortalMode();  // 800690F8
  void SetDesiredAnimState(int state);  // 80069104
  void SetCameraZoomRatio(float ratio);  // 8006910C
  int GetPlayerControl();  // 80069114
  void setAnimationDirectionToPersonDirection();  // 8006912C
  RET_UNKNOWN GetHeadOrient();  // 8006915C
  RET_UNKNOWN GetNormalSimDescription() const;  // 80069164
  void SetNormalSimDescription(CasSimDescriptionS2C *);  // 8006916C
  static RET_UNKNOWN IsStalled();  // 80069174
  double GetPauseMultiplier();  // 8006918C
  void SetBoneIndex(int, int);  // 80069194
  void SetRightHandBoneIndex(int);  // 800691A4
  void SetHeadBoneIndex(int);  // 800691AC
  void SetPelvisBoneIndex(int);  // 800691B4
  void SetTorsoBoneIndex(int);  // 800691BC
  RET_UNKNOWN GetBoneIndex(int);  // 800691C4
  RET_UNKNOWN GetRightHandBoneIndex();  // 800691D4
  RET_UNKNOWN GetHeadBoneIndex();  // 800691DC
  RET_UNKNOWN GetPelvisBoneIndex();  // 800691E4
  RET_UNKNOWN GetTorsoBoneIndex();  // 800691EC
  bool IsRouting();  // 800691F4
  bool IsDirectControlEnabled();  // 80069208
  int GetLastCostume();  // 80069220
  void SetLastCostume(int);  // 80069228
  double getPersonDirection();  // 80069230
  bool EndAutoRun();  // 800692E4
  int getIndexOfPropID(unsigned int id);  // 80069354
  void SetSkillName(AnimRef *);  // 800693B8
  void SetDesiredFacing(float);  // 800693E0
  void clearSuspendedCarry();  // 80069470
  void SetIdleAnimPlaybackParameters(EACTrack *);  // 800694E0
  static RET_UNKNOWN determineFirstFollowMode(TileList *);  // 8006952C
  void determineWalkRunStyle(float);  // 800695B8
  void StartDefaultIdle();  // 800696C0
  void DetachMonitoredTrack();  // 800697D8
  void updateDesiredAndDeltaDir(float &, float &);  // 8006982C
  void setFirstFollowMode(SAnimator2__eFollowMode);  // 80069870
  void setFollowMiddle();  // 800698E4
  void setFollowEnd();  // 80069920
  void setFollowDone();  // 80069998
  RET_UNKNOWN defaultVelocity(int);  // 80069A14
  double getAnimDuration(ERAnim *);  // 80069A8C
  void setSideStepSpeedFromAnimation(AnimRef *);  // 80069ADC
  void setMovementVelocityFromAnimation();  // 80069D10
  void setRotationRateFromShuffleDir(int, float);  // 80069FBC
  void initShuffleRotation(float delta);  // 8006A1A4
  RET_UNKNOWN getIsLeftFootUp();  // 8006A3AC
  int getShuffleDirFromDeltaDir(float delta);  // 8006A404
  double getDeltaDirFromDesiredDir(float desired);  // 8006A494
  double getDesiredDir();  // 8006A508
  double GetMovementVelocityFromStyle(SAnimator2__eWalkRunStyle);  // 8006A69C
  void setAnimationState();  // 8006A728
  void getTurnSkillID(const AnimRef*& out, int shuffle);  // 8006AA34
  static void getSidestepSkillIDs(AnimRef *&, AnimRef *&, float);  // 8006AC98
  void getWalkRunSkillID(const AnimRef*& out);  // 8006AD80
  RET_UNKNOWN UpdateNPCAutoRunState();  // 8006AF28
  void UpdateNPCAutoRun();  // 8006AFC4
  double getSidestepIntensity();  // 8006B4D0
  void initSidestep();  // 8006B52C
  void initWalkRun();  // 8006B9A4
  void initWalkRunFadeOut();  // 8006BC94
  void selectIdleOverlay();  // 8006BD5C
  void stopIdleOverlay();  // 8006BFE8
  bool skillIsIdle();  // 8006C040
  RET_UNKNOWN ShouldEndSkillTrack();  // 8006C094
  void startCarry();  // 8006C148
  void stopCarry();  // 8006C320
  void setAwarenessFollowEnd();  // 8006C490
  void setAwarenessFollowMiddle();  // 8006C664
  void exitAwarenessFollow();  // 8006C6DC
  static bool StartAutoRun(float);  // 8006C850

  // The anim states (eAnimState): 0 idle, 1 walk/run along a route, 2..4 sidesteps/turns while
  // routing, 5..7 skills, 8 idle (sim-controlled), 9 free-move idle, 10 impatient idle, 11 run stop,
  // 12 free-move turn on the spot, 13 free-move walk, 14 free-move run, 15..17 awareness.
  enum { kIdle = 0, kSimIdle = 8, kFreeIdle = 9, kImpatient = 10, kRunStop = 11, kFreeTurn = 12,
         kFreeWalk = 13, kFreeRun = 14 };
  // Walk/run styles (eWalkRunStyle): 0 walk, 1 run, 2 low-energy walk, 3 low-bladder walk, 4 none,
  // 5, 6 none, 7 free-move walk, 8 free-move run.
  static constexpr float kNoDir = -10000.0f;  // +0xB4 / +0x608: no desired direction

  // The flags of the controller's current main track (0 without one): 1 idle, 2 walk/run, 4/8 turns.
  unsigned int MainTrackFlags();
  // The most recent track fading out (the blend list's first), or null.
  EACTrack* OutgoingTrack();
  // The velocity of a walk/run style (1/16 tiles a second; GetMovementVelocityFromStyle).
  float StyleVelocity(int style);
  // The frame time scaled by the simulator's speed and the pause multiplier.
  float ScaledDt();
  // PC (inlined in TryAnimate): the carry track comes back after a skill played over it.
  void resumeCarryAfterSkill();

  // Per-node blend factors (one per node, 0x3A) for the secondary tracks: carrying with one arm or both,
  // the head, the fx track, the upper body (InitStaticAnimationElements).
  static float m_RightArmCarryIndexArray[0x3A];  // 804FB890
  static float m_LeftArmCarryIndexArray[0x3A];   // 804FB2C8
  static float m_DualArmCarryIndexArray[0x3A];   // 804FB978
  static float m_HeadIndexArray[0x3A];           // 804FBA60
  static float m_FXNodeIndexArray[0x3A];         // 804FBB48
  static float m_UpperBodyIndexArray[0x3A];      // 804FB1E0

  cXPerson* m_person = nullptr;          // +0x004
  ESim* m_sim = nullptr;                 // +0x008 its instance (the controller is ESim +0x334)
  int m_field00C = 0;                    // +0x00C the person's destination list (GetDestList)
  int m_routing = 0;                     // +0x010 1: following a route
  int m_routeIndex = 0;                  // +0x014
  int m_followMode = 0;                  // +0x018 eFollowMode (6.. awareness)
  float m_pos[2] = {};                   // +0x01C where it stands (animation format)
  int m_resetPos = 1;                    // +0x024 take the person's location next frame
  int m_pendingAnim = 0;                 // +0x028 an animation loading asynchronously
  int m_animState = kFreeIdle;           // +0x02C eAnimState
  float m_dir = 0.0f;                    // +0x030 facing (radians; 0 = direction 2, +x)
  float m_renderPos[2] = {};             // +0x034 where it is drawn
  unsigned int m_velocityKnown = 0;      // +0x03C bits: velocities / turn times read from animations
  float m_turnTime[2][11] = {};          // +0x040 seconds of each shuffle turn (by turn type, shuffle+5)
  float m_turnRate = 6.2831855f;         // +0x098 radians a second
  float m_walkVel = 19.84f;              // +0x09C 1/16 tiles a second
  float m_runVel = 64.96f;               // +0x0A0
  float m_field0A4 = 24.0f;              // +0x0A4
  float m_lowEnergyVel = 9.9f;           // +0x0A8
  float m_lowBladderVel = 9.9f;          // +0x0AC
  int m_desiredState = kFreeIdle;        // +0x0B0 the direct-control interactor's wish
  float m_desiredDir = kNoDir;           // +0x0B4
  float m_zoomRatio = 0.0f;              // +0x0B8 the camera's zoom (runs faster zoomed out)
  int m_eventFrame = 0;                  // +0x0C8 the frame events were processed to
  int m_overlayEventFrame = 0;           // +0x0CC
  float m_eventFraction = 0.0f;          // +0x0D0
  const AnimRef* m_curAnim = nullptr;    // +0x0D4 the main animation playing
  const AnimRef* m_overlayAnim = nullptr;  // +0x0D8
  float m_timeMult = 1.0f;               // +0x0FC the simulator's speed this frame
  float m_prevTimeMult = 1.0f;           // +0x100
  float m_pauseMult = 1.0f;              // +0x104
  int m_walkStyle = 0;                   // +0x108 eWalkRunStyle
  int m_prevWalkStyle = 0;               // +0x10C
  float m_walkBlend = 0.0f;              // +0x110 how far the walk cross-fade is
  int m_field114 = 0;                    // +0x114
  int m_shuffleDir = 2;                  // +0x118 the turn on the spot (-5..5, eighths)
  int m_longIdleCount = 0;               // +0x11C idles since the last long one
  int m_idleInitialized = 0;             // +0x120
  int m_idleState = 0;                   // +0x124 the person's idle state last frame
  float m_impatientTime = 0.0f;          // +0x128
  int m_lastCostume = 0;                 // +0x130 the costume worn (updateRenderModels; footsteps: 1, 2, 5 bare)
  CasSimDescriptionS2C* m_ownClothes = nullptr;  // +0x604 the person's own description while in a costume
  EVec3 m_bonePos[3];                    // +0x134, +0x14C, +0x158 right hand, head, pelvis (GetBonePos)
  EVec3 m_carryDir;                      // +0x140 the way a carried object faces (the pelvis's z axis, flat)
  short m_carryObject = 0;               // +0x164 the object held in the hand (updateCarryAnimation)
  std::vector<int> m_animEvents;         // +0x0DC the 'xevt' events waiting for the animate primitive
  int m_skillRunning = 0;                // +0x0EC an animate primitive's animation is playing
  int m_loopCount = 0;                   // +0x0F0 its loops so far
  int m_animatePrim = 0;                 // +0x1CC an animate primitive ran
  int m_censor = 0;                      // +0x17C the sim is censored (updateCensor: person data 0x1E)
  BoneParticleTree m_particles;          // +0x5D0 the main animation's bone particles (and attached ones)
  BoneParticleTree m_overlayParticles;   // +0x5DC the overlay animation's
  unsigned int m_skillAnimId = 0;        // +0x5E8 the skill animation playing
  unsigned int m_suspendedSkill = 0;     // +0x5EC a skill animation kept to resume (GC: social interrupts)
  int m_skillSuspended = 0;              // +0x5F0
  int m_reconstituted = 0;               // +0x5F4 read from a save
  // +0x168 the props it wears (AddProp; GC 0x0C each: the model's id, drawn in the shadow too, the
  // model once loaded).
  struct PreloadedModel {
    unsigned int m_res = 0;
    int m_flag = 0;
    ERModel* m_model = nullptr;
  };
  std::vector<PreloadedModel> m_preloadedModels;
  // Routing (PC: the path XRoute found, followed by moveAnimation; GC: the person's destination list,
  // a TileList, +0x00C).
  std::vector<EVec2> m_route;            // way points (animation format)
  size_t m_routeNode = 0;                // the one walked to
  float m_routeEndDir = kNoDir;          // the facing at the end (kNoDir: as it arrives)
  int m_routeResult = 0;                 // 0 walking, 1 arrived, -1 blocked
  int m_follow = 0;                      // PC: BeginFollow's walk (it ends in m_routing 2, done)
  unsigned char m_carryType = 0;         // +0x5F8
  unsigned char m_lowMotive = 0;         // +0x5F9 the low-motive walk (5 energy, 9 bladder)
  unsigned char m_lowMotiveIdle = 0;     // +0x5FA the motive whose low-motive idle plays (shouldUseLowMotiveIdle)
  unsigned char m_longIdle = 0;          // +0x5FB the long idle waiting to load (StdAnimIdx)
  float m_turnStartDir = kNoDir;         // +0x608
  int m_boneIndex[4] = {-1, -1, -1, -1}; // +0x60C right hand, head, pelvis, torso
  unsigned int m_flags = 0;              // +0x61C 1 player control, 0x10 turn playing, 0x40 shuffle,
                                         //   0x1000 idle overlay, 0x10000 events, 0x800000 run stop
  unsigned int m_awareFlags = 0;         // +0x620 AwarenessManager
  EACTrack* m_skillOverlay = nullptr;    // +0x668
};

// PC: the animation id an AnimRef record names (its first word; the records stay in the quickdata).
unsigned int AnimRefId(const AnimRef* ref);
// Record `index` of AnimRefTable `id` of an object file (its ResFile row; AnimTableImpl::Load / GetEntry).
const AnimRef* AnimTableEntry(const unsigned char* resFile, short id, int index);
