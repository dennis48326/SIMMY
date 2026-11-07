// Port of game/family.cpp (Family, FamilyImpl) and game/playerfamilyimpl.cpp (PlayerFamilyImpl): a
// family's members, house, money and flags, streamed as the neighbourhood file's 'FAMI' resources.
// A player family's inventory (BBI::InventoryItems), goals and unlocks (GoalUnlock) and clothing
// unlocks (CasClothingUnlocks) are kept as their streamed data; their classes are not ported.
// Not ported: putting the family on the lot (CreateFamilyInLot: the object module's people), friend
// counts and net worth.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

namespace {
constexpr int kFamilyResType = 0x46414d49;  // 'FAMI'
constexpr int kFamilyVersion = 0x12;
}  // namespace

// ---------------------------------------------------------------------------------------------------
// Family

// 800B6AEC
Family::Family() {}
// 800B5FA0
Family::~Family() {}

// 800B5F28
FamilyImpl* Family::CreateInstance(int& number) { return new FamilyImpl(number); }

// 800B5F60
void Family::DestroyInstance(Family* f) { delete f; }

// ---------------------------------------------------------------------------------------------------
// FamilyImpl

// 800B5FD4
FamilyImpl::FamilyImpl(int number) : m_number(number) {}

// 800B6068
FamilyImpl::~FamilyImpl() {}

// 800B6334
int* FamilyImpl::GetMemberByGUID(int guid) {
  for (int& m : m_members)
    if (m == guid) return &m;
  return nullptr;
}

// 800B62D0
// The person's selector's GUID is a member's.
bool FamilyImpl::TestMember(cXPerson* person) const {
  cXObject* o = person ? person->AsObject() : nullptr;
  ObjSelector* selector = o ? o->GetSelector() : nullptr;  // cXObject vtable 0x374 (GetSelector const)
  return TestMember(static_cast<int>(selector ? selector->GetGUID() : 0));
}

// 800B6368
bool FamilyImpl::TestMember(int guid) const {
  for (int m : m_members)
    if (m == guid) return true;
  return false;
}

// 800B63A4
int* FamilyImpl::GetIndexedMember(int index) {
  return index >= 0 && index < static_cast<int>(m_members.size()) ? &m_members[static_cast<size_t>(index)] : nullptr;
}

// 800B6414
int FamilyImpl::CountMembers() { return static_cast<int>(m_members.size()); }

// 800B6428
int FamilyImpl::GetFriendCount() { return Globs_pNeighborhood->GetFamilyFriendsCount(this); }
// 800B6464
int FamilyImpl::GetNetWorth() { return Globs_pNeighborhood->GetFamilyNetWorth(this); }

// 800B64A0
// Family `number` from its resource (none for a negative number: the family is emptied).
int FamilyImpl::LoadFamily(iResFile* file, int number) {
  if (number < 0) {
    m_number = -1;
    m_name.clear();
    return 0;
  }
  m_number = number;
  int version = 0;
  return LoadByResID(file, GetResfileStorageId(static_cast<short>(number)), &version) ? 1 : 0;
}

// 800B6530
void FamilyImpl::ClearFamily() {
  m_friendCount = 0;
  m_house = 0;
  m_funds = 0;
  m_houseValue = 0;
}

// 800B6548
// The house, creation order, funds (clamped first), house value, friend count, flags, the members'
// GUIDs (DoContainerStream<vector<FamilyMember>>: count, GUIDs) and the name. GC: a name read is then
// localised (free play lots 13..16 by EGlobal::GetNghFamilyName, others by GetNghHouseName).
void FamilyImpl::DoStream(ReconBuffer* b, int) {
  b->ReconInt(&m_house, 1);
  b->ReconInt(&m_creationOrder, 1);
  SetFunds(m_funds);
  b->ReconInt(&m_funds, 1);
  b->ReconInt(&m_houseValue, 1);
  b->ReconInt(&m_friendCount, 1);
  b->ReconInt(reinterpret_cast<int*>(&m_flags), 1);
  int n = static_cast<int>(m_members.size());
  b->Recon32(&n, 1);
  m_members.resize(static_cast<size_t>(n > 0 ? n : 0));
  for (int& m : m_members) b->Recon32(&m, 1);  // FamilyMember::DoStream (800B6A58)
  b->ReconString(m_name);
  if (b->m_mode == ReconBuffer__Mode::kRead) {
    if (static_cast<unsigned int>(m_house - 0xd) < 4) _globals.GetNghFamilyName(m_name);
    else _globals.GetNghHouseName(m_name);
  }
}

