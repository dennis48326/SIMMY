// Port of game/objectfolder.cpp (ObjectFolderImpl, partial) and game/objselector.cpp (ObjSelector,
// partial): the object database. Ported: the database from the "SimsObjects" quickdata (a selector per
// ObjDefinition, found by GUID) and the house type table ('objt') a house's objects are read with.
// Also the user selectors (the CAS-made sims: 'User' resources of the neighbourhood file, each a GUID
// and a CasSimDescriptionS2C) and the type attribute blocks ('TATT').
// Still stubbed: the object resource files (QuickResFile, FileList: behaviours, strings, catalogue),
// careers, skills, the thumbnails and the object language (XObjLang).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>

ObjectFolderImpl* Globs_pObjectFolder = nullptr;  // 804FDC0C Globs::pObjectFolder
int ObjectFolderImpl::fGlobalFileID = -1;         // ObjectFolder::fGlobalFileID

namespace {
const char kTemplatePersonName[] = "templateperson";  // gTemplatePersonName
constexpr unsigned int kSimsObjectsQuickdata = 0x0c33db41;
constexpr unsigned int kCASQuickdata = 0x2a2af469;
constexpr unsigned int kPlaceholderGUID = 0x3c08965f;
constexpr int kObjectTypeTableResType = 0x6f626a74;  // 'objt'

// The row index of `addr` in its quickdata table (-1: none; GC: findRow, 0 also counting as none for
// the file ids).
int RowIndex(const ERQuickdata* qd, const void* addr) {
  unsigned int index = 0;
  return addr && qd->findRow(addr, &index) ? static_cast<int>(index) : -1;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// ObjSelector

// 801104DC
ObjSelector::ObjSelector() {}
// 801105A0
ObjSelector::~ObjSelector() { delete m_simDesc; }
// 801108DC
unsigned int ObjSelector::GetGUID() const {
  if (m_user) return m_userGuid;
  return m_def ? objdef::GUID(m_def) : 0;
}
// 80110A2C
const std::u16string& ObjSelector::GetUserName() { return m_userName; }
// 80110A80
void ObjSelector::SetUserName(const std::u16string& name) { m_userName = name; }
// 80110B04
const std::u16string& ObjSelector::GetUserLastName() { return m_userLastName; }
// 80110B58
void ObjSelector::SetUserLastName(const std::u16string& name) { m_userLastName = name; }
// 8011150C
// The file name without its folders and extension (GC: ExtractFileName).
void ObjSelector::GetShortFilename(char* out, int capacity) {
  std::string name = m_fileName ? m_fileName : "";
  const size_t slash = name.find_last_of("/\\:");
  if (slash != std::string::npos) name = name.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  if (dot != std::string::npos) name = name.substr(0, dot);
  std::strncpy(out, name.c_str(), static_cast<size_t>(capacity - 1));
  out[capacity - 1] = 0;
}
// 801116A0
// GC: an object instance's description first (+0x98, the person on the lot; not ported).
CasSimDescriptionS2C* ObjSelector::GetNormalSimDescription() const { return m_simDesc; }
// 80111170
bool ObjSelector::GetIsPerson() { return m_def && objdef::Type(m_def) == 2; }

// ---------------------------------------------------------------------------------------------------
// ObjectFolder

// 800EBF94
void ObjectFolder::CreateInstance() { Globs_pObjectFolder = new ObjectFolderImpl; }

// 800EBFBC
void ObjectFolder::DestroyInstance(ObjectFolder*) {
  delete Globs_pObjectFolder;
  Globs_pObjectFolder = nullptr;
}

// 800EC684
ObjectFolderImpl::ObjectFolderImpl() {}

// 800EC84C
// GC: Destroy (the selectors, their files and data).
ObjectFolderImpl::~ObjectFolderImpl() {
  if (auto* careers = static_cast<CareersImpl*>(Globs_pCareers)) careers->TearDown();
  for (ObjSelector*& chain : m_selectors)
    while (chain) {
      ObjSelector* next = chain->m_next;
      delete chain;
      chain = next;
    }
  for (TypeAttrBlock* b : m_typeAttrBlocks) delete b;
  if (m_objects) _quickdataman.DelRef(m_objects, EResourceManager__DelRefMode{0});
  if (m_cas) _quickdataman.DelRef(m_cas, EResourceManager__DelRefMode{0});
}

// 800ED0E4
// `path` is the game's root (GC: the folder "gamedata/objects/" under it is listed into a FileList for
// the object resource files).
void ObjectFolderImpl::Init(char*, bool user) {
  m_user = user ? 1 : 0;
  SIMMY_STUB("ObjectFolderImpl::Init: the object files' FileList", 0x800ED0E4);
  if (auto* careers = static_cast<CareersImpl*>(Globs_pCareers)) careers->Load();
  LoadDatabase();
  InitSkillLookup();
  SIMMY_STUB("ObjectFolderImpl::Init: GlobalDispatch(0xdc)", 0x800ED0E4);
  m_initialized = 1;
}

// 800EE2B4
// The "SimsObjects" quickdata's "ResFile" table holds one row per object file: +0x00 the file's
// ObjDefinitions, +0x38 the name of the NPC it is (CAS quickdata "Sim::NPC"). Each definition becomes
// a selector named by its row (or the file). "GlobalResFile" names the global file and the template
// person's.
void ObjectFolderImpl::LoadDatabase() {
  SIMMY_STUB("ObjectFolderImpl::LoadDatabase: _edithtreesetman AddRefAll/DelRefAll", 0x800EE2B4);
  m_objects = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(kSimsObjectsQuickdata, nullptr, 0));
  m_cas = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(kCASQuickdata, nullptr, 0));
  if (!m_objects) return;
  const void* resFiles = m_objects->getTable("ResFile");
  const void* globals = m_objects->getTable("GlobalResFile");
  const void* npcs = m_cas ? m_cas->getTable("Sim::NPC") : nullptr;
  if (!resFiles || !globals) return;
  const void* globalRow = m_objects->getRow(globals, "global");
  const void* templateRow = m_objects->getRow(globals, kTemplatePersonName);
  const void* globalFile = globalRow ? ERQuickdata::Ptr(globalRow, 0) : nullptr;
  const void* templateFile = templateRow ? ERQuickdata::Ptr(templateRow, 0) : nullptr;
  SIMMY_STUB("ObjectFolderImpl::LoadDatabase: the object files (QuickResFile, FileList)", 0x800EE2B4);
  fGlobalFileID = RowIndex(m_objects, globalFile);
  if (fGlobalFileID == 0) fGlobalFileID = -1;
  m_templatePersonFile = RowIndex(m_objects, templateFile);
  if (m_templatePersonFile == 0) m_templatePersonFile = -1;
  const unsigned int count = ERQuickdata::TableRowCount(resFiles);
  for (unsigned int i = 0; i < count; ++i) {
    const auto* row = static_cast<const unsigned char*>(ERQuickdata::TableRow(resFiles, i));
    const unsigned char* defs = static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 0));
    const unsigned int n = defs ? ERQuickdata::U32(defs - 4, 0) : 0;
    const char* fileName = ERQuickdata::TableRowName(resFiles, i);
    if (!fileName) fileName = "";
    const void* npc = nullptr;
    if (ERQuickdata::Ptr(row, 0x38) && npcs && m_cas)
      npc = m_cas->getRow(npcs, const_cast<char*>(ERQuickdata::String(row, 0x38)));
    for (unsigned int j = 0; j < n; ++j) {
      const auto* def = static_cast<const ObjDefinition*>(ERQuickdata::Ptr(defs, j * 4));
      if (!def) continue;
      const char* name = nullptr;
      unsigned int index = 0;
      if (const void* table = m_objects->findRow(def, &index)) name = ERQuickdata::TableRowName(table, index);
      if (!name) name = fileName;
      AddSelector(const_cast<ObjDefinition*>(def), const_cast<char*>(name), const_cast<char*>(fileName),
                  reinterpret_cast<ResFile*>(const_cast<unsigned char*>(row)), ERQuickdata::S16(def, 0xbc),
                  static_cast<Sim__NPC*>(const_cast<void*>(npc)));
    }
    // The template person's definition, copied by the user selectors (GC: +0x6B8).
    if (row == templateFile && n) m_templateDef = static_cast<const ObjDefinition*>(ERQuickdata::Ptr(defs, 0));
  }
}

