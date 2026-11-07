// Port of game/person.cpp: the person object (construction, Initialize/PostLoad, motives and the mood's
// happy curves, service NPC records, the action queue, its primitives: animate, routing, motive deltas,
// idle for input, ...), the family put on the lot, and PC stand-ins for the action menu. Autonomy is in
// autonomy.cpp, social mode in socialmode.cpp, wants and fears in wantfear.cpp; see person.hpp. Still
// stubbed here: Reset (8011EB78), reaching.
#include "simmy/types_gen.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
// The happy weight curves (cXPersonImpl::sAdultHappyWeightCurves 804866BC, sChildHappyWeightCurves): the
// weight of each of sHappyMotives (80424348) by its value. The child curves are never set up.
struct HappyCurve {
  PiecewiseFn m_fn;
  int m_motive = 0;  // +0x10
};
std::vector<HappyCurve> sAdultHappyWeightCurves, sChildHappyWeightCurves;
bool sHappyCurvesSetup = false;
const int kHappyMotives[7] = {7, 6, 8, 9, 0xf, 0xd, 0xe};  // hunger, comfort, hygiene, bladder, fun, room, social
}  // namespace

// 8011C65C
cXPerson::~cXPerson() {}

// 8011AE04
// The object, then the person's state cleared (GC: field by field; PC: the members' initialisers): the
// motives (Motives::Init), the action queue, the routes, the current room (0xFFFB: none). Its animator
// (SAnimator2) is made here and initialised with the person (GC: Initialize, EGlobal::CreateAnimator).
cXPersonImpl::cXPersonImpl(int id, ObjSelector* selector, ObjectModule* module) : cXObjectImpl(id, selector, module) {
  m_animator = new SAnimator2;
  m_motives.Init();
  m_motives.m_person = this;
  m_motiveDeltas.reserve(2);
  m_spriteSlots.emplace_back(this);  // its sprite slot (balloons over the head)
}

// 8011C77C
cXPersonImpl::~cXPersonImpl() {
  delete m_animator;
  m_animator = nullptr;
  delete m_motiveEffects;
  m_motiveEffects = nullptr;
}

// 8011E0FC
// Person data 0x3A / 0x41 (adult, female), the neighbour's persistent data, its 3D instance (ESim), the
// motive effects and happy curves, the animator, and a want/fear shuffle the neighbour asked for.
// GC also gives the object its hand slot (object data 0x43 = slots - 1; PC: slots are held through
// object data 2/3, and a person's 0x43 stays 0 as in the GC's saves), the sprite renderer and clears the
// ignored object (not kept by the port).
void cXPersonImpl::Initialize() {
  // Person data 0x3A (20 an adult, 10 a child) and 0x41 (1 female), as ReconStream sets them again
  // (PC: without them here the first save stream measured the player's new sim 2 bytes short).
  m_personData[0x3a] = IsAdult() ? 0x14 : 10;
  m_personData[0x41] = IsMale() ? 0 : 1;
  // GC: the neighbour's persistent data comes in PostLoad (and, for a person made out of the world,
  // here): NeighborhoodImpl::LoadPersistentData (a person with no neighbour gets its type's, else a new
  // one: player 2's sim when the family has no second member).
  if (Globs_pNeighborhood) {
    const bool fresh = !m_neighbor && !Globs_pNeighborhood->FindNeighborByType(m_selector);
    Globs_pNeighborhood->LoadPersistentData(this);
    if (fresh && m_neighbor) {
      for (int i = 0; i < 0x50; ++i) m_neighbor->m_pers[i] = m_personData[i];
      for (int i = 0; i < Motives::kCount; ++i) m_neighbor->m_motives[i] = m_motives.m_values[i];
    }
  }
  _globals.AllocPersonInstance(this);
  // Its motive effects (the curves autonomy scores with; LoadMotiveEffects fills them) and the happy
  // weight curves (once).
  if (!m_motiveEffects) {
    m_motiveEffects = new MotiveEffects(this);
    LoadMotiveEffects();  // PC: GC loads them in Reset (8011EB78, not ported) and PostLoad
  }
  if (!sHappyCurvesSetup) InitializeStaticMemory();
  if (m_animator) {
    m_animator->Initialize(this);
    // PC: the animator starts where the person stands, facing its way (GC: the move-in behaviour
    // walks the family in).
    m_animator->SnapToGrid();
    m_animator->setAnimationDirectionToPersonDirection();
  }
  // Its neighbour asked for new wants and fears (flag 0x20 of its data 0x4A, NeedsWantFearShuffle): all
  // seven slots are drawn again.
  if (Neighbor* n = Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(GetPersonData(0x1f)) : nullptr)
    if (n->m_pers[0x4a] & 0x20) {
      n->ShuffleWantFear(-1);
      n->m_pers[0x4a] = static_cast<short>(n->m_pers[0x4a] & ~0x20);
    }
}

// 80125048
// The object's stream, then the person's: (before version 0xB) 28 unused floats; from version 3 the
// animator (SAnimator2::ReconStream), from 4 the motives, from 8 their old values (else cleared); from
// 0xB the person data (0x40 shorts, from 0x29 0x50: the neighbour id 0x1F among them); data 0x3A (20
// an adult, 10 a child) and 0x41 (female) set again; from 0xD the routes (a count: XRoute::DoStream
// keeps nothing); from 0x2B the action queue (before 0x58 a separate current action first, before
// 0x50 one more dropped), its object trees (ObjectRecord: the object, the stack depth, the icon flag);
// from 0x2C the motive deltas; from 0x58 whether the queue's head runs. Before 0x2D some data cleared.
void cXPersonImpl::ReconStream(ReconBuffer* b, int version, bool resize) {
  const bool reading = b->m_mode == ReconBuffer__Mode::kRead;
  cXObjectImpl::ReconStream(b, version, resize);
  if (version <= 10) {
    float unused[0x1c] = {};
    b->ReconFloat(unused, 0x1c);
  }
  if (version > 2) {
    if (m_animator) m_animator->ReconStream(b, version);
    if (version > 3) b->ReconFloat(m_motives.m_values, 0x10);
    if (version < 8) {
      if (reading)
        for (float& v : m_motives.m_old) v = 0.0f;
    } else {
      b->ReconFloat(m_motives.m_old, 0x10);
    }
  }
  if (version > 10) b->Recon16(m_personData, version < 0x29 ? 0x40 : 0x50);
  m_personData[0x3a] = IsAdult() ? 0x14 : 10;
  m_personData[0x41] = IsMale() ? 0 : 1;  // PC: set again in PostLoad, once the description is known
  if (version > 0xc) {
    // DoContainerStream<vector<XRoute>>: the count, then each route's stream (nothing). PC: no routes are
    // made from a save (GC fills the vector with empty ones, which its router replaces).
    int count = static_cast<int>(m_routes.size());
    b->Recon32(&count, 1);
  }
  if (version > 0x2a) {
    if (version < 0x58) {
      Interaction current;
      current.DoStream(b, version);
      if (reading) {
        m_actionQueue.Enqueue(current);
        m_hasCurrentAction = current.m_person != nullptr;
      }
    }
    if (version < 0x50 && reading) {
      Interaction dropped;
      dropped.DoStream(b, version);
    }
    int count = static_cast<int>(m_actionQueue.Count());
    b->ReconInt(&count, 1);
    for (int i = 0; i < count; ++i) {
      if (reading) {
        Interaction fresh;
        m_actionQueue.Enqueue(fresh);
      }
      m_actionQueue.m_actions[(m_actionQueue.m_head + static_cast<unsigned int>(i)) % 10].DoStream(b, version);
    }
    // DoContainerStream<vector<ObjectRecord>>: ObjectRecord::DoStream (8011ACFC).
    int records = static_cast<int>(m_gosubObjects.size());
    b->Recon32(&records, 1);
    if (reading) m_gosubObjects.assign(static_cast<size_t>(records > 0 ? records : 0), GosubObject{});
    for (GosubObject& r : m_gosubObjects) {
      if (Globs_pObjectModule) Globs_pObjectModule->DoReconObject(b, &r.m_object);
      b->ReconInt(&r.m_depth, 1);
      int icon = r.m_icon ? 1 : 0;
      b->ReconBool(&icon);
      r.m_icon = icon != 0;
    }
  }
  if (version > 0x2b) {
    // DoContainerStream<vector<MotiveInc>>: MotiveInc::DoStream (8011A984).
    int count = static_cast<int>(m_motiveDeltas.size());
    b->Recon32(&count, 1);
    if (reading) m_motiveDeltas.assign(static_cast<size_t>(count > 0 ? count : 0), MotiveInc{});
    for (MotiveInc& inc : m_motiveDeltas) {
      b->ReconInt(&inc.m_motive, 1);
      b->ReconFloat(&inc.m_delta, 1);
      b->ReconFloat(&inc.m_limit, 1);
    }
  }
  if (version >= 0x58) b->ReconBool(&m_hasCurrentAction);
  if (version <= 0x2c)
    for (int i : {0x1a, 1, 8, 0x15, 0x16, 0x17, 0x18, 0x19}) m_personData[i] = 0;
}

// 8011F06C
// Before the house is saved: the object's, then its neighbour keeps its persistent data.
void cXPersonImpl::PreSave() {
  cXObjectImpl::PreSave();
  if (Globs_pNeighborhood) Globs_pNeighborhood->SavePersistentData(this);
}

// 8011EC7C
// After the house is read: the neighbour's persistent data (NeighborhoodImpl::LoadPersistentData), the
// object's PostLoad (its load tree), the animator put where the person stands; a placeholder person
// is cleaned up and killed; motive effects; the idles start again and the sim is drawn (not while
// dying). PC: the sim's 3D instance and animator come from Initialize here (GC: the house's
// AllocInstance and cXPersonImpl::Initialize); people made by the PC family move-in have their own
// path (MoveFamilyOntoLot) and are not read from the house.
void cXPersonImpl::PostLoad(int version, bool loaded) {
  // GC: NeighborhoodImpl::LoadPersistentData first; PC: Initialize calls it (the neighbour gives the
  // sim's description, which the 3D instance needs).
  if (Globs_pNeighborhood) Globs_pNeighborhood->LoadPersistentData(this);
  if (!m_desc && m_neighbor && Globs_pNeighborhood) m_desc = Globs_pNeighborhood->SimDescriptionOf(m_neighbor->GetGUID());
  m_personData[0x41] = IsMale() ? 0 : 1;
  if (m_location[0] >= 0 && m_location[1] >= 0)
    SetLocation(static_cast<float>(m_location[1]) / 16.0f, static_cast<float>(m_location[0]) / 16.0f, m_level, GetData(1) & 7);
  Initialize();
  if (std::getenv("SIMMY_RECON_TRACE") && m_neighbor) {
    const CasSimDescriptionS2C* d = Globs_pNeighborhood ? Globs_pNeighborhood->SimDescriptionOf(m_neighbor->GetGUID()) : nullptr;
    char first[32] = {};
    for (int i = 0; d && i < 31 && d->m_firstName[i]; ++i) first[i] = static_cast<char>(d->m_firstName[i]);
    std::fprintf(stderr, "[recon] person %d is neighbour %d (%s), family %d, character %d\n", m_id, m_neighbor->m_id, first,
                 m_personData[0x3d], m_neighbor->IsCharacter() ? 1 : 0);
  }
  cXObjectImpl::PostLoad(version, loaded);
  if (m_animator) {
    m_animator->ForceLocation();
    m_animator->setAnimationDirectionToPersonDirection();
  }
  // A placeholder person (its object file is missing) lets go of everything and is killed.
  if (Globs_pObjectFolder && m_selector == Globs_pObjectFolder->GetPlaceholder() && Globs_pObjectModule) {
    Globs_pObjectModule->CleanupPeople(this);
    Cleanup(nullptr);
    Globs_pObjectModule->AddToKillQueue(static_cast<short>(m_id), true);
  }
  // Old saves (before 0x32) kept relationships to people in the object's own matrix: they go.
  if (version < 0x32 && m_relations) {
    bool removed;
    do {
      removed = false;
      for (int i = 0; i < m_relations->CountKeys(); ++i) {
        cXObject* other = Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(static_cast<short>(m_relations->GetNthKey(i))) : nullptr;
        cXObjectImpl* o = other ? other->GetObjectImplementation() : nullptr;
        if (o && o->GetType() == 2) {
          m_relations->RemoveArray(m_relations->GetNthKey(i));
          removed = true;
          break;
        }
      }
    } while (removed);
  }
  LoadMotiveEffects();
  // A dying person (IsPersonDying) is not drawn; anyone else's idles start again.
  auto* sim = dynamic_cast<ESim*>(m_instance);
  if (!IsPersonDying()) {
    if (m_animator) m_animator->SetIdleInitialized(false);
    if (sim) sim->DrawSim(true);
  } else if (sim) {
    sim->DrawSim(false);
  }
}

