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

void ForgetLotPerson(cXObjectImpl* object);  // person.cpp
void ReleasePendingCameraAnims();            // objectsim.cpp

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
// The players' pending camera animations released, then every object killed (KillObject: its people let
// go of it, a selected person hands the selection on, its instance freed, its tile and links cleared)
// while the module is killing all (+0x208C: KillObject neither recalculates the house nor tells the
// room); the id and sim-flag lists cleared, the marked sprites removed, the passive influences flushed.
void ObjectModuleImpl::KillAllObjects() {
  ReleasePendingCameraAnims();
  m_killingAll = 1;
  while (cXObjectImpl* first = m_first) {  // GetFirst (vtable 0x98)
    KillObject(static_cast<short>(first->GetID()));  // vtable 0x58
    if (m_first == first) {
      // PC guard: an object KillObject cannot find by its id (not expected) is unlinked and freed here.
      m_first = first->m_nextObject;
      ForgetLotPerson(first);
      delete first;
    }
  }
  m_killingAll = 0;
  m_byId.clear();
  m_simFlags.clear();
  // PC: the lot's people were in the module, so KillObject freed them; this only clears the family-on-lot
  // flag and the players' selected persons (person.cpp).
  RemoveFamilyFromLot();
  ESpriteRenderMan::Instance()->RemoveMarkedSprites();  // DAT_80475ED8 (PC: made on first use)
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
  if (r == 0) {
    UpdateSimObjects();
    // GC: then CheckIntegrity (800F7F28, vtable 0xB4), which only reports objects whose slot hierarchy
    // and tile disagree to the debug dump (CTGDump "Error detected"); it repairs nothing (PC: not done).
  }
  m_loaded = 1;
  return r;
}

