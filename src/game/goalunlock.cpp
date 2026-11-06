// Port of game/goalunlock.cpp: the player family's goals and unlocks. See goalunlock.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

ObjectUnlockInfo s_objectUnlocks[0x80];

namespace {
const unsigned int kSimsObjectsQuickdata = 0x0c33db41u;
}  // namespace

// 800B8438
GoalUnlock::GoalUnlock() { ClearGoalsAndUnlocks(); }
// 800B8474
GoalUnlock::~GoalUnlock() {}

// 800B84A8
// 0x40 shorts; before version 0x11 0x27 of them, then ClearGoalsAndUnlocks (vtable 0x14) as GC does.
void GoalUnlock::DoStream(ReconBuffer* buffer, int version) {
  if (!m_bits) return;
  if (version < 0x11) {
    buffer->Recon16(m_bits, 0x27);
    ClearGoalsAndUnlocks();
  } else {
    buffer->Recon16(m_bits, 0x40);
  }
}

// 800B8510
void GoalUnlock::ClearGoalsAndUnlocks() {
  if (m_bits) std::memset(m_bits, 0, 0x80);
}

// 800B853C
// How many unlocks of the kind there are.
int GoalUnlock::GetUnlockCount(IGoalUnlock__UnlockType type) {
  switch (static_cast<int>(type)) {
    case 0: return 0x20;
    case 1: return 0xd0;
    case 2: return 0x18;
    case 3: return 0x20;
    case 4: return 0x20;
    case 5: return 0x80;
    default: return 0;
  }
}

// 800B859C
// Per object file ("SimsObjects" ResFile row) its first definition with an unlock level (+0x0C > 0):
// the definition's GUID (+0x1C) and level, in file order; the rest of the 0x80 cleared.
void GoalUnlock::SetupObjectUnlockInfo() {
  auto* qd = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(kSimsObjectsQuickdata, nullptr, 0));
  unsigned int n = 0;
  const void* table = qd ? qd->getTable("ResFile") : nullptr;
  const unsigned int rows = table ? ERQuickdata::TableRowCount(table) : 0;
  for (unsigned int i = 0; i < rows && n < 0x80; ++i) {
    const void* row = ERQuickdata::TableRow(table, i);
    const auto* defs = static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 0));
    const unsigned int count = defs ? ERQuickdata::U32(defs - 4, 0) : 0;
    for (unsigned int j = 0; j < count; ++j) {
      const void* def = ERQuickdata::Ptr(defs, j * 4);
      if (!def || ERQuickdata::S16(def, 0xc) <= 0) continue;
      s_objectUnlocks[n].guid = ERQuickdata::U32(def, 0x1c);
      s_objectUnlocks[n].level = ERQuickdata::S16(def, 0xc);
      ++n;
      break;
    }
  }
  if (qd) _quickdataman.DelRef(qd, EResourceManager__DelRefMode{0});
  for (; n < 0x80; ++n) s_objectUnlocks[n] = ObjectUnlockInfo();
}

// 800B86D8
// The object's unlock index, or -1 when it is not an unlockable object.
int GoalUnlock::GetObjectIndexFromGuid(int guid) {
  for (short i = 0; i < GetUnlockCount(IGoalUnlock__UnlockType{5}); ++i)
    if (static_cast<unsigned int>(guid) == s_objectUnlocks[i].guid) return i;
  return -1;
}

// 800B875C
// The first short of the kind's bits.
int GoalUnlock::GetFirstUnlockVar(IGoalUnlock__UnlockType type) {
  switch (static_cast<int>(type)) {
    case 0: return 0;
    case 1: return 0x1f;
    case 2: return 4;
    case 3: return 7;
    case 4: return 0xb;
    case 5: return 0xf;
    default: return 0;
  }
}

// 800B87CC
// Bit 2 * index of the kind's shorts.
int GoalUnlock::IsUnlocked(IGoalUnlock__UnlockType type, short index) {
  if (!m_bits || index < 0 || index >= GetUnlockCount(type)) return 0;
  const int bit = index * 2;
  return (m_bits[GetFirstUnlockVar(type) + bit / 16] & (1 << (bit % 16)) & 0xffff) ? 1 : 0;
}

// 800B8880
// Unlocked, and marked recently unlocked (vtable 0x4C).
void GoalUnlock::GrantUnlock(IGoalUnlock__UnlockType type, short index) {
  if (!m_bits || index < 0 || index >= GetUnlockCount(type)) return;
  const int bit = index * 2;
  m_bits[GetFirstUnlockVar(type) + bit / 16] |= static_cast<short>(1 << (bit % 16));
  SetRecentlyUnlocked(type, index, true);
}

// 800B894C
// Every object whose unlock level the points (x100) reach is granted. GC: on the game thread, the HUD
// then shows a reward moment for the last one (H2DTarget::SpawnRewardMomentDialog, kind 5).
void GoalUnlock::GrantObjectUnlocks(int points) {
  int last = -1;
  for (short i = 0; i < GetUnlockCount(IGoalUnlock__UnlockType{5}); ++i) {
    if (s_objectUnlocks[i].level == 0 || s_objectUnlocks[i].level > static_cast<short>(points / 100)) continue;
    if (IsUnlocked(IGoalUnlock__UnlockType{5}, i)) continue;
    GrantUnlock(IGoalUnlock__UnlockType{5}, i);
    last = i;
  }
  if (last != -1 && _gd.m_h2dTarget) _gd.m_h2dTarget->SpawnRewardMomentDialog(5, last);  // GC: on the game thread only
}

// 800B8A6C
// Bit 2 * index + 1.
int GoalUnlock::IsRecentlyUnlocked(IGoalUnlock__UnlockType type, short index) {
  if (!m_bits || index < 0 || index >= GetUnlockCount(type)) return 0;
  const int bit = index * 2 + 1;
  return (m_bits[GetFirstUnlockVar(type) + bit / 16] & (1 << (bit % 16)) & 0xffff) ? 1 : 0;
}

// 800B8B24
void GoalUnlock::SetRecentlyUnlocked(IGoalUnlock__UnlockType type, short index, bool on) {
  if (!m_bits || index < 0 || index >= GetUnlockCount(type)) return;
  const int bit = index * 2 + 1;
  short& word = m_bits[GetFirstUnlockVar(type) + bit / 16];
  const unsigned short mask = static_cast<unsigned short>(1 << (bit % 16));
  word = static_cast<short>(on ? (static_cast<unsigned short>(word) | mask) : (static_cast<unsigned short>(word) & ~mask));
}

// 800B8BF4
// How many of the kind are unlocked.
int GoalUnlock::GetUnlockTotal(IGoalUnlock__UnlockType type) {
  int total = 0;
  for (short i = 0; i < GetUnlockCount(type); ++i)
    if (IsUnlocked(type, i)) ++total;
  return total;
}

GoalUnlock FamilyGoalUnlock(int number) {
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  auto* family = hood ? dynamic_cast<PlayerFamilyImpl*>(hood->GetFamily(number)) : nullptr;
  return GoalUnlock(family ? family->m_goals : nullptr);
}
