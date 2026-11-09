// Port of game/neighbor.cpp (Neighbor: its stream, the CAS hand-over to and from
// ENeighborhoodCustomChar, default values, interests) and game/relmatrix.cpp (the relationships).
// The wants and fears are in wantfear.cpp. Not ported: notifications, the portraits (ObjSelector
// thumbnails).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace {
// Interests::sInterestMap (804240F0): the person data slots of the 14 interests.
const int kInterestMap[14] = {0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x4c, 0x4d, 0x4e, 0x4f};
// Interests::kInterestCutoffs (80500614): the top of each of the four interest levels.
const short kInterestCutoffs[4] = {299, 499, 799, 1000};
}  // namespace

// ---------------------------------------------------------------------------------------------------
// RelMatrix

// 80134A60
RelMatrixImpl* RelMatrix::CreateInstance() { return new RelMatrixImpl; }
// 80134AA8
void RelMatrix::DestroyInstance(RelMatrix* m) { delete m; }

// 8013550C
RelMatrixImpl::RelMatrixImpl() {}
// 80135438
RelMatrixImpl::~RelMatrixImpl() { Clear(); }

// 80135384
void RelMatrixImpl::Clear() {
  while (!m_arrays.empty()) {
    delete m_arrays.back();
    m_arrays.pop_back();
  }
}

// 80134C44
void RelMatrixImpl::RemoveArray(int key) {
  for (auto it = m_arrays.begin(); it != m_arrays.end(); ++it) {
    if ((*it)->m_key != key) continue;
    delete *it;
    m_arrays.erase(it);
    return;
  }
}

// 80134F8C
RelArray* RelMatrixImpl::FindArray(int key) {
  for (RelArray* a : m_arrays)
    if (a->m_key == key) return a;
  return nullptr;
}

// 801354C0
int RelMatrixImpl::CountKeys() { return static_cast<int>(m_arrays.size()); }
// 801354D4
int RelMatrixImpl::GetNthKey(int index) {
  return index >= 0 && index < static_cast<int>(m_arrays.size()) ? m_arrays[static_cast<size_t>(index)]->m_key : 0;
}

// 801350D4
// Written: -1 (the current format; before 0x54 a 0 after it), then the rows as a pointer vector. Old
// streams hold the row count and the rows.
void RelMatrixImpl::DoStream(ReconBuffer* b, int version) {
  int count = -1;
  b->Recon32(&count, 1);
  if (b->m_mode != ReconBuffer__Mode::kRead && version < 0x54) {
    int zero = 0;
    b->Recon32(&zero, 1);
  }
  if (count < 0) {
    if (b->m_mode == ReconBuffer__Mode::kRead && version > 0x4e && version < 0x54) {
      int skipped = 0;
      b->Recon32(&skipped, 1);
    }
    DoPtrVectorStream(m_arrays, b, version);
    return;
  }
  Clear();
  for (int i = 0; i < count; ++i) {
    auto* a = new RelArray;
    a->DoStream(b, version);
    m_arrays.push_back(a);
  }
}

// 8013532C
// The key (from version 1), then the values (DoContainerStream<RelArray, RelInt>: count, values).
void RelArray::DoStream(ReconBuffer* b, int version) {
  if (version > 0) b->Recon32(&m_key, 1);
  int n = static_cast<int>(m_values.size());
  b->Recon32(&n, 1);
  m_values.resize(static_cast<size_t>(n > 0 ? n : 0));
  for (int& v : m_values) b->Recon32(&v, 1);  // RelInt::DoStream
}

// 80134BF0
int RelMatrixImpl::GetArraySize(int key) {
  RelArray* a = FindArray(key);
  return a ? static_cast<int>(a->m_values.size()) : 0;
}

// 80134E14
void RelMatrixImpl::CreateNewArray(int key) {
  auto* a = new RelArray;
  a->m_key = key;
  m_arrays.push_back(a);
}

// 80134D30
void RelMatrixImpl::SetArraySize(int key, int size) {
  RelArray* a = FindArray(key);
  if (!a) {
    CreateNewArray(key);
    a = FindArray(key);
    if (!a) return;
  }
  a->m_values.resize(static_cast<size_t>(size > 0 ? size : 0), 0);
}

// 80134FCC
// GC: no check (the callers ask GetArraySize first).
int RelMatrixImpl::GetValue(int key, int index) {
  RelArray* a = FindArray(key);
  return a && index >= 0 && index < static_cast<int>(a->m_values.size()) ? a->m_values[static_cast<size_t>(index)] : 0;
}

