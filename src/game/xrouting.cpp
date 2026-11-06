// Port of game/xrouting.cpp (XRoute: goals around a routing slot, the path to one) and the routing part
// of game/slots.cpp (RoutingSlot). See routing.hpp. Positions: FTilePt pairs in 1/16 tiles ([0] the
// object location's +0x68, [1] +0x6C; the tile is CTilePt([1] >> 4, [0] >> 4)); "tile axes" X = [1] / 16,
// Y = [0] / 16. Directions are eighths of a turn: 0 toward -[0], 2 toward +[1], 4 +[0], 6 -[1].
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <queue>

// ---------------------------------------------------------------------------------------------------
// RoutingSlot

// 80142994
RoutingSlot::RoutingSlot() {}
// 8014548C
RoutingSlot::~RoutingSlot() {}

// 80142A28
// PC: FromDescriptor (the GC ctor reads a SlotDescriptor made by the quickdata slot list).
RoutingSlot RoutingSlot::FromDescriptor(const unsigned char* d) {
  RoutingSlot s;
  s.m_x = ERQuickdata::F32(d, 4);
  s.m_y = ERQuickdata::F32(d, 8);
  s.m_alt = ERQuickdata::F32(d, 0xc);
  for (int i = 0; i < 3; ++i) s.m_multiplier[i] = ERQuickdata::S32(d, 0x1c + 4 * i);
  s.m_flags = ERQuickdata::U32(d, 0x28);
  s.m_snapTarget = ERQuickdata::S32(d, 0x2c);
  s.m_minDist = ERQuickdata::S32(d, 0x30);
  s.m_maxDist = ERQuickdata::S32(d, 0x34);
  s.m_optimalDist = ERQuickdata::S32(d, 0x38);
  s.m_gradient = ERQuickdata::F32(d, 0x3c);
  s.m_facing = ERQuickdata::S32(d, 0x40);
  s.m_resolution = ERQuickdata::S32(d, 0x44);
  return s;
}

// 80142B54
void RoutingSlot::SetIsOnTopOfObject() { m_flags &= ~0xffu; }
// 80142B64
void RoutingSlot::AllowDirection(int dir) {
  if (dir >= 0 && dir < 8) m_flags |= 1u << dir;
}
// 80142B84
void RoutingSlot::AllowAnyRotation() { m_flags |= kAnyRotation; }
// 80142B94
void RoutingSlot::AllowAnyFacing() { m_facing = kFaceAnywhere; }
// 80142BA0
void RoutingSlot::FaceTowardsObject() { m_facing = kFaceTowards; }
// 80142BAC
void RoutingSlot::FaceAwayFromObject() { m_facing = kFaceAway; }
// 80142BB8
void RoutingSlot::SetFacingDirection(int dir) {
  if (dir >= 0 && dir < 8) m_facing = dir;
}
// 80142BC8
void RoutingSlot::SetDistances(int minDist, int maxDist, int optimal) {
  m_minDist = minDist;
  m_maxDist = maxDist;
  m_optimalDist = optimal;
}
// 80142BD8
void RoutingSlot::SetTileDistances(float minDist, float maxDist, float optimal) {
  m_minDist = static_cast<int>(minDist * 16.0f);
  m_maxDist = static_cast<int>(maxDist * 16.0f);
  m_optimalDist = static_cast<int>(optimal * 16.0f);
}
// 80142C30
void RoutingSlot::SetMultiplier(int position, int value) {
  if (position >= 0 && position < 3) m_multiplier[position] = value;
}
// 801454C0
bool RoutingSlot::IsOnTopOfObject() const { return (m_flags & 0xff) == 0; }

// 80143828
// SlotLoader::Load's routing part (type 3 entries), PC: once per definition. The definition's slot
// list id (+0x28): from 1000 in the semi-global file (ResFile +0x34), else the object's own; the file's
// lists (+0x28, 8 bytes each: the entries, then the id) are searched by id (QuickDataSlotDescList::
// LoadFromDataID, FindRes); entries are 0x48 bytes (+0 the type).
namespace {
// The routing slots (type 3) of slot list `id` of object file `file`.
void LoadRoutingSlots(const unsigned char* file, int id, std::vector<RoutingSlot>& slots) {
  const auto* lists = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x28)) : nullptr;
  if (!lists) return;
  const unsigned int count = ERQuickdata::U32(lists - 4, 0);
  for (unsigned int i = 0; i < count; ++i) {
    if (ERQuickdata::S32(lists, i * 8 + 4) != id) continue;
    const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(lists, i * 8));
    const unsigned int n = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
    for (unsigned int k = 0; k < n; ++k) {
      const unsigned char* d = entries + k * 0x48;
      if (ERQuickdata::S16(d, 0) == 3) slots.push_back(RoutingSlot::FromDescriptor(d));
    }
    break;
  }
}
}  // namespace

const std::vector<RoutingSlot>& ObjectRoutingSlots(cXObjectImpl* object) {
  static std::map<const ObjDefinition*, std::vector<RoutingSlot>> cache;
  static const std::vector<RoutingSlot> none;
  if (!object || !object->m_def) return none;
  auto it = cache.find(object->m_def);
  if (it != cache.end()) return it->second;
  std::vector<RoutingSlot>& slots = cache[object->m_def];
  const short id = ERQuickdata::S16(object->m_def, 0x28);
  const unsigned char* file = SelectorResFile(object->m_selector);
  if (id == 0 || !file) return slots;
  if (id > 999) file = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x34));
  LoadRoutingSlots(file, id, slots);
  return slots;
}

