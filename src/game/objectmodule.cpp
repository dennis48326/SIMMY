// Port of game/objectmodule.cpp (ObjectModuleImpl, partial): the house's objects. Ported: loading
// them from the house's 'ObjM' resource (each object's id and type, then its own stream) and giving
// each its 3D instance, multi-tile parts (cXMTObjectImpl, mtobject.cpp) and groups made whole, the kill
// queue. Still stubbed: people and portals as their own classes (people are read as plain objects,
// portals as their multi-tile part), the simulator's side (the object map, the routing slots, passive
// influences), saving.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/influencemap.hpp"
#include "simmy/classes/placement.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

ObjectModuleImpl* Globs_pObjectModule = nullptr;  // 804FDC08 Globs::pObjectModule

namespace {
constexpr int kObjectModuleResType = 0x4f626a4d;  // 'ObjM'
int s_loadedVersion = 0;  // PC: the version Load read (GC: HouseImpl::LoadFile passes it to PostLoad)
constexpr unsigned int kPlaceholderGUID = 0x3c08965f;
}  // namespace

// 800F2C54
void ObjectModule::CreateInstance() { Globs_pObjectModule = new ObjectModuleImpl; }

// 800F2C7C
void ObjectModule::DestroyInstance(ObjectModule*) {
  delete Globs_pObjectModule;
  Globs_pObjectModule = nullptr;
}

// 800F2CBC
ObjectModuleImpl::ObjectModuleImpl() {}

// 800F2DCC
ObjectModuleImpl::~ObjectModuleImpl() { KillAllObjects(); }

// 800F5E78
// GC: each object is removed from the simulation (its instance, the object map, the people).
void ObjectModuleImpl::KillAllObjects() {
  RemoveFamilyFromLot();  // PC: the lot's people first (person.cpp's list owns them; they leave the module there)
  while (m_first) {
    cXObjectImpl* next = m_first->m_nextObject;
    delete m_first;
    m_first = next;
  }
  m_byId.clear();
  if (PassiveInfluenceSystem* influence = Globs_pSimulator ? Globs_pSimulator->GetPassiveInfluenceSystem() : nullptr)
    influence->FlushSystem();
}

// 800F30BC
// The house's objects from `file`: the type table first (the folder), the old objects gone, then the
// 'ObjM' stream. 0: loaded.
int ObjectModuleImpl::Load(iResFile* file) {
  ObjectFolderImpl* folder = Globs_pObjectFolder;
  SIMMY_STUB("ObjectModuleImpl::Load: ObjectFolder::FreeUnusedData", 0x800F30BC);
  folder->PrepareForModuleLoad(file);
  KillAllObjects();
  int version = 0;
  const int r = ReconLoadObject(this, file, kObjectModuleResType, 1, &version);
  s_loadedVersion = version;
  // The tile map: each object (not one held in another's slot, data 0x1A) at its tile.
  // PC: an object chained (entry 0) under one the save does not have (the port saves no people) is
  // its tile's object again.
  for (short& id : m_tileMap) id = 0;
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) {
    o->m_siteTile = -1;
    if (o->GetData(0x1a) != 0 && o->GetData(0xe) == 0 && !GetObjectFromID(o->GetData(0x1a))) {
      o->m_data.WriteVar(0x1a, 0);
      o->m_data.WriteVar(0xe, -1);
    }
    CTilePt tile = o->GetCTilePt();
    if (static_cast<unsigned int>(tile.GetX()) < 0x40 && static_cast<unsigned int>(tile.GetY()) < 0x40 &&
        o->GetData(0x1a) == 0) {
      SetTileObjectID(tile, static_cast<short>(o->GetID()));
      o->m_siteTile = tile.GetX() + tile.GetY() * 0x40;
    }
  }
  if (r == 0) SIMMY_STUB("ObjectModuleImpl::Load: UpdateSimObjects, CheckIntegrity", 0x800F30BC);
  m_loaded = 1;
  return r;
}

// 800F5A98
// A new day: every object's DayPassed (its age, its price going down).
void ObjectModuleImpl::DayChanged() {
  std::vector<cXObjectImpl*> objects;
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) objects.push_back(o);
  for (cXObjectImpl* o : objects) o->DayPassed();
}

// 800DB9A8
// Its age, then (unless the definition says it keeps its value, +0x4A, or the simulator's global 0x29
// stops depreciation) its value (data 0x29) loses the daily depreciation (+0x46 while it is at its
// price, else +0x48) down to the limit (+0x4C). A multi-tile object's lead does it for the group, from
// the master definition.
void cXObjectImpl::DayPassed() {
  SIMMY_STUB("cXObjectImpl::DayPassed: UpdateAge (the object's age, data 0x2C)", 0x800DBB18);
  const ObjDefinition* def = m_def;
  if (IsMultiTile()) {
    auto* mt = dynamic_cast<cXMTObjectImpl*>(this);
    if (!mt || mt->GetFirstMultiTileObject() != mt) return;
    auto* master = m_selector ? reinterpret_cast<ObjSelector*>(m_selector->GetMasterSelector()) : nullptr;
    if (!master || !master->m_def) return;
    def = master->m_def;
  }
  if (!def || ERQuickdata::S16(def, 0x4a) != 0 || (Globs_pSimulator && Globs_pSimulator->GetGlobal(0x29) != 0)) return;
  const short value = m_data[0x29];
  short step = ERQuickdata::S16(def, 0x46);
  if (value != ERQuickdata::S16(def, 0x24) || step == 0) step = ERQuickdata::S16(def, 0x48);
  const int next = std::max(static_cast<int>(static_cast<short>(value - step)), static_cast<int>(ERQuickdata::S16(def, 0x4c)));
  m_data.WriteVar(0x29, static_cast<short>(next));
}

