// Port of game/e_rlevel.cpp (partial) and game/e_levelman.cpp. See elevel.hpp. Still stubbed: weather,
// particle systems, effects and depth of field, bound-tree levels (versions 0 and 1) and EStream
// serialisation. Shadows: the destination-alpha stencil pass per room floor (sims, objects, walls).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/psystem.hpp"
#include "simmy/classes/epointlight.hpp"
#include "simmy/classes/weather.hpp"
#include "simmy/detmath.hpp"

#include <algorithm>
#include <cmath>

int ERLevel::m_fillingOrderTable = 0;  // 804FEA18
int ERLevel::m_drawingOrderTable = 0;
LevelLightingTuning ERLevel::s_levelLightingTuning;  // 80488B6C
namespace {
// The outside radiosity light (ERLevel::Draw: the outside room's floor light and overhead lights).
EVec3 s_outsideAmbient, s_outsideDiffuse;
float s_outsidePointLightDimmer = 0.0f;
}  // namespace

// 802380F4 ERLevel::GetTypeInfo
// 80238100 ERLevel::GetTypeName
// 8023810C ERLevel::GetTypeKey
// 80238118 ERLevel::GetTypeVersion
// 802380B4 ERLevel::SafeDelete
// 80238034 ERLevel::New
// 80238060 ERLevel::Construct
// 80238090 ERLevel::Destruct
// 80238124 ERLevel::GetTypeInfoStatic
// 80238130 ERLevel::GetReadVersion
SIMMY_STORABLE_IMPL(ERLevel, "ERLevel", 6, EResource)

// 80231240
// The location function of the level's own lights: the outside (room 0). (GC: with a room manager,
// the other side is room 0's id there.)
void Outside(unsigned int, bool, short& room, short& otherSide) {
  room = 0;
  otherSide = 0;
}

// 80231120
// GC: from a 0x200-entry pool, then the resource heap.
EOrderTableData* AllocOTD() { return new EOrderTableData; }
// 80231178
void FreeOTDList(EOrderTableData*& head) {
  while (EOrderTableData* d = head) {
    head = d->m_next;
    delete d;
  }
}

namespace {
// OrderTableSortReferenceCompare (802344B0, a qsort comparator) as a strict ordering for std::sort:
// back to front, then by sort layer (higher first), then by display list.
struct OTDRef {
  EOrderTableData* otd;
  float distance;
  float key;
};
bool OrderTableSortReferenceLess(const OTDRef& a, const OTDRef& b) {
  if (a.distance != b.distance) return a.distance > b.distance;
  if (a.key != b.key) return a.key > b.key;
  return a.otd->m_dl < b.otd->m_dl;
}
}  // namespace

// ---- outside lighting (day and night) ----

// 8023700C
void CalcSkyLights(SkyLights& sky, EVec3& dir, ELights& out) {
  for (int i = 0; i < 3; ++i) {
    out.m_ambient[i] = sky.m_ambientColor[i] * sky.m_ambientIntensity;
    out.m_dir[0].m_color[i] = sky.m_dirColor[i] * sky.m_dirIntensity;
    out.m_dir[0].m_dir[i] = dir[i];
  }
  out.m_numDir = 1;
  out.m_numPoint = 0;
}

// 80237350
// t weights `a`, 1 - t weights `b`.
void InterpolateSkyLights(float t, SkyLights& a, SkyLights& b, SkyLights& out) {
  const float u = 1.0f - t;
  for (int i = 0; i < 3; ++i) {
    out.m_dirColor[i] = t * a.m_dirColor[i] + u * b.m_dirColor[i];
    out.m_ambientColor[i] = t * a.m_ambientColor[i] + u * b.m_ambientColor[i];
  }
  out.m_dirIntensity = t * a.m_dirIntensity + u * b.m_dirIntensity;
  out.m_ambientIntensity = t * a.m_ambientIntensity + u * b.m_ambientIntensity;
}

// 80237490
// Blends the table entries around `hour` (the table wraps at 24 h).
void InterpolateLevelLighting(LevelLighting* table, float hour, LevelLightingEntry& out) {
  if (!table) return;
  const int count = table->m_count;
  LevelLightingEntry* e = table->m_entries;
  int next = 0;
  if (count > 0)
    while (e[next].m_time <= hour && ++next < count) {
    }
  int prev = (next + count - 1) % count;
  next %= count;
  float nextTime = e[next].m_time;
  if (nextTime < e[prev].m_time) nextTime += 24.0f;
  const float t = (nextTime - hour) / (nextTime - e[prev].m_time);  // the weight of the previous entry
  const float u = 1.0f - t;
  const LevelLightingEntry& a = e[prev];
  const LevelLightingEntry& b = e[next];
  for (int i = 0; i < 3; ++i) {
    out.m_ambientColor[i] = t * a.m_ambientColor[i] + u * b.m_ambientColor[i];
    out.m_sunColor[i] = t * a.m_sunColor[i] + u * b.m_sunColor[i];
    out.m_field54[i] = t * a.m_field54[i] + u * b.m_field54[i];
  }
  out.m_ambientIntensity = t * a.m_ambientIntensity + u * b.m_ambientIntensity;
  out.m_shadowAlpha = t * a.m_shadowAlpha + u * b.m_shadowAlpha;
  out.m_field40 = a.m_field40;
  out.m_sunIntensity = t * a.m_sunIntensity + u * b.m_sunIntensity;
  out.m_sunPitch = t * a.m_sunPitch + u * b.m_sunPitch;
  out.m_sunYaw = t * a.m_sunYaw + u * b.m_sunYaw;
  out.m_field60 = t * a.m_field60 + u * b.m_field60;
  InterpolateSkyLights(t, const_cast<SkyLights&>(a.m_sky), const_cast<SkyLights&>(b.m_sky), out.m_sky);
}

// 802370D8
// Toward grey by `t`: each colour toward its luminance (sun, ambient, the sky's two), each intensity
// down toward t x 0.5 (sun, ambient) or 0.3 (the sky's), the shadows fading in by t x 0.18.
void InterpolateToGraySky(float t, LevelLightingEntry& e) {
  const float k = 1.0f - t;
  auto grey = [&](EVec3& c, float& intensity, float floor) {
    const float l = t * (c.z * 0.11f + c.x * 0.3f + c.y * 0.59f);
    c.x = k * c.x + l;
    c.y = k * c.y + l;
    c.z = k * c.z + l;
    const float i = k * intensity + t * floor;
    if (i < intensity) intensity = i;
  };
  grey(e.m_sunColor, e.m_sunIntensity, 0.5f);
  grey(e.m_ambientColor, e.m_ambientIntensity, 0.5f);
  grey(e.m_sky.m_dirColor, e.m_sky.m_dirIntensity, 0.30000001192092896f);
  grey(e.m_sky.m_ambientColor, e.m_sky.m_ambientIntensity, 0.30000001192092896f);
  e.m_shadowAlpha = t * 0.18000000715255737f + k * e.m_shadowAlpha;
}

// 80237278
// While the house's rain runs (from half an hour before its start, fading in over that half hour,
// and out over the half hour after it ends) the lights grey.
void ApplyWeatherEffects(levelraininfo* rain, float hour, LevelLightingEntry& entry) {
  if (!rain) return;
  float start = rain->m_start * 0.016666668f - 0.5f;
  if (start < 0.0f) start += 24.0f;
  const float since = hour - start;
  const float length = (rain->m_end - rain->m_start) * 0.016666668f;
  if (!(0.0f < since)) return;
  float t = 0.0f;
  if (since < 0.5f) {
    t = since + since;
  } else if (since - 0.5f < length) {
    t = 1.0f;
  } else {
    const float after = (since - 0.5f) - length;
    if (after < 0.5f) t = 1.0f - (after + after);
  }
  if (t != 0.0f) InterpolateToGraySky(t, entry);
}