// 80135008
void RelMatrixImpl::SetValue(int key, int index, int value) {
  RelArray* a = FindArray(key);
  if (!a || index >= static_cast<int>(a->m_values.size())) {
    SetArraySize(key, index + 1);
    a = FindArray(key);
  }
  if (a && index >= 0 && index < static_cast<int>(a->m_values.size())) a->m_values[static_cast<size_t>(index)] = value;
}

// ---------------------------------------------------------------------------------------------------
// Relationship levels (a relationship's values: 0 the daily value, 2 the memories (3 bits each:
// GetIndividualMemory 8013FCC8), 3 flags (0x10 married))

namespace {
int sRelationshipLevelMin[10], sRelationshipLevelMax[10];  // 8048628C, 804862B4
int sCrushMemoryThreshold = 0, sLoverMemoryThreshold = 0;
bool sLevelsGathered = false;

// GatherRelationshipLevelValues (800C8B14): the global constants 0x132 (pairs of min / max from index 0,
// level k from pair k - 1: 2 enemy, 3 dislike, 4 neutral, 6 friend, 7 crush, 8 best friend, 9 lover;
// 18 / 19 the crush and love memory thresholds), then levels 7 and 8 swapped.
void GatherLevels() {
  const unsigned char* values = nullptr;
  int count = 0;
  Behavior* b = nullptr;
  if (Globs_pObjectFolder)
    if (ObjSelector* sel = Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(0x9e12bf3cu))) b = Behavior::For(sel);
  if (!b || !b->GetConstants(0x132, &values, &count)) return;
  auto at = [&](int i) { return i >= 0 && i < count ? static_cast<short>(simmy::be16(values + i * 8)) : 0; };
  for (int k = 0; k < 10; ++k) {
    sRelationshipLevelMin[k] = at(2 * k - 2);
    sRelationshipLevelMax[k] = at(2 * k - 1);
  }
  sCrushMemoryThreshold = at(0x12);
  sLoverMemoryThreshold = at(0x13);
  std::swap(sRelationshipLevelMin[7], sRelationshipLevelMin[8]);
  std::swap(sRelationshipLevelMax[7], sRelationshipLevelMax[8]);
  sLevelsGathered = true;
}

// GetIndividualMemory (8013FCC8)
int IndividualMemory(int memories, int which) { return memories >> (which * 3 & 0x3f) & 7; }
}  // namespace

// 800C9E30
bool Neighbor::RelCountsAsRel(int level, int type) {
  switch (type) {
    case 2: return level == 2;
    case 3: return static_cast<unsigned int>(level - 2) < 2u;
    case 4: return level == 4;
    case 6: return static_cast<unsigned int>(level - 6) < 4u;
    case 7: return level == 7;
    case 8: return level == 8;
    case 9: return level == 9;
    default: return false;
  }
}

// 800C9F10
// Lover (9) in the lover range with enough love memories, crush (8) likewise, else the first of 2, 3,
// 4, 6, 7 whose range holds the value, else neutral (4).
int Neighbor::CalculateRelationshipLevel(int value, int memory) {
  if (!sLevelsGathered) GatherLevels();
  if (value >= sRelationshipLevelMin[9] && value <= sRelationshipLevelMax[9] && IndividualMemory(memory, 5) >= sLoverMemoryThreshold)
    return 9;
  if (value >= sRelationshipLevelMin[8] && value <= sRelationshipLevelMax[8] && IndividualMemory(memory, 6) >= sCrushMemoryThreshold)
    return 8;
  for (int level = 2; level < 8; ++level)
    if (level != 5 && sRelationshipLevelMin[level] <= value && value <= sRelationshipLevelMax[level]) return level;
  return 4;
}

// 800CA024
int Neighbor::CalculateRelationshipLevel(Neighbor* other) {
  RelMatrixImpl* m = GetRelations();
  const int size = m->GetArraySize(other->m_id);
  const int memory = size > 2 ? m->GetValue(other->m_id, 2) : 0;
  return size > 0 ? CalculateRelationshipLevel(m->GetValue(other->m_id, 0), memory) : 4;
}

// 800CA0E8
int Neighbor::CountRelationshipsOfType(int type) {
  RelMatrixImpl* m = GetRelations();
  int n = 0;
  for (int i = 0; i < m->CountKeys(); ++i) {
    const int key = m->GetNthKey(i);
    const int size = m->GetArraySize(key);
    const int memory = size > 2 ? m->GetValue(key, 2) : 0;
    if (size > 0 && RelCountsAsRel(CalculateRelationshipLevel(m->GetValue(key, 0), memory), type)) ++n;
  }
  return n;
}