// 800F62DC
// The lot's simulation objects (type 7 definitions with +0x54 set: the controllers) are there once each:
// in the folder's selector order, one missing is made (MakeNewOutOfWorldObject), a second one of the
// same type killed. GC: also remembers the id of the one of GUID 0x3EEC206C (+0x2054; read only by the
// animation preview, PreviewAnimation: not kept).
void ObjectModuleImpl::UpdateSimObjects() {
  std::vector<cXObjectImpl*> simObjects;
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject)
    if (o->m_def && ERQuickdata::S16(o->m_def, 0x54) != 0) simObjects.push_back(o);
  ObjectFolderImpl* folder = Globs_pObjectFolder;
  for (ObjSelector* s = folder ? folder->GetNextSelector(nullptr) : nullptr; s; s = folder->GetNextSelector(s)) {
    if (!s->m_def || objdef::Type(s->m_def) != 7 || ERQuickdata::S16(s->m_def, 0x54) == 0) continue;
    short id = 0;
    for (size_t i = 0; i < simObjects.size();) {
      if (simObjects[i]->GetSelector() != s) {
        ++i;
        continue;
      }
      if (id == 0) {
        id = static_cast<short>(simObjects[i]->GetID());
        ++i;
        continue;
      }
      const short extra = static_cast<short>(simObjects[i]->GetID());
      simObjects.erase(simObjects.begin() + static_cast<std::ptrdiff_t>(i));
      std::printf("[vm] UpdateSimObjects: a second %s (#%d) goes\n", s->m_fileName ? s->m_fileName : "?", extra);
      KillObject(extra);  // vtable 0x5C
    }
    if (id == 0) {
      MakeNewOutOfWorldObject(s, &id);  // vtable 0x54
      std::printf("[vm] UpdateSimObjects: %s made (#%d)\n", s->m_fileName ? s->m_fileName : "?", id);
    }
  }
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
  UpdateAge();
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
// master definition; < 0x23: data 4 = 1); a placeholder cleaned up; then, for a house not loaded from
// a save, the load tree (entry 2) runs at once with param 0 = `loaded`; the sim flags are brought up to
// date. GC: also the location set again (SetLocation: the tile map; PC: the load does it).
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
  if (m_level == 0) SetLevel(1);
  InfluenceMapObjectLeaving(this);
  InfluenceMapObjectArrived(this);
  // A placeholder (the folder's) is cleaned up: the people let go of it (CleanupPeople, vtable 0x134) and
  // Cleanup(nullptr). (So are auto-reset objects, definition +0x7C flag 2, when cXObjectImpl::sAutoReset
  // is set, with their init tree run: it is zero-initialised and nothing sets it in the release game.)
  if (Globs_pObjectFolder && m_selector && m_selector == Globs_pObjectFolder->GetPlaceholder()) {
    if (Globs_pObjectModule) Globs_pObjectModule->CleanupPeople(this);
    Cleanup(nullptr);
  }
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
    // Read: the object is initialised first (the cXObjectImpl vtable's Initialize, 0x24), so a data
    // variable the save does not hold keeps its default (GC saves only an object's own variables: one
    // equal to its category's value is not written); misc flag 0x80 after. PC: only the data defaults
    // (EdithVariableSet::Initialize; the slots and tree sim are the constructor's, a person's own
    // Initialize runs when its family moves in).
    const bool reading = b->m_mode == ReconBuffer__Mode::kRead;
    if (reading) o->m_data.Initialize(o);
    o->ReconStream(b, version, placeholderObject);
    if (reading) o->SetMiscFlag(cXObject__MiscFlag{0x80}, true);
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
  SIMMY_STUB("ObjectModuleImpl::DoStream: the selected people, object lists", 0x800F3C94);
  // The placeholders (objects of a type the folder does not have, GUID 0x3C08965F) go (vtable 0x5C),
  // the list walked again from its start after each.
  if (b->m_mode == ReconBuffer__Mode::kRead)
    for (cXObjectImpl* o = m_first; o;) {
      if (o->m_def && objdef::GUID(o->m_def) == kPlaceholderGUID) {
        KillObject(static_cast<short>(o->m_id));
        o = m_first;
      } else {
        o = o->m_nextObject;
      }
    }
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
// purchase date), misc flag 0x80, Reset(true) (its init tree and a tick of its main tree at once), then
// data 0x29 the price. A multi-tile group's lead selector (sub index -1) makes every part (its sub
// selectors, in order; the first leads), the lead gets the price, then the group is reset (each part's
// init tree; the parts are all there by then); if one cannot be made, the group goes. GC: also the
// selector's file is loaded first, the module's sim flag 1 (vtable 0x244; PC: StartBehavior) and
// GlobalDispatch 0xDE (only its people part: the visitor types); a lone part (sub index >= 0) is not made
// (PC: it is, for buy mode's CreateObjectFromGUID, which makes the parts one by one).
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
    lead->AssignOffsets();  // PC: the chain's offsets before the parts are used (Reset turns them again)
    lead->m_data.WriteVar(0x29, ERQuickdata::S16(selector->m_def, 0x24));
    // Reset(true) through the last part (cXMTObjectImpl::Reset 800C6B10: the lead resets the group,
    // each part's init tree and a tick of its main tree).
    parts.back()->Reset(true);
    for (cXMTObjectImpl* p = lead; p; p = p->m_nextPart) p->SetMiscFlag(cXObject__MiscFlag{0x80}, true);
    if (id) *id = lead->m_id;
    return lead->m_id;
  }
  cXObject* object = ConstructObject(selector, nullptr);
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  if (!impl || !AddObject(object, 0, true)) {
    delete object;
    return 0;
  }
  // GC: Initialize through the interface; PC: the object part only (a person's own Initialize, the
  // neighbour and its instance, is not run here).
  impl->cXObjectImpl::Initialize();
  born(impl);
  impl->SetMiscFlag(cXObject__MiscFlag{0x80}, true);
  // Reset(true) (through the interface: cXObjectImpl::Reset 800DA848, a person's 8011EB78): its init
  // tree (entry 0) and a tick of its main tree at once, so what they set is in place before the
  // creator's tree goes on; then its value (data 0x29) the price.
  impl->Reset(true);
  impl->m_data.WriteVar(0x29, ERQuickdata::S16(selector->m_def, 0x24));
  if (id) *id = impl->m_id;
  // GlobalDispatch 0xDE: HouseImpl::DoCommand redoes the people's visitor types for a new person.
  HouseDispatch(0xde, impl->m_id);
  return impl->m_id;
}

// 800DAC94
// A new object's purchase date. PC: it also starts its behaviour (the tree sim on the main tree, the sim
// flag on); GC's Reset(true), which MakeNewOutOfWorldObject calls next, resets the tree sim the same way.
void cXObjectImpl::JustBorn() {
  // The purchase date and time from the simulator's clock (GetSim, vtable 0x3DC; GetGlobal): data 0x2D
  // the year, 0x2E the month, 0x2F the day, 0x18 the hour, 0x15 the minute.
  if (cSimulatorImpl* sim = Globs_pSimulator) {
    m_data.WriteVar(0x2d, static_cast<short>(sim->GetGlobal(8)));
    m_data.WriteVar(0x2e, static_cast<short>(sim->GetGlobal(7)));
    m_data.WriteVar(0x2f, static_cast<short>(sim->GetGlobal(1)));
    m_data.WriteVar(0x18, static_cast<short>(sim->GetGlobal(0)));
    m_data.WriteVar(0x15, static_cast<short>(sim->GetGlobal(5)));
  }
  StartBehavior();  // PC
}