// ---- construction ----

// 80231348
// The light grid, the sky matrix (scale 0.03 about the eye), the shadow shaders, and the sun and its
// ambient light (both outside) as the level's first lights. (GC: also records a unit quad display
// list for blob shadows.)
ERLevel::ERLevel() {
  EVec3 s{0.03f, 0.03f, 0.03f};
  m_drawData.m_skyMatrix.Scale(s);
  m_drawData.m_shadowProj.Id();
  m_instances.reserve(kMaxInstances);
  m_visible.reserve(kMaxInstances);
  m_shadowSquare = static_cast<ERShader*>(_shaderman.AddRef(const_cast<char*>("GLOBAL_SHADOW_SQUARE"), nullptr, 0));
  m_shadowCircle = static_cast<ERShader*>(_shaderman.AddRef(const_cast<char*>("GLOBAL_SHADOW_CIRCLE"), nullptr, 0));
  // The blob shadow: a unit square about the origin (uv 0..1) in a display list (GC: the
  // float-array ERC::TriStrip with no colours or normals, data at 803FF460 / 803FF4A0; PC: the same
  // strip as EGEVerts, colour 0x80 = white after the TEV's doubling).
  if (_pGfx) {
    if (ERC* rc = _pGfx->Open(RCMode::Deferred)) {
      rc->SetName("ERLevel::ERLevel");
      static const float kPos[4][2] = {{-0.5f, 0.5f}, {-0.5f, -0.5f}, {0.5f, 0.5f}, {0.5f, -0.5f}};
      static const float kUV[4][2] = {{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}};
      EGEVert v[4];
      for (int i = 0; i < 4; ++i) {
        v[i].pos[0] = kPos[i][0];
        v[i].pos[1] = kPos[i][1];
        v[i].pos[3] = 1.0f;
        v[i].normal[2] = 127;
        v[i].uv[0] = kUV[i][0];
        v[i].uv[1] = kUV[i][1];
        v[i].color[0] = v[i].color[1] = v[i].color[2] = 0x80;
        v[i].color[3] = 0xff;
      }
      rc->TriStrip(v, 4);
      m_shadowDL = _pGfx->Close(rc);
    }
  }
  m_sunAmbient.m_thisSideFn = Outside;
  m_sunAmbient.m_thisSideData = 0;
  m_sun.m_thisSideFn = Outside;
  m_sun.m_thisSideData = 0;
  InsertLight(&m_sun);
  InsertLight(&m_sunAmbient);
}

// 80231700
ERLevel::~ERLevel() {
  RemoveLight(&m_sun);
  RemoveLight(&m_sunAmbient);
  if (m_shadowSquare) {
    m_shadowSquare->DelRef();
    m_shadowSquare = nullptr;
  }
  if (m_shadowCircle) {
    m_shadowCircle->DelRef();
    m_shadowCircle = nullptr;
  }
  if (m_shadowDL && _pGfx) _pGfx->Destroy(m_shadowDL);
  m_shadowDL = nullptr;
  Deallocate();
  if (_pEngine) _pEngine->ClearCurrentLevel(this);
  m_roomInstances.clear();
  m_roomFloors.clear();
  for (EOrderTableData*& head : m_roomOrderTables) FreeOTDList(head);
  m_roomOrderTables.clear();
}

// ---- loading ----

namespace {
template <class T> void ReadPointers(EFile& file, std::vector<T*>& out) {
  const int count = static_cast<int>(simmy::read_u32(file));
  out.clear();
  for (int i = 0; i < count; ++i) {
    EStorable* p = nullptr;
    file >> p;
    out.push_back(static_cast<T*>(p));
  }
}
}  // namespace

// 80232334
// After the resource fields: four words, then by version (2 to 6) the name map (id -> instance), the
// sky count, the level's lights, the terrain and following counts, the fading instances and the
// instances (pointers into the object graph). Versions 0 and 1 stored bound trees.
void ERLevel::Load(EFile& file) {
  EResource::Load(file);
  for (int i = 0; i < 4; ++i) simmy::read_u32(file);
  const int version = m_typeInfo.m_readVersion;
  m_readVersion = version;
  if (version < 2) {
    SIMMY_STUB("ERLevel::Load: bound-tree levels (versions 0 and 1)", 0x80232334);
    return;
  }
  m_updateMap.clear();
  for (int n = static_cast<int>(simmy::read_u32(file)); n > 0; --n) {
    const unsigned int key = simmy::read_u32(file);
    EStorable* p = nullptr;
    file >> p;
    m_updateMap.emplace_back(key, static_cast<EInstance*>(p));
  }
  switch (version) {
    case 6:
      m_skyCount = static_cast<int>(simmy::read_u32(file));
      ReadPointers(file, m_levelLights);
      m_terrainCount = static_cast<int>(simmy::read_u32(file));
      m_count29D7C = static_cast<int>(simmy::read_u32(file));
      ReadPointers(file, m_fadeInstances);
      break;
    case 5:
      ReadPointers(file, m_instances5);
      ReadPointers(file, m_levelLights);
      ReadPointers(file, m_fadeInstances);
      break;
    case 4:
      ReadPointers(file, m_levelLights);
      ReadPointers(file, m_fadeInstances);
      break;
    case 3:
      ReadPointers(file, m_fadeInstances);
      break;
    default:
      break;
  }
  ReadPointers(file, m_instances);
  m_firstObject = static_cast<int>(m_instances.size());
}

// 80232D80
// A level file: a word, the object graph (whose root is the level), a word.
ERLevel* ERLevel::AllocAndLoadLevel(EFile& file) {
  simmy::read_u32(file);
  EStorable* level = nullptr;
  file >> level;
  simmy::read_u32(file);
  return static_cast<ERLevel*>(level);
}

// 80233000
void ERLevel::Deallocate() {
  for (int i = static_cast<int>(m_instances.size()) - 1; i >= 0; --i) {
    EInstance* inst = m_instances[i];
    inst->m_level = nullptr;  // PC: the instance is going; it must not try to leave the level.
    inst->SafeDelete();
  }
  m_instances.clear();
  DeallocateSub();
  m_updateMap.clear();
  m_idMap.clear();
}

// 80233094
void ERLevel::DeallocateSub() {
  for (int i = static_cast<int>(m_fadeInstances.size()) - 1; i >= 0; --i) m_fadeInstances[i]->SafeDelete();
  m_fadeInstances.clear();
  for (int i = static_cast<int>(m_levelLights.size()) - 1; i >= 0; --i) m_levelLights[i]->SafeDelete();
  m_levelLights.clear();
  for (int i = static_cast<int>(m_instances5.size()) - 1; i >= 0; --i) m_instances5[i]->SafeDelete();
  m_instances5.clear();
}

// 80233D1C
// After loading: no rooms yet, every instance prepared, marked as read with the level and initialised.
void ERLevel::Init() {
  SetRoomCount(0);
  for (EInstance* inst : m_instances) {
    PrepareInstance(inst);
    inst->m_flags |= EInstance::kReadByLevel;
    inst->Init();
  }
  m_depthMin = 0.0f;
  m_depthMax = 100.0f;
}

// ---- instances ----

// 802331BC
void ERLevel::InsertInstance(EInstance* instance, EInstance*) {
  AddInstanceToLevel(instance);
  PrepareInstance(instance);
}

// 802331F8
bool ERLevel::IsInstanceListKindaFull() { return m_instances.size() > 0x67f; }

