// Port of game/hoodmanager.cpp (the free-play lots: moving a family in, evicting it, bulldozing) and
// game/houserecon.cpp (HouseRecon: a house's object types read without loading the house, the types
// to remove written as its 'DUMP' resource), plus ObjectModuleImpl::killDemolishedObjects (game/
// objectmodule.cpp), which applies that 'DUMP' when the house next loads.
//
// Also HoodManager::ResetToPristine / ResetNeighbors (the memory card's pristine reset, with the lot's
// objects reset: ResetLotObjectsToPristine).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/placement.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

extern int g_freeBuildBuyCheat;  // DAT_80476188 (pcttarget.cpp)

namespace {
constexpr int kSimulatorResType = 0x53494d49;   // 'SIMI' (kSimulatorResType)
constexpr short kSimulatorResourceID = 1;       // kSimulatorResourceID
constexpr int kObjectTypeTableResType = 0x6f626a74;  // 'objt'
constexpr int kObjectModuleResType = 0x4f626a4d;     // 'ObjM'
constexpr int kDumpResType = 0x44554d50;             // 'DUMP'

// PC: the DUMP resource's words, as the save's other resources, are little-endian.
unsigned int ReadLE32(const unsigned char* p) {
  return static_cast<unsigned int>(p[0]) | static_cast<unsigned int>(p[1]) << 8 | static_cast<unsigned int>(p[2]) << 16 |
         static_cast<unsigned int>(p[3]) << 24;
}
void WriteLE32(unsigned char* p, unsigned int v) {
  p[0] = static_cast<unsigned char>(v);
  p[1] = static_cast<unsigned char>(v >> 8);
  p[2] = static_cast<unsigned char>(v >> 16);
  p[3] = static_cast<unsigned char>(v >> 24);
}

// The type (object definition +0x12) and GUID of an entry's selector (0/-1 when it has none). PC: a
// user selector's GUID is its own (ObjSelector::GetGUID), its definition the template person's.
short EntryType(const HouseRecon::Entry& e) {
  return e.m_selector && e.m_selector->m_def ? objdef::Type(e.m_selector->m_def) : static_cast<short>(-1);
}
int EntryGUID(const HouseRecon::Entry& e) { return e.m_selector ? static_cast<int>(e.m_selector->GetGUID()) : 0; }

// PC: the simulator and the world the HoodManager functions use (GC: Globs::pSimulator and
// Globs::pFixedWorld, which always exist on GC). The port makes them with the first house, so in the
// neighbourhood screen before any lot was played a temporary one stands in.
struct TempSimulator {
  cSimulatorImpl* m_sim;
  bool m_own;
  TempSimulator() : m_sim(Globs_pSimulator), m_own(!Globs_pSimulator) {
    if (m_own) m_sim = cSimulator::CreateInstance();
  }
  ~TempSimulator() {
    if (m_own) cSimulator::DestroyInstance(m_sim);
  }
};
struct TempWorld {
  cFixedWorldImpl* m_world;
  bool m_own;
  TempWorld() : m_world(Globs_pFixedWorld), m_own(!Globs_pFixedWorld) {
    if (m_own) m_world = new cFixedWorldImpl(0x27, 0x27);
  }
  ~TempWorld() {
    if (m_own) delete m_world;
  }
};
}  // namespace

// ---------------------------------------------------------------------------------------------------
// hoodmanager.cpp: helpers

// 80042C44
// The objects a lot keeps when its family goes or it is cleared (the mailbox, the phone, the trash
// can, ... by GUID): 0 for those.
RET_UNKNOWN GuidIsOk(int guid) {
  static const unsigned int kKept[] = {0x1cd89442u, 0xbf1c5efcu, 0x8963d7c1u, 0x81e6bef9u, 0x98301c41u, 0x9e12bf3cu,
                                       0xced26913u, 0xbf6cb56au, 0xc4be2870u, 0xe25246d2u, 0x13470f00u, 0x4de114eeu,
                                       0x3dbf6753u, 0x3a6a0428u, 0x3fd182d0u, 0x4a0c562fu, 0x6660026fu, 0x6acaf6adu,
                                       0x70f69082u, 0x50907e06u, 0x61b89983u};
  for (unsigned int k : kKept)
    if (static_cast<unsigned int>(guid) == k) return 0;
  return 1;
}

