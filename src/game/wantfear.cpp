// Port of game/wantfear.cpp (WantFearManager, WantFear::Bookmark / Tree / Node / Event) and the wants
// and fears of game/neighbor.cpp (InitWantsFears, the bookmark lottery, ads, active slots, the HUD
// icons), notifications (Neighbor::Notify, SatisfiedBy, ApplyReward, NeighborhoodImpl::NotifyAll*) and
// forced trees (ForceNewTree, GetStoryTreeRoot). See wantfear.hpp for the data. Not ported: the dialog.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern float kAspirationPointToMeterRatio;  // treesim.cpp (motive.cpp)

namespace {
const short kNoActiveIndex = -1;  // WantFear::kNoActiveIndex 8050063A
const short kNoTarget = 0;        // WantFear::kNoTarget 8050063E
const int kMaxAdMax = 0x7f;       // WantFear::kMaxAdMax 80500640
const unsigned int kWantsObjectGUID = 0x9e12bf3cu;
const int kTotalAspirationMax = 999999;  // 804FDxxx kTotalAspirationMax (playerfamilyimpl.cpp)
const short kEventBuildBuyCategory = 0x2ffb;         // WantFear::kEventBuildBuyCategory 80500644
const short kEventBuildBuyWorth = static_cast<short>(0x9fd5);  // WantFear::kEventBuildBuyWorth 80500646
const short kEventBuildBuyWorthPlatinum = 0x4c73;    // WantFear::kEventBuildBuyWorthPlatinum 8050064C
const short kEventBuyXLights = static_cast<short>(0x96c8);     // WantFear::kEventBuyXLights 80500652
const short kEventBuyXChairs = static_cast<short>(0xb53f);     // WantFear::kEventBuyXChairs 80500654

inline unsigned int U32(const unsigned char* p) { return simmy::be32(p); }
inline unsigned short U16(const unsigned char* p) { return simmy::be16(p); }
inline short S16(const unsigned char* p) { return static_cast<short>(simmy::be16(p)); }
inline signed char S8(const unsigned char* p) { return static_cast<signed char>(*p); }

// The wants object on the lot (ObjectModule vtable 0xE4: by GUID).
cXObjectImpl* WantsObject() {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module) return nullptr;
  for (int id = 1; id <= module->GetNumObjects(); ++id) {
    cXObject* x = module->GetObjectFromID(id);
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    if (o && o->m_def && objdef::GUID(o->m_def) == kWantsObjectGUID) return o;
  }
  return nullptr;
}

// The wants object's check tree `tree` with {neighbour id, story, target, count}: 1 passes.
int RunWantCheckTree(short tree, Neighbor* who, const WantFear__Bookmark& b) {
  cXObjectImpl* o = WantsObject();
  if (!o || !o->m_treeSim) return -1;
  short params[4] = {who ? who->m_id : static_cast<short>(0), static_cast<short>(b.IsInStoryTree() ? 1 : 0),
                     static_cast<short>(b.GetTarget()), S16(b.Node() + 8)};
  return o->m_treeSim->RunCheckTree(o->GetBehavior(), tree, 0, params);
}

void Replace(std::u16string& s, const std::u16string& key, const std::u16string& with) {
  for (size_t at = s.find(key); at != std::u16string::npos; at = s.find(key, at + with.size())) s.replace(at, key.size(), with);
}
std::u16string Ascii(const char* a) {
  std::u16string s;
  for (; *a; ++a) s += static_cast<char16_t>(*a);
  return s;
}

// WantFear::Event::GetEventText (8014A8FC): line `index` (1-based) of the event's string set (+4) in
// the wants object's file (ResFile +0x14: STR# sets {set, type, id}). False when the set has fewer.
bool GetEventText(const unsigned char* event, int index, std::u16string& out) {
  out.clear();
  const short set = S16(event + 4);
  if (set == 0) return false;
  ObjSelector* sel = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(kWantsObjectGUID)) : nullptr;
  const unsigned char* file = sel ? SelectorResFile(sel) : nullptr;
  if (!file) {
    out = Ascii("<No Want Fears Object>");
    return false;
  }
  const auto* sets = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x14));
  const unsigned int n = sets ? ERQuickdata::U32(sets - 4, 0) : 0;
  for (unsigned int i = 0; i < n; ++i) {
    const unsigned char* rec = sets + i * 12;
    if (ERQuickdata::S16(rec, 8) != set) continue;
    const auto* strings = static_cast<const unsigned char*>(ERQuickdata::Ptr(rec, 0));
    const unsigned int count = strings ? ERQuickdata::U32(strings - 4, 0) : 0;
    if (index < 1 || static_cast<unsigned int>(index) > count) return false;
    const unsigned short* text = ERQuickdata::LocString(strings, static_cast<unsigned int>(index - 1) * 4);
    for (; text && *text; ++text) out += static_cast<char16_t>(*text);
    return true;
  }
  return false;
}

// EGlobal::GetWantFearCategoryString (8003FE38): the UI quickdata's "WantCategory" table ({type, list}
// rows; the list {u16 id, pad, the localised name +4}, 12 bytes each) for target `id` of type `type`.
std::u16string CategoryString(int type, int id) {
  std::u16string s;
  ERQuickdata* qd = _globals.m_strings;
  const void* table = qd ? qd->getTable(const_cast<char*>("WantCategory")) : nullptr;
  if (!table) return s;
  for (unsigned int i = 0; i < ERQuickdata::TableRowCount(table); ++i) {
    const void* row = ERQuickdata::TableRow(table, i);
    if (ERQuickdata::S32(row, 0) != type) continue;
    const auto* list = static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 4));
    const unsigned int n = list ? ERQuickdata::U32(list - 4, 0) : 0;
    for (unsigned int k = 0; k < n; ++k)
      if (ERQuickdata::U16(list + k * 12, 0) == static_cast<unsigned short>(id)) {
        for (const unsigned short* t = ERQuickdata::LocString(list + k * 12, 4); t && *t; ++t) s += static_cast<char16_t>(*t);
        return s;
      }
    return s;
  }
  return s;
}

