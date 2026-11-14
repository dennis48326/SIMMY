// PC mod: mods - wall and floor coverings (see include/simmy/modcoverings.hpp).
//
// The build sets image's mod tail (ERQuickdata::ModTail, sized by simmy::mods_coverings_tail_bytes):
//   per kind (walls, floors): the set's row cell (a self-relative pointer to the array, what
//   EGlobal::m_wallSet / m_floorSet point at, as the disc's "WallSet" / "FloorSet" row), the count
//   word and 255 pointers (the disc's rows, then the slots given out);
//   per covering: its row, a copy of its base's (WallTile 0x14 bytes: +0 price, +4 name, +8 shader,
//   +0xC kind, +0x10 buyable; FloorTile 0x18: +0 price, +4 name, +8 footstep surface, +0xC shader,
//   +0x10 kind, +0x14 buyable) with its own price, name, shader (and floor surface), and its name's
//   slot cell (a native pointer to the mod's text, as the image's own cells hold).
// Every reader of the sets (catalogue, painters, walls' and floors' shaders, lot value, footsteps,
// grass test) works on the copied sets unchanged.
//
// The palette ('Arry' kPaletteId in the lot's file): "SMC1", a little-endian count, then per covering
// on the lot: kind (0 wall, 1 floor), its pattern number, the GameCube pattern to show without it, the
// key's length and the key.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"
#include "simmy/modcoverings.hpp"
#include "simmy/classes/buildtools.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace simmy::modcov {
namespace {
constexpr int kMaxPatterns = 255;  // a byte each; a floor of 0xFF is a tile split by a diagonal wall
constexpr int kArry = 0x41727279;

struct Kind {
  int disc = 0;
  int rowSize = 0;
  unsigned char* cell = nullptr;  // the set's row cell
  unsigned char* arr = nullptr;   // the pointer array (count at arr - 4)
  std::vector<int> cov;           // covering n -> its index in mods_coverings()
  std::vector<unsigned char*> rows;
  std::vector<int> fallback;      // covering n -> its base's pattern number on the disc
  std::vector<int> slotOf;        // covering n -> its slot, -1
  std::vector<int> atSlot;        // pattern -> covering n, -1
  std::map<std::string, int> byKey;
};
Kind g_kinds[2];
bool g_active = false;
const unsigned char* g_tail = nullptr;
std::map<const void*, int> g_rowCovering;  // a covering's row -> its index in mods_coverings()

void PutBE(unsigned char* p, unsigned int v) {
  p[0] = static_cast<unsigned char>(v >> 24), p[1] = static_cast<unsigned char>(v >> 16);
  p[2] = static_cast<unsigned char>(v >> 8), p[3] = static_cast<unsigned char>(v);
}
// A self-relative pointer field (ERQuickdata::Ptr reads it back).
void SetPtr(unsigned char* field, const void* target) {
  PutBE(field, static_cast<unsigned int>(static_cast<int>(static_cast<const unsigned char*>(target) - field)));
}
int Count(const Kind& k) { return static_cast<int>(ERQuickdata::U32(k.arr - 4)); }
void SetCount(Kind& k, int n) { PutBE(k.arr - 4, static_cast<unsigned int>(n)); }

void ResetSlots() {
  for (Kind& k : g_kinds) {
    if (!k.arr) continue;
    SetCount(k, k.disc);
    k.slotOf.assign(k.rows.size(), -1);
    k.atSlot.assign(256, -1);
  }
}

// Each byte of the world that holds a pattern of kind `floor` (0 walls, 1 floors): the floor array;
// the wall entry's pattern slots (bytes 4-7), and on a tile split by a diagonal its two floor halves
// (bytes 2 and 4) and the diagonal's sides (5-7) (TileWalls' unpacking).
template <class F>
void ForEachPatternByte(cFixedWorldImpl* world, F&& fn) {
  if (!world || !world->m_floors || !world->m_walls || !world->m_floors->m_rows || !world->m_walls->m_rows) return;
  const int xs = world->m_floors->m_xSize, ys = world->m_floors->m_ySize;
  if (world->m_walls->m_xSize != xs || world->m_walls->m_ySize != ys || world->m_walls->m_elemSize != 8) return;
  for (int x = 0; x < xs; ++x)
    for (int y = 0; y < ys; ++y) {
      fn(1, *world->m_floors->At(x, y));
      unsigned char* w = world->m_walls->At(x, y);
      if (w[0] & 0x30) {
        fn(1, w[2]);
        fn(1, w[4]);
      } else {
        fn(0, w[4]);
      }
      fn(0, w[5]);
      fn(0, w[6]);
      fn(0, w[7]);
    }
}

// The slot of covering n of kind `f`; `reclaim`: when all 255 are taken, one the lot no longer shows.
int AssignSlot(int f, int n, bool reclaim) {
  Kind& k = g_kinds[f];
  if (!g_active || n < 0 || n >= static_cast<int>(k.rows.size())) return -1;
  if (k.slotOf[n] >= 0) return k.slotOf[n];
  int slot = -1;
  const int count = Count(k);
  if (count < kMaxPatterns) {
    slot = count;
    SetCount(k, count + 1);
  } else if (reclaim) {
    bool used[256] = {};
    ForEachPatternByte(Globs_pFixedWorld, [&](int kind, unsigned char& b) {
      if (kind == f) used[b] = true;
    });
    for (int s = k.disc; s < kMaxPatterns && slot < 0; ++s)
      if (!used[s]) slot = s;
    if (slot >= 0 && k.atSlot[slot] >= 0) k.slotOf[k.atSlot[slot]] = -1;
  }
  if (slot < 0) {
    std::printf("[mods] covering %s: the lot already shows %d %s patterns\n", mods_coverings()[k.cov[n]].key.c_str(),
                kMaxPatterns, f ? "floor" : "wall");
    return -1;
  }
  SetPtr(k.arr + slot * 4, k.rows[n]);
  k.slotOf[n] = slot;
  k.atSlot[slot] = n;
  std::printf("[mods] covering %s: %s pattern %d\n", mods_coverings()[k.cov[n]].key.c_str(), f ? "floor" : "wall", slot);
  return slot;
}

int DiscCountOf(int f) {
  if (g_active) return g_kinds[f].disc;
  return ibuild::SetCount(f ? _globals.m_floorSet : _globals.m_wallSet);
}
}  // namespace

