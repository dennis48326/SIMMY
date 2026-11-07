// Port of the autonomy parts of game/person.cpp (cXPersonImpl::TryFindBestAction, primitive 0x03;
// SetIdlePlayerSimAutonomous), game/motiveeffects.cpp, game/motivecurve.cpp, game/piecewise.cpp,
// game/objtestsim.cpp (AppendInteractionsForAuto, the autonomous TestInteraction) and
// cXObjectImpl::CanChooseAutonomously (game/object.cpp): a sim nobody steers looks at every object's
// tree table, scores each interaction it may do by how much its motive advertisements would raise
// the sim's motive curves (less with distance), and takes one of the best few at random.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
// 804240CC: the motives the autonomy weighs (energy, comfort, hunger, hygiene, bladder, mood, 0xD,
// social, fun).
const int kAutonomyMotives[9] = {5, 6, 7, 8, 9, 3, 0xd, 0xe, 0xf};
// 803DCC3C: their default curves (LoadMotiveEffects when the person's definition +0x66 names no file).
const char* const kMotiveCurves[9] = {
    "(-100;-100) (-72;90) (100;90)",          "(-100;-100) (-80;-30) (-45;0) (10;10)",
    "(-100;-100) (-25;40) (50;50)",           "(-100;-100) (25;25) (-35;15)",
    "(-100;-100) (-20;-20) (0;0) (50;25)",    "(-100;-100) (-50;-80)",
    "(-100;-100) (-25;40) (50;50)",           "(-100;-100) (0;65) (75;75)",
    "(-100;-100) (0;65) (75;75)"};
// 803DA408: personality advertisements {person data, inverted} by GetPersonalityAd.
const struct {
  int data;
  int invert;
} kPersonalityAds[23] = {{-1, 0}, {2, 0}, {2, 1}, {3, 0}, {3, 1}, {4, 0}, {4, 1}, {5, 0}, {5, 1}, {6, 0}, {6, 1}, {7, 0},
                         {7, 1}, {9, 0}, {10, 0}, {11, 0}, {12, 0}, {13, 0}, {14, 0}, {15, 0}, {16, 0}, {17, 0}, {18, 0}};

float gMinAutonomyFamilyScore = 0.2f;    // 804FDD18
float gMinAutonomyVisitorScore = 0.05f;  // 804FDD1C
float gMinAutonomySittingScore = 0.1f;   // 804FDD20
int gInteractionRandCount = 5;           // 804FDD24
bool sAutonomyConstantsLoaded = false;
const int sUseScalingInteractionRange = 1;     // cXPerson::sUseScalingInteractionRange
const int sFreeWill = 1;                       // cXObjectImpl::sFreeWill

bool TraceAutonomy() {
  static const bool on = std::getenv("SIMMY_AUTO_TRACE") != nullptr;
  return on;
}

std::string Narrow16(const unsigned short* s) {
  std::string out;
  for (; s && *s && out.size() < 60; ++s) out += *s < 0x80 ? static_cast<char>(*s) : '?';
  return out;
}

// The object's tree table (cXObject::GetTreeTab, vtable 0x364): ResFile +0x24's first table (0x0C:
// +0 the entries, 0x24 bytes each, +8 the prefix check tree).
const unsigned char* TreeTab(cXObjectImpl* o) {
  const unsigned char* file = o ? SelectorTreeTabResFile(o->m_selector) : nullptr;
  const auto* tables = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x24)) : nullptr;
  if (!tables || ERQuickdata::U32(tables - 4, 0) == 0) return nullptr;
  return tables;
}
unsigned int TreeTabCount(const unsigned char* tab) {
  const auto* entries = tab ? static_cast<const unsigned char*>(ERQuickdata::Ptr(tab, 0)) : nullptr;
  return entries ? ERQuickdata::U32(entries - 4, 0) : 0;
}
const unsigned char* TreeTabNth(const unsigned char* tab, unsigned int i) {
  const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(tab, 0));
  return entries + i * 0x24;
}
// TreeTableEntryQuickData::GetDebugOnly (8015E688): flag 0x80 or a name starting with '!'.
bool EntryDebugOnly(const unsigned char* e) {
  const unsigned short* name = ERQuickdata::LocString(e, 0x1c);
  return (ERQuickdata::U32(e, 0x10) & 0x80) != 0 || (name && name[0] == '!');
}
// GetManualOnly (8015E6E4): the autonomy threshold +0x16 over 99.
bool EntryManualOnly(const unsigned char* e) { return ERQuickdata::S16(e, 0x16) > 99; }