std::u16string NeighborName(Neighbor* n) {
  std::u16string s;
  if (n && n->m_selector)
    for (char16_t c : n->m_selector->GetUserName()) s += c;
  return s;
}
}  // namespace

WantFearManager* TheWantFearManager() {
  static WantFearManager* m = nullptr;
  if (!m) m = new WantFearManager;
  return m;
}

cXPersonImpl* NeighborPerson(Neighbor* n) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!n || !module) return nullptr;
  for (int id = 1; id <= module->GetNumObjects(); ++id) {
    cXObject* x = module->GetObjectFromID(id);
    auto* p = x ? dynamic_cast<cXPersonImpl*>(x->GetObjectImplementation()) : nullptr;
    if (p && p->m_neighbor == n) return p;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------------------------------
// WantFearManager

// 8014BC6C
WantFearManager::WantFearManager() { LoadWants(); }
// 8014BCB8
WantFearManager::~WantFearManager() {
  if (m_res) m_res->DelRef();
}

// 8014BD08
// The binary 33B78948; versions 1 and 2 have shorter headers (GC: copied into a version-3 one), PC:
// version 3 only (the disc's). PC: offsets are resolved on access, the data is not patched.
bool WantFearManager::LoadWants() {
  if (m_res) m_res->DelRef();
  m_res = _binaryman.EResourceManager::AddRef(0x33b78948u, nullptr, 0);
  if (!m_res) return false;
  const auto* base = static_cast<const unsigned char*>(static_cast<ERBinary*>(m_res)->m_data);
  if (!base || U32(base) != 3) {
    SIMMY_STUB("WantFearManager::LoadWants: want data versions 1 and 2", 0x8014BD08);
    return false;
  }
  m_header = base;
  m_events = base + U32(base + 0x14);
  m_trees = base + U32(base + 0x18);
  m_nodes = base + U32(base + 0x1c);
  m_story = base + U32(base + 0x20);
  m_categories = base + U32(base + 0x28);
  std::printf("[wants] %u events, %u trees, %u nodes, %u story roots\n", U32(base + 4), U32(base + 8), U32(base + 0xc), U32(base + 0x10));
  return true;
}

int WantFearManager::TreeCount() const { return m_header ? static_cast<int>(U32(m_header + 8)) : 0; }
const unsigned char* WantFearManager::Nodes(const unsigned char* tree) const { return m_nodes + U32(tree + 0x28) * 0x20; }
// 8014AB4C
const unsigned char* WantFearManager::NthChildOfNode(const unsigned char* tree, const unsigned char* node, int n) const {
  const unsigned char* children = m_header + U32(tree + 0x2c);
  return Nodes(tree) + S16(children + (S16(node + 0xa) + n) * 2) * 0x20;
}
int WantFearManager::NodeIndex(const unsigned char* tree, const unsigned char* node) const {
  return static_cast<int>((node - Nodes(tree)) / 0x20);
}

// 8014BF20
const unsigned char* WantFearManager::GetEventByIndex(int index) const { return m_events + index * 0x24; }
// 8014BF30
const unsigned char* WantFearManager::GetEvent(unsigned short id) const {
  if (!m_header) return nullptr;
  int lo = 0, hi = static_cast<int>(U32(m_header + 4)) - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const unsigned short v = U16(m_events + mid * 0x24);
    if (v == id) return m_events + mid * 0x24;
    if (id < v) hi = mid - 1;
    else lo = mid + 1;
  }
  return nullptr;
}
// 8014BFBC
const unsigned char* WantFearManager::GetTreeByIndex(int index) const { return m_header + U32(m_trees + index * 4); }
// 8014BFCC
const unsigned char* WantFearManager::GetTree(unsigned short id) const {
  int lo = 0, hi = TreeCount() - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const unsigned char* t = GetTreeByIndex(mid);
    const unsigned short v = U16(t);
    if (v == id) return t;
    if (id < v) hi = mid - 1;
    else lo = mid + 1;
  }
  return nullptr;
}

// 8014C064
// The root bookmark of the story tree listed for (`a`, `b`) (the story roots: {tree, a, b}).
WantFear__Bookmark WantFearManager::StoryTreeRoot(short a, short b) const {
  const unsigned int count = U32(m_header + 0x10);
  for (unsigned int i = 0; i < count; ++i) {
    const unsigned char* e = m_story + i * 6;
    if (S16(e + 2) != a || S16(e + 4) != b) continue;
    const unsigned char* tree = GetTree(U16(e));
    if (!tree) break;
    const unsigned char* root = Nodes(tree);
    const unsigned char* event = root ? GetEvent(U16(root + 2)) : nullptr;
    return WantFear__Bookmark(U16(tree), 0, 0, event && event[0xb] == 2 ? U16(root + 8) : 0);
  }
  return WantFear__Bookmark();
}

