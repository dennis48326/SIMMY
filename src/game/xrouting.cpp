// Port of game/xrouting.cpp (XRoute: goals around a routing slot, the path to one) and the routing part
// of game/slots.cpp (RoutingSlot). See routing.hpp. Positions: FTilePt pairs in 1/16 tiles ([0] the
// object location's +0x68, [1] +0x6C; the tile is CTilePt([1] >> 4, [0] >> 4)); "tile axes" X = [1] / 16,
// Y = [0] / 16. Directions are eighths of a turn: 0 toward -[0], 2 toward +[1], 4 +[0], 6 -[1].
#include "simmy/types_gen.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/countereat.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/detmath.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

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

namespace {
// PC: the objects whose slot vector a route rewrote (GC: each object owns its slots, +0xA0), with the
// id and definition they were made for (a later object at the same address is not them).
struct SlotOverride {
  short id = 0;
  const ObjDefinition* def = nullptr;
  std::vector<RoutingSlot> slots;
};
std::map<const cXObjectImpl*, SlotOverride>& SlotOverrides() {
  static std::map<const cXObjectImpl*, SlotOverride> m;
  return m;
}
}  // namespace

void SetObjectRoutingSlot(cXObjectImpl* object, int index, const RoutingSlot& slot) {
  if (!object || index < 0) return;
  SlotOverride& o = SlotOverrides()[object];
  if (o.id != object->m_id || o.def != object->m_def || o.slots.empty()) {
    o.id = object->m_id;
    o.def = object->m_def;
    o.slots.clear();
    SlotOverrides().erase(object);
    std::vector<RoutingSlot> base = ObjectRoutingSlots(object);
    SlotOverride& fresh = SlotOverrides()[object];
    fresh.id = object->m_id;
    fresh.def = object->m_def;
    fresh.slots = base;
    if (fresh.slots.size() <= static_cast<size_t>(index)) fresh.slots.resize(static_cast<size_t>(index) + 1);
    fresh.slots[static_cast<size_t>(index)] = slot;
    return;
  }
  if (o.slots.size() <= static_cast<size_t>(index)) o.slots.resize(static_cast<size_t>(index) + 1);
  o.slots[static_cast<size_t>(index)] = slot;
}

const std::vector<RoutingSlot>& ObjectRoutingSlots(cXObjectImpl* object) {
  static std::map<const ObjDefinition*, std::vector<RoutingSlot>> cache;
  static const std::vector<RoutingSlot> none;
  if (!object || !object->m_def) return none;
  auto ov = SlotOverrides().find(object);
  if (ov != SlotOverrides().end()) {
    if (ov->second.id == object->m_id && ov->second.def == object->m_def) return ov->second.slots;
    SlotOverrides().erase(ov);
  }
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
namespace {
// PC mod: mods. A mod surface's object slots: copies of its base's descriptors with the height of its
// own top (altitude units: tiles x 2 sqrt 6 / 3) and a mark in the descriptor's padding (+2, the disc's
// 0xFAFA) so ObjectSlotHeight keeps that altitude for the slot's height kind (3 / 4 / 8 would give the
// standard 1.633). Kept for the session (a few per surface; the map's nodes do not move).
constexpr unsigned char kModSlotMark[2] = {'M', 'S'};
const unsigned char* ModSlotDescriptor(const unsigned char* base, float surface) {
  static std::mutex lock;
  static std::map<std::pair<const unsigned char*, float>, std::array<unsigned char, 0x48>> copies;
  std::lock_guard<std::mutex> guard(lock);
  auto it = copies.find({base, surface});
  if (it == copies.end()) {
    std::array<unsigned char, 0x48> d;
    std::memcpy(d.data(), base, d.size());
    d[2] = kModSlotMark[0];
    d[3] = kModSlotMark[1];
    const float alt = surface * 2.0f * std::sqrt(6.0f) / 3.0f;  // WorldToAlt
    unsigned int bits = 0;
    std::memcpy(&bits, &alt, 4);
    d[0xc] = static_cast<unsigned char>(bits >> 24), d[0xd] = static_cast<unsigned char>(bits >> 16);
    d[0xe] = static_cast<unsigned char>(bits >> 8), d[0xf] = static_cast<unsigned char>(bits);
    it = copies.emplace(std::make_pair(base, surface), d).first;
  }
  return it->second.data();
}
}  // namespace

void ObjectSlotHeight(const unsigned char* d, int& height, float& alt) {
  height = ERQuickdata::S32(d, 0x18);
  alt = ERQuickdata::F32(d, 0xc);
  if (d[2] == kModSlotMark[0] && d[3] == kModSlotMark[1] && height != 0 && height != 1) return;  // PC mod: its own top
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
  // PC mod: mods. A counter's eating slots (bar stools: countereat.cpp), past its own slot list.
  if (index >= simmy::countereat::kCounterEatSlot) return simmy::countereat::SlotDescriptor(object, index);
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
      if (k++ == index) {
        // PC mod: a mod surface's slots at its own top (not the floor's or a low table's kind)
        const float top = object->m_selector ? object->m_selector->m_modSurface : -1.0f;
        const int kind = ERQuickdata::S32(d, 0x18);
        if (top > 0.0f && kind != 1 && kind != 0) return ModSlotDescriptor(d, top);
        return d;
      }
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
cXPersonImpl* PersonOf(cXObject* o) { return o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr; }

// The eighth of a turn of the move by (d0, d1) (FTilePt [0], [1] deltas): the GC's inline direction
// code (XRoute::ConstructGoals, cXPersonImpl::InitRoute, MoveOutOfWay).
int DirectionOfDelta(int d0, int d1) {
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
}  // namespace

// The tile step of direction `dir` (FTilePt [0], [1] in 1/16 tiles): the GC's inline switch.
void DirectionStep(int dir, int out[2]) {
  static const int step[8][2] = {{-16, 0}, {-16, 16}, {0, 16}, {16, 16}, {16, 0}, {16, -16}, {0, -16}, {-16, -16}};
  out[0] = step[dir & 7][0];
  out[1] = step[dir & 7][1];
}

int DirectionBetween(const int from[2], const int to[2]) { return DirectionOfDelta(to[0] - from[0], to[1] - from[1]); }

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
      const float c = simmy::dm::cos(a), s = simmy::dm::sin(a);
      p1 = std::round(x * c - y * s);
      p2 = std::round(x * s + y * c);
      break;
    }
  }
  out[1] = static_cast<int>(p1 + static_cast<float>(origin[1]));
  out[0] = static_cast<int>(p2 + static_cast<float>(origin[0]));
}

// 800E2848 cXObjectImpl::GetWallBlockFlags
// Data 0x35 (the sides it blocks like a wall), turned by its direction.
unsigned int ObjWallBlockFlags(cXObjectImpl* o) {
  unsigned int f = static_cast<unsigned int>(static_cast<unsigned short>(o->GetData(0x35)));
  if (f != 0) {
    const int d = o->GetData(1);
    f = ((f << d) | (static_cast<unsigned int>(static_cast<int>(f << d) >> 8))) & 0xff;
  }
  return f;
}

