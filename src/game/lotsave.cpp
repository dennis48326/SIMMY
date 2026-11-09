// The lot's save path (PC file; the functions belong to game/neighborhood.cpp, game/house.cpp and
// game/objectmodule.cpp): what the player built and bought goes into the neighbourhood file when the
// game is saved from the pause menu.
//
// GC: SaveGameTarget::PerformSaveGame (step 0, in game) pauses the simulator, has the house compute its
// lot data (HouseImpl::ComputeAndStoreLotData), commits the simulator's pending funds and calls
// NeighborhoodImpl::SaveHouse: the family takes the lot's funds, the house's resources are flushed and
// HouseImpl::SaveFile writes them again ('SIMI' the simulator, 'HOUS' the house, 'Arry' the world's
// floors, walls and tile flags, 'objt' the object types, 'ObjM' the objects, 'DUMP'); then the
// neighbourhood itself is saved. The background save (SimsMemCardWrap::SaveGame ->
// NghResFile::WriteToMemoryCard) then packs the host section (SIMI, HOUS) and the house's section into
// the card cache and onto the card.
//
// PC: HouseImpl is not ported (Globs::pHouse); its SaveFile is a stand-in here (HouseSaveFile) on the
// port's world, simulator and object module. ObjectModuleImpl::DoStream (objectmodule.cpp) reads only;
// its write half is WriteObjectModuleStream here.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/placement.hpp"

#include <cstdio>
#include <string>

namespace {
constexpr int kSimulatorResType = 0x53494d49;    // 'SIMI' kSimulatorResType[0]
constexpr int kHouseResType = 0x484f5553;        // 'HOUS' kSimulatorResType[1]
constexpr short kSimulatorResourceID = 1;        // kSimulatorResourceID
constexpr short kHouseResourceID = 1;            // kHouseResourceID
constexpr int kObjectModuleResType = 0x4f626a4d; // 'ObjM'
constexpr int kDumpResType = 0x44554d50;         // 'DUMP'
constexpr unsigned int kPlaceholderGUID = 0x3c08965f;

// HouseImpl's own stream (800BC1B8 HouseImpl::DoStream): the house's name (BString +0x18); before
// version 0x5A also twelve unused words. PC: the name is kept as the house's 'HOUS' resource had it.
struct HouseRecord {
  std::string m_name;
  void DoStream(ReconBuffer* b, int version) {
    b->ReconString(m_name);
    if (version < 0x5a) {
      int unused = 0;
      for (int i = 0; i < 12; ++i) b->Recon32(&unused, 1);
    }
  }
};
HouseRecord s_house;

// The write half of ObjectModuleImpl::DoStream (800F3C94), version 0x2F on: whether compressed (yes);
// each object's id and type (by the type table's save id), 0 ending them; a mark; per object its own
// stream (cXObject vtable 0x4EC: cXObjectImpl::ReconStream) and from version 0x36 its object map entry
// (sim flags and idle ticks), each followed by a mark; then the module's own data: the object it
// remembers (DoReconObject: its id; PC: none), the list of objects after it (PC: empty) and the last
// name the user typed (cXObjectImpl::sLastUserTypedName; PC: none typed).
void WriteObjectModuleStream(ObjectModuleImpl* m, ReconBuffer* b, int version) {
  int compressed = 1;
  b->ReconBool(&compressed);
  if (compressed) b->EnableCompression();
  ObjSelector* placeholder = Globs_pObjectFolder->GetPlaceholder();
  for (cXObjectImpl* o = m->m_first; o; o = o->m_nextObject) {
    short id = static_cast<short>(o->GetID());
    b->Recon16(&id, 1);
    ObjSelector* selector = o->GetSelector();
    Globs_pObjectFolder->ReconSelector(&selector, b, true, version);
  }
  short end = 0;
  b->Recon16(&end, 1);
  b->ReconMark();
  for (cXObjectImpl* o = m->m_first; o; o = o->m_nextObject) {
    const bool placeholderObject = o->GetSelector() == placeholder || (o->m_def && objdef::GUID(o->m_def) == kPlaceholderGUID);
    o->ReconStream(b, version, placeholderObject);
    if (version > 0x35) {
      const unsigned int index = static_cast<unsigned int>(o->GetID() - 1);
      int entry = index < m->m_simFlags.size() ? static_cast<int>(m->m_simFlags[index]) : 0;
      b->ReconInt(&entry, 1);
    }
    b->ReconMark();
  }
  short remembered = 0;  // DoReconObject (800F3B1C): the object's id, 0 for none
  b->Recon16(&remembered, 1);
  if (version > 0x33) {
    int count = 0;
    b->ReconInt(&count, 1);
    std::string lastTyped;
    b->ReconString(lastTyped);
  }
}

// PC stand-in for ObjectModuleImpl::ComputeStats (800F8D44; ObjectModule is the world side's): over the
// house's objects (a multi-tile object once, by its lead; people and type 7 skipped), *evictValue the
// current value of what moving out leaves behind (IsDeletedByEvict: not a person, no build mode type,
// data 0x2B bit 0x10 clear), *buildValue the rest with a build mode type other than 0 and 4; the flags:
// a data 0x3B of 1, of 9, an object from the catalogue outside (tile flag 0x20 clear).
void ComputeObjectStats(int* evictValue, int* buildValue, bool* any1, bool* any9, bool* outside) {
  *evictValue = *buildValue = 0;
  *any1 = *any9 = *outside = false;
  if (!Globs_pObjectModule) return;
  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) {
    const int d = o->GetData(0x3b);
    if (d == 1) *any1 = true;
    else if (d == 9) *any9 = true;
    if (o->IsMultiTile() && GetLeadObjectImpl(o) != o) continue;
    const int type = o->GetType();
    if (type == 7) continue;
    auto* master = o->m_selector ? reinterpret_cast<ObjSelector*>(o->m_selector->GetMasterSelector()) : nullptr;
    const ObjDefinition* def = master && master->m_def ? master->m_def : o->m_def;
    const int buildType = def ? ERQuickdata::S16(def, 0x8a) : 0;  // GetBuildModeType (800EB488)
    const bool fromCatalog = def && (ERQuickdata::S16(def, 0x4e) != 0 || ERQuickdata::S16(def, 0x50) != 0 || buildType != 0);  // IsFromCatalog
    if (ObjIsInWorld(o) && fromCatalog && Globs_pFixedWorld) {
      CTilePt t = o->GetCTilePt();
      if (!(Globs_pFixedWorld->GetFlags(t) & 0x20)) *outside = true;
    }
    // IsDeletedByEvict (800E1D80)
    const bool evicted = type == 2 || (buildType == 0 && !(static_cast<unsigned short>(o->GetData(0x2b)) & 0x10));
    if (evicted) *evictValue += ObjGetCurrentValue(o);
    else if (buildType != 0 && buildType != 4) *buildValue += ObjGetCurrentValue(o);
  }
}

