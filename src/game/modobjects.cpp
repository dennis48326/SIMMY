// PC mod: mods. The objects the mods add to the object database (see platform.hpp, pc_mods.cpp).
//
// A mod object is a copy of a GameCube object (its "base"): its ObjDefinition row with the mod's GUID,
// price, catalogue functions and room rating, its ResData with the mod's thumbnail, and its ObjAnimDef
// table with the mod's model in every state (state animations, shader swaps, particles and the
// animated character dropped: they belong to the base's model). The copies live in the "SimsObjects"
// quickdata's mod tail (ERQuickdata::ModTail, sized from the mods), so their self-relative pointers
// reach the image like the originals'. The selector shares the base's object file (ResFile row): its
// trees, tree tables, slots, catalogue function and semi-globals, so it behaves like the base.
//
// Multi-tile objects (mod.ini "tiles"): a group made from the base, as the GameCube's own groups are: a
// lead (sub index -1, the catalogue entry: price, name, thumbnail; never put in the world) and one part
// per tile (sub index x << 8 | y), all with a master id of their own in the base's object file. The
// first tile draws the model (the GameCube draws a group's model at the middle of its tiles); the
// other tiles have no ResData. From a single-tile base every part is a copy of the base (each runs its
// trees, as the parts of the GameCube's two-tile paintings do); from a group base (a rug) the lead is
// the group's lead and the parts its parts.
//
// Surfaces (mod.ini "surface", per tile in "tiles"): the selectors keep their mod object and the height
// of their top; the base's object slots (xrouting.cpp ObjectSlotDescriptor) put things there, and a
// counter's pieces (corner, ends) are drawn by its neighbours (ISimsCounterTopObject::SetObjOrient).
//
// Saves name an object type by GUID ('objt'), so a lot keeps its mod objects while the mod is there;
// without the mod the GUID is unknown and the object loads as the placeholder, which the load removes
// (ObjectModuleImpl::DoStream), as the GameCube does for any missing object type.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
constexpr unsigned int kDefSize = 0xc4;      // an ObjDefinition row (pointer at +0xC0: ResData)
constexpr unsigned int kResDataSize = 0x38;  // ResData (pointer at +0x00: the ObjAnimDef table)
constexpr unsigned int kAnimDefSize = 0x20;  // ObjAnimDef (no pointers)
constexpr unsigned int kMaxStates = 48;      // states copied (mods_tail_bytes counts as many; a bathtub has 36)

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

// The base's rows: the definition a single-tile copy (or a group's lead) copies, and for a group base
// its part that draws (has a ResData) and a part that does not.
struct Base {
  ObjSelector* selector = nullptr;  // the base (a group's lead)
  const ObjDefinition* lead = nullptr;
  const ObjDefinition* drawing = nullptr;
  const ObjDefinition* plain = nullptr;
  bool group = false;
};

Base FindBase(ObjectFolderImpl* folder, const simmy::ModObject& m) {
  Base b;
  ObjSelector* found = nullptr;
  if (m.baseGuid) {
    found = folder->GetSelectorByGUID(static_cast<int>(m.baseGuid));
    if (found) found = reinterpret_cast<ObjSelector*>(found->GetMasterSelector());
  } else {
    // a single-tile object of the file with a model, else the file's first group lead
    for (ObjSelector* s = folder->GetNextSelector(nullptr); s; s = folder->GetNextSelector(s)) {
      if (s->m_mod || !s->m_def || !s->m_fileName || s->m_fileName != m.baseFile) continue;
      if (objdef::MasterID(s->m_def) == 0 && objdef::ResData(s->m_def)) {
        found = s;
        break;
      }
      if (!found && objdef::MasterID(s->m_def) != 0 && objdef::SubIndex(s->m_def) == -1) found = s;
    }
  }
  if (!found || !found->m_def || found->m_mod) return b;
  b.selector = found;
  b.lead = found->m_def;
  if (objdef::MasterID(found->m_def) == 0) {
    b.drawing = b.plain = found->m_def;
    return b;
  }
  b.group = true;
  for (int n = 0;; ++n) {
    auto* part = reinterpret_cast<ObjSelector*>(folder->GetNthSubSelector(found, n));
    if (!part) break;
    const unsigned char* res = objdef::ResData(part->m_def);
    if (res && objdef::AnimDefCount(objdef::AnimDefs(res)) && !b.drawing) b.drawing = part->m_def;
    else if (!res && !b.plain) b.plain = part->m_def;
  }
  if (!b.drawing) b.drawing = b.plain;
  if (!b.plain) b.plain = b.drawing;
  return b;
}

const std::vector<std::pair<int, unsigned int>>& NoStates() {
  static const std::vector<std::pair<int, unsigned int>> none;
  return none;
}