// 800EDD94
// A selector for `def` (NPC definitions only in the user's folder), first in its GUID chain.
// 1: added.
int ObjectFolderImpl::AddSelector(ObjDefinition* def, char*, char* fileName, ResFile* file, short, Sim__NPC* npc) {
  if (objdef::NPCFlag(def) != 0 && m_user) return 0;
  auto* selector = new ObjSelector;  // GC: 0xA8 bytes
  selector->m_def = def;
  int count = 0;
  for (ObjSelector* chain : m_selectors)
    for (ObjSelector* s = chain; s; s = s->m_next) ++count;
  selector->m_index = count;
  selector->m_folder = this;
  selector->m_resFile = file;
  selector->m_fileName = fileName;
  if (npc) SIMMY_STUB("ObjectFolderImpl::AddSelector: the NPC's user name", 0x800EDD94);
  selector->m_row = RowIndex(m_objects, def);
  if (ERQuickdata::Ptr(def, 0x34)) selector->m_row2 = RowIndex(m_objects, ERQuickdata::Ptr(def, 0x34));
  SIMMY_STUB("ObjectFolderImpl::AddSelector: XObjLang, the definition's data (SmartDataFactory)", 0x800EDD94);
  // The type's attribute block (+0xB4 values).
  if (const short n = ERQuickdata::S16(def, 0xb4)) {
    auto* block = new TypeAttrBlock;
    block->m_guid = static_cast<int>(objdef::GUID(def));
    if (static_cast<unsigned int>(n) < 32000) block->m_values.assign(static_cast<size_t>(n), 0);
    m_typeAttrBlocks.push_back(block);
  }
  const unsigned int slot = selector->GetGUID() & 0xff;
  selector->m_next = m_selectors[slot];
  m_selectors[slot] = selector;
  return 1;
}