// SlotLoader::Load's object-slot part (type 0 entries, ObjectSlot::ObjectSlot 80142CAC): the flags
// (descriptor +0x14; ObjectSlot +0x10) of `object`'s object slot `index` (the index-th type 0 entry of
// its slot list); -1 without one. Flag 2: a person put in it keeps where it is drawn (TrySnap does not
// snap the animator).
int ObjectSlotFlags(cXObjectImpl* object, int index) {
  const unsigned char* d = ObjectSlotDescriptor(object, index);
  return d ? ERQuickdata::S32(d, 0x14) : -1;
}

// ObjectSlot::SetHeight (80142D64): the slot's standard height (descriptor +0x18; ObjectSlot +8) and
// altitude (+0xC; +0x1C): 1 the floor (0), 2 a low surface (1.0206), 3 / 4 / 8 a counter or table
// (1.6330); 0: 1 when the descriptor gives no altitude, else 5 with its own; others keep theirs.
void ObjectSlotHeight(const unsigned char* d, int& height, float& alt) {
  height = ERQuickdata::S32(d, 0x18);
  alt = ERQuickdata::F32(d, 0xc);
  switch (height) {
    case 0: height = alt == 0.0f ? 1 : 5; break;
    case 1: alt = 0.0f; break;
    case 2: alt = 1.0206207f; break;
    case 3: case 4: case 8: alt = 1.6329932f; break;
    default: break;
  }
}

// cXObjectImpl::HierCountSlots (GC: the object's slot vector +0x90, 0x20 bytes an ObjectSlot): the
// number of object slots (type 0 entries) of `object`'s slot list.
int ObjectSlotCount(cXObjectImpl* object) {
  int n = 0;
  while (ObjectSlotDescriptor(object, n)) ++n;
  return n;
}

// ObjectSlot::ObjectSlot (80142CAC): the descriptor of `object`'s object slot `index` (+4 x, +8 y in
// 1/16 tiles, +0xC the height in altitude units: ObjectSlot +0x14 / +0x18 / +0x1C; +0x14 the flags),
// or null.
const unsigned char* ObjectSlotDescriptor(cXObjectImpl* object, int index) {
  if (!object || !object->m_def || index < 0) return nullptr;
  const short id = ERQuickdata::S16(object->m_def, 0x28);
  const unsigned char* file = SelectorResFile(object->m_selector);
  if (id == 0 || !file) return nullptr;
  if (id > 999) file = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x34));
  const auto* lists = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x28)) : nullptr;
  if (!lists) return nullptr;
  const unsigned int count = ERQuickdata::U32(lists - 4, 0);
  for (unsigned int i = 0; i < count; ++i) {
    if (ERQuickdata::S32(lists, i * 8 + 4) != id) continue;
    const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(lists, i * 8));
    const unsigned int n = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
    int k = 0;
    for (unsigned int e = 0; e < n; ++e) {
      const unsigned char* d = entries + e * 0x48;
      if (ERQuickdata::S16(d, 0) != 0) continue;
      if (k++ == index) return d;
    }
    break;
  }
  return nullptr;
}

// PC stand-in for ObjectModuleImpl::LoadGlobalRoutingSlots (800F461C): loaded on first use (GC: by the module,
// +0x2078; GetGlobalRoutingSlot 800F99E0 / GetNumGlobalRoutingSlots 800F99F0 index it): slot list 100
// of the global object file, the spots any object can be approached at (by the global trees).
const std::vector<RoutingSlot>& GlobalRoutingSlots() {
  static std::vector<RoutingSlot> slots;
  static bool loaded = false;
  if (loaded) return slots;
  const ERQuickdata* qd = Globs_pObjectFolder ? Globs_pObjectFolder->m_objects : nullptr;
  if (!qd) return slots;
  loaded = true;
  const void* globals = qd->getTable("GlobalResFile");
  const void* row = globals ? qd->getRow(globals, "global") : nullptr;
  LoadRoutingSlots(row ? static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 0)) : nullptr, 100, slots);
  return slots;
}

// ---------------------------------------------------------------------------------------------------
// Helpers

