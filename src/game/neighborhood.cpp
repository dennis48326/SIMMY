// Port of game/neighborhood.cpp (NeighborhoodImpl: load and save, neighbours, families, variables)
// and the neighbourhood side of game/gamedata.cpp (the default and saved neighbourhoods, the CAS sim
// handed to and from its neighbour).
//
// Not ported: the house side (LoadHouse, SaveHouse, MoveOut, house and family info for the menus),
// persistent data to and from the people on the lot (cXPerson), relationships counts and levels,
// wants/fears notifications, roommates; the simulator's house value at save time; HoodManager.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

NeighborhoodImpl* Globs_pNeighborhood = nullptr;  // 804FDC10 Globs::pNeighborhood
int Globs_iSaveFileVersion = 0x61;               // 804FDBF8 Globs::iSaveFileVersion
int gMoneyForNewFamily = 20000;                  // NeighborhoodConstants "money for new family"
float gArchValueMultiplier = 0.7f;               // NeighborhoodConstants "architecture value multiplier"
// The other NeighborhoodConstants (the neighbourhood's family score, which this build never computes).
int gNewFamilyStartHour = 7;
float gPerfectFriendCount = 15.0f;
float gHouseSizeWeight = 3.0f;
float gYardScoreMultiplier = 0.33f;
int gScorePerDirtyObject = 6;
int gScorePerBrokenObject = 15;

extern int g_loadedGameSlot;   // DAT_80476214 (game/memcardscreens.cpp)
extern int g_loadedGameSlot2;  // DAT_80476218
extern char ESimsApp_m_nameOfNgh[0x11];  // 804FB1D0 ESimsApp::m_nameOfNgh (appmain.cpp)

namespace {
constexpr int kNeighborhoodResType = 0x4e474248;  // 'NGBH'
constexpr int kNeighborsResType = 0x4e425253;     // 'NBRS'
constexpr int kFamilyResType = 0x46414d49;        // 'FAMI'

// The person objects that never become neighbours (GUIDs).
constexpr unsigned int kNoNeighborGUIDs[5] = {0x8a48ef3du, 0x5171d15du, 0x57d37970u, 0xe9c38767u, 0x608a1d66u};

// SortFamilyByCreation (800CCD54): by creation order, then number (both the other way round, as the
// GameCube's comparison reads).
bool SortFamilyByCreation(FamilyImpl* a, FamilyImpl* b) {
  if (a->GetCreationOrder() == b->GetCreationOrder()) return b->GetNumber() < a->GetNumber();
  return b->GetCreationOrder() < a->GetCreationOrder();
}

// ReconStreamPtrVector<Neighbor> (linkonce): the neighbour list as one resource.
struct NeighborListStream {
  std::vector<Neighbor*>* m_list;
  void DoStream(ReconBuffer* b, int version) { DoPtrVectorStream(*m_list, b, version); }
};

// NeighborhoodConstants::UpdateConstants' values (group 4 of the global object file's float constants;
// see the member below).
bool sConstantsLoaded = false;
void UpdateConstants() {
  gArchValueMultiplier = GlobalFloatConstant(4, "architecture value multiplier", 0.7f);
  gNewFamilyStartHour = static_cast<int>(GlobalFloatConstant(4, "new family start hour", 7.0f));
  gPerfectFriendCount = GlobalFloatConstant(4, "perfect family friend count", 15.0f);
  gHouseSizeWeight = GlobalFloatConstant(4, "weight of house size in overall score", 3.0f);
  gMoneyForNewFamily = static_cast<int>(GlobalFloatConstant(4, "money for new family", 20000.0f));
  gYardScoreMultiplier = GlobalFloatConstant(4, "yard score multiplier", 0.33f);
  gScorePerDirtyObject = static_cast<int>(GlobalFloatConstant(4, "upkeep penalty per dirty object", 6.0f));
  gScorePerBrokenObject = static_cast<int>(GlobalFloatConstant(4, "upkeep penalty per broken object", 15.0f));
  if (std::getenv("SIMMY_SIM_TRACE"))
    std::printf("[hood] constants: arch x%.2f, start hour %d, friends %.0f, house size weight %.1f, new family $%d, "
                "yard x%.2f, dirty %d, broken %d\n", gArchValueMultiplier, gNewFamilyStartHour, gPerfectFriendCount,
                gHouseSizeWeight, gMoneyForNewFamily, gYardScoreMultiplier, gScorePerDirtyObject, gScorePerBrokenObject);
}
}  // namespace

// 800CC740
// The neighbourhood tuning from group 4 (GlobalConstantsClient(4), GetNeighborhoodConstantsClient) of
// the global object file's float constants (FloatConstants::Load / Get), with the code's defaults. PC:
// NeighborhoodImpl::Load runs the body directly (the client object is not made).
void NeighborhoodConstants::UpdateConstants() { ::UpdateConstants(); }

// ---------------------------------------------------------------------------------------------------
// Neighborhood

// 800CC6D0
Neighborhood* Neighborhood::CreateInstance() {
  Globs_pNeighborhood = new NeighborhoodImpl;
  return Globs_pNeighborhood;
}

// 800CC6F8
void Neighborhood::DestroyInstance(Neighborhood* n) {
  if (n == Globs_pNeighborhood) Globs_pNeighborhood = nullptr;
  delete n;
}

// ---------------------------------------------------------------------------------------------------
// NeighborhoodImpl

// 800CCAD8
NeighborhoodImpl::NeighborhoodImpl() {}

// 800CCB84
NeighborhoodImpl::~NeighborhoodImpl() {
  for (FamilyImpl* f : m_families) Family::DestroyInstance(f);
  m_families.clear();
  for (Neighbor* n : m_neighbors) delete n;  // NeighborList::DeleteAll
  m_neighbors.clear();
}

// 800D194C
NeighborhoodImpl* NeighborhoodImpl::GetImpl() { return this; }

// 800CCCAC
void NeighborhoodImpl::RelationshipsChanged(Neighbor* n) {
  n->m_relationsDirty = 1;
  if (FamilyImpl* f = GetFamily(n->m_pers[0x3d])) f->m_friendCountDirty = 1;  // +0xEA: the family number
}

// 800CCD00
void NeighborhoodImpl::PostSim() {}

// 800CCD04
void NeighborhoodImpl::LevelComplete(int level) {
  if (GetHighestLevelCompleted() < level) m_vars[kVarLevel] = static_cast<short>(level);
}

// 800CCE18
// The neighbours, the user selectors (and their anim lists, not ported) and the families go.
void NeighborhoodImpl::Unload() {
  for (Neighbor* n : m_neighbors) delete n;
  m_neighbors.clear();
  if (Globs_pObjectFolder) Globs_pObjectFolder->DeleteUserSelectors();
  for (FamilyImpl* f : m_families) Family::DestroyInstance(f);
  m_families.clear();
}