// 800ECAE4
ObjSelector* ObjectFolderImpl::GetSelectorByGUID(int guid) {
  for (ObjSelector* s = m_selectors[static_cast<unsigned int>(guid) & 0xff]; s; s = s->m_next)
    if (s->GetGUID() == static_cast<unsigned int>(guid)) return s;
  return nullptr;
}

// 800F0884
// GC: the placeholder object's selector (for saved types the database lacks).
ObjSelector* ObjectFolderImpl::GetPlaceholder() { return GetSelectorByGUID(static_cast<int>(kPlaceholderGUID)); }

// 800EE98C
// The selector of a saved object: by GUID (old saves, or when not by save id), else by the house's
// type id through the type table (the found selector moves to the front of its chain); types the
// database lacks get the placeholder.
void ObjectFolderImpl::ReconSelector(ObjSelector** selector, ReconBuffer* b, bool bySaveId, int version) {
  if (version < 0x2f || !bySaveId) {
    int guid = 0;
    if (b->m_mode != ReconBuffer__Mode::kRead) guid = static_cast<int>((*selector)->GetGUID());
    b->Recon32(&guid, 1);
    if (b->m_mode != ReconBuffer__Mode::kRead) return;
    *selector = GetSelectorByGUID(guid);
    return;
  }
  short id = 0;
  if (b->m_mode != ReconBuffer__Mode::kRead) {
    id = (*selector)->m_saveId;
    b->Recon16(&id, 1);
    return;
  }
  b->Recon16(&id, 1);
  *selector = nullptr;
  if (id >= 0 && static_cast<unsigned int>(id) < m_saveGuids.size()) {
    const unsigned int guid = m_saveGuids[static_cast<unsigned int>(id)];
    ObjSelector** chain = &m_selectors[guid & 0xff];
    ObjSelector* prev = nullptr;
    for (ObjSelector* s = *chain; s; prev = s, s = s->m_next) {
      if (s->GetGUID() != guid) continue;
      if (prev) {
        prev->m_next = s->m_next;
        s->m_next = *chain;
        *chain = s;
      }
      *selector = s;
      break;
    }
  }
  if (*selector) return;
  // GC: the saved type's GUID is looked for among the unknown ones (the placeholder either way).
  *selector = GetPlaceholder();
}