// 800F39DC
// After the house is read (and its family moved in): each object's PostLoad (its load tree), then the demolished objects and the
// kill queue go. `loaded`: the house comes from a save (passed to the load trees).
void ObjectModuleImpl::PostLoad(iResFile* file, int version, bool loaded) {
  if (version < 0) version = s_loadedVersion;  // PC: -1 the version Load read
  std::vector<cXObjectImpl*> objects;
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) objects.push_back(o);
  for (cXObjectImpl* o : objects) o->PostLoad(version, loaded);
  killDemolishedObjects(file);  // hoodmanager.cpp
  KillQueueFlush();
}

// 800DADBC
// Old saves' data fixed (version < 0x20: data 0x2A/0x2B flags; < 0x21: the price (data 0x29) from the
// master definition; < 0x23: data 4 = 1); then, for a house not loaded from a save, the load tree
// (entry 2) runs at once with param 0 = `loaded`; the sim flags are brought up to date. GC: also the
// level and location set again (SetLevel / SetLocation: the tile map) and auto-reset objects reset.
void cXObjectImpl::PostLoad(int version, bool loaded) {
  if (version < 0x20) {
    const int f = m_data[0x2a];
    if (!(f & 1)) m_data.WriteVar(0x2a, static_cast<short>((m_data[0x2a] & 2) ? (f & ~1) : (f | 3)));
    else m_data.WriteVar(0x2a, static_cast<short>(f & ~2));
    if (m_data[0x2a] & 0x80) m_data.WriteVar(0x2a, static_cast<short>(m_data[0x2a] & ~0x81));
    m_data.WriteVar(0x2b, static_cast<short>(m_data[0x2b] | 10));
  }
  if (version < 0x21) {
    auto* mt = IsMultiTile() ? dynamic_cast<cXMTObjectImpl*>(this) : nullptr;
    if (!mt || mt->GetFirstMultiTileObject() == mt) {
      auto* master = m_selector ? reinterpret_cast<ObjSelector*>(m_selector->GetMasterSelector()) : nullptr;
      if (master && master->m_def && m_data[0x29] < ERQuickdata::S16(master->m_def, 0x4c))
        m_data.WriteVar(0x29, ERQuickdata::S16(master->m_def, 0x24));
    }
  }
  if (version < 0x23) m_data.WriteVar(4, 1);
  // GC: first (before the data fixes) SetLevel(1) for an object without a level, then SetLocation at its
  // own location. PC: of SetLocation, the interaction influence map's part (the rest is done by the load).
  InfluenceMapObjectLeaving(this);
  InfluenceMapObjectArrived(this);
  SIMMY_STUB("cXObjectImpl::PostLoad: SetLevel / the rest of SetLocation again, auto-reset objects", 0x800DADBC);
  // GC: unless the house was loaded from a save (DAT_80475C74; PC: never).
  short params[4] = {static_cast<short>(loaded ? 1 : 0), 0, 0, 0};
  if (m_treeSim && GetTreeID(2)) RunTree(2, 0, params);
  UpdateSimFlags();
}

// 800F3B1C
// An object reference: its id (0 for none); read: the object with that id.
void ObjectModuleImpl::DoReconObject(ReconBuffer* b, cXObjectImpl** object) {
  short id = 0;
  if (b->m_mode != ReconBuffer__Mode::kRead) id = *object ? (*object)->m_id : 0;
  b->Recon16(&id, 1);
  if (b->m_mode != ReconBuffer__Mode::kRead) return;
  cXObject* o = GetObjectFromID(id);
  *object = o ? o->GetObjectImplementation() : nullptr;
}

// 800F3BF0
// A person reference, as an object reference (read: none unless it is a person).
void ObjectModuleImpl::DoReconPerson(ReconBuffer* b, cXPersonImpl** person) {
  cXObjectImpl* o = *person;
  DoReconObject(b, &o);
  if (b->m_mode == ReconBuffer__Mode::kRead) *person = dynamic_cast<cXPersonImpl*>(o);
}

