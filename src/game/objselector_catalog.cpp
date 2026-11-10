// Port of the catalogue part of game/objselector.cpp (ObjSelector) and game/objectfolder.cpp
// (ObjectFolderImpl's multi-tile lookups), and game/catalogresource.cpp (CatalogResourceImpl::Load):
// an object's catalogue name, description and short name, its motive ratings and the lead selector of
// a multi-tile group. PC: the catalogue entry is looked up on each call (GC: a CatalogResource made
// once and kept at ObjSelector +0x80; the master selector cached at +0x60).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
// CatalogResourceImpl::Load (800B1CC8) with FindRes<CatalogData>: the object file's catalogue entry
// (ResFile +0x1C, 0x10 bytes each: +0x02 its id (the definition's +0x52), +0x04 name, +0x08
// description, +0x0C short name) for the selector, else the first; none without entries.
const unsigned char* CatalogEntry(const ObjSelector* s) {
  if (!s || !s->m_def || !s->m_resFile) return nullptr;
  const auto* list = static_cast<const unsigned char*>(ERQuickdata::Ptr(s->m_resFile, 0x1c));
  const unsigned int n = list ? ERQuickdata::U32(list - 4, 0) : 0;
  if (!n) return nullptr;
  const short id = ERQuickdata::S16(s->m_def, 0x52);
  for (unsigned int i = 0; i < n; ++i)
    if (ERQuickdata::S16(list + i * 0x10, 2) == id) return list + i * 0x10;
  return list;
}
// CatalogResourceImpl::pDefault: the empty string's slot.
const unsigned short kEmptyText[1] = {0};
const unsigned short* const kEmptySlot = kEmptyText;
RET_UNKNOWN CatalogSlot(const ObjSelector* s, unsigned int off) {
  const unsigned char* e = CatalogEntry(s);
  const unsigned short* const* slot = e ? ERQuickdata::LocSlot(e, off) : nullptr;
  return reinterpret_cast<RET_UNKNOWN>(slot ? slot : &kEmptySlot);
}
}  // namespace

// 80110828
// GC: the same object file (+0x04). PC: the same "ResFile" row.
bool ObjSelector::TestFromSameFile(ObjSelector* other) const { return other && other->m_resFile == m_resFile; }

// 80110840
// The lead selector of the object's multi-tile group (the object itself when it is not in one).
RET_UNKNOWN ObjSelector::GetMasterSelector() {
  ObjectFolderImpl* folder = m_folder ? m_folder : Globs_pObjectFolder;
  return folder ? folder->GetMasterSelector(this) : reinterpret_cast<RET_UNKNOWN>(this);
}

// 801108F0
// The catalogue name's slot (the master selector's entry).
// PC mod: mods. A mod object's own name (modobjects.cpp) in place of the object file's.
RET_UNKNOWN ObjSelector::GetCatalogName() {
  auto* master = reinterpret_cast<ObjSelector*>(GetMasterSelector());
  if (master && master->m_modNameSlot) return reinterpret_cast<RET_UNKNOWN>(&master->m_modNameSlot);
  return CatalogSlot(master, 4);
}
// 8011094C
RET_UNKNOWN ObjSelector::GetCatalogDescription() {
  auto* master = reinterpret_cast<ObjSelector*>(GetMasterSelector());
  if (master && master->m_modDescSlot) return reinterpret_cast<RET_UNKNOWN>(&master->m_modDescSlot);
  return CatalogSlot(master, 8);
}
// 801109A8
RET_UNKNOWN ObjSelector::GetCatalogShortName() {
  auto* master = reinterpret_cast<ObjSelector*>(GetMasterSelector());
  if (master && master->m_modNameSlot) return reinterpret_cast<RET_UNKNOWN>(&master->m_modNameSlot);
  return CatalogSlot(master, 0xc);
}

// 80111148
// A part of a multi-tile group other than its lead (master id set, sub index not -1).
RET_UNKNOWN ObjSelector::GetIsMultiTileSubObject() {
  return m_def && objdef::MasterID(m_def) != 0 && objdef::SubIndex(m_def) != -1 ? 1 : 0;
}

// 80111264
// Catalogue rating `kind`: 0..6 the motive ratings (definition +0xA4..+0xB0: hunger, comfort, hygiene,
// bladder, energy, fun, room), 7..13 the skill flags (+0xB2 bits 0..6).
RET_UNKNOWN ObjSelector::GetCatalogRating(int kind) {
  if (!m_def) return 0;
  if (kind >= 0 && kind <= 6) return ERQuickdata::S16(m_def, 0xa4 + kind * 2);
  if (kind >= 7 && kind <= 13) return (ERQuickdata::U16(m_def, 0xb2) >> (kind - 7)) & 1;
  return 0;
}

// 800ED028
// The group's lead: the selector from the same file with the same master id whose sub index is -1.
RET_UNKNOWN ObjectFolderImpl::GetMasterSelector(ObjSelector* selector) {
  const short master = objdef::MasterID(selector->m_def);
  if (master == 0) return reinterpret_cast<RET_UNKNOWN>(selector);
  for (ObjSelector* s = GetNextSelector(nullptr); s; s = GetNextSelector(s))
    if (s->TestFromSameFile(selector) && objdef::MasterID(s->m_def) == master && objdef::SubIndex(s->m_def) == -1)
      return reinterpret_cast<RET_UNKNOWN>(s);
  return reinterpret_cast<RET_UNKNOWN>(selector);
}

// 800ECED8
// The group's `n`th part other than the lead, or none.
RET_UNKNOWN ObjectFolderImpl::GetNthSubSelector(ObjSelector* selector, int n) {
  const short master = objdef::MasterID(selector->m_def);
  if (master == 0) return 0;
  for (ObjSelector* s = GetNextSelector(nullptr); s; s = GetNextSelector(s)) {
    if (!s->TestFromSameFile(selector) || objdef::MasterID(s->m_def) != master || objdef::SubIndex(s->m_def) == -1)
      continue;
    if (n == 0) return reinterpret_cast<RET_UNKNOWN>(s);
    --n;
  }
  return 0;
}

// ---------------------------------------------------------------------------------------------------
// The story sims' portraits (game/objselector.cpp)

// 80110C20
// The story sim's portrait shader (its "RelationshipTable" row: +8 the big portrait, else +4 the icon),
// or 0 for a sim not in the table.
RET_UNKNOWN GetNpcShaderId(int guid, bool big) {
  const void* row = reinterpret_cast<const void*>(_globals.GetRelationshipTableRow(static_cast<unsigned int>(guid)));
  if (!row) return 0;
  const unsigned int id = big ? ERQuickdata::U32(row, 8) : 0;
  return id ? id : ERQuickdata::U32(row, 4);
}

// 80110C80
// A story sim's big portrait (the shader held; GC: its two layers' textures are touched in, vtable 0x5C);
// else the sim's own thumbnail (GetThumbnail).
RET_UNKNOWN ObjSelector::GetBigThumbnail(ERShader** out) {
  const int id = static_cast<int>(GetNpcShaderId(static_cast<int>(GetGUID()), true));
  if (id == 0) return GetThumbnail(out) ? 1 : 0;
  if (out) *out = static_cast<ERShader*>(_shaderman.AddRef(static_cast<unsigned int>(id), nullptr, 0));
  return 1;
}

// ObjSelector::GetThumbnail (80110D44): thumbnail.cpp

// 80111004
// The thumbnail let go (while referenced).
void ObjSelector::DestroyThumbnail() {
  if (m_thumbnail && m_thumbnail->m_refs != 0) {
    m_thumbnail->DelRef();
    m_thumbnail = nullptr;
  }
}