void InstallSets(ERQuickdata* q, const void*& wallSet, const void*& floorSet) {
  const std::vector<ModCovering>& all = mods_coverings();
  if (!mods_enabled() || all.empty() || !q || !wallSet || !floorSet) return;
  unsigned char* tail = q->ModTail();
  const unsigned int size = q->ModTailSize();
  if (!tail || !size) {
    std::fprintf(stderr, "[mods] coverings: the build sets have no room for them; not added\n");
    return;
  }
  if (g_active && g_tail == tail) {  // SetDefaults again (back at the main menu)
    wallSet = g_kinds[0].cell;
    floorSet = g_kinds[1].cell;
    ResetSlots();
    return;
  }
  std::memset(tail, 0, size);
  unsigned int off = 0;
  bool full = false;
  auto alloc = [&](unsigned int n, unsigned int align) -> unsigned char* {
    off = (off + align - 1) & ~(align - 1);
    if (off + n > size) {
      full = true;
      return nullptr;
    }
    unsigned char* p = tail + off;
    off += n;
    return p;
  };
  const void* sets[2] = {wallSet, floorSet};
  const unsigned char* discArr[2] = {};
  for (int f = 0; f < 2; ++f) {
    Kind& k = g_kinds[f];
    k = Kind{};
    k.rowSize = f ? 0x18 : 0x14;
    discArr[f] = static_cast<const unsigned char*>(ERQuickdata::Ptr(sets[f], 0));
    k.disc = discArr[f] ? static_cast<int>(ERQuickdata::U32(discArr[f] - 4)) : 0;
    k.cell = alloc(8, 8);
    unsigned char* a = alloc(4 + kMaxPatterns * 4, 4);
    if (!k.cell || !a || k.disc <= 1 || k.disc >= kMaxPatterns) return;
    k.arr = a + 4;
    SetPtr(k.cell, k.arr);
    for (int i = 0; i < k.disc; ++i) SetPtr(k.arr + i * 4, ERQuickdata::Ptr(discArr[f], static_cast<unsigned int>(i) * 4));
  }
  int skipped = 0;
  g_rowCovering.clear();
  for (size_t i = 0; i < all.size() && !full; ++i) {
    const ModCovering& c = all[i];
    const int f = c.floor ? 1 : 0;
    Kind& k = g_kinds[f];
    const unsigned int shaderAt = f ? 0xc : 8;
    int base = -1;
    for (int r = 0; r < k.disc && base < 0; ++r)
      if (ERQuickdata::U32(ERQuickdata::Ptr(discArr[f], static_cast<unsigned int>(r) * 4), shaderAt) == c.base) base = r;
    if (base <= 0) {  // pattern 0: no paper / the lot's grass
      std::fprintf(stderr, "[mods] %s: covering %s: base %08X is not a GameCube %s covering; skipped\n", c.mod.c_str(),
                   c.key.c_str(), c.base, f ? "floor" : "wall");
      ++skipped;
      continue;
    }
    unsigned char* row = alloc(static_cast<unsigned int>(k.rowSize), 4);
    unsigned char* name = alloc(8, 8);
    if (!row || !name) break;
    std::memcpy(row, ERQuickdata::Ptr(discArr[f], static_cast<unsigned int>(base) * 4), static_cast<size_t>(k.rowSize));
    const unsigned short* text = reinterpret_cast<const unsigned short*>(c.name.c_str());
    std::memcpy(name, &text, sizeof text);
    PutBE(row, static_cast<unsigned int>(c.price));
    SetPtr(row + 4, name);
    PutBE(row + shaderAt, c.shader);
    if (f && c.sound >= 0) PutBE(row + 8, static_cast<unsigned int>(c.sound));
    row[f ? 0x14 : 0x10] = 1;  // for sale
    const int n = static_cast<int>(k.rows.size());
    if (!k.byKey.emplace(c.key, n).second) {
      ++skipped;
      continue;
    }
    k.cov.push_back(static_cast<int>(i));
    k.rows.push_back(row);
    k.fallback.push_back(base);
    g_rowCovering[row] = static_cast<int>(i);
  }
  if (full) std::fprintf(stderr, "[mods] coverings: the build sets' tail is full; the rest are not added\n");
  g_active = true;
  g_tail = tail;
  ResetSlots();
  wallSet = g_kinds[0].cell;
  floorSet = g_kinds[1].cell;
  std::fprintf(stderr, "[mods] coverings: %zu wall and %zu floor covering(s) added (after the disc's %d and %d), %d skipped\n",
               g_kinds[0].rows.size(), g_kinds[1].rows.size(), g_kinds[0].disc, g_kinds[1].disc, skipped);
}