// 800CCF44
// The neighbourhood from the file: its stream ('NGBH'), the CAS-made sims' looks ('User', as user
// selectors), the neighbours ('NBRS'; those whose object is missing are dropped), the type
// attributes ('TATT'), then every family ('FAMI'; members without a neighbour dropped), a "Default"
// family 0 when there is none, empty free play families removed, and a neighbour for each person
// object that has none. Then the players' neighbour ids into variables 4 and 0x13. 0: loaded.
int NeighborhoodImpl::Load(NghResFile* file) {
  if (!sConstantsLoaded) {
    UpdateConstants();
    sConstantsLoaded = true;
  }
  ObjectFolderImpl* folder = Globs_pObjectFolder;
  Unload();
  int version = 0;
  int r = ReconLoadObject(this, file, kNeighborhoodResType, 1, &version);
  m_visiting = static_cast<short>(GetNeighborhoodVar(kVarVisiting));
  if (folder) folder->LoadUserData(file);
  if (r == 0) {
    NeighborListStream list{&m_neighbors};
    r = ReconLoadObject(&list, file, kNeighborsResType, 1, nullptr);
    for (Neighbor* n : std::vector<Neighbor*>(m_neighbors)) {
      if (!n || n->m_selector) continue;
      std::printf("[hood] neighbourhood.cpp: Missing file: %s (neighbour %d, GUID %08X)\n", n->m_name, n->m_id,
                  static_cast<unsigned int>(n->m_guid));
      RemoveNeighbor(n);
    }
    if (folder) folder->Load(file);
  }
  if (r == -0x62) r = 0;
  const int count = file->Count(kFamilyResType);
  for (int i = 1; i <= count; ++i) {
    Memory__HandleNode* handle = file->GetByIndex(kFamilyResType, static_cast<short>(i), nullptr);
    if (!handle) continue;
    short rawId = -1;
    file->GetID(handle, &rawId);
    unsigned short id = static_cast<unsigned short>(rawId);
    if (id == 0xffff) continue;
    FamilyImpl* f;
    if (id & 0x4000) {
      f = new PlayerFamilyImpl(-1);  // GC: 0xD68 bytes
      id &= 0xbfff;
    } else {
      f = new FamilyImpl(-1);  // GC: 0x38 bytes
    }
    f->LoadFamily(file, static_cast<short>(id));
    m_families.push_back(f);
    // Members that are no neighbour go.
    for (;;) {
      const int n = f->CountMembers();
      int k = 0;
      for (; k < n; ++k) {
        int* m = f->GetIndexedMember(k);
        if (m && !FindNeighborByGUID(*m)) break;
      }
      if (k >= n) break;
      f->RemoveMember(*f->GetIndexedMember(k));
    }
  }
  if (!GetFamily(0)) {
    auto* f = new FamilyImpl(0);
    f->m_name = u"Default";  // GC: SetName
    m_families.push_back(f);
  }
  if (m_families.size() > 1) std::sort(m_families.begin(), m_families.end(), [](FamilyImpl* a, FamilyImpl* b) { return SortFamilyByCreation(a, b); });
  for (int number = 0x14; number < 0x1c; ++number) {
    FamilyImpl* f = GetFamily(number);
    if (!f || f->CountMembers() != 0) continue;
    if (f->GetHouseNumber() > 0xc && f->GetHouseNumber() < 0x11) HoodManager::RemoveAllObjects(f->GetHouseNumber() - 1);
    f->SetHouseNumber(0);
    RemoveFamily(f);
  }
  if (folder) {
    for (ObjSelector* s = folder->GetNextSelector(nullptr); s; s = folder->GetNextSelector(s)) {
      if (!s->m_def || objdef::Type(s->m_def) != 2 || FindNeighborByType(s)) continue;
      const unsigned int guid = s->GetGUID();
      if (std::find(std::begin(kNoNeighborGUIDs), std::end(kNoNeighborGUIDs), guid) != std::end(kNoNeighborGUIDs)) continue;
      AddNewNeighbor(s);
    }
  }
  UpdateFamilyNumbers();
  if (g_loadedGameSlot)
    if (Neighbor* n = _gd.m_players[0].GetPrimaryNeighbor()) SetNeighborhoodVar(kVarPlayer1, n->m_id);
  Neighbor* p2 = _gd.m_players[1].GetPrimaryNeighbor();
  if (!_gd.m_fieldEC || !p2 || !g_loadedGameSlot2) SetNeighborhoodVar(kVarPlayer2, 0);
  else SetNeighborhoodVar(kVarPlayer2, p2->m_id);
  return r;
}

// 800CD808
// Each neighbour's family number (person data +0xEA), 0 for none.
void NeighborhoodImpl::UpdateFamilyNumbers() {
  for (Neighbor* n : m_neighbors)
    if (n) n->m_pers[0x3d] = 0;
  for (FamilyImpl* f : m_families)
    for (int k = 0; k < f->CountMembers(); ++k)
      if (Neighbor* n = FindNeighborByGUID(*f->GetIndexedMember(k))) n->m_pers[0x3d] = static_cast<short>(f->GetNumber());
}

// 800CD930
// Into the file (its neighbourhood resources replaced): the house number into variable 2 when the
// simulator runs a lot (its global 0x1A), 'NGBH' (in free play with the visited house as loaded),
// 'NBRS'; each family (the played house's family first gets its house value from the simulator:
// objects + architecture x gArchValueMultiplier + the lot; every family's friend count is brought up
// to date), 'TATT' and the CAS-made sims ('User').
int NeighborhoodImpl::Save(NghResFile* file, int version) {
  cSimulatorImpl* sim = Globs_pSimulator;
  file->FlushCharacterData(false);
  file->FlushNeighborData();
  if (sim && sim->GetGlobal(0x1a) != 0) m_vars[kVarHouse] = static_cast<short>(m_houseNumber);
  const short visiting = static_cast<short>(GetNeighborhoodVar(kVarVisiting));
  if (_gd.m_fieldE8) SetNeighborhoodVar(kVarVisiting, m_visiting);
  int r = ReconSaveObject(this, file, kNeighborhoodResType, 1, version);
  if (_gd.m_fieldE8) SetNeighborhoodVar(kVarVisiting, visiting);
  if (r == 0) {
    NeighborListStream list{&m_neighbors};
    r = ReconSaveObject(&list, file, kNeighborsResType, 1, version);
  }
  for (FamilyImpl* f : m_families) {
    if (sim && f->GetHouseNumber() != 0 && f->GetHouseNumber() == m_houseNumber) {
      const int objects = sim->GetObjectsValue();
      const float arch = static_cast<float>(sim->GetArchValue()) * gArchValueMultiplier;
      f->SetHouseValue(objects + static_cast<int>(arch) + sim->GetLotValue());
    }
    GetFamilyFriendsCount(f);
    f->SaveFamily(file, version);
  }
  if (Globs_pObjectFolder) {
    Globs_pObjectFolder->Save(file);
    Globs_pObjectFolder->SaveUserData(file);
  }
  return r;
}