// 8012B784
// A frame of its tree stack runs the death tree (0x189): person data 0x4A flag 4 set. 1: dying.
RET_UNKNOWN cXPersonImpl::IsPersonDying() {
  if (!m_treeSim) return 0;
  for (int i = m_treeSim->GetStackSize() - 1; i >= 0; --i) {
    StackElem* e = m_treeSim->GetNthElem(static_cast<short>(i));
    if (e && e->GetTreeID() == 0x189) {
      m_personData[0x4a] = static_cast<short>(m_personData[0x4a] | 4);
      return 1;
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------------------------------
// Motives

// 801253A4
double cXPersonImpl::GetMotive(int index) { return index >= 0 && index < Motives::kCount ? m_motives.m_values[index] : 0.0; }
// 801253B4
float* cXPersonImpl::GetMotiveRef(int index) { return &m_motives.m_values[index & 0xf]; }
// 801253C4
float* cXPersonImpl::GetOldMotiveRef(int index) { return &m_motives.m_old[index & 0xf]; }
// 801253D4
void cXPersonImpl::SetMotive(int index, float value) {
  if (index >= 0 && index < Motives::kCount) m_motives.m_values[index] = value;
}
// 8012C980 cXPersonImpl::GetMotives (inline in the header)
// 8012C988 cXPersonImpl::GetMotiveEffects (inline)
// 8012CA00 cXPersonImpl::GetLastMotiveUpdateTick (inline)
// 8012CA08 cXPersonImpl::SetLastMotiveUpdateTick (inline)

int ServiceNPCKind(const unsigned char* npc) { return npc ? ERQuickdata::S32(npc, 4) : -1; }

// 8012B354
// A person without a sim description whose service NPC record is a dog, cat or monkey (kinds 2..4).
int cXPersonImpl::IsPet() {
  if (GetSimDescription()) return 0;
  const int kind = ServiceNPCKind(GetServiceNPC());
  return kind == 2 || kind == 3 || kind == 4 ? 1 : 0;
}

// 8012B1D4
bool cXPersonImpl::IsDog() { return !GetSimDescription() && ServiceNPCKind(GetServiceNPC()) == 2; }
// 8012B254
bool cXPersonImpl::IsCat() { return !GetSimDescription() && ServiceNPCKind(GetServiceNPC()) == 3; }
// 8012B2D4
bool cXPersonImpl::IsMonkey() { return !GetSimDescription() && ServiceNPCKind(GetServiceNPC()) == 4; }
// 8012B42C
// Only a service NPC (no description) whose record's kind is 1.
bool cXPersonImpl::IsChild() { return !GetSimDescription() && ServiceNPCKind(GetServiceNPC()) == 1; }

// 8012B4AC
// From its description (+0x84); a person without one by its service NPC record (+0: 0 male).
int cXPersonImpl::IsMale() {
  if (const CasSimDescriptionS2C* d = GetSimDescription()) return d->m_male ? 1 : 0;
  // PC: a person with neither (GC would read through a null record) counts as male.
  const unsigned char* npc = GetServiceNPC();
  return !npc || ERQuickdata::S32(npc, 0) == 0 ? 1 : 0;
}

// 8012B1B8
// Its sleep state (motive 0xB, GC +0xD4) below -30.
bool cXPersonImpl::IsSleeping() { return m_motives.m_values[Motives::kSleepState] < -30.0f; }

// 8012B564
// Everyone is an adult (GC: asks the person's object, vtable 0x154, and answers 1 anyway).
RET_UNKNOWN cXPersonImpl::IsAdult() { return 1; }

// 801256EC
double cXPersonImpl::GetAspirationScore() { return (static_cast<float>(GetMotive(0)) + 100.0f) / 200.0f * 1000.0f; }

// 8012574C
void cXPersonImpl::SetAspirationScore(float score) { SetMotive(0, (score / 1000.0f) * 200.0f - 100.0f); }


// 8012971C
// Whether it is player `player`'s primary sim (-1: either player's; player 2 only in two-player games).
RET_UNKNOWN cXPersonImpl::IsPrimarySim(int player) {
  Neighbor* p1 = _gd.m_players[0].GetPrimaryNeighbor();
  Neighbor* p2 = _gd.m_fieldEC ? _gd.m_players[1].GetPrimaryNeighbor() : nullptr;
  if (static_cast<unsigned int>(player + 1) < 2 && p1 && GetPersonData(0x1f) == p1->m_id) return 1;
  if ((player == 1 || player == -1) && p2 && GetPersonData(0x1f) == p2->m_id) return 1;
  return 0;
}

// 801257A8
// 0 red (up to 100 points), 1 green (to 600), 2 gold (to 900), 3 platinum.
int cXPersonImpl::GetAspirationStatus() {
  const double score = GetAspirationScore();
  if (score <= 100.0) return 0;
  if (score <= 600.0) return 1;
  return score > 900.0 ? 3 : 2;
}


// 8011A824
// The adult happy weight curves, once (MotiveCurveSet::LoadFromStringSet over seven point strings).
void cXPersonImpl::InitializeStaticMemory() {
  static const char* const kCurves[7] = {
      "(-100;15) (-60;5) (-40;3) (0;1) (100;1)", "(-100;10) (-80;3) (-40;1) (100;1)", "(-100;5) (-80;3) (-40;1) (100;1)",
      "(-100;10) (-80;3) (-60;1) (100;1)",       "(-100;5) (-40;2) (40;2) (100;5)",   "(-100;2) (0;1) (100;2)",
      "(-100;5) (-40;2) (40;2) (100;3)"};
  if (sHappyCurvesSetup) return;
  sAdultHappyWeightCurves.assign(7, HappyCurve());
  for (int i = 0; i < 7; ++i) {
    sAdultHappyWeightCurves[static_cast<size_t>(i)].m_fn.SetMaxPoints(7);
    sAdultHappyWeightCurves[static_cast<size_t>(i)].m_motive = kHappyMotives[i];
    sAdultHappyWeightCurves[static_cast<size_t>(i)].m_fn.AddPointsFromText(kCurves[i]);
  }
  sHappyCurvesSetup = true;
}

// 80125550
// The mood (motive 3): the happy motives' mean weighted by each one's curve at its value (a low
// motive weighs more). Energy is not one of them; the room (0xD) is.
void cXPersonImpl::CalcHappy() {
  const std::vector<HappyCurve>& curves = IsChild() ? sChildHappyWeightCurves : sAdultHappyWeightCurves;
  float sum = 0.0f, weights = 0.0f;
  for (const HappyCurve& c : curves) {
    const double v = m_motives.m_values[c.m_motive];
    const double w = c.m_fn.Eval(static_cast<float>(v));
    sum = static_cast<float>(v * w + sum);
    weights = static_cast<float>(weights + w);
  }
  // PC: no curves (a child service NPC; GC divides 0 by 0) leaves the mood as it was.
  if (!curves.empty()) m_motives.m_values[Motives::kMood] = sum / weights;
}

// 801253E4
// GC: a motive rising past 90 tells the neighbour's wants (Neighbor::Notify 0xDFBB). Motives run
// unless person data 0x22 is set (and while the sim's room update allows); a frozen sim (+0x42) only
// clamps them.
void cXPersonImpl::SimMotives() {
  // A motive that has come up to 90 tells the wants (event 0xDFBB "motive fulfilled", target the motive;
  // GC: the neighbour by GetNeighborID through the neighbourhood).
  for (int i = 0; i < Motives::kCount; ++i)
    if (m_motives.m_values[i] >= 90.0f && m_motives.m_old[i] < 90.0f && m_neighbor) {
      WantFear__Notification n;
      n.m_event = static_cast<short>(0xdfbb);
      n.m_target = static_cast<short>(i);
      n.m_count = 0;  // WantFear::kGenericParam (80500638)
      m_neighbor->Notify(n, true);
    }
  if (m_data[0x22] != 0) return;
  if (!ObjIsInWorld(this)) return;  // cXObject vtable 0x1AC IsInWorld
  m_motives.Sim();  // GC: a frozen sim (+0x42) has its motives cleaned up instead (vtable 0x84)
}

// 80124D20
// The object's tick, its motives every 60 ticks, the room motive, the tick count (person data 0x1B),
// the motive deltas by the aspiration's multipliers. GC: also the selected sims' motives floored at 0
// in the tutorial, the money balloon.
int cXPersonImpl::Simulate(int ticks) {
  const int r = cXObjectImpl::Simulate(ticks);
  if (!r) return 0;
  // PC development aid: SIMMY_WANTS_NOTIFY=<tick> tells the neighbour of player 1's sim, at that tick,
  // that the event of its first shown want happened (Neighbor::Notify: fulfilment and reward).
  // SIMMY_WANTS_NOTIFY=story:<event hex>@<tick>: from that tick on, every 30 ticks, the want of the story
  // tree is notified (as if done) until the story want is that event (staged story tests); "#<house>"
  // after it: only in that house (NeighborhoodImpl::GetHouseNumber).
  static const char* notify = std::getenv("SIMMY_WANTS_NOTIFY");
  if (notify && std::strncmp(notify, "story:", 6) == 0 && m_neighbor) {
    static const unsigned int until = static_cast<unsigned int>(std::strtoul(notify + 6, nullptr, 16));
    static const char* at = std::strchr(notify, '@');
    static const char* inHouse = std::strchr(notify, '#');
    // "#2/4/3": in those houses, each staged anew (reached once per house).
    static int reachedIn = -1;
    const int house = Globs_pNeighborhood ? Globs_pNeighborhood->GetHouseNumber() : 0;
    bool houseOk = !inHouse;
    for (const char* h = inHouse; h && *h && !houseOk;) {
      ++h;
      houseOk = std::atoi(h) == house;
      h = std::strchr(h, '/');
    }
    const bool reached = reachedIn == house;
    cXPerson* s = _globals.GetSelectedPerson(0);
    if (!reached && houseOk && s && s->AsObject() == this && ticks >= (at ? std::atoi(at + 1) : 0) && ticks % 30 == 0) {
      // The story tree's want bookmark (shown or not yet: Notify advances either).
      for (int b = 0; b < 0xd; ++b) {
        if (!m_neighbor->m_bookmarks[b].Node() || !m_neighbor->m_bookmarks[b].IsInStoryTree()) continue;
        WantFear__Notification n;
        n.m_event = static_cast<short>(simmy::be16(m_neighbor->m_bookmarks[b].Node() + 2));
        n.m_target = static_cast<short>(m_neighbor->m_bookmarks[b].GetTarget());
        n.m_count = static_cast<short>(simmy::be16(m_neighbor->m_bookmarks[b].Node() + 8));  // the node's count (skill levels...)
        // The story's closing want only (no wants after it, fears at most): lot 6's tree also has a
        // "visit lot" before its "help NPC" and dummy unlock.
        bool last = true;
        for (short br = 0; br < m_neighbor->m_bookmarks[b].GetNumBranches(); ++br) {
          const WantFear__Bookmark next = m_neighbor->m_bookmarks[b].GetNextBookmarkAlongBranch(br);
          if (next.Node() && !next.IsFear()) last = false;
        }
        if (static_cast<unsigned short>(n.m_event) == until && last) {
          reachedIn = house;
          std::printf("[wants] test: story want 0x%x reached for %s (house %d)\n", until, m_neighbor->m_name, house);
          break;
        }
        const int answer = m_neighbor->Notify(n, true);
        std::printf("[wants] test: story event 0x%x for %s: Notify -> %d\n", static_cast<unsigned short>(n.m_event),
                    m_neighbor->m_name, answer);
        break;
      }
    }
  } else if (notify && ticks == std::atoi(notify) && m_neighbor) {
    cXPerson* s = _globals.GetSelectedPerson(0);
    const int b = m_neighbor->m_active[0];
    if (s && s->AsObject() == this && b >= 0 && m_neighbor->m_bookmarks[b].Node()) {
      WantFear__Notification n;
      n.m_event = static_cast<short>(simmy::be16(m_neighbor->m_bookmarks[b].Node() + 2));
      n.m_target = static_cast<short>(m_neighbor->m_bookmarks[b].GetTarget());
      const int answer = m_neighbor->Notify(n, true);
      std::printf("[wants] test: event 0x%x for %s: Notify -> %d\n", n.m_event, m_neighbor->m_name, answer);
    }
  }
  if (m_lastMotiveTick + 60 <= ticks) {
    m_lastMotiveTick = ticks;
    SimMotives();
  }
  // The room motive (0xD) is the score of the room it is in (outside: the rooms' outdoor score).
  if (m_currentRoom != 0xfffb) {
    RoomManager* rooms = RoomManager::GetRoomManager();
    Room* room = rooms ? rooms->GetRoom(m_currentRoom) : nullptr;
    m_motives.m_values[0xd] = !room ? 0.0f : room->IsOutside() ? rooms->m_outdoorScore : room->m_field7C;
  }
  // Person data 0x1B counts ticks (to 0x7FFF, then 0): the main loop's "divide tick counts" paces its
  // autonomy checks with it.
  m_personData[0x1b] = static_cast<short>(((m_personData[0x1b] + 1) & 0x8000) ? 0 : m_personData[0x1b] + 1);
  float satisfy = 1.0f, cost = 1.0f;
  switch (GetAspirationStatus()) {
    case 0: satisfy = redAspMotiveSatisfyMult, cost = redAspMotiveCostMult; break;
    case 1: satisfy = greenAspMotiveSatisfyMult, cost = greenAspMotiveCostMult; break;
    case 2: satisfy = goldAspMotiveSatisfyMult, cost = goldAspMotiveCostMult; break;
    case 3: satisfy = platinumAspMotiveSatisfyMult, cost = platinumAspMotiveCostMult; break;
    default: break;
  }
  for (const MotiveInc& inc : m_motiveDeltas) {
    float& m = m_motives.m_values[inc.m_motive & 0xf];
    if (inc.m_delta >= 0.0f) {
      if (inc.m_delta > 0.0f && m < inc.m_limit) {
        m += inc.m_delta * satisfy;
        if (m > inc.m_limit) m = inc.m_limit;
      }
    } else if (m > inc.m_limit) {
      m += inc.m_delta * cost;
      if (m < inc.m_limit) m = inc.m_limit;
    }
  }
  return r;
}

// 8011F0BC
// Operands: the delta's scope and the limit's, the motive (4-9, 0xE, 0xF), flags (1: clear all deltas,
// 6: the object-level primitive), the delta (a value per game hour) and the limit. At most 16.
int cXPersonImpl::TrySetMotiveDelta(StackElem* frame, const unsigned char* params) {
  if (params[3] & 6) return cXObjectImpl::TryElement(frame, 0x1d, params);
  if (params[3] & 1) {
    m_motiveDeltas.clear();
    return 1;
  }
  const int motive = params[2];
  if (motive < 4 || (motive > 9 && motive != 0xe && motive != 0xf)) {
    m_treeSim->SetError(0x1a);
    Error(0x1a);
    return -1;
  }
  short delta = 0, limit = 0;
  if (InterpValue(static_cast<signed char>(params[0]), static_cast<short>(simmy::be16(params + 4)), nullptr, nullptr, &delta) == -1)
    return -1;
  if (InterpValue(static_cast<signed char>(params[1]), static_cast<short>(simmy::be16(params + 6)), nullptr, nullptr, &limit) == -1)
    return -1;
  if (m_motiveDeltas.size() < 0x10)
    m_motiveDeltas.push_back({motive, static_cast<float>(delta) / 60.0f / 30.0f, static_cast<float>(limit)});
  return 1;
}

// 80126A20
// Waits the ticks in param operand +0 (true when they run out; data 8 bit 0x40 cleared), unless the
// sim should give way (ShouldInterrupt): then the queue's head may start (executeInterruptOnIdlePrimitve;
// operand +2 set: any action, else only run-as-sub ones; true ends the wait so the tree can return).
int cXPersonImpl::TryIdleForInput(StackElem* frame, const unsigned char* params) {
  const int interrupt = ShouldInterrupt();
  const int index = static_cast<short>(simmy::be16(params));
  if (index < 0 || index >= frame->m_numParams) {
    m_treeSim->SetError(8);
    Error(8);
    return -1;
  }
  short& ticks = frame->GetParams()[index];
  if (interrupt) return executeInterruptOnIdlePrimitve(frame, &ticks, simmy::be16(params + 2) != 0);
  if (ticks == 0) {
    m_data.WriteVar(8, static_cast<short>(m_data[8] & ~0x40));
    return 1;
  }
  --ticks;
  return 2;
}

// ---------------------------------------------------------------------------------------------------
// The action queue

namespace {
Interaction s_emptyCurrentInteraction;  // 804FDC10

}  // namespace

namespace {
Interaction* sImmediateAction = nullptr;     // 804FDC08 the immediate action running
cXPersonImpl* sImmediatePerson = nullptr;    // 804FDC0C its person
}  // namespace

// 80125980
// An immediate interaction runs its action tree at once to its end (one tick, on my stack with its
// object as the stack object and its stack vars as params) at its priority (person data 0x21, put back
// after unless the tree set it to -100: then 0), when it is available and no other one is running.
RET_UNKNOWN cXPersonImpl::RunImmediateAction(Interaction* action) {
  cXObjectImpl* object = action->m_stackObject ? action->m_stackObject->GetObjectImplementation() : nullptr;
  ObjTestSim test(this, action->m_stackObject);  // its destructor puts my temps back after the tree
  test.TestInteraction(action, false, true, true, nullptr);
  if (!(action->m_flags & Interaction::kAvailable) || sImmediateAction || !object || !m_treeSim) return 0;
  Interaction* const savedAction = sImmediateAction;
  cXPersonImpl* const savedPerson = sImmediatePerson;
  sImmediatePerson = this;
  sImmediateAction = action;
  const short savedPriority = m_personData[0x21];
  m_personData[0x21] = static_cast<short>(action->m_priority);
  m_treeSim->RunOneTickTree(object->GetBehavior(), action->m_tree, static_cast<short>(object->m_id), action->m_stackVars);
  sImmediatePerson = savedPerson;
  sImmediateAction = savedAction;
  m_personData[0x21] = m_personData[0x21] == -100 ? 0 : savedPriority;
  if (action->m_id == 0) action->SetUniqueID();
  return 1;
}

// 80125ADC
// An immediate entry (flag 4) runs at once (RunImmediateAction).
int cXPersonImpl::AddAction(Interaction* action) {
  const unsigned char* e = action->GetEntry();
  if (!e) return 0;
  const unsigned int entryFlags = ERQuickdata::U32(e, 0x10);
  if (entryFlags & 4) return static_cast<int>(RunImmediateAction(action));
  ActionQueue& q = m_actionQueue;
  if (!(entryFlags & 8) && q.Count() != 0) {
    // Not allowed twice in a row: the same entry on the same object as the last one.
    const Interaction& last = q.At(q.Count() - 1);
    if (last.GetIconObject() == action->GetIconObject() && last.m_index == action->m_index) return 0;
  }
  const int priority = action->m_priority;
  action->SetUniqueID();
  const bool head = (action->m_flags & (Interaction::kContinuation | Interaction::kPushToHead | Interaction::kRunAsSub)) != 0;
  // A continuation that carries the name over takes the current action's.
  if ((action->m_flags & Interaction::kContinuation) && (action->m_flags & Interaction::kCarryNameOver))
    action->SetName(GetCurrentAction()->GetName());
  const unsigned int first = m_hasCurrentAction != 0 ? 1u : 0u;
  const int visible = q.CountVisible();
  if (5 < visible) {
    bool full = true;
    if (!head) {
      int lower = 0;
      for (unsigned int i = first; i < q.Count(); ++i) {
        const Interaction& a = q.At(i);
        if (a.m_priority < priority && !(a.m_flags & Interaction::kMustRun) && q.IsVisibleAction(a)) ++lower;
      }
      if (visible - lower < 6) full = false;
    }
    if (full) {
      if (!(action->m_flags & Interaction::kMustRun)) return 0;
      action->m_flags |= Interaction::kHidden;
    }
  }
  if (action->m_flags & Interaction::kRunAsSub) return q.Insert(*action, 0);
  if (head) return q.Insert(*action, first);
  // Queued actions of lower priority give way.
  for (unsigned int i = first; i < q.Count();) {
    Interaction& a = q.At(i);
    if (a.m_priority < priority && !(a.m_flags & Interaction::kMustRun)) ActionSkipped(a);
    else ++i;
  }
  // An action my immediate action pushes takes its place (GC: *sImmediateAction = *action, the name
  // carried over with flag 0x40).
  if (sImmediateAction && sImmediatePerson == this) {
    if (action->m_flags & Interaction::kCarryNameOver) action->SetName(sImmediateAction->GetName());
    *sImmediateAction = *action;
  }
  q.Enqueue(*action);
  return 1;
}

// 80125E64
int cXPersonImpl::RemoveAction(int id) {
  ActionQueue& q = m_actionQueue;
  unsigned int i = 0;
  Interaction* found = nullptr;
  for (; i < q.Count(); ++i)
    if (q.At(i).m_id == id) {
      found = &q.At(i);
      break;
    }
  if (!found) return 0;
  if (i == 0 && m_hasCurrentAction != 0) {
    const unsigned int flags = found->m_flags;
    found->m_flags = flags | Interaction::kCancelled;
    if (!(flags & Interaction::kMustRun)) {
      SetPersonData(0x21, 0);
      if (!ShouldInterrupt()) {
        // Nothing queued takes over: the person's own "stop" entry (0x21) at priority 0x32.
        Interaction stop(this, this, 0x21, 0x32);
        stop.SetIconObject(nullptr);
        AddAction(&stop);
      }
    }
  } else {
    if (found->m_flags & Interaction::kMustRun) {
      found->m_flags |= Interaction::kCancelled;
      return 1;
    }
    ActionSkipped(*found);
  }
  return 1;
}

// 8012606C
// The last queued action, or the running one when nothing is queued, is removed (RemoveAction, vtable
// 0xBC), unless the action before the last has flag 0x8000 (it must finish first). 1: removed.
int cXPersonImpl::CancelLastAction() {
  const int queued = CountActions(false);
  int id;
  int blocker = -1;
  if (queued < 1) {
    id = GetCurrentAction()->m_id;
  } else {
    id = GetIndAction(queued - 1, false)->m_id;
    const Interaction* before = queued < 2 ? GetCurrentAction() : GetIndAction(queued - 2, false);
    if (before->m_flags & 0x8000) blocker = before->m_id;
  }
  return blocker == -1 ? RemoveAction(id) : 0;
}

// 80126194
// The queued actions from the last, then the running one. 1: all removed.
int cXPersonImpl::CancelAllActions() {
  for (int i = CountActions(false) - 1; i >= 0; --i) {
    Interaction* a = GetIndAction(i, false);
    if (!a || !RemoveAction(a->m_id)) return 0;
  }
  const Interaction current(*GetCurrentAction());
  return RemoveAction(current.m_id) ? 1 : 0;
}

// 801262B0
int cXPersonImpl::CancelAllButLastActions() {
  for (int i = CountActions(false) - 1; i >= 0; --i) {
    Interaction* a = GetIndAction(i, false);
    if (!a || !RemoveAction(a->m_id)) return 0;
  }
  return 1;
}

// 80126360
Interaction* cXPersonImpl::GetIndAction(int index, bool includeCurrent) {
  if (!includeCurrent && m_hasCurrentAction != 0) ++index;
  if (index < 0 || static_cast<unsigned int>(index) >= m_actionQueue.Count()) return nullptr;
  return &m_actionQueue.At(static_cast<unsigned int>(index));
}

// 801263C8
Interaction* cXPersonImpl::GetCurrentAction() {
  if (m_hasCurrentAction != 0) return &m_actionQueue.At(0);
  return &s_emptyCurrentInteraction;
}

// 8012C6E4
int cXPersonImpl::CountActions(bool includeCurrent) {
  const int n = static_cast<int>(m_actionQueue.Count());
  if (includeCurrent || m_hasCurrentAction == 0) return n;
  return n - 1;
}

// 80125AD0 cXPersonImpl::IsIdle (inline in the header)

// 801264C8
int cXPersonImpl::ShouldInterrupt() {
  if (GetPersonData(0x47) != 0) return 0;
  ActionQueue& q = m_actionQueue;
  int r = 0;
  if (m_hasCurrentAction != 0) {
    const Interaction* cur = GetCurrentAction();
    if (cur->m_flags & Interaction::kCancelled) r = (cur->m_flags & Interaction::kMustRun) ? 0 : 1;
  }
  if (r == 0) {
    const unsigned int first = m_hasCurrentAction != 0 ? 1u : 0u;
    if (first < q.Count()) {
      const Interaction& next = q.At(first);
      if (GetPersonData(0x21) < next.m_priority || (next.m_flags & Interaction::kContinuation)) r = 1;
    }
    if (r == 0) {
      // A motive in failure (hunger, bladder, energy below -99).
      if (m_motives.m_values[7] < -99.0f || m_motives.m_values[9] < -99.0f || m_motives.m_values[5] < -99.0f) r = 1;
      if (r == 0 && q.Count() != 0) r = (q.At(0).m_flags & Interaction::kRunAsSub) ? 1 : 0;
    }
  }
  return r;
}

// 80126B64
int cXPersonImpl::executeInterruptOnIdlePrimitve(StackElem* frame, short* ticks, bool noSub) {
  m_data.WriteVar(8, static_cast<short>(m_data[8] | 0x40));
  ActionQueue& q = m_actionQueue;
  if (q.Count() == 0) return 1;
  Interaction& head = q.At(0);
  if (frame->m_primState == 1 && !(head.m_flags & Interaction::kContinuation)) return 1;
  if (!(head.m_flags & Interaction::kRunAsSub)) {
    if (!noSub) return 1;
    if (m_hasCurrentAction != 0) {
      m_treeSim->SetError(0x52);
      Error(0x52);
      return -1;
    }
  }
  cXObjectImpl* object = head.m_stackObject ? head.m_stackObject->GetObjectImplementation() : nullptr;
  if (!object) {
    q.Remove(0);
    return 2;
  }
  const int wasVisible = q.IsVisibleAction(head);
  ObjTestSim test(this, object);  // its destructor puts my temps back (after the gosub)
  test.TestInteraction(&head, false, true, true, nullptr);
  if ((head.m_flags & Interaction::kAvailable) && head.m_tree != 0 &&
      GosubObjectTree(head.m_stackObject, head.m_stackVars, head.m_tree, false)) {
    if (!(head.m_flags & Interaction::kContinuation) && !wasVisible && q.IsVisibleAction(head)) q.UpdateAction(0);
    m_hasCurrentAction = 1;
    UpdateCurrentAction();
    *ticks = 0;
    frame->m_primState = 1;
    m_data.WriteVar(8, static_cast<short>(m_data[8] | 0x40));
    const std::string who = DebugName();
    const unsigned short* name = head.NameText();
    std::string text;
    for (; name && *name && text.size() < 60; ++name) text += *name < 0x80 ? static_cast<char>(*name) : '?';
    std::printf("[sim] %s runs \"%s\" on %s (priority %d)\n", who.c_str(), text.c_str(), object->DebugName(),
                head.m_priority);
    return 3;
  }
  ActionSkipped(head);
  return 2;
}

// 80126E58
// Steered by its player (animator flag 1, the interactor steering): true at once. Else the animator's
// idle (SAnimator2::TryIdleAnimate); then by the count mode (+6): 0 true; else the count (param +3 of
// the frame, or the literal +7 with flag 0x10) runs down a tick (1) or a loop of the skill track (2,
// restarted) at a time, waiting until it is out (true, my data 8 bit 0x40 cleared).
int cXPersonImpl::TryIdleAnimate(StackElem* frame, const unsigned char* params) {
  const int mode = static_cast<signed char>(params[6]);
  if (!m_instance || !m_animator) return 2;  // IsModelLoaded
  if (m_animator->m_flags & 1) {
    for (int player = 0; player < 2; ++player) {
      cXPerson* s = _globals.GetSelectedPerson(player);
      if (!s || s->AsObject() != this) continue;
      InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player);
      if (d && d->m_controlStatus == 1) return 1;
    }
  }
  const int r = m_animator->TryIdleAnimate(frame, params);
  if (r == 2) return 2;
  if (r == -1) return -1;
  if (mode == 0) return 1;
  if (m_idleAnimateCount == -0x8000) {
    if (!(params[5] & 0x10)) {
      const int index = static_cast<signed char>(params[3]);
      const bool ok = (params[5] & 2) ? index > 0 : index >= 0;
      if (!ok || index >= frame->m_numParams) {
        m_treeSim->SetError(8);
        Error(8);
        return -1;
      }
      m_idleAnimateCount = frame->GetParams()[index];
    } else {
      m_idleAnimateCount = params[7];
    }
  }
  if (m_idleAnimateCount == 0) {
    m_data.WriteVar(8, static_cast<short>(m_data[8] & ~0x40));
    m_idleAnimateCount = static_cast<short>(0x8000);
    return 1;
  }
  if (mode == 1) {
    --m_idleAnimateCount;
  } else if (mode == 2 && m_animator->m_sim && m_animator->m_sim->m_anim.IsTrackAnimComplete(eTrackFlags{0x10000})) {
    --m_idleAnimateCount;
    m_animator->m_sim->m_anim.RestartTrack(eTrackFlags{0x10000});
  }
  return 2;
}

// 8011F460
int cXPersonImpl::TryGosubFoundAction(StackElem* frame) {
  if (frame->m_stackObject == 0) {
    m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  cXObject* object = GetObjectFromID(frame->m_stackObject);
  if (!object) return 0;
  const short index = GetData(0x14);
  Interaction probe(this, object, index, 2);
  const unsigned char* e = probe.GetEntry();
  if (!e) {
    m_treeSim->SetError(0x16);
    Error(0x16);
    return -1;
  }
  short vars[4] = {0, 0, 0, 0};
  if (!GosubObjectTree(object, vars, ERQuickdata::S16(e, 2), false)) return -1;
  probe.SetStackVars(vars);
  SetCurrentAction(probe);
  return 3;
}

// 80129ECC
void cXPersonImpl::SetCurrentAction(Interaction& action) {
  m_hasCurrentAction = 1;
  m_actionQueue.Insert(action, 0);
  UpdateCurrentAction();
}

// 80129F10
// The person's data follow the current action: 0x21 its priority (0 for none); with an object 0x26 /
// 0x27 cleared; 0x47 / 0x48 cleared, 0x49 (a seat left, GC: its footprint recomputed) cleared; the
// motive deltas end.
void cXPersonImpl::UpdateCurrentAction() {
  Interaction cur;
  if (m_hasCurrentAction != 0 && m_actionQueue.Count() != 0) cur = m_actionQueue.At(0);
  if (cur.GetStackObject()) {
    SetPersonData(0x27, 0);
    SetPersonData(0x26, 0);
  }
  SetPersonData(0x21, static_cast<short>(cur.m_priority));
  m_motiveDeltas.clear();
  if (GetPersonData(0x49) != 0) SetPersonData(0x49, 0);
  SetPersonData(0x47, 0);
  SetPersonData(0x48, 0);
}

// 8012AC7C
void cXPersonImpl::CompleteCurrentAction() {
  if (m_hasCurrentAction == 0) return;
  m_hasCurrentAction = (m_actionQueue.At(0).m_flags & Interaction::kRunAsSub) ? 1 : 0;
  m_actionQueue.Remove(0);
  if (m_actionQueue.Count() == 0 && m_hasCurrentAction != 0) m_hasCurrentAction = 0;
  UpdateCurrentAction();
}

// 8012A04C
// `object` is going (null: everything is): a person running one of its trees on the stack (a gosub
// object) or doing an action with it as the queue icon lets go: its stack back to the main tree, its
// animation stopped, data 0x22 / 9 cleared, the current action completed. Then the queued actions on
// `object` (stack or icon object) go; the current one is completed, or loses its icon. Misc flag 0x800
// guards against cleaning up twice. GC: also the selected person's interactor told (social mode left,
// the player's input reset), the routes' slots and the sprite slot, a person off the world put back
// (FindGoodLocation from its tile), the gosub objects told (the object module's vtable 0x134), and
// the "follow" records (+0x40C, 0xA4 each) on a person cleared: not ported (PC: those parts are not kept).
void cXPersonImpl::Cleanup(cXObject* object) {
  if (GetMiscFlag(cXObject__MiscFlag{0x800})) {
    std::printf("[sim] already cleaning up %s\n", DebugName());
    return;
  }
  SetMiscFlag(cXObject__MiscFlag{0x800}, true);
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  bool involved = object == nullptr;
  for (const GosubObject& g : m_gosubObjects)
    if (!object || g.m_object == impl) involved = true;
  if (!involved && m_hasCurrentAction && m_actionQueue.Count() && m_actionQueue.At(0).GetIconObject() == object)
    involved = true;
  if (involved) {
    m_gosubObjects.clear();
    if (m_animator) m_animator->stopCurAnim(false);
    m_data.WriteVar(0x22, 0);
    m_data.WriteVar(9, 0);
    if (m_treeSim) m_treeSim->Reset(GetBehavior(), GetTreeID(1));
    CompleteCurrentAction();
    const std::string who = DebugName();
    std::printf("[sim] %s lets go of %s (cleanup)\n", who.c_str(), impl ? impl->DebugName() : "everything");
  }
  if (!object) {
    while (m_actionQueue.Count()) m_actionQueue.Remove(0);
    m_hasCurrentAction = 0;
  } else {
    unsigned int i = 0;
    if (m_hasCurrentAction && m_actionQueue.Count()) {
      Interaction& cur = m_actionQueue.At(0);
      if (cur.GetStackObject() == object) {
        CompleteCurrentAction();
      } else {
        if (cur.GetIconObject() == object) cur.SetIconObject(nullptr);
        i = 1;
      }
    }
    while (i < m_actionQueue.Count()) {
      const Interaction& a = m_actionQueue.At(i);
      if (a.GetStackObject() == object || a.GetIconObject() == object) m_actionQueue.Remove(i);
      else ++i;
    }
  }
  SetMiscFlag(cXObject__MiscFlag{0x800}, false);
}

// 8012AD28
void cXPersonImpl::DeleteTopAction() {
  const unsigned int first = m_hasCurrentAction != 0 ? 1u : 0u;
  if (first < m_actionQueue.Count()) ActionSkipped(m_actionQueue.At(first));
}

// 8012AD98
// Out of the queue; GC: its object is told (cXObject vtable 0x104, message 4 with my id; not ported).
void cXPersonImpl::ActionSkipped(Interaction& action) {
  for (unsigned int i = 0; i < m_actionQueue.Count(); ++i)
    if (&m_actionQueue.At(i) == &action) {
      m_actionQueue.Remove(i);
      break;
    }
}

// 801266CC
// Operands: +0 the suit, +1 where from (1 an outfit: the person's data 8, put on by the animator; 0 / 2
// a dress-up prop from the global / the tree's own prop table), +2 flags (1 take the prop off, 2 only
// preload it; for an outfit, 2: do nothing). An outfit is waited for (the ESim's change: the shower
// curtain, the new models). Service NPCs keep their look.
int cXPersonImpl::TryChangeSuit(StackElem* frame, const unsigned char* params) {
  const unsigned char suit = params[0], source = params[1];
  const unsigned short flags = static_cast<unsigned short>(params[2] << 8 | params[3]);
  if (source == 1) {
    if ((flags & 2) || GetServiceNPC()) return 1;
    if (GetPersonData(8) != suit) SetPersonData(8, static_cast<short>(suit));
    return m_animator ? m_animator->TryChangeSuit() : 1;
  }
  if (source > 2) {
    m_treeSim->SetError(0x30);
    Error(0x30);
    return -1;
  }
  // The global or the tree's namespace's prop table 0x130 (Behavior::GetGlobNamespaceID /
  // GetNamespaceID; PC: their ResFile rows), its prop `suit` (none: error 0x31), then the animator's
  // Undress (flag 1), PreloadDress (flag 2; false while it cannot) or Dress: true.
  Behavior* behavior = frame ? frame->m_behavior : nullptr;
  const unsigned char* file = behavior ? (source == 0 ? behavior->m_globalResFile : behavior->m_resFile) : nullptr;
  PropTable* table = PropTable::CreateInstance();
  table->Load(file, 0x130);
  const PropRef* prop = table->GetEntry(suit);
  if (!prop) {
    PropTable::DestroyInstance(table);
    m_treeSim->SetError(0x31);
    Error(0x31);
    return -1;
  }
  int result = 1;
  if (m_animator) {
    if (flags & 1) m_animator->Undress(prop);
    else if (flags & 2) result = m_animator->PreloadDress(prop) ? 1 : 0;
    else m_animator->Dress(prop);
  }
  PropTable::DestroyInstance(table);
  return result;
}

// 8011F654
// Operand +0: 0 nothing, 1 turn to the camera, 2 turn to the stack object, 3 turn away from it; only
// standing (idle state 0 or 3; else false). The destination list becomes my spot twice and the point
// looked at (the camera: 100 tiles along its turn, EGlobal +0x164), object data 9 cleared, and the
// animator follows it (BeginFollow: a three-point list on the spot side-steps nowhere, then turns along
// the last leg to the nearest eighth); the result once the follow ends.
int cXPersonImpl::TryLookTowards(StackElem* frame, const unsigned char* params) {
  if (!m_instance || !m_animator) return 2;  // IsModelLoaded
  const short mode = static_cast<short>(params[0] << 8 | params[1]);
  if (mode == 0) return 1;
  if (mode < 0 || mode > 3) {
    m_treeSim->SetError(0x27);
    Error(0x27);
    return -1;
  }
  if (frame->m_primState == 0) {
    if (GetIdleState() != 0 && GetIdleState() != 3) return 0;
    TileListPoint me, target;
    me.p[0] = m_location[0];
    me.p[1] = m_location[1];
    if (mode == 1) {
      float yaw = 0.0f;
      std::memcpy(&yaw, &_globals.m_field164, sizeof yaw);
      const float a = (yaw + 45.0f) * 0.017453292f;
      target.p[0] = static_cast<int>(-std::cos(a) * 100.0f) * 0x10 + me.p[0];
      target.p[1] = static_cast<int>(-static_cast<double>(std::sin(a)) * 100.0) * 0x10 + me.p[1];
    } else {
      cXObject* o = GetObjectFromID(frame->m_stackObject);
      cXObjectImpl* t = o ? o->GetObjectImplementation() : nullptr;
      if (!t) {
        m_treeSim->SetError(0x15);
        Error(0x15);
        return -1;
      }
      target.p[0] = t->m_location[0];
      target.p[1] = t->m_location[1];
      if (mode == 3) {
        target.p[1] = target.p[1] + (target.p[1] - me.p[1]) * -2;
        target.p[0] = target.p[0] + (target.p[0] - me.p[0]) * -2;
      }
    }
    m_path.m_points.clear();
    m_path.m_points.push_back(me);
    m_path.m_points.push_back(me);
    m_path.m_points.push_back(target);
    SetData(9, 0);
    m_animator->BeginFollow(0.0f);
    frame->m_primState = 1;
  }
  const int r = m_animator->FollowOneStep();
  if (r == 2) return 2;
  if (m_animator->EndFollow()) return r;
  return 2;
}

// 80124ADC
// The person's own primitives, else the object's.
int cXPersonImpl::TryElement(StackElem* frame, short op, const unsigned char* params) {
  switch (op) {
    case 0x03: return TryFindBestAction(frame);
    case 0x06: return TryChangeSuit(frame, params);
    case 0x11: return TryIdleForInput(frame, params);
    case 0x16: return TryLookTowards(frame, params);
    case 0x1b: return TryGotoRelative(frame, params);
    case 0x1d: return TrySetMotiveDelta(frame, params);
    case 0x1e: return TryGosubFoundAction(frame);
    case 0x25: return TryTestInteractingWith(frame);
    case 0x26:
    case 100: return -1;
    case 0x2c: {
      // The kinds 7 and 8 (operand +3 = 4, 5) are the object's passive-influence animations.
      if (params[3] == 4 || params[3] == 5) return cXObjectImpl::TryAnimate(frame, params);
      return m_animator ? m_animator->TryAnimate(frame, params) : 1;
    }
    case 0x2d: return TryGotoRoutingSlot(frame, params);
    case 0x2f: SIMMY_STUB("primitive 0x2f cXPersonImpl::TryReach", 0x801249F4); return 1;
    case 0x3e: return TrySocialMode(frame, params);
    case 0x40: return TryIdleAnimate(frame, params);
    default: return cXObjectImpl::TryElement(frame, op, params);
  }
}

// ---------------------------------------------------------------------------------------------------
// Routing

namespace {
bool TraceRoutes() {
  static const bool on = std::getenv("SIMMY_SIM_TRACE") != nullptr || std::getenv("SIMMY_ROUTE_TRACE") != nullptr;
  return on;
}
cXObjectImpl* ImplOf(cXObject* o) { return o ? o->GetObjectImplementation() : nullptr; }
// ResourceBehavior::GetTreeIDByName (800ACA38): the behaviour's tree of that name (its own file's, then
// the semi-global and the global ones); 0 for none.
short RouteTreeByName(Behavior* b, const char* name) {
  if (!b) return 0;
  for (short cls : {static_cast<short>(0x1000), static_cast<short>(0x2000), static_cast<short>(0x100)})
    if (EREdithTreeSet* set = b->TreeSetFor(cls))
      for (const EREdithTreeSet::Tree& t : set->m_trees)
        if (t.m_name && std::strcmp(t.m_name, name) == 0) return static_cast<short>(t.m_id);
  return 0;
}
int SimTicks() { return Globs_pSimulator ? Globs_pSimulator->GetTicks() : 0; }
// The people of the lot (ObjectModuleImpl::GetPeople).
std::vector<cXPersonImpl*> LotPeople() {
  std::vector<cXPersonImpl*> out;
  if (!Globs_pObjectModule) return out;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
    if (auto* p = dynamic_cast<cXPersonImpl*>(o)) out.push_back(p);
  return out;
}
const char* const kStateNames[] = {"new",      "plan",     "rooms",     "follow",   "seat",   "stood", "failed",
                                   "fail",     "done",     "arrived",   "lost",     "endfail", "begin", "wait",
                                   "asking",   "endplan",  "feedback",  "fedback",  "init",   "gone",  "endgone"};
}  // namespace

// 801203A8
// GC: the slot's flag 0x4000 is set on the object's own slot; PC: on a copy.
int cXPersonImpl::TryGotoRoutingSlot(StackElem* frame, const unsigned char* params) {
  if (!m_instance) return 0;  // IsModelLoaded
  if (frame->m_primState != 0) return TryGotoRoutingSlot(frame, static_cast<RoutingSlot*>(nullptr));
  cXObjectImpl* object = static_cast<cXObjectImpl*>(GetObjectFromID(frame->m_stackObject));
  if (!object) {
    m_treeSim->SetError(0x35);
    Error(0x35);
    return -1;
  }
  int index = static_cast<short>(simmy::be16(params));
  const int kind = static_cast<short>(simmy::be16(params + 2));
  const std::vector<RoutingSlot>& slots =
      kind == 2 ? GlobalRoutingSlots() : ObjectRoutingSlots(object->GetObjectImplementation());
  if (kind == 0) {
    if (index < 0 && index >= -static_cast<int>(frame->m_numParams)) {
      m_treeSim->SetError(8);
      Error(8);
      return -1;
    }
    index = index >= 0 && index < frame->m_numParams ? frame->GetParams()[index] : -1;
  } else if (kind != 1 && kind != 2) {
    index = -1;
  }
  if (index < 0 || index >= static_cast<int>(slots.size())) {
    m_treeSim->SetError(0x25);
    Error(0x25);
    return -1;
  }
  RoutingSlot slot = slots[static_cast<size_t>(index)];
  slot.m_flags &= ~static_cast<unsigned int>(RoutingSlot::kFailureTrees);
  if (!(params[4] & 1)) slot.m_flags |= RoutingSlot::kFailureTrees;
  return TryGotoRoutingSlot(frame, &slot);
}

// 80123704
XRoute* cXPersonImpl::GetCurrentRoute() { return m_routes.empty() ? nullptr : &m_routes.back(); }

// 8012C878
bool cXPersonImpl::IsRouting() { return !m_routes.empty(); }

// 8012C9D8 cXPersonImpl::GetRouteStackSize (inline in the header)
// 8012CA10 cXPersonImpl::IgnoringObject (inline)

// 8012CA18
void cXPersonImpl::SetIgnoringObject(cXObject* object) { m_ignoringObject = object; }

// 8012CA20
void cXPersonImpl::ClearIgnoringObject() { m_ignoringObject = nullptr; }

// 8012B5A0
// Each route plans again (from its state 2 / 1) at its next tick.
void cXPersonImpl::InvalidateRoutes() {
  for (XRoute& r : m_routes) r.m_planned = 0;
}

// 801298E8
void cXPersonImpl::ForceLocation() {
  if (m_animator) m_animator->ForceLocation();
  SetPersonData(0x40, 0);
}

// 80120640
// Routes to `slot` of the frame's stack object over several ticks, a state machine on the frame's
// primitive state (1 there, 0 failed, 2 on its way, 3 a tree was pushed: the stand-up / sit / portal /
// waiting / failure trees, after which it goes on). A new route (state 0) is pushed on the person's
// route stack (a person routing to itself, facing it, at no distance is there at once). Then: to
// another room the best portal's tree walks it through (TryRoomRouting; a portal tree that failed marks
// that portal failed; routing to a portal it is already going through fails); in the room, a seated
// person whose goal is not her seat stands up first (the seat's entry 0x1B); then the goals and the
// path (InitRoute), which the animator follows (BeginFollow / FollowOneStep, which places the person at
// the end); a goal that is a seat is sat in (its entry 0x1A). A failed path: if the person moved since
// the last try it tries again; else it asks a person in the way to move (AskOthersToMove: the "ask to
// move" animation, then waiting up to 300 ticks with the "waiting" one) and tries once more; then it
// fails: the target's "route failure feedback" tree runs when the slot allows failure trees (flag
// 0x4000). A person who should give way (ShouldInterrupt, and the animator allows it) fails with 4. At
// the end the person's data tell the tree how it went: 0x3E the failure code (0: there), 0x25 the side
// of the object the goal is on, 0x27 set after a single route when 0x26 is clear; object data 9 is
// cleared, and a route that had asked this person to move is told whether it did.
int cXPersonImpl::TryGotoRoutingSlot(StackElem* frame, RoutingSlot* slot) {
  bool gone = false;
  double dist = -1.0;
  cXObjectImpl* object = ImplOf(GetObjectFromID(frame->m_stackObject));
  int state = frame->m_primState;
  if (!object || !ObjIsInWorld(object)) gone = true;
  if (state == 0) {
    if (gone) return 0;  // GC: also asks a found object IsPortal (vtable 0x44), unused
    XRoute route(this, object, slot);
    if (IsGhost()) route.GetRoutingSlot()->m_flags |= RoutingSlot::kIgnoreRooms;
    if (m_routes.empty() || (route.GetRoutingSlot()->m_flags & RoutingSlot::kFailureTrees)) {
      if (m_routes.empty() && GetData(0x11) == 0 && (dist = CalcShortDistance(object)) == 0.0 && route.m_object == route.m_person &&
          slot && slot->m_facing == RoutingSlot::kFaceTowards)
        return 1;
    } else {
      // A route within a route has no failure trees.
      route.GetRoutingSlot()->m_flags &= ~static_cast<unsigned int>(RoutingSlot::kFailureTrees);
    }
    if (m_routes.empty() && m_animator) m_animator->m_portalMode = 0;  // +0x5F8
    m_routes.push_back(route);
    state = 1;
    if (TraceRoutes())
      std::printf("[route] %s -> %s: route %zu (slot flags %x, facing %d, dist %d..%d opt %d)\n", std::string(DebugName()).c_str(),
                  object->DebugName(), m_routes.size(), route.m_slot.m_flags, route.m_slot.m_facing, route.m_slot.m_minDist,
                  route.m_slot.m_maxDist, route.m_slot.m_optimalDist);
  }
  XRoute* route = GetCurrentRoute();
  if (!route) return 0;
  if (!route->m_object) {
    if (m_animator) m_animator->IsFollowing();
    frame->m_primState = 0;
    m_routes.pop_back();
    return 2;
  }
  if (gone) state = state == 3 ? 0x14 : 0x13;
  if (route->m_planned == 0) {
    if (state == 2) route->m_portal = nullptr;
    else if (state == 3) state = 0xf;
    route->m_planned = 1;
  }
  RoutingSlot* rslot = route->GetRoutingSlot();
  int result = 2;
  int lastPos[2] = {route->m_personPos[0], route->m_personPos[1]};
  bool done = false;
  int stuck = static_cast<short>(route->m_stuck);
  SAnimator2* anim = m_animator;
  if (!anim) return 0;
  const int startState = state;
  for (int guard = 0;; ++guard) {
    if (guard > 64) state = 7;  // PC: a guard against a state loop (none is known)
    switch (state) {
      case 1:  // plan
        if (!(GetData(8) & 8)) {
          state = 7;
          break;
        }
        if (!IsGhost() && object->GetData(0x1d) != GetData(0x1d)) {
          // To another room. Routing to a portal this person is already going through fails.
          state = 2;
          if (object->GetType() == 8)
            for (XRoute& r : m_routes)
              if (ImplOf(r.m_portal) == object) state = 7;
          break;
        }
        if (ShouldInterrupt() && anim->IsInterruptable()) {
          state = 7;
          route->m_failure = 4;
          break;
        }
        state = 0x12;
        route->BuildGoalList();
        if (GetIdleState() == 1) {
          state = 8;
          if (!route->IsPersonSittingOnChairGoal(this)) {
            state = 7;
            cXObjectImpl* seat = ImplOf(GetContainer());
            if (!seat || !seat->IsChair()) {
              m_treeSim->SetError(0x35);
              Error(0x35);
              return -1;
            }
            const short stand = seat->GetTreeID(0x1b);
            if (stand != 0) {
              const short params[2] = {static_cast<short>(route->m_startDirs), 0};
              if (GosubObjectTree(seat, params, stand, false)) {
                state = 5;
                SetPersonData(0x48, 0);
                result = 3;
                SetPersonData(0x25, static_cast<short>(route->m_startDirs));
                done = true;
              }
            }
          }
        }
        break;
      case 2:  // to another room
        if (ShouldInterrupt() && anim->IsInterruptable()) {
          route->m_failure = 4;
          state = 7;
          break;
        }
        if (object->GetData(0x1d) != GetData(0x1d)) {
          if (route->m_portal && m_treeSim->GetLastResult() == 0) PortalFailedTree(this, ImplOf(route->m_portal));
          if (TryRoomRouting(route)) {
            state = 2;
            result = 3;
            SetPersonData(0x48, 0);
            done = true;
          } else {
            state = 0x10;
            route->m_failure = 2;
          }
          break;
        }
        state = 1;
        SetPersonData(0x48, route->m_insetId);
        break;
      case 3: {  // following the path
        if (ShouldInterrupt() && anim->IsInterruptable()) {
          state = 0xb;
          route->m_failure = 4;
          break;
        }
        if (m_routes.empty() || m_path.m_points.empty()) {
          state = 0xb;
          break;
        }
        const int r = anim->FollowOneStep();
        if (r == 1) state = 9;
        else if (r == 0) state = 10;
        else if (r == 2) done = true;
        break;
      }
      case 4:  // back from the seat's sit tree
        state = 8;
        if (!m_treeSim->GetLastTransition()) {
          int failure = GetPersonData(0x3e);
          if (failure == 0) failure = 5;
          state = 0x10;
          route->m_failure = failure;
        }
        break;
      case 5:  // back from the seat's stand-up tree
        SetPersonData(0x48, route->m_insetId);
        if (m_treeSim->GetLastTransition() && (GetIdleState() == 0 || GetIdleState() == 3)) {
          state = 1;
          break;
        }
        state = 0x10;
        route->m_failure = 6;
        break;
      case 6:  // the path failed
        if (stuck == 0) {
          const int d1 = m_location[1] - lastPos[1], d0 = m_location[0] - lastPos[0];
          lastPos[1] = m_location[1];
          lastPos[0] = m_location[0];
          if (d1 * d1 + d0 * d0 < 0x41) {
            stuck = 1;
          } else {
            state = 1;
            if (anim) anim->m_portalMode = 0;
          }
        } else if (stuck == 1) {
          stuck = 2;
          if (!route->m_asked && AskOthersToMove(route)) state = 0xe;
          else state = 0x10;
          if (anim) anim->m_portalMode = 0;
        } else if (stuck == 2) {
          stuck = 3;
          state = 1;
          if (anim) anim->m_portalMode = 0;
        } else if (stuck == 3) {
          state = 0x10;
          if (anim) anim->m_portalMode = 0;
        } else {
          state = 7;
          if (anim) anim->m_portalMode = 0;
        }
        break;
      case 7:
      case 8: {  // the end: failed / there
        done = true;
        if (state == 8) {
          bool ok;
          const int at[2] = {m_location[0], m_location[1]};
          if (GetPersonData(0x48) == 0) {
            ok = ObjCanPlace(this, at, m_level, nullptr, 0) != 0;
          } else {
            SetPersonData(0x49, GetPersonData(0x48));
            ok = ObjCanPlace(this, at, m_level, nullptr, 0) != 0;
            if (!ok) SetPersonData(0x49, 0);
          }
          if (!ok) {
            if (GetPersonData(0) == 0) {
              route->m_failure = 1;
              state = 7;
            }
          } else {
            PlacePerson(at);
            if (m_path.m_points.size() > 1 && GetData(9) == 0) {
              const TileListPoint& last = m_path.m_points.back();
              const TileListPoint& prev = m_path.m_points[m_path.m_points.size() - 2];
              const int from[2] = {prev.p[0], prev.p[1]}, to[2] = {last.p[0], last.p[1]};
              m_data.WriteVar(1, static_cast<short>(DirectionBetween(from, to)));
              ForceLocation();
            }
          }
        }
        if (anim && anim->IsFollowing()) anim->EndFollow();
        if (state == 7) {
          result = 0;
          if (route->m_failure == 0) route->m_failure = 1;
          if (anim) anim->m_portalMode = 0;
        } else {
          route->m_failure = 0;
          result = 1;
        }
        SetPersonData(0x3e, static_cast<short>(route->m_failure));
        if (m_routes.size() == 1 && GetPersonData(0x26) == 0) SetPersonData(0x27, 1);
        SetData(9, 0);
        // A route that asked this person to move out of its way with the action now running learns
        // whether it did.
        Interaction* current = GetCurrentAction();
        const int id = current ? current->GetID() : 0;
        if (id != 0)
          for (cXPersonImpl* p : LotPeople()) {
            XRoute* r = p != this ? p->GetCurrentRoute() : nullptr;
            if (r && ImplOf(r->m_asked) == this && r->m_askedAction == id) {
              r->m_askedAction = 0;
              r->m_askedDone = state == 8 ? 1 : 0;
              r->m_asked = nullptr;
            }
          }
        if (cXPersonImpl* asked = dynamic_cast<cXPersonImpl*>(ImplOf(route->m_asked))) asked->RemoveAction(route->m_askedAction);
        if (TraceRoutes()) {
          std::printf("[route] %s -> %s: %s (failure %d, blocker %d)\n", std::string(DebugName()).c_str(), object->DebugName(),
                      state == 8 ? "there" : "failed", route->m_failure, route->m_blocker);
        }
        if (!m_routes.empty()) m_routes.pop_back();
        route = nullptr;
        break;
      }
      case 9:  // arrived
        if (!anim->EndFollow()) done = true;
        else state = 8;
        break;
      case 10:  // the follow failed
        if (!anim->EndFollow()) done = true;
        else state = 6;
        anim->m_portalMode = 0;
        break;
      case 0xb:
        if (!anim->EndFollow()) done = true;
        else state = 7;
        anim->m_portalMode = 0;
        break;
      case 0xc:  // set out
        if (dist == -1.0) dist = CalcShortDistance(object);
        state = 3;
        done = true;
        anim->BeginFollow(static_cast<float>(dist));
        break;
      case 0xd: {  // waiting
        if (ShouldInterrupt() && anim->IsInterruptable()) {
          route->m_failure = 4;
          state = 7;
          break;
        }
        int start = route->m_waitStart;
        if (start == 0) start = SimTicks();
        if (SimTicks() - start < 0x1e) {
          const short tree = RouteTreeByName(GetBehavior(), "sg_route waiting animation");
          if (tree != 0 && GosubObjectTree(this, nullptr, tree, false)) {
            done = true;
            result = 3;
          }
        }
        if (!done) {
          start = 0;
          state = 1;
        }
        route->m_waitStart = start;
        anim->m_portalMode = 0;
        break;
      }
      case 0xe: {  // a person was asked to move
        if (ShouldInterrupt() && anim->IsInterruptable()) {
          route->m_failure = 4;
          state = 7;
          break;
        }
        int start = route->m_waitStart;
        if (!route->m_asked) {
          start = 0;
          state = 1;
          if (route->m_askedDone != 0) stuck = 0;
        } else {
          bool pushed = false;
          if (start == 0) {
            const short tree = RouteTreeByName(GetBehavior(), "sg_route ask to move animation");
            if (tree != 0 && GosubObjectTree(this, nullptr, tree, false)) {
              result = 3;
              done = true;
              pushed = true;
              if (StackElem* top = m_treeSim->GetCurElem()) top->m_stackObject = ImplOf(route->m_asked)->m_id;
              start = SimTicks();
            }
          } else {
            if (SimTicks() - start < 300) {
              const short tree = RouteTreeByName(GetBehavior(), "sg_route waiting animation");
              if (tree != 0 && GosubObjectTree(this, nullptr, tree, false)) {
                done = true;
                result = 3;
                pushed = true;
              }
            }
            if (!pushed) start = 0;
          }
          if (!pushed) state = 6;
        }
        route->m_waitStart = start;
        anim->m_portalMode = 0;
        break;
      }
      case 0xf:  // plan again
        if (!anim->EndFollow()) done = true;
        else state = 1;
        anim->m_portalMode = 0;
        break;
      case 0x10: {  // failure feedback
        if (route->m_failure == 0) route->m_failure = 1;
        state = 7;
        SetPersonData(0x3e, static_cast<short>(route->m_failure));
        if (!(rslot->m_flags & RoutingSlot::kFailureTrees)) {
          if (!anim->EndFollow()) done = true;
        } else {
          const short tree = RouteTreeByName(object->GetBehavior(), "gl_route failure feedback");
          const short params[4] = {static_cast<short>(route->m_failure), route->m_blocker, 0, 0};
          if (tree != 0 && GosubObjectTree(object, params, tree, false)) {
            SetPersonData(0x48, 0);
            result = 3;
            state = 0x11;
            done = true;
          }
        }
        anim->m_portalMode = 0;
        break;
      }
      case 0x11:
        state = 7;
        SetPersonData(0x3e, static_cast<short>(route->m_failure));
        anim->m_portalMode = 0;
        break;
      case 0x12: {  // the goals and the path
        const int r = InitRoute(route);
        result = r;
        if (r == 1) state = 8;
        else if (r == 0) state = 6;
        else if (r == 2) state = 0xc;
        else if (r == 3) {
          state = 4;
          SetPersonData(0x48, 0);
          done = true;
        }
        break;
      }
      case 0x13:  // the target went
        state = 7;
        anim->m_portalMode = 0;
        break;
      case 0x14:
        if (!anim->EndFollow()) done = true;
        else state = 0x13;
        anim->m_portalMode = 0;
        break;
      default:
        state = 7;
        break;
    }
    if (!done) continue;
    frame->m_primState = state;
    if (route) {
      route->m_stuck = stuck;
      route->m_personPos[0] = lastPos[0];
      route->m_personPos[1] = lastPos[1];
    }
    if (TraceRoutes() && state != startState && state != 3)
      std::printf("[route] %s: state %s -> %s (result %d)\n", DebugName(), kStateNames[startState < 21 ? startState : 0],
                  kStateNames[state < 21 ? state : 0], result);
    if (result != 2) {
      ClearIgnoringObject();
      return result;
    }
    return 2;
  }
}

// 801281B4
// When its room changed: it is in the new one (+0x41C); the old room, now empty of people, has its
// overhead lights off, the new one on.
void cXPersonImpl::UpdateCurrentRoom() {
  const unsigned int room = static_cast<unsigned int>(GetRoom());
  if (room == m_currentRoom) return;
  RoomManager* rooms = RoomManager::GetRoomManager();
  Room* old = m_currentRoom < 0xfffb && rooms ? rooms->GetRoom(m_currentRoom) : nullptr;
  Room* now = room < 0xfffb && rooms ? rooms->GetRoom(static_cast<unsigned short>(room)) : nullptr;
  m_currentRoom = static_cast<unsigned short>(room);
  if (old && old->GetPeopleCount() == 0) old->SetOverheadLights(false);
  if (now) now->SetOverheadLights(true);
}

// 8012C7F4
unsigned short cXPersonImpl::GetCurrentRoom() const { return m_currentRoom; }

// cXPersonImpl::Place (80128294): cXObjectImpl::Place, then the person's room (UpdateCurrentRoom).
void cXPersonImpl::PlacePerson(const int pos[2]) {
  ObjPlace(this, pos, m_level, nullptr, 0);
  UpdateCurrentRoom();
}

// 8012373C
// The path (FindPath) into m_path. A seat goal: there already (1), or the seat's sit tree (entry 0x1A)
// pushed (3), else failure 5. Else the person walks it (2) with one more point a tile on from its end
// in the direction to face there: along the last leg (slot facing "any"; on the spot: as it faces), at
// the object (toward / away, the slot's direction to it unless any rotation: the object's spot itself),
// or the object's facing turned by the slot's.
int cXPersonImpl::InitRoute(XRoute* route) {
  if (!route) return 0;
  cXObjectImpl* object = ImplOf(route->m_object);
  RoutingSlot* slot = route->GetRoutingSlot();
  Room* room = RoomManager::GetRoomManager()->GetRoom(static_cast<unsigned short>(GetData(0x1d)));
  int found = 0;
  if (room) {
    m_path.m_points.clear();
    found = route->FindPath(m_path);
  }
  int r;
  if (found == 0) {
    r = 0;
    route->IsPersonSittingOnChairGoal(this);
  } else {
    route->HasCurrentGoal();
    r = 2;
    RouteGoal* goal = route->m_goal >= 0 ? route->GetCurrentGoal() : nullptr;  // GC: reads goal -1
    if (goal) SetPersonData(0x25, static_cast<short>(goal->m_dirMask));
    if (goal && goal->m_object != 0) {
      r = 0;
      cXObjectImpl* chair = ImplOf(GetObjectFromID(goal->m_object));
      if (chair && chair->IsChair()) {
        if (ImplOf(GetContainer()) == chair) {
          r = 1;
        } else if (GosubObjectTree(chair, nullptr, chair->GetTreeID(0x1a), false)) {
          r = 3;
        }
      }
      if (r == 0) route->m_failure = 5;
    }
  }
  if (r == 2 && m_path.m_points.size() < 2) r = 0;
  if (r != 2) {
    route->ResetGoals();
    return r;
  }
  const TileListPoint last = m_path.m_points.back();
  int at[2] = {object->m_location[0], object->m_location[1]};
  if ((slot->m_flags & RoutingSlot::kAverageLocation) && object->IsMultiTile()) {
    PlacementGroup* g = PlacementGroupOf(object);
    if (g && !g->parts.empty()) {
      int s0 = 0, s1 = 0, n = 0;
      for (auto& part : g->parts) s0 += part.object->m_location[0], s1 += part.object->m_location[1], ++n;
      at[0] = s0 / n, at[1] = s1 / n;
    }
  }
  int face[2] = {last.p[0], last.p[1]};
  const int facing = slot->m_facing;
  if (facing == RoutingSlot::kFaceAnywhere) {
    const TileListPoint& prev = m_path.m_points[m_path.m_points.size() - 2];
    const int d1 = last.p[1] - prev.p[1], d0 = last.p[0] - prev.p[0];
    if (d1 == 0 && d0 == 0) {
      int step[2];
      DirectionStep(GetData(1) & 7, step);
      face[1] += step[1];
      face[0] += step[0];
    } else {
      face[1] = last.p[1] + d1;
      face[0] = last.p[0] + d0;
    }
  } else if (facing == RoutingSlot::kFaceAway || facing == RoutingSlot::kFaceTowards) {
    int dir;
    if (slot->m_flags & 0xff) {
      face[0] = at[0], face[1] = at[1];
      if (!(slot->m_flags & RoutingSlot::kAnyRotation)) {
        const int from[2] = {last.p[0], last.p[1]};
        dir = DirectionBetween(from, at);
        if (facing == RoutingSlot::kFaceAway) dir += 4;
        int step[2];
        DirectionStep(dir & 7, step);
        face[1] = last.p[1] + step[1];
        face[0] = last.p[0] + step[0];
      }
    } else {
      if (at[0] == last.p[0] && at[1] == last.p[1]) {
        dir = object->GetData(1);
      } else {
        const int from[2] = {last.p[0], last.p[1]};
        dir = DirectionBetween(from, at);
      }
      if (facing == RoutingSlot::kFaceAway) dir += 4;
      int step[2];
      DirectionStep(dir & 7, step);
      face[1] = last.p[1] + step[1];
      face[0] = last.p[0] + step[0];
    }
  } else {
    int step[2];
    DirectionStep((object->GetData(1) + facing) & 7, step);
    face[1] = last.p[1] + step[1];
    face[0] = last.p[0] + step[0];
  }
  if (face[1] == last.p[1] && face[0] == last.p[0]) return 0;
  TileListPoint f;
  f.p[0] = face[0];
  f.p[1] = face[1];
  m_path.m_points.push_back(f);
  if (TraceRoutes()) {
    const RouteGoal* g = route->m_goal >= 0 ? route->GetCurrentGoal() : nullptr;
    std::printf("[route] %s -> %s: %d goals, goal %d at %.2f,%.2f (score %d, side %x), %zu way points:", std::string(DebugName()).c_str(),
                object->DebugName(), route->CountGoals(), route->m_goal, g ? g->m_pos[1] / 16.0f : 0.0f,
                g ? g->m_pos[0] / 16.0f : 0.0f, g ? g->m_score : 0, g ? g->m_dirMask : 0, m_path.m_points.size());
    for (const TileListPoint& p : m_path.m_points) std::printf(" %.2f,%.2f", p.p[1] / 16.0f, p.p[0] / 16.0f);
    std::printf("\n");
  }
  return 2;
}

// 80124698
// To the target's room through a portal: the best portal of this room (its route scores made fresh for
// a route's first portal) whose portal tree (entry 0xF) runs on this person.
int cXPersonImpl::TryRoomRouting(XRoute* route) {
  cXObjectImpl* target = ImplOf(route->m_object);
  if (!target) return 0;
  if (!(route->m_portal != nullptr || PortalInitRoute(this) != 0)) return 0;
  cXObjectImpl* portal = PortalFindBest(this, target);
  if (!portal) {
    if (TraceRoutes()) {
      const std::string me = DebugName();
      std::printf("[route] %s -> %s: no portal from room %d toward room %d (route id %d)\n", me.c_str(), target->DebugName(),
                  GetData(0x1d), target->GetData(0x1d), GetData(6));
      for (int i = 0; i < static_cast<int>(Globs_pObjectModule->GetNumPortals()); ++i) {
        auto* q = reinterpret_cast<cXObjectImpl*>(Globs_pObjectModule->GetPortal(i));
        cXObjectImpl* o = PortalOtherSide(q);
        std::printf("[route]   portal #%d %s room %d tree %d other #%d room %d score %.1f\n", q->m_id, q->DebugName(),
                    q->GetData(0x1d), q->GetTreeID(0xf), o ? o->m_id : 0, o ? o->GetData(0x1d) : -1, PortalRouteScore(q, GetData(6)));
      }
    }
    return 0;
  }
  const short tree = portal->GetTreeID(0xf);
  if (tree == 0 || !GosubObjectTree(portal, nullptr, tree, false)) return 0;
  PortalBeginningTree(this, portal);
  route->m_portal = portal;
  if (TraceRoutes())
    std::printf("[route] %s -> %s: through portal #%d (room %d -> %d)\n", std::string(DebugName()).c_str(), target->DebugName(), portal->m_id,
                GetData(0x1d), target->GetData(0x1d));
  return 1;
}

// 80122998
// The interaction is the "move out of the way" object's (GUID 0x7C4) entry 3.
bool IsMoveOutOfWay(Interaction* a) {
  cXObject* so = a ? a->GetStackObject() : nullptr;
  cXObjectImpl* o = ImplOf(so);
  if (!o || !o->m_def) return false;
  return objdef::GUID(o->m_def) == 0x7c4 && a->m_index == 3;
}

// 80122A14
// One of its actions (the running one too) is a "move out of the way".
bool IsAskedToMove(cXPersonImpl* p) {
  for (int i = p->CountActions(true); i != 0;) {
    --i;
    if (IsMoveOutOfWay(p->GetIndAction(i, true))) return true;
  }
  return false;
}

namespace {
// The "move out of the way" object made at the person's spot (GC: GetSelectorByGUID 0x7C4,
// MakeNewOutOfWorldObject, CanPlace then Place there); null when it cannot be.
cXObjectImpl* MakeMoveOutOfWayObject(cXPersonImpl* p, bool* made) {
  *made = false;
  ObjSelector* sel = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(0x7c4) : nullptr;
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!sel || !module) return nullptr;
  short id = 0;
  if (!module->MakeNewOutOfWorldObject(sel, &id) || id == 0) return nullptr;
  *made = true;
  cXObjectImpl* o = ImplOf(module->GetObjectFromID(id));
  if (!o) return nullptr;
  const int at[2] = {p->m_location[0], p->m_location[1]};
  if (!ObjCanPlace(o, at, p->m_level, nullptr, 0)) return nullptr;
  ObjPlace(o, at, p->m_level, nullptr, 0);
  return o;
}
}  // namespace

// 80122AAC
// GC: Globs::bStupidMode is off while the object is made.
bool cXPersonImpl::MoveOutOfWay(int priority) {
  if (IsAskedToMove(this)) return false;
  bool made = false;
  cXObjectImpl* o = MakeMoveOutOfWayObject(this, &made);
  if (!o) return false;  // GC: an object that cannot be placed stays out of the world
  RoutingSlot slot;
  for (int d = 0; d < 8; d += 2) slot.AllowDirection(d);
  slot.AllowAnyRotation();
  slot.SetDistances(0x10, 0x50, 0x10);
  slot.AllowAnyFacing();
  slot.SetMultiplier(0, 1);
  slot.m_flags |= RoutingSlot::kRandomScoring;
  SetObjectRoutingSlot(o, 0, slot);
  Interaction action(this, o, 3, priority);
  const int added = AddAction(&action);
  if (!added) Globs_pObjectModule->KillObject(o->m_id);
  else SetPersonData(0x2a, static_cast<short>(GetPersonData(0x2a) | 1));
  return added != 0;
}

// 80122E00
// This person, standing on `route`'s path (another's), is asked to step aside: where the path enters
// and leaves its footprint gives the way the path runs; the "move out of the way" object's slot is the
// two sides across it (a ring of 1..5 tiles, random scoring; if no way there, also straight ahead and
// at the far end), and its action is pushed at the asker's priority (person data 0x21). The route
// remembers whom it asked and the action's id. Not while this person is asking someone itself or was
// asked already.
int cXPersonImpl::MoveOutOfWay(XRoute* route, TileList& path) {
  if (IsAskedToMove(this)) return 0;
  if (path.m_points.size() < 2) return 0;
  XRoute* mine = GetCurrentRoute();
  if (mine && mine->m_asked) return 0;
  int r[4];
  ObjRect(this, r);
  int crossing = 0;
  int enter[2] = {0, 0}, leave[2] = {0, 0};
  size_t seg = 1;
  for (; seg < path.m_points.size(); ++seg) {
    const TileListPoint& a = path.m_points[seg - 1];
    const TileListPoint& b = path.m_points[seg];
    int n = std::max(std::abs(b.p[0] - a.p[0]), std::abs(b.p[1] - a.p[1]));
    const double steps = static_cast<double>(n + 1);
    float t = 0.0f;
    do {
      const int x = static_cast<int>(t * static_cast<float>(b.p[1] - a.p[1]) + static_cast<float>(a.p[1]) + 0.5f);
      const int y = static_cast<int>(t * static_cast<float>(b.p[0] - a.p[0]) + static_cast<float>(a.p[0]) + 0.5f);
      const bool inside = x < r[1] && r[3] <= x && y < r[0] && r[2] <= y;
      if (inside) {
        if (crossing == 0) {
          crossing = 1;
          enter[0] = y, enter[1] = x;
        }
      } else if (crossing == 1) {
        crossing = 2;
        leave[0] = y, leave[1] = x;
        break;
      }
      t = static_cast<float>(t + 1.0 / static_cast<float>(steps));
    } while (t < 1.0f);
    if (crossing >= 2 || seg + 1 >= path.m_points.size()) break;
  }
  if (crossing == 0) {
    enter[0] = path.m_points[0].p[0], enter[1] = path.m_points[0].p[1];
    leave[0] = m_location[0], leave[1] = m_location[1];
  } else if (crossing == 1) {
    const TileListPoint& end = path.m_points[std::min(seg, path.m_points.size() - 1)];
    leave[0] = end.p[0], leave[1] = end.p[1];
  }
  bool made = false;
  cXObjectImpl* o = MakeMoveOutOfWayObject(this, &made);
  if (!o) return 0;
  const int d1 = leave[1] - enter[1], d0 = leave[0] - enter[0];
  if (d1 == 0 && d0 == 0) return 0;  // GC: the object stays where it was placed
  const int dir = DirectionBetween(enter, leave);
  RoutingSlot slot;
  slot.AllowDirection((dir + 2) & 7);
  slot.AllowDirection((dir + 6) & 7);
  slot.AllowAnyRotation();
  slot.SetDistances(0x10, 0x50, 0x10);
  slot.m_resolution = 8;
  slot.AllowAnyFacing();
  slot.SetMultiplier(0, 1);
  slot.m_flags |= RoutingSlot::kRandomScoring;
  {
    XRoute test(this, o, &slot);
    test.BuildGoalList();
    TileList way;
    if (!test.FindPath(way)) {
      slot.AllowDirection(dir);
      slot.SetDistances(0x10, 0x50, 0x50);
    }
  }
  SetObjectRoutingSlot(o, 0, slot);
  cXPersonImpl* asker = dynamic_cast<cXPersonImpl*>(ImplOf(route->m_person));
  Interaction action(this, o, 3, asker ? asker->GetPersonData(0x21) : 0);
  if (AddAction(&action)) {
    SetPersonData(0x2a, static_cast<short>(GetPersonData(0x2a) | 1));
    route->m_askedAction = action.GetID();
    route->m_askedDone = 0;
    route->m_asked = this;
    if (TraceRoutes()) std::printf("[route] %s steps aside for %s\n", std::string(DebugName()).c_str(), asker ? asker->DebugName() : "?");
    return 1;
  }
  Globs_pObjectModule->KillObject(o->m_id);
  return 0;
}

// 80122120
// The people in the way of `route` (not one a player steers, not one ShouldIgnore leaves out, not one
// another route has asked already; the asker standing, person data 0): the route's path ignoring
// people is followed (but its last leg) until it crosses one's footprint, who is asked to move.
int cXPersonImpl::AskOthersToMove(XRoute* route) {
  if (!route) return 0;
  static std::vector<cXPersonImpl*> consider;  // GC: a function static
  consider.clear();
  const std::vector<cXPersonImpl*> people = LotPeople();
  for (cXPersonImpl* p : people) {
    bool steered = false;
    for (int player = 0; player < 2; ++player) {
      cXPerson* sel = _globals.GetSelectedPerson(player);  // DAT_80475E88
      if (!sel || sel->AsObject() != p) continue;
      if (PlayerDirectInteractor(player) && p->m_animator && !p->m_animator->IsSimulatorControlled()) {
        steered = true;
        break;
      }
    }
    if (steered || p == this || route->ShouldIgnore(p) != 0 || GetPersonData(0) != 0) continue;
    bool asked = false;
    for (cXPersonImpl* q : people)
      for (XRoute& r : q->m_routes)
        if (ImplOf(r.m_asked) == p) asked = true;
    if (!asked) consider.push_back(p);
  }
  if (consider.empty()) return 0;
  XRoute test(this, route->m_object, route->GetRoutingSlot());
  test.m_ignorePeople = 1;
  test.m_avoid = nullptr;
  test.BuildGoalList();
  TileList way;
  if (!test.FindPath(way) || way.m_points.size() < 2) return 0;
  cXPersonImpl* found = nullptr;
  for (size_t seg = 1; seg < way.m_points.size() && !found; ++seg) {
    const TileListPoint& a = way.m_points[seg - 1];
    const TileListPoint& b = way.m_points[seg];
    const int n = std::max(std::abs(b.p[0] - a.p[0]), std::abs(b.p[1] - a.p[1]));
    const double steps = static_cast<double>(n + 1);
    float t = 0.0f;
    do {
      const float next = static_cast<float>(t + 1.0 / static_cast<float>(steps));
      const int x = static_cast<int>(t * static_cast<float>(b.p[1] - a.p[1]) + static_cast<float>(a.p[1]) + 0.5f);
      const int y = static_cast<int>(t * static_cast<float>(b.p[0] - a.p[0]) + static_cast<float>(a.p[0]) + 0.5f);
      for (cXPersonImpl* p : consider) {
        int r[4];
        ObjRect(p, r);
        if (x < r[1] && r[3] <= x && y < r[0] && r[2] <= y) {
          found = p;
          break;
        }
      }
      t = next;
    } while (!found && t < 1.0f);
  }
  if (found && found->MoveOutOfWay(route, way)) {
    if (TraceRoutes()) std::printf("[route] %s asks %s to move\n", std::string(DebugName()).c_str(), found->DebugName());
    return 1;
  }
  return 0;
}

// 80124370
int cXPersonImpl::TryGotoRelative(StackElem* frame, const unsigned char* params) {
  if (!m_instance) return 0;
  if (frame->m_primState != 0) return TryGotoRoutingSlot(frame, static_cast<RoutingSlot*>(nullptr));
  RoutingSlot slot;
  slot.SetMultiplier(0, 1);
  slot.SetTileDistances(1.0f, 1.0f, 1.0f);
  slot.m_flags &= ~static_cast<unsigned int>(RoutingSlot::kFailureTrees);
  if (!(params[6] & 2)) slot.m_flags |= RoutingSlot::kFailureTrees;
  slot.m_flags &= ~static_cast<unsigned int>(RoutingSlot::kDifferentAlts);
  if (params[6] & 4) slot.m_flags |= RoutingSlot::kDifferentAlts;
  const int side = static_cast<signed char>(params[2]);
  auto four = [&] {
    for (int d = 0; d < 8; d += 2) slot.AllowDirection(d);
    slot.SetTileDistances(1.0f, 2.0f, 1.0f);
  };
  switch (side) {
    case -2: slot.SetIsOnTopOfObject(); break;
    case 0: case 2: case 4: case 6: slot.AllowDirection(side); break;
    case 1: slot.AllowDirection(0), slot.AllowDirection(2); break;
    case 3: slot.AllowDirection(2), slot.AllowDirection(4); break;
    case 5: slot.AllowDirection(4), slot.AllowDirection(6); break;
    case 7: slot.AllowDirection(6), slot.AllowDirection(0); break;
    default: four(); break;
  }
  const int facing = static_cast<signed char>(params[3]);
  if (facing == -1) slot.AllowAnyFacing();
  else if (facing == -2) slot.FaceTowardsObject();
  else if (facing >= 0 && facing < 8) slot.SetFacingDirection(facing);
  else {
    SIMMY_STUB("cXPersonImpl::TryGotoRelative: a bad facing (vtable 0x50C)", 0x80124370);
    return 0;
  }
  return TryGotoRoutingSlot(frame, &slot);
}

// 80129934
// Also the action queue's icon object (when `setIcon`) and the tree's object's "in use" count (data 0x3E).
int cXPersonImpl::GosubObjectTree(cXObject* object, const short* params, short tree, bool setIcon) {
  if (!cXObjectImpl::GosubObjectTree(object, params, tree, setIcon)) return 0;
  cXObjectImpl* impl = object->GetObjectImplementation();
  // The running action's queue icon becomes the tree's object.
  if (setIcon && m_hasCurrentAction) m_actionQueue.SetIconObject(object, 0);
  m_gosubObjects.push_back({impl, m_treeSim->GetStackSize(), setIcon});
  if (impl) impl->m_data.WriteVar(0x3e, static_cast<short>(impl->m_data[0x3e] + 1));
  return 1;
}

// 80129C40
// The object tree on top returned (or the stack fell below it): its object's user count drops. With no
// object tree left the current action is complete (also when a run-as-sub action waits at the head).
// GC: also the action queue's icon object (ActionQueue::SetIconObject, HUD).
void cXPersonImpl::StackJustPopped() {
  if (m_gosubObjects.empty() || !m_treeSim) return;
  const int size = m_treeSim->GetStackSize();
  const GosubObject& top = m_gosubObjects.back();
  if (size != top.m_depth - 1 && size >= top.m_depth) return;
  if (top.m_object) {
    top.m_object->m_data.WriteVar(0x3e, static_cast<short>(top.m_object->m_data[0x3e] - 1));
    const std::string who = DebugName();
    std::printf("[sim] %s is done with %s\n", who.c_str(), top.m_object->DebugName());
  }
  m_gosubObjects.pop_back();
  if (m_gosubObjects.empty()) CompleteCurrentAction();
  else if (m_actionQueue.Count() != 0 && (m_actionQueue.At(0).m_flags & Interaction::kRunAsSub))
    CompleteCurrentAction();
}

// 8012C758
int cXPersonImpl::GetPersonData(int index) const {
  return index >= 0 && index < 0x100 ? m_personData[index] : 0;
}
// 8012C838
void cXPersonImpl::SetNeighborID(short id) { SetPersonData(0x1f, id); }

// 8012C768
void cXPersonImpl::SetPersonData(int index, short value) {
  if (index >= 0 && index < 0x100) m_personData[index] = value;
}
// 8012C780
int cXPersonImpl::GetIdleState() { return m_personData[0]; }
// 8012C7EC
SAnimator2* cXPersonImpl::GetSAnimator() const { return m_animator; }
// 8012C788
bool cXPersonImpl::IsCarrying() { return GetContainedObject(0) != nullptr; }
// 8012C1F4
bool cXPersonImpl::IsCarryingDCObject() {
  if (!IsCarrying()) return false;
  cXObjectImpl* held = GetContainedObject(0);
  return held && held->m_def && static_cast<unsigned int>(ERQuickdata::S16(held->m_def, 0x0a) - 0x23) < 3;
}
// 8012CA5C
cXObject* cXPersonImpl::GetControllingObject() {
  if (m_gosubObjects.empty()) return this;
  return m_gosubObjects.back().m_object;
}
// 8012C8C0
// GC: the selector's neighbour record's description. PC: the one it was made with.
CasSimDescriptionS2C* cXPersonImpl::GetSimDescription() { return m_desc; }
// 8012B804
void cXPersonImpl::SetCurrentActionText(const std::u16string& text) { m_actionQueue.SetActionText(text, 0); }

// 8012C08C
// Its neighbour (by its id, person data 0x1F) puts its wants and fears on the HUD.
void cXPersonImpl::InitWantFearIcons() {
  Neighbor* n = Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(GetPersonData(0x1f)) : nullptr;
  if (n) n->InitWantFearIcons();
}

// 8012C0F8
void cXPersonImpl::SpawnWantFearDialog(unsigned int slot) const {
  Neighbor* n = Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(GetPersonData(0x1f)) : nullptr;
  if (n) n->SpawnWantFearDialog(slot);
}

// 8012C8D0
// The selector's service NPC record (+0x90; null for everyone else).
const unsigned char* cXPersonImpl::GetServiceNPC() { return m_selector ? m_selector->m_npc : nullptr; }
// 8012C8E0
int cXPersonImpl::IsInvisible() { return static_cast<unsigned short>(m_personData[0x4a]) & 1; }
// 8012C8EC
int cXPersonImpl::IsGreen() { return (static_cast<unsigned short>(m_personData[0x4a]) >> 1) & 1; }
// 8012C8F8
int cXPersonImpl::IsGhost() { return (static_cast<unsigned short>(m_personData[0x4a]) >> 2) & 1; }
// 8012C904
int cXPersonImpl::IsAlien() { return (static_cast<unsigned short>(m_personData[0x4a]) >> 3) & 1; }
// 8012C910
int cXPersonImpl::IsInMotiveFailure() { return (static_cast<unsigned short>(m_personData[0x4a]) >> 4) & 1; }
// 8012C91C
int cXPersonImpl::NeedsWantFearShuffle() { return (static_cast<unsigned short>(m_personData[0x4a]) >> 5) & 1; }
// 8012C928
void cXPersonImpl::SetInvisible(bool on) {
  if (on) m_personData[0x4a] = static_cast<short>(m_personData[0x4a] | 1);
  else m_personData[0x4a] = static_cast<short>(m_personData[0x4a] & 0xfffe);
}
// 8012C950
void cXPersonImpl::SetIsAlien(bool on) {
  if (on) m_personData[0x4a] = static_cast<short>(m_personData[0x4a] | 8);
  else m_personData[0x4a] = static_cast<short>(m_personData[0x4a] & 0xfff7);
}
// 8012C978
int cXPersonImpl::GetVisibility() { return m_personData[0x4a]; }
// 8012C7FC
int cXPersonImpl::GetNeighborID() const { return GetPersonData(0x1f); }
// 8012CAA8
void cXPersonImpl::SetAwareOfObject(cXObject* object) {
  if (m_animator) m_animator->SetAwareOfObject(object);
}
// 8012CAE0
void cXPersonImpl::SetAwareOfObjectKilled(cXObject* object) {
  if (m_animator) m_animator->SetAwareOfObjectKilled(object);
}
// 8012CB18
void cXPersonImpl::ClearAwareOfObject(cXObject* object) {
  if (m_animator) m_animator->ClearAwareOfObject(object);
}

void cXPersonImpl::SetLocation(float x, float y, int level, int dir) {
  m_pos[0] = x;
  m_pos[1] = y;
  m_dir = dir & 7;
  // cXObjectImpl's location is in 1/16 tiles (GetCTilePt: the tile is +0x6C >> 4, +0x68 >> 4).
  m_location[1] = static_cast<int>(x * 16.0f);
  m_location[0] = static_cast<int>(y * 16.0f);
  m_level = level;
  m_data.WriteVar(1, static_cast<short>(m_dir));
}

// ---------------------------------------------------------------------------------------------------
// PC: the interact button

namespace {
std::string Narrow(const unsigned short* s) {
  std::string out;
  for (; s && *s && out.size() < 60; ++s) out += *s < 0x80 ? static_cast<char>(*s) : '?';
  return out;
}
}  // namespace

// The interactions of `object` the person can do (its tree table, ResFile +0x24: the entries' check
// trees run on the person's stack with the object as stack object), and the first one in menu order
// queued for the person at the player's priority (cXPersonImpl::AddAction); the person's main loop runs
// it ("SG_check queue": idle for input, executeInterruptOnIdlePrimitve). GC: the A button opens the
// action menu (ActionMenu, the pie menu) from the object's ObjTestSim tests and the choice is queued
// the same way (DirectInteractor::ChooseAction). 1: queued.
namespace {
short s_pushVars[4] = {};  // PC: the chosen entry's menu item's stack vars
bool s_pushHasVars = false;
}  // namespace
int PushInteraction(cXPersonImpl* person, cXObjectImpl* object, const char* pickName) {
  s_pushHasVars = false;
  if (!person || !object || !person->m_treeSim) return 0;
  object = object->GetInteractionLeader();  // ObjTestSim(person, object): a group's interaction leader
  const unsigned char* file = SelectorTreeTabResFile(object->m_selector);
  const auto* tables = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x24)) : nullptr;
  if (!tables || ERQuickdata::U32(tables - 4, 0) == 0) return 0;
  const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(tables, 0));
  const unsigned int n = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
  Behavior* behavior = object->GetBehavior();
  const unsigned char* best = nullptr;
  int bestIndex = -1;
  for (unsigned int i = 0; i < n; ++i) {
    const unsigned char* e = entries + i * 0x24;
    const unsigned short* name = ERQuickdata::LocString(e, 0x1c);
    const bool debug = (ERQuickdata::U32(e, 0x10) & 0x80) != 0 || (name && name[0] == '!');
    const short check = ERQuickdata::S16(e, 0), action = ERQuickdata::S16(e, 2);
    // PC: entries of +0x16 > 99 are left to SIMMY_INTERACT_PICK (by name: the direct-control "Pick Up").
    static const char* pickEnvVar = std::getenv("SIMMY_INTERACT_PICK");
    const char* pickEnv = pickName ? pickName : pickEnvVar;
    if (debug || !action || (ERQuickdata::S16(e, 0x16) > 99 && !pickEnv)) continue;
    if (!best) s_pushHasVars = false;
    int ok = 1;
    PcMenuBuild items;  // the menu items the check tree makes (TryMakeActionString)
    g_pcMenuBuild = &items;
    if (check) ok = person->m_treeSim->RunCheckTree(behavior, check, static_cast<short>(object->m_id), nullptr);
    g_pcMenuBuild = nullptr;
    // PC development aid: SIMMY_MENU_ITEM=<n> takes the check tree's n-th menu item (default the first).
    if (ok && !items.empty()) {
      static const char* nth = std::getenv("SIMMY_MENU_ITEM");
      size_t k = nth && nth[0] != 'v' ? std::min(static_cast<size_t>(std::atoi(nth)), items.size() - 1) : 0;
      if (nth && nth[0] == 'v')  // "v<n>[/<m>...]": the item whose first stack var is the last of them offered
        for (const char* v = nth; v && *v; v = std::strchr(v, '/')) {
          ++v;
          for (size_t j = 0; j < items.size(); ++j)
            if (items[j].vars[0] == std::atoi(v)) k = j;
        }
      for (size_t j = 0; j < items.size() && !pickName; ++j)
        std::printf("[sim]   menu item %zu \"%s\" (stack vars %d %d %d %d)\n", j,
                    Narrow(reinterpret_cast<const unsigned short*>(items[j].name.c_str())).c_str(), items[j].vars[0],
                    items[j].vars[1], items[j].vars[2], items[j].vars[3]);
      for (int v = 0; v < 4; ++v) s_pushVars[v] = items[k].vars[v];
      s_pushHasVars = true;
    }
    if (!pickName)
      std::printf("[sim] %s: \"%s\" (check %04x %s, action %04x)\n", object->DebugName(), Narrow(name).c_str(),
                static_cast<unsigned short>(check), ok ? "passes" : "fails", static_cast<unsigned short>(action));
    // PC development aid: SIMMY_INTERACT_PICK=<text>[,<text>...] takes the first one whose name
    // contains one of them.
    // (`pickName`, from a caller, in its place: SIMMY_PLAYER_WORK's "Go to Work".)
    const char* pick = pickEnv;
    bool picked = false;
    if (ok && pick) {
      const std::string label = Narrow(name);
      for (const char* p = pick; *p && !picked;) {
        const char* end = std::strchr(p, ',');
        const std::string part(p, end ? end : p + std::strlen(p));
        picked = !part.empty() && label.find(part) != std::string::npos;
        p = end ? end + 1 : p + std::strlen(p);
      }
    }
    if (picked) {
      best = e;
      bestIndex = static_cast<int>(i);
      break;
    }
    if (ok && !pick && (!best || ERQuickdata::S16(e, 0x1a) < ERQuickdata::S16(best, 0x1a))) {
      best = e;
      bestIndex = static_cast<int>(i);
    }
  }
  if (!best) return 0;
  Interaction action(person, object, bestIndex);
  if (s_pushHasVars)
    for (int v = 0; v < 4; ++v) action.m_stackVars[v] = s_pushVars[v];
  s_pushHasVars = false;
  const int r = person->AddAction(&action);
  const std::string who = person->DebugName();
  std::printf("[sim] %s queues \"%s\" on %s: %s\n", who.c_str(), Narrow(ERQuickdata::LocString(best, 0x1c)).c_str(),
              object->DebugName(), r ? "queued" : "refused");
  return r;
}