// 802332C4
void ERLevel::InsertLight(EILight* light) {
  m_lights.push_back(light);
  light->m_listNode = light;
  m_lightGrid.m_dirty = 1;
}

// 80233308
void ERLevel::RemoveLight(EILight* light) {
  auto it = std::find(m_lights.begin(), m_lights.end(), light);
  if (it != m_lights.end()) {
    m_lights.erase(it);
    light->m_listNode = nullptr;
  }
  m_lightGrid.m_dirty = 1;
}

// 80233364
void ERLevel::ValidateInstanceInLevel(EInstance*) {}

// 80233210
// A wall part into the level; with a shadow model, also the wall list (shadows).
void ERLevel::InsertWall(EInstance* wall, bool shadow) {
  InsertInstance(wall, nullptr);
  if (shadow) {
    m_wallList.push_back(wall);
    wall->m_listNode = wall;
  } else {
    wall->m_listNode = nullptr;
  }
}

// 8023326C
void ERLevel::RemoveWall(EInstance* wall) {
  if (wall->m_listNode) m_wallList.erase(std::remove(m_wallList.begin(), m_wallList.end(), wall), m_wallList.end());
  wall->m_listNode = nullptr;
  RemoveInstance(wall);
}

// 8023362C
// Wall parts are kept together in the instance array (from m_wallFirst to m_wallLast), each next to
// the last of its wallpaper (by shader resource id), so they draw with few shader changes.
void ERLevel::AddWallInstanceToLevel(EInstance* instance) {
  auto* wall = static_cast<EIWallPart*>(instance);
  if (m_wallFirst == -1) {
    m_wallFirst = m_wallLast = static_cast<int>(m_instances.size());
    m_instances.push_back(instance);
    return;
  }
  int at = m_wallFirst;
  if (ERShader* paper = wall->GetWallpaperShader()) {
    const unsigned int id = paper->m_id;
    for (at = m_wallFirst; at <= m_wallLast; ++at) {
      ERShader* other = static_cast<EIWallPart*>(m_instances[at])->GetWallpaperShader();
      if (other && other->m_id == id) break;
    }
    if (at <= m_wallLast) {
      if (at < m_wallLast) {
        const unsigned int model = wall->m_modelId;
        for (; at <= m_wallLast; ++at) {
          auto* other = static_cast<EIWallPart*>(m_instances[at]);
          ERShader* otherPaper = other->GetWallpaperShader();
          if ((otherPaper && otherPaper->m_id != id) || other->m_modelId == model) break;
        }
      }
    }
  }
  m_instances.insert(m_instances.begin() + at, instance);
  ++m_wallLast;
}

// 802337D0
void ERLevel::GroupWallInstances() {}

// 80233454
// Into its room, then into the array (walls grouped by wallpaper), or the extra list; instances that
// update each frame (0x100) into the update tree.
void ERLevel::AddInstanceToLevel(EInstance* instance) {
  ValidateInstanceInLevel(instance);
  instance->m_level = this;
  AddInstanceToRoom(instance);
  if (instance->m_flags & EInstance::kListed) {
    m_listed.push_back(instance);
    instance->m_listNode = instance;
  } else if (!(instance->m_flags & EInstance::kRoomFloor)) {
    if (instance->m_wall) AddWallInstanceToLevel(instance);
    else m_instances.push_back(instance);
  }
  if (instance->m_flags & EInstance::kUpdated) {
    // ERedBlackTree::Insert keyed by GetUpdatePriority (duplicates allowed, after their equals).
    const unsigned int key = static_cast<unsigned int>(instance->GetUpdatePriority());
    auto it = std::upper_bound(m_updateMap.begin(), m_updateMap.end(), key,
                               [](unsigned int k, const std::pair<unsigned int, EInstance*>& e) { return k < e.first; });
    m_updateMap.emplace(it, key, instance);
    instance->m_nameNode = instance;
  }
}

// 80233368
// An instance drawn with the sky domes (an object whose first shader is a sky one, 0x2000): it goes in
// after the level's sky domes (the first m_skyCount instances), which it then counts with.
void ERLevel::InsertSkydomeInstance(EInstance* instance) {
  ValidateInstanceInLevel(instance);
  instance->m_level = this;
  m_instances.insert(m_instances.begin() + m_skyCount, instance);
  ++m_skyCount;
  ++m_firstObject;
  if (instance->m_flags & EInstance::kUpdated) {
    const unsigned int key = static_cast<unsigned int>(instance->GetUpdatePriority());
    auto it = std::upper_bound(m_updateMap.begin(), m_updateMap.end(), key,
                               [](unsigned int k, const std::pair<unsigned int, EInstance*>& e) { return k < e.first; });
    m_updateMap.emplace(it, key, instance);
    instance->m_nameNode = instance;
  }
}

// 802337D4
void ERLevel::PrepareInstance(EInstance* instance) {
  AddInstanceToIdMap(instance);
  instance->m_level = this;
  instance->AddedToLevel(this);
}

// 80233824
// A room's floor is its room's; listed instances join the room's list; floors register their draw
// records.
void ERLevel::AddInstanceToRoom(EInstance* instance) {
  const int room = GetRoomIndex(instance);
  instance->m_roomIndex = room;
  if (instance->m_flags & EInstance::kRoomFloor) {
    m_roomFloors[room] = instance;
    instance->m_listNode = nullptr;
    return;
  }
  if (instance->m_flags & EInstance::kInRoomList) {
    m_roomInstances[room].push_back(instance);
    instance->m_listNode = instance;
  }
  if (instance->m_flags & EInstance::kFloor) instance->RegisterFloor();
}

// 802338C8
void ERLevel::RemoveInstanceFromRoom(EInstance* instance) {
  const int room = instance->m_roomIndex;
  if (room == -1) return;
  if (instance->m_flags & EInstance::kRoomFloor) {
    m_roomFloors[room] = nullptr;
  } else {
    if (instance->m_flags & EInstance::kInRoomList) {
      auto& list = m_roomInstances[room];
      list.erase(std::remove(list.begin(), list.end(), instance), list.end());
      instance->m_listNode = nullptr;
    }
    if (instance->m_flags & EInstance::kFloor) UnregisterFloorOTDs(instance);
  }
  instance->m_roomIndex = -1;
}

// 80233964
// The instance's room from its location function; anything outside the level's rooms counts as the
// last slot (room count).
int ERLevel::GetRoomIndex(EInstance* instance) const {
  short room = 0, other = 0;
  if (instance->m_thisSideFn) instance->m_thisSideFn(instance->m_thisSideData, false, room, other);
  else other = 0;
  int index = other;
  if (index < 0 || m_roomCount <= index) index = m_roomCount;
  return index;
}

// 80233B9C
void ERLevel::AddInstanceToIdMap(EInstance* instance) {
  if (instance->m_id) m_idMap.emplace_back(instance->m_id, instance);
}

// 80233BD8
void ERLevel::RemoveInstanceFromIdMap(EInstance* instance) {
  if (!instance->m_id) return;
  for (auto it = m_idMap.begin(); it != m_idMap.end(); ++it)
    if (it->second == instance) {
      m_idMap.erase(it);
      return;
    }
}

// 80238334
void ERLevel::ClearWallIndexSaves() { m_wallFirst = m_wallLast = -1; }

