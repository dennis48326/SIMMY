// @class cXPerson after cXObjectImpl CasSimDescriptionS2C ObjSelector Motives Neighbor XRoute ActionQueue
// @class cXPersonImpl
// A sim in the simulation (game/person.cpp): cXPerson, the interface the 3D side (ESim) and the camera
// ask, and cXPersonImpl, the person object. GC: cXPersonImpl is also the behaviour (motives, wants and
// fears, interactions, routing) and owns the sim's animator (SAnimator2, +0x3F4). Ported: motives and
// the mood, the action queue, the behaviour stack's person primitives, autonomy (autonomy.cpp), social
// mode (socialmode.cpp), service NPC records; the wants and fears are its neighbour's (wantfear.cpp).
// PC: its description comes from whoever made it (see PlayerSimDescription in person.cpp).
#pragma once
#include <vector>

// GC: an abstract class (its own vtable at +0x04 of the implementation). PC: the virtual methods live
// mode uses, defaulting to "nothing" as the GC's pure virtuals have no body.
struct cXPerson {
  cXPerson() = default;
  virtual ~cXPerson();  // 8011C65C
  cXPerson(int);  // 8012C4D0
  void setPersonImpl(cXPersonImpl *);  // 8012C684
  virtual void CastToPerson();  // 8012C694
  RET_UNKNOWN CAST_IMPL();  // 8012C698
  virtual int GetPersonData(int) const { return 0; }                       // 0x13C
  virtual CasSimDescriptionS2C* GetSimDescription() { return nullptr; }     // 0x154
  // The service NPC's "Sim::NPC" row (see ObjSelector::m_npc); null for other people.
  virtual const unsigned char* GetServiceNPC() { return nullptr; }         // 0x164
  virtual SAnimator2* GetSAnimator() const { return nullptr; }             // 0x18C
  virtual int IsGhost() { return 0; }                                      // 0x21C
  virtual int IsInvisible() { return 0; }                                  // 0x23C
  virtual int IsGreen() { return 0; }                                      // 0x244
  virtual double GetMotive(int) { return 0.0; }                            // 0x5C
  // PC: the person's object side (GC: the cXObject the interface is a part of, *(this - 4)).
  virtual cXObject* AsObject() { return nullptr; }
};