// 800E28AC cXObject::GetWallBlockFlagsAtTile
// The walls round `tile` as direction bits (segment 1: 0x40, 2: 1, 4: 4, 8: 0x10) and the blocking
// sides of the objects there with data 0x35, turned so bit 0 is direction `dir`. PC: the objects whose
// site is the tile (ObjectsAtTile; GC: the tile's ObjectIterator, its chain and slots).
unsigned int WallBlockFlagsAtTile(CTilePt& tile, int dir) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w || w->OutOfBounds(tile)) return 0;
  TileWalls walls = w->GetWall(tile);
  unsigned int f = 0;
  if (walls.HasWall(TileWallsSegment(1))) f = 0x40;
  if (walls.HasWall(TileWallsSegment(2))) f |= 1;
  if (walls.HasWall(TileWallsSegment(4))) f |= 4;
  if (walls.HasWall(TileWallsSegment(8))) f |= 0x10;
  for (cXObjectImpl* o : TileObjects(tile))
    if (o->GetData(0x35) != 0) f |= ObjWallBlockFlags(o);
  if (dir != 0) {
    f <<= (8 - dir);
    f = (f | static_cast<unsigned int>(static_cast<int>(f) >> 8)) & 0xff;
  }
  return f;
}

// cXObjectImpl::GetRect (800EB0BC: +0x70, kept by Place): the footprint (ComputeRect) at its location.
// [0] the largest FTilePt[0], [1] the largest [1], [2] the smallest [0], [3] the smallest [1].
void ObjRect(cXObjectImpl* o, int r[4]) { ObjComputeRect(o, o->m_location, r); }

// PC stand-in for cXObjectImpl::CanPlace (vtable 0x134) of a person at `pos` (FTilePt), used by
// TrySnap: on the lot, no wall within reach, no object's footprint but `near`'s group and `person`
// itself, no other person. (The router uses the GC CanPlace: EvalTileForGoal.)
namespace {
float SegmentDistance(float px, float py, float ax, float ay, float bx, float by) {
  const float dx = bx - ax, dy = by - ay;
  const float len = dx * dx + dy * dy;
  float t = len > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / len : 0.0f;
  t = std::max(0.0f, std::min(1.0f, t));
  const float cx = ax + dx * t - px, cy = ay + dy * t - py;
  return std::sqrt(cx * cx + cy * cy);
}
void Footprint(cXObjectImpl* o, float r[4]) {
  int rect[4];
  ObjRect(o, rect);
  r[0] = rect[3] / 16.0f, r[1] = rect[2] / 16.0f, r[2] = rect[1] / 16.0f, r[3] = rect[0] / 16.0f;
}
std::vector<cXObjectImpl*> RouteIgnores(cXObject* object) {
  std::vector<cXObjectImpl*> out;
  cXObjectImpl* impl = Impl(object);
  if (!impl) return out;
  out.push_back(impl);
  if (PlacementGroup* g = PlacementGroupOf(impl))
    for (auto& part : g->parts)
      if (part.object != impl) out.push_back(part.object);
  return out;
}
}  // namespace

bool PersonCanStandAt(cXObject* person, cXObject* near, const int pos[2]) {
  std::vector<cXObjectImpl*> ignore = RouteIgnores(near);
  cXObjectImpl* self = Impl(person);
  if (self) ignore.push_back(self);
  const float X = pos[1] / 16.0f, Y = pos[0] / 16.0f, radius = 0.24f;
  cFixedWorldImpl* w = Globs_pFixedWorld;
  ObjectModuleImpl* module = Globs_pObjectModule;
  CTilePt here(static_cast<int>(std::floor(X)), static_cast<int>(std::floor(Y)), 0);
  if (!w || !module || X < 0.0f || Y < 0.0f || w->OutOfBounds(here) || (w->GetFlags(here) & 0x10)) return false;
  for (cXObjectImpl* o = module->m_first; o; o = o->m_nextObject) {
    if (o == self || o->GetType() != 2 || !self || o->m_level != self->m_level || o->GetContainer()) continue;
    const float px = o->m_location[1] / 16.0f, py = o->m_location[0] / 16.0f, d = radius + 6.0f / 16.0f;
    if (self->CalcShortDistance(o) <= 3.0 && (px - X) * (px - X) + (py - Y) * (py - Y) < d * d) return false;
  }
  for (int tx = here.GetX() - 1; tx <= here.GetX() + 1; ++tx) {
    for (int ty = here.GetY() - 1; ty <= here.GetY() + 1; ++ty) {
      CTilePt t(tx, ty, 0);
      if (w->OutOfBounds(t)) continue;
      TileWalls walls = w->GetWall(t);
      if (walls.HasWall()) {
        const float x0 = static_cast<float>(tx), y0 = static_cast<float>(ty), x1 = x0 + 1.0f, y1 = y0 + 1.0f;
        auto edge = [&](int seg, float ax, float ay, float bx, float by) {
          if (!walls.HasWall(TileWallsSegment(seg))) return false;
          const int style = walls.GetStyle(TileWallsSegment(seg));
          if (style == 3 || style == 5 || style == 6 || style == 0xf || style == 0x17) return false;
          return SegmentDistance(X, Y, ax, ay, bx, by) < radius;
        };
        if (edge(1, x0, y0, x0, y1) || edge(4, x1, y0, x1, y1) || edge(2, x0, y0, x1, y0) || edge(8, x0, y1, x1, y1))
          return false;
        if (walls.HasWall(TileWallsSegment(TileWallsSegment::kDiagonal1)) && SegmentDistance(X, Y, x1, y0, x0, y1) < radius)
          return false;
        if (walls.HasWall(TileWallsSegment(TileWallsSegment::kDiagonal2)) && SegmentDistance(X, Y, x0, y0, x1, y1) < radius)
          return false;
      }
      CTilePt tl(tx, ty, 1);
      for (cXObjectImpl* o : TileObjects(tl)) {
        if (!o->m_def || std::find(ignore.begin(), ignore.end(), o) != ignore.end()) continue;
        const int type = objdef::Type(o->m_def);
        if (((o->GetData(8) >> 4) & 1) || type == 2 || type == 8) continue;
        float f[4];
        Footprint(o, f);
        const float cx = std::max(f[0], std::min(X, f[2])), cy = std::max(f[1], std::min(Y, f[3]));
        if ((cx - X) * (cx - X) + (cy - Y) * (cy - Y) < radius * radius) return false;
      }
    }
  }
  return true;
}

void RoutingSlotPosition(cXObjectImpl* object, const RoutingSlot& slot, int out[2]) {
  const int origin[2] = {object->m_location[0], object->m_location[1]};
  TransformToWorldCoords(slot.m_x, slot.m_y, origin, object->GetData(1) & 7, out);
}

// ---------------------------------------------------------------------------------------------------
// The room partition

namespace {
// BuildRoomPartition's inline test: the tile (level 1) belongs to `room` for routing: not a flag-0x10
// tile, and in the room (or the room id 0xFFFB), or holding a portal (a type-8 object with a portal
// tree, entry 0xF) whose other side is in the room. PC: the objects whose site is the tile.
bool TileInRoom(int x, int y, unsigned short room) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  CTilePt t(x, y, 1);
  if (w->GetFlags(t) & 0x10) return false;
  const unsigned short r = w->GetRoom(t);
  if (r == room || r == 0xfffb) return true;
  bool in = false;
  for (cXObjectImpl* o : TileObjects(t)) {
    if (o->GetType() != 8 || o->GetTreeID(0xf) == 0 || !IsPortalObject(o)) continue;
    cXObjectImpl* other = PortalOtherSide(o);
    if (other && static_cast<unsigned short>(other->GetData(0x1d)) == room) in = true;
  }
  return in;
}
// A wall segment doors and windows let people through (styles 3, 5, 6, 0xF, 0x17).
bool PassableWallStyle(int style) { return style == 3 || style == 5 || style == 6 || style == 0xf || style == 0x17; }
}  // namespace