// cXObjectImpl::IsMultiTile (vtable 0x394; PC: a multi-tile group's master id) and the definition's
// group lead number (+0x10: > 0 a part that leads nothing, -n the part leading part n).
bool MultiTile(cXObjectImpl* o) { return o->m_def && objdef::MasterID(o->m_def) != 0; }
short GroupLead(cXObjectImpl* o) { return o->m_def ? ERQuickdata::S16(o->m_def, 0x10) : 0; }

// cXObjectImpl::IsInWorld (800DCB50): on the lot.
bool InWorld(cXObjectImpl* o) { return (o->m_location[1] >> 4) > 0 && (o->m_location[0] >> 4) > 0; }

// cXPersonImpl::IsSelected (8012953C: misc flag 2). PC: some player's selected person.
bool SelectedByPlayer(cXPersonImpl* p) {
  for (int i = 0; i < 2; ++i)
    if (cXPerson* s = _globals.GetSelectedPerson(i))
      if (s->AsObject() == p) return true;
  return false;
}

// 801270E0
bool IsPermittedToRunCheckTreeForAds(unsigned int guid, int index) {
  return guid == 0xC867A344u && (index == 2 || index == 10);
}

}  // namespace

// FloatConstantsQuickData::Get (800B6DF0) on the global object file's constants (ResFile +0x30: {items,
// id} groups, an item {float value, char* name}): `name`'s value in group `id`, else `def`. GC:
// FloatConstants::CreateInstance + Load(GlobalConstantsClient::GetFileID, GetID).
float GlobalFloatConstant(int id, const char* name, float def) {
  const ERQuickdata* qd = Globs_pObjectFolder ? Globs_pObjectFolder->m_objects : nullptr;
  const void* globals = qd ? qd->getTable("GlobalResFile") : nullptr;
  const void* row = globals ? qd->getRow(globals, "global") : nullptr;
  const auto* file = row ? static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 0)) : nullptr;
  const auto* groups = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x30)) : nullptr;
  if (!groups) return def;
  const unsigned int n = ERQuickdata::U32(groups - 4, 0);
  for (unsigned int g = 0; g < n; ++g) {
    if (ERQuickdata::S32(groups + g * 8, 4) != id) continue;
    const auto* items = static_cast<const unsigned char*>(ERQuickdata::Ptr(groups + g * 8, 0));
    const unsigned int count = items ? ERQuickdata::U32(items - 4, 0) : 0;
    for (unsigned int i = 0; i < count; ++i) {
      const char* key = ERQuickdata::String(items + i * 8, 4);
      if (key && std::strcmp(key, name) == 0) return ERQuickdata::F32(items + i * 8, 0);
    }
    return def;
  }
  return def;
}

namespace {
struct ScoredInteraction {
  short m_object = 0;      // +0x00
  int m_index = 0;         // +0x04
  float m_score = 0.0f;    // +0x08
  unsigned int m_autoFirst = 0;  // +0x0C
  Behavior* m_behavior = nullptr;  // +0x10
  short m_checkTree = 0;   // +0x14
  short m_prefixTree = 0;  // +0x16
  unsigned int m_typeOne = 0;      // +0x18
  int m_checkedWithAds = 0;        // +0x1C
  int m_prefixPassed = 0;          // +0x20
};
}  // namespace

float gFunctionalScoreDistanceAttenuation = 3.0f;  // 804FDD2C (TryFindFunctionalObject)

// 8011A9E8
// The autonomy tuning: the global file's constants group 2 (sTheAutonomyClient), the defaults as the
// GC passes them (lot 9's data: family / sitting / visitor minimum 0, attenuations 0.1 / 0.3 / 0.6,
// visitors' 0.01 / 0.02 / 0.03, four at random, functional distance attenuation 3).
void AutonomyConstantsClient::UpdateConstants() { UpdateAutonomyConstants(); }