// 8014C164
// The category's {target, shader} list (categories {type, count, list}); C07C3544 when none.
unsigned int WantFearManager::GetCategoryShader(int type, short target) {
  const unsigned int none = 0xc07c3544u;
  if (!m_header) return none;
  const unsigned int n = U32(m_header + 0x24);
  for (unsigned int i = 0; i < n; ++i) {
    const unsigned char* c = m_categories + i * 12;
    if (static_cast<int>(U32(c)) != type) continue;
    const unsigned char* list = m_header + U32(c + 8);
    for (int k = 0; k < static_cast<int>(U32(c + 4)); ++k)
      if (S16(list + k * 8) == target) return U32(list + k * 8 + 4) ? U32(list + k * 8 + 4) : none;
    return none;
  }
  return none;
}

// 8014C3B8
int WantFearManager::Lottery(unsigned int* weights, unsigned int count) const {
  unsigned int total = 0;
  for (unsigned int i = 0; i < count; ++i) total += weights[i];
  if (total == 0) return -1;
  int left = static_cast<int>(static_cast<unsigned int>(simmy::sim_rand()) % total);  // RangedRandInt
  int i = -1;
  while (left >= 0) left -= static_cast<int>(weights[++i]);
  return i;
}

// 8014C1FC
// Each non-story tree of the kind (its root's event a fear or not) weighs its root's ad; drawn until
// one is new to the neighbour and valid (its countdown reset), else none.
WantFear__Bookmark WantFearManager::PickNewRoot(Neighbor* who, bool fear) {
  const int n = TreeCount();
  std::vector<unsigned int> weights(static_cast<size_t>(n), 0u);
  for (int i = 0; i < n; ++i) {
    const unsigned char* t = GetTreeByIndex(i);
    const unsigned char* root = Nodes(t);
    const unsigned char* ev = GetEvent(U16(root + 2));
    if (!(U16(t + 2) & 1) && ev && fear == (S8(ev + 2) == -0x80))
      weights[static_cast<size_t>(i)] = static_cast<unsigned int>(who->CalculateWantFearAd(root + 0x10, 0));
  }
  WantFear__Bookmark b;
  for (;;) {
    const int pick = Lottery(weights.data(), static_cast<unsigned int>(n));
    if (pick == -1) {
      b.Clear();
      return b;
    }
    const unsigned char* t = GetTreeByIndex(pick);
    b = WantFear__Bookmark(U16(t), 0, 0, 0);
    if (who->FindDuplicateBookmark(b) == -1 && b.IsValid(who, true)) break;
    weights[static_cast<size_t>(pick)] = 0;
  }
  b.ResetCountdown();
  return b;
}

// ---------------------------------------------------------------------------------------------------
// WantFear::Bookmark

const unsigned char* WantFear__Bookmark::Node() const {
  WantFearManager* m = TheWantFearManager();
  const unsigned char* t = m_node >= 0 ? m->GetTree(m_tree) : nullptr;
  if (!t || m_node >= S16(t + 0x24)) return nullptr;
  return m->Nodes(t) + m_node * 0x20;
}
const unsigned char* WantFear__Bookmark::Event() const {
  const unsigned char* n = Node();
  return n ? TheWantFearManager()->GetEvent(U16(n + 2)) : nullptr;
}

// 8014ABA0
WantFear__Bookmark::WantFear__Bookmark(unsigned short tree, short target, int node, unsigned short countdown) {
  m_tree = tree;
  m_field02 = 0;
  m_field08 = countdown;
  m_node = static_cast<short>(node);
  if (const unsigned char* n = Node())
    if (n[0x1d] & 2) m_field02 = target;
}
// 8014ABC8
void WantFear__Bookmark::Clear() {
  m_field08 = 0;
  m_node = -1;
  m_tree = 0;
  m_field02 = 0;
}
// 8014ABE4
void WantFear__Bookmark::ResetCountdown() {
  const unsigned char* e = Event();
  m_field08 = e && e[0xb] == 2 ? U16(Node() + 8) : 0;
}
// 8014AC34
int WantFear__Bookmark::GetNumBranches() const { return Node() ? S16(Node() + 0xc) : 0; }
// 8014AC40
bool WantFear__Bookmark::IsPlatinum() const { return Event() && S8(Event() + 2) == 1; }
// 8014AC74
bool WantFear__Bookmark::IsFear() const { return Event() && S8(Event() + 2) == -0x80; }
// 8014ACA8
bool WantFear__Bookmark::IsHidden() const { return Node() && (Node()[0x1d] & 1); }
// 8014ACB8
bool WantFear__Bookmark::IsDuplicateNode(const WantFear__Bookmark& o) const {
  return m_tree == o.m_tree && m_node == o.m_node && GetTarget() == o.GetTarget();
}
// 8014AD30
bool WantFear__Bookmark::IsDuplicateEvent(const WantFear__Bookmark& o) const {
  const unsigned char* a = Event();
  const unsigned char* b = o.Event();
  return a && b && U16(a) == U16(b) && GetTarget() == o.GetTarget();
}
// 8014ADA0
int WantFear__Bookmark::GetType() const {
  const unsigned char* e = Event();
  if (!e) return 0;
  if (S8(e + 2) == -0x80) return 0x80;
  if (S8(e + 2) == 1) return 1;
  return IsInStoryTree() ? 4 : 0;
}
// 8014AE08
bool WantFear__Bookmark::IsInStoryTree() const {
  const unsigned char* t = TheWantFearManager()->GetTree(m_tree);
  return t && (U16(t + 2) & 1);
}
// 8014B36C
int WantFear__Bookmark::GetTarget() const {
  const unsigned char* n = Node();
  if (!n) return 0;
  return (n[0x1d] & 2) ? m_field02 : S16(n + 6);
}
// 8014B38C
const unsigned char* WantFear__Bookmark::GetAdData() const { return Node() ? Node() + 0x10 : nullptr; }