// ---------------------------------------------------------------------------------------------------
// PC: the family on the lot

namespace {
std::vector<cXPersonImpl*> g_lotPeople;
CasSimDescriptionS2C g_defaultDesc{true};

// Where the family steps onto the lot: at the street end of the front walk, facing the lot's middle.
// GC: the move-in behaviour (SimAntics, not ported) walks the family in from the street; there is no
// fixed entry point in the data. PC: the lot's floors give the front walk: the sidewalk is a line of
// one floor pattern along a whole side of the lot (inside its border), and the walk is the outside
// floor tile (not the border's pattern, not the sidewalk) nearest the lot's edge, the most central
// along that edge when several are. Without one, the lot's middle.
void LotEntryPoint(float& x, float& y, int& dir) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  x = w ? w->m_xSize * 0.5f : 20.0f;
  y = w ? w->m_ySize * 0.5f : 20.0f;
  dir = 0;
  if (!w || !w->m_floors || w->m_xSize < 6 || w->m_ySize < 6) return;
  const int xs = w->m_xSize, ys = w->m_ySize;
  auto floor = [&](int tx, int ty) { return *w->m_floors->At(tx, ty); };
  const unsigned char border = floor(1, 1);
  std::vector<bool> sideX(static_cast<size_t>(xs)), sideY(static_cast<size_t>(ys));
  for (int tx = 2; tx < xs - 2; ++tx) {
    const unsigned char f = floor(tx, 2);
    bool line = f && f != border;
    for (int ty = 2; line && ty < ys - 2; ++ty) line = floor(tx, ty) == f;
    sideX[static_cast<size_t>(tx)] = line;
  }
  for (int ty = 2; ty < ys - 2; ++ty) {
    const unsigned char f = floor(2, ty);
    bool line = f && f != border;
    for (int tx = 2; line && tx < xs - 2; ++tx) line = floor(tx, ty) == f;
    sideY[static_cast<size_t>(ty)] = line;
  }
  int bestX = -1, bestY = -1, bestEdge = 0, bestCentre = 0;
  for (int ty = 2; ty < ys - 2; ++ty)
    for (int tx = 2; tx < xs - 2; ++tx) {
      const unsigned char f = floor(tx, ty);
      if (!f || f == border || sideX[static_cast<size_t>(tx)] || sideY[static_cast<size_t>(ty)]) continue;
      CTilePt tile(tx, ty, 1);
      if (w->GetRoom(tile) != 0) continue;
      const int ex = std::min(tx, xs - 1 - tx), ey = std::min(ty, ys - 1 - ty);
      const int edge = std::min(ex, ey);
      const int centre = ex < ey ? std::abs(ty - ys / 2) : std::abs(tx - xs / 2);
      if (bestX < 0 || edge < bestEdge || (edge == bestEdge && centre < bestCentre)) {
        bestX = tx, bestY = ty, bestEdge = edge, bestCentre = centre;
      }
    }
  if (bestX < 0) return;
  x = static_cast<float>(bestX);
  y = static_cast<float>(bestY);
  // Facing the lot's middle, in eighths of a turn as people face (SAnimator2::setPersonDirection):
  // 2 along +x, 4 along +y, 0 along -y.
  const float a = std::atan2(ys * 0.5f - y, xs * 0.5f - x);
  dir = (static_cast<int>(std::lround(a / (3.1415927f * 0.25f))) + 2) & 7;
}
}  // namespace

