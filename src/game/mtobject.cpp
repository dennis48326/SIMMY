// Port of game/mtobject.cpp (cXMTObjectImpl, the simulation side): the parts of a multi-tile object, the
// chain from the group's lead, the parts' offsets, and cXObjectImpl::GetInteractionLeader (game/object.cpp),
// the part whose interactions stand for the group. The placement side (CanPlace, Place, Pickup, Turn and the
// User* tests, dynamic merging) is buy mode's (objectplace.cpp: PlacementGroup) and stays stubbed here.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/placement.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
// 800D9430 ObjDefinition::GetMultiTileOffsets: the tile of a part from the lead (sub index >> 8, low byte).
void GetMultiTileOffsets(const ObjDefinition* def, int* x, int* y) {
  const short sub = def ? objdef::SubIndex(def) : 0;
  *x = sub >> 8;
  *y = sub & 0xff;
}

cXMTObjectImpl* AsMT(cXObject* o) {
  cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
  return impl ? dynamic_cast<cXMTObjectImpl*>(impl) : nullptr;
}

// A part's place among its group's sub selectors (the order MakeNewOutOfWorldObject makes them in).
int SubSelectorIndex(cXObjectImpl* o, int* count) {
  ObjectFolderImpl* folder = Globs_pObjectFolder;
  int index = -1, n = 0;
  for (; folder && o->m_selector; ++n) {
    auto* s = reinterpret_cast<ObjSelector*>(folder->GetNthSubSelector(o->m_selector, n));
    if (!s) break;
    if (s == o->m_selector) index = n;
  }
  if (count) *count = n;
  return index;
}

// The offsets turned by a direction as AssignOffsets turns them.
void Turned(int dir, int x, int y, int* ox, int* oy) {
  switch (dir & 7) {
    case 0: *ox = x, *oy = y; break;
    case 2: *ox = -y, *oy = x; break;
    case 4: *ox = -x, *oy = -y; break;
    case 6: *ox = y, *oy = -x; break;
    default: *ox = *oy = 0; break;
  }
}

void Chain(std::vector<cXMTObjectImpl*>& parts, bool complete) {
  for (cXMTObjectImpl* p : parts) {
    p->m_lead = nullptr;
    p->m_nextPart = nullptr;
  }
  for (size_t i = 1; i < parts.size(); ++i) parts[i]->SetLeader(parts[0]);
  for (cXMTObjectImpl* p : parts) {
    GetMultiTileOffsets(p->m_def, &p->m_offset[0], &p->m_offset[1]);
    p->m_offset[2] = p->m_def ? ERQuickdata::S16(p->m_def, 0x70) : 0;
    p->m_linked = complete;
  }
  if (!parts.empty()) parts[0]->AssignOffsets();
}
}  // namespace

// 800C3684
// GC: the cXObjectImpl, its TreeSim and cXMTObject interface, then SetLeader(lead) and the offsets
// cleared (Initialize or the stream sets them).
cXMTObjectImpl::cXMTObjectImpl(int, ObjSelector* selector, cXMTObjectImpl* lead, ObjectModule* module)
    : cXObjectImpl(0, selector, module) {
  SetLeader(lead);
  if (lead) m_linked = lead->m_linked = true;
}

// 800C489C
// GC: also the cXObjectImpl and the interface; the part leaves its chain.
cXMTObjectImpl::~cXMTObjectImpl() { RemoveFromChain(); }

// 800C5848
void cXMTObjectImpl::Initialize() {
  cXObjectImpl::Initialize();
  GetMultiTileOffsets(GetDef(), &m_offset[0], &m_offset[1]);
  m_offset[2] = ERQuickdata::S16(GetDef(), 0x70);
}

// 800C58CC
void cXMTObjectImpl::SetLeader(cXMTObjectImpl* lead) {
  m_lead = lead;
  m_nextPart = nullptr;
  if (!lead) return;
  cXMTObjectImpl* last = lead;
  while (last->m_nextPart) last = last->m_nextPart;
  last->m_nextPart = this;
}

// 800C5BAC
void cXMTObjectImpl::RemoveFromChain() {
  if (!m_lead) {
    // The lead: the next part leads the rest.
    cXMTObjectImpl* next = m_nextPart;
    for (cXMTObjectImpl* p = next; p; p = p->m_nextPart) p->m_lead = next;
    if (next) next->m_lead = nullptr;
  } else {
    for (cXMTObjectImpl* p = m_lead; p; p = p->m_nextPart)
      if (p->m_nextPart == this) {
        p->m_nextPart = m_nextPart;
        break;
      }
  }
  m_lead = nullptr;
  m_nextPart = nullptr;
}