// 8014AE3C
std::u16string WantFear__Bookmark::GetText(Neighbor* who, bool full) const {
  std::u16string text;
  const unsigned char* n = Node();
  const unsigned char* e = Event();
  if (!n || !e) return text;
  const int target = GetTarget();
  bool ok = false;
  if (S8(n + 4) != 0 && (!(n[0x1d] & 2) || target != 0)) ok = GetEventText(e, S8(n + 4) + (full ? 2 : 1), text);
  if (!ok) {
    int index = 1;
    if (target != 0) {
      index = 3;
      Neighbor* t = S8(e + 0xa) == 4 && Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(target) : nullptr;
      if (t && !NeighborPerson(t)) index = 5;  // GC: the target's selector has no sim description +0x84
    }
    GetEventText(e, index + (full ? 1 : 0), text);
  }
  const int type = S8(e + 0xa);
  if (type == 4) {
    Neighbor* t = Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(target) : nullptr;
    Replace(text, u"$sim", t ? NeighborName(t) : Ascii("<??? Sim>"));
  } else if (type != 0) {
    static const char* const keys[8] = {nullptr, "$category", "$career", "$skill", nullptr, "$relationship", "$motive", "$foodeffect"};
    if (type < 8 && keys[type]) Replace(text, Ascii(keys[type]), CategoryString(type, target));
  }
  char buf[32];
  std::snprintf(buf, sizeof buf, "%d", S16(n + 8));
  Replace(text, u"$int", Ascii(buf));
  std::snprintf(buf, sizeof buf, "\xa7%d", S16(n + 8) * 100);  // GC: UIObjectBase::makeMoneyString
  Replace(text, u"$moneyHundreds", Ascii(buf));
  std::snprintf(buf, sizeof buf, "\xa7%d", S16(n + 8));
  Replace(text, u"$money", Ascii(buf));
  Replace(text, u"$me", NeighborName(who));
  if (!full) {
    std::snprintf(buf, sizeof buf, " (%+d)", S16(n + 0xe));
    text += Ascii(buf);
  }
  return text;
}

// 8014B398
// By the target type: none (0) or a sim (4), or no target: the event's icon (C07C3544 for none);
// else the category's icon for the target.
unsigned int WantFear__Bookmark::GetBaseShader(Neighbor*) const {
  const unsigned char* e = Event();
  if (!e) return 0xc07c3544u;
  if (S8(e + 0xa) == 0 || S8(e + 0xa) == 4 || GetTarget() == 0) return U32(e + 0xc) ? U32(e + 0xc) : 0xc07c3544u;
  return TheWantFearManager()->GetCategoryShader(S8(e + 0xa), static_cast<short>(GetTarget()));
}
// 8014B440
unsigned int WantFear__Bookmark::GetOverlayShader(Neighbor*) const { return 0x9f93ff74u; }
// 8014B44C
unsigned int WantFear__Bookmark::GetBackgroundShader(Neighbor*) const {
  switch (GetType()) {
    case 0: return 0xa0d97dcfu;
    case 1: return 0x087b35fbu;
    case 4: return 0x4364efb7u;
    case 0x80: return 0x8ca1c6a6u;
    default: return 0xc07c3544u;
  }
}

// 8014B5F4
bool WantFear__Bookmark::IsValid(Neighbor* who, bool checkAd) const {
  if (checkAd && who->CalculateBookmarkAd(*this) == 0) return false;
  if (!IsFear() && IsInStoryTree()) return true;
  const unsigned char* e = Event();
  if (!e || S16(e + 6) == -1) return true;
  const int r = RunWantCheckTree(S16(e + 6), who, *this);
  return r != 0;  // no wants object: valid
}
// 8014B748
bool WantFear__Bookmark::IsCompleted(Neighbor* who) const {
  const unsigned char* e = Event();
  if (!e || S16(e + 8) == -1) return false;
  return RunWantCheckTree(S16(e + 8), who, *this) == 1;
}
// 8014B4DC
bool WantFear__Bookmark::SatisfiedBy(const WantFear__Notification& n) const {
  const unsigned char* node = Node();
  if (!node || S16(node + 2) != n.m_event) return false;
  if (GetTarget() != 0 && GetTarget() != n.m_target) return false;
  const short count = S16(node + 8);
  if (count == 0) return true;
  switch (S8(node + 0x1c)) {
    case 0: return true;
    case 1: return n.m_count < count;
    case 2: return n.m_count <= count;
    case 3: return n.m_count == count;
    case 4: return count <= n.m_count;
    case 5: return count < n.m_count;
    default: return false;
  }
}

// 8014B858
void WantFear__Bookmark::ApplyReward(Neighbor* who) {
  const unsigned char* node = Node();
  const int reward = node ? S16(node + 0xe) : 0;
  cXPersonImpl* person = NeighborPerson(who);
  const float points = static_cast<float>(reward) / kAspirationPointToMeterRatio;
  if (!person) {
    who->SetAspirationScore(static_cast<float>(who->GetAspirationScore() + points));
  } else {
    person->SetAspirationScore(static_cast<float>(person->GetAspirationScore() + points));
    person->SpawnAspirationBalloon(reward);
  }
  const bool counts = person && (person->IsPrimarySim(0) || person->IsPrimarySim(1) || person->GetPersonData(0x4b) != 0);
  if (!counts) return;
  PlayerFamilyImpl* family = Globs_pNeighborhood ? dynamic_cast<PlayerFamilyImpl*>(Globs_pNeighborhood->GetFamily(1)) : nullptr;
  if (!family) return;
  int total = family->m_totalAspirationScore + reward;
  if (total > kTotalAspirationMax) total = kTotalAspirationMax;
  family->m_totalAspirationScore = total;
  FamilyGoalUnlock(1).GrantObjectUnlocks(total);
  if (std::getenv("SIMMY_WANTS_TRACE"))
    std::printf("[wants] reward %d: aspiration %.0f, family total %d\n", reward,
                person ? person->GetAspirationScore() : who->GetAspirationScore(), total);
}

