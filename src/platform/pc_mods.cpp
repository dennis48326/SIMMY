// PC mod: mods. The mod folder (see platform.hpp): each sub-folder with a mod.ini is one mod. Read once
// at start-up; everything here is immutable afterwards (resource loads may run on the house-load thread).
//
// mod.ini (UTF-8; '#' or ';' start a comment):
//   [object]                 one per object the mod adds
//   guid = 0x4CC4FD6C         the new object's GUID (must not be a GameCube object's)
//   base = plant_floor_rubbertree   the GameCube object it copies (object file name), or
//   base_guid = 0x5C53DF88
//   name = ..., desc = ...     catalogue texts
//   price = 150                (default: the base object's)
//   model = 0x12345678         model resource id (default: the base object's)
//   thumbnail = 0x12345678     catalogue thumbnail shader id (default: the base object's)
//   functions = 0x20           catalogue function flags: the buy-mode tab (default: the base object's)
//   room = 3                   catalogue room rating (default: the base object's)
//   tiles = 0,0:0xGUID 1,0:0xGUID   a multi-tile object: each tile's offset (x,y) and GUID (":0.80": a
//                              surface's top over that tile)
//   surface = 0.80             a surface's top (tiles): where its object slots put things
//   connect = counter          counter pieces drawn by neighbours (counter or island), with
//   model_corner = 0x..., model_end_px, model_end_nx, model_ends   their models
//   pack = ...                 where the object came from (informational)
//   seat_offset = -0.09        seating / beds: a person in its slots is drawn this much higher (tiles)
//   state_models = 1:0x... 2:0x...   per-state models (state 1 shows this model)
//   base_parts = 1             a group base's parts are matched by tile (x << 8 | y sub index)
//   tile_model = 1,0:0xMODEL 1:0x... 2:0x...   (repeatable) the model a tile's part draws, its states
//   comfort = 7, energy = 2    motive ratings, where the base has one
// Resource files next to it: "<manager>_<8 hex digits>.res", e.g. "models_9A3F0C12.res". Only their
// names are read at start-up; a resource is read from its file each time the game loads it.
#include "pc_settings.hpp"

#include "simmy/platform.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace simmy {
namespace {
bool g_modsEnabled = false;
std::vector<ModObject> g_modObjects;
// (manager name lower-case, id) -> the resource's file.
std::map<std::pair<std::string, unsigned int>, std::string> g_modResources;

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}
bool LoadFile(const std::string& path, std::vector<unsigned char>& out) {
  size_t size = 0;
  void* data = SDL_LoadFile(path.c_str(), &size);
  if (!data) return false;
  out.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
  SDL_free(data);
  return true;
}
// UTF-8 -> UTF-16 (the game's text).
std::u16string Utf16(const std::string& s) {
  std::u16string out;
  for (size_t i = 0; i < s.size();) {
    unsigned int c = static_cast<unsigned char>(s[i]), n = 0;
    if (c < 0x80) n = 0;
    else if ((c & 0xe0) == 0xc0) c &= 0x1f, n = 1;
    else if ((c & 0xf0) == 0xe0) c &= 0x0f, n = 2;
    else c &= 0x07, n = 3;
    ++i;
    for (unsigned int k = 0; k < n && i < s.size(); ++k, ++i) c = c << 6 | (static_cast<unsigned char>(s[i]) & 0x3f);
    if (c >= 0x10000) {
      c -= 0x10000;
      out.push_back(static_cast<char16_t>(0xd800 + (c >> 10)));
      out.push_back(static_cast<char16_t>(0xdc00 + (c & 0x3ff)));
    } else {
      out.push_back(static_cast<char16_t>(c));
    }
  }
  return out;
}
bool ParseU32(const std::string& v, unsigned int& out) {
  char* end = nullptr;
  const unsigned long x = std::strtoul(v.c_str(), &end, 0);
  if (!end || *end || v.empty()) return false;
  out = static_cast<unsigned int>(x);
  return true;
}

// "x,y:0xGUID x,y:0xGUID ...": a multi-tile object's tiles (offsets 0..127).
bool ParseTiles(const std::string& v, std::vector<ModTile>& out) {
  out.clear();
  size_t pos = 0;
  while (pos < v.size()) {
    while (pos < v.size() && std::isspace(static_cast<unsigned char>(v[pos]))) ++pos;
    if (pos >= v.size()) break;
    size_t end = pos;
    while (end < v.size() && !std::isspace(static_cast<unsigned char>(v[end]))) ++end;
    const std::string item = v.substr(pos, end - pos);
    pos = end;
    ModTile t;
    char guid[48] = {};
    if (std::sscanf(item.c_str(), "%d,%d:%47s", &t.x, &t.y, guid) != 3) return false;
    std::string g = guid;
    const size_t colon = g.find(':');
    if (colon != std::string::npos) {
      t.surface = static_cast<float>(std::atof(g.c_str() + colon + 1));
      g.resize(colon);
    }
    if (!ParseU32(g, t.guid) || !t.guid || t.x < 0 || t.y < 0 || t.x > 127 || t.y > 127) return false;
    out.push_back(t);
  }
  return !out.empty();
}