// 80153500
// The room's static obstacles for the router, in 1/16 tiles: blocking strips round its bounding box
// (grown by a tile, kept inside the lot's border), blocking rectangles over the box's tiles that are not
// the room (runs of them, each grown down while the rows stay clear of the room), the room's inner
// walls (but doors and windows) as zero-width lines on each tile's segments 8 and 4 between two room
// tiles, and a diagonal wall's tile as three small squares along it; then every rectangle is grown by
// half a person's width. `third` is not read.
void BuildRoomPartition(unsigned short room, std::vector<PenaltyRect>* rects, bool third) {
  (void)third;
  cFixedWorldImpl* w = Globs_pFixedWorld;
  const int half = static_cast<int>(cXObject::GetPersonWidth()) / 2;
  int minY = 0, minX = 0, maxX = 0, maxY = 0;
  RoomManager::GetRoomManager()->GetRoom(room);
  const int xs = w->GetXSize(), ys = w->GetYSize();
  int x = 1, y = 1;
  bool done = false;
  do {
    CTilePt t(x, y, 1);
    if (w->GetRoom(t) == room) {
      done = true;
      minY = y, minX = x, maxX = x, maxY = y;
    }
    if (xs - 1 <= ++x) {
      ++y;
      x = 1;
      if (ys - 1 <= y) done = true;
    }
  } while (!done);
  done = false;
  do {
    CTilePt t(x, y, 1);
    if (w->GetRoom(t) == room) {
      minX = std::min(minX, x);
      maxX = std::max(maxX, x);
      minY = std::min(minY, y);
      maxY = std::max(maxY, y);
    }
    if (xs - 1 <= ++x) {
      ++y;
      x = 1;
      if (ys - 1 <= y) done = true;
    }
  } while (!done);
  if (1 < minX) --minX;
  if (maxX < xs - 2) ++maxX;
  if (1 < minY) --minY;
  if (maxY < ys - 2) ++maxY;
  // The box's sides.
  PenaltyRect side(minX - 1, minY - 1, minX, maxY + 2, PenaltyRect::kBlocked);
  side.left <<= 4, side.top <<= 4, side.right <<= 4, side.bottom <<= 4;
  rects->push_back(side);
  const int dx = (maxX - minX) * 16 + 0x20;
  side.left += dx, side.right += dx;
  rects->push_back(side);
  PenaltyRect top(minX - 1, minY - 1, maxX + 2, minY, PenaltyRect::kBlocked);
  top.left <<= 4, top.top <<= 4, top.right <<= 4, top.bottom <<= 4;
  rects->push_back(top);
  const int dy = (maxY - minY) * 16 + 0x20;
  top.top += dy, top.bottom += dy;
  rects->push_back(top);
  auto push16 = [&](int l, int t, int r, int b) {
    PenaltyRect p(l, t, r, b, PenaltyRect::kBlocked);
    p.left <<= 4, p.top <<= 4, p.right <<= 4, p.bottom <<= 4;
    rects->push_back(p);
  };
  x = minX, y = minY;
  for (;;) {
    if (TileInRoom(x, y, room)) {
      CTilePt t(x, y, 1);
      TileWalls walls = w->GetWall(t);
      if (walls.HasWall(TileWallsSegment(8)) && !PassableWallStyle(walls.GetStyle(TileWallsSegment(8))) &&
          TileInRoom(x, y + 1, room))
        push16(x, y + 1, x + 1, y + 1);
      if (walls.HasWall(TileWallsSegment(4)) && !PassableWallStyle(walls.GetStyle(TileWallsSegment(4))) &&
          TileInRoom(x + 1, y, room))
        push16(x + 1, y, x + 1, y + 1);
      if (walls.HasDiagonal()) {
        PenaltyRect a(x, y, x + 1, y + 1, PenaltyRect::kBlocked);
        a.left <<= 4, a.top <<= 4, a.right <<= 4, a.bottom <<= 4;
        PenaltyRect b = a, c = a;
        if (!walls.HasWall(TileWallsSegment(0x10))) {
          a.bottom -= 0xe;
          b.top += 0xe;
        } else {
          a.top += 0xe;
          b.bottom -= 0xe;
        }
        b.left += 0xe;
        a.right -= 0xe;
        c.left += 7, c.right -= 7, c.top += 7, c.bottom -= 7;
        rects->push_back(a);
        rects->push_back(b);
        rects->push_back(c);
      }
    } else {
      PenaltyRect cell(x, y, x + 1, y + 1, PenaltyRect::kBlocked);
      cell.left <<= 4, cell.top <<= 4, cell.right <<= 4, cell.bottom <<= 4;
      const int hit = FindIntersectingRect(&cell, rects);
      if (hit == -1) {
        // The run of tiles out of the room along the row, then grown down while those rows stay out.
        int end = x + 1;
        while (end <= maxX && !TileInRoom(end, y, room)) ++end;
        int row = y + 1;
        for (;;) {
          bool clear = true;
          for (int i = x; i < end; ++i)
            if (TileInRoom(i, row, room)) {
              clear = false;
              break;
            }
          if (!clear || maxY < row) break;
          ++row;
        }
        cell.bottom = row << 4;
        cell.right = end << 4;
        rects->push_back(cell);
      } else {
        int r = (*rects)[static_cast<size_t>(hit)].right;
        if (r < 0) r += 0xf;
        if (x != (r >> 4)) x = (r >> 4) - 1;
      }
    }
    ++x;
    if (maxX < x) {
      ++y;
      x = minX;
      if (maxY < y) break;
    }
  }
  for (PenaltyRect& r : *rects) {
    r.left -= half;
    r.right += half;
    r.top -= half;
    r.bottom += half;
  }
}

// 801558D0
void BuildRoomPartition(unsigned short room, std::vector<PenaltyRect>* rects) { BuildRoomPartition(room, rects, true); }

// 801558F4
// The router is stuck at `p`: a blocking rectangle of `rects` covers it (GC: only the debug check
// verifyRoute, off in the release build, calls it).
int Stuck(std::vector<PenaltyRect>* rects, IPoint* p) {
  IRect r;
  r.top = p->y, r.left = p->x, r.bottom = p->y + 1, r.right = p->x + 1;
  if (!rects) return 0;
  const int i = FindIntersectingRect(&r, rects);
  return i != -1 && (*rects)[static_cast<size_t>(i)].penalty == PenaltyRect::kBlocked ? 1 : 0;
}