// 80233F10
void ERLevel::RemoveInstance(EInstance* instance) {
  instance->AboutToBeRemovedFromLevel(this);
  if (instance->m_nameNode) {
    m_updateMap.erase(std::remove_if(m_updateMap.begin(), m_updateMap.end(),
                                   [&](const auto& e) { return e.second == instance; }),
                    m_updateMap.end());
    instance->m_nameNode = nullptr;
  }
  RemoveInstanceFromRoom(instance);
  if (instance->m_flags & EInstance::kListed) {
    m_listed.erase(std::remove(m_listed.begin(), m_listed.end(), instance), m_listed.end());
    instance->m_listNode = nullptr;
  } else if (!(instance->m_flags & EInstance::kRoomFloor)) {
    // PC guard: a sky instance (InsertSkydomeInstance) leaves the sky count (GC kept the counts).
    const auto at = std::find(m_instances.begin(), m_instances.end(), instance);
    if (at != m_instances.end() && at - m_instances.begin() < m_skyCount) {
      --m_skyCount;
      if (m_firstObject > 0) --m_firstObject;
    }
    m_instances.erase(std::remove(m_instances.begin(), m_instances.end(), instance), m_instances.end());
    m_visible.erase(std::remove(m_visible.begin(), m_visible.end(), instance), m_visible.end());
  }
  RemoveInstanceFromIdMap(instance);
  instance->m_level = nullptr;
  instance->RemovedFromLevel(this);
}

// 80234090
// (GC: the bound-tree node of old levels is checked too, without effect.)
void ERLevel::SetBounds(EInstance* instance, EBound3& bounds) {
  const float* b = reinterpret_cast<const float*>(&bounds);
  for (int i = 0; i < 6; ++i) instance->m_bounds[i] = b[i];
}

// 80238214
EIDirLight* ERLevel::GetSun() { return &m_sun; }
// 80238220
EIAmbLight* ERLevel::GetSunAmbient() { return &m_sunAmbient; }
// 8023822C
void ERLevel::SetDayNightLightTable(LevelLighting* table) { m_dayNight = table; }
// 80238238
void ERLevel::SetRainInfoTable(levelraininfo* info) { m_rainInfo = info; }
// 80238244
void ERLevel::SetSunYawOffset(float degrees) { m_sunYawOffset = degrees; }
// 80238290
int ERLevel::IsDrawingOrderTable() { return m_drawingOrderTable; }
// 802382A4
int ERLevel::GetRoomCount() { return m_roomCount; }
// 80238348
int ERLevel::GetNumInstances() { return static_cast<int>(m_instances.size()); }
// 80238354
EInstance* ERLevel::GetInstance(int index) { return m_instances[index]; }

// ---- rooms and lights ----

// 80236CA8
// Room lists, floors and floor draw lists for `count` rooms plus the outside slot; the instances added
// after loading go back into their rooms.
void ERLevel::SetRoomCount(int count) {
  m_roomInstances.clear();
  m_roomFloors.clear();
  for (EOrderTableData*& head : m_roomOrderTables) FreeOTDList(head);
  m_roomOrderTables.clear();
  m_roomCount = count;
  m_roomInstances.resize(count + 1);
  m_roomFloors.assign(count + 1, nullptr);
  m_roomOrderTables.assign(count + 1, nullptr);
  for (int i = m_firstObject; i < static_cast<int>(m_instances.size()); ++i) AddInstanceToRoom(m_instances[i]);
}

// 80236F50
void ERLevel::InitializeLightingGrid() { m_lightGrid.Flush(); }

// 80236F74
// Every light recomputes its room.
void ERLevel::ResetLightLocations() {
  for (EILight* light : m_lights) {
    short room = 0, other = 0;
    if (light->m_thisSideFn) light->m_thisSideFn(light->m_thisSideData, true, room, other);
    if (light->m_otherSideFn) light->m_otherSideFn(light->m_otherSideData, true, room, other);
  }
}

// 80234368
void ERLevel::UpdateLightGrid() {
  m_lightGrid.Update(m_lights, m_instances.data(), static_cast<int>(m_instances.size()), m_roomCount);
}

// 802343A0
// Lights, room data, then every instance in the update tree (by priority) not flagged 0x10 updates.
void ERLevel::Update() {
  UpdateLightGrid();
  UpdatePerRoomData();
  for (size_t i = 0; i < m_updateMap.size(); ++i)
    if (!(m_updateMap[i].second->m_flags & 0x10)) m_updateMap[i].second->Update();
}

// 80236268
void ERLevel::UpdatePerRoomData() { UpdateFloorLighting(); }

// 802379B4
// Up to four point lights of the grid at `pos` in `room` into `out`.
void ERLevel::GetLightsAtLocation(EVec3& pos, int room, ELights& out) {
  const short* idx = m_lightGrid.GetLightsAtLocation(pos, room);
  out.m_numPoint = 0;
  for (int i = 0; i < 4 && idx[i] != -1; ++i) {
    const ELightGrid__LightData& l = m_lightGrid.m_lights[idx[i]];
    ELightsPoint& p = out.m_point[out.m_numPoint++];
    p.m_pos[0] = l.m_dir.x;
    p.m_pos[1] = l.m_dir.y;
    p.m_pos[2] = l.m_dir.z;
    p.m_range = l.m_range;
    p.m_color[0] = l.m_color.x;
    p.m_color[1] = l.m_color.y;
    p.m_color[2] = l.m_color.z;
  }
}

// 80237918
void ERLevel::UpdateFloorLighting() {
  for (int i = 0; i < m_roomCount; ++i)
    if (m_roomFloors[i]) m_roomFloors[i]->DoLightingCalculation();
}

// 80237784
void ERLevel::OutsideLightsInit(float hour, LevelLightingEntry& entry) { InterpolateLevelLighting(m_dayNight, hour, entry); }

// 802377AC
// The day/night table at `hour` (greyed by rain, overridden by the tuning) sets the sun (direction
// from its pitch and yaw plus the yaw offset), its colour and intensity, the ambient light, the shadow
// alpha and the sky lights.
void ERLevel::OutsideLightsUpdate(float hour, LevelLightingEntry& entry) {
  InterpolateLevelLighting(m_dayNight, hour, entry);
  ApplyWeatherEffects(m_rainInfo, hour, entry);
  s_levelLightingTuning.ProcessOverrides(entry);
  const float pitch = entry.m_sunPitch * 0.017453292f;
  const float yaw = (m_sunYawOffset + entry.m_sunYaw) * 0.017453292f;
  EVec3 dir{simmy::dm::cos(pitch) * simmy::dm::cos(yaw), simmy::dm::cos(pitch) * simmy::dm::sin(yaw), simmy::dm::sin(pitch)};
  m_sun.m_dir = dir;
  m_sun.m_color = entry.m_sunColor;
  m_sun.m_intensity = entry.m_sunIntensity;
  m_sunAmbient.m_color = entry.m_ambientColor;
  m_sunAmbient.m_intensity = entry.m_ambientIntensity;
  m_shadowAlpha = entry.m_shadowAlpha;
  CalcSkyLights(entry.m_sky, dir, m_skyLights);
}

// 80237A7C
LevelLightingTuning::LevelLightingTuning() {
  const float head[17] = {1.0f, 1.0f, 0.8f, 0.2f, 1.46f, 0.94f, 1.53f, 0.05f, 0.1f,
                          0.25f, 0.32f, 0.34f, 0.45f, 0.16f, 0.0f, 0.5f, 0.2f};
  for (int i = 0; i < 17; ++i) m_field00[i] = head[i];
  m_override = 0;
  m_sky = SkyLights{{1, 1, 1}, 1, {1, 1, 1}, 1};
  m_sunPitch = 0.0f;
  m_sunYaw = 0.0f;
  m_field70 = 0.0f;
  m_sunColor = EVec3{1, 1, 1};
  m_sunIntensity = 1.0f;
  m_ambientColor = EVec3{1, 1, 1};
  m_ambientIntensity = 1.0f;
  m_field94 = EVec3{1, 1, 1};
  m_fieldA0 = 1.0f;
  m_shadowAlpha = 0.5f;
  for (int i = 0; i < 3; ++i) m_fieldA8[i] = 0.5f;
  for (int i = 3; i < 6; ++i) m_fieldA8[i] = 0.0f;
}