// 800B6650
bool FamilyImpl::LoadByResID(iResFile* file, short id, int* version) {
  m_members.clear();
  return ReconLoadObject(this, file, kFamilyResType, id, version) == 0;
}

// 800B6754
// As resource GetResfileStorageId(number), version 0x12 (whatever `version` says, as on GC).
bool FamilyImpl::SaveFamily(iResFile* file, int) {
  if (m_number == -1) return false;
  return ReconSaveObject(this, file, kFamilyResType, GetResfileStorageId(static_cast<short>(m_number)), kFamilyVersion) == 0;
}

// 800B6818
void FamilyImpl::AddMember(int guid) {
  if (!TestMember(guid)) m_members.push_back(guid);
}

// 800B69D4
void FamilyImpl::RemoveMember(int guid) {
  for (auto it = m_members.begin(); it != m_members.end(); ++it)
    if (*it == guid) {
      m_members.erase(it);
      return;
    }
}

// 800B6810
RET_UNKNOWN FamilyImpl::MyDoCommand(short, int) { return 0; }
// 800B6B00
short FamilyImpl::GetResfileStorageId(short number) { return number; }
// 800B6B08
int FamilyImpl::GetNumber() { return m_number; }
// 800B6B10
void FamilyImpl::SetNumber(int number) { m_number = number; }
// 800B6B18
int FamilyImpl::GetHouseNumber() { return m_house; }
// 800B6B20
void FamilyImpl::SetHouseNumber(int house) { m_house = house; }
// 800B6B28
int FamilyImpl::GetCreationOrder() { return m_creationOrder; }
// 800B6B30
void FamilyImpl::SetCreationOrder(int order) { m_creationOrder = order; }
// 800B6B38
int FamilyImpl::GetFunds() { return m_funds; }
// 800B6B40
void FamilyImpl::SetFunds(int funds) { m_funds = funds < 1000000 ? funds : 999999; }
// 800B6B60
int FamilyImpl::GetHouseValue() { return m_houseValue; }
// 800B6B68
void FamilyImpl::SetHouseValue(int value) { m_houseValue = value; }
// 800B6B70
void FamilyImpl::SetFriendCount(int count) {
  m_friendCount = count;
  m_friendCountDirty = 0;
}

namespace {
void SetFlag(unsigned int& flags, unsigned int bit, bool on) { flags = on ? flags | bit : flags & ~bit; }
}  // namespace
// 800B6B80
void FamilyImpl::SetHasPhone(bool on) { SetFlag(m_flags, kHasPhone, on); }
// 800B6BA0
unsigned int FamilyImpl::GetHasPhone() { return m_flags & 1; }
// 800B6BAC
void FamilyImpl::SetHasBaby(bool on) { SetFlag(m_flags, kHasBaby, on); }
// 800B6BCC
unsigned int FamilyImpl::GetHasBaby() { return m_flags >> 1 & 1; }
// 800B6BD8
void FamilyImpl::SetJustMovedIn(bool on) { SetFlag(m_flags, kJustMovedIn, on); }
// 800B6BF8
unsigned int FamilyImpl::GetJustMovedIn() { return m_flags >> 2 & 1; }
// 800B6C04
void FamilyImpl::SetIsFreeplay(bool on) { SetFlag(m_flags, kFreeplay, on); }
// 800B6C24
unsigned int FamilyImpl::GetIsFreeplay() { return m_flags >> 3 & 1; }
// 800B6C30
void FamilyImpl::SetHasP1DoneCAS(bool on) { SetFlag(m_flags, kP1DoneCAS, on); }
// 800B6C50
unsigned int FamilyImpl::GetHasP1DoneCAS() { return m_flags >> 4 & 1; }
// 800B6C5C
void FamilyImpl::SetHasP2DoneCAS(bool on) { SetFlag(m_flags, kP2DoneCAS, on); }
// 800B6C7C
unsigned int FamilyImpl::GetHasP2DoneCAS() { return m_flags >> 5 & 1; }
// 800B6C88
RET_UNKNOWN FamilyImpl::GetInventory() { return 0; }
// 800B6C90
RET_UNKNOWN FamilyImpl::GetGoalAndUnlockData() { return 0; }
// 800B6C98
int FamilyImpl::GetStatValue(int) const { return 0; }
// 800B6CA0
int FamilyImpl::GetStatRef(int, short**) { return 0; }