// 8014BA7C
WantFear__Bookmark WantFear__Bookmark::GetNextBookmarkAlongBranch(short branch) const {
  WantFearManager* m = TheWantFearManager();
  const unsigned char* t = m->GetTree(m_tree);
  const unsigned char* child = m->NthChildOfNode(t, Node(), branch);
  int target = 0;
  if ((child[0x1d] & 2) && (target = m_field02) == 0) target = S16(Node() + 6);
  const unsigned char* e = m->GetEvent(U16(child + 2));
  const unsigned short count = e && e[0xb] == 2 ? U16(child + 8) : 0;
  return WantFear__Bookmark(m_tree, static_cast<short>(target), m->NodeIndex(t, child), count);
}

// ---------------------------------------------------------------------------------------------------
// Neighbor

// 800CAA40
// New wants and fears: five want trees and four fear trees, then the seven active slots.
void Neighbor::InitWantsFears() {
  ResetAllWantsAndFears();
  for (unsigned int i = 0; i < 5; ++i) StartNewTree(i);
  for (unsigned int i = 0; i < 4; ++i) StartNewTree(i + 0xd);
  for (unsigned int i = 0; i < 7; ++i) MakeNewActiveBookmark(i);
  InitWantFearIcons();
}

// 800CAAC8
void Neighbor::InitWantFearIcons() {
  // PC development aid: SIMMY_WANTS_TRACE=1 prints the active wants and fears.
  static const bool trace = std::getenv("SIMMY_WANTS_TRACE") != nullptr;
  if (trace)
    for (int s = 0; s < 7; ++s) {
      std::string line;
      if (m_active[s] >= 0)
        for (char16_t c : m_bookmarks[m_active[s]].GetText(this, false)) line += c < 0x80 ? static_cast<char>(c) : '?';
      std::printf("[wants] %s slot %d: bookmark %d type %d \"%s\"\n", m_name, s, m_active[s],
                  m_active[s] >= 0 ? m_bookmarks[m_active[s]].GetType() : -1, line.c_str());
    }
  if (trace)
    for (int i = 0; i < m_relations->CountKeys(); ++i) {
      const int key = m_relations->GetNthKey(i);
      Neighbor* o = Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(key) : nullptr;
      int v = 0;
      GetRelationshipById(key, &v);
      std::printf("[wants] %s relationship with %s (#%d): %d, level %d%s\n", m_name, o ? o->m_name : "?", key, v,
                  o ? CalculateRelationshipLevel(o) : -1, o && IsMarriedTo(o) ? ", married" : "");
    }
  if (!_gd.m_h2dTarget) return;
  for (unsigned int i = 0; i < 7; ++i) UpdateUIifNeeded(i, 3);
}

// 800CAED4
int Neighbor::FindDuplicateBookmark(const Bookmark& b) const {
  for (int i = 0; i < 20; ++i)
    if (!m_bookmarks[i].Empty() && m_bookmarks[i].IsDuplicateNode(b)) return i;
  return -1;
}

// 800CB2D0
int Neighbor::FindActiveSlotIndex(int b) const {
  if (b < 0 || m_bookmarks[b].Empty()) return kNoActiveIndex;
  for (int s = 0; s < 7; ++s)
    if (m_active[s] == b) return s;
  return kNoActiveIndex;
}

// 800CB32C
// The bookmark's node is done: each branch's next node that is new and valid takes a free (or the
// weakest inactive) bookmark of its kind, and moves on at once when already completed; with none left
// here a new tree starts.
void Neighbor::AdvanceBookmark(unsigned int index) {
  const Bookmark old = m_bookmarks[index];
  m_bookmarks[index].Clear();
  for (short br = 0; br < old.GetNumBranches(); ++br) {
    const Bookmark next = old.GetNextBookmarkAlongBranch(br);
    if (FindDuplicateBookmark(next) != -1 || !next.IsValid(this, true)) continue;
    unsigned int first, end;
    GetBookmarkRange(next.IsFear(), first, end);
    const int slot = FindBookmarkToReplace(first, end);
    if (slot < 0) continue;
    m_bookmarks[slot] = next;
    if (m_bookmarks[slot].IsCompleted(this)) {
      if (std::getenv("SIMMY_WANTS_TRACE") && m_bookmarks[slot].Node())
        std::printf("[wants] %s: event 0x%x completed at once (its check tree)\n", m_name,
                    static_cast<unsigned short>(simmy::be16(m_bookmarks[slot].Node() + 2)));
      AdvanceBookmark(static_cast<unsigned int>(slot));
    }
  }
  if (m_bookmarks[index].Empty()) StartNewTree(index);
}

// 800CB490
void Neighbor::GetSlotRange(bool fear, unsigned int& first, unsigned int& end) {
  first = fear ? 4 : 0;
  end = fear ? 7 : 4;
}
// 800CB4BC
void Neighbor::GetBookmarkRange(bool fear, unsigned int& first, unsigned int& end) {
  first = fear ? 0xd : 0;
  end = fear ? 0x14 : 0xd;
}