// 800EED2C
// A behaviour (a frame's trees) by its owner: old saves by GUID, then by the type's save id (read: the
// found selector moves to the front of its chain; -1 or unknown ids give none). PC: a selector's
// behaviour is Behavior::For (GC: ObjSelector +0x0C).
void ObjectFolderImpl::ReconBehavior(Behavior** behavior, ReconBuffer* b, int version) {
  if (version < 0x2f) {
    int guid = 0;
    if (b->m_mode != ReconBuffer__Mode::kRead) {
      ObjSelector* owner = *behavior ? (*behavior)->GetOwner() : nullptr;
      guid = owner ? static_cast<int>(owner->GetGUID()) : 0;
    }
    b->Recon32(&guid, 1);
    if (b->m_mode != ReconBuffer__Mode::kRead) return;
    ObjSelector* s = GetSelectorByGUID(guid);
    *behavior = s ? Behavior::For(s) : nullptr;
    return;
  }
  if (b->m_mode != ReconBuffer__Mode::kRead) {
    ObjSelector* owner = *behavior ? (*behavior)->GetOwner() : nullptr;
    short id = owner ? owner->m_saveId : 0;
    b->Recon16(&id, 1);
    return;
  }
  short id = 0;
  b->Recon16(&id, 1);
  *behavior = nullptr;
  if (id < 0 || static_cast<unsigned int>(id) >= m_saveGuids.size()) return;
  const unsigned int guid = m_saveGuids[static_cast<unsigned int>(id)];
  ObjSelector** chain = &m_selectors[guid & 0xff];
  ObjSelector* prev = nullptr;
  for (ObjSelector* s = *chain; s; prev = s, s = s->m_next) {
    if (s->GetGUID() != guid) continue;
    if (prev) {
      prev->m_next = s->m_next;
      s->m_next = *chain;
      *chain = s;
    }
    *behavior = Behavior::For(s);
    return;
  }
}

namespace {
// PC: the tree versions of each type as the house's table last listed them. GC: ObjSelector::
// GetInitTreeVersion / GetMainTreeVersion (80111194, 801111FC) sum the behaviour's tree versions
// (Behavior::GetCumulativeTreeVersion, not ported); a type new to the house gets 0.
std::map<unsigned int, std::pair<int, int>> s_savedTreeVersions;
void SelectorTreeVersions(ObjSelector& s, int& initTree, int& mainTree) {
  const auto it = s_savedTreeVersions.find(s.GetGUID());
  initTree = it != s_savedTreeVersions.end() ? it->second.first : 0;
  mainTree = it != s_savedTreeVersions.end() ? it->second.second : 0;
}
}  // namespace

// 800F03E4
// Before the house's objects are saved: each type with objects in the house gets a save id (from 1, in
// chain order; the id -> GUID table rebuilt, entry 0 unused), the others 0; then the type table as the
// house's 'objt' resource (version 2). PC: GC counts each selector's objects as they are made (+0x68);
// the port counts the module's objects here.
void ObjectFolderImpl::PrepareForModuleSave(iResFile* file) {
  std::map<const ObjSelector*, int> counts;
  if (Globs_pObjectModule)
    for (cXObjectImpl* o = Globs_pObjectModule->m_first; o; o = o->m_nextObject) ++counts[o->GetSelector()];
  m_saveGuids.assign(1, 0u);
  short next = 1;
  for (ObjSelector* chain : m_selectors)
    for (ObjSelector* s = chain; s; s = s->m_next) {
      const auto it = counts.find(s);
      if (it == counts.end() || it->second < 1) {
        s->m_saveId = 0;
        continue;
      }
      s->m_saveId = next++;
      m_saveGuids.push_back(s->GetGUID());
    }
  ObjectSaveTypeTable table;
  table.m_folder = this;
  ReconSaveObject(&table, file, kObjectTypeTableResType, 0, 2);  // GC: id DAT_804FDCC2 (0)
}