// 80237D04
// The tuning shows the table's values, or with m_override replaces them.
void LevelLightingTuning::ProcessOverrides(LevelLightingEntry& e) {
  auto sync = [&](auto& tuned, auto& value) {
    if (m_override) value = tuned;
    else tuned = value;
  };
  sync(m_ambientColor, e.m_ambientColor);
  sync(m_ambientIntensity, e.m_ambientIntensity);
  sync(m_shadowAlpha, e.m_shadowAlpha);
  sync(m_sunPitch, e.m_sunPitch);
  sync(m_sunYaw, e.m_sunYaw);
  sync(m_sunColor, e.m_sunColor);
  sync(m_sunIntensity, e.m_sunIntensity);
  sync(m_field94, e.m_field54);
  sync(m_fieldA0, e.m_field60);
  sync(m_sky.m_ambientColor, e.m_sky.m_ambientColor);
  sync(m_sky.m_ambientIntensity, e.m_sky.m_ambientIntensity);
  sync(m_sky.m_dirColor, e.m_sky.m_dirColor);
  sync(m_sky.m_dirIntensity, e.m_sky.m_dirIntensity);
}

// ---- drawing ----

// 80233DDC
// The list loses (and frees) the records of `instance`.
static void UnregisterFloorOTDsFromList(EOrderTableData*& head, EInstance* instance) {
  EOrderTableData** link = &head;
  while (EOrderTableData* d = *link) {
    if (d->m_instance == instance) {
      *link = d->m_next;
      delete d;
    } else {
      link = &d->m_next;
    }
  }
}

// 80233EA4
// Every room's floor list (and the outside's) loses the instance's floor records.
void ERLevel::UnregisterFloorOTDs(EInstance* instance) {
  for (EOrderTableData*& head : m_roomOrderTables) UnregisterFloorOTDsFromList(head, instance);
}

// 8023637C
// A floor record (drawn with its room's floor, before the order table) joins its instance's room list.
void ERLevel::RegisterFloor(EOrderTableData& data) {
  if (m_roomOrderTables.empty()) return;
  EOrderTableData* d = AllocAndCopyOTD(data);
  const int room = GetRoomIndex(d->m_instance);
  if (room < 0 || room >= static_cast<int>(m_roomOrderTables.size())) {  // PC guard
    delete d;
    return;
  }
  d->m_next = m_roomOrderTables[room];
  m_roomOrderTables[room] = d;
}

// indoorObjectCullDistance (804FEA10) = 30, squared by the static initialiser (804FFC4C).
const float kIndoorObjectCullDistanceSq = 30.0f * 30.0f;

// 80236288
EOrderTableData* ERLevel::AllocAndCopyOTD(EOrderTableData& data) {
  EOrderTableData* d = AllocOTD();
  *d = data;
  return d;
}

// 802363E0
// A copy of the record goes into the order table, with its distance from the eye in the ground plane.
void ERLevel::InsertInOrderTable(EOrderTableData& data) {
  EOrderTableData* d = AllocAndCopyOTD(data);
  const float dy = data.m_pos[1] - m_drawData.m_eye.y, dx = data.m_pos[0] - m_drawData.m_eye.x;
  d->m_distance = std::sqrt(dx * dx + dy * dy);
  d->m_key = static_cast<float>(data.m_sortKey);
  d->m_next = m_orderTable;
  m_orderTable = d;
  ++m_orderTableCount;
}

// 80236488
// A copy of the record into the depth-of-field foreground list (drawn after the frame effect),
// kept in order of its flags.
void ERLevel::InsertInDOFForegroundObjectList(EOrderTableData& data) {
  EOrderTableData* d = AllocAndCopyOTD(data);
  if (!d) return;
  EOrderTableData* p = m_dofForeground;
  if (!p || data.m_flags <= p->m_flags) {
    d->m_next = p;
    m_dofForeground = d;
    return;
  }
  EOrderTableData* prev;
  do {
    prev = p;
    p = prev->m_next;
  } while (p && p->m_flags < data.m_flags);
  d->m_next = prev->m_next;
  prev->m_next = d;
}

// 80236598
// Draws one record (visible or never fading): its model matrix, its lights (or the outside radiosity)
// and its instance's alpha, then the callback.
void ERLevel::DrawOrderTableEntry(EOrderTableData* data, ERC* rc) {
  EInstance* inst = data->m_instance;
  const float alpha = inst->GetInstAlpha();
  if (!(alpha > 0.0f) && inst->m_fadeMode != 0) return;
  if (data->m_callback == EIStaticModel::AnimOrderTableCallback && data->m_animController) {
    // Animated models: the controller's node matrices (a skinned model's display lists index them).
    EAnimController* anim = data->m_animController;
    if (m_drawData.m_curAnim != anim) {
      m_drawData.m_curAnim = anim;
      m_drawData.m_curMatrix = nullptr;
      const int count = anim->NodeCount();
      if (data->m_skinned) rc->ModelMatrixList(anim->m_matrices, count);
      else rc->ModelMatrices(anim->m_matrices, count);
    }
  } else if (m_drawData.m_curMatrix != data->m_matrix) {
    m_drawData.m_curMatrix = data->m_matrix;
    m_drawData.m_curAnim = nullptr;
    rc->ModelMatrices(data->m_matrix, 1);
  }
  if (!(data->m_field34 & 1)) {
    if (m_drawData.m_curLights != data->m_lights) {
      m_drawData.m_curLights = data->m_lights;
      rc->Lights(data->m_lights);
    }
  } else {
    rc->LightsRadiosity(s_outsideAmbient, s_outsideDiffuse, s_outsidePointLightDimmer);
    m_drawData.m_curLights = nullptr;
  }
  rc->SetAlpha(alpha);
  m_drawingOrderTable = 1;
  data->m_callback(m_drawData, data);
  m_drawingOrderTable = 0;
}

// 80234A70
void ERLevel::DrawOrderTableSlot(EOrderTableData* head) {
  ERC* rc = m_drawData.m_rc;
  for (EOrderTableData* d = head; d;) {
    EOrderTableData* next = d->m_next;
    DrawOrderTableEntry(d, rc);
    d = next;
  }
  rc->SetAlpha(1.0f);
}

// 802367B8
// Draws the order table back to front and empties it.
void ERLevel::DrawOrderTable() {
  if (!m_orderTableCount) return;
  ERC* rc = m_drawData.m_rc;
  std::vector<OTDRef> refs;
  refs.reserve(m_orderTableCount);
  for (EOrderTableData* d = m_orderTable; d; d = d->m_next) refs.push_back({d, d->m_distance, d->m_key});
  std::sort(refs.begin(), refs.end(), OrderTableSortReferenceLess);
  for (OTDRef& r : refs) {
    DrawOrderTableEntry(r.otd, rc);
    delete r.otd;
  }
  m_orderTableCount = 0;
  m_orderTable = nullptr;
  rc->SetAlpha(1.0f);
}

// 80236570
void ERLevel::DrawDOFForegroundObjects() { DrawOrderTableSlot(m_dofForeground); }