// A group base's part at tile (x, y) (sub index x << 8 | y), or null.
const ObjDefinition* BasePart(ObjectFolderImpl* folder, ObjSelector* lead, int x, int y) {
  for (int n = 0;; ++n) {
    auto* part = reinterpret_cast<ObjSelector*>(folder->GetNthSubSelector(lead, n));
    if (!part) return nullptr;
    if ((objdef::SubIndex(part->m_def) & 0xffff) == (x << 8 | y)) return part->m_def;
  }
}

// Mod rows in the tail, one after another.
struct Tail {
  unsigned char* base = nullptr;
  unsigned int used = 0, size = 0;
  unsigned char* Take(unsigned int n) {
    n = (n + 3) & ~3u;
    if (used + n > size) return nullptr;
    unsigned char* p = base + used;
    used += n;
    return p;
  }
};

// A copy of `from` with the mod's GUID, price, functions and room rating, and (withRes) a ResData copy
// with the mod's thumbnail and model in every state; without: no ResData (a part that draws nothing).
unsigned char* CopyDef(Tail& tail, const ObjDefinition* from, const simmy::ModObject& m, unsigned int guid,
                       bool withRes, int room, unsigned int model,
                       const std::vector<std::pair<int, unsigned int>>& states, const unsigned char* animRes = nullptr) {
  const unsigned char* baseRes = objdef::ResData(from);
  // An animated appliance keeps a character and its state table (animations, effects) from animRes.
  const unsigned char* baseAnims = animRes ? objdef::AnimDefs(animRes) : baseRes ? objdef::AnimDefs(baseRes) : nullptr;
  unsigned int anims = objdef::AnimDefCount(baseAnims);
  if (anims > kMaxStates) anims = kMaxStates;
  const unsigned int n = anims ? anims : 1;
  unsigned char* def = tail.Take(kDefSize + (withRes ? kResDataSize + 4 + n * kAnimDefSize : 0));
  if (!def) return nullptr;
  std::memcpy(def, from, kDefSize);
  PutBE32(def + 0x1c, guid);
  if (m.price >= 0) PutBE16(def + 0x24, static_cast<unsigned int>(m.price > 0x7fff ? 0x7fff : m.price));
  if (m.functions >= 0) PutBE16(def + 0x50, static_cast<unsigned int>(m.functions));
  if (room >= 0) PutBE16(def + 0xb0, static_cast<unsigned int>(room));
  // Motive ratings (+0xA4: hunger, comfort, hygiene, bladder, energy, fun): the PC object's comfort,
  // hygiene, bladder, energy and fun where the base row has one (a sofa's lead its comfort, its parts
  // their energy; a toilet its bladder).
  auto rating = [&](unsigned int off, int v) {
    if (v >= 0 && ERQuickdata::S16(def, off) != 0) PutBE16(def + off, static_cast<unsigned int>(v > 10 ? 10 : v));
  };
  rating(0xa6, m.comfort);
  rating(0xa8, m.hygiene);
  rating(0xaa, m.bladder);
  rating(0xac, m.energy);
  rating(0xae, m.fun);
  if (!withRes) {
    PutPtr(def + 0xc0, nullptr);
    return def;
  }
  unsigned char* res = def + kDefSize;
  unsigned char* animCount = res + kResDataSize;
  unsigned char* animDefs = animCount + 4;
  PutPtr(def + 0xc0, res);
  // ResData: the mod's thumbnail; no animated character (+0x20: its skeleton is the base model's).
  if (baseRes) std::memcpy(res, baseRes, kResDataSize);
  else std::memset(res, 0, kResDataSize);
  PutPtr(res, animDefs);
  if (m.thumbnail) PutBE32(res + 0x1c, m.thumbnail);
  if (model) PutBE32(res + 0x20, animRes ? ERQuickdata::U32(animRes, 0x20) : 0);
  // The state table: every state shows the mod's model, without the base's animation, effects and
  // shader swaps (they name the base model's parts); an animated appliance keeps animRes's animations
  // and effects attachments (its model's strips are bound to that character's nodes).
  PutBE32(animCount, n);
  for (unsigned int i = 0; i < n; ++i) {
    unsigned char* d = animDefs + i * kAnimDefSize;
    if (anims) std::memcpy(d, baseAnims + i * kAnimDefSize, kAnimDefSize);
    else std::memset(d, 0, kAnimDefSize), PutBE32(d + 0x18, 0xffffffffu);
    if (!model) continue;
    unsigned int shown = model;
    for (const auto& st : states)
      if (st.first == static_cast<int>(i)) shown = st.second;
    PutBE32(d + 0x00, shown);
    if (!animRes) {
      PutBE32(d + 0x04, 0);
      PutBE32(d + 0x08, 0);
    }
    PutBE32(d + 0x0c, 0);
    PutBE32(d + 0x10, 0);
  }
  return def;
}
}  // namespace