// 800CA2BC
bool Neighbor::IsMarriedTo(Neighbor* other) const {
  return other && m_relations->GetArraySize(other->m_id) > 3 && (m_relations->GetValue(other->m_id, 3) & 0x10);
}

// 800CA348
int Neighbor::GetRelationship(Neighbor* other, int* value) {
  if (!other) {
    if (value) *value = 0;
    return 0;
  }
  return GetRelationshipById(other->m_id, value);
}

// 800CA388
int Neighbor::GetRelationshipById(int id, int* value) {
  if (m_relations->GetArraySize(id) == 0) {
    if (value) *value = 0;
    return 0;
  }
  if (value) *value = m_relations->GetValue(id, 0);
  return 1;
}

// 800CA424
void Neighbor::AdjustRelationship(Neighbor* other, int value) {
  if (!other) return;
  if (m_relations->GetArraySize(other->m_id) == 0) m_relations->SetArraySize(other->m_id, 1);
  m_relations->SetValue(other->m_id, 0, value);
}

// 800C9BA8
// The friend / enemy counts and the reputation (mean |relationship| x10 over the met characters not
// in family 0x13, max 999), recomputed when the relationships changed. The players' selected sims
// show it (m_shownReputation; GC: only while the player has the plumb bob).
void Neighbor::RecalculateRelationshipData() {
  if (m_relationsDirty == 0) return;
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  unsigned int count = 0;
  int sum = 0;
  for (int id = hood->GetNextNeighborID(0); id != 0; id = hood->GetNextNeighborID(static_cast<short>(id))) {
    Neighbor* n = hood->FindNeighborByID(id);
    if (!n || n == this || !n->IsCharacter() || n->m_pers[0x3d] == 0) continue;
    FamilyImpl* f13 = hood->GetFamily(0x13);
    if (f13 && f13->TestMember(n->GetGUID())) continue;
    ++count;
    RelMatrixImpl* rel = GetRelations();
    if (rel->GetArraySize(n->m_id) != 0) sum += rel->GetValue(n->m_id, 0);
  }
  m_friendCount = static_cast<short>(CountRelationshipsOfType(6));
  m_enemyCount = static_cast<short>(CountRelationshipsOfType(2));
  m_reputation = static_cast<short>(count ? (std::abs(sum) * 10) / static_cast<int>(count) : 0);
  if (m_reputation > 999) m_reputation = 999;
  for (int player = 0; player < kMaxPlayers; ++player) {  // PC mod: 4 players (GC: 2)
    cXPerson* p = _globals.GetSelectedPerson(player);
    cXObject* o = p ? p->AsObject() : nullptr;
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    if (impl && impl->m_selector && impl->m_selector->GetGUID() == static_cast<unsigned int>(m_guid))
      m_shownReputation = m_reputation;
  }
  m_relationsDirty = 0;
}

// 800CA208
int Neighbor::GetFriendCount() {
  if (m_relationsDirty) RecalculateRelationshipData();
  return m_friendCount;
}
// 800CA244
int Neighbor::GetEnemyCount() {
  if (m_relationsDirty) RecalculateRelationshipData();
  return m_enemyCount;
}
// 800CA280
int Neighbor::GetReputationValue() {
  if (m_relationsDirty) RecalculateRelationshipData();
  return m_reputation;
}

// 80044BCC HoodManager::GetRelatedPeople (with RelationsCmp 80044930)
// The neighbours (characters, met: person data 0x3D) `person`'s neighbour has a relationship with;
// family first. PC: then by the relationship's value (RelationsCmp also orders by the people on the
// lot, not ported).
std::vector<Neighbor*> RelatedPeopleOf(cXPerson* person) {
  std::vector<Neighbor*> out;
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  Neighbor* me = PersonNeighbor(person);
  if (!hood || !me) return out;
  for (int id = hood->GetNextNeighborID(0); id != 0; id = hood->GetNextNeighborID(static_cast<short>(id))) {
    Neighbor* n = hood->FindNeighborByID(id);
    if (!n || n == me || !n->IsCharacter() || n->m_pers[0x3d] == 0) continue;
    if (me->GetRelations()->GetArraySize(n->m_id) <= 0) continue;
    out.push_back(n);
  }
  FamilyImpl* family = me->GetFamily();
  std::stable_sort(out.begin(), out.end(), [&](Neighbor* a, Neighbor* b) {
    const bool fa = family && a->GetFamily() == family, fb = family && b->GetFamily() == family;
    if (fa != fb) return fa;
    int va = 0, vb = 0;
    me->GetRelationship(a, &va);
    me->GetRelationship(b, &vb);
    return va > vb;
  });
  return out;
}