// 80235DA4
// An instance in a room is hidden while the room's floor is cut away above its top (portals: either
// side's floor; as on the GameCube, a portal ends up never occluded once both sides were looked at).
int ERLevel::ObjectShouldBeOccluded(EInstance* instance) {
  short room = 0, level = 0, other = 0;
  if (instance->m_thisSideFn) instance->m_thisSideFn(instance->m_thisSideData, false, room, level);
  if (level <= 0) return 0;
  if (instance->m_shaderFlags != 0) {
    if (!instance->m_otherSideFn) other = -1;
    else instance->m_otherSideFn(instance->m_otherSideData, false, room, other);
    if (other < 1) return 0;
  }
  int result = 0;
  for (int i = 0; i < 2; ++i) {
    short r;
    if (i == 0) {
      r = level;
    } else {
      if (instance->m_shaderFlags == 0) return result;
      r = other;
    }
    auto* floor = r < static_cast<int>(m_roomFloors.size()) ? static_cast<EIFloor*>(m_roomFloors[r]) : nullptr;
    if (!floor) return 0;
    if (instance->m_bounds[5] < floor->m_cutHeight) result = 1;
  }
  return 0;
}

// 802339DC
// A moved instance changes rooms when its location function now says another one.
void ERLevel::NotifyInstanceMoved(EInstance* instance) {
  if (GetRoomIndex(instance) != instance->m_roomIndex && instance->m_roomIndex != -1) {
    RemoveInstanceFromRoom(instance);
    AddInstanceToRoom(instance);
  }
}

// 802382F8
void ERLevel::RefreshRoomAssignment(EInstance* instance) {
  RemoveInstanceFromRoom(instance);
  AddInstanceToRoom(instance);
}

// 80235F14
// A point at height `z` in `room` is under the room's cut-away floor (rooms other than the outside).
bool ERLevel::ObjectShouldBeOccluded(short room, float z) {
  if (room == 0) return false;
  auto* floor = room < static_cast<int>(m_roomFloors.size()) ? static_cast<EIFloor*>(m_roomFloors[room]) : nullptr;
  return floor && z < floor->m_cutHeight;
}

// 80238388
void ERLevel::SetDrawWallShadowsFunc(DrawWallShadowsFn fn) { m_drawWallShadows = fn; }

// 80234508
// The outside objects' shadows: each object's own model flattened onto the ground along the sun (the
// level's shadow projection), when its shadow's rectangle (its bounds pushed along the sun by their
// top or bottom) is in view.
void ERLevel::DrawObjectShadowsOutside(std::vector<EInstance*>& list) {
  EVec3 sun = m_lightGrid.m_rooms[0].m_sun.m_dir;
  sun.z -= 0.4f;
  if (sun.x != 0.0f || sun.y != 0.0f || sun.z != 0.0f) {
    const float inv = 1.0f / std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    sun = EVec3{sun.x * inv, sun.y * inv, sun.z * inv};
  }
  const float k = -1.0f / sun.z;
  const float dx = sun.x * k, dy = sun.y * k;
  // The heights pushing each side of the rectangle out (the box's top or bottom, by the sun's side).
  const int x0 = dx < 0.0f ? 5 : 2, x1 = dx < 0.0f ? 2 : 5;
  const int y0 = dy < 0.0f ? 5 : 2, y1 = dy < 0.0f ? 2 : 5;
  ELevelDrawData& data = m_drawData;
  if (data.m_fieldB0 != &data.m_shadowProj) {
    data.m_fieldB0 = &data.m_shadowProj;
    data.m_rc->ViewMatrix(&data.m_shadowProj, 0, 0.0f);
  }
  for (EInstance* inst : list) {
    const float* b = inst->m_bounds;
    float rect[6] = {dx * b[x0] + b[0], dy * b[y0] + b[1], 0.0f, dx * b[x1] + b[3], dy * b[y1] + b[4], 0.0f};
    if (!m_drawData.m_win->TestAsRect(*reinterpret_cast<EBound3*>(rect)))
      static_cast<EIStaticModel*>(inst)->DrawAsShadow(data);
  }
}

// 80234740
// Inside a room, objects get a soft square under them (GLOBAL_SHADOW_SQUARE, alpha 0.35, the bounds'
// footprint scaled by 1.35) when standing within 0.4 of the floor; lit by room 1's ambient alone.
void ERLevel::DrawObjectShadowsInside(std::vector<EInstance*>& list) {
  static const float kSoftShadowAlpha = 0.35f;  // 804FEA1C s_softShadowAlpha.6642
  static const float kShadowScale = 1.35f;      // 804FEA20 s_shadowScale.6643
  if (!m_shadowDL || !m_shadowSquare) return;
  ELevelDrawData& data = m_drawData;
  ERC* rc = data.m_rc;
  ELights lights;
  for (int i = 0; i < 3; ++i) lights.m_ambient[i] = m_lightGrid.m_rooms[1].m_ambient.m_color[i];
  if (data.m_curLights != &lights) {
    data.m_curLights = &lights;
    rc->Lights(&lights);
  }
  if (EShader* shader = m_shadowSquare->GetShader()) shader->Select(rc, 0);
  rc->SetAlpha(kSoftShadowAlpha);
  EMat4 m;
  m.Id();
  m.m[2][2] = kShadowScale;
  for (EInstance* inst : list) {
    const float* b = inst->m_bounds;
    const float cx = (b[0] + b[3]) * 0.5f, cy = (b[1] + b[4]) * 0.5f;
    const float sx = kShadowScale * (b[3] - b[0]), sy = kShadowScale * (b[4] - b[1]);
    m.m[0][0] = sx;
    m.m[1][1] = sy;
    m.m[3][0] = cx;
    m.m[3][1] = cy;
    float rect[6] = {cx - sx * 0.5f, cy - sy * 0.5f, 0.0f, cx + sx * 0.5f, cy + sy * 0.5f, 0.0f};
    if (data.m_win->TestAsRect(*reinterpret_cast<EBound3*>(rect))) continue;
    EVec3 pos{0.0f, 0.0f, 0.0f};
    inst->GetPos(pos);
    if (pos.z > 0.4f) continue;
    rc->ModelMatrices(&m, 1);
    rc->DisplayList(m_shadowDL);
  }
  data.m_curShader = nullptr;
  data.m_curMatrix = nullptr;
  data.m_curAnim = nullptr;
  rc->SetAlpha(1.0f);
  data.m_curLights = nullptr;  // PC: `lights` lives on this stack frame
}

// 80235F58
// The sims' shadows in `room` (the level's listed instances are the sims' shadow instances): those
// not hidden (flag 0x100000: the sim is not drawn) and not under a cut-away floor, whose 1.4 square
// about the shadow centre is in view. Outside the shadow model draws itself (ESimShadow::DrawShadow);
// inside a room a soft round blob (GLOBAL_SHADOW_CIRCLE, alpha 0.35, unlit black) under the sim.
void ERLevel::DrawSimShadow(int room) {
  static const float kSoftShadowAlpha = 0.35f;  // 804FEA24 s_softShadowAlpha.6665
  ELevelDrawData& data = m_drawData;
  ERC* rc = data.m_rc;
  ELights lights;
  if (room != 0) {
    if (data.m_curLights != &lights) {
      data.m_curLights = &lights;
      rc->Lights(&lights);
    }
    if (data.m_curShader != m_shadowCircle) {
      data.m_curShader = m_shadowCircle;
      if (EShader* shader = m_shadowCircle ? m_shadowCircle->GetShader() : nullptr) shader->Select(rc, 0);
    }
    rc->SetAlpha(kSoftShadowAlpha);
  }
  EMat4 m;
  m.Id();
  for (EInstance* shadow : m_listed) {
    short thisRoom = 0, level = 0;
    if (shadow->m_thisSideFn) shadow->m_thisSideFn(shadow->m_thisSideData, false, thisRoom, level);
    if (room != level || (shadow->m_flags & 0x100000) || ObjectShouldBeOccluded(static_cast<short>(room), 0.0f)) continue;
    EVec3 c{0.0f, 0.0f, 0.0f};
    shadow->GetShadowCenter(c);
    float rect[6] = {c.x - 0.7f, c.y - 0.7f, 0.0f, c.x + 0.7f, c.y + 0.7f, 0.0f};
    if (data.m_win->TestAsRect(*reinterpret_cast<EBound3*>(rect))) continue;
    if (room == 0) {
      shadow->DrawShadow(data);
    } else {
      m.m[3][0] = c.x;
      m.m[3][1] = c.y;
      m.m[3][2] = c.z;
      data.m_curAnim = nullptr;
      data.m_curMatrix = &m;
      rc->ModelMatrices(&m, 1);
      rc->DisplayList(m_shadowDL);
    }
  }
  if (data.m_curLights == &lights) data.m_curLights = nullptr;  // PC: stack lifetimes
  if (data.m_curMatrix == &m) data.m_curMatrix = nullptr;
}