// PC: AutonomyConstantsClient::UpdateConstants' body, callable without the client (GC: run when the
// global file's constants load).
void UpdateAutonomyConstants() {
  gMinAutonomyFamilyScore = GlobalFloatConstant(2, "min autonomy score for family", 0.2f);
  gMinAutonomyVisitorScore = GlobalFloatConstant(2, "min autonomy score for visitors", 0.05f);
  gLowAttenuation = GlobalFloatConstant(2, "low attenuation", 0.002f);
  gModerateAttenuation = GlobalFloatConstant(2, "moderate attenuation", 0.02f);
  gHighAttenuation = GlobalFloatConstant(2, "high attenuation", 0.1f);
  gVisLowAttenuation = GlobalFloatConstant(2, "visitor low attenuation", 0.002f);
  gVisModerateAttenuation = GlobalFloatConstant(2, "visitor moderate attenuation", 0.02f);
  gVisHighAttenuation = GlobalFloatConstant(2, "visitor high attenuation", 0.1f);
  gMinAutonomySittingScore = GlobalFloatConstant(2, "min autonomy score for sitting", 0.2f);
  gInteractionRandCount = static_cast<int>(GlobalFloatConstant(2, "random selection count", 10.0f));
  gFunctionalScoreDistanceAttenuation = GlobalFloatConstant(2, "functional distance attenuation", 3.0f);
  sAutonomyConstantsLoaded = true;
}

// ---------------------------------------------------------------------------------------------------
// PiecewiseFn

// 8012CBE8
PiecewiseFn::PiecewiseFn() {}
// 8012CC04
PiecewiseFn::~PiecewiseFn() {}
// 8012CC48
void PiecewiseFn::Reset() {
  m_points.clear();
  m_slopes.clear();
}
// 8012CC54
void PiecewiseFn::SetMaxPoints(int count) {
  Reset();
  m_max = count < 1 ? 0 : count;
}
// 8012CCE4
// Inserted in x order (a point at an x already there is ignored); the slopes are made again.
void PiecewiseFn::AddPoint(Point& p) {
  if (static_cast<int>(m_points.size()) == m_max) return;
  size_t at = 0;
  for (; at < m_points.size(); ++at) {
    if (m_points[at].x == p.x) return;
    if (m_points[at].x > p.x) break;
  }
  m_points.insert(m_points.begin() + static_cast<long>(at), p);
  m_slopes.assign(m_points.size() > 1 ? m_points.size() - 1 : 0, 0.0f);
  for (size_t i = 0; i + 1 < m_points.size(); ++i) m_slopes[i] = 1.0f / (m_points[i + 1].x - m_points[i].x);
}
// 8012CE1C
// "(x;y) (x;y) ..." (integers).
void PiecewiseFn::AddPointsFromText(const char* text) {
  const char* s = text;
  for (;;) {
    while (*s && *s != '(') ++s;
    int n = 0;
    while (s[n] && s[n] != ')') ++n;
    if (n == 0 || !s[n]) break;
    ++n;
    int x = 0, y = 0;
    if (std::sscanf(std::string(s, static_cast<size_t>(n)).c_str(), "(%d;%d)", &x, &y) == 2) {
      Point p{static_cast<float>(x), static_cast<float>(y)};
      AddPoint(p);
    }
    s += n;
  }
}
// PC (inline in MotiveEffects::GetCurrentScore / GetInteractionScore): the last point left of `v`,
// then linear to the next one; flat past the ends, 0 without points.
float PiecewiseFn::Eval(float v) const {
  const int count = static_cast<int>(m_points.size());
  if (count == 0) return 0.0f;
  int i = count - 1;
  float d = v - m_points[static_cast<size_t>(i)].x;
  while (d <= 0.0f) {
    if (--i < 0) break;
    d = v - m_points[static_cast<size_t>(i)].x;
  }
  if (i == count - 1) return m_points[static_cast<size_t>(i)].y;
  if (i == -1) return m_points[0].y;
  const Point& a = m_points[static_cast<size_t>(i)];
  return d * m_slopes[static_cast<size_t>(i)] * (m_points[static_cast<size_t>(i) + 1].y - a.y) + a.y;
}

// ---------------------------------------------------------------------------------------------------
// MotiveEffects

// 8011EF40
// The default curves (803DCC3C) unless its definition (+0x66) names a string set of its own
// (MotiveCurveSet::LoadFromFile: no definition on the disc does).
void cXPersonImpl::LoadMotiveEffects() {
  if (!m_motiveEffects) m_motiveEffects = new MotiveEffects(this);
  if (m_def && ERQuickdata::S16(m_def, 0x66) != 0)
    SIMMY_STUB("cXPersonImpl::LoadMotiveEffects: curves from a string set file (MotiveCurveSet::LoadFromFile)", 0x8011EF40);
  else
    m_motiveEffects->LoadFromStringSet(kMotiveCurves, 9);
}