// 80042DAC
// The type-4 objects an evicted family takes with it.
bool DeleteSelectorOnEvict(int guid) {
  static const unsigned int kTaken[] = {0x24c95f99u, 0xc5f45aa3u, 0x39e377cfu, 0xffecf6b5u,
                                        0x9087e0a2u, 0x4740f763u, 0xa4e8b034u, 0x8b519fa6u};
  for (unsigned int k : kTaken)
    if (static_cast<unsigned int>(guid) == k) return true;
  return false;
}

// 80042E44
// The family of lot `lot` (0-based) leaves it with `value` more simoleons; the neighbourhood is saved.
void AddFamilyToPool(int lot, int value) {
  NeighborhoodImpl* hood = Globs_pNeighborhood->GetImpl();
  FamilyImpl* family = hood->GetFamilyInHouse(lot + 1);
  if (!family) return;
  family->SetFunds(family->GetFunds() + value);
  family->SetHouseNumber(0);
  family->SetHouseValue(0);
  Globs_pNeighborhood->GetImpl()->Save(Globs_pNghResFile, Globs_iSaveFileVersion);
  std::printf("[hood] family %d leaves house %d with %d more: funds %d\n", family->GetNumber(), lot + 1, value,
              family->GetFunds());
}

// ---------------------------------------------------------------------------------------------------
// HoodManager

// 80042F5C
// What the family would have left after paying for lot `lot` (0-based); -1: it cannot pay (unless the
// free build/buy cheat is on: then 0).
RET_UNKNOWN HoodManager::TestMoveInFamily(int lot, Family* family) {
  int funds = static_cast<FamilyImpl*>(family)->GetFunds();
  HouseInfo info;
  info.m_family = -1;
  NeighborhoodImpl* hood = Globs_pNeighborhood->GetImpl();
  hood->m_houseNumber = lot + 1;  // GC: NeighborhoodImpl +0x70
  hood->GetHouseInfo(Globs_pNghResFile, lot + 1, &info);
  funds -= info.m_price;
  if (funds < 0 && g_freeBuildBuyCheat) funds = 0;
  return funds < 0 ? -1 : funds;
}

// 80043034
// The family moves into lot `lot` (0-based) if it can pay: its funds are what is left, it is saved,
// and its own sims' objects on the lot are kept. 1: moved in. (GC: static.)
RET_UNKNOWN HoodManager::MoveInFamily(int lot, Family* family, bool) {
  const int left = static_cast<int>(TestMoveInFamily(lot, family));
  if (left < 0) return 0;
  auto* f = static_cast<FamilyImpl*>(family);
  if (Globs_pNeighborhood->GetImpl()->MoveIn(f, lot + 1) != 0) return 0;
  f->SetFunds(left);
  f->SaveFamily(Globs_pNghResFile, Globs_iSaveFileVersion);
  UnmarkFamilyForDeletion(lot, family);
  std::printf("[hood] family %d moves into house %d: funds %d\n", f->GetNumber(), lot + 1, left);
  return 1;
}