// 80159738
// Merges rectangles (but the 10000-penalty ones) that line up, twice along the rows (same top and
// bottom, overlapping or touching on the left-right axis) and twice along the columns; a merged one is
// replaced by the last of the list.
void CollapsePenaltyRects(std::vector<PenaltyRect>& rects) {
  int count = static_cast<int>(rects.size());
  int last = count - 1;
  for (int pass = 0; pass < 2; ++pass) {
    last = count - 1;
    for (int i = 0; i < last;) {
      const int a = i++;
      if (rects[static_cast<size_t>(a)].penalty == 10000 || count <= i) {
        last = count - 1;
        continue;
      }
      int j = i;
      do {
        PenaltyRect& ra = rects[static_cast<size_t>(a)];
        PenaltyRect& rb = rects[static_cast<size_t>(j)];
        bool merged = false;
        if (rb.penalty != 10000 && ra.top == rb.top && ra.bottom == rb.bottom) {
          if (ra.right < rb.left || rb.right < ra.right) {
            if (!(ra.left < rb.left || rb.right < ra.left)) {
              ra.left = rb.left;
              merged = true;
            }
          } else {
            ra.right = rb.right;
            merged = true;
          }
        }
        if (merged) {
          --count;
          rects[static_cast<size_t>(j)] = rects[static_cast<size_t>(count)];
          rects.pop_back();
        } else {
          ++j;
        }
        last = count - 1;
      } while (j < count);
    }
  }
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < last;) {
      const int a = i++;
      if (rects[static_cast<size_t>(a)].penalty == 10000 || count <= i) {
        last = count - 1;
        continue;
      }
      int j = i;
      do {
        PenaltyRect& ra = rects[static_cast<size_t>(a)];
        PenaltyRect& rb = rects[static_cast<size_t>(j)];
        bool merged = false;
        if (rb.penalty != 10000 && ra.left == rb.left && ra.right == rb.right) {
          if (ra.bottom < rb.top || rb.bottom < ra.bottom) {
            if (!(ra.top < rb.top || rb.bottom < ra.top)) {
              ra.top = rb.top;
              merged = true;
            }
          } else {
            ra.bottom = rb.bottom;
            merged = true;
          }
        }
        if (merged) {
          --count;
          rects[static_cast<size_t>(j)] = rects[static_cast<size_t>(count)];
          rects.pop_back();
        } else {
          ++j;
        }
        last = count - 1;
      } while (j < count);
    }
  }
}

// ---------------------------------------------------------------------------------------------------
// XRoute

// 80152D18
XRoute::XRoute() { Construct(nullptr, nullptr, nullptr); }

// 80152D74
XRoute::XRoute(cXObject* person, cXObject* object, RoutingSlot* slot) { Construct(person, object, slot); }

// 80152DDC
// The router and its target, a copy of the slot, where the router stands; a person's inset id (person
// data 0x48) is kept for the goal search and it stops ignoring the object it routed through.
void XRoute::Construct(cXObject* person, cXObject* object, RoutingSlot* slot) {
  m_person = person;
  m_object = object;
  m_asked = nullptr;
  m_avoid = nullptr;
  if (slot) m_slot = *slot;
  m_maxScore = -1;
  m_goal = -1;
  m_stuck = 0;
  m_waitStart = 0;
  if (cXObjectImpl* p = Impl(person)) m_personPos[0] = p->m_location[0], m_personPos[1] = p->m_location[1];
  m_planned = 1;
  m_insetId = 0;
  m_portal = nullptr;
  m_askedDone = 0;
  m_failure = 0;
  m_blocker = 0;
  m_ignorePeople = 0;
  if (cXPersonImpl* p = PersonOf(person)) {
    m_insetId = static_cast<short>(p->GetPersonData(0x48));
    p->ClearIgnoringObject();
  }
  m_maxGoals = 0x30;
}

// 80152F5C
int XRoute::HasCurrentGoal() { return m_goal != -1 ? 1 : 0; }

// 80152F98
int XRoute::GetMaxScore() {
  if (m_maxScore == -1) {
    m_maxScore = 0;
    for (const RouteGoal& g : m_goals)
      if (m_maxScore < g.m_score) m_maxScore = g.m_score;
  }
  return m_maxScore;
}

// 80152FE8
void XRoute::SetCurrentGoal(int goal) { m_goal = goal; }

// 80152FF0
// The goal tried (and failed) is worth nothing now.
void XRoute::ClearCurrentGoal() {
  if (m_goal == -1) return;
  if (m_goal >= 0 && static_cast<size_t>(m_goal) < m_goals.size()) m_goals[static_cast<size_t>(m_goal)].m_score = 0;
  m_maxScore = -1;
  m_goal = -1;
}

// 80153248
RouteGoal* XRoute::GetCurrentGoal() { return &m_goals[static_cast<size_t>(m_goal)]; }

// 80152F74 XRoute::CountGoals (inline in the header)
// 80152F88 XRoute::GetNthGoal (inline)
// 8015303C XRoute::GetRoutingSlot (inline)
// 801534C8 XRoute::DoStream (inline: nothing)

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

// 80153328
// Objects the router's room partition leaves out: those people may stand in (unless data 10), those
// in a container; people when the route ignores people, the route's own avoided one, those farther than
// three tiles from the router; other objects whose "allow intersection" tree (entry 5, the router as
// the stack object) says so.
int XRoute::ShouldIgnore(cXObject* object) {
  cXObjectImpl* o = Impl(object);
  if (!o) return 1;
  if ((ObjCanIntersectPeople(o) && o->GetData(10) == 0) || o->GetContainer()) return 1;
  cXObjectImpl* router = Impl(m_person);
  if (o->GetType() == 2) {
    if (m_ignorePeople != 0) return 1;
    if (dynamic_cast<cXPersonImpl*>(o)) {
      if (m_avoid && object == m_avoid) return 1;
      if (router && router->CalcShortDistance(object) > 3.0) return 1;
    }
  } else if (router && o->RunTree(5, router->m_id, nullptr) != 0) {
    return 1;
  }
  return 0;
}

// 8015282C
// One goal at `pos` facing `dir` (eighths; -1 none): 0 off the lot (or a placement error other than a
// wall or an object), 2 in another room than the router, 3 walled off from the slot's side (or a wall
// in the way), 1 an object in the way (its id in m_blocker), 5 a person in the way, 6 good. The room and
// wall checks are skipped with slot flag 0x800 (ignore rooms). Room to stand is the router's CanPlace.
int XRoute::EvalTileForGoal(const int pos[2], int dir) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  cXObjectImpl* person = Impl(m_person);
  if (!w || !person) return 0;
  CTilePt tile(pos[1] >> 4, pos[0] >> 4, person->m_level);
  if (pos[0] < 0 || pos[1] < 0 || w->OutOfBounds(tile)) return 0;
  if (!(m_slot.m_flags & RoutingSlot::kIgnoreRooms)) {
    if (w->GetRoom(tile) != static_cast<unsigned short>(person->GetData(0x1d))) return 2;
    if (dir != -1) {
      const unsigned int f = WallBlockFlagsAtTile(tile, (dir + 4) & 7);
      if ((f & 1) || ((dir & 1) && (f & 0x82))) return 3;
    }
  }
  gPlacementConflict = nullptr;
  if (!ObjCanPlace(person, pos, person->m_level, nullptr, 0)) {
    if (gPlacementError == 7) return 3;
    if (gPlacementError != 0xb) return 0;
    if (gPlacementConflict && (!m_avoid || gPlacementConflict != m_avoid)) {
      cXObjectImpl* c = Impl(gPlacementConflict);
      m_blocker = c ? c->m_id : 0;
      return c && c->GetType() == 2 ? 5 : 1;
    }
  }
  return 6;
}