namespace {
cXObjectImpl* Impl(cXObject* o) { return o ? o->GetObjectImplementation() : nullptr; }

// The eighth of a turn from `from` toward `to` (FTilePt), -1 on the same tile.
int DirectionBetween(const int from[2], const int to[2]) {
  if ((from[0] >> 4) == (to[0] >> 4) && (from[1] >> 4) == (to[1] >> 4)) return -1;
  int d0 = to[0] - from[0], d1 = to[1] - from[1];
  const int a1 = std::abs(d1);
  const bool neg0 = d0 < 0;
  d0 = std::abs(d0);
  int dir = 2 * a1 < d0 ? 4 : (2 * d0 < a1 ? 2 : 3);
  if (d1 < 0) {
    if (dir == 2) dir = 6;
    else if (dir == 3) dir = 5;
  }
  if (neg0) {
    if (dir == 4) dir = 0;
    else if (dir == 3) dir = 1;
    else if (dir == 5) dir = 7;
  }
  return dir;
}

// 80152A78
// (x, y) in the frame of something at `origin` facing `dir` (y forward is -y), to FTilePt. GC: odd
// directions through ObjectRotationTf and the iso frame; PC: the same 45 degree turn directly.
void TransformToWorldCoords(float x, float y, const int origin[2], int dir, int out[2]) {
  float p1 = x, p2 = y;
  switch (dir & 7) {
    case 0: break;
    case 2: p1 = -y, p2 = x; break;
    case 4: p1 = -x, p2 = -y; break;
    case 6: p1 = y, p2 = -x; break;
    default: {
      const float a = static_cast<float>(dir & 7) * 0.7853982f;
      const float c = std::cos(a), s = std::sin(a);
      p1 = std::round(x * c - y * s);
      p2 = std::round(x * s + y * c);
      break;
    }
  }
  out[1] = static_cast<int>(p1 + static_cast<float>(origin[1]));
  out[0] = static_cast<int>(p2 + static_cast<float>(origin[0]));
}

// 800E28AC cXObject::GetWallBlockFlagsAtTile
// The walls round `tile` as direction bits (segment 1: 0x40, 2: 1, 4: 4, 8: 0x10), turned so bit 0
// is direction `dir`. GC: also the blocking flags of objects on the tile with data 0x35 set (vtable
// 0x4A4); PC: walls only.
unsigned int WallBlockFlagsAtTile(CTilePt& tile, int dir) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w || w->OutOfBounds(tile)) return 0;
  TileWalls walls = w->GetWall(tile);
  unsigned int f = 0;
  if (walls.HasWall(TileWallsSegment(1))) f |= 0x40;
  if (walls.HasWall(TileWallsSegment(2))) f |= 1;
  if (walls.HasWall(TileWallsSegment(4))) f |= 4;
  if (walls.HasWall(TileWallsSegment(8))) f |= 0x10;
  if (dir != 0) {
    f <<= (8 - dir);
    f = (f | f >> 8) & 0xff;
  }
  return f;
}

// cXObjectImpl::ComputeRect (as collisionmap.cpp's): the footprint in tile axes (x0, y0, x1, y1).
void Footprint(cXObjectImpl* o, float r[4]) {
  const int y = o->m_location[0], x = o->m_location[1];
  int rect[4] = {y + 8, x + 8, y - 8, x - 8};
  const unsigned int insets = static_cast<unsigned short>(ERQuickdata::S16(o->m_def, 0x9a));
  if (insets) {
    const int side[4] = {static_cast<int>(insets & 0xf), static_cast<int>(insets >> 4 & 0xf),
                         static_cast<int>(insets >> 8 & 0xf), static_cast<int>(insets >> 12 & 0xf)};
    const int q = (8 - o->GetData(1) % 8) / 2;
    auto at = [&](int i) { return side[((i % 4) + 4) % 4]; };
    rect[2] += at(q);
    rect[1] -= at(q + 1);
    rect[0] -= at(q + 2);
    rect[3] += at(q + 3);
  }
  r[0] = rect[3] / 16.0f, r[1] = rect[2] / 16.0f, r[2] = rect[1] / 16.0f, r[3] = rect[0] / 16.0f;
}

float SegmentDistance(float px, float py, float ax, float ay, float bx, float by) {
  const float dx = bx - ax, dy = by - ay;
  const float len = dx * dx + dy * dy;
  float t = len > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / len : 0.0f;
  t = std::max(0.0f, std::min(1.0f, t));
  const float cx = ax + dx * t - px, cy = ay + dy * t - py;
  return std::sqrt(cx * cx + cy * cy);
}