// 800430F4
// The family's own sims (types that are its members) are no longer to go from lot `lot`.
void HoodManager::UnmarkFamilyForDeletion(int lot, Family* family) {
  const unsigned int house = static_cast<unsigned int>(lot + 1);
  NghResFile* file = Globs_pNghResFile;
  file->LoadHouseContents(house, nullptr, false);
  file->SetCurrentHouse(house);
  auto* recon = new HouseRecon;  // GC: 0x16008 bytes
  recon->LoadHouseData(file);
  file->SetCurrentHouse(house);
  auto* f = static_cast<FamilyImpl*>(family);
  for (HouseRecon::Entry& e : recon->m_entries)
    if (e.m_selector && e.m_selector->m_def && e.m_dump && f->TestMember(EntryGUID(e))) e.m_dump = 0;
  recon->SaveHouseData(file, Globs_iSaveFileVersion);
  file->SaveHouseContents(house, nullptr);
  file->FlushHouseContents(house);
  delete recon;
}

// 80043230
// Evicting the family of lot `lot` (0-based): its lot is cleared of everything but the fixed objects
// (RemoveAllObjects), the lot's objects value goes to 0 and the family takes the lot's price with it
// (AddFamilyToPool). In author mode (DAT_80475C74) only the family's own things go (EvictFamily).
void HoodManager::EvictFamilyAndLiquidateAssets(int lot) {
  if (_gd.m_fieldF0) {
    EvictFamily(lot, true);
    return;
  }
  const unsigned int house = static_cast<unsigned int>(lot + 1);
  NghResFile* file = Globs_pNghResFile;
  file->LoadHouseContents(house, nullptr, false);
  file->SetCurrentHouse(static_cast<unsigned int>(lot));  // (as GC: GetHouseInfo sets the house)
  HouseInfo info;
  info.m_family = -1;
  NeighborhoodImpl* hood = Globs_pNeighborhood->GetImpl();
  hood->m_houseNumber = static_cast<int>(house);
  hood->GetHouseInfo(file, static_cast<int>(house), &info);
  const int value = info.m_price;
  RemoveAllObjects(lot);
  file->SaveHouseContents(house, nullptr);
  file->FlushHouseContents(house);
  file->SetCurrentHouse(house);
  TempSimulator sim;
  int version = 0;
  ReconLoadObject(sim.m_sim, file, kSimulatorResType, kSimulatorResourceID, &version);
  sim.m_sim->SetObjectsValue(0);
  ReconSaveObject(sim.m_sim, file, kSimulatorResType, kSimulatorResourceID, version);
  AddFamilyToPool(lot, value);
}

// 80043388
// (Author mode) The family's sims (types 1, 2 and 0x22 but the fixed ones) and its type-4 belongings
// go from lot `lot` (0-based); with `liquidate` the family takes the lot's price with it.
void HoodManager::EvictFamily(int lot, bool liquidate) {
  const unsigned int house = static_cast<unsigned int>(lot + 1);
  NghResFile* file = Globs_pNghResFile;
  file->LoadHouseContents(house, nullptr, false);
  file->SetCurrentHouse(house);
  auto* recon = new HouseRecon;
  recon->LoadHouseData(file);
  file->SetCurrentHouse(house);
  int value = 0;
  if (liquidate) {
    HouseInfo info;
    info.m_family = -1;
    NeighborhoodImpl* hood = Globs_pNeighborhood->GetImpl();
    hood->m_houseNumber = static_cast<int>(house);
    hood->GetHouseInfo(file, static_cast<int>(house), &info);
    value = info.m_price;
  }
  for (HouseRecon::Entry& e : recon->m_entries) {
    if (!e.m_selector || !e.m_selector->m_def) continue;
    const short type = EntryType(e);
    if (type == 4) {
      if (DeleteSelectorOnEvict(EntryGUID(e))) e.m_dump = 1;
    } else if ((type > 0 && type < 3) || type == 0x22) {
      if (GuidIsOk(EntryGUID(e))) e.m_dump = 1;
    }
  }
  recon->SaveHouseData(file, Globs_iSaveFileVersion);
  file->SaveHouseContents(house, nullptr);
  file->FlushHouseContents(house);
  AddFamilyToPool(lot, value);
  delete recon;
}