// 800F3C94
// Read (versions after 0x2E): whether the stream is compressed; the objects' ids and types (0 ends);
// each object made and added; then, between marks, each object's own stream (what is not read is
// skipped to the next mark); then each object gets its 3D instance. GC: then the module's own data
// (the selected people, the object list, the last typed name) and placeholder objects are removed.
void ObjectModuleImpl::DoStream(ReconBuffer* b, int version) {
  if (b->m_mode != ReconBuffer__Mode::kRead) {
    SIMMY_STUB("ObjectModuleImpl::DoStream: writing", 0x800F3C94);
    return;
  }
  if (version <= 0x2e) {
    SIMMY_STUB("ObjectModuleImpl::DoStream: saves before version 0x2F", 0x800F3C94);
    return;
  }
  int compressed = 1;
  b->ReconBool(&compressed);
  if (compressed) b->EnableCompression();
  ObjSelector* placeholder = Globs_pObjectFolder->GetPlaceholder();
  short id = 0;
  b->Recon16(&id, 1);
  while (id != 0) {
    if (version <= 0x53) {
      int unused = 0;
      if (version > 0x40) b->Recon32(&unused, 1);
      if (version > 0x42) b->Recon32(&unused, 1);
      if (version > 0x43) {
        signed char n = 0;
        b->Recon8(&n, 1);
        for (int i = 0; i < static_cast<unsigned char>(n); ++i) b->Recon32(&unused, 1);
      }
    }
    ObjSelector* selector = nullptr;
    Globs_pObjectFolder->ReconSelector(&selector, b, true, version);
    if (selector && selector->m_def && objdef::Type(selector->m_def) == 9)
      selector = Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(kPlaceholderGUID));
    cXObject* object = selector ? ConstructObject(selector, nullptr) : nullptr;
    if (object) AddObject(object, id, true);
    b->Recon16(&id, 1);
  }
  b->ReconMark();
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) {
    const bool placeholderObject = o->GetSelector() == placeholder ||
                                   (o->m_def && objdef::GUID(o->m_def) == kPlaceholderGUID);
    o->ReconStream(b, version, placeholderObject);
    SIMMY_STUB("ObjectModuleImpl::DoStream: the object's behaviour reset and SetMiscFlag(0x80)", 0x800F3C94);
    // Its object map entry (sim flags << 16 | idle ticks; before 0x39 only the ticks, flag 1 on).
    if (version > 0x35 && o->m_id > 0) {
      if (m_simFlags.size() < static_cast<size_t>(o->m_id)) m_simFlags.resize(static_cast<size_t>(o->m_id), 0xffffu);
      int entry = static_cast<int>(m_simFlags[static_cast<size_t>(o->m_id) - 1]);
      b->ReconInt(&entry, 1);
      if (version < 0x39) entry = (entry & 0xffff) | 0x10000;
      m_simFlags[static_cast<size_t>(o->m_id) - 1] = static_cast<unsigned int>(entry);
    }
    // PC development aid: SIMMY_RECON_TRACE=1 reports an object whose stream was not read to its end
    // (the next mark) and prints each person's data as read.
    static const bool reconTrace = std::getenv("SIMMY_RECON_TRACE") != nullptr;
    if (reconTrace) {
      if (b->m_compressed) b->PadBits();
      if (b->m_pos != b->m_mark)
        std::fprintf(stderr, "[recon] object %d (%s, type %d): stream read to %d, its end at %d\n", o->m_id, o->DebugName(),
                     o->GetType(), b->m_pos, b->m_mark);
      if (auto* p = dynamic_cast<cXPersonImpl*>(o)) {
        std::fprintf(stderr, "[recon] person %d: neighbour %d, family %d, stack", o->m_id, p->m_personData[0x1f],
                     p->m_personData[0x3d]);
        for (int i = 0; o->m_treeSim && i < o->m_treeSim->GetStackSize(); ++i)
          std::fprintf(stderr, " %04x:%d", static_cast<unsigned short>(o->m_treeSim->GetNthElem(static_cast<short>(i))->GetTreeID()),
                       o->m_treeSim->GetNthElem(static_cast<short>(i))->m_node);
        std::fprintf(stderr, "; actions %u; object data 0x1d %d; data", p->m_actionQueue.Count(), o->GetData(0x1d));
        for (int i = 0; i < 0x50; ++i) std::fprintf(stderr, " %d", p->m_personData[i]);
        std::fprintf(stderr, "\n");
      }
    }
    b->ReadToNextMark();
    b->ReconMark();
  }
  // PC development aid: SIMMY_DUMP_WORLD=1 lists the objects read (id, GUID, type, tile, level, direction,
  // state, and data 11, which holds the id again when the stream was read in step).
  if (std::getenv("SIMMY_DUMP_WORLD"))
    for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) {
      const CTilePt t = o->GetCTilePt();
      std::fprintf(stderr, "OBJ %3d guid %08x type %2d tile %2d,%2d level %d dir %d state %d data11 %d\n", o->m_id,
                   o->m_def ? objdef::GUID(o->m_def) : 0u, o->m_def ? objdef::Type(o->m_def) : -1, t.GetX(), t.GetY(),
                   o->m_level, o->GetData(1), o->GetData(0), o->GetData(11));
      if (o->m_def && objdef::MasterID(o->m_def))
        std::fprintf(stderr, "    multi-tile master %d sub %04x interaction group %d\n", objdef::MasterID(o->m_def),
                     static_cast<unsigned short>(objdef::SubIndex(o->m_def)), ERQuickdata::S16(o->m_def, 0x10));
      if (o->m_attrCount) {
        std::fprintf(stderr, "    attrs");
        for (int i = 0; i < o->m_attrCount && i < 16; ++i) std::fprintf(stderr, " %d", o->m_attrs[i]);
        std::fprintf(stderr, "\n");
      }
    }
  // PC: the multi-tile parts are chained (GC: each part's stream names its lead, cXMTObjectImpl::ReconStream).
  LinkMultiTileGroups(this);
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) _globals.AllocInstance(o);
  // Each object's behaviour carries on from the stack its stream held (TreeStack::ReconStream); one
  // without a stack starts at its main tree. PC development aid: SIMMY_SAVED_STACKS=0 starts every
  // object at its main tree (the port before the stacks were read).
  static const char* savedStacks = std::getenv("SIMMY_SAVED_STACKS");
  const bool keep = !(savedStacks && savedStacks[0] == '0');
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) {
    if (keep && o->m_treeSim && o->m_treeSim->GetStackSize() > 0) o->ResumeBehavior();
    else o->StartBehavior();
  }
  SIMMY_STUB("ObjectModuleImpl::DoStream: the selected people, object lists, placeholders removed", 0x800F3C94);
  // The objects in the world take their places on the passive influence map (targets).
  if (PassiveInfluenceSystem* influence = Globs_pSimulator ? Globs_pSimulator->GetPassiveInfluenceSystem() : nullptr)
    for (cXObjectImpl* o = m_first; o; o = o->m_nextObject)
      if (ObjIsInWorld(o)) influence->MoveObjectInWorld(o);
}