namespace {
CasSimDescriptionS2C g_defaultDesc2{false};

// Player `player`'s neighbour (Player::GetPrimaryNeighbor; two-player: family member 1).
Neighbor* PlayerNeighbor(int player) {
  if (Neighbor* n = _gd.m_players[player].GetPrimaryNeighbor()) return n;
  if (!Globs_pNeighborhood) return nullptr;
  FamilyImpl* f = Globs_pNeighborhood->GetFamily(_gd.m_fieldD8);
  if (!f || f->CountMembers() <= player) return nullptr;
  int* guid = f->GetIndexedMember(player);
  return guid ? Globs_pNeighborhood->FindNeighborByGUID(*guid) : nullptr;
}

// The person joins the house's objects (GC: ObjectModuleImpl::AddObject from MakeNewOutOfWorldObject /
// the move-in): the first free id, the end of the module's object list, so the house's save writes it
// with the house (WriteObjectModuleStream) and a continued game reads it back as a saved person.
void AddPersonToModule(cXPersonImpl* person) {
  ObjectModuleImpl* m = Globs_pObjectModule;
  if (!m) return;
  if (!m->AddObject(person, 0, true)) return;
  person->m_data.WriteVar(0xb, person->m_id);
}
// PC: out of the module's ids and object list (the person is freed by RemoveFamilyFromLot).
void RemovePersonFromModule(cXPersonImpl* person) {
  ObjectModuleImpl* m = Globs_pObjectModule;
  if (!m) return;
  for (cXObject*& o : m->m_byId)
    if (o == person) o = nullptr;
  for (cXObjectImpl** link = &m->m_first; *link; link = &(*link)->m_nextObject)
    if (*link == person) {
      *link = person->m_nextObject;
      break;
    }
  while (!m->m_byId.empty() && !m->m_byId.back()) m->m_byId.pop_back();
}

// The description of member `index` of family `family`, if it has one.
CasSimDescriptionS2C* FamilyMemberDescription(int family, int index) {
  if (!Globs_pNeighborhood) return nullptr;
  FamilyImpl* f = Globs_pNeighborhood->GetFamily(family);
  if (!f || f->CountMembers() <= index) return nullptr;
  int* guid = f->GetIndexedMember(index);
  return guid ? Globs_pNeighborhood->SimDescriptionOf(*guid) : nullptr;
}
}  // namespace