// 80043590
// Everything movable (types 1..7, 9, 0xF, 0x22, but the fixed objects GuidIsOk names) is to go from lot
// `lot` (0-based).
void HoodManager::RemoveAllObjects(int lot) {
  NghResFile* file = Globs_pNghResFile;
  file->SetCurrentHouse(static_cast<unsigned int>(lot + 1));
  auto* recon = new HouseRecon;
  recon->LoadHouseData(file);
  int marked = 0;
  for (HouseRecon::Entry& e : recon->m_entries) {
    if (!e.m_selector || !e.m_selector->m_def) continue;
    const short type = EntryType(e);
    if ((type > 0 && type < 8) || type == 9 || type == 0xf || type == 0x22)
      if (GuidIsOk(EntryGUID(e))) e.m_dump = 1, ++marked;
  }
  recon->SaveHouseData(file, Globs_iSaveFileVersion);
  std::printf("[hood] house %d: %d of %d object types to go\n", lot + 1, marked, static_cast<int>(recon->m_entries.size()));
  delete recon;
}

// 800436AC
// Bulldozing lot `lot` (0-based): every object type but the fixed ones is to go (types 0..9, 0xF,
// 0x22), the ground floor's floors and walls are cleared (tiles but the locked ones, flag 0x20, inside
// the lot's edge), the lot is worth nothing, and the world and the type list are saved.
void HoodManager::DemolishCurrentHouse(int lot) {
  const unsigned int house = static_cast<unsigned int>(lot + 1);
  NghResFile* file = Globs_pNghResFile;
  file->LoadHouseContents(house, nullptr, false);
  file->SetCurrentHouse(house);
  auto* recon = new HouseRecon;
  recon->LoadHouseData(file);
  file->SetCurrentHouse(house);
  for (HouseRecon::Entry& e : recon->m_entries) {
    if (!e.m_selector || !e.m_selector->m_def) continue;
    const short type = EntryType(e);
    if ((type >= 0 && type < 10) || type == 0xf || type == 0x22)
      if (GuidIsOk(EntryGUID(e))) e.m_dump = 1;
  }
  TileWalls none;
  TempWorld world;
  TempSimulator sim;
  int version = 0;
  ReconLoadObject(sim.m_sim, file, kSimulatorResType, kSimulatorResourceID, &version);
  world.m_world->Load(file, version);
  int cleared = 0;
  for (int level = 1; level < 2; ++level)
    for (int y = 1; y < world.m_world->GetYSize() - 1; ++y)
      for (int x = 1; x < world.m_world->GetXSize() - 1; ++x) {
        CTilePt tile(x, y, level);
        if (world.m_world->GetFlags(tile) & 0x20) continue;
        world.m_world->SetFloor(tile, 0);
        TileWalls walls(none);
        world.m_world->SetWall(tile, walls);
        ++cleared;
      }
  sim.m_sim->SetObjectsValue(0);
  sim.m_sim->SetArchValue(0);
  ReconSaveObject(sim.m_sim, file, kSimulatorResType, kSimulatorResourceID, version);
  world.m_world->Save(file, Globs_iSaveFileVersion);
  recon->SaveHouseData(file, Globs_iSaveFileVersion);
  file->SaveHouseContents(house, nullptr);
  file->FlushHouseContents(house);
  int marked = 0;
  for (const HouseRecon::Entry& e : recon->m_entries) marked += e.m_dump != 0;
  std::printf("[hood] house %u bulldozed: %d tiles cleared, %d of %d object types to go\n", house, cleared, marked,
              static_cast<int>(recon->m_entries.size()));
  delete recon;
}

// ---------------------------------------------------------------------------------------------------
// houserecon.cpp

// 800451D0
HouseRecon::HouseRecon() {
  m_ids.reserve(kMaxIds);
  m_entries.reserve(kMaxEntries);
}

// 80045234
HouseRecon::~HouseRecon() {}

// 800452A4
int HouseRecon::findHRSelector(int guid) {
  for (size_t i = 0; i < m_entries.size(); ++i)
    if (m_entries[i].m_guid == guid) return static_cast<int>(i);
  return -1;
}