// 800CDC2C
// The variables (variable 1, the story mode flag, keeps its value; above 1 it is cleared; GC: the
// EORGlobals' +0x32C/+0x32D overrides, debug), the name, and from version 0x5D +0x6C.
void NeighborhoodImpl::DoStream(ReconBuffer* b, int version) {
  const short story = m_vars[kVarStory];
  b->Recon16(m_vars, kVarCount);
  if (b->m_mode == ReconBuffer__Mode::kRead) {
    m_vars[kVarStory] = story;
    if (static_cast<unsigned short>(story) > 1) SetNeighborhoodVar(kVarStory, 0);
  }
  unsigned short name[0x20];
  for (int i = 0; i < 0x20; ++i) name[i] = static_cast<unsigned short>(m_name[i]);
  b->ReconString(name, 0x20);
  for (int i = 0; i < 0x20; ++i) m_name[i] = static_cast<char16_t>(name[i]);
  if (version < 0x5d) m_field6C = 0;
  else b->Recon16(&m_field6C, 1);
}

// 800CDD38
int NeighborhoodImpl::GetHouseNumberForLevel(int) { return 0; }

// 800CDD40
Neighbor* NeighborhoodImpl::FindNeighborByID(int id) {
  if (id > 0 && id <= static_cast<int>(m_neighbors.size())) return m_neighbors[static_cast<size_t>(id - 1)];
  return nullptr;
}

// 800CDD78
Neighbor* NeighborhoodImpl::FindNeighborByGUID(int guid) {
  if (guid == 0) return nullptr;
  for (Neighbor* n : m_neighbors)
    if (n && n->GetGUID() == guid) return n;
  return nullptr;
}

// 800CDE00
bool NeighborhoodImpl::FindFamilyAndIndexByGUID(int guid, int& family, int& index) {
  Neighbor* n = FindNeighborByGUID(guid);
  FamilyImpl* f = n ? n->GetFamily() : nullptr;
  if (!f) return false;
  for (int k = 0; k < f->CountMembers(); ++k) {
    if (*f->GetIndexedMember(k) != guid) continue;
    family = f->GetNumber();
    index = k;
    return true;
  }
  return false;
}

// 800CDEE0
Neighbor* NeighborhoodImpl::FindNeighborByType(ObjSelector* selector) {
  for (Neighbor* n : m_neighbors)
    if (n && n->m_selector == selector) return n;
  return nullptr;
}

// 800CE120
// The person's neighbour (by its type; none: a new neighbour of that type) becomes its neighbour id;
// a neighbour with person data gives the person its persistent fields (those its data version has, but
// 0x3A and 0x41) and its family (0x3D); a member of family 1 also gets the neighbour's motives. PC:
// characters share the template person's selector (no user selectors), so a person whose neighbour is
// known (the saved id, data 0x1F, of a neighbour of its type; or the one it was made for) keeps it.
void NeighborhoodImpl::LoadPersistentData(cXPerson* person) {
  cXObject* object = person ? person->AsObject() : nullptr;
  auto* p = object ? dynamic_cast<cXPersonImpl*>(object->GetObjectImplementation()) : nullptr;
  if (!p) return;
  Neighbor* n = p->m_neighbor;
  if (!n) {
    Neighbor* saved = p->m_personData[0x1f] > 0 ? FindNeighborByID(p->m_personData[0x1f]) : nullptr;
    if (saved && saved->m_selector == p->GetSelector()) n = saved;
  }
  if (!n) n = FindNeighborByType(p->GetSelector());
  if (!n) {
    n = AddNewNeighbor(p->GetSelector());
    if (!n) return;
    p->m_neighbor = n;
    p->SetNeighborID(n->m_id);
    return;
  }
  p->m_neighbor = n;
  p->SetNeighborID(n->m_id);
  if (n->m_persVersion != 0) {
    const int count = Neighbor::GetNumPersistentDataFields();
    for (int i = 0; i < count; ++i) {
      const Neighbor::PersDataPair* f = Neighbor::GetPersistentDataFieldsByIndex(i);
      if (f->m_version <= n->m_persVersion && f->m_field != 0x3a && f->m_field != 0x41)
        p->SetPersonData(f->m_field, n->m_pers[f->m_field]);
    }
  }
  p->SetPersonData(0x3d, n->m_pers[0x3d]);
  FamilyImpl* family = n->GetFamily();
  if (family && family->GetNumber() == 1) {
    Motives* m = p->GetMotives();
    for (int i = 0; i < Motives::kCount; ++i) {
      m->m_values[i] = n->m_motives[i];
      m->m_old[i] = n->m_motives2[i];
    }
  }
}

// 800CE408
// The person's persistent fields (and, for family 1, its motives) into its neighbour (by its id), at
// the latest person data version.
void NeighborhoodImpl::SavePersistentData(cXPerson* person) {
  cXObject* object = person ? person->AsObject() : nullptr;
  auto* p = object ? dynamic_cast<cXPersonImpl*>(object->GetObjectImplementation()) : nullptr;
  Neighbor* n = p ? FindNeighborByID(p->GetPersonData(0x1f)) : nullptr;
  if (!n) return;
  n->m_persVersion = Neighbor::GetLatestPersDataVersion();
  const int count = Neighbor::GetNumPersistentDataFields();
  for (int i = 0; i < count; ++i) {
    const Neighbor::PersDataPair* f = Neighbor::GetPersistentDataFieldsByIndex(i);
    n->m_pers[f->m_field] = static_cast<short>(p->GetPersonData(f->m_field));
  }
  FamilyImpl* family = n->GetFamily();
  if (family && family->GetNumber() == 1) {
    const Motives* m = p->GetMotives();
    for (int i = 0; i < Motives::kCount; ++i) {
      n->m_motives[i] = m->m_values[i];
      n->m_motives2[i] = m->m_old[i];
    }
  }
}

// 800CDF30
// A neighbour for the object, in the last free id (else a new one at the end).
Neighbor* NeighborhoodImpl::AddNewNeighbor(ObjSelector* selector) {
  if (!selector) return nullptr;
  int id = 0;
  for (size_t i = 0; i < m_neighbors.size(); ++i)
    if (!m_neighbors[i]) id = static_cast<int>(i) + 1;
  if (id == 0) {
    m_neighbors.push_back(nullptr);
    id = static_cast<int>(m_neighbors.size());
  }
  auto* n = new Neighbor(static_cast<short>(id), selector);  // GC: calloc'd 0x1B8 bytes
  m_neighbors[static_cast<size_t>(id - 1)] = n;
  return n;
}