void ObjectFolderImpl::AddModObjects() {
  if (!simmy::mods_enabled() || !m_objects) return;
  Tail tail{m_objects->ModTail(), 0, m_objects->ModTailSize()};
  if (!tail.base) return;
  const void* resFiles = m_objects->getTable("ResFile");
  if (!resFiles) return;
  static const bool verbose = std::getenv("SIMMY_MODS_VERBOSE") != nullptr;
  const auto start = std::chrono::steady_clock::now();
  // the next free master id of each object file (a group's parts share their file and master id)
  std::map<const void*, short> nextMaster;
  auto masterFor = [&](ObjSelector* base) -> short {
    auto it = nextMaster.find(base->m_resFile);
    if (it == nextMaster.end()) {
      short top = 0;
      for (ObjSelector* s = GetNextSelector(nullptr); s; s = GetNextSelector(s))
        if (s->m_resFile == base->m_resFile && s->m_def && objdef::MasterID(s->m_def) > top) top = objdef::MasterID(s->m_def);
      it = nextMaster.emplace(base->m_resFile, static_cast<short>(top < 0x100 ? 0x100 : top + 1)).first;
    }
    return it->second < 0x7fff ? it->second++ : 0;
  };
  int added = 0, groups = 0, skipped = 0;
  for (const simmy::ModObject& m : simmy::mods_objects()) {
    const std::string what = m.mod + " " + Narrow(m.name);
    bool taken = GetSelectorByGUID(static_cast<int>(m.guid)) != nullptr;
    for (const simmy::ModTile& t : m.tiles) taken = taken || GetSelectorByGUID(static_cast<int>(t.guid)) || t.guid == m.guid;
    if (taken) {
      std::fprintf(stderr, "[mods] %s: GUID %08X (or a tile's) is taken; skipped\n", what.c_str(), m.guid);
      ++skipped;
      continue;
    }
    const Base base = FindBase(this, m);
    if (!base.selector || objdef::Type(base.lead) == 2 || objdef::Type(base.lead) == 8 || !base.drawing ||
        !objdef::ResData(base.drawing)) {
      std::fprintf(stderr, "[mods] %s: base object \"%s\" (%08X) not found or not usable; skipped\n", what.c_str(),
                   m.baseFile.c_str(), m.baseGuid);
      ++skipped;
      continue;
    }
    const bool multi = !m.tiles.empty();
    if (base.group && !multi) {
      std::fprintf(stderr, "[mods] %s: base \"%s\" is a multi-tile group: the object needs tiles; skipped\n", what.c_str(),
                   base.selector->m_fileName);
      ++skipped;
      continue;
    }
    // Animated appliances: the character and state table of the base's drawing part, or anim_base's.
    const unsigned char* animBaseRes = nullptr;
    if (m.animated && !m.animBase.empty()) {
      for (ObjSelector* s = GetNextSelector(nullptr); s && !animBaseRes; s = GetNextSelector(s))
        if (!s->m_mod && s->m_def && s->m_fileName && m.animBase == s->m_fileName && objdef::ResData(s->m_def) &&
            ERQuickdata::U32(objdef::ResData(s->m_def), 0x20))
          animBaseRes = objdef::ResData(s->m_def);
      if (!animBaseRes)
        std::fprintf(stderr, "[mods] %s: anim_base \"%s\" has no animated character; still model\n", what.c_str(),
                     m.animBase.c_str());
    }
    auto animOf = [&](const ObjDefinition* from) -> const unsigned char* {
      if (!m.animated) return nullptr;
      if (animBaseRes) return animBaseRes;
      const unsigned char* r = objdef::ResData(from);
      return r && ERQuickdata::U32(r, 0x20) ? r : nullptr;
    };
    const unsigned int stateModel = m.animated && m.animModel ? m.animModel : m.model;
    char* file = const_cast<char*>(base.selector->m_fileName);
    auto* resFile = reinterpret_cast<ResFile*>(const_cast<void*>(base.selector->m_resFile));
    const unsigned int mark = tail.used;
    unsigned char* lead = multi ? CopyDef(tail, base.lead, m, m.guid, true, m.room, m.model, NoStates())
                                : CopyDef(tail, base.drawing, m, m.guid, true, m.room, stateModel, m.states, animOf(base.drawing));
    std::vector<unsigned char*> parts;
    short master = 0;
    bool partsOk = true;
    if (lead && multi) {
      master = masterFor(base.selector);
      PutBE16(lead + 0x14, static_cast<unsigned int>(master));
      PutBE16(lead + 0x16, 0xffff);
      bool first = true;
      for (const simmy::ModTile& t : m.tiles) {
        const ObjDefinition* from = first ? base.drawing : base.plain;
        bool withRes = first;
        unsigned int model = stateModel;
        const std::vector<std::pair<int, unsigned int>>* states = &m.states;
        if (m.baseParts && base.group) {
          // the base's part at this tile (a bed's sides, a sofa's seats): its slots, its model role
          from = BasePart(this, base.selector, t.x, t.y);
          if (!from) {
            partsOk = false;
            break;
          }
          withRes = objdef::ResData(from) != nullptr;
          model = t.model ? t.model : stateModel;
          states = t.model ? &t.states : &m.states;
        }
        unsigned char* d = CopyDef(tail, from, m, t.guid, withRes, first ? m.room : 0, model, *states, withRes ? animOf(from) : nullptr);
        if (!d) break;
        PutBE16(d + 0x14, static_cast<unsigned int>(master));
        PutBE16(d + 0x16, static_cast<unsigned int>(t.x << 8 | t.y));
        parts.push_back(d);
        first = false;
      }
    }
    if (!partsOk) {
      tail.used = mark;
      std::fprintf(stderr, "[mods] %s: a tile has no part of base \"%s\" at its place (base_parts); skipped\n", what.c_str(),
                   base.selector->m_fileName);
      ++skipped;
      continue;
    }
    if (!lead || (multi && (parts.size() != m.tiles.size() || !master))) {
      tail.used = mark;
      std::fprintf(stderr, "[mods] %s: no room for its rows (mod tail %u bytes); skipped\n", what.c_str(), tail.size);
      ++skipped;
      continue;
    }
    auto add = [&](unsigned char* def) -> ObjSelector* {
      if (!AddSelector(reinterpret_cast<ObjDefinition*>(def), file, file, resFile, ERQuickdata::S16(def, 0xbc), nullptr)) return nullptr;
      ObjSelector* s = GetSelectorByGUID(static_cast<int>(objdef::GUID(reinterpret_cast<ObjDefinition*>(def))));
      if (s) s->m_mod = m.mod.c_str();
      return s;
    };
    ObjSelector* s = add(lead);
    if (!s) continue;
    s->m_modObject = &m;
    s->m_modSurface = m.surface;
    for (size_t i = 0; i < parts.size(); ++i) {
      ObjSelector* part = add(parts[i]);
      if (!part) continue;
      part->m_modObject = &m;
      part->m_modSurface = m.tiles[i].surface > 0.0f ? m.tiles[i].surface : m.surface;
    }
    s->m_modName = m.name;
    s->m_modDesc = m.desc;
    if (!s->m_modName.empty()) s->m_modNameSlot = reinterpret_cast<const unsigned short*>(s->m_modName.c_str());
    if (!s->m_modDesc.empty()) s->m_modDescSlot = reinterpret_cast<const unsigned short*>(s->m_modDesc.c_str());
    ++added;
    if (multi) ++groups;
    if (verbose || simmy::mods_objects().size() <= 20)
      std::fprintf(stderr, "[mods] %s: object %08X (a copy of %s %08X%s), price %d, model %08X\n", what.c_str(), m.guid,
                   base.selector->m_fileName, objdef::GUID(base.lead),
                   multi ? (", " + std::to_string(parts.size()) + " tiles").c_str() : "", ERQuickdata::S16(lead, 0x24), m.model);
  }
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  std::fprintf(stderr, "[mods] objects: %d added (%d multi-tile), %d skipped; rows %u of %u bytes; %.1f ms\n", added, groups,
               skipped, tail.used, tail.size, ms);
}

// PC mod: mods. After an object's init tree (cXObjectImpl::Reset, cXMTObjectImpl::Reset): a mod
// object's init_data (data index := value, or |= / &= it), e.g. the heights it may stand at (data 4), a
// ceiling lamp's zero extent (data 8 bit 2), inside only (data 0x1E 2) and no layer (data 0x3F 0).
void cXObjectImpl::ApplyModInitData() {
  const simmy::ModObject* m = m_selector ? m_selector->m_modObject : nullptr;
  if (!m) return;
  for (const auto& d : m->initData) {
    const int index = d.first & 0xff;
    short v = static_cast<short>(d.second);
    if (d.first & simmy::ModObject::kInitOr) v = static_cast<short>(m_data[index] | v);
    else if (d.first & simmy::ModObject::kInitAnd) v = static_cast<short>(m_data[index] & v);
    m_data.WriteVar(index, v);
  }
}
