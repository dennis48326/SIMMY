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
// Resource files next to it: "<manager>_<8 hex digits>.res", e.g. "models_9A3F0C12.res".
#include "pc_settings.hpp"

#include "simmy/platform.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

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
// (manager name lower-case, id) -> the resource's bytes.
std::map<std::pair<std::string, unsigned int>, std::vector<unsigned char>> g_modResources;

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
    std::vector<unsigned char> data;
    if (!LoadFile(dir + "/" + name, data)) continue;
    const auto key = std::make_pair(Lower(name.substr(0, us)), id);
    if (g_modResources.count(key)) {
      std::fprintf(stderr, "[mods] %s/%s: id %08X is already given by another mod; skipped\n", mod.c_str(), name.c_str(), id);
      continue;
    }
    g_modResources.emplace(key, std::move(data));
  }
  SDL_free(names);
}
}  // namespace

void mods_init() {
  const std::string dir = settings_mods_dir();
  if (dir.empty()) return;
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
  std::fprintf(stderr, "[mods] %s: %zu mod(s), %zu object(s), %zu resource(s)\n", dir.c_str(), mods.size(),
               g_modObjects.size(), g_modResources.size());
}

bool mods_enabled() { return g_modsEnabled; }

const std::vector<ModObject>& mods_objects() { return g_modObjects; }

const std::vector<unsigned char>* mods_resource(const char* manager, unsigned int id) {
  if (!g_modsEnabled || !manager) return nullptr;
  const auto it = g_modResources.find(std::make_pair(Lower(manager), id));
  return it != g_modResources.end() ? &it->second : nullptr;
}
}  // namespace simmy