// 800CE300
// The neighbour's row goes from everyone's relationships, and its own relationships.
void NeighborhoodImpl::RemoveNeighborRelationships(Neighbor* n) {
  for (Neighbor* o : m_neighbors)
    if (o) o->GetRelations()->RemoveArray(n->m_id);
  n->RemoveAllRelationships();
}

// 800CE388
// GC: GlobalDispatch(0xF6, id) first.
void NeighborhoodImpl::RemoveNeighbor(Neighbor* n) {
  m_neighbors[static_cast<size_t>(n->m_id - 1)] = nullptr;
  RemoveNeighborRelationships(n);
  delete n;
}

// 800CE5A4
int NeighborhoodImpl::GetNextNeighborID(short id) {
  const int n = static_cast<int>(m_neighbors.size());
  int i = static_cast<unsigned short>(id);
  while (i < n) {
    i = static_cast<short>(i + 1);
    if (m_neighbors[static_cast<size_t>(i - 1)]) return i;
  }
  return 0;
}

// 800CE5F8
// GC: case 0 walks the module's people (GetNumPeople/GetPeople); PC: the people by id (cXPersonImpl),
// as everywhere else. Case 1: GC asks the house's family (HouseImpl::GetFamily); PC: the family in the
// house loaded. Nothing writable: `ref` is cleared.
int NeighborhoodImpl::GetNeighborData(short id, short data, short** ref) {
  short* dummy = nullptr;
  if (!ref) ref = &dummy;
  *ref = nullptr;
  Neighbor* n = FindNeighborByID(id);
  if (!n) return 0;
  switch (data) {
    case 0: {
      // The person in the house playing the neighbour (PC: by its neighbour; GC: the neighbour's
      // object, matched through its selector, which is the neighbour's own).
      ObjectModuleImpl* module = Globs_pObjectModule;
      const int count = module ? module->GetNumObjects() : 0;
      for (int i = 1; i <= count; ++i) {
        cXObject* o = module->GetObjectFromID(i);
        cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
        auto* person = impl && impl->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
        // PC: a saved person before its PostLoad (an earlier object's load tree asking) by its saved
        // neighbour id (person data 0x1F).
        if (person && (person->m_neighbor ? person->m_neighbor == n
                                          : (impl->m_selector == n->m_selector || person->m_personData[0x1f] == n->m_id)))
          return impl->m_id;
      }
      return 0;
    }
    case 1: {
      FamilyImpl* f = GetFamilyInHouse(GetHouseNumber());
      return f && f->TestMember(n->m_guid) ? 1 : 0;
    }
    case 2: return n->m_pers[0x3a];
    case 5:
    case 9: return n->GetFriendCount();
    case 6:
      for (FamilyImpl* f : m_families)
        if (f->TestMember(n->GetGUID())) return static_cast<short>(f->GetHouseNumber());
      return 0;
    case 7:
    case 8: {
      if (n->m_pers[0x3d] == 0) return 0;
      FamilyImpl* f = GetFamily(n->m_pers[0x3d]);
      if (!f || f->GetHouseNumber() == 0) return 0;
      return (data == 7 ? f->GetHasPhone() : f->GetHasBaby()) != 0 ? 1 : 0;
    }
    case 10: return n->GetReputationValue();
    default: return 0;
  }
}

// 800CE968
// The family numbered `number` (in free play, family 1 is the one in the house being played).
FamilyImpl* NeighborhoodImpl::GetFamily(int number) {
  for (FamilyImpl* f : m_families)
    if (f->GetNumber() == number) return f;
  if (number == 1 && _gd.m_fieldE8) return GetFamilyInHouse(m_houseNumber);
  return nullptr;
}

// 800CEA24
// The family living in `house` (story lots below 13 never give family 1).
FamilyImpl* NeighborhoodImpl::GetFamilyInHouse(int house) {
  for (FamilyImpl* f : m_families)
    if (f->GetHouseNumber() == house && (house >= 0xd || f->GetNumber() != 1)) return f;
  return nullptr;
}

// 800CEB20
int NeighborhoodImpl::GetNumFamilies() { return static_cast<int>(m_families.size()); }
// 800CEB34
FamilyImpl* NeighborhoodImpl::GetFamilyByIndex(int index) { return m_families[static_cast<size_t>(index)]; }

// 800CEB44
int NeighborhoodImpl::GetFamilyIndex(Family* f) {
  for (size_t i = 0; i < m_families.size(); ++i)
    if (GetFamilyByIndex(static_cast<int>(i)) == f) return static_cast<int>(i);
  return -1;
}

// 800CEBDC
FamilyImpl* NeighborhoodImpl::MakeNewFamily() { return BaseMakeNewFamily(false, -1); }

// 800CEC04
FamilyImpl* NeighborhoodImpl::MakeNewFreePlayFamily(int number) {
  return GetFamily(number) ? nullptr : BaseMakeNewFamily(true, number);
}

// 800CEC64
// A new (player) family numbered `number` (the first free number for -1), created after all others,
// with the new family money.
FamilyImpl* NeighborhoodImpl::BaseMakeNewFamily(bool player, int number) {
  if (number < 0) {
    number = 1;
    while (GetFamily(number)) ++number;
    if (number > 0x7fff) return nullptr;
  }
  int order = 1;
  for (FamilyImpl* f : m_families)
    if (order <= f->GetCreationOrder()) order = f->GetCreationOrder() + 1;
  FamilyImpl* f = player ? new PlayerFamilyImpl(number) : new FamilyImpl(number);
  f->ClearFamily();
  f->SetCreationOrder(order);
  f->SetFunds(gMoneyForNewFamily);
  m_families.push_back(f);
  if (m_families.size() > 1) std::sort(m_families.begin(), m_families.end(), [](FamilyImpl* a, FamilyImpl* b) { return SortFamilyByCreation(a, b); });
  return f;
}

// 800CEF48
// Only a numbered family without a house can go. 0: removed.
int NeighborhoodImpl::RemoveFamily(Family* family) {
  auto* f = static_cast<FamilyImpl*>(family);
  if (f->GetNumber() == 0 || f->GetHouseNumber() != 0) return -1;
  auto it = std::find(m_families.begin(), m_families.end(), f);
  if (it == m_families.end()) return -1;
  m_families.erase(it);
  Family::DestroyInstance(f);
  return 0;
}