// 800452E8
void HouseRecon::LoadHouseData(iResFile* file) {
  ObjectSaveTypeTable3 types;
  types.m_recon = this;
  ReconLoadObject(&types, file, kObjectTypeTableResType, 0, nullptr);
  ObjectSaveIDTable ids;
  ids.m_recon = this;
  ReconLoadObject(&ids, file, kObjectModuleResType, 1, nullptr);
  Memory__HandleNode* dump = file->GetByID(kDumpResType, 1, nullptr);
  const unsigned int size = dump ? dump->m_size : 0;
  for (unsigned int i = 0; dump && dump->m_data && i < size / 4; ++i) {
    const int index = findHRSelector(static_cast<int>(ReadLE32(dump->m_data + i * 4)));
    if (index >= 0) m_entries[static_cast<size_t>(index)].m_dump = 1;
  }
}

// 800453EC
void HouseRecon::SaveHouseData(iResFile* file, int) {
  int count = 0;
  for (const Entry& e : m_entries)
    if (e.m_dump) ++count;
  Memory__HandleNode* handle = nullptr;
  if (count) {
    handle = new Memory__HandleNode;  // GC: 0x0C bytes from the main heap, the data after it
    handle->m_size = static_cast<unsigned int>(count * 4);
    handle->m_data = new unsigned char[handle->m_size];
    handle->m_owned = 1;
    unsigned char* p = handle->m_data;
    for (const Entry& e : m_entries)
      if (e.m_dump) WriteLE32(p, static_cast<unsigned int>(e.m_guid)), p += 4;
  }
  StringBuffer name;
  file->Add(handle, kDumpResType, 1, name, false);
}

// 80044E60
// Read: the house's types (GUID, the tree versions from version 1, the save id, the type from version 2
// (else -1), the name) until a zero GUID, each with its selector. Written: the types not marked.
void ObjectSaveTypeTable3::DoStream(ReconBuffer* b, int version) {
  std::vector<HouseRecon::Entry>& entries = m_recon->m_entries;
  if (b->m_mode != ReconBuffer__Mode::kRead) {
    for (HouseRecon::Entry& e : entries) {
      if (e.m_dump) continue;
      b->Recon32(&e.m_guid, 1);
      if (version > 0) {
        b->Recon32(&e.m_initTree, 1);
        b->Recon32(&e.m_mainTree, 1);
      }
      b->Recon16(&e.m_saveId, 1);
      if (version >= 2) b->Recon16(&e.m_type, 1);
      b->ReconString(e.m_name);
    }
    int end = 0;
    b->Recon32(&end, 1);
    return;
  }
  entries.clear();
  for (;;) {
    HouseRecon::Entry e;
    b->Recon32(&e.m_guid, 1);
    if (e.m_guid == 0 || b->m_pos > b->m_size) return;
    if (version > 0) {
      b->Recon32(&e.m_initTree, 1);
      b->Recon32(&e.m_mainTree, 1);
    }
    b->Recon16(&e.m_saveId, 1);
    if (version < 2) e.m_type = -1;
    else b->Recon16(&e.m_type, 1);
    b->ReconString(e.m_name);
    e.m_selector = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(e.m_guid) : nullptr;
    e.m_dump = 0;
    if (entries.size() >= HouseRecon::kMaxEntries) return;
    entries.push_back(e);
  }
}

// 80045084
// Whether the stream is compressed, then the objects' ids and type (save) ids, until a zero id.
void ObjectSaveIDTable::DoStream(ReconBuffer* b, int) {
  int compressed = 1;
  b->ReconBool(&compressed);
  if (compressed) b->EnableCompression();
  short id = 0;
  b->Recon16(&id, 1);
  m_recon->m_ids.clear();
  while (id != 0 && b->m_pos <= b->m_size && m_recon->m_ids.size() < HouseRecon::kMaxIds) {
    short type = 0;
    b->Recon16(&type, 1);
    m_recon->m_ids.push_back({id, findHRSel(type)});
    b->Recon16(&id, 1);
  }
}