// Player 1 plays the player family's first member, player 2 (two-player mode) its second
// (Player::GetPrimaryNeighbor: member m_index). GC: a two-player game without a second member gets one
// before CAS (PreGameState::InsertPlayer2Roommate: family 2's first member moves in as a roommate,
// NeighborhoodImpl::AddAsPlayerRoommate) and player 2 makes it over in CAS (both ported: pregame.cpp).
// Without a second member, player 2 falls back to family 2's first member as it is, then the CAS slot
// 1, then a default female sim.
CasSimDescriptionS2C* PlayerSimDescription(int player) {
  if (player != 0 && (player != 1 || !_gd.m_fieldEC)) return nullptr;
  if (CasSimDescriptionS2C* desc = FamilyMemberDescription(_gd.m_fieldD8, player)) return desc;
  if (player == 1) {
    if (CasSimDescriptionS2C* desc = FamilyMemberDescription(2, 0)) return desc;
  }
  NeighborhoodCustomChars* chars = _gd.m_customChars;
  if (chars && chars->m_used[player]) return &chars->m_chars[player].m_desc;
  return player == 0 ? &g_defaultDesc : &g_defaultDesc2;
}

namespace {
bool s_familyOnLot = false;  // PC: MoveFamilyOntoLot has run for this house
}  // namespace

