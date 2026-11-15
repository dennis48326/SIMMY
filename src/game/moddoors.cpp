// PC mod: mods - doors (see include/simmy/moddoors.hpp).
//
// The palette ('Arry' kPaletteId in the lot's file): "SMD1", a little-endian count, then per converted
// door type on the lot (lead or part): its GUID and the GameCube type's GUID, little-endian.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"
#include "simmy/moddoors.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

namespace simmy::moddoor {
namespace {
constexpr int kArry = 0x41727279;

std::map<unsigned int, unsigned int> g_fallback;          // converted door type -> GameCube type
std::map<unsigned int, unsigned int> g_saved;             // the loading lot's palette
std::vector<std::pair<short, unsigned int>> g_unknown;    // saved types the game lacks: save id, GUID

unsigned int Get32(const unsigned char* p) {
  return static_cast<unsigned int>(p[0]) | static_cast<unsigned int>(p[1]) << 8 | static_cast<unsigned int>(p[2]) << 16 |
         static_cast<unsigned int>(p[3]) << 24;
}
void Put32(std::vector<unsigned char>& out, unsigned int v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(v >> (8 * i)));
}
}  // namespace

void Register(unsigned int guid, unsigned int fallback) {
  if (guid && fallback) g_fallback[guid] = fallback;
}

unsigned int WallCut(cXObject* portal, int variant) {
  if (!portal || variant < 0 || variant > 8) return 0;
  cXObjectImpl* o = portal->GetObjectImplementation();
  const ModObject* m = o && o->m_selector ? o->m_selector->m_modObject : nullptr;
  if (!m || !m->door || !o->m_def) return 0;
  const int sub = objdef::SubIndex(o->m_def) & 0xffff;
  for (const ModTile& t : m->tiles)
    if ((t.x << 8 | t.y) == sub) return t.cut[variant];
  return 0;
}

void SavePalette(iResFile* file) {
  if (!file || g_fallback.empty() || !Globs_pObjectFolder) return;
  std::vector<unsigned char> out = {'S', 'M', 'D', '1', 0, 0};
  int count = 0;
  for (ObjSelector* chain : Globs_pObjectFolder->m_selectors)
    for (ObjSelector* s = chain; s; s = s->m_next) {
      if (!s->m_saveId) continue;
      const auto it = g_fallback.find(s->GetGUID());
      if (it == g_fallback.end()) continue;
      Put32(out, it->first);
      Put32(out, it->second);
      ++count;
    }
  static StringBuffer name;
  if (!count) {
    file->Add(nullptr, kArry, static_cast<short>(kPaletteId), name, false);  // none on the lot (an old one goes)
    return;
  }
  out[4] = static_cast<unsigned char>(count), out[5] = static_cast<unsigned char>(count >> 8);
  auto* handle = new Memory__HandleNode;
  handle->m_size = static_cast<unsigned int>(out.size());
  handle->m_data = new unsigned char[out.size()];
  handle->m_owned = 1;
  std::memcpy(handle->m_data, out.data(), out.size());
  file->Add(handle, kArry, static_cast<short>(kPaletteId), name, false);
  std::printf("[mods] lot palette saved: %d converted door type(s)\n", count);
}

void LoadPalette(iResFile* file) {
  g_saved.clear();
  g_unknown.clear();
  Memory__HandleNode* h = file ? file->GetByID(kArry, static_cast<short>(kPaletteId), nullptr) : nullptr;
  if (!h || !h->m_data || h->m_size < 6 || std::memcmp(h->m_data, "SMD1", 4) != 0) return;
  const unsigned char* p = h->m_data + 6;
  const int count = h->m_data[4] | h->m_data[5] << 8;
  for (int i = 0; i < count && p + 8 <= h->m_data + h->m_size; ++i, p += 8) g_saved[Get32(p)] = Get32(p + 4);
}

void NoteUnknown(unsigned int guid, short saveId) {
  if (!g_saved.empty() && g_saved.count(guid)) g_unknown.emplace_back(saveId, guid);
}

void ApplyFallbacks(std::vector<unsigned int>& saveGuids) {
  int fell = 0;
  for (const auto& u : g_unknown) {
    const unsigned int to = g_saved[u.second];
    ObjSelector* s = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(to)) : nullptr;
    if (!s || u.first <= 0) continue;
    if (saveGuids.size() <= static_cast<size_t>(u.first)) saveGuids.resize(static_cast<size_t>(u.first) + 1u, 0u);
    saveGuids[static_cast<size_t>(u.first)] = to;
    ++fell;
    std::printf("[mods] lot palette: door type %08X is not in the mods; loaded as GameCube %s %08X\n", u.second,
                s->m_fileName ? s->m_fileName : "?", to);
  }
  if (!g_saved.empty())
    std::printf("[mods] lot palette: %zu converted door type(s), %d fell back to GameCube doors\n", g_saved.size(), fell);
  g_unknown.clear();
}
}  // namespace simmy::moddoor
