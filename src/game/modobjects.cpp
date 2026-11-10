// PC mod: mods. The objects the mods add to the object database (see platform.hpp, pc_mods.cpp).
//
// A mod object is a copy of a GameCube object (its "base"): its ObjDefinition row with the mod's GUID
// and price, its ResData with the mod's thumbnail, and its ObjAnimDef table with the mod's model in
// every state (state animations, shader swaps, particles and the animated character dropped: they
// belong to the base's model). The copies live in the "SimsObjects" quickdata's mod tail
// (ERQuickdata::ModTail), so their self-relative pointers reach the image like the originals'. The
// selector shares the base's object file (ResFile row): its trees, tree tables, slots, catalogue
// function and semi-globals, so it behaves exactly like the base. Only single-tile bases with a
// ResData are taken.
//
// Saves name an object type by GUID ('objt'), so a lot keeps its mod objects while the mod is there;
// without the mod the GUID is unknown and the object loads as the placeholder, which the load removes
// (ObjectModuleImpl::DoStream), as the GameCube does for any missing object type.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace {
constexpr unsigned int kDefSize = 0xc4;      // an ObjDefinition row (pointer at +0xC0: ResData)
constexpr unsigned int kResDataSize = 0x38;  // ResData (pointer at +0x00: the ObjAnimDef table)
constexpr unsigned int kAnimDefSize = 0x20;  // ObjAnimDef (no pointers)

void PutBE32(unsigned char* p, unsigned int v) {
  p[0] = static_cast<unsigned char>(v >> 24), p[1] = static_cast<unsigned char>(v >> 16);
  p[2] = static_cast<unsigned char>(v >> 8), p[3] = static_cast<unsigned char>(v);
}
void PutBE16(unsigned char* p, unsigned int v) {
  p[0] = static_cast<unsigned char>(v >> 8), p[1] = static_cast<unsigned char>(v);
}
// A self-relative pointer word at `field` to `target` (both in the same image allocation).
void PutPtr(unsigned char* field, const void* target) {
  const long long rel = target ? static_cast<const unsigned char*>(target) - field : 0;
  PutBE32(field, static_cast<unsigned int>(static_cast<int>(rel)));
}
std::string Narrow(const std::u16string& s) {
  std::string out;
  for (char16_t c : s) out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
  return out;
}

ObjSelector* FindBase(ObjectFolderImpl* folder, const simmy::ModObject& m) {
  if (m.baseGuid) return folder->GetSelectorByGUID(static_cast<int>(m.baseGuid));
  for (ObjSelector* s = folder->GetNextSelector(nullptr); s; s = folder->GetNextSelector(s)) {
    if (s->m_mod || !s->m_def || !s->m_fileName || s->m_fileName != m.baseFile) continue;
    if (objdef::MasterID(s->m_def) == 0 && objdef::ResData(s->m_def)) return s;
  }
  return nullptr;
}
}  // namespace

void ObjectFolderImpl::AddModObjects() {
  if (!simmy::mods_enabled() || !m_objects) return;
  unsigned char* const tail = m_objects->ModTail();
  if (!tail) return;
  const void* resFiles = m_objects->getTable("ResFile");
  if (!resFiles) return;
  unsigned int used = 0;
  for (const simmy::ModObject& m : simmy::mods_objects()) {
    const std::string what = m.mod + " " + Narrow(m.name);
    if (GetSelectorByGUID(static_cast<int>(m.guid))) {
      std::fprintf(stderr, "[mods] %s: GUID %08X is taken; skipped\n", what.c_str(), m.guid);
      continue;
    }
    ObjSelector* base = FindBase(this, m);
    if (!base || !base->m_def || base->m_mod) {
      std::fprintf(stderr, "[mods] %s: base object \"%s\" (%08X) not found; skipped\n", what.c_str(), m.baseFile.c_str(), m.baseGuid);
      continue;
    }
    const auto* baseDef = reinterpret_cast<const unsigned char*>(base->m_def);
    const unsigned char* baseRes = objdef::ResData(base->m_def);
    const unsigned char* baseAnims = objdef::AnimDefs(baseRes);
    const unsigned int anims = objdef::AnimDefCount(baseAnims);
    if (objdef::MasterID(base->m_def) != 0 || !baseRes || objdef::Type(base->m_def) == 2 || objdef::Type(base->m_def) == 8) {
      std::fprintf(stderr, "[mods] %s: base \"%s\" is not a single-tile object with a model; skipped\n", what.c_str(),
                   base->m_fileName);
      continue;
    }
    const unsigned int need = kDefSize + kResDataSize + 4 + (anims ? anims : 1) * kAnimDefSize;
    if (used + need > ERQuickdata::kModTail) {
      std::fprintf(stderr, "[mods] %s: too many mod objects; skipped\n", what.c_str());
      break;
    }
    unsigned char* def = tail + used;
    unsigned char* res = def + kDefSize;
    unsigned char* animCount = res + kResDataSize;
    unsigned char* animDefs = animCount + 4;
    used += need;
    // The definition: the base's, with the mod's GUID, price and ResData.
    std::memcpy(def, baseDef, kDefSize);
    PutBE32(def + 0x1c, m.guid);
    if (m.price >= 0) PutBE16(def + 0x24, static_cast<unsigned int>(m.price > 0x7fff ? 0x7fff : m.price));
    PutPtr(def + 0xc0, res);
    // ResData: the mod's thumbnail; no animated character (+0x20: its skeleton is the base model's).
    std::memcpy(res, baseRes, kResDataSize);
    PutPtr(res, animDefs);
    if (m.thumbnail) PutBE32(res + 0x1c, m.thumbnail);
    if (m.model) PutBE32(res + 0x20, 0);
    // The state table: every state shows the mod's model, without the base's animation, effects and
    // shader swaps (they name the base model's parts).
    const unsigned int n = anims ? anims : 1;
    PutBE32(animCount, n);
    for (unsigned int i = 0; i < n; ++i) {
      unsigned char* d = animDefs + i * kAnimDefSize;
      if (anims) std::memcpy(d, baseAnims + i * kAnimDefSize, kAnimDefSize);
      else PutBE32(d + 0x18, 0xffffffffu);
      if (!m.model) continue;
      PutBE32(d + 0x00, m.model);
      PutBE32(d + 0x04, 0);
      PutBE32(d + 0x08, 0);
      PutBE32(d + 0x0c, 0);
      PutBE32(d + 0x10, 0);
    }
    const int added = AddSelector(reinterpret_cast<ObjDefinition*>(def), const_cast<char*>(base->m_fileName), const_cast<char*>(base->m_fileName),
                reinterpret_cast<ResFile*>(const_cast<void*>(base->m_resFile)), ERQuickdata::S16(def, 0xbc), nullptr);
    ObjSelector* s = GetSelectorByGUID(static_cast<int>(m.guid));
    if (!added || !s) continue;
    s->m_mod = m.mod.c_str();
    s->m_modName = m.name;
    s->m_modDesc = m.desc;
    if (!s->m_modName.empty()) s->m_modNameSlot = reinterpret_cast<const unsigned short*>(s->m_modName.c_str());
    if (!s->m_modDesc.empty()) s->m_modDescSlot = reinterpret_cast<const unsigned short*>(s->m_modDesc.c_str());
    std::fprintf(stderr, "[mods] %s: object %08X (a copy of %s %08X), price %d, model %08X\n", what.c_str(), m.guid,
                 base->m_fileName, objdef::GUID(base->m_def), ERQuickdata::S16(def, 0x24), m.model);
  }
}