// PC (GC: cXObjectImpl::CanPlace for a person, and the room partition's walls and penalty rects): a
// person of radius `r` cannot stand at (X, Y) (tile axes): off the lot, a blocked tile, a wall (not a
// door or window) within r, or an object's footprint (not people, doors, pass-through objects or
// `ignore`'s group), or another person (AddPeople).
struct Blocker {
  const std::vector<cXObjectImpl*>* m_ignore = nullptr;
  float m_r = 0.24f;
  std::vector<EVec2> m_people;  // tile axes
  // The room partition's people (FindPath: every object of the room as a penalty rect, people grown by
  // gPersonWidth 6/16; XRoute::ShouldIgnore: not the router, not seated (a container), not the route's
  // object, not farther than 3 tiles from the router).
  void AddPeople(cXObject* router, cXObject* target) {
    cXObjectImpl* self = Impl(router);
    cXObjectImpl* goal = Impl(target);
    if (!self || !Globs_pObjectModule) return;
    // PC: the people are cXPersonImpls (the lot file's saved type-2 objects do not run).
    ObjectModuleImpl* module = Globs_pObjectModule;
    for (int id = 1; id <= module->GetNumObjects(); ++id) {
      cXObject* x = module->GetObjectFromID(id);
      auto* o = x ? dynamic_cast<cXPersonImpl*>(x->GetObjectImplementation()) : nullptr;
      if (!o) continue;
      if (o == self || o == goal || o->GetType() != 2 || o->m_level != self->m_level) continue;
      if (o->GetContainer() || (o->m_location[1] >> 4) <= 0 || (o->m_location[0] >> 4) <= 0) continue;
      if (self->CalcShortDistance(o) > 3.0) continue;
      m_people.push_back(EVec2{o->m_location[1] / 16.0f, o->m_location[0] / 16.0f});
    }
  }
  // A person within reach of (X, Y).
  bool Person(float X, float Y) const {
    const float d = m_r + 6.0f / 16.0f;
    for (const EVec2& p : m_people)
      if ((p.x - X) * (p.x - X) + (p.y - Y) * (p.y - Y) < d * d) return true;
    return false;
  }
  bool operator()(float X, float Y) const {
    if (!m_people.empty() && Person(X, Y)) return true;
    cFixedWorldImpl* w = Globs_pFixedWorld;
    CTilePt here(static_cast<int>(std::floor(X)), static_cast<int>(std::floor(Y)), 0);
    if (!w || X < 0.0f || Y < 0.0f || w->OutOfBounds(here) || (w->GetFlags(here) & 0x10)) return true;
    for (int tx = here.GetX() - 1; tx <= here.GetX() + 1; ++tx) {
      for (int ty = here.GetY() - 1; ty <= here.GetY() + 1; ++ty) {
        CTilePt t(tx, ty, 0);
        if (w->OutOfBounds(t)) continue;
        TileWalls walls = w->GetWall(t);
        if (!walls.HasWall()) continue;
        const float x0 = static_cast<float>(tx), y0 = static_cast<float>(ty), x1 = x0 + 1.0f, y1 = y0 + 1.0f;
        auto edge = [&](int seg, float ax, float ay, float bx, float by) {
          if (!walls.HasWall(TileWallsSegment(seg))) return false;
          const int style = walls.GetStyle(TileWallsSegment(seg));
          if (style == 3 || style == 5 || style == 6 || style == 0xf || style == 0x17) return false;
          return SegmentDistance(X, Y, ax, ay, bx, by) < m_r;
        };
        if (edge(1, x0, y0, x0, y1) || edge(4, x1, y0, x1, y1) || edge(2, x0, y0, x1, y0) || edge(8, x0, y1, x1, y1))
          return true;
        if (walls.HasWall(TileWallsSegment(TileWallsSegment::kDiagonal1)) && SegmentDistance(X, Y, x1, y0, x0, y1) < m_r)
          return true;
        if (walls.HasWall(TileWallsSegment(TileWallsSegment::kDiagonal2)) && SegmentDistance(X, Y, x0, y0, x1, y1) < m_r)
          return true;
      }
    }
    ObjectModuleImpl* module = Globs_pObjectModule;
    if (!module) return false;
    for (int tx = here.GetX() - 1; tx <= here.GetX() + 1; ++tx) {
      for (int ty = here.GetY() - 1; ty <= here.GetY() + 1; ++ty) {
        CTilePt t(tx, ty, 0);
        cXObjectImpl* o = Impl(module->GetObjectFromID(module->GetTileObjectID(t)));
        if (!o || !o->m_def) continue;
        if (m_ignore && std::find(m_ignore->begin(), m_ignore->end(), o) != m_ignore->end()) continue;
        const int type = objdef::Type(o->m_def);
        if (((o->GetData(8) >> 4) & 1) || type == 2 || type == 8) continue;
        float f[4];
        Footprint(o, f);
        const float cx = std::max(f[0], std::min(X, f[2])), cy = std::max(f[1], std::min(Y, f[3]));
        if ((cx - X) * (cx - X) + (cy - Y) * (cy - Y) < m_r * m_r) return true;
      }
    }
    return false;
  }
  // The straight walk from a to b stays clear (sampled every 1/16 tile).
  bool Clear(float ax, float ay, float bx, float by) const {
    const float len = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    const int steps = std::max(1, static_cast<int>(len * 16.0f));
    for (int i = 1; i <= steps; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(steps);
      if ((*this)(ax + (bx - ax) * t, ay + (by - ay) * t)) return false;
    }
    return true;
  }
};

// The objects a route to `object` may touch: it and its multi-tile group's parts (same file and master
// id, facing the same way, next to it).
std::vector<cXObjectImpl*> RouteIgnores(cXObject* object) {
  std::vector<cXObjectImpl*> out;
  cXObjectImpl* impl = Impl(object);
  if (!impl) return out;
  out.push_back(impl);
  const short master = impl->m_def ? objdef::MasterID(impl->m_def) : 0;
  if (master == 0) return out;
  for (cXObjectImpl* o = Globs_pObjectModule ? Globs_pObjectModule->m_first : nullptr; o; o = o->m_nextObject) {
    if (o == impl || !o->m_selector || !impl->m_selector || o->m_selector->m_resFile != impl->m_selector->m_resFile ||
        objdef::MasterID(o->m_def) != master)
      continue;
    if (std::abs(o->m_location[0] - impl->m_location[0]) <= 0x40 && std::abs(o->m_location[1] - impl->m_location[1]) <= 0x40)
      out.push_back(o);
  }
  return out;
}