// 80157FB8
// While goals are scored the router faces the way it would there (its footprint turns with it): no
// slot directions, toward the object's front (objDir + 4); else by the slot's facing (away: dir,
// toward: dir + 4, anywhere: as it is, else objDir + facing).
void SetDirectionForGoalSearch(cXObject* person, short objDir, short dir, RoutingSlot* slot) {
  int face;
  if ((slot->m_flags & 0xff) == 0) {
    face = static_cast<short>(objDir + 4);
  } else if (slot->m_facing == RoutingSlot::kFaceAway) {
    face = dir;
  } else if (slot->m_facing == RoutingSlot::kFaceTowards) {
    face = static_cast<short>(dir + 4);
  } else if (slot->m_facing == RoutingSlot::kFaceAnywhere) {
    return;
  } else {
    face = static_cast<short>(objDir + slot->m_facing);
  }
  if (cXObjectImpl* p = Impl(person)) p->SetData(1, static_cast<short>(face % 8));
}

// 80158058
// The router's own spot. A person on a seat starts from the free side of the seat (the chair
// definition's side mask +0x84, turned by the chair's facing) nearest the object, where she could stand
// (EvalTileForGoal 5 or 6): m_startDirs is that side's bit.
void XRoute::ChooseStartingPoint() {
  cXObjectImpl* p = Impl(m_person);
  m_startDirs = 0;
  if (!p) return;
  m_start[0] = p->m_location[0], m_start[1] = p->m_location[1];
  const int here[2] = {m_start[0], m_start[1]};
  cXObjectImpl* seat = Impl(p->GetContainer());
  cXObjectImpl* target = Impl(m_object);
  if (!seat || !seat->IsChair() || !seat->m_def || !target) return;
  const int sides = ERQuickdata::S16(seat->m_def, 0x84);
  bool found = false;
  int best = 0;
  for (int c = 0; c < 8; ++c) {
    if (!((sides >> c) & 1)) continue;
    int step[2];
    DirectionStep((c + seat->GetData(1)) & 7, step);
    const int pos[2] = {here[0] + step[0], here[1] + step[1]};
    const int r = EvalTileForGoal(pos, -1);  // GC: the direction register is not set here
    if (static_cast<unsigned int>(r - 5) >= 2) continue;
    const int d1 = target->m_location[1] - pos[1], d0 = target->m_location[0] - pos[0];
    const int d2 = d1 * d1 + d0 * d0;
    if (!found || d2 < best) {
      m_start[0] = pos[0], m_start[1] = pos[1];
      found = true;
      m_startDirs = 1 << c;
      best = d2;
    }
  }
}

// 8015325C
bool XRoute::IsPersonSittingOnChairGoal(cXObject* person) {
  cXObjectImpl* p = Impl(person);
  cXObjectImpl* object = Impl(m_object);
  if (!p || !object) return false;
  for (const RouteGoal& g : m_goals) {
    if (g.m_object == 0) continue;
    cXObjectImpl* chair = Impl(object->GetObjectFromID(g.m_object));
    if (chair && chair->IsChair() && Impl(p->GetContainer()) == chair) return true;
  }
  return false;
}

// 8015597C
// The start (ChooseStartingPoint), then the goals (ConstructGoals) with the person's inset id in person
// data 0x49 (its footprint for CanPlace) and its facing put back after. A room over 250 tiles allows
// only 8 goals.
void XRoute::BuildGoalList() {
  ChooseStartingPoint();
  cXPersonImpl* person = PersonOf(m_person);
  if (person) {
    Room* room = RoomManager::GetRoomManager()->GetRoom(static_cast<unsigned short>(person->GetData(0x1d)));
    if (room && room->GetArea() > 0xfa) m_maxGoals = 8;
  }
  cXObjectImpl* p = Impl(m_person);
  const short dir = p ? static_cast<short>(p->GetData(1)) : 0;
  if (person) person->SetPersonData(0x49, m_insetId);
  ConstructGoals();
  if (person) person->SetPersonData(0x49, 0);
  if (p) p->SetData(1, dir);
}

namespace {
// 800E2A68
// 800C7884
// cXObjectImpl / cXMTObjectImpl::GetAverageLocation: its location; a multi-tile object the mean of its
// parts' locations.
void AverageLocation(cXObjectImpl* o, int out[2]) {
  out[0] = o->m_location[0], out[1] = o->m_location[1];
  PlacementGroup* g = o->IsMultiTile() ? PlacementGroupOf(o) : nullptr;
  if (!g || g->parts.empty()) return;
  int s0 = 0, s1 = 0, n = 0;
  for (auto& part : g->parts) {
    s0 += part.object->m_location[0];
    s1 += part.object->m_location[1];
    ++n;
  }
  out[0] = s0 / n, out[1] = s1 / n;
}
}  // namespace