// A person object (GC 0x5A8 bytes).
struct cXPersonImpl : public cXObjectImpl, public cXPerson {
  cXPersonImpl(int, ObjSelector* selector, ObjectModule* module);  // 8011AE04
  ~cXPersonImpl() override;  // 8011C77C
  void Initialize();  // 8011E0FC
  void ReconStream(ReconBuffer *, int, bool) override;  // 80125048
  int GetPersonData(int index) const override;  // 8012C758
  SAnimator2* GetSAnimator() const override;  // 8012C7EC
  CasSimDescriptionS2C* GetSimDescription() override;  // 8012C8C0
  const unsigned char* GetServiceNPC() override;  // 8012C8D0
  int IsInvisible() override;  // 8012C8E0
  int IsGreen() override;  // 8012C8EC
  int IsGhost() override;  // 8012C8F8
  cXObject* AsObject() override { return this; }
  static void InitializeStaticMemory();  // 8011A824
  static void CleanupStaticMemory();  // 8011A90C
  static void EORDrawStickFigure(int);  // 8011AD64
  void GetNextQueueStr(int);  // 8011AD9C
  void Reset(bool);  // 8011EB78
  void PostLoad(int, bool);  // 8011EC7C
  // Its motive effects' curves: the defaults (803DCC3C), or a string set file (definition +0x66).
  void LoadMotiveEffects();  // 8011EF40
  void PreSave();  // 8011F06C
  int TrySetMotiveDelta(StackElem* frame, const unsigned char* params);  // 8011F0BC
  // Primitive 0x1E: the stack object's interaction (my data 0x14 the entry) runs as the current action.
  int TryGosubFoundAction(StackElem* frame);  // 8011F460
  int TryLookTowards(StackElem* frame, const unsigned char* params);  // 8011F654
  // The go-to-routing-slot primitive (0x2D): operand +0 the slot (+2: 0 the param it names, 1 itself,
  // 2 a global slot), +4 bit 0 clear: failure feedback (slot flag 0x4000).
  int TryGotoRoutingSlot(StackElem* frame, const unsigned char* params);  // 801203A8
  // Routes to `slot` of the frame's stack object over several ticks (the frame's primitive state):
  // 1 there, 0 failed, 2 on its way.
  int TryGotoRoutingSlot(StackElem* frame, RoutingSlot* slot);  // 80120640
  // A person in the way of `route` (not one a player steers) is asked to move out of it (MoveOutOfWay).
  int AskOthersToMove(XRoute* route);  // 80122120
  // Pushes the "move out of the way" interaction (an object of GUID 0x7C4 here, its slot a ring of 1..5
  // tiles) at `priority`.
  bool MoveOutOfWay(int priority);  // 80122AAC
  // Asked to step off `path` (another's route): the same, to either side of where the path crosses it.
  int MoveOutOfWay(XRoute* route, TileList& path);  // 80122E00
  XRoute* GetCurrentRoute();  // 80123704
  // The path to `route`'s best reachable goal into m_path with the facing point: 2 walk it, 1 there (in
  // the goal's seat), 3 the seat's sit tree was pushed, 0 none (the route's failure says why).
  int InitRoute(XRoute* route);  // 8012373C
  // The go-relative primitive (0x1B): to a spot next to the stack object, operand +2 the side
  // (-1/7..: any, -2 on top, 0..7 a direction, 1/3/5/7 the two beside it), +3 the facing (-1 any, -2
  // toward it, 0..7), +6 flags (2 clear: failure feedback, 4 different altitudes).
  int TryGotoRelative(StackElem* frame, const unsigned char* params);  // 80124370
  // To another room: the best portal's tree (entry 0xF) is pushed (1), or none (0).
  int TryRoomRouting(XRoute* route);  // 80124698
  RET_UNKNOWN TryGetReachInfo(StackElem *, XPrimParam *, cXObject **, cXObject **, int *, float *);  // 8012479C
  RET_UNKNOWN TryReach(StackElem *, XPrimParam *);  // 801249F4
  RET_UNKNOWN NetTrySnapTo(StackElem *, XPrimParam *);  // 80124A4C
  int TryElement(StackElem* frame, short op, const unsigned char* params) override;  // 80124ADC
  // The object's tick, then the person's: motives every 60 ticks, motive deltas every tick.
  int Simulate(int ticks) override;  // 80124D20
  static RET_UNKNOWN ReconType();  // 8012503C
  double GetMotive(int index) override;  // 801253A4
  float* GetMotiveRef(int index);  // 801253B4
  float* GetOldMotiveRef(int index);  // 801253C4
  void SetMotive(int index, float value);  // 801253D4
  void SimMotives();  // 801253E4
  void CalcHappy();  // 80125550
  double GetAspirationScore();  // 801256EC
  void SetAspirationScore(float);  // 8012574C
  int GetAspirationStatus();  // 801257A8
  void SpawnAspirationBalloon(int);  // 80125834
  RET_UNKNOWN RunImmediateAction(Interaction *);  // 80125980
  int IsIdle() { return m_hasCurrentAction ^ 1; }  // 80125AD0
  // Queues `action` (immediate entries run at once; a repeat of the last one is dropped; with six
  // visible, only higher priorities or must-runs; lower priorities are skipped). 1: queued.
  int AddAction(Interaction* action);  // 80125ADC
  // The action with unique id `id`: the running one is cancelled (person data 0x21 cleared so its
  // trees give up), a queued one skipped. 1: found.
  int RemoveAction(int id);  // 80125E64
  RET_UNKNOWN HasQueuedActionOfPri(int) const;  // 80125FE8
  // B: the last queued action (or the running one) is cancelled. 1: removed.
  int CancelLastAction();  // 8012606C
  int CancelAllActions();  // 80126194
  int CancelAllButLastActions();  // 801262B0
  Interaction* GetIndAction(int index, bool includeCurrent);  // 80126360
  Interaction* GetCurrentAction();  // 801263C8
  RET_UNKNOWN GetCurrentInteractionStackObject();  // 8012640C
  RET_UNKNOWN GetCurrentInteractionIconObject();  // 80126468
  static void DebugDumpHappyScape();  // 801264C4
  // Whether what it does should give way: the current action cancelled, a higher priority (or
  // continuation) queued, a motive below -99, or a run-as-sub action at the head.
  int ShouldInterrupt();  // 801264C8
  int TryTestInteractingWith(StackElem* frame);  // 80126640
  int TryChangeSuit(StackElem* frame, const unsigned char* params);  // 801266CC
  int TryIdleForInput(StackElem* frame, const unsigned char* params);  // 80126A20
  // The queue's head runs (its check tree passing) as the current action on top of `frame`: 3 (gosub);
  // 2 skipped; 1 nothing to run.
  int executeInterruptOnIdlePrimitve(StackElem* frame, short* ticks, bool noSub);  // 80126B64
  // Primitive 0x40: an idle animation while waiting (+6: 0 once, 1 ticks, 2 loops, counted down from
  // param +3 or, with flag 0x10, the literal +7).
  int TryIdleAnimate(StackElem* frame, const unsigned char* params);  // 80126E58
  // Primitive 0x03, autonomy (autonomy.cpp).
  int TryFindBestAction(StackElem* frame);  // 80127114
  void Skipping3D();  // 80128134
  void UpdateCurrentRoom();  // 801281B4
  void Place(FTilePt &, int, cXObject *, int);  // 80128294
  int TrySocialMode(StackElem* frame, const unsigned char* params);  // 801282E0 (socialmode.cpp)
  void SetHilite(int);  // 801294C8
  RET_UNKNOWN IsSelected();  // 8012953C
  bool IsSelectableByPlayer(int);  // 8012954C
  RET_UNKNOWN IsPrimarySim(int);  // 8012971C
  void AttachToHUD(bool);  // 80129804
  RET_UNKNOWN IsPersonInFamily(int) const;  // 80129878
  void ForceLocation();  // 801298E8
  // cXPersonImpl::Place (80128294, vtable 0x13C) at `pos` (FTilePt) on its level, out of any container.
  void PlacePerson(const int pos[2]);
  int GosubObjectTree(cXObject* object, const short* params, short tree, bool setIcon) override;  // 80129934
  // An object tree the person runs (GosubObjectTree) returned: the object's user count (data 0x3E) drops.
  void StackJustPopped();  // 80129C40
  void TreeStackPopped() override { StackJustPopped(); }
  void SetCurrentAction(Interaction& action);  // 80129ECC
  void UpdateCurrentAction();  // 80129F10
  void Cleanup(cXObject *);  // 8012A04C
  void CompleteCurrentAction();  // 8012AC7C
  void DeleteTopAction();  // 8012AD28
  void ActionSkipped(Interaction &);  // 8012AD98
  void DumpDestList(char *);  // 8012AE88
  // Its sleep state (motive 0xB) below -30.
  bool IsSleeping();  // 8012B1B8
  bool IsDog();  // 8012B1D4
  bool IsCat();  // 8012B254
  bool IsMonkey();  // 8012B2D4
  int IsPet();  // 8012B354
  bool IsChild();  // 8012B42C
  int IsMale();  // 8012B4AC
  RET_UNKNOWN IsFemale();  // 8012B528
  RET_UNKNOWN IsAdult();  // 8012B564
  void InvalidateRoutes();  // 8012B5A0
  void StartRecording(int, int);  // 8012B5D4
  static void StopRecording();  // 8012B5D8
  static void ClearRecording();  // 8012B5DC
  static RET_UNKNOWN TickRecording();  // 8012B5E0
  void LogEvent(char *, char *, char *);  // 8012B5E8
  static void Track();  // 8012B5EC
  void GetJobSuitTex(StringBuffer &, StringBuffer &, StringBuffer &);  // 8012B5F0
  RET_UNKNOWN UserCanPickup();  // 8012B5F4
  static void Turn(int);  // 8012B6D8
  RET_UNKNOWN IsModelLoaded();  // 8012B754
  RET_UNKNOWN IsPersonDying();  // 8012B784
  // The current action's name (the head of the queue).
  void SetCurrentActionText(const std::u16string& text);  // 8012B804
  void SetFirstPlayerInSocialMode(bool);  // 8012B82C
  void SetSecondPlayerInSocialMode(bool);  // 8012B834
  void SetWaitingForSocialMode(bool);  // 8012B8C4
  void CheckFirstPlayerForFailedSocialModeEntry();  // 8012B8CC
  void CheckSecondPlayerForFailedSocialModeEntry();  // 8012B978
  RET_UNKNOWN GetSocialModeTarget();  // 8012BB20
  void IncrementSimSocialInterest(float);  // 8012BB8C
  double GetSimSocialInterestAsMotiveValue(cXPerson *);  // 8012BDD8
  void SetSimSocialIntrestFromMotiveValue(cXPerson *, float);  // 8012BF48
  void InitWantFearIcons();  // 8012C08C
  void SpawnWantFearDialog(unsigned int) const;  // 8012C0F8
  void SetIdlePlayerSimAutonomous();  // 8012C16C
  // Holding a direct-control object (definition +0x0A 0x23..0x25).
  bool IsCarryingDCObject();  // 8012C1F4
  RET_UNKNOWN GetNormalSimDescription();  // 8012C2B8
  virtual void CastToPersonImpl();  // 8012C6DC
  static void GetPersonImplementation();  // 8012C6E0
  int CountActions(bool includeCurrent);  // 8012C6E4
  RET_UNKNOWN GetPendingAction() const;  // 8012C714
  RET_UNKNOWN GetCurrentInteractionNC();  // 8012C72C
  void SetPersonData(int, short);  // 8012C768
  RET_UNKNOWN GetPersonDataArray();  // 8012C778
  // Person data 0: 0 standing, 1 sitting, 3 (GC: lying), ...
  int GetIdleState();  // 8012C780
  // Something is in its hand (slot 0).
  bool IsCarrying();  // 8012C788
  void SetIdleLoopCount(short);  // 8012C7DC
  RET_UNKNOWN GetDestList();  // 8012C7E4
  int GetCurrentRoom() const;  // 8012C7F4
  void GetNeighborID() const;  // 8012C7FC
  void SetNeighborID(short);  // 8012C838
  bool IsRouting();  // 8012C878
  RET_UNKNOWN IsVisitor();  // 8012C8A8
  RET_UNKNOWN IsAlien();  // 8012C904
  RET_UNKNOWN IsInMotiveFailure();  // 8012C910
  RET_UNKNOWN NeedsWantFearShuffle();  // 8012C91C
  void SetInvisible(bool);  // 8012C928
  void SetIsAlien(bool);  // 8012C950
  RET_UNKNOWN GetVisibility();  // 8012C978
  Motives* GetMotives() { return &m_motives; }  // 8012C980
  MotiveEffects* GetMotiveEffects() { return m_motiveEffects; }  // 8012C988
  RET_UNKNOWN GetRecording();  // 8012C990
  RET_UNKNOWN GetRecordDuration();  // 8012C998
  void SetRecordDuration(int);  // 8012C9A0
  RET_UNKNOWN GetRecordMaxDuration();  // 8012C9A8
  void SetRecordMaxDuration(int);  // 8012C9B0
  RET_UNKNOWN GetRecordStartTicks();  // 8012C9B8
  RET_UNKNOWN GetRecordCurTicks();  // 8012C9C0
  RET_UNKNOWN GetRecordTicksElapsed();  // 8012C9C8
  RET_UNKNOWN GetRecordSkill();  // 8012C9D0
  int GetRouteStackSize() { return static_cast<int>(m_routes.size()); }  // 8012C9D8
  void SetChangingOutfitMode(bool);  // 8012C9F8
  int GetLastMotiveUpdateTick() const { return m_lastMotiveTick; }  // 8012CA00
  void SetLastMotiveUpdateTick(int tick) { m_lastMotiveTick = tick; }  // 8012CA08
  cXObject* IgnoringObject() { return m_ignoringObject; }  // 8012CA10
  void SetIgnoringObject(cXObject *);  // 8012CA18
  void ClearIgnoringObject();  // 8012CA20
  RET_UNKNOWN IsInSocialMode();  // 8012CA2C
  RET_UNKNOWN IsSecondPlayerInSocialMode();  // 8012CA44
  RET_UNKNOWN IsFirstPlayerInSocialMode();  // 8012CA4C
  RET_UNKNOWN IsWaitingForSocialMode();  // 8012CA54
  // The object whose tree it runs (the newest GosubObjectTree's), else itself.
  cXObject* GetControllingObject();  // 8012CA5C
  static void SetAwareOfObject(cXObject *);  // 8012CAA8
  static void SetAwareOfObjectKilled(cXObject *);  // 8012CAE0
  static void ClearAwareOfObject(cXObject *);  // 8012CB18
  RET_UNKNOWN GetSimMemory(cXPerson *, int);  // 8013FDF0
  void SetSimMemory(cXPerson *, int, int);  // 8013FE84
  void GetDominantMemory(cXPerson *, int);  // 8013FF20