// 800EFDCC
// The house's type table: every selector's save id is cleared, the 'objt' resource read (setting the
// ids of the types it lists), then the id -> GUID table built.
void ObjectFolderImpl::PrepareForModuleLoad(iResFile* file) {
  for (ObjSelector* chain : m_selectors)
    for (ObjSelector* s = chain; s; s = s->m_next) s->m_saveId = 0;
  m_unknownGuids.clear();
  ObjectSaveTypeTable table;
  table.m_folder = this;
  ReconLoadObject(&table, file, kObjectTypeTableResType, 0, nullptr);  // GC: id DAT_804FDCC2 (0)
  m_saveGuids.assign(1, 0u);
  for (ObjSelector* chain : m_selectors)
    for (ObjSelector* s = chain; s; s = s->m_next) {
      const auto id = static_cast<unsigned short>(s->m_saveId);
      if (!id) continue;
      if (m_saveGuids.size() <= id) m_saveGuids.resize(id + 1u, 0u);
      m_saveGuids[id] = s->GetGUID();
    }
}

// 800EC08C
// Read: GUID, the tree versions (version 1 on), the save id, the type (version 2 on) and the name, until
// a zero GUID. A known type takes its save id (and is marked when its trees changed); unknown persons
// are remembered. GC: writing lists every selector with a save id.
void ObjectSaveTypeTable::DoStream(ReconBuffer* b, int version) {
  if (b->m_mode != ReconBuffer__Mode::kRead) {
    // Written: every selector with a save id (PrepareForModuleSave numbered them), in chain order, then
    // a zero GUID. GC: the name is the catalogue resource's (BString::assignDebug) when the definition's
    // +0x14 is 0 or +0x16 is -1; nothing reads it, the port writes it empty.
    for (ObjSelector* chain : m_folder->m_selectors)
      for (ObjSelector* s = chain; s; s = s->m_next) {
        if (!s->m_saveId) continue;
        int guid = static_cast<int>(s->GetGUID());
        int initTree = 0, mainTree = 0;
        SelectorTreeVersions(*s, initTree, mainTree);
        short saveId = s->m_saveId;
        short type = static_cast<short>(objdef::Type(s->m_def));
        std::string name;
        b->Recon32(&guid, 1);
        b->Recon32(&initTree, 1);
        b->Recon32(&mainTree, 1);
        b->Recon16(&saveId, 1);
        b->Recon16(&type, 1);
        b->ReconString(name);
      }
    int end = 0;
    b->Recon32(&end, 1);
    return;
  }
  int guid = 0;
  b->Recon32(&guid, 1);
  while (guid) {
    int initTree = 0, mainTree = 0;
    if (version >= 1) {
      b->Recon32(&initTree, 1);
      b->Recon32(&mainTree, 1);
    }
    short saveId = 0, type = -1;
    b->Recon16(&saveId, 1);
    if (version >= 2) b->Recon16(&type, 1);
    // The name (GC: ReconString into a BString): the text, its NUL and a pad to an even position.
    if (b->m_compressed) b->PadBits();
    while (b->m_pos < b->m_size && b->m_data[b->m_pos]) ++b->m_pos;
    ++b->m_pos;
    if (b->m_pos & 1) ++b->m_pos;
    if (ObjSelector* s = m_folder->GetSelectorByGUID(guid)) {
      s->m_saveId = saveId;
      s_savedTreeVersions[s->GetGUID()] = {initTree, mainTree};
      SIMMY_STUB("ObjectSaveTypeTable::DoStream: tree versions (ObjSelector::GetInitTreeVersion)", 0x800EC08C);
      s->m_flags &= ~2u;
    } else if (type == 2) {
      m_folder->m_unknownGuids.push_back(static_cast<unsigned int>(guid));
    }
    b->Recon32(&guid, 1);
  }
}

// ---------------------------------------------------------------------------------------------------
// User selectors: the CAS-made sims

namespace {
constexpr int kUserResType = 0x55736572;  // 'User'
constexpr int kTypeAttrResType = 0x54415454;  // 'TATT'

std::u16string FromLoc(const unsigned short* s) {
  std::u16string out;
  for (; s && *s; ++s) out.push_back(static_cast<char16_t>(*s));
  return out;
}
std::u16string FromBuffer(const unsigned short* s, int n) {
  std::u16string out;
  for (int i = 0; i < n && s[i]; ++i) out.push_back(static_cast<char16_t>(s[i]));
  return out;
}
}  // namespace