// 80158318
// With slot directions: for each allowed direction (turned by the object's facing unless absolute) not
// walled off at the object's tile, rows of spots from the minimum to the maximum distance in front (a
// row as wide as it is far, `resolution` apart; a slot with a sitting score looks a tile at a time, a
// row further each way). At each spot: standing there (score the standing multiplier x10), and for a
// sitting score each of the eight tiles round it outside the rows that holds a chair facing the object
// with that side free (its occupant none or the router; else the seat is taken and m_blocker names who
// sits there): sitting there; standing is tried first unless sitting scores more. A spot within the
// distances scores 100 - |distance - optimal| x gradient + that, +-10 at random with random scoring, +25
// where no wall is beside its direction, +20 at the router's start, and is kept if EvalTileForGoal finds
// it good (random scoring also skips spots on a door); a wall there stops the farther rows. More goals
// than allowed: those with a seat, then random others. Without directions: the one spot at the slot's
// offset (score 100). No goal: the failure says why (9 a seat taken, 0xD a person, 8 an object, 0xB a
// wall or another room, 0xC (EvalTileForGoal 4), 10 only sitting was allowed).
void XRoute::ConstructGoals() {
  ResetGoals();
  cXObjectImpl* object = Impl(m_object);
  if (!object) return;
  int objDir = object->GetData(1);
  if (m_slot.m_flags & RoutingSlot::kAbsolute) objDir = 0;
  int origin[2] = {object->m_location[0], object->m_location[1]};
  if (m_slot.m_flags & RoutingSlot::kAverageLocation) AverageLocation(object, origin);
  bool sittingOnly = false, roomFlag = false, seatTaken = false, outFlag = false, objectFlag = false, personFlag = false,
       wallFlag = false;
  auto note = [&](int r) {
    if (r == 3 || r == 2) wallFlag = true;
    else if (r == 1) objectFlag = true;
    else if (r == 0) outFlag = true;
    else if (r == 5) personFlag = true;
    else if (r == 4) roomFlag = true;
  };
  RouteGoal goal;
  if (m_slot.m_flags & 0xff) {
    const int minD = m_slot.m_minDist, maxD = m_slot.m_maxDist, opt = m_slot.m_optimalDist;
    if (minD > 0x200 || maxD < 1 || maxD > 0x200 || maxD < minD || opt < minD || maxD < opt) return;
    const int sitting = m_slot.m_multiplier[1] * 10;
    const int standing = std::max(m_slot.m_multiplier[0], m_slot.m_multiplier[2]) * 10;
    const int maxD2 = maxD * maxD, minD2 = minD * minD;
    unsigned int walls = 0;
    if (!(m_slot.m_flags & RoutingSlot::kIgnoreRooms)) {
      CTilePt at = object->GetCTilePt();
      walls = WallBlockFlagsAtTile(at, objDir);
    }
    unsigned int nearWall = 0;
    for (int b = 0; b < 8; ++b)
      if ((walls >> b) & 1) nearWall |= (1u << b) | (1u << ((b + 1) & 7)) | (1u << ((b - 1) & 7));
    const bool sits = sitting > 0;
    int res = m_slot.m_resolution;
    if (sitting != 0) res = 0x10;
    const double scale = static_cast<double>(16.0f / static_cast<float>(res));
    if (sits) sittingOnly = standing == 0;
    for (int c = 0; c < 8; ++c) {
      if (!((m_slot.m_flags >> c) & 1)) continue;
      if ((walls >> c) & 1) {
        wallFlag = true;
        continue;
      }
      const int dir = (objDir + c) % 8;
      const int seatDir = ((dir + 4) % 8 + 8) % 8;
      int far = static_cast<int>(static_cast<double>(static_cast<float>(scale * static_cast<double>(static_cast<float>(-maxD)))) * 0.0625);
      int row = static_cast<int>(static_cast<double>(static_cast<float>(scale * static_cast<double>(static_cast<float>(-minD)))) * 0.0625);
      if (sits) {
        --far;
        ++row;
        if (-1 < row) row = -1;
      }
      SetDirectionForGoalSearch(m_person, static_cast<short>(objDir), static_cast<short>(dir), &m_slot);
      bool stop = false;
      if (far <= row) {
        for (;;) {
          const int colEnd = -row;
          const int nextRow = row - 1;
          if (row <= colEnd) {
            const int ly = row * res;
            const unsigned short dirBit = static_cast<unsigned short>(1u << c);
            for (int col = row; col <= colEnd; ++col) {
              const int lx = col * res;
              TransformToWorldCoords(static_cast<float>(lx) + m_slot.m_x, static_cast<float>(ly) + m_slot.m_y, origin, dir,
                                     goal.m_pos);
              bool tryStand = false;
              for (int k = -1; k < 9; ++k) {
                int score = 0;
                int py = static_cast<int>(static_cast<float>(ly) + m_slot.m_y);
                int px = static_cast<int>(static_cast<float>(lx) + m_slot.m_x);
                short seatId = 0;
                unsigned short mask = 0;
                if (k == -1 || tryStand) {
                  bool stand;
                  if (standing < sitting) stand = k == 8;
                  else stand = k == -1;
                  if (stand) score = standing;
                  mask = dirBit;
                } else if (sitting != 0) {
                  int step[2];
                  DirectionStep(k, step);
                  // DirectionStep gives (FTilePt [0], [1]) = (local y, local x) steps.
                  px += step[1];
                  py += step[0];
                  bool outside = true;
                  const int tx = px >> 4, ty = py >> 4;
                  if (ty == row) outside = row <= tx && tx <= colEnd;
                  else if (row < ty) outside = row < tx && tx < colEnd;
                  if (outside) {
                    int at[2];
                    TransformToWorldCoords(static_cast<float>(px), static_cast<float>(py), origin, dir, at);
                    CTilePt t(at[1] >> 4, at[0] >> 4, object->m_level);
                    if (!Globs_pFixedWorld->OutOfBounds(t)) {
                      cXObjectImpl* seat = ObstacleAtLocation(at, object->m_level, false);
                      if (seat && seat->IsChair() && seat->GetData(1) == seatDir && seat->m_def) {
                        mask = static_cast<unsigned short>(1u << k);
                        if (static_cast<unsigned short>(ERQuickdata::S16(seat->m_def, 0x84)) & mask) {
                          cXObjectImpl* occupant = seat->GetContainedObject(0);
                          if (!occupant || occupant == Impl(m_person)) {
                            seatId = seat->m_id;
                            score = sitting;
                          } else {
                            seatTaken = true;
                            m_blocker = occupant->m_id;
                          }
                        }
                      }
                    }
                  }
                }
                bool added = false;
                if (score != 0) {
                  const float d2 = static_cast<float>(px * px + py * py);
                  if (d2 <= static_cast<float>(maxD2) && static_cast<float>(minD2) <= d2) {
                    const float dist = std::sqrt(d2);
                    const int off = std::abs(static_cast<int>(dist) - opt);
                    goal.m_score = (100 - static_cast<int>(static_cast<float>(off) * m_slot.m_gradient)) + score;
                    if (m_slot.m_flags & RoutingSlot::kRandomScoring)
                      goal.m_score += static_cast<int>(GetNextRandomNumber() % 0x14) - 10;
                    if (!((nearWall >> c) & 1)) goal.m_score += 0x19;
                    int at[2];
                    TransformToWorldCoords(static_cast<float>(px), static_cast<float>(py), origin, dir, at);
                    if (at[1] == m_start[1] && at[0] == m_start[0]) goal.m_score += 0x14;
                    const int r = EvalTileForGoal(goal.m_pos, dir);
                    note(r);
                    if (r == 3) stop = true;
                    if (r < 0 || r > 5) {
                      bool door = false;
                      if (m_slot.m_flags & RoutingSlot::kRandomScoring) {
                        cXObjectImpl* d = ObstacleAtLocation(at, object->m_level, true);
                        door = d && d->IsDoor();
                      }
                      if (!door) {
                        goal.m_object = seatId;
                        goal.m_dirMask = mask;
                        AddGoal(goal);
                        added = true;
                      }
                    }
                  }
                }
                if (added) break;
                tryStand = k + 1 == 8;
              }
            }
          }
          if (!(far <= nextRow) || stop) break;
          row = nextRow;
        }
      }
    }
    if (static_cast<int>(m_goals.size()) > m_maxGoals) {
      // The goals with a seat, then random ones (scanning on from a random place to the end of the list)
      // until the limit.
      std::vector<RouteGoal> all = m_goals;
      m_goals.clear();
      for (RouteGoal& g : all) {
        if (g.m_object == 0) continue;
        m_goals.push_back(g);
        g.m_score = 0;
      }
      int count = static_cast<int>(m_goals.size());
      while (count < m_maxGoals) {
        ++count;
        const unsigned int n = static_cast<unsigned int>(all.size());
        size_t i = GetNextRandomNumber() % n;
        for (; i < all.size(); ++i) {
          if (all[i].m_score == 0) continue;
          m_goals.push_back(all[i]);
          all[i].m_score = 0;
          break;
        }
      }
    }
    if (CountGoals() != 0) return;
  } else {
    int dir = -1;
    TransformToWorldCoords(m_slot.m_x, m_slot.m_y, origin, objDir, goal.m_pos);
    if (!(origin[0] == goal.m_pos[0] && origin[1] == goal.m_pos[1]) &&
        ((origin[1] >> 4) != (goal.m_pos[1] >> 4) || (origin[0] >> 4) != (goal.m_pos[0] >> 4)))
      dir = DirectionOfDelta(goal.m_pos[0] - origin[0], goal.m_pos[1] - origin[1]);
    SetDirectionForGoalSearch(m_person, static_cast<short>(objDir), 0, &m_slot);
    sittingOnly = roomFlag = seatTaken = outFlag = objectFlag = personFlag = wallFlag = false;
    const int r = EvalTileForGoal(goal.m_pos, dir);
    note(r);
    if (r == 6) {
      goal.m_score = 100;
      goal.m_object = 0;
      AddGoal(goal);
      return;
    }
  }
  (void)outFlag;
  m_failure = seatTaken ? 9 : personFlag ? 0xd : objectFlag ? 8 : wallFlag ? 0xb : roomFlag ? 0xc : sittingOnly ? 10 : 0;
}