  // PC: puts the person at `x`, `y` (tiles, centre at .5) on `level`, facing `dir` (eighths of a turn).
  void SetLocation(float x, float y, int level, int dir);

  short m_personData[0x100] = {};             // +0x08 (of the person part) GetPersonData
  // A motive delta (TrySetMotiveDelta, GC MotiveInc 0x0C): `delta` a tick until `limit`.
  struct MotiveInc {
    int m_motive = 0;
    float m_delta = 0.0f;
    float m_limit = 0.0f;
  };
  Motives m_motives;                          // +0xA8
  int m_lastMotiveTick = 0;                   // +0x12C (cXPersonImpl +0x4B words)
  unsigned short m_currentRoom = 0xfffb;      // +0x41C the room it is in (UpdateCurrentRoom; 0xFFFB none yet)
  std::vector<MotiveInc> m_motiveDeltas;      // +0x3E4
  Neighbor* m_neighbor = nullptr;             // PC: the neighbour it is (GC: by its neighbour id)
  // The object trees running on its stack (GC +0x420, 0x0C each): the object, the stack size with the
  // tree's frame, whether it sets the action queue's icon.
  struct GosubObject {
    cXObjectImpl* m_object = nullptr;
    int m_depth = 0;
    bool m_icon = false;
  };
  std::vector<GosubObject> m_gosubObjects;
  ActionQueue m_actionQueue;                  // +0x130 (head +0x3D8, tail +0x3DC)
  int m_hasCurrentAction = 0;                 // +0x534 the queue's head is running
  cXObject* m_ignoringObject = nullptr;       // +0x53C the object it routes through (SetIgnoringObject)
  short m_idleAnimateCount = -0x8000;         // +0x540 TryIdleAnimate's count (0x8000: none)
  int m_firstInSocialMode = 0;                // +0x544 SetFirstPlayerInSocialMode
  int m_secondInSocialMode = 0;               // +0x548 SetSecondPlayerInSocialMode
  int m_waitingForSocialMode = 0;             // +0x54C SetWaitingForSocialMode
  SAnimator2* m_animator = nullptr;           // +0x3F4
  TileList m_path;                            // +0x3F8 the way being walked (InitRoute; the animator follows it)
  MotiveEffects* m_motiveEffects = nullptr;   // +0x408 (made by Initialize; autonomy's curves)
  std::vector<XRoute> m_routes;               // +0x40C the routes being walked (GC: a stack of them)
  CasSimDescriptionS2C* m_desc = nullptr;     // GC: the selector's (+0x8C) neighbour description (+0x8C)
  float m_pos[2] = {};                        // PC: where it stands (tiles, x and y)
  int m_dir = 0;                              // PC: facing (eighths of a turn)
};