void MoveFamilyOntoLot(EHouse* house) {
  if (!house || s_familyOnLot) return;
  s_familyOnLot = true;
  // The family's funds become the lot's (GC: NeighborhoodImpl::LoadHouse / MoveIn; the simulator keeps
  // them while the family is on the lot, MoveOut gives them back).
  if (Globs_pSimulator && Globs_pNeighborhood)
    if (FamilyImpl* f = Globs_pNeighborhood->GetFamily(_gd.m_fieldD8)) Globs_pSimulator->SetFunds(f->GetFunds(), 0);
  // PC development aid: SIMMY_FUNDS=<n> gives the family n simoleons on the lot (the story family starts
  // with none: the fridge's $5 snack says "not enough simoleons", as on GC).
  if (const char* funds = std::getenv("SIMMY_FUNDS"))
    if (Globs_pSimulator) Globs_pSimulator->SetFunds(std::atoi(funds), 0);
  for (int player = 0; player < 2; ++player) {
    CasSimDescriptionS2C* desc = PlayerSimDescription(player);
    if (!desc) continue;
    // The sim's neighbour and its object (a CAS sim's user selector, else the template person's).
    Neighbor* neighbor = PlayerNeighbor(player);
    // The house's load trees may have made the person already (the scenario's "Create and Place Players":
    // TryCreateObject for the neighbour, placed by the tree): that one is the player's sim.
    cXPersonImpl* made = nullptr;
    for (cXPersonImpl* p : g_lotPeople)
      if (neighbor && p->m_neighbor == neighbor) made = p;
    const bool saved = made == nullptr;
    // Or the house kept the person (a saved game: the person read from the house).
    for (cXObjectImpl* o = Globs_pObjectModule && !made ? Globs_pObjectModule->m_first : nullptr; o; o = o->m_nextObject)
      if (auto* p = dynamic_cast<cXPersonImpl*>(o))
        if (neighbor && p->m_neighbor == neighbor && !made) made = p;
    if (made) {
      made->m_desc = desc;
      // A family member's person data 0x3D is its family (GC: set when the family moves in; the lot's save
      // writes it back to the neighbour, NeighborhoodImpl::SavePersistentData, and a character with none
      // would be deleted as unused, SaveHouse).
      if (made->GetPersonData(0x3d) == 0 && neighbor) made->SetPersonData(0x3d, static_cast<short>(_gd.m_fieldD8));
      float x = static_cast<float>(made->m_location[1]) / 16.0f, y = static_cast<float>(made->m_location[0]) / 16.0f;
      int dir = made->GetData(1) & 7;
      if (made->m_location[0] < 0 || made->m_location[1] < 0) {
        LotEntryPoint(x, y, dir);
        x += 0.5f, y += 0.5f;
      }
      if (const char* start = std::getenv("SIMMY_SIM_START")) {
        std::sscanf(start, "%f,%f", &x, &y);
        x += static_cast<float>(player) + 0.5f, y += 0.5f;
      }
      made->SetLocation(x, y, made->m_level ? made->m_level : 1, dir);
      if (made->m_animator) {
        made->m_animator->SnapToGrid();
        made->m_animator->setAnimationDirectionToPersonDirection();
      }
      std::printf("[sim] player %d's sim: object %d (%s), %s, at %.2f,%.2f\n", player + 1, made->m_id, made->DebugName(),
                  saved ? "read from the house's save" : "made by the house's load trees", x, y);
      _globals.SetSelectedPerson(player, made);
      continue;
    }
    ObjSelector* selector = neighbor ? neighbor->m_selector : nullptr;
    if (!selector && Globs_pObjectFolder && Globs_pObjectFolder->m_templateDef)
      selector = Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(objdef::GUID(Globs_pObjectFolder->m_templateDef)));
    auto* person = new cXPersonImpl(0, selector, nullptr);  // GC: ObjectModuleImpl::ConstructObject (selector, module)
    person->m_desc = desc;
    person->m_neighbor = neighbor;
    {
      char first[32] = {};
      for (int i = 0; i < 31 && desc->m_firstName[i]; ++i) first[i] = static_cast<char>(desc->m_firstName[i]);
      std::printf("[coop] player %d's sim: %s (%s)\n", player + 1, first, desc->m_male ? "male" : "female");
    }
    float x, y;
    int dir;
    LotEntryPoint(x, y, dir);
    // PC development aid: SIMMY_SIM_START=x,y puts the sims on that tile instead (e.g. by an object to try).
    if (const char* start = std::getenv("SIMMY_SIM_START")) std::sscanf(start, "%f,%f", &x, &y);
    person->SetLocation(x + static_cast<float>(player) + 0.5f, y + 0.5f, 1, dir);
    g_lotPeople.push_back(person);
    AddPersonToModule(person);
    person->Initialize();
    // Its behaviour: the main tree, with the init tree run first (GC: the move-in creates the person and
    // ObjectModuleImpl::AddObject runs its init tree).
    person->StartBehavior();
    if (const short init = person->GetTreeID(0)) person->m_treeSim->Gosub(person->GetBehavior(), nullptr, init);
    std::printf("[sim] player %d's sim: object %d (%s), main tree %04x\n", player + 1, person->m_id, person->DebugName(),
                static_cast<unsigned short>(person->GetTreeID(1)));
    _globals.SetSelectedPerson(player, person);
    // GC: the move-in's new person (GlobalDispatch 0xDE -> HouseImpl::DoCommand) redoes the visitor types.
    HouseDispatch(0xde, person->m_id);
  }

}