// 800C2F34
// Nine identity curves (-100,-100)..(100,100) over the autonomy motives; LoadMotiveEffects replaces them.
MotiveEffects::MotiveEffects(cXPerson* person) : m_person(person) {
  m_curves.resize(9);
  if (cXObject* o = person ? person->AsObject() : nullptr)
    if (auto* p = dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation())) m_motives = p->m_motives.m_values;
  PiecewiseFn::Point lo{-100.0f, -100.0f}, hi{100.0f, 100.0f};
  for (size_t i = 0; i < m_curves.size(); ++i) {
    m_curves[i].m_motive = kAutonomyMotives[i];
    m_curves[i].m_fn.SetMaxPoints(4);
    m_curves[i].m_fn.AddPoint(lo);
    m_curves[i].m_fn.AddPoint(hi);
  }
}

// 800C2E98 MotiveCurveSet::LoadFromStringSet
void MotiveEffects::LoadFromStringSet(const char* const* texts, int count) {
  for (int i = 0; i < count && i < static_cast<int>(m_curves.size()); ++i) {
    PiecewiseFn& fn = m_curves[static_cast<size_t>(i)].m_fn;
    fn.Reset();
    if (texts[i]) fn.AddPointsFromText(texts[i]);
  }
}

// 800C3058
double MotiveEffects::GetCurrentScore() {
  if (!m_motives || m_curves.empty()) return 0.0;
  float sum = 0.0f;
  for (const Curve& c : m_curves) sum += c.m_fn.Eval(m_motives[c.m_motive]);
  return sum / static_cast<float>(m_curves.size());
}

// 800C31AC
// TTAB ads: entry +4 points at a list (count at -4) of 8-byte {personality ad, min, range, motive}; the
// motive's ad is the one whose +6 names it (TreeTableEntryQuickData::GetAd -> TreeTableEntry::GetAdByID
// 803AF7F0), none: zero. An ad adds its value / 1000 to the motive before the curve.
double MotiveEffects::GetInteractionScore(const unsigned char* entry) {
  if (!entry || !m_motives || m_curves.empty()) return 0.0;
  TTabScratchAds ads;
  ads.CopyFrom(entry);
  return GetInteractionScore(ads);
}

double MotiveEffects::GetInteractionScore(const TTabScratchAds& ads) {
  if (!m_motives || m_curves.empty()) return 0.0;
  float sum = 0.0f;
  for (const Curve& c : m_curves) {
    const float cur = m_motives[c.m_motive];
    float ad = 0.0f;
    const int personality = ads.m_personality[c.m_motive];
    const int min = ads.m_min[c.m_motive], range = ads.m_range[c.m_motive];
    if (personality == 0) {
      ad = static_cast<float>(min + range);  // GetMax
    } else {
      int trait = 0;
      if (personality > 0 && personality < 23 && m_person) {
        trait = m_person->GetPersonData(kPersonalityAds[personality].data);
        if (kPersonalityAds[personality].invert) trait = 1000 - trait;
      }
      ad = static_cast<float>(min) + static_cast<float>(range) * static_cast<float>(trait) / 1000.0f;
    }
    sum += c.m_fn.Eval(static_cast<float>(ad * 0.0010000000474974513 + cur));
    static const bool adTrace = std::getenv("SIMMY_AUTO_TRACE") && !std::strcmp(std::getenv("SIMMY_AUTO_TRACE"), "ads");  // PC dev trace
    if (adTrace && ad != 0.0f) std::printf("[auto]     ad motive %d: %.1f (motive %.1f)\n", c.m_motive, ad, cur);
  }
  return sum / static_cast<float>(m_curves.size());
}

TTabScratchAds gCheckTreeModEntry;   // ObjTestSim::sCheckTreeModEntry (80113390 constructs it)
int gAllowingCheckTreeAds = 0;       // cXPerson::sbAllowingCheckTreeAds