// ---------------------------------------------------------------------------------------------------
// Interests

// 800C88C0
// Random interests: 3 low (0..299), 3 lower middle (300..499), 5 upper middle (500..799) and 3 high
// (800..1000) values, dealt at random to the 14 interest slots.
void Interests::SetInterests(short* pers) {
  int map[14];
  std::memcpy(map, kInterestMap, sizeof map);
  const int counts[4] = {3, 3, 5, 3};
  short values[14];
  int k = 0;
  for (int level = 0; level < 4; ++level) {
    const short lo = level < 1 ? 0 : static_cast<short>(kInterestCutoffs[level - 1] + 1);
    const unsigned int range = level < 1 ? static_cast<unsigned int>(kInterestCutoffs[0] + 1)
                                         : static_cast<unsigned int>(kInterestCutoffs[level] - kInterestCutoffs[level - 1]);
    for (int i = 0; i < counts[level]; ++i)
      values[k++] = static_cast<short>(static_cast<short>(GetNextRandomNumber() % range) + lo);
  }
  unsigned int left = 14;
  for (int i = 0; i < 14; ++i) {
    const unsigned int j = static_cast<unsigned int>(GetNextRandomNumber()) % left;
    pers[map[j]] = values[i];
    if (j < left - 1) map[j] = map[left - 1];
    --left;
  }
}

// ---------------------------------------------------------------------------------------------------
// Neighbor

// 800C8C48
// GC: Motives::Init twice (the motives zeroed) and the bookmarks cleared.
Neighbor::Neighbor() {
  m_relationsDirty = 1;
  m_relations = RelMatrix::CreateInstance();
}

// 800C8D58
// A neighbour for the object `selector`: its GUID and file name. (GC: calloc'd, so +0x24 is 0.)
Neighbor::Neighbor(short id, ObjSelector* selector) {
  m_id = id;
  m_selector = selector;
  m_relationsDirty = 0;
  m_guid = selector ? static_cast<int>(selector->GetGUID()) : 0;
  m_relations = RelMatrix::CreateInstance();
  if (selector) selector->GetShortFilename(m_name, sizeof m_name);
}

// 800C8D00
Neighbor::~Neighbor() { RelMatrix::DestroyInstance(m_relations); }

// 800C90B4
RelMatrixImpl* Neighbor::GetRelations() const { return m_relations; }
namespace {
std::vector<Neighbor::PersDataPair> sPersistentFields;  // 80486278
}  // namespace

// 800C926C
// The fields: personality 2..7 (not 8), skills 9..0x12, interests 0x2E..0x37 and 0x4C..0x4F, 0x3A, 0x3C
// and the family 0x3D (version 1); 0x38, 0x39, 0x3F (2); 0x41 (3); 0x43, 0x44 (4); 0x45 (5); 0x4A (6);
// the star sign 0x46 (7); the aspiration 0x14 (8).
int Neighbor::GetNumPersistentDataFields() {
  if (sPersistentFields.empty()) {
    sPersistentFields.reserve(0x2b);
    static const PersDataPair kFields[] = {
        {2, 1},    {3, 1},    {4, 1},    {5, 1},    {6, 1},    {7, 1},    {9, 1},    {10, 1},   {0xb, 1},
        {0xc, 1},  {0xd, 1},  {0xe, 1},  {0xf, 1},  {0x10, 1}, {0x11, 1}, {0x12, 1}, {0x2e, 1}, {0x2f, 1},
        {0x30, 1}, {0x31, 1}, {0x32, 1}, {0x33, 1}, {0x34, 1}, {0x35, 1}, {0x36, 1}, {0x37, 1}, {0x4c, 1},
        {0x4d, 1}, {0x4e, 1}, {0x4f, 1}, {0x3a, 1}, {0x3c, 1}, {0x3d, 1}, {0x38, 2}, {0x39, 2}, {0x3f, 2},
        {0x41, 3}, {0x43, 4}, {0x44, 4}, {0x45, 5}, {0x4a, 6}, {0x46, 7}, {0x14, 8}};
    sPersistentFields.assign(std::begin(kFields), std::end(kFields));  // AddPersDataPair each
  }
  return static_cast<int>(sPersistentFields.size());
}