// HouseImpl::ComputeAndStoreLotData (800BBAE8, below): the objects' value (ComputeStats; PC: the
// stand-in above) and the architecture's (walls, wallpaper, floors) plus the build items into the
// simulator, the family's phone (an object with data 0x3B 1) and baby (9) flags (vtables 0x104 / 0x114);
// global 0x16 set (GC: the low half of the word ComputeArchValue was given, set to 1 after the call).
void ComputeAndStoreLotData(FamilyImpl* family) {
  int objects = 0, build = 0;
  bool any1 = false, any9 = false, outside = false;
  ComputeObjectStats(&objects, &build, &any1, &any9, &outside);
  bool built = false;
  const int arch = Globs_pFixedWorld ? static_cast<int>(Globs_pFixedWorld->ComputeArchValue(&built)) : 0;
  if (family) {
    family->SetHasPhone(any1);
    family->SetHasBaby(any9);
  }
  if (!Globs_pSimulator) return;
  Globs_pSimulator->SetObjectsValue(objects);
  Globs_pSimulator->SetArchValue(arch + build);
  Globs_pSimulator->SetGlobal(0x16, 1);
  // PC: printed when the values change (the rooms' updates call this often).
  static int last[2] = {-1, -1};
  if (last[0] != objects || last[1] != arch + build) {
    last[0] = objects;
    last[1] = arch + build;
    std::printf("[save] lot value: objects %d, architecture %d + build items %d\n", objects, arch, build);
  }
}

// PC stand-in for NeighborhoodImpl::DeleteCharacter (800CF5C4; the neighbourhood is the world side's):
// a character no family has (FamilyImpl::TestMember) goes: its person objects killed, its selector
// removed (ObjectFolderImpl vtable 0xBC), the neighbour removed. 0: deleted, -1: kept. PC guard: a
// player's selected sim is kept.
int DeleteUnusedCharacter(NeighborhoodImpl* hood, Neighbor* n) {
  if (!n->IsCharacter()) return -1;
  ObjSelector* selector = n->m_selector;
  const int guid = static_cast<int>(selector->GetGUID());
  for (FamilyImpl* f : hood->m_families)
    if (f && f->TestMember(guid)) return -1;
  for (int p = 0; p < kMaxPlayers; ++p)  // PC mod: 4 players (GC: 2)
    if (cXPerson* sel = _globals.GetSelectedPerson(p))
      if (cXObjectImpl* impl = sel->AsObject() ? sel->AsObject()->GetObjectImplementation() : nullptr)
        if (impl->m_def && static_cast<int>(objdef::GUID(impl->m_def)) == guid) return -1;
  std::printf("[save] neighbour #%d (%s) deleted: no family took it\n", n->m_id, n->m_name);
  if (ObjectModuleImpl* module = Globs_pObjectModule)  // GetObjectByGUID (vtable 0xE4) until none
    for (int id = 1; id <= module->GetNumObjects(); ++id) {
      cXObject* x = module->GetObjectFromID(id);
      cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
      if (o && o->m_def && static_cast<int>(objdef::GUID(o->m_def)) == guid) module->KillObject(static_cast<short>(id));
    }
  if (Globs_pObjectFolder) Globs_pObjectFolder->RemoveSelector(selector);
  hood->RemoveNeighbor(n);
  return 0;
}