// 80234C98
// Draws the level into `rc` through the current 3D window:
//  - per frame: the eye, the sky matrix around it, the sun's ground shadow projection, the depth range;
//    every instance animates;
//  - the instances added after loading are culled against the window (unless never culled) and fade:
//    out quickly when crossing the near plane, towards a floor when close, else back in;
//  - sky domes (then their order table), terrain, the level's lights and the following instances;
//    per room its floor, floor records and shadows; then the instances read with the level (culled),
//    the fading ones (the sort point moves to the nearest point to the eye), the visible ones and the
//    house objects; finally the outside floor records, the order table back to front, particles and
//    depth-of-field foreground objects.
void ERLevel::Draw(ERC* rc) {
  m_drawingWindow = 0;
  // The outside radiosity light: room 0's overhead lights and the outside floor light.
  if (Room* outside = Globs_pRoomManager ? Globs_pRoomManager->GetRoom(0) : nullptr)
    s_outsidePointLightDimmer = outside->m_field48;
  EvaluateFloorLights(m_lightGrid, 0, s_outsideAmbient, s_outsideDiffuse);
  auto* win = static_cast<E3DWindow*>(EWindow::m_pCurrent3DWindow);
  g_pRC = rc;
  m_drawData.m_rc = rc;
  m_drawData.m_win = win;
  m_drawData.m_eye = win->m_lookPos;
  for (int i = 0; i < 4; ++i) m_drawData.m_skyMatrix.m[3][i] = win->m_lookAtPos.m[3][i];
  EVec3 sun = m_lightGrid.m_rooms[0].m_sun.m_dir;
  sun.z -= 0.4f;
  if (sun.x != 0.0f || sun.y != 0.0f || sun.z != 0.0f) {
    const float inv = 1.0f / std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    sun = EVec3{sun.x * inv, sun.y * inv, sun.z * inv};
  }
  m_drawData.m_sunDir = sun;
  EMat4& shadow = m_drawData.m_shadowProj;
  shadow.Id();
  const float k = -1.0f / sun.z;
  shadow.m[2][1] = sun.y / k;
  shadow.m[2][0] = sun.x / k;
  shadow.m[2][2] = 0.0f;
  EMat4 proj;
  proj.Mult4x4(shadow, win->m_lookAt);
  shadow = proj;
  m_drawData.m_curShader = nullptr;
  m_drawData.m_curMatrix = nullptr;
  m_drawData.m_fieldB0 = 0;
  m_drawData.m_curAnim = nullptr;
  m_drawData.m_curLights = nullptr;
  m_field2A188 = m_depthMax;
  float range = m_depthMax - m_depthMin;
  if (range <= 0.0f) range = 0.01f;
  m_depthMin = 9999999.0f;
  m_depthMax = -9999999.0f;
  m_depthScale = 46.0f / range;
  m_fillingOrderTable = 1;
  for (EInstance* inst : m_instances) inst->DoAnimation();
  // The rooms' floors: the outside's always drawn; an inside room's when in view (GC: with the
  // height its walls are cut away to, from the eye's side of the room).
  for (int r = 0; r < m_roomCount; ++r) {
    auto* floor = static_cast<EIFloor*>(m_roomFloors[r]);
    if (!floor) continue;
    if (r == 0) {
      floor->m_cutHeight = -100.0f;
      floor->m_hidden = 0;
    } else if (floor->VisibilityTest(*win) == 1) {
      floor->m_hidden = 1;
    } else {
      // In view: a room at least indoorObjectCullDistance (30) from the eye on both axes is cut at the
      // height where the line from the eye over its nearer corner reaches 3 (GC formula); nearer rooms
      // are not cut (-100).
      floor->m_hidden = 0;
      const float* b = floor->m_bounds;  // +0x3C min x y z, max x y z
      const EVec3& eye = win->m_lookPos;
      const float c[2] = {b[0] - eye.x, b[1] - eye.y}, f[2] = {b[3] - eye.x, b[4] - eye.y};
      const float nx = std::fabs(c[0]) < std::fabs(f[0]) ? c[0] : f[0];
      const float ny = std::fabs(c[1]) < std::fabs(f[1]) ? c[1] : f[1];
      if (kIndoorObjectCullDistanceSq <= nx * nx + ny * ny) {
        float nearer, farther;
        if (std::fabs(win->m_lookDir.x) <= std::fabs(win->m_lookDir.y)) {
          nearer = f[1], farther = c[1];
          if (std::fabs(c[1]) < std::fabs(f[1])) nearer = c[1], farther = f[1];
        } else {
          nearer = c[0], farther = f[0];
          if (std::fabs(c[0]) >= std::fabs(f[0])) nearer = f[0], farther = c[0];
        }
        floor->m_cutHeight = farther / nearer * (3.0f - eye.z) + eye.z;
      } else {
        floor->m_cutHeight = -100.0f;
      }
    }
  }

  const int count = static_cast<int>(m_instances.size());
  m_visible.clear();
  for (int i = m_firstObject; i < count; ++i) {
    EInstance* inst = m_instances[i];
    unsigned int vis = inst->m_cullable ? inst->VisibilityTest(*win) : 0;
    if ((inst->m_cullable && (vis & 1)) || ObjectShouldBeOccluded(inst)) continue;
    m_visible.push_back(inst);
    if (!inst->m_fadeMode) continue;
    if (inst->m_bounds[5] < 1.0f) vis &= ~6u;
    float dt = inst->m_fadeMode == 1 ? _dt : 10.0f;
    // An object between the eye and the point of interest (taller than the pyramid's least height)
    // fades to its floor alpha.
    float floor = 0.0f;
    if (vis == 0 && inst->m_interestFade == 1 && g_pyramidFade.m_minHeight < inst->m_bounds[5] &&
        !g_pyramidFade.IsOutsideFadeVolume(inst, inst->m_bounds)) {
      vis = 2;
      floor = g_pyramidFade.m_floor;
      if (g_pyramidFade.m_instant == 1) dt = 100000000.0f;
    }
    if (vis & 4) {
      inst->SetInstAlpha(inst->GetInstAlpha() - dt * 4.0f);
    } else if (vis & 2) {
      float a = inst->GetInstAlpha() - (dt + dt);
      if (a < floor) a = floor;
      inst->SetInstAlpha(a);
    } else if (inst->GetInstAlpha() < 1.0f) {
      inst->SetInstAlpha(dt * 3.0f + inst->GetInstAlpha());
    }
  }
  for (int i = 0; i < m_skyCount; ++i) m_instances[i]->DoLightingCalculation();
  for (EInstance* inst : m_visible) inst->DoLightingCalculation();
  // The destination alpha is the shadow stencil: the sky and the terrain keep the cleared alpha (colour
  // writes only), everything drawn after them writes alpha 0x40 (can receive shadows).
  rc->Stencil(1, 0, 0);
  if (m_skyCount) {
    for (int i = 0; i < m_skyCount; ++i) m_instances[i]->Draw(m_drawData);
    DrawOrderTable();
  }
  const int terrainEnd = m_skyCount + m_terrainCount;
  for (int i = m_skyCount; i < terrainEnd; ++i) m_instances[i]->Draw(m_drawData);
  DrawOrderTable();
  rc->Stencil(0, 0, 1);
  m_drawData.m_noOrderTable = 1;
  for (EILight* light : m_levelLights) {
    if (!(win->Test(*reinterpret_cast<EBound3*>(light->m_bounds)) & 1)) {
      light->SetInstAlpha(m_lightAlpha);
      light->Draw(m_drawData);
    }
  }
  auto* polyShadow = static_cast<ERShader*>(_shaderman.AddRef(const_cast<char*>("POLYSHADOW"), nullptr, 0));
  const int thirdEnd = terrainEnd + m_count29D7C;
  for (int i = terrainEnd; i < thirdEnd; ++i) m_instances[i]->Draw(m_drawData);
  m_fillingOrderTable = 0;
  // Each shown room: its floor (alpha 0x40: receives shadows) and floor records, then the shadows on
  // it. Shadow geometry (POLYSHADOW's shadow mask: untextured, black at half alpha, no z test) blends
  // by the destination alpha and writes alpha 0, so overlapping shadows darken once. Outside: the
  // walls' (through the house's callback), the sims' (their shadow models projected along the sun) and
  // the objects' (their own models projected); inside a room, soft blobs under sims and objects.
  // Last the floor's alpha is cleared so later rooms' shadows do not land on it.
  for (int r = 0; r < m_roomCount; ++r) {
    auto* floor = static_cast<EIFloor*>(m_roomFloors[r]);
    if (!floor || floor->m_hidden) continue;
    const bool outside = r == 0;
    rc->Stencil(0, 0, 1);
    floor->Draw(m_drawData);
    if (r < static_cast<int>(m_roomOrderTables.size())) DrawOrderTableSlot(m_roomOrderTables[r]);
    PSystemProcess(_gamedt, g_pParticleModelSystem, 0);
    EMat4 view = win->m_lookAt;
    if (EShader* shadow = polyShadow ? polyShadow->GetShader() : nullptr) {
      shadow->SetShadowAlpha(outside ? m_shadowAlpha : 0.32f);
      shadow->SelectForShadowMask(rc);
    }
    rc->Stencil(0, 2, 0);
    std::vector<EInstance*> none;
    std::vector<EInstance*>& objects = r < static_cast<int>(m_roomInstances.size()) ? m_roomInstances[r] : none;
    if (outside) {
      if (m_drawWallShadows) m_drawWallShadows(m_wallList, m_lightGrid.m_rooms[0].m_sun.m_dir, m_drawData);
      rc->DisableGeometryModes(0x4000);
      rc->EnableGeometryModes(8);
      DrawSimShadow(0);
      rc->EnableGeometryModes(0x4000);
      rc->DisableGeometryModes(8);
      DrawObjectShadowsOutside(objects);
    } else {
      rc->Stencil(1, 2, 1);
      rc->DisableGeometryModes(0x4000);
      rc->EnableGeometryModes(8);
      DrawSimShadow(r);
      DrawObjectShadowsInside(objects);
      rc->Stencil(0, 2, 0);
      rc->EnableGeometryModes(0x4000);
      rc->DisableGeometryModes(8);
    }
    rc->Stencil(0, 0, 0);
    rc->ViewMatrix(&view, 0, 0.0f);
    floor->DrawStencil(m_drawData);
    rc->DisableGeometryModes(0x4000);
    rc->EnableGeometryModes(8);
    if (outside) PSystemProcess(_gamedt, g_pParticleWeatherSystem, 0);
  }
  if (polyShadow) polyShadow->DelRef();
  m_drawData.m_noOrderTable = 0;
  m_fillingOrderTable = 1;
  for (int i = thirdEnd; i < m_firstObject; ++i) {
    EInstance* inst = m_instances[i];
    if (!(inst->VisibilityTest(*win) & 1)) inst->Draw(m_drawData);
  }
  for (EInstance* inst : m_fadeInstances) {
    const unsigned int vis = win->Test(*reinterpret_cast<EBound3*>(inst->m_bounds));
    if (vis & 1) continue;
    // Also out when the line from the eye to the point of interest crosses it, or it stands over
    // that point.
    const float* b = inst->m_bounds;
    const bool overPoint = g_lineFade.m_point[0] < b[3] && b[0] < g_lineFade.m_point[0] &&
                           g_lineFade.m_point[1] < b[4] && b[1] < g_lineFade.m_point[1];
    if (vis & 4) inst->SetInstAlpha(inst->GetInstAlpha() - _dt * 4.0f);
    else if ((vis & 2) || g_lineFade.IsLineIntersected(b) || overPoint) inst->SetInstAlpha(inst->GetInstAlpha() - (_dt + _dt));
    else if (inst->GetInstAlpha() < 1.0f) inst->SetInstAlpha(_dt * 3.0f + inst->GetInstAlpha());
    // The sort point: the point of its bounds nearest the eye (x and y).
    auto* sm = static_cast<EIStaticSubModel*>(inst);
    for (int a = 0; a < 2; ++a) {
      const float eye = win->m_lookPos[a];
      sm->m_sphere[a] = eye < inst->m_bounds[a] ? inst->m_bounds[a] : (inst->m_bounds[3 + a] < eye ? inst->m_bounds[3 + a] : eye);
    }
    inst->Draw(m_drawData);
  }
  for (EInstance* inst : m_visible) inst->Draw(m_drawData);
  while (EOrderTableData* d = m_houseObjects) {
    m_houseObjects = d->m_next;
    InsertInOrderTable(*d);
  }
  if (_gamedt > 0.0f && EInstance::GetContext() == 0) Effects__EffectsManager::GetSingleton()->Update(_gamedt);
  GameEffectsManager::GetSingleton();
  GameEffectsManager::PreRender();
  m_fillingOrderTable = 0;
  DrawOrderTableSlot(m_roomOrderTables[m_roomCount]);
  DrawOrderTable();
  g_pRC = rc;
  PSystemProcess(_gamedt, nullptr, 0);
  m_drawingWindow = 1;
  // The current camera director's depth of field over what is drawn so far; the foreground list
  // (the sims of the social interaction, their effects) goes on top, sharp.
  if (_globals.m_cameraDirector)
    if (FrameEffectsManager* fx = _globals.m_cameraDirector->GetFrameEffectsManager())
      if (DepthOfField* dof = fx->DepthOfFieldObject()) dof->Update(_dt, rc);
  DrawDOFForegroundObjects();
  FreeOTDList(m_dofForeground);
  g_pRC = nullptr;
}

// ---- the level manager ----

// 8022F944
EResource* ELevelManager::AllocateAndLoadResource(EFile* file, unsigned int, unsigned int id) {
  ERLevel* level = ERLevel::AllocAndLoadLevel(*file);
  if (level) level->m_id = id;
  return level;
}
// 8022F9D4
EResource* ELevelManager::AddRef(unsigned int id, EFile* file, int pos) { return EResourceManager::AddRef(id, file, pos); }
// 8022F9F4
EResource* ELevelManager::AddRef(char* name, EFile* file, int pos) { return EResourceManager::AddRef(name, file, pos); }