// 800CF028
// `n` joins the family: the visitor types of the people on the lot are redone; the person on the lot
// (if any) and the neighbour get the family number (data 0x3D) and data 0x43 cleared (the person's
// object data: SetPersonData, vtable 0x144), the family's friend count is to be redone, GlobalDispatch
// 0xE4 (no listener in the port). 0: joined.
int NeighborhoodImpl::AddToFamily(Neighbor* n, Family* family) {
  auto* f = static_cast<FamilyImpl*>(family);
  if (f->TestMember(n->GetGUID())) return -1;
  f->AddMember(n->GetGUID());
  UpdateInstanceVisitorTypes();
  if (!f->GetMemberByGUID(n->GetGUID())) return -1;
  if (cXPerson* p = Globs_pObjectModule ? Globs_pObjectModule->GetPersonByGUID(n->GetGUID()) : nullptr)
    if (auto* impl = dynamic_cast<cXPersonImpl*>(p)) {
      impl->SetPersonData(0x3d, static_cast<short>(f->GetNumber()));
      impl->SetPersonData(0x43, 0);
    }
  n->m_pers[0x3d] = static_cast<short>(f->GetNumber());
  n->m_pers[0x43] = 0;
  f->m_friendCountDirty = 1;
  return 0;
}

// 800CF1CC
// `n` leaves its family: the person on the lot (if any) and the neighbour lose the family number (data
// 0x3D), the visitor types are redone, the friend count is to be redone, GlobalDispatch 0xE4.
int NeighborhoodImpl::RemoveFromFamily(Neighbor* n) {
  if (n->m_pers[0x3d] == 0) return -1;
  FamilyImpl* f = GetFamily(n->m_pers[0x3d]);
  if (!f || !f->GetMemberByGUID(n->GetGUID())) return -1;
  if (cXPerson* p = Globs_pObjectModule ? Globs_pObjectModule->GetPersonByGUID(n->GetGUID()) : nullptr)
    if (auto* impl = dynamic_cast<cXPersonImpl*>(p)) impl->SetPersonData(0x3d, 0);
  n->m_pers[0x3d] = 0;
  f->RemoveMember(n->GetGUID());
  UpdateInstanceVisitorTypes();
  f->m_friendCountDirty = 1;
  return 0;
}

// 800CF320
// (Two-player games) `n` moves into the player family (1) as player 2's roommate; the family it
// came from is remembered (+0x6C) for RemovePlayerRoommate.
void NeighborhoodImpl::AddAsPlayerRoommate(Neighbor* n) {
  FamilyImpl* player = GetFamily(1);
  FamilyImpl* from = n->GetFamily();
  if (from == player) return;
  RemovePlayerRoommate();
  if (m_field6C != 0 || !from) return;
  m_field6C = static_cast<short>(from->GetNumber());
  if (RemoveFromFamily(n) == 0) AddToFamily(n, player);
}

// 800CF408
// The roommate (the player family's second member) goes back to the family it came from.
void NeighborhoodImpl::RemovePlayerRoommate() {
  if (m_field6C == 0) return;
  FamilyImpl* player = GetFamily(1);
  int* guid = player ? player->GetIndexedMember(1) : nullptr;
  Neighbor* n = guid ? FindNeighborByGUID(*guid) : nullptr;
  if (player && n) {
    FamilyImpl* home = GetFamily(m_field6C);
    if (RemoveFromFamily(n) == 0) {
      player->m_flags &= ~FamilyImpl::kP2DoneCAS;
      AddToFamily(n, home);
    }
  }
  m_field6C = 0;
}

// 800CF54C
// A neighbour for a new CAS-made sim (a new user selector), with default values (and new wants and
// fears). 0: made.
int NeighborhoodImpl::AddNewCharacter(Neighbor** out) {
  Neighbor* n = Globs_pObjectFolder ? AddNewNeighbor(Globs_pObjectFolder->CreateNewUserSelector()) : nullptr;
  if (!n) return -1;
  n->InitDefaultValues();
  n->InitWantsFears();
  *out = n;
  return 0;
}

// 800CF724
int NeighborhoodImpl::GetNumCharacters() {
  int count = 0;
  for (Neighbor* n : m_neighbors)
    if (n && n->IsCharacter()) ++count;
  return count;
}

// 800CF5C4
// A CAS-made sim (IsCharacter) that no family has goes: its people on the lot are killed, its user
// selector removed and the neighbour removed. 0: deleted, -1: kept.
int NeighborhoodImpl::DeleteCharacter(Neighbor* n) {
  if (!n || !n->IsCharacter()) return -1;
  ObjSelector* selector = n->m_selector;
  const int guid = static_cast<int>(selector->GetGUID());
  for (FamilyImpl* f : m_families)
    if (f->TestMember(guid)) return -1;
  // GC: ObjectModule GetObjectByGUID (vtable 0xE4) until none, each killed. PC: the people by id.
  if (ObjectModuleImpl* module = Globs_pObjectModule)
    for (int id = 1; id <= module->GetNumObjects(); ++id) {
      cXObject* x = module->GetObjectFromID(id);
      cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
      if (o && o->GetSelector() && static_cast<int>(o->GetSelector()->GetGUID()) == guid)
        module->KillObject(static_cast<short>(id));
    }
  std::printf("[hood] neighbour %d (GUID %08X) deleted\n", n->m_id, static_cast<unsigned int>(guid));
  if (n->IsCharacter() && Globs_pObjectFolder) Globs_pObjectFolder->RemoveSelector(selector);
  RemoveNeighbor(n);
  return 0;
}

// 800CF794
int NeighborhoodImpl::GetFamilyFriendsCount(Family* family) {
  auto* f = static_cast<FamilyImpl*>(family);
  if (f->m_friendCountDirty) UpdateFamilyFriendsCount(f);
  return f->m_friendCount;
}

// 800CF7D0
// Its funds and house value; for the family of the house being played, the simulator's funds and
// the lot's worth (objects, architecture at 0.7, lot value).
int NeighborhoodImpl::GetFamilyNetWorth(Family* family) {
  auto* f = static_cast<FamilyImpl*>(family);
  int funds = f->GetFunds();
  int value = f->m_houseValue;
  if (m_houseNumber != 0 && f->GetHouseNumber() == m_houseNumber && Globs_pSimulator) {
    cSimulatorImpl* sim = Globs_pSimulator;
    const int objects = sim->GetObjectsValue();
    const int arch = static_cast<int>(static_cast<float>(sim->GetArchValue()) * gArchValueMultiplier);
    value = objects + arch + sim->GetLotValue();
    funds = sim->GetFunds(0);
  }
  return funds + value;
}

// 800D00AC
// The neighbours of other families that some member of the family counts as a friend (relationship
// type 6).
void NeighborhoodImpl::UpdateFamilyFriendsCount(Family* family) {
  auto* f = static_cast<FamilyImpl*>(family);
  int count = 0;
  for (Neighbor* n : m_neighbors) {
    if (!n || f->TestMember(n->GetGUID()) || n->m_pers[0x3d] == 0) continue;
    for (int k = 0; k < f->CountMembers(); ++k) {
      Neighbor* m = FindNeighborByGUID(*f->GetIndexedMember(k));
      if (m && Neighbor::RelCountsAsRel(m->CalculateRelationshipLevel(n), 6)) {
        ++count;
        break;
      }
    }
  }
  f->SetFriendCount(count);
}