// SetDirectionForGoalSearch (GC 80157FB8): how the person faces at a goal in direction `dir` from an
// object facing `objDir`: no slot directions, toward the object's front (objDir + 4); else by the
// slot's facing (away: dir, toward: dir + 4, anywhere: -1, else objDir + facing).
int GoalFacing(const RoutingSlot& slot, int objDir, int dir) {
  int face;
  if ((slot.m_flags & 0xff) == 0) face = objDir + 4;
  else if (slot.m_facing == RoutingSlot::kFaceAway) face = dir;
  else if (slot.m_facing == RoutingSlot::kFaceTowards) face = dir + 4;
  else if (slot.m_facing == RoutingSlot::kFaceAnywhere) return -1;
  else face = objDir + slot.m_facing;
  return ((face % 8) + 8) % 8;
}
}  // namespace

bool PersonCanStandAt(cXObject* person, cXObject* near, const int pos[2]) {
  std::vector<cXObjectImpl*> ignore = RouteIgnores(near);
  if (cXObjectImpl* p = Impl(person)) ignore.push_back(p);
  Blocker blocked{&ignore};
  blocked.AddPeople(person, near);
  return !blocked(pos[1] / 16.0f, pos[0] / 16.0f);
}

void RoutingSlotPosition(cXObjectImpl* object, const RoutingSlot& slot, int out[2]) {
  const int origin[2] = {object->m_location[0], object->m_location[1]};
  TransformToWorldCoords(slot.m_x, slot.m_y, origin, object->GetData(1) & 7, out);
}

// ---------------------------------------------------------------------------------------------------
// XRoute

// 80152D18
XRoute::XRoute() {}

// 80152D74
XRoute::XRoute(cXObject* person, cXObject* object, RoutingSlot* slot) { Construct(person, object, slot); }

// 80152DDC
// GC: also the person's "route id" (person data 0x48) into +0x9C.
void XRoute::Construct(cXObject* person, cXObject* object, RoutingSlot* slot) {
  m_person = person;
  m_object = object;
  if (slot) m_slot = *slot;
  m_goal = -1;
  m_maxScore = -1;
  if (cXObjectImpl* p = Impl(person)) m_personPos[0] = p->m_location[0], m_personPos[1] = p->m_location[1];
  m_first = 1;
  m_failure = 0;
  m_blocker = 0;
  m_maxGoals = 0x30;
}

// 80152F74 XRoute::CountGoals (inline in the header)
// 80152F88 XRoute::GetNthGoal (inline)
// 8015303C XRoute::GetRoutingSlot (inline)

// 801534CC
void XRoute::ResetGoals() {
  m_goals.clear();
  m_goal = -1;
  m_maxScore = -1;
}

// 80153044
void XRoute::AddGoal(RouteGoal& goal) {
  m_goals.push_back(goal);
  if (m_maxScore < goal.m_score) m_maxScore = goal.m_score;
}

// 8015282C
// GC: room to stand is cXObjectImpl::CanPlace (vtable 0x134) for the person, whose placement error
// says what is in the way (7 a wall: 3; 0xB an object: 1, a person 5); the conflict may only be the
// route's object itself (+0x80), not another part of its multi-tile group (a goal on the next tile of
// a 2x2 table is in the table). PC: Blocker (a person nearby on the spot: 5).
int XRoute::EvalTileForGoal(const int pos[2], int dir) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  cXObjectImpl* person = Impl(m_person);
  cXObjectImpl* object = Impl(m_object);
  CTilePt tile(pos[1] >> 4, pos[0] >> 4, person ? person->m_level : 1);
  if (!w || pos[0] < 0 || pos[1] < 0 || w->OutOfBounds(tile)) return 0;
  if (!(m_slot.m_flags & RoutingSlot::kIgnoreRooms) && object) {
    CTilePt at = object->GetCTilePt();
    if (w->GetRoom(tile) != w->GetRoom(at)) return 2;
    if (dir != -1) {
      const unsigned int f = WallBlockFlagsAtTile(tile, (dir + 4) & 7);
      if ((f & 1) || ((dir & 1) && (f & 0x82))) return 3;
    }
  }
  std::vector<cXObjectImpl*> ignore;
  if (object) ignore.push_back(object);
  Blocker blocked{&ignore};
  blocked.AddPeople(m_person, m_object);
  if (blocked.Person(pos[1] / 16.0f, pos[0] / 16.0f)) return 5;
  blocked.m_people.clear();
  if (blocked(pos[1] / 16.0f, pos[0] / 16.0f)) {
    m_blocker = 0;
    return 1;
  }
  return 6;
}

// 80158058
// The person's own spot, facing its way. A person on a seat (its container is a chair) starts from the
// free side of the seat (the chair definition's side mask +0x84, turned by the chair's facing) nearest
// the object: m_startDirs is that side's bit.
void XRoute::ChooseStartingPoint() {
  cXObjectImpl* p = Impl(m_person);
  m_startDirs = 0;
  if (!p) return;
  m_start[0] = p->m_location[0], m_start[1] = p->m_location[1];
  cXObjectImpl* seat = Impl(p->GetContainer());
  cXObjectImpl* target = Impl(m_object);
  if (!seat || !seat->IsChair() || !seat->m_def || !target) return;
  const int sides = ERQuickdata::S16(seat->m_def, 0x84);
  static const int step[8][2] = {{-16, 0}, {-16, 16}, {0, 16}, {16, 16}, {16, 0}, {16, -16}, {0, -16}, {-16, -16}};
  bool found = false;
  int best = 0;
  const int here[2] = {m_start[0], m_start[1]};
  for (int c = 0; c < 8; ++c) {
    if (!((sides >> c) & 1)) continue;
    const int d = (c + seat->GetData(1)) & 7;
    const int pos[2] = {here[0] + step[d][0], here[1] + step[d][1]};
    const int r = EvalTileForGoal(pos, -1);
    if (r < 5 || r > 6) continue;
    const int dx = target->m_location[1] - pos[1], dy = target->m_location[0] - pos[0];
    const int d2 = dx * dx + dy * dy;
    if (!found || d2 < best) {
      m_start[0] = pos[0], m_start[1] = pos[1];
      m_startDirs = 1 << c;
      best = d2;
      found = true;
    }
  }
}

