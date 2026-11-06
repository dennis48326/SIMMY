// Port of game/skilllookup.cpp (partial): the standard animation tables people play (idles, walks,
// runs, turns, ...) and GetStdAnimRef. Each table is an AnimRefTable resource of the global object
// file: a list of AnimRef records (the first word of each is the animation's id) by StdAnimIdx.
// Tables (sTables): 0 0x96 everyone, 1 0x97, 2 0x98 adult male, 3 0x99 adult female, 4..9 0x9A 0x9B
// 0x82 0x80 0x9C 0x9D, 10..12 0x9E..0xA0 (person data 0x40), 13..15 0xB3..0xB5 dog, cat, monkey.
// GC: AnimTable / AnimTableImpl wrap each table (AnimTableImpl::Load finds the table in the global
// QuickResFile's AnimRefTable list, GetEntry reads one record); PC: the tables are the quickdata
// records themselves, read from the "SimsObjects" quickdata's global ResFile row (+0x04: the
// AnimRefTable list, 8 bytes each: the records' array and the table's id), as LoadDatabase reads it.
// The animate primitive's lookups: an object's own tables (AnimRefTable 0x81 adult, 0x82 child, 0x83
// adult to child, 0x84 child to adult of the object's file; GC: ObjSelector +0x40..+0x4C made on first
// use), the stock (0x82), global (0x80) and misc (0x9C) tables of the global file. Not ported: reach
// and passive influence lookups.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <map>

namespace {
constexpr int kTableCount = 16;
// The AnimRefTable ids InitSkillLookup loads, by table.
constexpr short kTableIds[kTableCount] = {0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x82, 0x80,
                                          0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xb3, 0xb4, 0xb5};
// One table: its records' array (a count at [-4], then a pointer per StdAnimIdx), or null.
const unsigned char* sTables[kTableCount] = {};

// The global object file's row of the "ResFile" table (GC: ObjectFolder::GetFileByID(fGlobalFileID)).
const unsigned char* GlobalResFileRow() {
  if (!Globs_pObjectFolder || !Globs_pObjectFolder->m_objects) return nullptr;
  const ERQuickdata* qd = Globs_pObjectFolder->m_objects;
  const void* globals = qd->getTable("GlobalResFile");
  const void* row = globals ? qd->getRow(globals, "global") : nullptr;
  return row ? static_cast<const unsigned char*>(ERQuickdata::Ptr(row, 0)) : nullptr;
}

// AnimTableImpl::Load: (GC 800AAB3C) the global file's AnimRefTable `id` (the list is sorted by id).
const unsigned char* LoadTable(short id, const unsigned char* file = GlobalResFileRow()) {
  const auto* list = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 4)) : nullptr;
  if (!list) return nullptr;
  const unsigned int count = ERQuickdata::U32(list - 4, 0);
  for (unsigned int i = 0; i < count; ++i)
    if (ERQuickdata::S16(list, i * 8 + 4) == id) return static_cast<const unsigned char*>(ERQuickdata::Ptr(list, i * 8));
  return nullptr;
}

// AnimTableImpl::GetEntry: (GC 800AABC4) the table's record for `index`, or null.
const AnimRef* GetEntry(const unsigned char* table, int index) {
  if (!table || index < 0 || static_cast<unsigned int>(index) >= ERQuickdata::U32(table - 4, 0)) return nullptr;
  const void* slot = ERQuickdata::Ptr(table, static_cast<unsigned int>(index) * 4);
  return slot ? static_cast<const AnimRef*>(ERQuickdata::Ptr(slot, 0)) : nullptr;
}
}  // namespace

unsigned int AnimRefId(const AnimRef* ref) { return ref ? ERQuickdata::U32(ref, 0) : 0u; }

// AnimTable::CreateInstance / AnimTableImpl::Load(namespace, id) / GetEntry (GC 800AAA84, GC 800AAB3C,
// GC 800AABC4): record `index` of AnimRefTable `id` of the namespace's file (its ResFile row), or null.
const AnimRef* AnimTableEntry(const unsigned char* resFile, short id, int index) {
  return resFile ? GetEntry(LoadTable(id, resFile), index) : nullptr;
}

// 80141C80
// The sixteen tables, from the global object file.
void InitSkillLookup() {
  for (int i = 0; i < kTableCount; ++i) sTables[i] = LoadTable(kTableIds[i]);
}

// 8014226C
// The person's standard animation `idx`: people with data 0x40 set use tables 10..12 (by sex,
// children 12); pets their species' table; then the adult table of the person's sex, then the shared
// one. GC: the pet checks are cXPerson's IsDog/IsCat/IsMonkey (vtable 0x1DC, 0x1E4, 0x1EC); PC:
// people only (pets are not ported).
int GetStdAnimRef(cXPerson* person, int idx, const AnimRef*& out) {
  out = nullptr;
  if (!person) return 1;
  CasSimDescriptionS2C* desc = person->GetSimDescription();
  const int female = desc && !desc->m_male ? 1 : 0;
  if (person->GetPersonData(0x40)) out = GetEntry(sTables[10 + female], idx);
  if (!out) out = GetEntry(sTables[2 + female], idx);
  if (!out) out = GetEntry(sTables[0], idx);
  return 1;
}

namespace {
// ReportMissingAnimation (GC 80141BA8 / 80141C14): logged once per table and index.
int Missing(const char* table, int index) {
  static int logged = 0;
  if (logged < 30) {
    ++logged;
    std::printf("[anim] missing animation %s #%d\n", table, index);
  }
  return -1;
}
}  // namespace

// 80142500
int GetMiscAnimRef(cXPerson* person, int index, const AnimRef*& out) {
  out = GetEntry(sTables[8], index);
  return out || !person ? 1 : Missing("misc", index);
}

// 8014282C
int GetPersonStockAnimRef(cXPerson* person, int index, const AnimRef*& out) {
  out = GetEntry(sTables[6], index);
  return out || !person ? 1 : Missing("person stock", index);
}

// 801428B8
int GetGlobalAnimRef(cXPerson* person, int index, const AnimRef*& out) {
  out = GetEntry(sTables[7], index);
  return out || !person ? 1 : Missing("global", index);
}

// 8014258C
// `object`'s animation `index` for `person`: the object's adult table, or for children (and for an
// interaction's own object, `ownObject`, by the icon object's age) the child or cross-age tables.
// PC: people are adults (IsChild is not ported), so the adult table (0x81).
int GetObjectAnimRef(cXObject* object, cXPerson* person, int index, bool ownObject, const AnimRef*& out) {
  (void)ownObject;
  out = nullptr;
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  const unsigned char* file = impl ? SelectorResFile(impl->m_selector) : nullptr;
  // GC: ObjSelector::GetAdultAnimTable (80111744), made once per selector.
  static std::map<const unsigned char*, const unsigned char*> adult;
  const unsigned char* table = nullptr;
  if (file) {
    auto it = adult.find(file);
    table = it != adult.end() ? it->second : (adult[file] = LoadTable(0x81, file));
  }
  out = GetEntry(table, index);
  return out ? 1 : Missing(impl && impl->m_selector ? impl->m_selector->m_fileName : "object", index);
}
