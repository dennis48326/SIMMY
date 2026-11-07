// Port of the routing part of game/portal.cpp (cXPortal / cXPortalImpl: doors as the links between
// rooms) and the module's people and portal lists (game/objectmodule.cpp). A person routes within its
// room (XRoute::FindPath); to reach another room it goes through portals (cXPersonImpl::TryRoomRouting):
// FindBestPortal picks the portal of its room whose way to the target's room is shortest, and the
// person runs the portal's tree (entry 0xF), which walks it through to the other side.
//
// Each portal keeps a score per route id (GC cXPortalImpl +0x08, a float vector; PC: the portal part's
// cXMTObjectImpl::m_portalValues, which the save keeps): the distance from that route's start (a person,
// or a portal for GetDistToPortal's own search), or 10001 not known yet, 10000 no way, 1101 failed,
// 1001..1100 being walked through (BeginningPortalTree). Route ids are object data 6 of people and
// portals (FindAvailRouteID: the first free one).
//
// PC: a portal is a type-8 multi-tile part (IsPortalObject; GC: ObjectModuleImpl::ConstructObject makes
// those cXPortalImpls). The module's portal and people lists are its objects of those kinds, in its
// object order (GC: in the order AddObject added them).
#include "simmy/types_gen.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
cXObjectImpl* Impl(cXObject* o) { return o ? o->GetObjectImplementation() : nullptr; }
cXMTObjectImpl* PortalImpl(cXObjectImpl* o) { return o && IsPortalObject(o) ? dynamic_cast<cXMTObjectImpl*>(o) : nullptr; }

std::vector<cXObjectImpl*> Portals() {
  std::vector<cXObjectImpl*> out;
  if (!Globs_pObjectModule) return out;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
    if (PortalImpl(o)) out.push_back(o);
  return out;
}
std::vector<cXObjectImpl*> People() {
  std::vector<cXObjectImpl*> out;
  if (!Globs_pObjectModule) return out;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject)
    if (dynamic_cast<cXPersonImpl*>(o)) out.push_back(o);
  return out;
}
unsigned short RoomOf(cXObjectImpl* o) { return static_cast<unsigned short>(o->GetData(0x1d)); }
}  // namespace

// ---------------------------------------------------------------------------------------------------
// The module's lists (game/objectmodule.cpp)

// 800F9988
RET_UNKNOWN ObjectModuleImpl::GetPeople(int i) {
  const std::vector<cXObjectImpl*> people = People();
  return i >= 0 && static_cast<size_t>(i) < people.size() ? reinterpret_cast<RET_UNKNOWN>(people[static_cast<size_t>(i)]) : 0;
}
// 800F99A8
RET_UNKNOWN ObjectModuleImpl::GetNumPeople() { return static_cast<RET_UNKNOWN>(People().size()); }
// 800F99BC
RET_UNKNOWN ObjectModuleImpl::GetPortal(int i) {
  const std::vector<cXObjectImpl*> portals = Portals();
  return i >= 0 && static_cast<size_t>(i) < portals.size() ? reinterpret_cast<RET_UNKNOWN>(portals[static_cast<size_t>(i)]) : 0;
}
// 800F99CC
RET_UNKNOWN ObjectModuleImpl::GetNumPortals() { return static_cast<RET_UNKNOWN>(Portals().size()); }

// 800F84D8
// Every person plans its routes again (from their state 2) and every portal forgets its scores (house
// changes, live mode entered).
void ObjectModuleImpl::InvalidateAllRoutes() {
  for (cXObjectImpl* o : People())
    if (auto* p = dynamic_cast<cXPersonImpl*>(o)) p->InvalidateRoutes();
  PortalDirtyAllRoutes();
}

// ---------------------------------------------------------------------------------------------------
// Portals

// 8013048C
// The next part of the portal's group (round from the lead) that has a portal tree (entry 0xF); null when
// none but itself.
cXObjectImpl* PortalOtherSide(cXObjectImpl* portal) {
  auto* me = dynamic_cast<cXMTObjectImpl*>(portal);
  if (!me) return nullptr;
  portal->GetTreeID(0xf);
  // The ring of parts from this one (GC: +0x08 the next part, else +0x0C the lead).
  cXMTObjectImpl* first = me->GetFirstMultiTileObject();
  cXMTObjectImpl* p = me;
  for (int guard = 0; guard < 64; ++guard) {
    p = p->GetNextMultiTileObject();
    if (!p) p = first;
    if (!p || p == me) return nullptr;
    if (p->GetTreeID(0xf) != 0) return p;
  }
  return nullptr;
}