// 8015325C
bool XRoute::IsPersonSittingOnChairGoal(cXObject* person) {
  cXObjectImpl* p = Impl(person);
  if (!p) return false;
  for (const RouteGoal& g : m_goals) {
    if (g.m_object == 0) continue;
    cXObjectImpl* chair = Impl(p->GetObjectFromID(g.m_object));
    if (chair && chair->IsChair() && Impl(p->GetContainer()) == chair) return true;
  }
  return false;
}

// 8015597C
// GC: a big room (area over 250) allows only 8 goals; the person's data 0x49 holds its route id while
// the goals are made.
void XRoute::BuildGoalList() {
  ChooseStartingPoint();
  ConstructGoals();
}

// 80158318
// With slot directions: for each allowed direction (turned by the object's facing unless absolute)
// not walled off at the object's tile, rows of spots from the minimum to the maximum distance in front
// (a row as wide as it is far, `resolution` apart), each scored 100 - |distance - optimal| x gradient
// + the slot's standing score (x10) (random +-10 with random scoring, +25 where no wall is beside the
// direction, +20 at the person's own spot) and kept if EvalTileForGoal finds it good; the farther rows
// stop at a wall. Without directions: the one spot at the slot's offset (score 100). No goal: the
// failure says why (8 an object in the way, 9 a seat taken, 0xB walls, 0xC another room, 0xD a person).
// GC: sitting slots also look for seats (chairs facing the spot) next to each spot; PC: not ported.
void XRoute::ConstructGoals() {
  ResetGoals();
  cXObjectImpl* object = Impl(m_object);
  if (!object) return;
  int objDir = object->GetData(1);
  if (m_slot.m_flags & RoutingSlot::kAbsolute) objDir = 0;
  const int origin[2] = {object->m_location[0], object->m_location[1]};
  if (m_slot.m_flags & RoutingSlot::kAverageLocation)
    SIMMY_STUB("XRoute::ConstructGoals: the average location of a multi-tile object (vtable 0x34C)", 0x80158318);
  bool anyBlocked = false, anyObject = false, anyPerson = false, anyWall = false, anyRoom = false, anyOut = false;
  auto note = [&](int r) {
    switch (r) {
      case 0: anyOut = true; break;
      case 1: anyObject = true; break;
      case 2: anyRoom = true; break;
      case 3: anyWall = true; break;
      case 5: anyPerson = true; break;
      default: break;
    }
  };
  if (m_slot.m_flags & 0xff) {
    const int minD = m_slot.m_minDist, maxD = m_slot.m_maxDist, opt = m_slot.m_optimalDist;
    if (minD > 0x200 || maxD < 1 || maxD > 0x200 || maxD < minD || opt < minD || maxD < opt) return;
    const int sitting = m_slot.m_multiplier[1] * 10;
    const int standing = std::max(m_slot.m_multiplier[0], m_slot.m_multiplier[2]) * 10;
    if (sitting > 0 && standing == 0) SIMMY_STUB("XRoute::ConstructGoals: sitting goals (seats next to the spots)", 0x80158318);
    unsigned int walls = 0;
    if (!(m_slot.m_flags & RoutingSlot::kIgnoreRooms)) {
      CTilePt at = object->GetCTilePt();
      walls = WallBlockFlagsAtTile(at, objDir);
    }
    unsigned int nearWall = 0;
    for (int b = 0; b < 8; ++b)
      if ((walls >> b) & 1) nearWall |= (1u << b) | (1u << ((b + 1) & 7)) | (1u << ((b - 1) & 7));
    const int res = sitting != 0 ? 0x10 : std::max(1, m_slot.m_resolution);
    const float step = 16.0f / static_cast<float>(res);
    for (int c = 0; c < 8; ++c) {
      if (!((m_slot.m_flags >> c) & 1)) continue;
      if ((walls >> c) & 1) {
        anyWall = true;
        continue;
      }
      const int dir = (objDir + c) % 8;
      int far = static_cast<int>(step * static_cast<float>(-maxD) * 0.0625f);
      int near = static_cast<int>(step * static_cast<float>(-minD) * 0.0625f);
      if (sitting > 0) {
        --far;
        near = std::min(near + 1, -1);
      }
      const int facing = GoalFacing(m_slot, objDir, dir);
      bool stop = false;
      for (int row = near; far <= row && !stop; --row) {
        for (int col = row; col <= -row; ++col) {
          const float lx = static_cast<float>(col * res) + m_slot.m_x;
          const float ly = static_cast<float>(row * res) + m_slot.m_y;
          if (standing == 0) continue;
          const int d2 = static_cast<int>(lx * lx + ly * ly);
          if (static_cast<float>(d2) > static_cast<float>(maxD * maxD) || static_cast<float>(d2) < static_cast<float>(minD * minD))
            continue;
          RouteGoal g;
          TransformToWorldCoords(lx, ly, origin, dir, g.m_pos);
          const int off = std::abs(static_cast<int>(std::sqrt(static_cast<float>(d2))) - opt);
          g.m_score = 100 - static_cast<int>(static_cast<float>(off) * m_slot.m_gradient) + standing;
          if (m_slot.m_flags & RoutingSlot::kRandomScoring) g.m_score += simmy::sim_rand() % 20 - 10;
          if (!((nearWall >> c) & 1)) g.m_score += 25;
          if (g.m_pos[0] == m_start[0] && g.m_pos[1] == m_start[1]) g.m_score += 20;
          const int r = EvalTileForGoal(g.m_pos, dir);
          note(r);
          if (r == 3) stop = true;
          if (r >= 6 || r < 0) {
            g.m_dirMask = static_cast<unsigned short>(1u << c);
            g.m_facing = facing;
            AddGoal(g);
          }
        }
      }
    }
    if (static_cast<int>(m_goals.size()) > m_maxGoals) {
      // GC: the best, then random others, up to the limit.
      std::sort(m_goals.begin(), m_goals.end(), [](const RouteGoal& a, const RouteGoal& b) { return a.m_score > b.m_score; });
      m_goals.resize(static_cast<size_t>(m_maxGoals));
    }
    if (!m_goals.empty()) return;
  } else {
    RouteGoal g;
    TransformToWorldCoords(m_slot.m_x, m_slot.m_y, origin, objDir, g.m_pos);
    const int dir = DirectionBetween(origin, g.m_pos);
    const int r = EvalTileForGoal(g.m_pos, dir);
    note(r);
    if (r == 6) {
      g.m_score = 100;
      g.m_object = 0;
      g.m_facing = GoalFacing(m_slot, objDir, 0);
      AddGoal(g);
      return;
    }
  }
  (void)anyBlocked;
  (void)anyOut;
  m_failure = anyObject ? 8 : anyPerson ? 0xd : anyWall ? 0xb : anyRoom ? 0xc : 0;
}