// GC 8014951C TTabScratchEntry::CopyFrom: the ads (SetAd(i, entry->GetAd(i)) for the sixteen motives).
void TTabScratchAds::CopyFrom(const unsigned char* entry) {
  const auto* ads = entry ? static_cast<const unsigned char*>(ERQuickdata::Ptr(entry, 4)) : nullptr;
  const unsigned int count = ads ? ERQuickdata::U32(ads - 4, 0) : 0;
  for (int m = 0; m < 16; ++m) {
    m_personality[m] = m_min[m] = m_range[m] = 0;
    for (unsigned int i = 0; i < count; ++i)
      if (ERQuickdata::S16(ads + i * 8, 6) == m) {
        m_personality[m] = ERQuickdata::S16(ads + i * 8, 0);
        m_min[m] = ERQuickdata::S16(ads + i * 8, 2);
        m_range[m] = ERQuickdata::S16(ads + i * 8, 4);
        break;
      }
  }
}

// ---------------------------------------------------------------------------------------------------
// Objects

// 800E2604
// Not a type 7 object, its own interaction leader, with a tree table entry that is neither manual-only
// nor debug-only. GC: cached in sim flag 8 (UpdateSimFlags), which TryFindBestAction tests.
bool cXObjectImpl::CanChooseAutonomously() {
  if (GetType() == 7) return false;
  if (MultiTile(this) && GroupLead(this) > 0) return false;  // GetInteractionLeader is another part
  const unsigned char* tab = TreeTab(this);
  if (!tab) return false;
  int n = 0;
  for (unsigned int i = 0; i < TreeTabCount(tab); ++i) {
    const unsigned char* e = TreeTabNth(tab, i);
    if (!EntryManualOnly(e) && !EntryDebugOnly(e)) ++n;
  }
  return n != 0;
}

// ---------------------------------------------------------------------------------------------------
// The person

// 8012954C
// Not frozen (object data 0x22), on the lot, and the player's family's (GC: the neighbour's family is
// the player's, vtable 0x9C; a two-player game asks the house, vtable 0x4C), else person data 0x4B's
// player bits (1 << player, 3 for either). PC: the people on the lot with a neighbour are the family's.
bool cXPersonImpl::IsSelectableByPlayer(int player) {
  if (GetData(0x22) != 0 || !InWorld(this)) return false;
  if (m_neighbor) return true;
  const int mask = player != -1 ? 1 << player : 3;
  return (GetPersonData(0x4b) & mask) != 0;
}

// 8012C16C
// From the animator, when an impatient idle ends: the player's interactor gives the sim to autonomy.
void cXPersonImpl::SetIdlePlayerSimAutonomous() {
  for (int player = 0; player < 2; ++player) {
    cXPerson* s = _globals.GetSelectedPerson(player);
    if (!s || s->AsObject() != this) continue;
    if (InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player)) {
      d->SetAutonomous();
      std::printf("[auto] player %d's %s is autonomous\n", player + 1, DebugName());
    }
    return;
  }
}