namespace {
bool TraceRoute() {
  static const bool on = std::getenv("SIMMY_ROUTE_TRACE") != nullptr || std::getenv("SIMMY_SIM_TRACE") != nullptr;
  return on;
}
// Path's search loop (FindPath's): one A* step (state 1) or smoothing pass (state 2) at a time, up to
// `limit` steps.
void RunPath(Path& path, int limit) {
  while (path.m_state != 0 && path.m_steps <= limit) {
    if (path.m_state == 1) {
      ++path.m_steps;
      if (path.OpenANode() != 0) path.m_state = path.m_params->smooth == 0 ? 0 : 2;
    } else if (path.m_state == 2) {
      ++path.m_steps;
      if (path.DoOneSmooth()) path.m_state = 0;
    } else {
      break;  // PC: GC InitPath only leaves 0 or 1
    }
  }
}
}  // namespace

// 80155AC8
// The way through the router's room to the goal it can best reach, in `out` (way points; the start
// first, then where it crosses from one free rectangle to the next, then the goal's centre); the goal
// is the current goal. Only within the router's room (else, or without the room: no path and no current
// goal): the room's partition (BuildRoomPartition, kept by the Room), the room's objects but those
// ShouldIgnore leaves out, each its footprint grown by half a person's width (blocking; data 10: penalty
// 10, not grown; a portal's wall-blocking sides as strips of penalty 10000), people too. An object or
// person the start is inside does not block: a blocking object the router does not overlap is kept
// ungrown, else it costs 10000 and is ignored (SetIgnoringObject); a person costs 1000 where they
// overlap. A ghost ignores everything but the goals. Goals cost (best score - score) x 16. A big room
// (over 256 tiles) with few objects searches coarse rectangles first and fails (3) if that finds
// nothing. No goals: failure 1 becomes 7.
int XRoute::FindPath(TileList& out) {
  cXPersonImpl* person = PersonOf(m_person);
  cXObjectImpl* router = Impl(m_person);
  const bool ghost = person && person->IsGhost() != 0;
  if (!router) return 0;
  const int data11 = router->GetData(0x11);
  if (m_goals.empty()) {
    if (m_failure == 1) m_failure = 7;
    return 0;
  }
  const unsigned short roomId = static_cast<unsigned short>(router->GetData(0x1d));
  cXObjectImpl* object = Impl(m_object);
  if (ghost || (object && roomId == static_cast<unsigned short>(object->GetData(0x1d)))) {
    Room* room = RoomManager::GetRoomManager()->GetRoom(roomId);
    if (room) {
      int defaultPenalty = 0;
      const int outside = room->IsOutside();
      IPoint start;
      start.x = m_start[1];
      start.y = m_start[0];
      int rr[4];
      ObjRect(router, rr);
      IRect routerRect;
      routerRect.top = rr[2], routerRect.left = rr[3], routerRect.bottom = rr[0], routerRect.right = rr[1];
      if (data11 == 0 && outside) defaultPenalty = 3;
      const int half = static_cast<int>(cXObject::GetPersonWidth()) / 2;
      std::vector<PenaltyRect> obstacles;
      ObjectModuleImpl* module = Globs_pObjectModule;
      if (!ghost && module) {
        for (cXObjectImpl* o = module->m_first; o; o = o->m_nextObject) {
          if (o == router || static_cast<unsigned short>(o->GetData(0x1d)) != roomId || ShouldIgnore(o)) continue;
          int r[4];
          ObjRect(o, r);
          int pen = PenaltyRect::kBlocked;
          if (o->GetData(10) != 0) pen = 10;
          PenaltyRect pr(r[3], r[2], r[1], r[0], pen);
          const int type = o->GetType();
          if (type == 8) {
            const unsigned int f = ObjWallBlockFlags(o);
            auto strip = [&](int l, int t, int rt, int b) {
              pen = 10000;
              pr = PenaltyRect(l, t, rt, b, 10000);
              pr.top -= half, pr.left -= half, pr.bottom += half, pr.right += half;
            };
            if (f & 1) strip(r[3], r[2], r[1], r[2] + 1);
            if (f & 4) strip(r[1] - 1, r[2], r[1], r[0]);
            if (f & 0x10) strip(r[3], r[0] - 1, r[1], r[0]);
            if (f & 0x40) strip(r[3], r[2], r[3] + 1, r[0]);
          }
          if (type == 2 || pen == PenaltyRect::kBlocked) {
            pr.left -= half, pr.right += half, pr.top -= half, pr.bottom += half;
          }
          const bool startInside = pr.left <= start.x && start.x < pr.right && pr.top <= start.y && start.y < pr.bottom;
          if (!startInside) {
            obstacles.push_back(pr);
            continue;
          }
          IRect overlap;
          if (!dynamic_cast<cXPersonImpl*>(o)) {
            if (pr.penalty != PenaltyRect::kBlocked) continue;
            pr.left += half, pr.right -= half, pr.top += half, pr.bottom -= half;
            if (overlap.Intersect(&pr, &routerRect) == 0) {
              obstacles.push_back(pr);
            } else {
              pr.penalty = 10000;
              obstacles.push_back(pr);
              if (person) person->SetIgnoringObject(o);
            }
          } else {
            pr.left += half, pr.right -= half, pr.top += half, pr.bottom -= half;
            if (overlap.Intersect(&pr, &routerRect) == 0) {
              obstacles.push_back(pr);
            } else {
              pr.penalty = 1000;
              obstacles.push_back(pr);
            }
            if (person) person->SetIgnoringObject(o);
          }
        }
      }
      CollapsePenaltyRects(obstacles);
      std::vector<PenaltyRect> goalRects;
      std::vector<int> goalIndex;
      goalRects.reserve(m_goals.size());
      goalIndex.reserve(m_goals.size());
      RoutingParams params;
      params.obstacles = &obstacles;
      if (!ghost) params.partition = room->GetPartition();
      params.defaultPenalty = defaultPenalty;
      params.goals = &goalRects;
      for (int i = 0; i < CountGoals(); ++i) {
        const RouteGoal* g = GetNthGoal(i);
        if (g->m_score <= 0) continue;
        goalRects.push_back(PenaltyRect(g->m_pos[1], g->m_pos[0], g->m_pos[1] + 1, g->m_pos[0] + 1, (m_maxScore - g->m_score) * 0x10));
        goalIndex.push_back(i);
      }
      if (ghost || (room->GetObjectDensity() <= 0.2f && room->GetArea() > 99)) {
        params.limitSize = 1;
        params.maxSize = 0x80;
      } else {
        params.limitSize = ghost ? 1 : 0;
      }
      params.start = start;
      params.noGoalCentre = 0;
      params.limitAspect = 0;
      params.field1C = 1;
      params.smooth = 1;
      int limit = static_cast<int>(goalRects.size()) * room->GetArea() + 0x32;
      if (1000 < limit) limit = 1000;
      bool none = false;
      Path path;
      if (0x100 < room->GetArea() && params.limitSize != 0) {
        params.limitSize = 0;
        path.InitPath(&params);
        RunPath(path, limit);
        none = path.m_points.empty();
        params.limitSize = 1;
        Path::fSpacePartition.Clear();
        path.m_chain.clear();
        path.m_pathNodes.clear();
        path.m_points.clear();
        path.m_state = -1;
      }
      if (none) {
        m_failure = 3;
        if (TraceRoute()) std::printf("[route] %s: no way out of the coarse search\n", router->DebugName());
        return 0;
      }
      path.InitPath(&params);
      RunPath(path, limit);
      if (path.m_goalRect == -1) ClearCurrentGoal();
      else SetCurrentGoal(goalIndex[static_cast<size_t>(path.m_goalRect)]);
      for (const IPoint& p : path.m_points) {
        TileListPoint t;
        t.p[0] = p.y;
        t.p[1] = p.x;
        out.m_points.push_back(t);
      }
      if (!path.m_points.empty()) return 1;
      m_failure = 3;
      if (TraceRoute())
        std::printf("[route] %s: no way to %zu goals (%zu obstacles, %zu partition rects, %d steps)\n", router->DebugName(),
                    goalRects.size(), obstacles.size(), params.partition ? params.partition->size() : static_cast<size_t>(0),
                    path.m_steps);
      return 0;
    }
  }
  ClearCurrentGoal();
  return 0;
}