// 800C9780
const Neighbor::PersDataPair* Neighbor::GetPersistentDataFieldsByIndex(int index) {
  return &sPersistentFields[static_cast<size_t>(index)];
}

// 800C9794
int Neighbor::GetLatestPersDataVersion() { return 8; }
// 800C9A8C
int Neighbor::GetGUID() { return m_guid; }
// 800C9A94
void Neighbor::SetGUID(int guid) { m_guid = guid; }

int Neighbor::ActiveSlotOf(int b) const {
  for (int s = 0; s < 7; ++s)
    if (m_active[s] == b) return s;
  return -1;
}

// 800C979C
// From version 0x1E a stream version (4); the file name, +0x14, the person data (when it has any),
// the motives, the id and GUID (read back: the object found by it, its file name when the stream had
// none), the relationships, +0x1B4 (0x5F on) and the 20 wants/fears with the 7 active ones (0x5E
// on; older neighbours get new ones).
void Neighbor::DoStream(ReconBuffer* b, int version) {
  const bool read = b->m_mode == ReconBuffer__Mode::kRead;
  int streamVersion = 0;
  if (version >= 0x1e) {
    streamVersion = 4;
    b->Recon32(&streamVersion, 1);
  }
  b->ReconString(m_name, sizeof m_name);
  b->ReconInt(&m_field14, 1);
  b->ReconInt(&m_persVersion, 1);
  if (m_persVersion != 0) b->Recon16(m_pers, 0x50);
  b->ReconFloat(m_motives, 0x10);
  b->ReconFloat(m_motives2, 0x10);
  b->Recon16(&m_id, 1);
  b->Recon32(&m_guid, 1);
  if (read) m_selector = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(m_guid) : nullptr;
  if (streamVersion < 2 && m_selector && !m_name[0]) m_selector->GetShortFilename(m_name, sizeof m_name);
  m_relations->DoStream(b, version);
  if (version < 0x5f) m_field1B4 = 0;
  else b->Recon32(&m_field1B4, 1);
  if (version < 0x5e) {
    InitWantsFears();
    return;
  }
  int active[7] = {-1, -1, -1, -1, -1, -1, -1};
  for (int i = 0; i < 20; ++i) {
    Bookmark& bm = m_bookmarks[i];  // WantFear::Bookmark::DoStream (8014BB4C)
    b->Recon16(reinterpret_cast<short*>(&bm.m_tree), 1);
    b->Recon16(&bm.m_field02, 1);
    if (version < 0x60) bm.m_field08 = 0;
    else b->Recon16(reinterpret_cast<short*>(&bm.m_field08), 1);
    b->Recon16(&bm.m_node, 1);  // PC: the node index as streamed (GC: resolved in the tree, or Clear)
    if (b->m_mode == ReconBuffer__Mode::kWrite) {
      const int slot = ActiveSlotOf(i);
      if (slot >= 0) active[slot] = i;
    }
  }
  b->ReconInt(active, 7);
  if (!read) return;
  for (int s = 0; s < 7; ++s) m_active[s] = active[s] >= 0 && active[s] < 20 ? active[s] : -1;
  for (int s = 0; s < 7; ++s)
    if (m_active[s] < 0 || m_bookmarks[m_active[s]].Empty()) MakeNewActiveBookmark(static_cast<unsigned int>(s));
}

// 800C9A9C
// The family that has this neighbour as a member.
FamilyImpl* Neighbor::GetFamily() {
  if (!Globs_pNeighborhood) return nullptr;
  const int n = Globs_pNeighborhood->GetNumFamilies();
  for (int i = 0; i < n; ++i) {
    FamilyImpl* f = Globs_pNeighborhood->GetFamilyByIndex(i);
    if (f && f->TestMember(m_guid)) return f;
  }
  return nullptr;
}

// 800C9B50
// A CAS-made (or story) sim: its object has a look of its own.
bool Neighbor::IsCharacter() { return m_selector && m_selector->GetSimDescription(); }

// 800CA4D0
void Neighbor::RemoveAllRelationships() { m_relations->Clear(); }