// 800CB4E8
// A free bookmark of the range, else the inactive one (a fear or not a story want) with the lowest ad.
int Neighbor::FindBookmarkToReplace(unsigned int first, unsigned int end) {
  for (unsigned int i = first; i < end; ++i)
    if (m_bookmarks[i].Empty()) return static_cast<int>(i);
  int best = -1;
  unsigned int bestAd = 0x80;
  for (unsigned int i = first; i < end; ++i) {
    if (FindActiveSlotIndex(static_cast<int>(i)) != kNoActiveIndex) continue;
    if (!m_bookmarks[i].IsFear() && m_bookmarks[i].IsInStoryTree()) continue;
    const unsigned int ad = static_cast<unsigned int>(CalculateBookmarkAd(m_bookmarks[i]));
    if (ad < bestAd) best = static_cast<int>(i), bestAd = ad;
  }
  return best;
}

// 800CB5F4
int Neighbor::CalculateBookmarkAd(const Bookmark& b) {
  const unsigned char* ad = b.GetAdData();
  return ad ? CalculateWantFearAd(ad, static_cast<short>(b.GetTarget())) : 0;
}

// 800CB644
double Neighbor::CalcAdMultiplier(float value, float from, float to) {
  const float lo = to < from ? to : from, hi = to < from ? from : to;
  double r = 0.0;
  if (lo < value) r = value < hi ? (value - lo) / (hi - lo) : 1.0;
  if (to < from) r = 1.0 - r;
  return r;
}

// 800CB6A8
// AdData: +0 aspiration weight, +1 personality weight, +2 relationship weight, +3 mood weight (bit 7 of
// each: required), +4 flags (bits 0..9 the personality traits nice / not nice, active / lazy,
// playful, outgoing, neat and their opposites; 0x400..0x4000 the aspirations 0..4), +6/+7 the
// relationship range, +8/+9 the mood range, +0xA/+0xB the weight range. The sim's traits, mood and
// aspiration (its person's when on the lot, else the neighbour's person data). 0: not wanted, else the
// weight + 1.
int Neighbor::CalculateWantFearAd(const unsigned char* ad, short target) {
  cXPersonImpl* p = NeighborPerson(this);
  float nice, active, playful, outgoing, neat, mood;
  int aspiration;
  if (!p) {
    nice = m_pers[2], active = m_pers[3], playful = m_pers[5], outgoing = m_pers[6], neat = m_pers[7];
    mood = m_motives[3];
    aspiration = m_pers[0x14];
  } else {
    mood = p->m_motives.m_values[3];
    aspiration = p->GetPersonData(0x14);
    nice = static_cast<float>(p->GetPersonData(2)), active = static_cast<float>(p->GetPersonData(3));
    playful = static_cast<float>(p->GetPersonData(5)), outgoing = static_cast<float>(p->GetPersonData(6));
    neat = static_cast<float>(p->GetPersonData(7));
  }
  int relValue = 0;
  bool rel = false;
  if (ad[2] != 0 && target != kNoTarget) rel = GetRelationshipById(target, &relValue) != 0;
  float sum = 0.0f;
  if (ad[3] != 0) {
    const double m = CalcAdMultiplier(mood, static_cast<float>(S8(ad + 8)), static_cast<float>(S8(ad + 9)));
    if (m <= 0.0 && (ad[3] & 0x80)) return 0;
    sum = static_cast<float>(m * static_cast<float>(ad[3] & 0x7f) + sum);
  }
  if (ad[2] != 0) {
    double m = rel ? CalcAdMultiplier(static_cast<float>(relValue), static_cast<float>(S8(ad + 6)), static_cast<float>(S8(ad + 7))) : 0.0;
    if (S8(ad + 7) < relValue) m = 0.0;
    if (m <= 0.0 && (ad[2] & 0x80)) return 0;
    sum = static_cast<float>(m * static_cast<float>(ad[2] & 0x7f) + sum);
  }
  const unsigned short flags = U16(ad + 4);
  const bool aspMatch = (aspiration == 2 && (flags & 0x1000)) || (aspiration == 0 && (flags & 0x400)) ||
                        (aspiration == 3 && (flags & 0x2000)) || (aspiration == 1 && (flags & 0x800)) ||
                        (aspiration == 4 && (flags & 0x4000));
  if (aspMatch) sum += static_cast<float>(ad[0] & 0x7f);
  else if (ad[0] & 0x80) return 0;
  const float traits[10] = {nice, 1000.0f - nice, active, 1000.0f - active, playful, 1000.0f - playful,
                            outgoing, 1000.0f - outgoing, neat, 1000.0f - neat};
  float best = 0.0f;
  for (int i = 0; i < 10; ++i)
    if ((flags >> i) & 1) best = std::max(best, traits[i] / 1000.0f);
  if (best == 0.0f && (ad[1] & 0x80)) return 0;
  const float w = ((best * static_cast<float>(ad[1] & 0x7f) + sum) / static_cast<float>(kMaxAdMax)) *
                      static_cast<float>(S8(ad + 0xb) - S8(ad + 0xa)) +
                  static_cast<float>(S8(ad + 0xa));
  return static_cast<int>(static_cast<unsigned int>(w)) + 1;
}