// 8004516C
HouseRecon__Entry* ObjectSaveIDTable::findHRSel(short saveId) const {
  for (HouseRecon::Entry& e : m_recon->m_entries)
    if (e.m_saveId == saveId) return &e;
  return nullptr;
}

// ---------------------------------------------------------------------------------------------------
// objectmodule.cpp

// 800F36E4
// The house's objects whose type its 'DUMP' names are killed (the bulldozed or evicted lot's things).
// GC: called by PostLoad (objectmodule.cpp, the world side's; PC: BGExec_LoadHouse calls it after
// PostLoad until PostLoad does).
void ObjectModuleImpl::killDemolishedObjects(iResFile* file) {
  Memory__HandleNode* dump = file->GetByID(kDumpResType, 1, nullptr);
  const unsigned int count = dump && dump->m_data ? dump->m_size / 4 : 0;
  if (!count) return;
  std::vector<short> ids;
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) {
    ObjSelector* s = o->GetSelector();
    const unsigned int guid = s ? s->GetGUID() : 0u;
    for (unsigned int i = 0; i < count; ++i)
      if (ReadLE32(dump->m_data + i * 4) == guid) ids.push_back(static_cast<short>(o->GetID()));
  }
  for (short id : ids)
    if (GetObjectFromID(id)) KillObject(id);
  std::printf("[hood] %d demolished objects killed (%u types)\n", static_cast<int>(ids.size()), count);
}

// ---------------------------------------------------------------------------------------------------
// The pristine reset (SimsMemCardWrap::SaveGame in author mode, DAT_80475C74, or when asked): the
// neighbourhood as a new game would find it.

namespace {
constexpr int kFamilyResType = 0x46414d49;     // 'FAMI'
constexpr int kNeighborsResType = 0x4e425253;  // 'NBRS'
constexpr int kUserResType = 0x55736572;       // 'User'
constexpr int kSocialInterestVariableIndex = 1;  // 80500624

// ReconStreamPtrVector<Neighbor> (linkonce): the neighbour list as one resource.
struct NeighborVectorStream {
  std::vector<Neighbor*>* m_list;
  void DoStream(ReconBuffer* b, int version) { DoPtrVectorStream(*m_list, b, version); }
};
}  // namespace