// 80132788
float PortalRouteScore(cXObjectImpl* portal, int id) {
  cXMTObjectImpl* p = PortalImpl(portal);
  if (!p || id < 1 || p->m_portalValues.size() < static_cast<size_t>(id & 0xffff)) return 0.0f;
  return p->m_portalValues[static_cast<size_t>(id - 1)];
}

// 8012E608
// The score grows to the id with 10001 (not known).
void PortalSetRouteScore(cXObjectImpl* portal, int id, float score) {
  cXMTObjectImpl* p = PortalImpl(portal);
  if (!p || id < 1) return;  // GC: id 0 writes before the vector
  while (p->m_portalValues.size() < static_cast<size_t>(id)) p->m_portalValues.push_back(10001.0f);
  p->m_portalValues[static_cast<size_t>(id - 1)] = score;
}

// 80130824
// The first id (1..9999) no person and no portal has as its route id (object data 6); 0: none left.
int PortalFindAvailRouteID() {
  const std::vector<cXObjectImpl*> people = People();
  const std::vector<cXObjectImpl*> portals = Portals();
  for (int id = 1; id <= 9999; ++id) {
    bool taken = false;
    for (cXObjectImpl* o : people)
      if (o->GetData(6) == id) taken = true;
    for (cXObjectImpl* o : portals)
      if (o->GetData(6) == id) taken = true;
    if (!taken) return id;
  }
  return 0;
}

// 80130960
void PortalDirtyAllRoutes() {
  for (cXObjectImpl* o : Portals())
    if (cXMTObjectImpl* p = PortalImpl(o))
      for (float& v : p->m_portalValues) v = 10001.0f;
}

// 801314B0
void PortalClearRoute(int id, float score) {
  for (cXObjectImpl* o : Portals()) PortalSetRouteScore(o, id, score);
}

// 8013156C
// The scores of route `id` that are distances or "being walked through" (0..1100) become unknown.
void PortalClearRouteScores(int id) {
  for (cXObjectImpl* o : Portals()) {
    const float v = PortalRouteScore(o, id);
    if (0.0f <= v && v <= 1100.0f) PortalSetRouteScore(o, id, 10001.0f);
  }
}

// 80130A34
// The person's route id (made when it has none), every portal's score for it unknown.
int PortalInitRoute(cXObjectImpl* person) {
  int id = person->GetData(6);
  if (id == 0) {
    id = PortalFindAvailRouteID();
    person->SetData(6, static_cast<short>(id));
  }
  PortalClearRoute(id, 10001.0f);
  if (std::getenv("SIMMY_ROUTE_TRACE")) std::printf("[route]   %s: route id %d (data 6 %d)\n", person->DebugName(), id, person->GetData(6));
  return 1;
}

// 80130AC8
// The walking distance from portal `from` to portal `to` through rooms: `from`'s own route id (made
// when it has none) holds a shortest-path search from it over all portals (along a room between its
// portals, CalcShortDistance + 1, and through a portal to its other side), done once until the scores
// are made unknown again. 10000: no way.
float PortalDistToPortal(cXObjectImpl* from, cXObjectImpl* to) {
  int id = from->GetData(6);
  if (id == 0) {
    id = PortalFindAvailRouteID();
    from->SetData(6, static_cast<short>(id));
    PortalClearRoute(id, 10001.0f);
  }
  cXMTObjectImpl* fp = PortalImpl(from);
  bool known = false;
  if (fp && static_cast<unsigned int>(id) <= fp->m_portalValues.size()) known = PortalRouteScore(from, id) != 10001.0f;
  if (!known) {
    PortalClearRoute(id, 10000.0f);
    PortalSetRouteScore(from, id, 0.0f);
    std::vector<cXObjectImpl*> open{from};
    const std::vector<cXObjectImpl*> portals = Portals();
    while (!open.empty()) {
      int best = static_cast<int>(open.size()) - 1;
      for (int j = best - 1; j >= 0; --j)
        if (PortalRouteScore(open[static_cast<size_t>(j)], id) < PortalRouteScore(open[static_cast<size_t>(best)], id)) best = j;
      cXObjectImpl* cur = open[static_cast<size_t>(best)];
      open.erase(open.begin() + best);
      cXObjectImpl* other = PortalOtherSide(cur);
      for (cXObjectImpl* x : portals) {
        if (RoomOf(x) != RoomOf(cur) && x != other) continue;
        const float sx = PortalRouteScore(x, id), sc = PortalRouteScore(cur, id);
        if (!(sc < sx)) continue;
        const float d = static_cast<float>(static_cast<float>(sc + static_cast<float>(cur->CalcShortDistance(x))) + 1.0);
        if (d < PortalRouteScore(x, id)) {
          PortalSetRouteScore(x, id, d);
          open.push_back(x);
        }
      }
    }
  }
  return to ? PortalRouteScore(to, id) : 0.0f;
}