// GC: ObjectModuleImpl::MakeNewOutOfWorldObject (ConstructObject: a cXPersonImpl for the type-2
// selector), AddObject (the module's people), JustBorn and Reset(true) (the init tree at once), with
// the neighbour found by Initialize (LoadPersistentData). PC: the people live in the module's ids after
// the objects (AddPersonToModule) and are freed with the family (RemoveFamilyFromLot).
cXPersonImpl* MakeNeighborPerson(Neighbor* n, ObjSelector* selector) {
  if (!n || !selector || !Globs_pNeighborhood) return nullptr;
  auto* person = new cXPersonImpl(0, selector, nullptr);
  person->m_neighbor = n;
  person->m_desc = Globs_pNeighborhood->SimDescriptionOf(n->GetGUID());
  if (!person->m_desc) person->m_desc = &g_defaultDesc2;
  person->m_location[0] = person->m_location[1] = -16;
  person->m_level = 1;
  g_lotPeople.push_back(person);
  AddPersonToModule(person);
  person->Initialize();
  person->StartBehavior();
  if (const short init = person->GetTreeID(0)) person->m_treeSim->Gosub(person->GetBehavior(), nullptr, init);
  {
    char first[32] = {};
    const CasSimDescriptionS2C* d = person->m_desc;
    for (int i = 0; d && i < 31 && d->m_firstName[i]; ++i) first[i] = static_cast<char>(d->m_firstName[i]);
    std::printf("[sim] neighbour %d (%s) made: object %d (%s)\n", n->m_id, first, person->m_id, person->DebugName());
  }
  // GC: MakeNewOutOfWorldObject's GlobalDispatch(0xDE) -> HouseImpl::DoCommand: a new person redoes the
  // people's visitor types.
  HouseDispatch(0xde, person->m_id);
  return person;
}