// 800C5F10
void cXMTObjectImpl::AssignOffsets() {
  cXMTObjectImpl* lead = m_lead ? m_lead : this;
  const int base[3] = {lead->m_offset[0], lead->m_offset[1], lead->m_offset[2]};
  const int dir = lead->m_data[1] & 7;
  for (cXMTObjectImpl* p = lead; p; p = p->m_nextPart) {
    const int x = p->m_offset[0] - base[0], y = p->m_offset[1] - base[1];
    p->m_turned[2] = p->m_offset[2] - base[2];
    Turned(dir, x, y, &p->m_turned[0], &p->m_turned[1]);
  }
}

// 800C6AD0
bool cXMTObjectImpl::IsPartOfMe(cXObject* other) {
  EnsureLinked();
  for (cXMTObjectImpl* p = m_lead ? m_lead : this; p; p = p->m_nextPart)
    if (static_cast<cXObject*>(p) == other) return true;
  return false;
}

// 800C71F0
// A dynamic multi-tile object: one with a tree for entry point 8 (its parts are merged and split as
// it is placed).
bool cXMTObjectImpl::IsDynamic() { return GetTreeID(8) != 0; }

// 800C6C50
int cXMTObjectImpl::ReconType() { return 0x584d544f; }  // 'XMTO'

// 800C6C5C
// After the object's stream the turned offsets (+0x1C, +0x20, from version 0x3A +0x24), from version
// 0x28 the offsets (+0x10..+0x18), then the lead's id (16 bits, 0 for a lead; read: SetLeader to it).
// PC: a part read with its lead is linked (LinkMultiTileGroup leaves it alone); a lead learns it is
// linked when its first part joins it.
void cXMTObjectImpl::ReconStream(ReconBuffer* b, int version, bool resize) {
  cXObjectImpl::ReconStream(b, version, resize);
  b->ReconInt(&m_turned[0], 1);
  b->ReconInt(&m_turned[1], 1);
  if (version > 0x39) b->ReconInt(&m_turned[2], 1);
  if (version > 0x27) {
    b->ReconInt(&m_offset[0], 1);
    b->ReconInt(&m_offset[1], 1);
    b->ReconInt(&m_offset[2], 1);
  }
  if (b->m_mode != ReconBuffer__Mode::kRead) EnsureLinked();
  short leadId = m_lead ? static_cast<short>(m_lead->GetID()) : 0;
  b->Recon16(&leadId, 1);
  if (b->m_mode == ReconBuffer__Mode::kRead && leadId != 0) {
    cXObject* o = GetObjectFromID(leadId);
    auto* lead = o ? dynamic_cast<cXMTObjectImpl*>(o->GetObjectImplementation()) : nullptr;
    if (lead && lead != this) {
      SetLeader(lead);
      m_linked = lead->m_linked = true;
    }
  }
  if (m_def && objdef::Type(m_def) == 8) ReconPortalStream(this, b, version);
}

// PC: the portal part of a portal's stream (GC cXPortalImpl::ReconStream, at 8013058C, after the multi-
// tile part): from version 0xF the count of its route values (+0x08 vector of floats), then the values
// (before 0x26 as ints, read as nothing).
void ReconPortalStream(cXMTObjectImpl* portal, ReconBuffer* b, int version) {
  if (version <= 0xe) return;
  int count = static_cast<int>(portal->m_portalValues.size());
  b->ReconInt(&count, 1);
  if (b->m_mode == ReconBuffer__Mode::kRead) portal->m_portalValues.assign(static_cast<size_t>(count > 0 ? count : 0), 0.0f);
  if (version < 0x26) {
    for (int i = 0; i < count; ++i) {
      int unused = 0;
      b->ReconInt(&unused, 1);
    }
  } else if (count > 0) {
    b->ReconFloat(portal->m_portalValues.data(), count);
  }
}

// 800C6DAC
// GC: saves before version 0x3A reset the group (vtable 0x2C), a dynamic group re-merges (0x44 / 0x4C).
void cXMTObjectImpl::PostLoad(int version, bool b) {
  cXObjectImpl::PostLoad(version, b);
  SIMMY_STUB("cXMTObjectImpl::PostLoad: old saves' group reset, dynamic groups", 0x800C6DAC);
}

// 800C798C
void cXMTObjectImpl::GetNumTiles(int& x, int& y) {
  x = y = 1;
  for (cXMTObjectImpl* p = GetFirstMultiTileObject(); p; p = p->GetNextMultiTileObject()) {
    const short sub = objdef::SubIndex(p->GetDef());
    x = std::max(x, (sub >> 8) + 1);
    y = std::max(y, (sub & 0xff) + 1);
  }
}

// 800C7D14
cXMTObjectImpl* cXMTObjectImpl::GetFirstMultiTileObject() {
  EnsureLinked();
  return m_lead ? m_lead : this;
}

// 800C7D44
cXMTObjectImpl* cXMTObjectImpl::GetNextMultiTileObject() {
  EnsureLinked();
  return m_nextPart;
}