// ---------------------------------------------------------------------------------------------------
// PlayerFamilyImpl

// 8012CF68
// GC: the clothing unlocks start at 2 (CasClothingUnlocks::ClearUnlocks, then UnlockGroup(0), not
// ported).
PlayerFamilyImpl::PlayerFamilyImpl(int number) : FamilyImpl(number) {
  std::memset(m_clothingUnlocks, 2, sizeof m_clothingUnlocks);
  std::memset(m_tattooUnlocks, 2, sizeof m_tattooUnlocks);
}

// 8012CFD0
PlayerFamilyImpl::~PlayerFamilyImpl() {}

// 8012D038
void PlayerFamilyImpl::ClearFamily() {
  FamilyImpl::ClearFamily();
  std::memset(m_goals, 0, sizeof m_goals);  // GoalUnlock::ClearGoalsAndUnlocks
  for (bool& h : m_haveItem) h = false;     // BBI::InventoryItems::Clear
  for (InventoryItem& i : m_items) i = InventoryItem();
  std::memset(m_clothingUnlocks, 2, sizeof m_clothingUnlocks);  // CasClothingUnlocks::ClearUnlocks
  std::memset(m_tattooUnlocks, 2, sizeof m_tattooUnlocks);
  std::memset(m_stats, 0, sizeof m_stats);
  m_totalAspirationScore = 0;
}

// 8012D0A8
// The family, then:
//  - the inventory (BBI::InventoryItems::DoStream, 8004AF14): the tag 'INV\0', then per slot (36) a
//    "held" byte and the item (InventoryItem::DoStream, 8004A144: four bytes and a word); streams
//    before version 10 held a GUID per slot instead (not ported);
//  - the goals and unlocks (GoalUnlock::DoStream, 800B84A8): 0x40 shorts (0x27 before 0x11);
//  - the clothing unlocks (CasClothingUnlocks::DoStream, 8015F0D4): a byte per gender, part and style
//    (2 x 32 x 40), then per gender 8 x 30 tattoo bytes (from 0x10; cleared before);
//  - 0x19 stats, and from 0x12 the total aspiration score.
void PlayerFamilyImpl::DoStream(ReconBuffer* b, int version) {
  const bool read = b->m_mode == ReconBuffer__Mode::kRead;
  FamilyImpl::DoStream(b, version);
  int tag = 0x494e5600;
  b->Recon32(&tag, 1);
  if (read && version < 10) {
    SIMMY_STUB("PlayerFamilyImpl::DoStream: an inventory before version 10", 0x8004ADDC);
  } else {
    if (read)
      for (bool& h : m_haveItem) h = false;
    for (int i = 0; i < 36; ++i) {
      signed char held = m_haveItem[i] ? 1 : 0;
      b->Recon8(&held, 1);
      if (read) m_haveItem[i] = held != 0;
      if (!held) continue;
      b->Recon8(m_items[i].m_bytes, 4);
      b->Recon32(&m_items[i].m_word, 1);
    }
  }
  if (version < 0x11) {
    b->Recon16(m_goals, 0x27);
    SIMMY_STUB("PlayerFamilyImpl::DoStream: GoalUnlock's old-version fix-up", 0x800B84A8);
  } else {
    b->Recon16(m_goals, 0x40);
  }
  if (version < 0x10) {
    if (read) {
      std::memset(m_clothingUnlocks, 2, sizeof m_clothingUnlocks);
      std::memset(m_tattooUnlocks, 2, sizeof m_tattooUnlocks);
    }
  } else {
    for (auto& g : m_clothingUnlocks) b->Recon8(g, 0x500);
    for (auto& g : m_tattooUnlocks) b->Recon8(g, 0xf0);
  }
  b->Recon16(m_stats, 0x19);
  if (version < 0x12) m_totalAspirationScore = 0;
  else b->ReconInt(&m_totalAspirationScore, 1);
}

// 8012D140
int PlayerFamilyImpl::GetStatValue(int index) const {
  return static_cast<unsigned int>(index) < 0x19 ? m_stats[index] : 0;
}
// 8012D160
int PlayerFamilyImpl::GetStatRef(int index, short** ref) {
  if (!ref) return 0;
  if (static_cast<unsigned int>(index) < 0x19) {
    *ref = &m_stats[index];
    return 1;
  }
  *ref = nullptr;
  return 0;
}

// 8012D1A4
short PlayerFamilyImpl::GetResfileStorageId(short number) { return static_cast<short>(number | 0x4000); }
