// Port of game/influencemap.cpp: the interaction influence map (see influencemap.hpp) and
// ObjectModuleImpl::UpdateInteractionInfluences / cFixedWorldImpl::GetInfluenceMap.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/influencemap.hpp"
#include "simmy/classes/collisionmap.hpp"

#include <algorithm>

const unsigned char* SelectorTreeTabResFile(ObjSelector* selector);  // treesim.cpp
int ObjIsInWorld(cXObjectImpl* o);                                   // 800DCB50 (objectplace.cpp)

namespace {
// cXObjectImpl::GetTreeTab (vtable 0x364) as the port reads it: the selector's tree table rows (0x24
// bytes each) and their count.
const unsigned char* TreeTabEntries(cXObjectImpl* o, unsigned int& count) {
  count = 0;
  const unsigned char* file = o ? SelectorTreeTabResFile(o->m_selector) : nullptr;
  const auto* tables = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x24)) : nullptr;
  if (!tables || ERQuickdata::U32(tables - 4, 0) == 0) return nullptr;
  const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(tables, 0));
  count = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
  return entries;
}
// TreeTableEntryQuickData::GetUseCustomActivation (8015E7D4): flags (+0x10) bit 12.
bool UseCustomActivation(const unsigned char* e) { return (ERQuickdata::U32(e, 0x10) >> 12 & 1) != 0; }
bool IsPersonObject(cXObject* o) {
  cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
  return impl && dynamic_cast<cXPersonImpl*>(impl) != nullptr;
}
// The object's entries that make its area: each custom one, and the first that is not (the others
// share its area).
template <typename F>
void ForEachAreaEntry(cXObjectImpl* o, F f) {
  unsigned int n = 0;
  const unsigned char* entries = TreeTabEntries(o, n);
  if (!entries) return;
  bool plainSeen = false;
  for (unsigned int i = 0; i < n; ++i) {
    const unsigned char* e = entries + i * 0x24;
    bool use = false;
    if (!UseCustomActivation(e)) {
      if (!plainSeen) {
        plainSeen = true;
        use = true;
      }
    } else {
      use = true;
    }
    if (use) f(e);
  }
}
// A tile coordinate in 1/16 tiles to its tile, as GC divides (rounding toward zero); `end` the tile of
// the last 1/16 before it.
int TileOf(int v) { return (v < 0 ? v + 0xf : v) >> 4; }
int LastTileBefore(int v) {
  const int w = v - 1;
  return (w < 0 ? v + 0xe : w) >> 4;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// ObjectList

// 80048570
ObjectList::ObjectList() {}
// 80048590
// Listed once.
void ObjectList::AddObject(cXObject* object) {
  if (std::find(m_objects.begin(), m_objects.end(), object) != m_objects.end()) return;
  m_objects.push_back(object);
}
// 80048714
void ObjectList::RemoveObject(cXObject* object) {
  auto it = std::find(m_objects.begin(), m_objects.end(), object);
  if (it != m_objects.end()) m_objects.erase(it);
}
// 80048798
void ObjectList::RemoveAll() { std::vector<cXObject*>().swap(m_objects); }

// ---------------------------------------------------------------------------------------------------
// InfluenceMap

// 80048844
InfluenceMap::InfluenceMap(int xSize, int ySize) : m_xSize(xSize), m_ySize(ySize) {
  m_lists.resize(static_cast<size_t>(std::max(xSize * ySize, 0)));
}
// 800488BC
InfluenceMap::~InfluenceMap() { RemoveAll(); }

// 80048968
void InfluenceMap::RemoveAll() {
  for (ObjectList& l : m_lists) l.RemoveAll();
}

// 800489EC
// The entry's activation area round the object's tile: a custom one is the entry's sub-tile and
// extents (+0x20: 4 bits each sub-tile x and y, then 6 bits each extent), else a footprint-sized area
// (GetNumTiles) with 8 (half a tile) on the near sides; a further half tile all round. A multi-tile part
// offsets it by its place in the group (definition +0x16: x in the high byte, y in the low). Objects
// (not people) turn it by their direction (data 1: 2, 4, 6); then it moves to the object's location and
// is kept on the lot.
void InfluenceMap::CalculateAffectedArea(cXObjectImpl* object, const unsigned char* entry, int rect[4]) {
  unsigned char subX, subY, e0, e1, e2, e3;
  if (!UseCustomActivation(entry)) {
    int w = 1, h = 1;
    if (auto* mt = dynamic_cast<cXMTObjectImpl*>(object)) mt->GetNumTiles(w, h);
    subX = 0;
    subY = 0;
    e1 = static_cast<unsigned char>(static_cast<unsigned char>(w << 2) + 4);
    e2 = static_cast<unsigned char>(static_cast<unsigned char>(h << 2) + 4);
    e0 = 8;
    e3 = 8;
  } else {
    const unsigned int a = ERQuickdata::U32(entry, 0x20);  // GetActivationSubtile / GetActivationExtents
    subX = static_cast<unsigned char>(a & 0xf);
    subY = static_cast<unsigned char>(a >> 4 & 0xf);
    e0 = static_cast<unsigned char>(a >> 8 & 0x3f);
    e1 = static_cast<unsigned char>(a >> 14 & 0x3f);
    e2 = static_cast<unsigned char>(a >> 20 & 0x3f);
    e3 = static_cast<unsigned char>(a >> 26);
  }
  rect[3] = static_cast<int>(e3) * -4 - 8;
  rect[0] = static_cast<int>(e2) * 4 + 8;
  rect[2] = static_cast<int>(e0) * -4 - 8;
  rect[1] = static_cast<int>(e1) * 4 + 8;
  const ObjDefinition* def = object->GetDef();
  const short sub = def ? objdef::SubIndex(def) : 0;
  const int offA = (static_cast<int>(subX) - (static_cast<int>(sub) >> 8)) * 0x10;
  const int offB = (static_cast<int>(subY) - static_cast<int>(static_cast<unsigned char>(sub & 0xff))) * 0x10;
  rect[3] += offA;
  rect[2] += offB;
  rect[1] += offA;
  rect[0] += offB;
  if (!IsPersonObject(object)) {
    const int dir = object->GetData(1);
    if (dir == 2) {
      const int t = rect[3];
      rect[3] = -rect[0];
      rect[0] = rect[1];
      rect[1] = -rect[2];
      rect[2] = t;
    } else if (dir == 4) {
      const int t3 = rect[3], t0 = rect[0];
      rect[3] = -rect[1];
      rect[1] = -t3;
      rect[0] = -rect[2];
      rect[2] = -t0;
    } else if (dir == 6) {
      const int t = rect[3];
      rect[3] = rect[2];
      rect[2] = -rect[1];
      rect[1] = rect[0];
      rect[0] = -t;
    }
  }
  rect[3] += object->m_location[1];
  rect[2] += object->m_location[0];
  rect[1] += object->m_location[1];
  rect[0] += object->m_location[0];
  if (rect[3] < 0) rect[3] = 0;
  if (rect[2] < 0) rect[2] = 0;
  if (m_xSize << 4 < rect[1]) rect[1] = m_xSize << 4;
  if (m_ySize << 4 < rect[0]) rect[0] = m_ySize << 4;
}

// 80048D44
// An interaction leader in the world with a model and a tree table: its area entries' tiles.
void InfluenceMap::AddObjectToMap(cXObjectImpl* object) {
  if (!object || object != object->GetInteractionLeader()) return;
  if (!ObjIsInWorld(object) || !object->m_instance) return;
  ValidateMissing(object);
  ForEachAreaEntry(object, [this, object](const unsigned char* e) { AddObjectEntry(object, e); });
}

// 80048EA8
// Each tile of the entry's area whose centre the object's location reaches without a wall (radius 0.1).
void InfluenceMap::AddObjectEntry(cXObjectImpl* object, const unsigned char* entry) {
  int rect[4];
  CalculateAffectedArea(object, entry, rect);
  EVec2 from{static_cast<float>(object->m_location[0]) * 0.0625f, static_cast<float>(object->m_location[1]) * 0.0625f};
  for (int row = TileOf(rect[3]); row <= LastTileBefore(rect[1]); ++row)
    for (int col = TileOf(rect[2]); col <= LastTileBefore(rect[0]); ++col) {
      EVec2 to{static_cast<float>(col) + 0.5f, static_cast<float>(row) + 0.5f};
      EVec2 hit;
      if (!CollisionMap::DoesCollideEnvironment(from, to, 0.1f, false, false, hit))
        if (ObjectList* l = GetObjectList(row, col)) l->AddObject(object);  // GC: unchecked index
    }
}

// 80049074
void InfluenceMap::RemoveObjectFromMap(cXObjectImpl* object) {
  if (!object || object != object->GetInteractionLeader()) return;
  if (!ObjIsInWorld(object) || !object->m_instance) return;
  ForEachAreaEntry(object, [this, object](const unsigned char* e) { RemoveObjectEntry(object, e); });
}

// 800491CC
void InfluenceMap::RemoveObjectEntry(cXObjectImpl* object, const unsigned char* entry) {
  int rect[4];
  CalculateAffectedArea(object, entry, rect);
  for (int row = TileOf(rect[3]); row <= LastTileBefore(rect[1]); ++row)
    for (int col = TileOf(rect[2]); col <= LastTileBefore(rect[0]); ++col)
      if (ObjectList* l = GetObjectList(row, col)) l->RemoveObject(object);
}

// 80049294
ObjectList* InfluenceMap::GetObjectList(int row, int col) {
  if (row < 0 || col < 0 || row >= m_xSize || col >= m_ySize) return nullptr;
  return &m_lists[static_cast<size_t>(row * m_ySize + col)];
}

// 800492E0
int InfluenceMap::ValidateMissing(cXObjectImpl*) { return 1; }

// 800492E8
// GC: a debug check that each list's first object is in the resource heap (EAHeap::FindHeapForAddress,
// result unused). PC: nothing to check.
void InfluenceMap::Validate() {}

void InfluenceMap::ForgetObject(cXObject* object) {
  for (ObjectList& l : m_lists) l.RemoveObject(object);
}

// ---------------------------------------------------------------------------------------------------

// 8015235C
InfluenceMap* cFixedWorldImpl::GetInfluenceMap() { return m_influence; }

// 800F5DD0
// The map emptied, then every object that is not a person put in.
void ObjectModuleImpl::UpdateInteractionInfluences() {
  InfluenceMap* map = Globs_pFixedWorld ? Globs_pFixedWorld->GetInfluenceMap() : nullptr;
  if (!map) return;
  map->RemoveAll();
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject)
    if (!IsPersonObject(o)) map->AddObjectToMap(o);
}

// 800DF784 (cXObjectImpl::SetLocation), the influence map's part.
void InfluenceMapObjectLeaving(cXObjectImpl* object) {
  InfluenceMap* map = Globs_pFixedWorld ? Globs_pFixedWorld->GetInfluenceMap() : nullptr;
  if (!map || !object || IsPersonObject(object)) return;
  map->RemoveObjectFromMap(object);
}
void InfluenceMapObjectArrived(cXObjectImpl* object) {
  InfluenceMap* map = Globs_pFixedWorld ? Globs_pFixedWorld->GetInfluenceMap() : nullptr;
  if (!map || !object) return;
  // Not a person, nor held by one (up its container chain).
  for (cXObject* o = object; o; o = o->GetContainer())
    if (IsPersonObject(o)) return;
  map->AddObjectToMap(object);
}