// 800CBF84
// A want slot (0..3) prefers a story want, else any of its kind (a fear for 4..6) by lottery; with none
// a bookmark of the kind is replaced by a new tree and the draw repeats. A story want also clears
// active wants with the same event.
int Neighbor::MakeNewActiveBookmark(unsigned int slot) {
  const bool fear = slot > 3;
  m_active[slot] = -1;
  int b = -1;
  while (b < 0) {
    if (!fear) b = SelectInactiveBookmark(false, true);
    if (b < 0) b = SelectInactiveBookmark(fear, false);
    if (b < 0) {
      unsigned int first, end;
      GetBookmarkRange(fear, first, end);
      const int r = FindBookmarkToReplace(first, end);
      if (r < 0) return -1;
      m_bookmarks[r].Clear();
      StartNewTree(static_cast<unsigned int>(r));
      if (m_bookmarks[r].Empty()) return -1;  // PC: nothing valid to start (GC: tries again)
    } else {
      m_active[slot] = b;
      m_bookmarks[b].ResetCountdown();
    }
  }
  if (!m_bookmarks[b].IsFear() && m_bookmarks[b].IsInStoryTree()) {
    for (unsigned int s = 0; s < 7; ++s) {
      const int o = m_active[s];
      if (o >= 0 && o != b && !m_bookmarks[o].Empty() && !m_bookmarks[o].IsInStoryTree() &&
          m_bookmarks[o].IsDuplicateEvent(m_bookmarks[b]))
        ShuffleWantFear(static_cast<int>(s));
    }
  }
  return b;
}

// 800CC118
// Every visible bookmark not shown, of the kind (a story want only when asked), not the same event
// as one shown (story wants may), weighs its ad (+1 for story wants); drawn until a valid one.
int Neighbor::SelectInactiveBookmark(bool fear, bool storyOnly) {
  unsigned int weights[20] = {};
  for (int i = 0; i < 20; ++i) {
    Bookmark& b = m_bookmarks[i];
    if (b.Empty() || b.IsHidden() || FindActiveSlotIndex(i) != kNoActiveIndex) continue;
    if (fear != b.IsFear() || (storyOnly && !b.IsInStoryTree())) continue;
    if (b.IsFear() || !b.IsInStoryTree()) {
      bool dup = false;
      for (int s = 0; s < 7 && !dup; ++s) {
        const int o = m_active[s];
        if (o >= 0 && !m_bookmarks[o].Empty() && m_bookmarks[o].IsDuplicateEvent(b)) dup = true;
      }
      if (dup) continue;
    }
    weights[i] = static_cast<unsigned int>(CalculateBookmarkAd(b));
    if (b.IsInStoryTree() && !b.IsFear()) ++weights[i];
  }
  WantFearManager* m = TheWantFearManager();
  for (;;) {
    const int pick = m->Lottery(weights, 20);
    if (pick == -1) return -1;
    if (m_bookmarks[pick].IsValid(this, true)) return pick;
    m_bookmarks[pick].Clear();
    weights[pick] = 0;
  }
}

// 800CC380
void Neighbor::StartNewTree(unsigned int index) {
  m_bookmarks[index] = TheWantFearManager()->PickNewRoot(this, index > 0xc);
  if (!m_bookmarks[index].Empty() && m_bookmarks[index].IsCompleted(this)) AdvanceBookmark(index);
}

// 800CC418
// The slot's icon to the HUD of the player whose selected sim this neighbour is.
void Neighbor::UpdateUIifNeeded(unsigned int slot, int anim) {
  const int b = m_active[slot];
  cXPersonImpl* p = NeighborPerson(this);
  H2DTarget* h2d = _gd.m_h2dTarget;
  if (b < 0 || !p || !h2d) return;
  int player = -1;
  for (int i = 0; i < 2; ++i)
    if (cXPerson* s = _globals.GetSelectedPerson(i))
      if (s->AsObject() == p) player = i;
  if (player < 0) return;
  h2d->NewWantFearIcon(player, static_cast<int>(slot), m_bookmarks[b].GetBaseShader(this), m_bookmarks[b].GetOverlayShader(this),
                       anim, this);
}

// 800CAD10
// GC: nothing while DAT_80475C74 is set (not written in the port's paths).
int Neighbor::Notify(WantFear__Notification& n, bool tellOthers) {
  int result = 0;
  if (std::getenv("SIMMY_WANTS_TRACE"))
    std::printf("[wants] %s: event 0x%x target %d count %d\n", m_name, static_cast<unsigned short>(n.m_event), n.m_target, n.m_count);
  for (unsigned int i = 0; i < 20; ++i) {
    Bookmark& b = m_bookmarks[i];
    if (b.Empty() || !b.SatisfiedBy(n)) continue;
    if (b.m_field08 != 0 && --b.m_field08 != 0) continue;
    result = 1;
    b.m_field02 = n.m_target;
    const int slot = FindActiveSlotIndex(static_cast<int>(i));
    const Bookmark done = b;
    if (std::getenv("SIMMY_WANTS_TRACE")) {
      const std::u16string t = done.GetText(this, true);
      std::string a(t.begin(), t.end());
      std::printf("[wants] %s: event 0x%x fulfils \"%s\"%s\n", m_name, n.m_event, a.c_str(), slot != kNoActiveIndex ? " (shown)" : "");
    }
    AdvanceBookmark(i);
    if (slot == kNoActiveIndex) continue;
    m_active[slot] = -1;
    result = 2;
    Bookmark reward = done;
    reward.ApplyReward(this);
    if (tellOthers && reward.IsPlatinum()) Globs_pNeighborhood->NotifyAllNeighbors(n, this);
    MakeNewActiveBookmark(static_cast<unsigned int>(slot));
    UpdateUIifNeeded(static_cast<unsigned int>(slot), 1);
    if (!reward.IsFear()) ShuffleWantFear(static_cast<int>(simmy::sim_rand() % 3) + 4);
  }
  return result;
}