// 80155AC8
// PC stand-in for the GC's FindPath (BuildRoomPartition and the SpacePartition A*): an A* from the
// route's start over the lot in quarter tiles (8 neighbours; a cell is open when Blocker lets a person
// stand at its centre, other people aside: they only add a cost), to the best-scoring goal it reaches (goals tried best first), then smoothed:
// each way point is the farthest one still in a clear straight line. The start may touch an object
// (the first steps away from it are not checked against objects).
int XRoute::FindPath(TileList& out) {
  out.m_points.clear();
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w || m_goals.empty()) return 0;
  std::vector<int> order(m_goals.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return m_goals[a].m_score > m_goals[b].m_score; });
  const std::vector<cXObjectImpl*> ignore = RouteIgnores(m_object);
  Blocker blocked{&ignore};
  blocked.AddPeople(m_person, m_object);
  // People are penalty rects on GC (the route may pass them at a cost), not walls: the cells only test
  // the lot, walls and objects; a cell a person stands on costs more.
  Blocker fixedOnly{&ignore};
  const float kPersonPenalty = 1.0f;
  const int cw = w->m_xSize * 4, ch = w->m_ySize * 4;
  if (cw <= 0 || ch <= 0) return 0;
  const float sx = m_start[1] / 16.0f, sy = m_start[0] / 16.0f;
  std::vector<signed char> open(static_cast<size_t>(cw * ch), -1);
  auto cellOpen = [&](int i, int j) {
    if (i < 0 || j < 0 || i >= cw || j >= ch) return false;
    signed char& o = open[static_cast<size_t>(j * cw + i)];
    if (o < 0) o = fixedOnly((i + 0.5f) * 0.25f, (j + 0.5f) * 0.25f) ? 0 : 1;
    return o == 1;
  };
  const int si = std::max(0, std::min(cw - 1, static_cast<int>(sx * 4.0f)));
  const int sj = std::max(0, std::min(ch - 1, static_cast<int>(sy * 4.0f)));
  for (int gi : order) {
    const RouteGoal& goal = m_goals[static_cast<size_t>(gi)];
    const float gx = goal.m_pos[1] / 16.0f, gy = goal.m_pos[0] / 16.0f;
    const int ti = static_cast<int>(gx * 4.0f), tj = static_cast<int>(gy * 4.0f);
    std::vector<float> cost(static_cast<size_t>(cw * ch), 1e30f);
    std::vector<int> from(static_cast<size_t>(cw * ch), -1);
    // PC: a person left standing inside objects (a goal the router allowed in a table, a seat's tile)
    // walks out: from a blocked start every cell is passable until the path reaches an open one
    // (GC: the room partition routes from the person's own rect, which may overlap objects).
    std::vector<char> escaping(static_cast<size_t>(cw * ch), 0);
    using Node = std::pair<float, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> queue;
    const int s = sj * cw + si;
    cost[static_cast<size_t>(s)] = 0.0f;
    escaping[static_cast<size_t>(s)] = !cellOpen(si, sj);
    queue.push({std::hypot(gx - sx, gy - sy), s});
    int found = -1, expanded = 0;
    while (!queue.empty() && expanded < 60000) {
      const int n = queue.top().second;
      queue.pop();
      ++expanded;
      const int i = n % cw, j = n / cw;
      if (i == ti && j == tj) {
        found = n;
        break;
      }
      for (int dj = -1; dj <= 1; ++dj) {
        for (int di = -1; di <= 1; ++di) {
          if (!di && !dj) continue;
          const int ni = i + di, nj = j + dj;
          const bool target = ni == ti && nj == tj;
          // PC: the start's ring, and its whole tile (a sim left standing in a chair's or a counter's
          // tile walks out of it; the GC router starts from the person's own rect, not the cells).
          const bool nearStart = (std::abs(ni - si) <= 1 && std::abs(nj - sj) <= 1) || (ni / 4 == si / 4 && nj / 4 == sj / 4);
          if (ni < 0 || nj < 0 || ni >= cw || nj >= ch) continue;
          const bool escape = escaping[static_cast<size_t>(n)] != 0;
          const bool openCell = cellOpen(ni, nj);
          if (!target && !nearStart && !escape && !openCell) continue;
          if (di && dj && !nearStart && !escape && (!cellOpen(i + di, j) || !cellOpen(i, j + dj))) continue;
          const float step = (di && dj) ? 1.41421f : 1.0f;
          const int m = nj * cw + ni;
          float c = cost[static_cast<size_t>(n)] + step * 0.25f;
          if (escape && !openCell) c += 0.5f;  // out of the objects the shortest way
          if (!blocked.m_people.empty() && blocked.Person((ni + 0.5f) * 0.25f, (nj + 0.5f) * 0.25f)) c += kPersonPenalty;
          if (c < cost[static_cast<size_t>(m)]) {
            cost[static_cast<size_t>(m)] = c;
            from[static_cast<size_t>(m)] = n;
            escaping[static_cast<size_t>(m)] = escape && !openCell ? 1 : 0;
            queue.push({c + std::hypot(gx - (ni + 0.5f) * 0.25f, gy - (nj + 0.5f) * 0.25f), m});
          }
        }
      }
    }
    if (found < 0) continue;
    // Cell centres back to the start, then the exact start and goal.
    std::vector<EVec2> cells;
    for (int n = found; n >= 0 && n != s; n = from[static_cast<size_t>(n)])
      cells.push_back(EVec2{((n % cw) + 0.5f) * 0.25f, ((n / cw) + 0.5f) * 0.25f});
    std::reverse(cells.begin(), cells.end());
    if (!cells.empty()) cells.back() = EVec2{gx, gy};
    else cells.push_back(EVec2{gx, gy});
    std::vector<EVec2> path{EVec2{sx, sy}};
    size_t k = 0;
    while (k < cells.size()) {
      size_t best = k;
      const EVec2& p = path.back();
      for (size_t q = cells.size(); q-- > k + 1;) {
        if (blocked.Clear(p.x, p.y, cells[q].x, cells[q].y)) {
          best = q;
          break;
        }
      }
      path.push_back(cells[best]);
      k = best + 1;
    }
    for (const EVec2& p : path) out.m_points.push_back(EVec2{p.x * 16.0f, p.y * 16.0f});
    m_goal = gi;
    return 1;
  }
  m_failure = 0x10;
  if (std::getenv("SIMMY_ROUTE_TRACE") || std::getenv("SIMMY_SIM_TRACE")) {  // PC: why (the start's ring of cells)
    int openNear = 0;
    for (int dj = -2; dj <= 2; ++dj)
      for (int di = -2; di <= 2; ++di) openNear += cellOpen(si + di, sj + dj) ? 1 : 0;
    std::printf("[route]   no path: lot %dx%d cells, start cell %d,%d (%s), %d of 25 cells round it open\n", cw, ch, si, sj,
                cellOpen(si, sj) ? "open" : "blocked", openNear);
    static bool mapped = false;  // once: the lot's tiles ('.' a person fits at the centre, 'f' tile flag 0x10, 'x' not)
    if (!mapped) {
      mapped = true;
      for (int ty = 0; ty < ch / 4; ++ty) {
        std::printf("[route]   map %2d ", ty);
        for (int tx = 0; tx < cw / 4; ++tx) {
          CTilePt t(tx, ty, 0);
          const char c = (w->GetFlags(t) & 0x10) ? 'f' : cellOpen(tx * 4 + 2, ty * 4 + 2) ? '.' : 'x';
          std::printf("%c", (tx == si / 4 && ty == sj / 4) ? 'S' : c);
        }
        std::printf("\n");
      }
      // What stands round the start.
      for (int ty = sj / 4 - 1; ty <= sj / 4 + 1; ++ty)
        for (int tx = si / 4 - 1; tx <= si / 4 + 1; ++tx) {
          CTilePt t(tx, ty, 0);
          if (w->OutOfBounds(t)) continue;
          ObjectModuleImpl* module = Globs_pObjectModule;
          cXObjectImpl* o = module ? Impl(module->GetObjectFromID(module->GetTileObjectID(t))) : nullptr;
          TileWalls walls = w->GetWall(t);
          std::printf("[route]   tile %d,%d: flags %x walls %d object %s\n", tx, ty, w->GetFlags(t), walls.HasWall() ? 1 : 0,
                      o ? o->DebugName() : "-");
        }
    }
  }
  return 0;
}