bool Active() { return g_active; }
int DiscCount(bool floor) { return DiscCountOf(floor ? 1 : 0); }
int CoveringCount(bool floor) { return g_active ? static_cast<int>(g_kinds[floor ? 1 : 0].rows.size()) : 0; }
const void* CoveringRow(bool floor, int n) {
  const Kind& k = g_kinds[floor ? 1 : 0];
  return g_active && n >= 0 && n < static_cast<int>(k.rows.size()) ? k.rows[n] : nullptr;
}
const unsigned short* CoveringDesc(const void* row) {
  if (!g_active) return nullptr;
  const auto it = g_rowCovering.find(row);
  if (it == g_rowCovering.end()) return nullptr;
  const ModCovering& c = mods_coverings()[it->second];
  return c.desc.empty() ? nullptr : reinterpret_cast<const unsigned short*>(c.desc.c_str());
}
int Slot(bool floor, int n) { return AssignSlot(floor ? 1 : 0, n, true); }

int FloorScale(int pattern) {
  if (!g_active) return 1;
  const Kind& k = g_kinds[1];
  if (pattern < k.disc || pattern >= kMaxPatterns || k.atSlot[pattern] < 0) return 1;
  return mods_coverings()[k.cov[k.atSlot[pattern]]].scale;
}