// "s:0xMODEL s:0xMODEL ...": per-state models.
bool ParseStates(const std::string& v, std::vector<std::pair<int, unsigned int>>& out) {
  size_t pos = 0;
  while (pos < v.size()) {
    while (pos < v.size() && std::isspace(static_cast<unsigned char>(v[pos]))) ++pos;
    if (pos >= v.size()) break;
    size_t end = pos;
    while (end < v.size() && !std::isspace(static_cast<unsigned char>(v[end]))) ++end;
    const std::string item = v.substr(pos, end - pos);
    pos = end;
    const size_t colon = item.find(':');
    unsigned int model = 0;
    if (colon == std::string::npos || !ParseU32(item.substr(colon + 1), model)) return false;
    const int state = std::atoi(item.c_str());
    if (state < 0 || state > 255) return false;
    out.emplace_back(state, model);
  }
  return true;
}

ModTile* FindTile(ModObject& o, int x, int y) {
  for (ModTile& t : o.tiles)
    if (t.x == x && t.y == y) return &t;
  return nullptr;
}

void LoadModIni(const std::string& mod, const std::string& path) {
  std::vector<unsigned char> raw;
  if (!LoadFile(path, raw)) return;
  std::string text(raw.begin(), raw.end());
  ModObject* cur = nullptr;
  size_t pos = 0;
  int lineNo = 0;
  std::vector<ModObject> objects;
  while (pos <= text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    std::string line = Trim(text.substr(pos, end - pos));
    pos = end + 1;
    ++lineNo;
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    if (line[0] == '[') {
      if (Lower(line) == "[object]") {
        objects.emplace_back();
        cur = &objects.back();
        cur->mod = mod;
      } else {
        cur = nullptr;
      }
      continue;
    }
    const size_t eq = line.find('=');
    if (!cur || eq == std::string::npos) continue;
    const std::string key = Lower(Trim(line.substr(0, eq))), v = Trim(line.substr(eq + 1));
    bool ok = true;
    if (key == "guid") ok = ParseU32(v, cur->guid);
    else if (key == "base_guid") ok = ParseU32(v, cur->baseGuid);
    else if (key == "base") cur->baseFile = v;
    else if (key == "name") cur->name = Utf16(v);
    else if (key == "desc") cur->desc = Utf16(v);
    else if (key == "price") cur->price = std::atoi(v.c_str());
    else if (key == "model") ok = ParseU32(v, cur->model);
    else if (key == "thumbnail") ok = ParseU32(v, cur->thumbnail);
    else if (key == "functions") {
      unsigned int f = 0;
      ok = ParseU32(v, f);
      if (ok) cur->functions = static_cast<int>(f & 0xffff);
    } else if (key == "room") cur->room = std::atoi(v.c_str());
    else if (key == "pack") cur->pack = v;
    else if (key == "tiles") ok = ParseTiles(v, cur->tiles);
    else if (key == "surface") {
      cur->surface = static_cast<float>(std::atof(v.c_str()));
      ok = cur->surface > 0.0f && cur->surface < 8.0f;
    } else if (key == "connect") {
      const std::string c = Lower(v);
      cur->connect = c == "counter" ? 1 : c == "island" ? 2 : 0;
      ok = cur->connect != 0 || c == "none";
    } else if (key == "model_corner") ok = ParseU32(v, cur->pieces[0]);
    else if (key == "model_end_px") ok = ParseU32(v, cur->pieces[1]);
    else if (key == "model_end_nx") ok = ParseU32(v, cur->pieces[2]);
    else if (key == "model_ends") ok = ParseU32(v, cur->pieces[3]);
    else if (key == "seat_offset") {
      cur->seatOffset = static_cast<float>(std::atof(v.c_str()));
      ok = cur->seatOffset > -1.0f && cur->seatOffset < 1.0f;
    } else if (key == "state_models") ok = ParseStates(v, cur->states);
    else if (key == "base_parts") cur->baseParts = std::atoi(v.c_str()) != 0;
    else if (key == "comfort") cur->comfort = std::atoi(v.c_str());
    else if (key == "energy") cur->energy = std::atoi(v.c_str());
    else if (key == "tile_model") {
      // (after "tiles") "x,y:0xMODEL [state:0xMODEL ...]"
      int x = 0, y = 0, n = 0;
      ModTile* t = std::sscanf(v.c_str(), "%d,%d:%n", &x, &y, &n) == 2 ? FindTile(*cur, x, y) : nullptr;
      const size_t sp = t ? v.find_first_of(" \t", static_cast<size_t>(n)) : std::string::npos;
      ok = t && ParseU32(v.substr(static_cast<size_t>(n), sp == std::string::npos ? std::string::npos : sp - n), t->model) &&
           (sp == std::string::npos || ParseStates(v.substr(sp), t->states));
    }
    if (!ok) std::fprintf(stderr, "[mods] %s:%d: bad value \"%s\"\n", path.c_str(), lineNo, v.c_str());
  }
  for (ModObject& o : objects) {
    if (!o.guid || (!o.baseGuid && o.baseFile.empty())) {
      std::fprintf(stderr, "[mods] %s: an [object] without guid or base is skipped\n", path.c_str());
      continue;
    }
    g_modObjects.push_back(std::move(o));
  }
}