// 800CAF58
bool Neighbor::ForceNewTree(unsigned short treeId) {
  WantFearManager* m = TheWantFearManager();
  const unsigned char* tree = m->GetTree(treeId);
  if (!tree) return false;
  const bool story = (U16(tree + 2) & 1) != 0;
  if (story) {
    // The story wants give way: cleared, and the empty slots filled again.
    unsigned int first, end;
    GetBookmarkRange(false, first, end);
    for (unsigned int i = first; i < end; ++i) {
      if (m_bookmarks[i].Empty() || !m_bookmarks[i].IsInStoryTree()) continue;
      const int slot = FindActiveSlotIndex(static_cast<int>(i));
      if (slot != kNoActiveIndex) m_active[slot] = -1;
      m_bookmarks[i].Clear();
    }
    for (unsigned int s = 0; s < 7; ++s) {
      if (m_active[s] == -1) {
        MakeNewActiveBookmark(s);
        UpdateUIifNeeded(s, 2);
      }
    }
  }
  // The root (node id 0, the first node).
  const unsigned char* root = m->Nodes(tree);
  const unsigned char* event = root ? m->GetEvent(U16(root + 2)) : nullptr;
  if (!event) return false;
  const bool fear = S8(event + 2) == -0x80;
  Bookmark b(treeId, kNoTarget, 0, event[0xb] == 2 ? U16(root + 8) : 0);
  if (!b.IsValid(this, false)) return false;
  int index = FindDuplicateBookmark(b);
  bool advanced = false;
  if (index == -1) {
    unsigned int first, end;
    GetBookmarkRange(fear, first, end);
    index = FindBookmarkToReplace(first, end);
    if (index == -1) return false;
    m_bookmarks[index] = b;
    if (m_bookmarks[index].IsCompleted(this)) {
      AdvanceBookmark(static_cast<unsigned int>(index));
      advanced = true;
      if (!story) return true;
    }
  }
  if (!advanced && FindActiveSlotIndex(index) != -1) return true;
  // An active slot of its kind: an empty one, else (not for a story tree's want) the weakest non-story
  // one by its ad.
  unsigned int first, end;
  GetSlotRange(fear, first, end);
  int best = 10000;
  int chosen = -1, pick = -1;
  for (unsigned int s = first; s < end; ++s) {
    pick = static_cast<int>(s);
    const int o = m_active[s];
    if (o < 0 || m_bookmarks[o].Empty()) break;
    if (m_bookmarks[o].IsInStoryTree()) {
      pick = chosen;
      continue;
    }
    if (story && !m_bookmarks[index].IsFear()) break;
    const int ad = CalculateBookmarkAd(m_bookmarks[o]);
    if (ad < best) {
      best = ad;
      chosen = static_cast<int>(s);
    }
    pick = chosen;
  }
  if (pick == -1) return false;
  if (!advanced) {
    m_active[pick] = index;
    UpdateUIifNeeded(static_cast<unsigned int>(pick), 2);
  } else {
    ShuffleWantFear(pick);
  }
  return true;
}

// 800CC4E4
void Neighbor::ResetAllWantsAndFears() {
  for (Bookmark& b : m_bookmarks) b.Clear();
  for (int& a : m_active) a = -1;
  m_field1B4 = 0;
}

// 800CC554
void Neighbor::ShuffleWantFear(int slot) {
  if (slot == -1) {
    for (unsigned int s = 0; s < 7; ++s) {
      MakeNewActiveBookmark(s);
      UpdateUIifNeeded(s, 2);
    }
  } else {
    MakeNewActiveBookmark(static_cast<unsigned int>(slot));
    UpdateUIifNeeded(static_cast<unsigned int>(slot), 2);
  }
}

// ---------------------------------------------------------------------------------------------------
// The purchase's want/fear events (PCTTarget::PlaceModeCallback 801D745C, its part before the price is
// paid; PC: a helper so buy mode's file only calls it): the residents are told the platinum and the
// plain "worth" events (target the catalogue category, def +0xA; count the price), the category event,
// and for lights (category 0x3A) / chairs (0x18) their counting events.
void NotifyBuildBuyPurchase(ObjSelector* master, int price) {
  const auto* def = master ? master->m_def : nullptr;
  if (!def || !Globs_pNeighborhood) return;
  const short category = ERQuickdata::S16(def, 0xa);
  WantFear__Notification n;
  n.m_event = kEventBuildBuyWorthPlatinum;
  n.m_target = category;
  n.m_count = static_cast<short>(price);
  Globs_pNeighborhood->NotifyAllResidents(n, nullptr);
  n.m_event = kEventBuildBuyWorth;
  Globs_pNeighborhood->NotifyAllResidents(n, nullptr);
  n.m_event = kEventBuildBuyCategory;
  n.m_count = 0;
  Globs_pNeighborhood->NotifyAllResidents(n, nullptr);
  if (category == 0x3a) {
    n.m_event = kEventBuyXLights;
    Globs_pNeighborhood->NotifyAllResidents(n, nullptr);
  }
  if (category == 0x18) {
    n.m_event = kEventBuyXChairs;
    Globs_pNeighborhood->NotifyAllResidents(n, nullptr);
  }
}

// ---------------------------------------------------------------------------------------------------
// PC: the HUD's view of a slot (hudpanels.cpp NeighborWantFearSlot)

bool WantFearSlot(Neighbor* n, int slot, int* type, unsigned int* base, unsigned int* overlay, std::u16string* text) {
  if (!n || slot < 0 || slot >= 7 || n->m_active[slot] < 0) return false;
  const WantFear__Bookmark& b = n->m_bookmarks[n->m_active[slot]];
  if (b.Empty()) return false;
  *type = b.GetType();
  *base = b.GetBaseShader(n);
  *overlay = b.GetOverlayShader(n);
  *text = b.GetText(n, false);
  return true;
}

// 8014AB70 WantFear::Bookmark::Bookmark (inline in the header)
// 8014BD04 WantFearManager::CaptureLog (inline)