// Part of 800439FC (HoodManager::ResetToPristine): the lot being played back to as it was bought. The
// scenario scripts (GUID 0xE6B26801) go; so do the objects with data 0x3B 10 or 0x1E and those of GUID
// 0x3FD182D0; every other object gets its value (data 0x29: the master's definition +0x4C, on a
// free-play lot its own +0x24) and fresh attributes (definition +0x74 of them, else 8), its tree stack
// emptied (data 0 kept; on a free-play lot it counts as placed from the inventory). Then the chairs face
// again, and with objects left: the lot data, the story's funds cleared (story lots), "msg:show special"
// and the lot saved. PC: the object calls are the port's world-side members (GetData, m_data, KillObject,
// TreeStack::Reset; ObjectModuleImpl::GetFirst / GetObjectByGUID / cXObjectImpl::GetNext as the module's
// list).
void ResetLotObjectsToPristine(NghResFile* file) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module) return;
  auto guidOf = [](cXObjectImpl* o) { return o->m_selector ? static_cast<unsigned int>(o->m_selector->GetGUID()) : 0u; };
  for (cXObjectImpl* o = module->m_first; o;) {  // GetObjectByGUID(0xE6B26801) until none
    cXObjectImpl* next = o->m_nextObject;
    if (guidOf(o) == 0xe6b26801u) module->KillObject(o->m_id);
    o = next;
  }
  const int house = Globs_pNeighborhood ? Globs_pNeighborhood->GetHouseNumber() : 0;  // vtable 0x5C
  const bool storyLot = static_cast<unsigned int>(house - 0xd) > 3;
  for (cXObjectImpl* o = module->m_first; o;) {  // GetFirst / GetNext
    cXObjectImpl* next = o->m_nextObject;
    const int kind = o->GetData(0x3b);
    if (kind == 10 || kind == 0x1e || guidOf(o) == 0x3fd182d0u) {
      module->KillObject(o->m_id);  // vtable 0x5C
      o = next;
      continue;
    }
    const short data0 = static_cast<short>(o->GetData(0));
    auto* master = o->m_selector ? reinterpret_cast<ObjSelector*>(o->m_selector->GetMasterSelector()) : nullptr;
    short value = master && master->m_def ? ERQuickdata::S16(master->m_def, 0x4c) : 0;
    if (!storyLot && o->m_selector && o->m_selector->m_def) value = ERQuickdata::S16(o->m_selector->m_def, 0x24);
    o->m_data[0x29] = value;  // SetData(0x29)
    delete[] o->m_attrs;
    o->m_attrCount = o->m_def ? ERQuickdata::S16(o->m_def, 0x74) : 0;  // GetDef()
    if (o->m_attrCount == 0) o->m_attrCount = 8;
    o->m_attrs = new short[o->m_attrCount]();  // GC: left uninitialised
    if (o->m_treeSim) o->m_treeSim->m_stack.Reset();  // +0xEC vtable 0x2C
    o->m_data[0] = data0;  // SetData(0)
    if (!storyLot) ObjSetWasPlacedFromInventory(o, true);  // vtable 0x46C
    o = next;
  }
  for (cXObjectImpl* o = module->m_first; o; o = o->m_nextObject) ObjUpdateChairFacing(o);
  if (module->m_first) {
    if (Globs_pHouse) Globs_pHouse->ComputeAndStoreLotData();  // vtable 0x8C
    if (storyLot && Globs_pSimulator) {
      Globs_pSimulator->SetFunds(0, cSimulator__tFundsPlayerNumber{0});
      Globs_pSimulator->SetFunds(0, cSimulator__tFundsPlayerNumber{1});
      Globs_pSimulator->CommitPendingFunds();
    }
    module->BroadcastMessage(const_cast<char*>("msg:show special"), 0);  // vtable 0x174
    if (Globs_pNeighborhood) Globs_pNeighborhood->SaveHouse(file);  // vtable 0x4C
  }
  std::printf("[hood] pristine: the lot's objects reset (house %d)\n", house);
}