void cXMTObjectImpl::EnsureLinked() {
  if (!m_linked) LinkMultiTileGroup(this);
}

// ---------------------------------------------------------------------------------------------------
// PC: the chains of the house's groups

void LinkMultiTileGroup(cXMTObjectImpl* part) {
  if (!part || part->m_linked || !part->m_def) return;
  std::vector<cXMTObjectImpl*> parts;
  // A group buy mode made (CreateObjectFromGUID) or found: its parts, its lead first.
  if (PlacementGroup* g = PlacementGroupOf(part)) {
    for (auto& p : g->parts)
      if (cXMTObjectImpl* mt = dynamic_cast<cXMTObjectImpl*>(p.object)) parts.push_back(mt);
    if (std::find(parts.begin(), parts.end(), part) != parts.end()) {
      Chain(parts, true);
      return;
    }
    parts.clear();
  }
  int count = 0;
  SubSelectorIndex(part, &count);
  const short master = objdef::MasterID(part->m_def);
  const int dir = part->m_data[1] & 7;
  int x = 0, y = 0, ox = 0, oy = 0;
  GetMultiTileOffsets(part->m_def, &x, &y);
  Turned(dir, x, y, &ox, &oy);
  const CTilePt tile = part->GetCTilePt();
  const int leadX = tile.GetX() - ox, leadY = tile.GetY() - oy;
  const int leadLevel = part->m_level - ERQuickdata::S16(part->m_def, 0x70);
  const bool inWorld = part->m_location[0] >= 0 && part->m_location[1] >= 0;
  for (cXObjectImpl* o = Globs_pObjectModule ? Globs_pObjectModule->m_first : nullptr; inWorld && o;
       o = o->m_nextObject) {
    auto* mt = dynamic_cast<cXMTObjectImpl*>(o);
    if (!mt || mt->m_linked || !o->m_def || !o->m_selector || !part->m_selector ||
        !o->m_selector->TestFromSameFile(part->m_selector) || objdef::MasterID(o->m_def) != master ||
        objdef::SubIndex(o->m_def) == -1 || (o->m_data[1] & 7) != dir)
      continue;
    GetMultiTileOffsets(o->m_def, &x, &y);
    Turned(dir, x, y, &ox, &oy);
    const CTilePt t = o->GetCTilePt();
    if (t.GetX() == leadX + ox && t.GetY() == leadY + oy && o->m_level - ERQuickdata::S16(o->m_def, 0x70) == leadLevel)
      parts.push_back(mt);
  }
  if (std::find(parts.begin(), parts.end(), part) == parts.end()) parts.insert(parts.begin(), part);
  std::stable_sort(parts.begin(), parts.end(), [](cXMTObjectImpl* a, cXMTObjectImpl* b) {
    return SubSelectorIndex(a, nullptr) < SubSelectorIndex(b, nullptr);
  });
  // Only a whole group is kept (a part made out of the world on its own is linked again later).
  Chain(parts, static_cast<int>(parts.size()) >= count);
}

void LinkMultiTileGroups(ObjectModuleImpl* module) {
  for (cXObjectImpl* o = module ? module->m_first : nullptr; o; o = o->m_nextObject)
    if (auto* mt = dynamic_cast<cXMTObjectImpl*>(o)) LinkMultiTileGroup(mt);
}

// ---------------------------------------------------------------------------------------------------
// cXObjectImpl (game/object.cpp)

// 800E19D8
// The part whose interactions stand for its group: for a multi-tile part with a positive interaction
// group (definition +0x10), the part of the group whose group is its negative; else the object itself.
cXObjectImpl* cXObjectImpl::GetInteractionLeader() {
  if (IsMultiTile()) {
    const short group = ERQuickdata::S16(GetDef(), 0x10);
    if (group > 0)
      if (cXMTObjectImpl* mt = AsMT(this)) {
        for (cXMTObjectImpl* p = mt->GetFirstMultiTileObject(); p; p = p->GetNextMultiTileObject())
          if (ERQuickdata::S16(p->GetDef(), 0x10) == -group) return p;
      }
  }
  return this;
}

// 800E318C
// A tile people interact with: in the world, and (unless the definition's +0x8A says never (2)) a
// person, a single-tile object or a group's interaction leader, or (unless +0x8A is 1) one flagged so
// (misc flag 8).
RET_UNKNOWN cXObjectImpl::IsInteractionTile() {
  if (!ObjIsInWorld(this)) return 0;
  const ObjDefinition* def = GetDef();
  const short mode = ERQuickdata::S16(def, 0x8a);
  if (mode == 2) return 0;
  if (objdef::Type(def) == 2 || objdef::MasterID(def) == 0 || ERQuickdata::S16(def, 0x10) < 0) return 1;
  return mode != 1 && GetMiscFlag(cXObject__MiscFlag{8}) ? 1 : 0;
}