// 800F8550
// The VM's accessed-variable notifications (a development aid): empty in the release build.
void ObjectModuleImpl::SkillAccessed(cXPerson*, int, bool) {}
// 800F8554
void ObjectModuleImpl::MotiveAccessed(cXPerson*, int, bool) {}
// 800F8558
void ObjectModuleImpl::PersonalityAccessed(cXPerson*, int, bool) {}
// 800F855C
void ObjectModuleImpl::RelationshipAccessed(cXObject*, cXObject*, int, bool) {}
// 800F8560
void ObjectModuleImpl::RelationshipAccessed(Neighbor*, Neighbor*, int, bool) {}

// 800F4A14
// GC: the module's people list. PC: the people by id; characters share the template person's selector,
// so a person whose neighbour has the GUID matches too.
cXPerson* ObjectModuleImpl::GetPersonByGUID(int guid) {
  for (int id = 1; id <= GetNumObjects(); ++id) {
    cXObject* o = GetObjectFromID(id);
    auto* p = o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
    if (!p) continue;
    if (p->GetSelector() && static_cast<int>(p->GetSelector()->GetGUID()) == guid) return p;
    if (p->m_neighbor && p->m_neighbor->GetGUID() == guid) return p;
  }
  return nullptr;
}

// 800F4AB0
// The object class for the selector's type: people (2), portals (8, multi-tile), multi-tile objects and
// plain ones; none for types 8 and 9 on their own. `lead`: the group a new part joins.
cXObject* ObjectModuleImpl::ConstructObject(ObjSelector* selector, cXMTObject* lead) {
  const ObjDefinition* def = selector->m_def;
  const short type = objdef::Type(def);
  auto* module = reinterpret_cast<ObjectModule*>(this);
  if (objdef::MasterID(def) == 0) {
    if (type == 2) return new cXPersonImpl(1, selector, module);  // GC: 0x5A8 bytes
    if (type >= 8 && type < 10) return nullptr;
    return new cXObjectImpl(1, selector, module);  // GC: 0x158 bytes
  }
  // PC: the lead is passed as its cXMTObjectImpl (GC: its cXMTObject interface).
  auto* leadPart = reinterpret_cast<cXMTObjectImpl*>(lead);
  if (type == 8)
    SIMMY_STUB("ObjectModuleImpl::ConstructObject: portals (cXPortalImpl; made as their cXMTObjectImpl part)", 0x800F4AB0);
  return new cXMTObjectImpl(1, selector, leadPart, module);  // GC: 0x188 bytes (portals 0x1A8)
}

// 800F719C
// The object under `id` (GC: 0 picks the first free one), at the end of the object list. 1: added.
// GC: also the simulator's lists (people, portals, the global objects), the object's init tree.
int ObjectModuleImpl::AddObject(cXObject* object, short id, bool) {
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  if (!impl) return 0;
  if (id == 0) {
    id = 1;
    while (static_cast<unsigned int>(id) <= m_byId.size() && m_byId[static_cast<unsigned int>(id) - 1]) ++id;
  }
  if (id < 1) return 0;
  if (m_byId.size() < static_cast<unsigned int>(id)) m_byId.resize(static_cast<unsigned int>(id), nullptr);
  m_byId[static_cast<unsigned int>(id) - 1] = object;
  impl->m_id = id;
  cXObjectImpl** link = &m_first;
  while (*link) link = &(*link)->m_nextObject;
  *link = impl;
  impl->m_nextObject = nullptr;
  SIMMY_STUB("ObjectModuleImpl::AddObject: the simulator's lists (people, portals, init trees)", 0x800F719C);
  return 1;
}