// 800439FC
// GC: first the lot being played: the objects of GUID 0xE6B26801 go, then each object of kind 10 or 0x1E
// (object data 0x3B) or of GUID 0x3FD182D0 goes and every other one is reset (its graphic, attributes
// and behaviour stack; in story lots also its data via vtable 0x46C), chairs re-faced, the lot data
// stored, story funds zeroed and committed, "msg:show special" sent and the house saved. Then each
// family resource is reset: a player family's members lose their relationships and the family is
// cleared (a free-play family keeps its house, funds and house value); families but 1..4 are named
// "#freeplayN" (lots 13..16) or "#houseNN"; each is saved again. Then the neighbours (ResetNeighbors),
// and the 'User' resources are read once more. 1: done.
RET_UNKNOWN HoodManager::ResetToPristine() {
  NghResFile* file = Globs_pNghResFile;
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  if (!file || !hood) return 0;
  const int version = Globs_iSaveFileVersion;
  ResetLotObjectsToPristine(file);
  int result = -1;
  const int count = file->Count(kFamilyResType);
  for (int i = 1; i <= count; ++i) {
    Memory__HandleNode* handle = file->GetByIndex(kFamilyResType, static_cast<short>(i), nullptr);
    if (!handle) continue;
    short id = -1;
    file->GetID(handle, &id);
    if (id == -1) continue;
    const bool player = (static_cast<unsigned short>(id) & 0x4000) != 0;
    FamilyImpl* f = nullptr;
    if (!player) {
      f = new FamilyImpl(-1);  // GC: 0x38 bytes
    } else {
      f = new PlayerFamilyImpl(-1);  // GC: 0xD68 bytes
      id = static_cast<short>(id & 0xbfff);
    }
    f->LoadFamily(file, id);
    if (player) {
      for (int m = 0; int* guid = f->GetIndexedMember(m); ++m)
        hood->RemoveNeighborRelationships(hood->FindNeighborByGUID(*guid));
      int house = 0, funds = 0, value = 0;
      if (f->GetNumber() > 0x13 && f->GetNumber() < 0x1c) {
        house = f->GetHouseNumber();
        funds = f->GetFunds();
        value = f->GetHouseValue();
      }
      f->ClearFamily();
      f->SetHouseNumber(house);
      f->SetFunds(funds);
      f->SetHouseValue(value);
    }
    if (static_cast<unsigned int>(id - 1) > 3) {
      const int house = f->GetHouseNumber();
      char name[16];
      if (static_cast<unsigned int>(house - 0xd) < 4) std::snprintf(name, sizeof name, "#freeplay%d", house - 0xc);
      else std::snprintf(name, sizeof name, "#house%02d", house);
      // GC: FamilyImpl::SetName (a StringBuffer2 copy).
      f->m_name.clear();
      for (const char* c = name; *c; ++c) f->m_name.push_back(static_cast<char16_t>(*c));
    }
    result = f->SaveFamily(file, version) ? 0 : -1;
    delete f;
  }
  if (result != 0 || !ResetNeighbors()) return 0;
  // The 'User' resources read once more (GC: into a scratch description, with the CAS quickdata held).
  auto* casData = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(0x2a2af469u, nullptr, 0));
  const int users = file->Count(kUserResType);
  for (int i = 1; i <= users; ++i) {
    UserDataSaveLoad data;
    if (file->GetByIndex(kUserResType, static_cast<short>(i), nullptr)) ReconLoadObject(&data, file, kUserResType, static_cast<short>(i), nullptr);
  }
  if (casData) casData->DelRef();
  std::printf("[hood] neighbourhood reset to pristine (%d families)\n", count);
  return 1;
}

// 8004455C
// The neighbour list read from the file; each neighbour's social interest in every relationship
// cleared, its motives reset (motives 5..9 and 13..15 at 20, the rest 0; both sets) and new wants and
// fears; the list written back. 1: done.
RET_UNKNOWN HoodManager::ResetNeighbors() {
  NghResFile* file = Globs_pNghResFile;
  if (!file) return 0;
  const int version = Globs_iSaveFileVersion;
  std::vector<Neighbor*> list;
  NeighborVectorStream stream{&list};
  int r = ReconLoadObject(&stream, file, kNeighborsResType, 1, nullptr);
  if (r == 0) {
    for (Neighbor* n : list) {
      if (!n) continue;
      if (RelMatrixImpl* rel = n->GetRelations()) {
        const int keys = rel->CountKeys();
        for (int k = 0; k < keys; ++k) {
          const int key = rel->GetNthKey(k);
          if (kSocialInterestVariableIndex < rel->GetArraySize(key)) rel->SetValue(key, kSocialInterestVariableIndex, 0);
        }
      }
      for (int m = 0; m < 0x10; ++m) {
        float v = 0.0f;
        if (m != 11 && ((m >= 5 && m <= 9) || (m >= 13 && m <= 15))) v = 20.0f;
        n->m_motives[m] = v;
        n->m_motives2[m] = v;
      }
      n->InitWantsFears();
    }
    r = ReconSaveObject(&stream, file, kNeighborsResType, 1, version);
  }
  for (Neighbor* n : list) delete n;  // NeighborList::DeleteAll
  return r == 0 ? 1 : 0;
}