// 8003FF0C (game/eglobal.cpp)
// The "RelationshipTable" row (0x18 bytes: +0 GUID, +0x10 and +0x14 the localised first and last
// name) of the story sim `guid`, or none.
RET_UNKNOWN EGlobal::GetRelationshipTableRow(unsigned int guid) {
  if (!guid || !m_strings) return 0;
  const void* table = m_strings->getTable(const_cast<char*>("RelationshipTable"));
  if (!table) return 0;
  const unsigned int n = ERQuickdata::TableRowCount(table);
  for (unsigned int i = 0; i < n; ++i) {
    const void* row = ERQuickdata::TableRow(table, i);
    if (ERQuickdata::U32(row, 0) == guid) return reinterpret_cast<RET_UNKNOWN>(row);
  }
  return 0;
}

// 800ED964
// A selector for the CAS-made sim `guid` (none when it exists): a copy of the template person's
// definition with its GUID and 'User' resource id, first in its GUID chain. GC: also its XObjLang,
// the definition's data and a type attribute block when the template has one.
ObjSelector* ObjectFolderImpl::AddUserSelector(int guid, short resId) {
  if (guid == 0 || GetSelectorByGUID(guid)) return nullptr;
  auto* selector = new ObjSelector;  // GC: 0xA8 bytes
  selector->m_def = m_templateDef;
  selector->m_user = true;
  selector->m_userGuid = static_cast<unsigned int>(guid);
  selector->m_userResId = resId;
  int count = 0;
  for (ObjSelector* chain : m_selectors)
    for (ObjSelector* s = chain; s; s = s->m_next) ++count;
  selector->m_index = count;
  selector->m_folder = this;
  selector->m_row = m_templatePersonFile;
  selector->m_fileName = "template_person";  // GC: +0x14 "UserDefined", +0x18 "template_person"
  selector->m_flags = (selector->m_flags & ~0xcu) | 0x800004u;
  if (m_templateDef && ERQuickdata::S16(m_templateDef, 0xb4)) {
    auto* block = new TypeAttrBlock;
    block->m_guid = guid;
    block->m_values.assign(static_cast<size_t>(ERQuickdata::S16(m_templateDef, 0xb4)), 0);
    m_typeAttrBlocks.push_back(block);
  }
  const unsigned int slot = static_cast<unsigned int>(guid) & 0xff;
  selector->m_next = m_selectors[slot];
  m_selectors[slot] = selector;
  return selector;
}

// 800ECB6C
// The selector after `after` (the first with its GUID) in the table's order; the first one for none.
ObjSelector* ObjectFolderImpl::GetNextSelector(ObjSelector* after) {
  unsigned int slot = 0;
  if (after) {
    const unsigned int guid = after->GetGUID();
    slot = guid & 0xff;
    ObjSelector* s = m_selectors[slot];
    while (s && s->GetGUID() != guid) s = s->m_next;
    if (!s) return nullptr;
    if (s->m_next) return s->m_next;
    ++slot;
  }
  for (; slot < 0x100; ++slot)
    if (m_selectors[slot]) return m_selectors[slot];
  return nullptr;
}

// 800F239C
// The GUID and the look; read back, a story sim's names come from its "RelationshipTable" row.
// (GC also formats "<root>DATA/sim-<guid>.txt", unused.)
void UserDataSaveLoad::DoStream(ReconBuffer* b, int version) {
  b->Recon32(&m_guid, 1);
  m_desc.DoStream(b, version);
  if (b->m_mode != ReconBuffer__Mode::kRead) return;
  if (const void* row = reinterpret_cast<const void*>(_globals.GetRelationshipTableRow(static_cast<unsigned int>(m_guid)))) {
    // wcsncpy(.., 0x20): copied up to the 0, the rest zeroed.
    auto copy = [](unsigned short* dst, const unsigned short* src) {
      int i = 0;
      for (; src && i < 0x20 && src[i]; ++i) dst[i] = src[i];
      for (; i < 0x20; ++i) dst[i] = 0;
    };
    copy(m_desc.m_firstName, ERQuickdata::LocString(row, 0x10));
    copy(m_desc.m_lastName, ERQuickdata::LocString(row, 0x14));
  }
}