// 800F9290
// With a neighbourhood: in free play or author mode, or once the simulator has run 0x5B ticks.
int ObjectModuleImpl::IsSelectionSwitchingAllowed() {
  if (!Globs_pNeighborhood) return 0;
  if (_gd.m_fieldE8 || _gd.m_fieldF0) return 1;  // DAT_80475C6C, DAT_80475C74
  return Globs_pSimulator && Globs_pSimulator->GetTicks() > 0x5b ? 1 : 0;
}

// 800F9620
cXObject* ObjectModuleImpl::GetObjectFromID(int id) {
  if (id < 1 || static_cast<unsigned int>(id) > m_byId.size()) return nullptr;
  return m_byId[static_cast<unsigned int>(id) - 1];
}

// 800F9310
int ObjectModuleImpl::IsValidObject(cXObject* object) {
  if (!object) return 0;
  for (cXObject* o : m_byId)
    if (o == object) return 1;
  return 0;
}

// 800F9668
int ObjectModuleImpl::GetTileObjectID(CTilePt& tile) {
  const int x = tile.GetX(), y = tile.GetY();
  return x >= 0 && y >= 0 && x < 0x40 && y < 0x40 ? m_tileMap[x + y * 0x40] : 0;
}

// 800F96E4
void ObjectModuleImpl::SetTileObjectID(CTilePt& tile, short id) {
  const auto x = static_cast<unsigned int>(tile.GetX()), y = static_cast<unsigned int>(tile.GetY());
  if (x < 0x40 && y < 0x40) m_tileMap[x + y * 0x40] = id;
}

// PC: the object leaves its site in the slot hierarchy (TestAndPlace's pickup half, HierUnlink: the
// object chained under it takes the site; objectplace.cpp).
void ObjectModuleImpl::SeverTile(cXObjectImpl* object) { HierUnlink(object); }

// PC: a person the walker moved takes the tile it stands on (HierResite; GC: its per-frame Place).
void ObjectModuleImpl::SetSiteTile(cXObjectImpl* object) { HierResite(object); }

// ---------------------------------------------------------------------------------------------------
// ObjectIterator (game/objectiterator.cpp)

// 800F29F8
cXObject* ObjectIterator::init(CTilePt& tile, ObjectIterator__IterateType type) {
  cXObject* object = Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(Globs_pObjectModule->GetTileObjectID(tile))
                                         : nullptr;
  m_type = type;
  m_current = object;
  m_root = object;
  return object;
}

// 800F2A6C
ObjectIterator::ObjectIterator(cXObject* object, ObjectIterator__IterateType type) {
  m_current = object;
  m_type = type;
  m_root = object;
}

// 800F2A80
bool ObjectIterator::operator==(const ObjectIterator& other) const {
  return other.m_current == m_current && (!m_current || other.m_root == m_root);
}

// 800F2AC0
// The next object down the slot hierarchy, depth first: the current object's slot vector entries
// (HierGetChild: entry 0 the object chained under it at its tile, k + 1 object slot k's), else up to its
// site (HierarchySite: the object holding it) and on from the entry after its own; the end at the root.
ObjectIterator& ObjectIterator::operator++() {
  if (!m_current) return *this;
  cXObjectImpl* cur = m_current->GetObjectImplementation();
  int next = 0;
  for (int guard = 0; cur && guard < 256; ++guard) {  // PC guard against a looped hierarchy
    const int count = static_cast<int>(cur->m_slotContents.size());
    while (next < count) {
      cXObjectImpl* child = HierGetChild(cur, next++);
      if (child) {
        m_current = child;
        return *this;
      }
    }
    m_current = nullptr;
    if (cur == (m_root ? m_root->GetObjectImplementation() : nullptr)) return *this;
    HierSite site;
    HierGetSite(cur, site);
    next = site.slot + 1;
    cur = site.container;
  }
  m_current = nullptr;
  return *this;
}

// ---------------------------------------------------------------------------------------------------
// Objects made and killed by the simulation

