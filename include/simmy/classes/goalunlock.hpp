// @class IGoalUnlock__UnlockType
// @class GoalUnlock
// The player family's goals and unlocks (game/goalunlock.cpp): two bits per unlockable thing, "unlocked"
// and "recently unlocked" (shown as new), in 0x40 shorts streamed with the family
// (PlayerFamilyImpl::DoStream). Each kind of unlock (IGoalUnlock::UnlockType) has its own run of shorts:
// 0 recipes (32), 1 fashions (208), 2 lots (24), 3 and 4 (32 each), 5 catalogue objects (128, those
// whose object file has an unlock level, in s_objectUnlocks: SetupObjectUnlockInfo).
// PC: the shorts stay in PlayerFamilyImpl::m_goals (the family keeps its streamed data); a GoalUnlock
// is a view on them (FamilyGoalUnlock). GC: the GoalUnlock object itself held them (+0x04).
#pragma once

struct IGoalUnlock__UnlockType {
  SIMMY_ENUM_BODY(IGoalUnlock__UnlockType)
};

// 0x84 bytes on GC (vtable, then the 0x40 shorts).
struct GoalUnlock {
  GoalUnlock();  // 800B8438
  explicit GoalUnlock(short* bits) : m_bits(bits) {}
  virtual ~GoalUnlock();  // 800B8474
  virtual void DoStream(ReconBuffer* buffer, int version);  // 800B84A8
  virtual void ClearGoalsAndUnlocks();  // 800B8510
  virtual int GetUnlockCount(IGoalUnlock__UnlockType type);  // 800B853C
  static void SetupObjectUnlockInfo();  // 800B859C
  virtual int GetObjectIndexFromGuid(int guid);  // 800B86D8
  int GetFirstUnlockVar(IGoalUnlock__UnlockType type);  // 800B875C
  virtual int IsUnlocked(IGoalUnlock__UnlockType type, short index);  // 800B87CC
  virtual void GrantUnlock(IGoalUnlock__UnlockType type, short index);  // 800B8880
  virtual void GrantObjectUnlocks(int level);  // 800B894C
  virtual int IsRecentlyUnlocked(IGoalUnlock__UnlockType type, short index);  // 800B8A6C
  virtual void SetRecentlyUnlocked(IGoalUnlock__UnlockType type, short index, bool on);  // 800B8B24
  virtual int GetUnlockTotal(IGoalUnlock__UnlockType type);  // 800B8BF4

  short* m_bits = nullptr;  // +0x04 the 0x40 shorts (PC: the family's)
};

// s_objectUnlocks (80485E7C): per unlockable catalogue object its GUID and unlock level (x100 points).
struct ObjectUnlockInfo {
  unsigned int guid = 0;
  short level = 0;
};
extern ObjectUnlockInfo s_objectUnlocks[0x80];

// PC: family `number`'s goals and unlocks (GC: PlayerFamilyImpl::GetGoalAndUnlockData, vtable 0x14C,
// the GoalUnlock at +0xCD8); its bits are null without that player family.
GoalUnlock FamilyGoalUnlock(int number);