// "<manager>_<8 hex>.res" files of one mod folder.
void LoadModResources(const std::string& mod, const std::string& dir) {
  int count = 0;
  char** names = SDL_GlobDirectory(dir.c_str(), "*.res", SDL_GLOB_CASEINSENSITIVE, &count);
  if (!names) return;
  for (int i = 0; i < count; ++i) {
    const std::string name = names[i];
    const size_t us = name.rfind('_'), dot = name.rfind('.');
    unsigned int id = 0;
    if (us == std::string::npos || dot == std::string::npos || dot <= us + 1 ||
        !ParseU32("0x" + name.substr(us + 1, dot - us - 1), id)) {
      std::fprintf(stderr, "[mods] %s/%s: not <manager>_<id>.res\n", mod.c_str(), name.c_str());
      continue;
    }
    const auto key = std::make_pair(Lower(name.substr(0, us)), id);
    if (g_modResources.count(key)) {
      std::fprintf(stderr, "[mods] %s/%s: id %08X is already given by another mod; skipped\n", mod.c_str(), name.c_str(), id);
      continue;
    }
    g_modResources.emplace(key, dir + "/" + name);
  }
  SDL_free(names);
}
}  // namespace

void mods_init() {
  const std::string dir = settings_mods_dir();
  if (dir.empty()) return;
  const Uint64 start = SDL_GetTicksNS();
  g_modsEnabled = true;
  int count = 0;
  char** entries = SDL_GlobDirectory(dir.c_str(), nullptr, 0, &count);
  std::vector<std::string> mods;
  for (int i = 0; entries && i < count; ++i) {
    SDL_PathInfo info;
    const std::string path = dir + "/" + entries[i];
    if (SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY &&
        SDL_GetPathInfo((path + "/mod.ini").c_str(), &info) && info.type == SDL_PATHTYPE_FILE)
      mods.push_back(entries[i]);
  }
  if (entries) SDL_free(entries);
  std::sort(mods.begin(), mods.end());  // a fixed order (GUID and id clashes: the first mod wins)
  for (const std::string& m : mods) {
    LoadModIni(m, dir + "/" + m + "/mod.ini");
    LoadModResources(m, dir + "/" + m);
  }
  size_t tiles = 0;
  for (const ModObject& o : g_modObjects) tiles += o.tiles.size();
  std::fprintf(stderr, "[mods] %s: %zu mod(s), %zu object(s) (%zu multi-tile parts), %zu resource(s); read in %.1f ms\n",
               dir.c_str(), mods.size(), g_modObjects.size(), tiles, g_modResources.size(),
               static_cast<double>(SDL_GetTicksNS() - start) / 1e6);
}

bool mods_enabled() { return g_modsEnabled; }

const std::vector<ModObject>& mods_objects() { return g_modObjects; }

bool mods_resource(const char* manager, unsigned int id, std::vector<unsigned char>& out) {
  if (!g_modsEnabled || !manager) return false;
  const auto it = g_modResources.find(std::make_pair(Lower(manager), id));
  if (it == g_modResources.end()) return false;
  if (!LoadFile(it->second, out)) {
    std::fprintf(stderr, "[mods] %s: cannot be read\n", it->second.c_str());
    return false;
  }
  return true;
}

// Per object: its definition, its ResData and up to 32 states (the lead of a group the same), and per
// tile a definition (the first tile's ResData too).
unsigned int mods_tail_bytes() {
  if (!g_modsEnabled) return 0;
  unsigned long long bytes = 0;
  for (const ModObject& o : g_modObjects) {
    const unsigned long long res = 0x38 + 4 + 32 * 0x20;
    bytes += 0xc4 + res;
    if (!o.tiles.empty()) bytes += o.tiles.size() * 0xc4 + (o.baseParts ? o.tiles.size() : 1) * res;
  }
  bytes = (bytes + 0x1000 + 0xfff) & ~0xfffull;
  return bytes > 0x4000000ull ? 0x4000000u : static_cast<unsigned int>(bytes);
}
}  // namespace simmy