// 800F4BD0
// A new object of `selector` out of the world: added under the next free id, its instance made, born (its
// main tree), data 0x29 the price, and Reset(true) runs its init tree at once. A multi-tile group's lead
// selector (sub index -1) makes every part (its sub selectors, in order; the first leads), then the group
// is reset (each part's init tree; the parts are all there by then); if one cannot be made, the group goes.
// GC: also the selector's file is loaded first, misc flag 0x80, the module's "new object" event (vtable
// 0x244) and GlobalDispatch 0xDE (only its people part: the visitor types); a lone part (sub index >= 0) is not made (PC: it is, for buy mode's
// CreateObjectFromGUID, which makes the parts one by one).
int ObjectModuleImpl::MakeNewOutOfWorldObject(ObjSelector* selector, short* id) {
  if (id) *id = 0;
  if (!selector || !selector->m_def) return 0;
  auto born = [this, selector](cXObjectImpl* impl) {
    impl->m_location[0] = impl->m_location[1] = -16;
    impl->m_level = 1;
    impl->m_data.WriteVar(0x1d, -5);  // its room: off the lot (GC SetLocation: ResolveRoomID 0xFFFB)
    impl->m_data.WriteVar(11, impl->m_id);
    _globals.AllocInstance(impl);
    impl->JustBorn();
    (void)selector;
  };
  if (objdef::MasterID(selector->m_def) != 0 && objdef::SubIndex(selector->m_def) == -1) {
    std::vector<cXMTObjectImpl*> parts;
    bool failed = false;
    ObjectFolderImpl* folder = Globs_pObjectFolder;
    for (int n = 0; folder; ++n) {
      auto* sub = reinterpret_cast<ObjSelector*>(folder->GetNthSubSelector(selector, n));
      if (!sub) break;
      if (!parts.empty() && parts[0]->m_selector == sub) continue;
      cXObject* object = ConstructObject(sub, parts.empty() ? nullptr : reinterpret_cast<cXMTObject*>(parts[0]));
      auto* part = object ? dynamic_cast<cXMTObjectImpl*>(object->GetObjectImplementation()) : nullptr;
      if (!part || !AddObject(object, 0, true)) {
        delete object;
        failed = true;
        break;
      }
      part->Initialize();
      born(part);
      parts.push_back(part);
    }
    if (failed || parts.empty()) {
      for (cXMTObjectImpl* p : parts) KillObject(static_cast<short>(p->m_id));  // GC: KillOutOfWorldObject
      return 0;
    }
    cXMTObjectImpl* lead = parts[0];
    lead->m_linked = true;
    lead->AssignOffsets();
    lead->m_data.WriteVar(0x29, ERQuickdata::S16(selector->m_def, 0x24));
    // Reset(true) through the last part (cXMTObjectImpl::Reset: the lead resets the group).
    for (cXMTObjectImpl* p = lead; p; p = p->m_nextPart)
      if (p->m_treeSim && p->m_treeSim->HasMainTreeSim()) p->RunTree(0, 0, nullptr);
    if (id) *id = lead->m_id;
    return lead->m_id;
  }
  cXObject* object = ConstructObject(selector, nullptr);
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  if (!impl || !AddObject(object, 0, true)) {
    delete object;
    return 0;
  }
  born(impl);
  impl->m_data.WriteVar(0x29, ERQuickdata::S16(selector->m_def, 0x24));
  // GC: Reset(true) (800DA848, through the cXObject interface) runs the init tree (entry 0) at once, so
  // what it sets (its attributes cleared, data) is in place before the creator's tree goes on.
  if (impl->m_treeSim && impl->m_treeSim->HasMainTreeSim()) impl->RunTree(0, 0, nullptr);
  if (id) *id = impl->m_id;
  // GlobalDispatch 0xDE: HouseImpl::DoCommand redoes the people's visitor types for a new person.
  HouseDispatch(0xde, impl->m_id);
  return impl->m_id;
}

// 800DAC94
// A new object starts its behaviour: the main tree (PC: GC's Reset resets the tree sim to it; its init
// tree is run at once by MakeNewOutOfWorldObject, as GC's Reset(true)).
void cXObjectImpl::JustBorn() {
  SIMMY_STUB("cXObjectImpl::JustBorn: data 0x2D/0x2E/0x2F/0x18/0x15 from the object's model", 0x800DAC94);
  StartBehavior();
}

// 800F8214
// Each person but `object` itself cleans up after it (cXPersonImpl::Cleanup; null: everything).
void ObjectModuleImpl::CleanupPeople(cXObject* object) {
  std::vector<cXPersonImpl*> people;
  for (int id = 1; id <= GetNumObjects(); ++id) {
    cXObject* o = GetObjectFromID(id);
    auto* p = o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
    if (p && (!object || static_cast<cXObject*>(p) != object)) people.push_back(p);
  }
  for (cXPersonImpl* p : people) p->Cleanup(object);
}

void ForgetLotPerson(cXObjectImpl* object);  // person.cpp

// 800F5668
// GC: also its room's lighting and impact (data 7), multi-tile parts, the house recalculated for
// structural objects, the people's routes and memories of it.
void ObjectModuleImpl::KillObject(short id) {
  cXObject* object = GetObjectFromID(id);
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  if (!impl) return;
  static int logged = 0;
  if (logged++ < 20) std::printf("[vm] %s killed\n", impl->DebugName());
  SeverTile(impl);
  // GC KillOutOfWorldObject (800F50FC): a player's selected person being killed hands the selection on
  // (AdvanceSelectedPerson, vtable 0x124); still the same: the player has none (SetSelectedPerson 0).
  // Before CleanupPeople. (PC: without it the HUD/interactors kept a freed person.)
  if (impl->GetType() == 2)
    for (int player = 0; player < 2; ++player) {
      cXPerson* selected = _globals.GetSelectedPerson(player);
      if (!selected || selected->AsObject() != object) continue;
      _globals.AdvanceSelectedPerson(player);
      selected = _globals.GetSelectedPerson(player);
      if (selected && selected->AsObject() == object) _globals.SetSelectedPerson(player, nullptr);
    }
  // People running its trees or doing actions with it let go (GC: KillOutOfWorldObject's CleanupPeople).
  CleanupPeople(object);
  for (int other = 1; other <= GetNumObjects(); ++other) {
    cXObject* o = GetObjectFromID(other);
    cXObjectImpl* oi = o ? o->GetObjectImplementation() : nullptr;
    if (!oi || oi == impl) continue;
    if (oi->m_data[2] == id) oi->m_data.WriteVar(2, 0);
    if (oi->m_data[0x1a] == id) {  // PC: its slots' objects lose their site (the slot vector goes)
      oi->m_data.WriteVar(0x1a, 0);
      oi->m_data.WriteVar(0xe, -1);
    }
    if (auto* person = oi->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(oi) : nullptr)
      for (auto& g : person->m_gosubObjects)
        if (g.m_object == impl) g.m_object = nullptr;
  }
  if (impl->m_instance && _globals.m_house && _globals.m_house->m_objectMan)
    _globals.m_house->m_objectMan->FreeSimsObjectInstance(impl->m_instance);
  impl->m_instance = nullptr;
  for (cXObjectImpl** link = &m_first; *link; link = &(*link)->m_nextObject)
    if (*link == impl) {
      *link = impl->m_nextObject;
      break;
    }
  m_byId[static_cast<unsigned int>(id) - 1] = nullptr;
  SetSimFlag(id, ObjectModule__SimFlag{1}, false);
  ForgetLotPerson(impl);  // PC: person.cpp's list must not free it again (RemoveFamilyFromLot)
  delete impl;
}

