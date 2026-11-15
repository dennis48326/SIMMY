// PC mod: mods - fences (see include/simmy/modfences.hpp).
//
// The palette ('Arry' kPaletteId in the lot's file): "SMF1", a little-endian count, then per converted
// fence on the lot: its style, the GameCube fence (style) to show without it, the key's length and the
// key.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/platform.hpp"
#include "simmy/modfences.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace simmy::modfence {
namespace {
constexpr int kArry = 0x41727279;

bool g_init = false;
std::vector<PCTFenceData> g_data;               // fence n's catalogue data
std::vector<const unsigned short*> g_names;     // their names' slot cells
std::vector<int> g_atStyle;                     // style -> fence n, -1
std::map<std::string, int> g_byKey;
std::map<const void*, int> g_byData;

void Init() {
  if (g_init) return;
  g_init = true;
  g_atStyle.assign(256, -1);
  const std::vector<ModFence>& all = mods_fences();
  if (!mods_enabled() || all.empty()) return;
  g_data.resize(all.size());
  g_names.resize(all.size());
  for (size_t i = 0; i < all.size(); ++i) {
    g_names[i] = reinterpret_cast<const unsigned short*>(all[i].name.c_str());
    PCTFenceData& d = g_data[i];
    d.shader = all[i].thumbnail;
    d.price = all[i].price;
    d.name = &g_names[i];
    d.style = -1;
    g_byKey[all[i].key] = static_cast<int>(i);
    g_byData[&d] = static_cast<int>(i);
  }
  std::fprintf(stderr, "[mods] fences: %zu added to the Walls tab (lot styles 0x%X..0x%X)\n", all.size(), kFirstStyle,
               kLastStyle);
}

void ResetSlots() {
  g_atStyle.assign(256, -1);
  for (PCTFenceData& d : g_data) d.style = -1;
}

// Each byte of the world holding a wall style: a diagonal's (byte 3), else segment 1's (byte 2) and
// segment 2's (byte 3) when present (TileWalls' unpacking).
template <class F>
void ForEachStyleByte(cFixedWorldImpl* world, F&& fn) {
  if (!world || !world->m_walls || !world->m_walls->m_rows || world->m_walls->m_elemSize != 8) return;
  const int xs = world->m_walls->m_xSize, ys = world->m_walls->m_ySize;
  for (int x = 0; x < xs; ++x)
    for (int y = 0; y < ys; ++y) {
      unsigned char* w = world->m_walls->At(x, y);
      if (w[0] & 0x30) {
        fn(w[3]);
        continue;
      }
      if (w[0] & 1) fn(w[2]);
      if (w[0] & 2) fn(w[3]);
    }
}

int FenceAt(int style) {
  if (!IsModStyle(style)) return -1;
  Init();
  return g_atStyle[style];
}

int AssignSlot(int n, bool reclaim) {
  Init();
  if (n < 0 || n >= static_cast<int>(g_data.size())) return -1;
  if (g_data[n].style >= 0) return g_data[n].style;
  int style = -1;
  for (int s = kFirstStyle; s <= kLastStyle && style < 0; ++s)
    if (g_atStyle[s] < 0) style = s;
  if (style < 0 && reclaim) {
    bool used[256] = {};
    ForEachStyleByte(Globs_pFixedWorld, [&](unsigned char& b) { used[b] = true; });
    for (int s = kFirstStyle; s <= kLastStyle && style < 0; ++s)
      if (!used[s]) style = s;
    if (style >= 0) g_data[g_atStyle[style]].style = -1;
  }
  if (style < 0) {
    std::printf("[mods] fence %s: the lot already shows %d converted fences\n", mods_fences()[n].key.c_str(),
                kLastStyle - kFirstStyle + 1);
    return -1;
  }
  g_atStyle[style] = n;
  g_data[n].style = style;
  std::printf("[mods] fence %s: style 0x%X\n", mods_fences()[n].key.c_str(), style);
  return style;
}

// A segment's ends as tile corners (x, y: the corner at the tile's x, y; xrouting's edges): 1 and 4 the
// tile's x and x + 1 sides, 2 and 8 its y and y + 1 sides, 0x10 from (x + 1, y) to (x, y + 1), 0x20 from
// (x, y) to (x + 1, y + 1).
struct Ends {
  int ax, ay, bx, by;
};
bool SegmentEnds(int x, int y, int seg, Ends& e) {
  switch (seg) {
    case 1: e = {x, y, x, y + 1}; return true;
    case 4: e = {x + 1, y, x + 1, y + 1}; return true;
    case 2: e = {x, y, x + 1, y}; return true;
    case 8: e = {x, y + 1, x + 1, y + 1}; return true;
    case 0x10: e = {x + 1, y, x, y + 1}; return true;
    case 0x20: e = {x, y, x + 1, y + 1}; return true;
    default: return false;
  }
}

// Whether a post stands at corner (cx, cy) of the edge `self` (style `style`): not when exactly one other
// wall or fence meets it there, of the same style and straight on.
bool PostAt(cFixedWorldImpl* w, const Ends& self, int cx, int cy, int style) {
  struct Seg {
    Ends e;
    int style;
  };
  std::vector<Seg> others;
  auto same = [](const Ends& a, const Ends& b) {
    return (a.ax == b.ax && a.ay == b.ay && a.bx == b.bx && a.by == b.by) ||
           (a.ax == b.bx && a.ay == b.by && a.bx == b.ax && a.by == b.ay);
  };
  for (int tx = cx - 1; tx <= cx; ++tx)
    for (int ty = cy - 1; ty <= cy; ++ty) {
      if (tx < 0 || ty < 0 || tx >= w->m_xSize || ty >= w->m_ySize) continue;
      CTilePt t(tx, ty, 1);
      TileWalls walls = w->GetWall(t);
      for (int seg = walls.First(); seg; seg = walls.Next(TileWallsSegment{seg})) {
        Ends e;
        if (!SegmentEnds(tx, ty, seg, e)) continue;
        if (!((e.ax == cx && e.ay == cy) || (e.bx == cx && e.by == cy)) || same(e, self)) continue;
        bool dup = false;
        for (const Seg& o : others) dup = dup || same(o.e, e);
        if (!dup) others.push_back({e, walls.GetStyle(TileWallsSegment{seg})});
      }
    }
  if (others.size() != 1 || others[0].style != style) return true;
  const Ends& o = others[0].e;
  const int sx = self.bx - self.ax, sy = self.by - self.ay, ox = o.bx - o.ax, oy = o.by - o.ay;
  return sx * oy - sy * ox != 0;  // a turn
}
}  // namespace