// PC stand-in for HouseImpl::SaveFile (800BBC3C): the lot data, then the simulator ('SIMI'), the house
// ('HOUS'), the world ('Arry') and the objects ('objt', 'ObjM', 'DUMP') into `file`. 0: saved.
int HouseSaveFile(NghResFile* file) {
  const int version = Globs_iSaveFileVersion;
  if (Globs_pHouse) Globs_pHouse->ComputeAndStoreLotData();  // HouseImpl vtable 0x8C
  else ComputeAndStoreLotData(nullptr);
  if (Globs_pSimulator) ReconSaveObject(Globs_pSimulator, file, kSimulatorResType, kSimulatorResourceID, version);
  ReconSaveObject(&s_house, file, kHouseResType, kHouseResourceID, version);
  if (Globs_pFixedWorld) Globs_pFixedWorld->Save(file, version);
  if (Globs_pObjectModule) Globs_pObjectModule->Save(file);
  // GC: GlobalDispatch(0x92, file): the listeners' own resources (none the port registers).
  return 0;
}
}  // namespace

// 800F2FD4
// The house's objects into `file`: the type table ('objt', PrepareForModuleSave), each object told
// first (PreSave), the module's stream ('ObjM' 1), an empty 'DUMP' 1.
RET_UNKNOWN ObjectModuleImpl::Save(iResFile* file) {
  const int version = Globs_iSaveFileVersion;
  Globs_pObjectFolder->PrepareForModuleSave(file);
  for (cXObjectImpl* o = m_first; o; o = o->m_nextObject) o->PreSave();
  struct Adapter : ReconObject {
    ObjectModuleImpl* m_module;
    explicit Adapter(ObjectModuleImpl* m) : m_module(m) {}
    void DoStream(ReconBuffer* b, int v) override { WriteObjectModuleStream(m_module, b, v); }
    int GetType() override { return kObjectModuleResType; }
  } adapter(this);
  ReconBuilder builder;
  const int r = builder.Compact(&adapter, version, file, 1);  // ReconSaveObject<ObjectModuleImpl>
  static StringBuffer noName;
  file->Add(nullptr, kDumpResType, 1, noName, false);
  return r;
}

// 800D1110
// The lot being played into the neighbourhood file: the player family takes the lot's funds, the
// house's resources are flushed and written again (HouseImpl::SaveFile), then the neighbourhood.
// 0: saved. Between the two, the neighbours made for this lot that no family took (a sim description,
// person data 0x3D and 0x43 both 0) are deleted (DeleteCharacter).
RET_UNKNOWN NeighborhoodImpl::SaveHouse(NghResFile* file) {
  const int funds = Globs_pSimulator ? Globs_pSimulator->GetFunds(0) : 0;
  if (FamilyImpl* f = GetFamily(_gd.m_fieldD8)) f->SetFunds(funds);
  // PC: the file's current house is the lot being played (BGExec_LoadHouse's stand-in restores the
  // previous one after loading); the house's name is kept from its 'HOUS' resource (HouseImpl).
  if (_globals.m_house) file->SetCurrentHouse(static_cast<unsigned int>(_globals.m_house->m_house));
  if (ReconLoadObject(&s_house, file, kHouseResType, kHouseResourceID, nullptr) != 0) s_house.m_name.clear();
  file->FlushHouseData(file->GetCurrentHouse());
  const int r = HouseSaveFile(file);
  // The characters made for this lot that no family took (a sim description, person data 0x3D and 0x43
  // both 0) are deleted, the list walked again after each.
  for (bool again = true; again;) {
    again = false;
    for (Neighbor* n : m_neighbors) {
      if (!n || !n->m_selector || !n->m_selector->GetSimDescription() || n->m_pers[0x3d] != 0 || n->m_pers[0x43] != 0)
        continue;
      if (DeleteUnusedCharacter(this, n) == 0) {
        again = true;
        break;
      }
    }
  }
  if (r == 0) Save(file, Globs_iSaveFileVersion);
  std::printf("[save] house %u saved: funds %d, %d objects\n", file->GetCurrentHouse(), funds,
              [] {
                int n = 0;
                if (Globs_pObjectModule)
                  for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) ++n;
                return n;
              }());
  return r;
}

// 800BBAE8
// The lot data into the simulator and the house's family (see the helper above).
void HouseImpl::ComputeAndStoreLotData() { ::ComputeAndStoreLotData(m_family); }