// 800F6690
// Killed at the end of the tick (KillQueueFlush), never at once: GC's `now` only clears misc flag 0x1000
// (SetMiscFlag(0x1000, !now)). Not when it, or a container it is in, is queued already. It leaves the
// world (and its container's slot) at once (Pickup, vtable 0x12C): a tree that kills what a slot holds
// and loops until the slot is empty ends. PC: a multi-tile object queues every part (GC: the lead).
// (The port used to kill a "now" kill at once: KillObject's CleanupPeople then reset the killer's own
// tree stack under the running primitive, a use after free.)
void ObjectModuleImpl::AddToKillQueue(short id, bool now) {
  cXObject* object = GetObjectFromID(id);
  if (!object) return;
  auto queued = [this](short i) { return std::find(m_killQueue.begin(), m_killQueue.end(), i) != m_killQueue.end(); };
  int depth = 0;
  for (cXObject* c = object->GetObjectImplementation()->GetContainer(); c && depth < 8; ++depth) {
    cXObjectImpl* ci = c->GetObjectImplementation();
    if (!ci) break;
    if (queued(static_cast<short>(ci->m_id))) return;
    c = ci->GetContainer();
  }
  std::vector<short> ids;
  if (auto* mt = dynamic_cast<cXMTObjectImpl*>(object->GetObjectImplementation())) {
    for (cXMTObjectImpl* p = mt->GetFirstMultiTileObject(); p; p = p->GetNextMultiTileObject()) ids.push_back(p->m_id);
  } else {
    ids.push_back(id);
  }
  for (short i : ids) {
    if (queued(i)) continue;
    if (cXObject* o = GetObjectFromID(i)) {
      cXObjectImpl* impl = o->GetObjectImplementation();
      ObjPickup(impl);
      impl->SetMiscFlag(cXObject__MiscFlag{0x1000}, !now);
    }
    m_killQueue.push_back(i);
  }
}