// 800CA604
// A sim made in CAS into this neighbour: personality, star sign, aspiration, skills and the rest of
// the person data the slot carries, and the look and the portrait (its thumbnail) into its object.
void Neighbor::LoadFromCharacter(ENeighborhoodCustomChar& c) {
  m_pers[2] = c.m_nice;
  m_pers[3] = c.m_active;
  m_pers[4] = c.m_field04;
  m_pers[5] = c.m_playful;
  m_pers[6] = c.m_outgoing;
  m_pers[7] = c.m_neat;
  m_pers[0x46] = ComputeZodiacSign(m_pers);
  m_pers[0x14] = c.m_aspiration;
  for (int k = 0; k < 10; ++k) m_pers[9 + k] = static_cast<short>(c.m_field13C[1 + k]);
  m_pers[0x18] = static_cast<short>(c.m_field13C[11]);
  m_pers[0x38] = static_cast<short>(c.m_field13C[12]);
  m_pers[0x39] = static_cast<short>(c.m_field13C[13]);
  m_pers[0x3f] = static_cast<short>(c.m_field174);
  m_pers[0x3a] = static_cast<short>(c.m_field10);
  m_persVersion = GetLatestPersDataVersion();
  if (m_selector) {
    if (!m_selector->m_simDesc) m_selector->m_simDesc = new CasSimDescriptionS2C;  // GC: always has one
    *m_selector->m_simDesc = c.m_desc;
  }
  // PC: a slot without a portrait (none was made) leaves the old one (GC: set either way).
  if (m_selector && c.m_faceImage) m_selector->SetThumbnail(static_cast<ETexture*>(c.m_faceImage));
}

// 800CA758
// The reverse, for CAS to edit this neighbour: the names (then overwritten by the look copied after
// them, as on GC) and the look come from its object. GC: also the portrait from its thumbnail.
void Neighbor::SaveToCharacter(ENeighborhoodCustomChar& c) {
  c.m_nice = m_pers[2];
  c.m_active = m_pers[3];
  c.m_field04 = m_pers[4];
  c.m_playful = m_pers[5];
  c.m_outgoing = m_pers[6];
  c.m_neat = m_pers[7];
  c.m_aspiration = static_cast<unsigned char>(m_pers[0x14]);
  for (int k = 0; k < 10; ++k) c.m_field13C[1 + k] = m_pers[9 + k];
  c.m_field13C[11] = m_pers[0x18];
  c.m_field13C[12] = m_pers[0x38];
  c.m_field13C[13] = m_pers[0x39];
  c.m_field174 = m_pers[0x3f];
  c.m_field10 = m_pers[0x3a];
  if (m_selector) {
    const std::u16string& first = m_selector->GetUserName();
    const std::u16string& last = m_selector->GetUserLastName();
    for (int i = 0; i < 0x20; ++i) c.m_desc.m_firstName[i] = i < static_cast<int>(first.size()) && i < 0x1f ? first[i] : 0;
    for (int i = 0; i < 0x20; ++i) c.m_desc.m_lastName[i] = i < static_cast<int>(last.size()) && i < 0x1f ? last[i] : 0;
    if (CasSimDescriptionS2C* d = m_selector->m_simDesc) c.m_desc = *d;
  }
  SIMMY_STUB("Neighbor::SaveToCharacter: the portrait (ObjSelector::GetThumbnail)", 0x800CA758);
  c.m_faceImage = nullptr;
}

// 800CA924
double Neighbor::GetAspirationScore() { return ((m_motives[0] + 100.0f) / 200.0f) * 1000.0f; }
// 800CA950
void Neighbor::SetAspirationScore(float score) { m_motives[0] = (score / 1000.0f) * 200.0f - 100.0f; }

// 800CA978
void Neighbor::ResetInterests() { Interests::SetInterests(m_pers); }

// 800CA99C
// A sim about to be made in CAS: new interests, skills and the like cleared, the aspiration scores at
// -100, no relationships; person data flags 0x60.
void Neighbor::InitDefaultValues() {
  ResetInterests();
  m_pers[0x3f] = 0;
  m_motives2[0] = -100.0f;
  m_motives[0] = -100.0f;
  for (int i = 9; i <= 0x12; ++i) m_pers[i] = 0;
  m_pers[0x18] = 0;
  m_pers[0x38] = 0;
  m_pers[0x39] = 0;
  if (Globs_pNeighborhood) Globs_pNeighborhood->RemoveNeighborRelationships(this);
  m_pers[0x4a] = static_cast<short>(m_pers[0x4a] | 0x60);
}

// Neighbor::InitWantsFears and the other wants and fears: wantfear.cpp