// 80127114
// Primitive 0x03 (the person main's "SG_try autonomy"): true with my data 0x14 the TTAB entry and the
// frame's stack object the object to use; false when there is nothing (or no autonomy: a player
// steers the sim, free will (global 0x27) is off, or person data 0x24 is 0). Every object that can
// be chosen autonomously (from a random one on) offers its tree table's autonomous entries
// (AppendInteractionsForAuto: not manual or debug only, threshold within data 0x24, for adults, for
// visitors when one); each that tests available (its check tree with param 0 = 1) scores
// (GetInteractionScore - GetCurrentScore) / (distance x attenuation + 1), kept from the family's
// minimum (0.2; visitors 0.05). The best first, one of the best n at random (n from the lowest motive,
// (m + 100) / 20 + 0.75) whose object's prefix check tree and its own check tree pass, and that is
// worth getting up for when seated (0.1). GC also: the check-tree advertisement edits of one object
// (GUID C867A344, TTAB 2 / 10), the "away from the lot" tiles (flag 0x40) for non-primary sims, a
// plumb bob held by the player, story mode (+0xC of DAT_80475C48: wait).
int cXPersonImpl::TryFindBestAction(StackElem* frame) {
  if (!sAutonomyConstantsLoaded) UpdateAutonomyConstants();  // GC: when the global file loads
  auto no = [this](const char* why) {
    if (TraceAutonomy()) std::printf("[auto] %s: no autonomy (%s)\n", DebugName(), why);
    return 0;
  };
  if (!InWorld(this)) return no("not on the lot");
  cSimulatorImpl* sim = Globs_pSimulator;
  if (GetPersonData(0x24) == 0) return no("person data 0x24 is 0");
  if (!(sim && sim->GetGlobal(0x1e) != 0) && IsSelectableByPlayer(-1)) return no("global 0x1E is 0");
  for (int player = 0; player < 2; ++player) {
    cXPerson* s = _globals.GetSelectedPerson(player);
    if (s && s->AsObject() == this) {
      InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player);
      if (d && d->m_autonomous == 0) return no("a player steers it");
    }
  }
  const int freeWill = sim ? sim->GetGlobal(0x27) : 0;
  // GC: the plumb bob over a selected sim (PlumbBobOwner::GetPlumbBob) counts as the player's; PC:
  // a selected sim has one.
  const bool plumbBob = SelectedByPlayer(this);
  if (freeWill == 0 || (SelectedByPlayer(this) && freeWill == -1 && (!plumbBob || sFreeWill == 0))) return no("free will off");

  if (!m_motiveEffects) {  // PC: a person Initialize has not run for
    m_motiveEffects = new MotiveEffects(this);
    LoadMotiveEffects();
  }
  MotiveEffects& effects = *m_motiveEffects;
  const double base = effects.GetCurrentScore();
  const float minScore = GetPersonData(0x20) != 0 ? gMinAutonomyVisitorScore : gMinAutonomyFamilyScore;
  std::vector<ScoredInteraction> list;
  ObjectModuleImpl* module = Globs_pObjectModule;
  const int count = module ? module->GetNumObjects() : 0;
  const int start = count > 0 ? simmy::sim_rand() : 0;
  const bool visitor = GetPersonData(0x20) != 0;
  const int threshold = GetPersonData(0x24);
  for (int k = 0; k < count; ++k) {
    const int id = (start % count + k) % count + 1;
    cXObject* x = module->GetObjectFromID(id);
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    if (!o || o == this || !o->CanChooseAutonomously() || o->GetData(0x19) >= 1) continue;
    // PC: people saved in the house are read as plain objects (ObjectModuleImpl::ConstructObject) and
    // are not people: no one socialises with them.
    if (o->GetType() == 2 && !dynamic_cast<cXPersonImpl*>(o)) continue;
    if (MultiTile(o) && GroupLead(o) >= 0) continue;
    // ObjTestSim::AppendInteractionsForAuto
    const unsigned char* tab = TreeTab(o);
    if (!tab) continue;
    const short prefix = ERQuickdata::S16(tab, 8);
    const double dist = CalcShortDistance(o);
    for (unsigned int i = TreeTabCount(tab); i-- > 0;) {
      const unsigned char* e = TreeTabNth(tab, i);
      const unsigned int flags = ERQuickdata::U32(e, 0x10);
      if ((visitor && !(flags & 1)) || (flags & 0x40) || EntryDebugOnly(e) || ERQuickdata::S16(e, 0x16) > threshold)
        continue;
      Interaction action(this, o, static_cast<int>(i), 2);
      // ObjTestSim::TestInteraction (autonomous): not while it burns or is in use (data 0x19), for
      // adults (GetAvailableToAdults) or ghosts, and the check tree passes with param 0 = 1.
      // The entries IsPermittedToRunCheckTreeForAds allows run their check tree on a copy of their ads
      // (ObjTestSim::sCheckTreeModEntry) it may change (scopes 0x0C, 0x1C, 0x1D, cXPerson::
      // sbAllowingCheckTreeAds); in another room than the person their social ad is 0. They score with
      // the copy.
      const bool modAds = IsPermittedToRunCheckTreeForAds(ERQuickdata::U32(o->m_def, 0x1c), static_cast<int>(i));
      const bool forMe = IsGhost() ? (flags & 0x10) != 0 : !(flags & 0x40);
      if (!forMe) continue;
      if (modAds) gCheckTreeModEntry.CopyFrom(e);  // TTabScratchEntry::CopyFrom
      const short check = ERQuickdata::S16(e, 0);
      short params[4] = {1, 0, 0, 0};
      if (check != 0) {
        if (modAds) gAllowingCheckTreeAds = 1;
        const int passed = m_treeSim->RunCheckTree(o->GetBehavior(), check, static_cast<short>(o->m_id), params);
        gAllowingCheckTreeAds = 0;
        if (passed != 1) continue;
      }
      if (modAds && static_cast<unsigned short>(o->GetData(0x1d)) != static_cast<unsigned short>(GetData(0x1d)))
        gCheckTreeModEntry.SetMax(0xd, 0);  // GetAd(0xD)->SetMax(0)
      const double score = modAds ? effects.GetInteractionScore(gCheckTreeModEntry) : effects.GetInteractionScore(e);
      const double falloff = 1.0f / static_cast<float>(dist * action.m_attenuation + 1.0);
      const float s = score == base ? 0.0f : static_cast<float>(static_cast<float>(score - base) * falloff);
      if (TraceAutonomy())
        std::printf("[auto]   %s #%d \"%s\": score %.4f (base %.3f, distance %.1f, attenuation %.3f)\n", o->DebugName(),
                    o->m_id, Narrow16(ERQuickdata::LocString(e, 0x1c)).c_str(), s, base, dist, action.m_attenuation);
      if (s < minScore) continue;
      ScoredInteraction si;
      si.m_object = static_cast<short>(o->m_id);
      si.m_index = static_cast<int>(i);
      si.m_score = s;
      si.m_autoFirst = flags & 0x100 ? 1u : 0u;
      si.m_behavior = o->GetBehavior();
      si.m_checkTree = check;
      si.m_prefixTree = (flags & 0x400) ? 0 : prefix;  // GetIgnorePrefixTree
      si.m_typeOne = 0;
      si.m_checkedWithAds = 1;  // the check tree ran above
      list.push_back(si);
    }
  }
  if (list.empty()) return no("nothing scores");
  std::stable_sort(list.begin(), list.end(), [](const ScoredInteraction& a, const ScoredInteraction& b) { return a.m_score > b.m_score; });
  if (sUseScalingInteractionRange) {
    float lowest = 100.0f;
    for (int m : kAutonomyMotives)
      if (m != 0xd && m_motives.m_values[m] < lowest) lowest = m_motives.m_values[m];
    gInteractionRandCount = static_cast<int>((lowest + 100.0f) / 20.0f + 0.75f);
    if (gInteractionRandCount < 1) gInteractionRandCount = 1;
  } else {
    gInteractionRandCount = 5;
  }
  std::map<short, int> prefixResults;
  while (!list.empty()) {
    const int n = static_cast<int>(list.size());
    int pick = 0;
    if (list.front().m_autoFirst == 0)
      pick = static_cast<int>(static_cast<unsigned int>(simmy::sim_rand()) % static_cast<unsigned int>(n < gInteractionRandCount ? n : gInteractionRandCount));
    ScoredInteraction& si = list[static_cast<size_t>(pick)];
    int ok = 1;
    if (si.m_prefixTree != 0 && si.m_prefixPassed == 0) {
      auto it = prefixResults.find(si.m_object);
      if (it == prefixResults.end())
        it = prefixResults.emplace(si.m_object, m_treeSim->RunCheckTree(si.m_behavior, si.m_prefixTree, si.m_object, nullptr) == 1).first;
      ok = it->second;
    }
    if (ok) {
      int check = 1;
      if (si.m_checkedWithAds == 0) {
        short params[4] = {static_cast<short>(si.m_typeOne != 0), 1, 0, 0};
        check = m_treeSim->RunCheckTree(si.m_behavior, si.m_checkTree, si.m_object, params) == 1;
      }
      const int idle = GetIdleState();
      if (!check || (idle != 0 && idle != 3 && si.m_score < gMinAutonomySittingScore)) ok = 0;
    }
    if (ok) {
      m_data.WriteVar(0x14, static_cast<short>(si.m_index));
      frame->m_stackObject = si.m_object;
      cXObject* x = module->GetObjectFromID(si.m_object);
      const unsigned char* e = x ? TreeTabNth(TreeTab(x->GetObjectImplementation()), static_cast<unsigned int>(si.m_index)) : nullptr;
      const std::string who = DebugName();
      std::printf("[auto] %s chooses \"%s\" on %s #%d (score %.4f, %d scored, from the best %d)\n", who.c_str(),
                  e ? Narrow16(ERQuickdata::LocString(e, 0x1c)).c_str() : "?", x ? x->GetObjectImplementation()->DebugName() : "?",
                  si.m_object, si.m_score, n, gInteractionRandCount);
      return 1;
    }
    list.erase(list.begin() + pick);
  }
  return 0;
}