namespace {
// PC development aid: SIMMY_SAVE_SELFTEST=<tick> writes every object's stream (the lot save's
// cXObject::ReconStream, at the save version) at that simulator tick and reads it back into a fresh
// object of the same type, reporting streams not read to their end and fields that differ.
void SaveSelfTest(ObjectModuleImpl* m) {
  const int version = Globs_iSaveFileVersion;
  int objects = 0, bad = 0;
  // The save ids as ObjectFolderImpl::PrepareForModuleSave gives them (here over every object by id,
  // the people the PC family move-in made too; no type table is written).
  {
    std::map<const ObjSelector*, int> counts;
    for (int id = 1; id <= m->GetNumObjects(); ++id)
      if (cXObject* x = m->GetObjectFromID(id)) ++counts[x->GetSelector()];
    ObjectFolderImpl* folder = Globs_pObjectFolder;
    folder->m_saveGuids.assign(1, 0u);
    short next = 1;
    for (ObjSelector* chain : folder->m_selectors)
      for (ObjSelector* sel = chain; sel; sel = sel->m_next) {
        sel->m_saveId = counts.count(sel) ? next++ : 0;
        if (sel->m_saveId) folder->m_saveGuids.push_back(sel->GetGUID());
      }
  }
  for (int id = 1; id <= m->GetNumObjects(); ++id) {
    cXObject* x = m->GetObjectFromID(id);
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    if (!o || !o->GetSelector()) continue;
    ReconBuffer measure(nullptr, 0, ReconBuffer__Mode{ReconBuffer__Mode::kMeasure}, false);
    measure.EnableCompression();
    o->ReconStream(&measure, version, false);
    measure.PadBits();
    std::vector<unsigned char> data(static_cast<size_t>(measure.m_pos) + 16, 0);
    ReconBuffer out(data.data(), static_cast<int>(data.size()), ReconBuffer__Mode{ReconBuffer__Mode::kWrite}, false);
    out.EnableCompression();
    o->ReconStream(&out, version, false);
    out.PadBits();
    const int written = out.m_pos;
    cXObject* made = m->ConstructObject(o->GetSelector(), nullptr);
    cXObjectImpl* copy = made ? made->GetObjectImplementation() : nullptr;
    if (!copy) continue;
    ReconBuffer in(data.data(), static_cast<int>(data.size()), ReconBuffer__Mode{ReconBuffer__Mode::kRead}, false);
    in.EnableCompression();
    copy->ReconStream(&in, version, false);
    in.PadBits();
    if (auto* mt = dynamic_cast<cXMTObjectImpl*>(copy)) mt->RemoveFromChain();
    ++objects;
    std::string diff;
    if (in.m_pos != written) diff += " length " + std::to_string(in.m_pos) + "/" + std::to_string(written);
    for (int i = 0; i < 0x48; ++i)
      if (copy->GetData(i) != o->GetData(i)) diff += " data" + std::to_string(i);
    for (int i = 0; i < 8; ++i)
      if (copy->m_temps[i] != o->m_temps[i]) diff += " temp" + std::to_string(i);
    for (int i = 0; i < o->m_attrCount && i < copy->m_attrCount; ++i)
      if (copy->m_attrs[i] != o->m_attrs[i]) diff += " attr" + std::to_string(i);
    const int depth = o->m_treeSim ? o->m_treeSim->GetStackSize() : 0;
    if ((copy->m_treeSim ? copy->m_treeSim->GetStackSize() : 0) != depth) diff += " stack";
    for (int i = 0; i < depth && copy->m_treeSim && copy->m_treeSim->GetStackSize() == depth; ++i) {
      StackElem* a = o->m_treeSim->GetNthElem(static_cast<short>(i));
      StackElem* b = copy->m_treeSim->GetNthElem(static_cast<short>(i));
      if (a->GetTreeID() != b->GetTreeID() || a->m_node != b->m_node || a->m_primState != b->m_primState ||
          a->m_stackObject != b->m_stackObject)
        diff += " frame" + std::to_string(i);
      if (a->m_behavior != b->m_behavior) {
        ObjSelector* owner = a->m_behavior ? a->m_behavior->GetOwner() : nullptr;
        diff += " behaviour" + std::to_string(i) + "(" + (owner && owner->m_fileName ? owner->m_fileName : "?") +
                " save id " + std::to_string(owner ? owner->m_saveId : -1) + ")";
      }
    }
    auto* p = dynamic_cast<cXPersonImpl*>(o);
    auto* q = dynamic_cast<cXPersonImpl*>(copy);
    if (p && q) {
      for (int i = 0; i < 0x50; ++i)
        if (p->m_personData[i] != q->m_personData[i] && i != 0x3a && i != 0x41) diff += " pd" + std::to_string(i);
      for (int i = 0; i < Motives::kCount; ++i)
        if (p->m_motives.m_values[i] != q->m_motives.m_values[i]) diff += " motive" + std::to_string(i);
      if (p->m_actionQueue.Count() != q->m_actionQueue.Count()) diff += " queue";
      if (p->m_motiveDeltas.size() != q->m_motiveDeltas.size()) diff += " deltas";
      if (p->m_gosubObjects.size() != q->m_gosubObjects.size()) diff += " gosubs";
    }
    if (!diff.empty()) {
      ++bad;
      std::printf("[selftest] object %d (%s): %s\n", o->m_id, o->DebugName(), diff.c_str());
    }
    delete copy;
  }
  std::printf("[selftest] save stream (version 0x%x): %d objects written and read back, %d differ\n", version, objects, bad);
}
}  // namespace

// 800F587C
void ObjectModuleImpl::KillQueueFlush() {
  std::vector<short> queue;
  queue.swap(m_killQueue);
  for (short id : queue) KillObject(id);
  static const int selfTestTick = std::getenv("SIMMY_SAVE_SELFTEST") ? std::atoi(std::getenv("SIMMY_SAVE_SELFTEST")) : -1;
  if (selfTestTick >= 0 && Globs_pSimulator && Globs_pSimulator->GetTicks() >= selfTestTick) {
    static bool done = false;
    if (!done) {
      done = true;
      SaveSelfTest(this);
    }
  }
}

int ObjIsInWorld(cXObjectImpl* o);  // 800DCB50 (objectplace.cpp)

// 800F69BC
// A person on the lot whose object data 0x22 is clear, awake (cXPersonImpl::IsSleeping), in the world
// and selectable by a player. (The intro camera waits for one.) PC: the module's people are found by id
// (GC: its people list, +0x2058; the answer does not depend on the order).
RET_UNKNOWN ObjectModuleImpl::IsFamilyMemberAwakeAndVisible() {
  for (int id = 1; id <= GetNumObjects(); ++id) {
    cXObject* object = GetObjectFromID(static_cast<short>(id));
    auto* person = object ? dynamic_cast<cXPersonImpl*>(object->GetObjectImplementation()) : nullptr;
    if (!person) continue;
    if (person->m_data[0x22] == 0 && !person->IsSleeping() && ObjIsInWorld(person) && person->IsSelectableByPlayer(-1)) return 1;
  }
  return 0;
}