// PC: a person the object module kills during play (a visitor or service NPC leaving the lot,
// ObjectModuleImpl::KillObject) is freed there, so it leaves the lot's list too.
void ForgetLotPerson(cXObjectImpl* object) {
  g_lotPeople.erase(std::remove(g_lotPeople.begin(), g_lotPeople.end(), object), g_lotPeople.end());
}

void RemoveFamilyFromLot() {
  s_familyOnLot = false;
  for (int player = 0; player < 2; ++player)
    if (_globals.GetSelectedPerson(player)) _globals.SetSelectedPerson(player, nullptr);
  for (cXPersonImpl* p : g_lotPeople) {
    RemovePersonFromModule(p);
    delete p;
  }
  g_lotPeople.clear();
}

float PlayerMotive(int player, int index) {
  cXPerson* p = _globals.GetSelectedPerson(player);
  return p ? static_cast<float>(p->GetMotive(index)) : 0.0f;
}

namespace {
void TracePerson(cXPersonImpl* person, bool named);
}  // namespace

void TraceSelectedPerson(int player) {
  cXPerson* selected = _globals.GetSelectedPerson(player);
  cXObject* self = selected ? selected->AsObject() : nullptr;
  auto* person = self ? dynamic_cast<cXPersonImpl*>(self->GetObjectImplementation()) : nullptr;
  TracePerson(person, false);
  // PC development aid: SIMMY_TRACE_IDS=98,126 traces those objects (people) too.
  static const char* ids = std::getenv("SIMMY_TRACE_IDS");
  if (player == 0 && ids && Globs_pObjectModule)
    for (const char* c = ids; *c;) {
      const int id = std::atoi(c);
      cXObject* o = Globs_pObjectModule->GetObjectFromID(id);
      auto* p = o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
      if (p && p != person) TracePerson(p, true);
      while (*c && *c != ',') ++c;
      if (*c == ',') ++c;
    }
}

namespace {
void TracePerson(cXPersonImpl* person, bool named) {
  if (!person || !person->m_treeSim) return;
  if (named)
    std::printf("[sim]   %s at %.1f,%.1f room %d sim %d:", person->DebugName(), person->m_pos[0], person->m_pos[1],
                person->GetData(0x1d), Globs_pObjectModule ? Globs_pObjectModule->GetSimFlag(person->m_id, ObjectModule__SimFlag{1}) : -1);
  const Motives& m = person->m_motives;
  std::printf("%s motives: energy %.1f comfort %.1f hunger %.1f hygiene %.1f bladder %.1f social %.1f fun %.1f "
              "mood %.1f room %.1f; deltas %d; stack", named ? "" : "[sim]  ", m.m_values[5], m.m_values[6], m.m_values[7], m.m_values[8], m.m_values[9],
              m.m_values[0xe], m.m_values[0xf], m.m_values[3], m.m_values[0xd], static_cast<int>(person->m_motiveDeltas.size()));
  for (int i = 0; i < person->m_treeSim->GetStackSize(); ++i) {
    StackElem* e = person->m_treeSim->GetNthElem(static_cast<short>(i));
    std::printf(" %04x:%d", static_cast<unsigned short>(e->GetTreeID()), e->m_node);
  }
  for (const cXPersonImpl::MotiveInc& inc : person->m_motiveDeltas)
    std::printf("; delta motive %d %+.4f/tick to %.0f", inc.m_motive, inc.m_delta, inc.m_limit);
  if (SAnimator2* a = person->m_animator)
    std::printf("; anim state %d skill %08x routing %d", a->m_animState, a->m_skillAnimId, a->m_routing);
  std::printf("\n");
}
}  // namespace