// PC (GC: the neighbourhood's family and Neighbor records): the description of the sim player
// `player` plays, for the family's people on the lot; null for none.
CasSimDescriptionS2C* PlayerSimDescription(int player);
// PC stand-in for the family moving onto the lot (GC: Neighborhood::LoadHouse finds the family's
// people in the house, or the move-in makes them, and cXPersonImpl::Initialize gives each its ESim):
// a person per playing player, placed at the lot's entry point, selected for that player.
void MoveFamilyOntoLot(EHouse* house);
// A person for neighbour `n` out of the world (TryCreateObject's neighbour path; PC stand-in for
// MakeNewOutOfWorldObject with the neighbour's selector making a cXPersonImpl). Null: none made.
cXPersonImpl* MakeNeighborPerson(Neighbor* n, ObjSelector* selector);
// The people made by MoveFamilyOntoLot (GC: ObjectModule's person list), freed with the house.
void RemoveFamilyFromLot();
// PC stand-in for the action menu: `person` queues `object`'s first available interaction. 1: queued.
int PushInteraction(cXPersonImpl* person, cXObjectImpl* object, const char* pick = nullptr);
// The motive `index` of player `player`'s selected sim (cXPerson::GetMotive, vtable 0x5C); 0 for none.
float PlayerMotive(int player, int index);
// PC development aid (SIMMY_SIM_TRACE): player `player`'s sim's motives and tree stack.
void TraceSelectedPerson(int player);
// A service NPC record's kind (+4: 0 adult, 1 child, 2 dog, 3 cat, 4 monkey); -1 for none.
int ServiceNPCKind(const unsigned char* npc);