bool Active() {
  Init();
  return !g_data.empty();
}
int Count() {
  Init();
  return static_cast<int>(g_data.size());
}
const PCTFenceData* Data(int n) {
  Init();
  return n >= 0 && n < static_cast<int>(g_data.size()) ? &g_data[n] : nullptr;
}
const unsigned short* Desc(const void* data) {
  Init();
  const auto it = g_byData.find(data);
  if (it == g_byData.end()) return nullptr;
  const ModFence& f = mods_fences()[it->second];
  return f.desc.empty() ? nullptr : reinterpret_cast<const unsigned short*>(f.desc.c_str());
}
int Slot(int n) { return AssignSlot(n, true); }

int Price(int style) {
  const int n = FenceAt(style);
  return n < 0 ? -1 : mods_fences()[n].price;
}
bool Walk(int style) {
  const int n = FenceAt(style);
  return n >= 0 && mods_fences()[n].walk;
}

unsigned int Model(int style, int segment, const CTilePt& tile) {
  const int n = FenceAt(style);
  if (n < 0) return 0;
  const ModFence& f = mods_fences()[n];
  const bool diagonal = segment == 0x10 || segment == 0x20;
  const unsigned int* ends = diagonal ? f.diagEnds : f.ends;
  const unsigned int both = diagonal ? f.modelDiag : f.model;
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!ends[0] || !w) return both;
  // The model's -x and +x ends on the tile (EIFenceWall's placement: segment 4 runs from corner (x + 1, y)
  // to (x + 1, y + 1), 8 from (x, y + 1) to (x + 1, y + 1), 0x10 from (x, y + 1) to (x + 1, y), 0x20 from
  // (x, y) to (x + 1, y + 1)).
  const int x = tile.GetX(), y = tile.GetY();
  Ends self;
  if (!SegmentEnds(x, y, segment, self)) return both;
  int nx, ny, px, py;
  switch (segment) {
    case 4: nx = x + 1, ny = y, px = x + 1, py = y + 1; break;
    case 8: nx = x, ny = y + 1, px = x + 1, py = y + 1; break;
    case 0x10: nx = x, ny = y + 1, px = x + 1, py = y; break;
    default: nx = x, ny = y, px = x + 1, py = y + 1; break;
  }
  const bool postN = PostAt(w, self, nx, ny, style), postP = PostAt(w, self, px, py, style);
  if (postN && postP) return both;
  return ends[postN ? 1 : postP ? 2 : 0];
}