int PatternFromText(bool floor, const char* text) {
  if (!text) return -1;
  char* end = nullptr;
  const long v = std::strtol(text, &end, 0);
  if (end != text && (*end == 0 || *end == ',' || *end == ';')) return static_cast<int>(v);
  std::string key(text);
  key = key.substr(0, key.find_first_of(",;"));
  if (!g_active) return -1;
  const Kind& k = g_kinds[floor ? 1 : 0];
  const auto it = k.byKey.find(key);
  return it == k.byKey.end() ? -1 : Slot(floor, it->second);
}

void AfterWorldLoad(cFixedWorldImpl* world, iResFile* file) {
  if (g_active) ResetSlots();
  Memory__HandleNode* h = file ? file->GetByID(kArry, static_cast<short>(kPaletteId), nullptr) : nullptr;
  if (!h || !h->m_data || h->m_size < 6 || std::memcmp(h->m_data, "SMC1", 4) != 0) return;  // as saved
  const unsigned char* p = h->m_data;
  const unsigned char* const end = p + h->m_size;
  const int count = p[4] | p[5] << 8;
  p += 6;
  int map[2][256];
  const int disc[2] = {DiscCountOf(0), DiscCountOf(1)};
  for (int f = 0; f < 2; ++f)
    for (int v = 0; v < 256; ++v) map[f][v] = v < disc[f] || v == 0xff ? v : 0;  // unknown: none (bare wall, grass)
  int mapped[2] = {}, fellBack[2] = {};
  for (int i = 0; i < count && p + 4 <= end; ++i) {
    const int f = p[0] ? 1 : 0, slot = p[1], fallback = p[2], len = p[3];
    p += 4;
    if (p + len > end) break;
    const std::string key(reinterpret_cast<const char*>(p), static_cast<size_t>(len));
    p += len;
    if (slot < disc[f] || slot >= kMaxPatterns) continue;
    int to = -1;
    if (g_active) {
      const auto it = g_kinds[f].byKey.find(key);
      if (it != g_kinds[f].byKey.end()) to = AssignSlot(f, it->second, false);
    }
    if (to >= 0) {
      ++mapped[f];
    } else {
      to = fallback > 0 && fallback < disc[f] ? fallback : 0;
      ++fellBack[f];
      std::printf("[mods] lot palette: %s covering %s is not in the mods; shown as GameCube %s %d\n", f ? "floor" : "wall",
                  key.c_str(), f ? "floor" : "wall", to);
    }
    map[f][slot] = to;
  }
  int changed = 0;
  ForEachPatternByte(world, [&](int f, unsigned char& b) {
    const int to = map[f][b];
    if (to != b) {
      b = static_cast<unsigned char>(to);
      ++changed;
    }
  });
  std::printf("[mods] lot palette: %d wall and %d floor covering(s) from the mods, %d and %d fell back; %d pattern byte(s) renumbered\n",
              mapped[0], mapped[1], fellBack[0], fellBack[1], changed);
}

void AfterWorldSave(cFixedWorldImpl* world, iResFile* file) {
  if (!g_active || !file) return;
  bool used[2][256] = {};
  ForEachPatternByte(world, [&](int f, unsigned char& b) { used[f][b] = true; });
  std::vector<unsigned char> out = {'S', 'M', 'C', '1', 0, 0};
  int count = 0;
  for (int f = 0; f < 2; ++f) {
    const Kind& k = g_kinds[f];
    for (int s = k.disc; s < kMaxPatterns; ++s) {
      if (!used[f][s] || k.atSlot[s] < 0) continue;
      const int n = k.atSlot[s];
      const std::string& key = mods_coverings()[k.cov[n]].key;
      out.push_back(static_cast<unsigned char>(f));
      out.push_back(static_cast<unsigned char>(s));
      out.push_back(static_cast<unsigned char>(k.fallback[n]));
      out.push_back(static_cast<unsigned char>(key.size()));
      out.insert(out.end(), key.begin(), key.end());
      ++count;
    }
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
  std::printf("[mods] lot palette saved: %d covering(s)\n", count);
}
}  // namespace simmy::modcov