// 800DBB18
// Its age from its purchase date (data 0x2D year, 0x2E month, 0x2F day) to the simulator's date (GetSim,
// vtable 0x3DC; globals 8, 7, 1): data 0x12 the whole years, 0x30 the whole months (16-bit sums).
void cXObjectImpl::UpdateAge() {
  cSimulatorImpl* sim = Globs_pSimulator;
  if (!sim) return;  // PC guard
  short years = static_cast<short>(sim->GetGlobal(8) - m_data[0x2d]);
  short months = static_cast<short>(sim->GetGlobal(7) - m_data[0x2e]);
  if (static_cast<short>(sim->GetGlobal(1) - m_data[0x2f]) < 0) months = static_cast<short>(months - 1);
  if (months < 0) {
    months = static_cast<short>(months + 12);
    years = static_cast<short>(years - 1);
  }
  m_data.WriteVar(0x12, years);
  m_data.WriteVar(0x30, static_cast<short>(months + years * 12));
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


// 800F5668
// GC: also its room's lighting and impact (data 7), multi-tile parts, the house recalculated for
// structural objects, the people's routes and memories of it.
void ObjectModuleImpl::KillObject(short id) {
  cXObject* object = GetObjectFromID(id);
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  if (!impl) return;
  static int logged = 0;
  if (!m_killingAll && logged++ < 20) std::printf("[vm] %s killed\n", impl->DebugName());
  ObjPortalPartKilled(impl);  // GC: Pickup (vtable 0x12C): a door's or window's wall back to plain
  // GC: Pickup (cXMTObjectImpl::Pickup 800C63E4): a dynamic part leaves its group first
  // (RemoveFromDynamic: the rest and it learn their neighbours). PC: GC's Pickup then picks up the parts
  // left in its old lead's chain too; not done (the other parts stay where they are).
  if (impl->IsMultiTile())
    if (auto* mt = dynamic_cast<cXMTObjectImpl*>(impl))
      if (mt->IsDynamic()) mt->RemoveFromDynamic();
  SeverTile(impl);
  // GC KillOutOfWorldObject (800F50FC): a player's selected person being killed hands the selection on
  // (AdvanceSelectedPerson, vtable 0x124); still the same: the player has none (SetSelectedPerson 0).
  // Before CleanupPeople. (PC: without it the HUD/interactors kept a freed person.)
  if (impl->GetType() == 2)
    for (int player = 0; player < kMaxPlayers; ++player) {  // PC mod: 4 players (GC: 2)
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
    if (auto* person = oi->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(oi) : nullptr) {
      for (auto& g : person->m_gosubObjects)
        if (g.m_object == impl) g.m_object = nullptr;
      // GC ~cXPersonImpl (8011C77C): every person aware of a dying person forgets it
      // (SAnimator2::GetAwareOfObject, ClearAwareOfObject: only an awareness a tree or the other sim set).
      if (impl->GetType() == 2 && person->m_animator && person->m_animator->GetAwareOfObject() == object)
        person->m_animator->ClearAwareOfObject(object);
    }
  }
  // GC ~cXPersonImpl (8011C77C) deletes a person's animator (+0x3F4) before its instance
  // (EGlobal::DestroyInstance): the animator's awareness manager ends its reaction on the ESim's
  // animation (MemoryAwarenessClearAction) and its props and particles hang on the ESim's model. PC: the
  // instance goes here, before the person is deleted, so the animator goes first here too (it read and
  // could write the freed ESim on every visitor or service NPC leaving the lot).
  if (impl->GetType() == 2)
    if (auto* person = dynamic_cast<cXPersonImpl*>(impl)) {
      delete person->m_animator;
      person->m_animator = nullptr;
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

// 800F69B0
// The kill queue emptied (its objects are not killed). GC: from ELiveMode::Reset.
void ObjectModuleImpl::ClearKillQueue() { m_killQueue.clear(); }

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