// 80157C44
// The point of the way nearest `pos` (rounded), on the legs from point `last` - 1 to `last` (last 0:
// the first leg; < 0: every leg but the last one, which only says how to face); `pos` becomes it.
void TileList::FindNearestPoint(int pos[2], int last) {
  const int n = static_cast<int>(m_points.size());
  int lo = 0, hi = 0;
  if (last < 0) {
    hi = n - 3;
  } else if (last > 0) {
    lo = hi = last - 1;
  }
  if (lo < 0) return;
  const unsigned int lim = static_cast<unsigned int>(n - 3);
  if (!(static_cast<unsigned int>(lo) <= lim && hi >= 0 && static_cast<unsigned int>(hi) <= lim)) return;
  int best0 = m_points[static_cast<size_t>(lo)].p[0], best1 = m_points[static_cast<size_t>(lo)].p[1];
  float best = static_cast<float>((best1 - pos[1]) * (best1 - pos[1]) + (best0 - pos[0]) * (best0 - pos[0]));
  for (int i = lo; i <= hi; ++i) {
    const int a0 = m_points[static_cast<size_t>(i)].p[0], a1 = m_points[static_cast<size_t>(i)].p[1];
    const int b0 = m_points[static_cast<size_t>(i + 1)].p[0], b1 = m_points[static_cast<size_t>(i + 1)].p[1];
    const float f1 = static_cast<float>(a1 - b1), f0 = static_cast<float>(a0 - b0);
    const float len = f1 * f1 + f0 * f0;
    float t = len != 0.0f ? (static_cast<float>(pos[1] - b1) * f1 + static_cast<float>(pos[0] - b0) * f0) / len : 0.0f;
    if (!(0.0f <= t)) t = 0.0f;
    else if (1.0f < t) t = 1.0f;
    const int q1 = static_cast<int>(t * static_cast<float>(a1 - b1) + static_cast<float>(b1) + 0.5f);
    const int q0 = static_cast<int>(t * static_cast<float>(a0 - b0) + static_cast<float>(b0) + 0.5f);
    const float d = static_cast<float>((q1 - pos[1]) * (q1 - pos[1]) + (q0 - pos[0]) * (q0 - pos[0]));
    if (d < best) {
      best = d;
      best0 = q0;
      best1 = q1;
    }
  }
  pos[0] = best0;
  pos[1] = best1;
}

// ---------------------------------------------------------------------------------------------------
// The router's parts of game/object.cpp (cXObjectImpl)

// 800D95EC
RET_UNKNOWN cXObject::GetPersonWidth() { return 6; }  // cXObjectImpl::gPersonWidth (804FDC74)

// 800EB2A0
RET_UNKNOWN cXObjectImpl::GetRoom() { return static_cast<unsigned short>(GetData(0x1d)); }

// 800EAE40
// A portal part with a portal tree (entry 0xF).
RET_UNKNOWN cXObjectImpl::IsDoor() { return GetType() == 8 && GetTreeID(0xf) != 0 && IsPortalObject(this) ? 1 : 0; }

// 800DC968
// PC: over the objects whose site is the tile (GC: the tile's root object and its siblings).
cXObjectImpl* ObstacleAtLocation(const int pos[2], int level, bool preferDoor) {
  CTilePt t(pos[1] >> 4, pos[0] >> 4, level);
  if (!Globs_pFixedWorld || Globs_pFixedWorld->OutOfBounds(t)) return nullptr;
  cXObjectImpl* door = nullptr;
  for (cXObjectImpl* o : TileObjects(t)) {
    if (!ObjCanIntersectPeople(o)) return o;
    if (preferDoor && o->IsDoor()) door = o;
  }
  return door;
}

// 800E0080
// Beyond 5 tiles the straight distance plus, sampled every 2 tiles along the line (from the first
// step until past `other`), 5 per blocked tile (flag 0x10) and 10 per tile of another room.
double cXObjectImpl::CalcDistanceWithPentalties(cXObject* other) {
  const double d = CalcShortDistance(other);
  cXObjectImpl* o = Impl(other);
  if (!o || d < 5.0) return d;
  cFixedWorldImpl* w = Globs_pFixedWorld;
  double penalty = 0.0;
  const int to0 = o->m_location[0], to1 = o->m_location[1];
  const unsigned short room = static_cast<unsigned short>(GetData(0x1d));
  const int from1 = m_location[1], from0 = m_location[0];
  const double step1 = static_cast<float>(static_cast<double>(static_cast<float>(to1 - from1) * 2.0f) / d);
  const double step0 = static_cast<float>(static_cast<double>(static_cast<float>(to0 - from0) * 2.0f) / d);
  double p1 = static_cast<float>(static_cast<float>(from1) + step1);
  double p0 = static_cast<float>(static_cast<float>(from0) + step0);
  bool alongFirst = false, increasing = false;
  if (from1 == to1) {
    increasing = from0 < to0;
    alongFirst = true;
  } else if (from1 < to1) {
    increasing = true;
  }
  for (bool done = false; !done;) {
    CTilePt t(static_cast<int>(p1) >> 4, static_cast<int>(p0) >> 4, 1);
    if (w->GetFlags(t) & 0x10) penalty = static_cast<float>(penalty + 5.0);
    else if (w->GetRoom(t) != room) penalty = static_cast<float>(penalty + 10.0);
    p1 = static_cast<float>(p1 + step1);
    p0 = static_cast<float>(p0 + step0);
    if (!alongFirst) done = increasing ? static_cast<float>(to1) < p1 : p1 < static_cast<float>(to1);
    else done = increasing ? !(p0 <= static_cast<float>(to0)) : p0 < static_cast<float>(to0);
  }
  return static_cast<float>(d + penalty);
}

// 800EAEE8
void cXObjectImpl::SetData(int index, short value) { m_data.WriteVar(index, value); }