// 800D0304
int NeighborhoodImpl::GetHousePrice(cSimulatorImpl* sim) {
  return sim->GetLotValue() + sim->GetObjectsValue() + static_cast<int>(gArchValueMultiplier * static_cast<float>(sim->GetArchValue()));
}

// 800D03C0
// The family's name, net worth and house (its friends when `friends`).
bool NeighborhoodImpl::GetFamilyInfo(Family* family, FamilyInfo* info, bool friends) {
  auto* f = static_cast<FamilyImpl*>(family);
  info->m_name = f->m_name.substr(0, 0x7f);  // GC: GetName into a StringBuffer2 of 0x80
  info->m_netWorth = f->GetNetWorth();
  info->m_friends = 0;
  info->m_house = f->GetHouseNumber();
  if (friends) info->m_friends = f->GetFriendCount();
  return true;
}

// 800D04C0
bool NeighborhoodImpl::GetFamilyInfo(int number, FamilyInfo* info) {
  FamilyImpl* f = GetFamily(number);
  return f ? GetFamilyInfo(f, info, true) : false;
}

// 800D0520
bool NeighborhoodImpl::GetHouseInfo(NghResFile* file, int house, HouseInfo* info) {
  if (static_cast<unsigned int>(house - 1) >= 0x10) return false;
  file->SetCurrentHouse(static_cast<unsigned int>(house));
  if (!GetHouseInfo(file, info)) return false;
  if (FamilyImpl* f = GetFamilyInHouse(house)) {
    info->m_family = f->GetNumber();
    GetFamilyInfo(info->m_family, &info->m_familyInfo);
  } else {
    info->m_family = -1;
  }
  info->m_house = house;
  return true;
}

// 800D05F8
bool NeighborhoodImpl::GetHouseInfo(NghResFile* file, HouseInfo* info) {
  return GetHouseFileInfo(file, &info->m_price, &info->m_global1A, &info->m_valued, &info->m_empty);
}

// 800D0640
// From the current house's 'SIMI' (read into a simulator of its own): global 0x16 (the lot's value was
// computed), 0x1A, empty (neither 0x1A nor 0x15) and the price. False: no 'SIMI'.
bool NeighborhoodImpl::GetHouseFileInfo(NghResFile* file, int* price, bool* global1A, bool* valued, bool* empty) {
  cSimulatorImpl* sim = cSimulator::CreateInstance();  // GC: DestroyInstance(0) first (nothing)
  int version = 0;
  if (ReconLoadObject(sim, file, 0x53494d49 /*SIMI*/, 1, &version) != 0) {
    cSimulator::DestroyInstance(sim);
    return false;
  }
  *valued = sim->GetGlobal(0x16) != 0;
  *global1A = sim->GetGlobal(0x1a) != 0;
  *empty = !*global1A && sim->GetGlobal(0x15) == 0;
  *price = GetHousePrice(sim);
  cSimulator::DestroyInstance(sim);
  return true;
}

// 800CF90C
// The family into a free `house` (1..16), as just moved in. 0: moved in.
int NeighborhoodImpl::MoveIn(Family* family, int house) {
  auto* f = static_cast<FamilyImpl*>(family);
  if (static_cast<unsigned int>(house - 1) >= 0x10 || GetFamilyInHouse(house)) return -1;
  f->SetHouseNumber(house);
  f->SetJustMovedIn(true);
  return 0;
}

// 800CF9B0
// The selected player family (GameData +0xD8) lives in `house` now.
void NeighborhoodImpl::SetPlayerFamilyInHouse(int house) {
  const int number = _gd.m_fieldD8;
  FamilyImpl* f = GetFamily(number);
  if (number == 0 || !f) return;
  f->SetHouseNumber(house);
  f->SetJustMovedIn(false);
}

// 800D12E0
// Each person on the lot gets its visitor type (person data 0x20): a resident of the house (HouseImpl::
// IsResident) 0, anyone else keeps theirs (none: 1, a visitor). GC: the module's people list; PC: the
// people by id.
RET_UNKNOWN NeighborhoodImpl::UpdateInstanceVisitorTypes() {
  ObjectModuleImpl* module = Globs_pObjectModule;
  const int count = module ? module->GetNumObjects() : 0;
  for (int i = 1; i <= count; ++i) {
    cXObject* o = module->GetObjectFromID(i);
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    cXPersonImpl* p = impl && impl->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
    if (!p) continue;
    short type = static_cast<short>(p->GetPersonData(0x20));
    if (type == 0) type = 1;
    if (Globs_pHouse && Globs_pHouse->IsResident(p)) type = 0;  // Globs::pHouse vtable 0x4C
    p->SetPersonData(0x20, type);
  }
  return 0;
}


// 800D13D4
// Every neighbour but `except`; the strongest answer.
int NeighborhoodImpl::NotifyAllNeighbors(WantFear__Notification& n, Neighbor* except) {
  int best = 0;
  for (Neighbor* nb : m_neighbors)
    if (nb && nb != except) best = std::max(best, nb->Notify(n, true));
  return best;
}

// 800D146C
// The neighbours living in the house (GC: HouseImpl::GetFamily) or in the player's family.
int NeighborhoodImpl::NotifyAllResidents(WantFear__Notification& n, Neighbor* except) {
  int best = 0;
  FamilyImpl* house = GetFamilyInHouse(GetHouseNumber());
  FamilyImpl* player = GetFamily(_gd.m_fieldD8);
  for (Neighbor* nb : m_neighbors) {
    if (!nb || nb == except) continue;
    if (!(house && house->TestMember(nb->GetGUID())) && !(player && player->TestMember(nb->GetGUID()))) continue;
    best = std::max(best, nb->Notify(n, true));
  }
  return best;
}

// 800D15B0
// The people on the lot but `except`'s (GC: ObjectModule GetNumPeople/GetPeople; PC: the people by id).
int NeighborhoodImpl::NotifyAllPersons(WantFear__Notification& n, Neighbor* except) {
  int best = 0;
  ObjectModuleImpl* module = Globs_pObjectModule;
  const int count = module ? module->GetNumObjects() : 0;
  for (int i = 1; i <= count; ++i) {
    cXObject* o = module->GetObjectFromID(i);
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    cXPersonImpl* p = impl && impl->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
    if (!p || (except && NeighborPerson(except) == p)) continue;
    if (Neighbor* nb = FindNeighborByID(p->GetPersonData(0x1f))) best = std::max(best, nb->Notify(n, true));
  }
  return best;
}