// 800EF184
// Every 'User' resource becomes a user selector with its look and names (a story sim's from the
// "RelationshipTable"); the highest resource id is remembered for new ones.
void ObjectFolderImpl::LoadUserData(iResFile* file) {
  const int n = file->Count(kUserResType);
  m_lastUserResId = 0;
  for (int i = 1; i <= n; ++i) {
    Memory__HandleNode* handle = file->GetByIndex(kUserResType, static_cast<short>(i), nullptr);
    if (!handle) continue;
    short id = 0;
    file->GetID(handle, &id);
    if (id > m_lastUserResId) m_lastUserResId = id;
    UserDataSaveLoad data;
    ReconBuilder builder;
    struct Adapter : ReconObject {
      UserDataSaveLoad* m_data;
      explicit Adapter(UserDataSaveLoad* d) : m_data(d) {}
      void DoStream(ReconBuffer* b, int v) override { m_data->DoStream(b, v); }
      int GetType() override { return kUserResType; }
    } adapter(&data);
    builder.Reconstitute(&adapter, handle, nullptr);
    ObjSelector* selector = AddUserSelector(data.m_guid, id);
    if (!selector) continue;
    if (const void* row = reinterpret_cast<const void*>(_globals.GetRelationshipTableRow(static_cast<unsigned int>(data.m_guid)))) {
      selector->SetUserName(FromLoc(ERQuickdata::LocString(row, 0x10)));
      selector->SetUserLastName(FromLoc(ERQuickdata::LocString(row, 0x14)));
    } else {
      selector->SetUserName(FromBuffer(data.m_desc.m_firstName, 0x20));
      selector->SetUserLastName(FromBuffer(data.m_desc.m_lastName, 0x20));
    }
    selector->m_simDesc = new CasSimDescriptionS2C(data.m_desc);
  }
}

// 800EF3E0
// The guid is a member of a story family 1..4 living in a house, or of a free-play family 20..27
// (`family` its number).
RET_UNKNOWN IsPlayableCharacter(int guid, int& family) {
  family = 0;
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  if (!hood) return 0;
  for (int n = 1; n < 5; ++n) {
    FamilyImpl* f = hood->GetFamily(n);
    if (f && f->TestMember(guid)) {
      if (f->GetHouseNumber() == 0) return 0;
      family = n;
      return 1;
    }
  }
  for (int n = 0x14; n <= 0x1b; ++n) {
    FamilyImpl* f = hood->GetFamily(n);
    if (f && f->TestMember(guid)) {
      family = n;
      return 1;
    }
  }
  return 0;
}

// 800EF4FC
// The user selectors with a look, in resource id order, as 'User' resources 1, 2, ...; each one of a
// playable family also writes its portrait as the family slot's 'thmb' (ThumbnailLoader) and the slot
// remembers the sim's GUID.
void ObjectFolderImpl::SaveUserData(iResFile* file) {
  std::vector<ObjSelector*> users;
  for (ObjSelector* s = GetNextSelector(nullptr); s; s = GetNextSelector(s))
    if (s->GetSimDescription()) users.push_back(s);
  std::stable_sort(users.begin(), users.end(),
                   [](ObjSelector* a, ObjSelector* b) { return a->m_userResId < b->m_userResId; });  // ObjSelResIDLessThan
  short id = 1;
  for (ObjSelector* s : users) {
    UserDataSaveLoad data;
    data.m_guid = static_cast<int>(s->GetGUID());
    data.m_desc = *s->GetNormalSimDescription();
    ReconSaveObject(&data, file, kUserResType, id, Globs_iSaveFileVersion);
    ++id;
    // A playable family's member: its portrait as the family slot's 'thmb', the slot remembering whose.
    const int guid = static_cast<int>(s->GetGUID());
    int family = 0, index = 0;
    if (Globs_pNeighborhood && Globs_pNeighborhood->FindFamilyAndIndexByGUID(guid, family, index) &&
        IsPlayableCharacter(guid, family) && Globs_pNghResFile) {
      ThumbnailLoader loader;
      loader.m_selector = s;
      const short thumbId = static_cast<short>(Globs_pNghResFile->GetThumbIdFromFamilyAndIndex(family, index));
      if (ReconSaveObject(&loader, file, 0x74686d62, thumbId, Globs_iSaveFileVersion) == 0)  // 'thmb'
        Globs_pNghResFile->StoreFamilyThumbGUID(family, index, static_cast<unsigned int>(guid));
    }
  }
}