int StyleFromText(const char* text) {
  if (!text) return -1;
  char* end = nullptr;
  const long v = std::strtol(text, &end, 0);
  if (end != text && (*end == 0 || *end == ',' || *end == ';')) return static_cast<int>(v);
  std::string key(text);
  key = key.substr(0, key.find_first_of(",;"));
  Init();
  const auto it = g_byKey.find(key);
  return it == g_byKey.end() ? -1 : Slot(it->second);
}

void AfterWorldLoad(cFixedWorldImpl* world, iResFile* file) {
  Init();
  ResetSlots();
  int map[256];
  for (int v = 0; v < 256; ++v) map[v] = IsModStyle(v) ? -1 : v;
  int mapped = 0, fellBack = 0;
  Memory__HandleNode* h = file ? file->GetByID(kArry, static_cast<short>(kPaletteId), nullptr) : nullptr;
  if (h && h->m_data && h->m_size >= 6 && std::memcmp(h->m_data, "SMF1", 4) == 0) {
    const unsigned char* p = h->m_data;
    const unsigned char* const end = p + h->m_size;
    const int count = p[4] | p[5] << 8;
    p += 6;
    for (int i = 0; i < count && p + 3 <= end; ++i) {
      const int style = p[0], fallback = p[1], len = p[2];
      p += 3;
      if (p + len > end) break;
      const std::string key(reinterpret_cast<const char*>(p), static_cast<size_t>(len));
      p += len;
      if (!IsModStyle(style)) continue;
      int to = -1;
      const auto it = g_byKey.find(key);
      if (it != g_byKey.end()) to = AssignSlot(it->second, false);
      if (to >= 0) {
        ++mapped;
      } else {
        to = fallback == 2 || fallback == 0xc || fallback == 0xd ? fallback : 2;
        ++fellBack;
        std::printf("[mods] lot palette: fence %s is not in the mods; shown as GameCube fence style 0x%X\n", key.c_str(), to);
      }
      map[style] = to;
    }
  } else {
    h = nullptr;
  }
  int changed = 0, stray = 0;
  ForEachStyleByte(world, [&](unsigned char& b) {
    int to = map[b];
    if (to < 0) {  // a converted fence's style the palette does not name: a picket fence
      to = 2;
      ++stray;
    }
    if (to != b) {
      b = static_cast<unsigned char>(to);
      ++changed;
    }
  });
  if (h || changed)
    std::printf("[mods] lot palette: %d fence(s) from the mods, %d fell back; %d style byte(s) renumbered%s\n", mapped,
                fellBack, changed, stray ? " (some without a palette entry: picket fence)" : "");
}

void AfterWorldSave(cFixedWorldImpl* world, iResFile* file) {
  if (!file || !Active()) return;
  bool used[256] = {};
  ForEachStyleByte(world, [&](unsigned char& b) { used[b] = true; });
  std::vector<unsigned char> out = {'S', 'M', 'F', '1', 0, 0};
  int count = 0;
  for (int s = kFirstStyle; s <= kLastStyle; ++s) {
    if (!used[s] || g_atStyle[s] < 0) continue;
    const ModFence& f = mods_fences()[g_atStyle[s]];
    out.push_back(static_cast<unsigned char>(s));
    out.push_back(static_cast<unsigned char>(f.base));
    out.push_back(static_cast<unsigned char>(f.key.size()));
    out.insert(out.end(), f.key.begin(), f.key.end());
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
  std::printf("[mods] lot palette saved: %d fence(s)\n", count);
}
}  // namespace simmy::modfence