// 800D1294
int NeighborhoodImpl::GetHouseNumber() { return m_houseNumber; }
// 800D129C
void NeighborhoodImpl::UnloadHouse() {}
// 800D1950
void NeighborhoodImpl::SetHouseNum(int house) { m_houseNumber = house; }
// 800D1958
bool NeighborhoodImpl::IsFreeplayLot(int house) { return static_cast<unsigned int>(house - 0xd) < 4; }
// 800D1884
const char16_t* NeighborhoodImpl::GetNeighborhoodName() { return m_name; }
// 800D18A8
int NeighborhoodImpl::GetHighestLevelCompleted() { return m_vars[kVarLevel]; }
// 800D18B0
int NeighborhoodImpl::GetNeighborhoodVar(int index) const { return m_vars[index]; }
// 800D18C0
short* NeighborhoodImpl::GetNeighborhoodVarRef(int index) { return &m_vars[index]; }
// 800D18D0
void NeighborhoodImpl::SetNeighborhoodVar(int index, short value) { m_vars[index] = value; }
// 800D18E0
bool NeighborhoodImpl::GetPlayerVisitingAnotherHouse() const { return GetNeighborhoodVar(kVarVisiting) != 0; }
// 800D1928
int NeighborhoodImpl::GetNumNeighborHouses() { return static_cast<int>(m_neighborHouses.size()); }
// 800D193C
int NeighborhoodImpl::GetNeighborHouseByIndex(int index) { return m_neighborHouses[static_cast<size_t>(index)]; }

CasSimDescriptionS2C* NeighborhoodImpl::SimDescriptionOf(int guid) {
  Neighbor* n = FindNeighborByGUID(guid);
  return n && n->m_selector ? n->m_selector->GetNormalSimDescription() : nullptr;
}

// ---------------------------------------------------------------------------------------------------
// GameData (game/gamedata.cpp): the neighbourhood

// 8003B2B0
// A new game's neighbourhood: the default file read and copied into the card cache, loaded (variable
// 1 says story mode, 2 cleared) and saved back into the file's resources.
void GameData::LoadDefaultNeighborhood() {
  Globs_pNghResFile->ReadFromFile(ESimsApp_m_nameOfNgh);
  Globs_pNghResFile->CopyDefaultNghToCache(ESimsApp_m_nameOfNgh);
  NeighborhoodImpl* hood = Globs_pNeighborhood->GetImpl();
  hood->Load(Globs_pNghResFile);
  hood->m_vars[NeighborhoodImpl::kVarStory] = static_cast<short>(_gd.m_fieldE8 == 0);
  hood->m_vars[NeighborhoodImpl::kVarHouse] = 0;
  hood->Save(Globs_pNghResFile, Globs_iSaveFileVersion);
  std::printf("[hood] default neighbourhood: %d neighbours, %d families\n", static_cast<int>(hood->m_neighbors.size()),
              hood->GetNumFamilies());
}

// 8003B5A0
// The neighbourhood of the save just read (variable 1 says story mode).
void GameData::LoadSavedNeighborhood() {
  NeighborhoodImpl* hood = Globs_pNeighborhood->GetImpl();
  hood->Load(Globs_pNghResFile);
  hood->m_vars[NeighborhoodImpl::kVarStory] = static_cast<short>(_gd.m_fieldE8 == 0);
  FamilyImpl* f = hood->GetFamily(_gd.m_fieldD8 ? _gd.m_fieldD8 : 1);
  std::printf("[hood] saved neighbourhood: %d neighbours, %d families; player family %d: house %d, %d members, flags %X\n",
              static_cast<int>(hood->m_neighbors.size()), hood->GetNumFamilies(), f ? f->GetNumber() : -1,
              f ? f->GetHouseNumber() : 0, f ? f->CountMembers() : 0, f ? f->m_flags : 0);
}