// 801312A4
// The shortest walk from `from` through a portal of its room and one of `to`'s room to `to`.
float PortalEstimateDistance(cXObjectImpl* from, cXObjectImpl* to) {
  float best = 10000.0f;
  const std::vector<cXObjectImpl*> portals = Portals();
  for (cXObjectImpl* p : portals) {
    if (RoomOf(p) != RoomOf(from)) continue;
    const double d0 = p->CalcShortDistance(from);
    for (cXObjectImpl* q : portals) {
      if (RoomOf(q) != RoomOf(to)) continue;
      const float d = static_cast<float>(static_cast<float>(d0 + PortalDistToPortal(p, q)) + q->CalcShortDistance(to));
      if (d < best) best = d;
    }
  }
  return best;
}

// 801316A0
// The portal of the person's room to go through toward `target`'s room: each portal's score for the
// person's route (worked out once: the person's distance to it, outdoors with the walk penalties, plus
// the way on from it to a portal of the target's room that going through it brings closer, plus that
// portal's distance to the target; at most 1000; 10000 when none), the best that has not failed.
cXObjectImpl* PortalFindBest(cXObjectImpl* person, cXObjectImpl* target) {
  const int id = person->GetData(6);
  if (id == 0) return nullptr;
  cXObjectImpl* best = nullptr;
  const std::vector<cXObjectImpl*> portals = Portals();
  const unsigned short personRoom = RoomOf(person), targetRoom = RoomOf(target);
  for (cXObjectImpl* p : portals) {
    if (RoomOf(p) != personRoom) continue;
    float s = PortalRouteScore(p, id);
    if (s == 10001.0f) {
      PortalSetRouteScore(p, id, 10000.0f);
      const double d0 = personRoom == 0 ? person->CalcDistanceWithPentalties(p) : person->CalcShortDistance(p);
      for (cXObjectImpl* q : portals) {
        if (RoomOf(q) != targetRoom) continue;
        const float sq = PortalRouteScore(q, id);
        if (!(static_cast<double>(sq) == 10001.0 || static_cast<double>(sq) <= 1000.0)) continue;
        const float dpq = PortalDistToPortal(p, q);
        if (dpq == 10000.0f) continue;
        cXObjectImpl* other = PortalOtherSide(p);
        if (!other) continue;  // GC: reads a null portal
        const float doq = PortalDistToPortal(other, q);
        if (!(doq <= dpq)) continue;
        const double dqt = targetRoom == 0 ? q->CalcDistanceWithPentalties(target) : q->CalcShortDistance(target);
        float total = static_cast<float>(static_cast<double>(static_cast<float>(d0 + dpq)) + dqt);
        if (1000.0f < total) total = 1000.0f;
        if (total < PortalRouteScore(p, id)) PortalSetRouteScore(p, id, total);
      }
      s = PortalRouteScore(p, id);
    }
    if (s != 10000.0f && s != 1101.0f) {
      if (best && PortalRouteScore(best, id) <= s) continue;
      best = p;
    }
  }
  return best;
}

// 80131B98
// Going through the portal failed: it and its other side are marked failed (1101) for the person's
// route and its other scores are worked out again.
void PortalFailedTree(cXObjectImpl* person, cXObjectImpl* portal) {
  const int id = person->GetData(6);
  PortalSetRouteScore(portal, id, 1101.0f);
  PortalSetRouteScore(PortalOtherSide(portal), id, 1101.0f);
  PortalClearRouteScores(id);
}

// 80131C7C
// The person starts through the portal: a portal it already went through on this route (1001..1100)
// is marked failed (no going back and forth); else it and its other side get a score above every other
// "being walked through" score of the room (1001 and up).
void PortalBeginningTree(cXObjectImpl* person, cXObjectImpl* portal) {
  const int id = person->GetData(6);
  const float s = PortalRouteScore(portal, id);
  if (1001.0f <= s && s <= 1100.0f) {
    PortalSetRouteScore(portal, id, 1101.0f);
    PortalSetRouteScore(PortalOtherSide(portal), id, 1101.0f);
    return;
  }
  if (s == 1101.0f) return;
  const unsigned short room = RoomOf(person);
  double next = 1001.0;
  for (cXObjectImpl* p : Portals()) {
    if (RoomOf(p) != room) continue;
    const double v = PortalRouteScore(p, id);
    if (1001.0 <= v && v <= 1100.0 && next <= v) next = static_cast<float>(v + 1.0);
  }
  PortalSetRouteScore(portal, id, static_cast<float>(next));
  PortalSetRouteScore(PortalOtherSide(portal), id, static_cast<float>(next));
}