// 800EF9E8
// A new CAS-made sim: a new GUID, the next 'User' id, a default look.
ObjSelector* ObjectFolderImpl::CreateNewUserSelector() {
  static unsigned int s_next = 0x70000000u;
  SIMMY_STUB("ObjectFolderImpl::CreateNewUserSelector: StubObject::MakeNewGUID", 0x800EF9E8);
  unsigned int guid = ++s_next;
  while (GetSelectorByGUID(static_cast<int>(guid))) guid = ++s_next;
  ObjSelector* selector = AddUserSelector(static_cast<int>(guid), ++m_lastUserResId);
  if (selector) selector->m_simDesc = new CasSimDescriptionS2C;
  return selector;
}

// 800F0B18
// The user selectors go (GC: through DestroySelector, with their data and type attribute blocks).
void ObjectFolderImpl::DeleteUserSelectors() {
  for (ObjSelector*& chain : m_selectors) {
    ObjSelector** link = &chain;
    while (*link) {
      ObjSelector* s = *link;
      if (!s->m_user) {
        link = &s->m_next;
        continue;
      }
      *link = s->m_next;
      const int guid = static_cast<int>(s->GetGUID());
      auto it = std::find_if(m_typeAttrBlocks.begin(), m_typeAttrBlocks.end(),
                             [guid](TypeAttrBlock* b) { return b->m_guid == guid; });
      if (it != m_typeAttrBlocks.end()) {
        delete *it;
        m_typeAttrBlocks.erase(it);
      }
      delete s;
    }
  }
}

// 800F1264
ObjectFolderImpl::TypeAttrBlock* ObjectFolderImpl::GetTypeAttrBlock(int guid) {
  for (TypeAttrBlock* b : m_typeAttrBlocks)
    if (b->m_guid == guid) return b;
  return nullptr;
}

// 800F129C
void ObjectFolderImpl::Save(iResFile* file) { ReconSaveObject(this, file, kTypeAttrResType, 1, 0); }
// 800F12CC
void ObjectFolderImpl::Load(iResFile* file) { ReconLoadObject(this, file, kTypeAttrResType, 1, nullptr); }

// 800F12FC
// 'TATT': compressed; the number of blocks, then per block its GUID, value count and values (the
// values of types the database lacks are skipped; fewer values read than held are zeroed).
void ObjectFolderImpl::DoStream(ReconBuffer* b, int) {
  int compressed = 1;
  b->ReconBool(&compressed);
  if (compressed) b->EnableCompression();
  int count = static_cast<int>(m_typeAttrBlocks.size());
  b->ReconInt(&count, 1);
  unsigned int index = 0;
  for (int i = 0; i < count; ++i) {
    int guid = 0;
    if (b->m_mode != ReconBuffer__Mode::kRead) guid = m_typeAttrBlocks[index]->m_guid;
    b->Recon32(&guid, 1);
    TypeAttrBlock* block = GetTypeAttrBlock(guid);
    if (b->m_mode == ReconBuffer__Mode::kRead && !block) {
      int n = 0;
      b->ReconInt(&n, 1);
      std::vector<short> skipped(static_cast<size_t>(n > 0 ? n : 0));
      if (n > 0) b->Recon16(skipped.data(), n);
      continue;
    }
    index = (index + 1) & 0xffff;
    if (!block) continue;
    const int held = static_cast<int>(block->m_values.size());
    int n = held;
    b->ReconInt(&n, 1);
    if (n > held) block->m_values.resize(static_cast<size_t>(n));
    if (n > 0) b->Recon16(block->m_values.data(), n);
    for (int k = n; k < held; ++k) block->m_values[static_cast<size_t>(k)] = 0;
  }
}