namespace {
// The neighbour sim `id` stands for: neighbour `id`, or (negative) member `index` of the player's
// family (player 2's when PlayerNumber says so and a second player's game is loaded).
Neighbor* SimNeighbor(int id, int index) {
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  if (!hood) return nullptr;
  if (id >= 0) return hood->FindNeighborByID(id);
  int number = g_loadedGameSlot2;
  if (UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber")) == 0) number = g_loadedGameSlot ? g_loadedGameSlot : 1;
  FamilyImpl* f = hood->GetFamily(number);
  int* guid = f ? f->GetIndexedMember(index) : nullptr;
  return guid ? hood->FindNeighborByGUID(*guid) : nullptr;
}
}  // namespace

// 8003B638
// CAS slot `slot` gets neighbour `id` (see SimNeighbor) to edit: its id and data.
void GameData::PrepareSimData(int id, int slot) {
  Neighbor* n = SimNeighbor(id, slot);
  if (!n || !m_customChars) return;
  m_customChars->m_neighborIds[slot] = static_cast<unsigned int>(n->m_id);
  n->SaveToCharacter(m_customChars->m_chars[slot]);
}

// 8003B744
// CAS slot `slot`'s sim into its neighbour (see SimNeighbor, `index` the member); `renameFamily`:
// the neighbour is the head (person data 0x1A) and its family takes the CAS family name.
void GameData::ReplaceSimData(int id, bool renameFamily, int slot, int index) {
  Neighbor* n = SimNeighbor(id, index);
  if (!n || !m_customChars) return;
  n->LoadFromCharacter(m_customChars->m_chars[slot]);
  if (!renameFamily) return;
  n->m_pers[0x1a] = 1;
  if (FamilyImpl* f = n->GetFamily()) {
    f->m_name.clear();  // GC: SetName(the custom characters' family name)
    for (unsigned short c : m_customChars->m_familyName) f->m_name.push_back(static_cast<char16_t>(c));
  }
}

// 8003B878
void GameData::LoadCreateASim(int id, int slot) { PrepareSimData(id, slot); }

// 8003B8A0
void GameData::SaveCreateASim() { Globs_pNeighborhood->GetImpl()->Save(Globs_pNghResFile, Globs_iSaveFileVersion); }

// 8003C728
// A family made in CAS (any slot used) is stored. 1: stored (or nothing to store).
RET_UNKNOWN GameData::SaveCreateAFamily() {
  bool any = false;
  for (int i = 0; i < 4 && m_customChars && !any; ++i) any = m_customChars->m_used[i] != 0;
  return any ? StoreCreateAFamilyData() : 1;
}

namespace {
// A UTF-16 name (at most `n` characters) into a selector's name.
std::u16string Name16(const unsigned short* s, int n) {
  std::u16string out;
  for (int i = 0; i < n && s[i]; ++i) out.push_back(static_cast<char16_t>(s[i]));
  return out;
}
}  // namespace

// 8003BBCC
// Create-a-Family on family `number` (GameData +0x38; 0 or less: a new family): the family's name, the
// four slots emptied (a default look), then each member in its slot (used, its GUID and neighbour id,
// its look, personality, aspiration, skills, interests and names).
void GameData::PrepCreateAFamilyData(int number) {
  m_field38 = number;
  if (number <= 0 || !m_customChars) return;
  NeighborhoodImpl* hood = Globs_pNeighborhood->GetImpl();
  FamilyImpl* family = hood->GetFamily(number);
  if (!family) return;
  NeighborhoodCustomChars* table = m_customChars;
  // GC: GetName into the family name buffer when there is one.
  table->m_familyName.assign(family->m_name.begin(), family->m_name.end());
  for (int i = 0; i < 4; ++i) {
    table->m_used[i] = 0;
    table->m_field630[i] = 0;
    table->m_neighborIds[i] = 0;
    table->m_chars[i].m_desc = CasSimDescriptionS2C(true);
  }
  for (int i = 0; i < family->CountMembers() && i < 4; ++i) {
    const int guid = *family->GetIndexedMember(i);
    table->m_used[i] = 1;
    table->m_field630[i] = guid;
    Neighbor* n = hood->FindNeighborByGUID(guid);
    if (!n) continue;
    table->m_neighborIds[i] = static_cast<unsigned int>(n->m_id);
    ObjSelector* selector = hood->GetNeighborSelector(n->m_id);
    ENeighborhoodCustomChar& c = table->m_chars[i];
    if (selector && selector->m_simDesc) c.m_desc = *selector->m_simDesc;
    c.m_nice = n->m_pers[2];
    c.m_active = n->m_pers[3];
    c.m_field04 = n->m_pers[4];
    c.m_playful = n->m_pers[5];
    c.m_outgoing = n->m_pers[6];
    c.m_neat = n->m_pers[7];
    c.m_aspiration = static_cast<unsigned char>(n->m_pers[0x14]);
    for (int k = 0; k < 10; ++k) c.m_field13C[1 + k] = n->m_pers[9 + k];
    c.m_field13C[11] = n->m_pers[0x18];
    c.m_field13C[12] = n->m_pers[0x38];
    c.m_field13C[13] = n->m_pers[0x39];
    c.m_field174 = n->m_pers[0x3f];
    if (selector) {
      const std::u16string& first = selector->GetUserName();
      const std::u16string& last = selector->GetUserLastName();
      for (int k = 0; k < 0x20; ++k) c.m_desc.m_firstName[k] = k < static_cast<int>(first.size()) ? first[k] : 0;
      for (int k = 0; k < 0x20; ++k) c.m_desc.m_lastName[k] = k < static_cast<int>(last.size()) ? last[k] : 0;
      c.m_desc.m_firstName[0x1f] = 0;  // GC: wcsncpy 0x20 then the 32nd character cleared
      c.m_desc.m_lastName[0x1f] = 0;
    }
  }
  std::printf("[caf] family %d into Create-a-Family: %d members\n", number, family->CountMembers());
}

// 8003C074
// A new CAS-made sim in the family (person data 0x3A = 0x1B); its GUID.
int GameData::CreateAFamilyAddFamilyMember(Family* family) {
  Neighbor* n = nullptr;
  Globs_pNeighborhood->AddNewCharacter(&n);
  if (!n) return 0;
  Globs_pNeighborhood->GetImpl()->AddToFamily(n, family);
  n->m_pers[0x3a] = 0x1b;
  return n->GetGUID();
}

// 8003C114
// The family made in Create-a-Family into the neighbourhood: the family (GameData +0x38's, else a new
// one) takes the CAS family name; its members no slot holds any more leave it and are deleted; each
// used slot without a sim gets a new one (its skills cleared); then each slot's look, thumbnail,
// personality (and star sign), aspiration, skills, interests and names go into its neighbour. 1: stored.
RET_UNKNOWN GameData::StoreCreateAFamilyData() {
  NeighborhoodImpl* hood = Globs_pNeighborhood->GetImpl();
  FamilyImpl* family = m_field38 < 1 ? hood->MakeNewFamily() : hood->GetFamily(m_field38);
  if (!family || !m_customChars) return 0;
  NeighborhoodCustomChars* table = m_customChars;
  family->m_name.assign(table->m_familyName.begin(), table->m_familyName.end());  // GC: SetName
  // The members no used slot holds (GC: at most four, in a local array).
  std::vector<Neighbor*> gone;
  for (int k = 0; k < family->CountMembers(); ++k) {
    const int guid = *family->GetIndexedMember(k);
    bool held = false;
    for (int i = 0; i < 4; ++i)
      if (table->m_used[i] && table->m_field630[i] == guid) held = true;
    if (!held) gone.push_back(hood->FindNeighborByGUID(guid));
  }
  for (Neighbor* n : gone) {
    if (!n) continue;
    hood->RemoveFromFamily(n);
    hood->DeleteCharacter(n);
  }
  for (int i = 0; i < 4; ++i) {
    if (!table->m_used[i]) continue;
    ENeighborhoodCustomChar& c = table->m_chars[i];
    if (table->m_field630[i] == 0) {
      for (int k = 1; k < 14; ++k) c.m_field13C[k] = 0;
      c.m_field174 = 0;
      table->m_field630[i] = CreateAFamilyAddFamilyMember(family);
    }
    Neighbor* n = hood->FindNeighborByGUID(table->m_field630[i]);
    if (!n) continue;
    table->m_neighborIds[i] = static_cast<unsigned int>(n->m_id);
    ObjSelector* selector = hood->GetNeighborSelector(n->m_id);
    if (!selector) continue;
    selector->SetThumbnail(static_cast<ETexture*>(c.m_faceImage));
    if (!selector->m_simDesc) selector->m_simDesc = new CasSimDescriptionS2C;  // GC: a user selector always has one
    *selector->m_simDesc = c.m_desc;
    n->m_pers[2] = c.m_nice;
    n->m_pers[3] = c.m_active;
    n->m_pers[4] = c.m_field04;
    n->m_pers[5] = c.m_playful;
    n->m_pers[6] = c.m_outgoing;
    n->m_pers[7] = c.m_neat;
    n->m_pers[0x46] = ComputeZodiacSign(n->m_pers);
    n->m_pers[0x14] = c.m_aspiration;
    for (int k = 0; k < 10; ++k) n->m_pers[9 + k] = static_cast<short>(c.m_field13C[1 + k]);
    n->m_pers[0x18] = static_cast<short>(c.m_field13C[11]);
    n->m_pers[0x38] = static_cast<short>(c.m_field13C[12]);
    n->m_pers[0x39] = static_cast<short>(c.m_field13C[13]);
    n->m_pers[0x3f] = static_cast<short>(c.m_field174);
    selector->SetUserName(Name16(c.m_desc.m_firstName, 0x20));
    selector->SetUserLastName(Name16(c.m_desc.m_lastName, 0x20));
    n->m_persVersion = Neighbor::GetLatestPersDataVersion();
  }
  std::printf("[caf] family %d stored: %d members, funds %d\n", family->GetNumber(), family->CountMembers(), family->GetFunds());
  return 1;
}

// 800CE55C
ObjSelector* NeighborhoodImpl::GetNeighborSelector(int id) {
  Neighbor* n = FindNeighborByID(id);
  return n ? n->m_selector : nullptr;
}
