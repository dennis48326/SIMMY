// Port of game/e_lightgrid.cpp: which lights light which room, and where. Each room takes the
// lights located in it (its ambient and sun directly, its point lights and portal lights as ranges of
// the light table). A 63x63 grid of vertices (one per tile corner) knows the rooms touching it and,
// per room, its four strongest lights (up to two point lights, the rest portal lights; strength =
// range squared over the squared distance in the ground plane). Portal lights (windows) carry the
// light of the room beyond them in: the ambient and sun outside, or the nearest lights of the other
// room. Objects and sims take the lights of the vertex nearest them (GetLightsAtLocation).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/epointlight.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
float k_maxPortalColor = 1.0f;       // 804FEA08
float sAmbientPortalScaler = 1.0f;   // 804FEA0C
}  // namespace

// 8022FA6C
ELightGrid::ELightGrid() { Flush(); }

// 8022FAF4
// Every vertex in no room; every room without lights.
void ELightGrid::Flush() {
  for (auto& column : m_vertices)
    for (VertexData& v : column)
      for (signed char& r : v.m_rooms) r = -1;
  for (RoomData& r : m_rooms) r.Flush();
  m_framesSince = 0;
  m_pointFirst = 0x100;
  m_dirty = 1;
  m_roomCount = 0;
  m_portalEnd = 0;
  m_portalFirst = 0;
  m_pointEnd = 0x100;
  m_stable = 0;
}

// 8022FBC4
void ELightGrid__RoomData::Flush() {
  m_sun.Clear();
  m_ambient.Clear();
  m_pointFirst = 0x100;
  m_portalFirst = 0;
  m_portalEnd = 0;
  m_pointEnd = 0x100;
}

// 80230F8C
void ELightGrid__LightData::Clear() {
  m_dir = EVec3{};
  m_color = EVec3{};
  m_light = nullptr;
  m_range = 0.0f;
}

// 8022FC14
// `room` touches vertex (x, y) (at most four rooms; its lights are found again). The grid is rebuilt.
void ELightGrid::AddRoomToVertex(int x, int y, int room) {
  if (static_cast<unsigned int>(x) > 0x3e || y < 0 || y > 0x3e) return;
  VertexData& v = m_vertices[x][y];
  int i = 0;
  while (v.m_rooms[i] != -1 && v.m_rooms[i] != room) {
    if (++i > 3) {
      m_dirty = 1;
      return;
    }
  }
  v.m_rooms[i] = static_cast<signed char>(room);
  for (short& l : v.m_lights[i]) l = -1;
  m_dirty = 1;
}

// 8022FCAC
// When lights changed (or the grid was flushed): each room takes the lights located in it, its
// ambient and directional light directly and its point and portal lights as ranges of the light
// table; the portal lights find the lights beyond them (an inside room's portal scale: the light its
// windows let in over its area, at most 1, times the tuning's 0.2); then the vertices take their
// nearest lights. Otherwise only the evaluation runs (the grid counts as stable after two quiet
// frames following a rebuild).
void ELightGrid::Update(std::vector<EILight*>& lights, EInstance**, int, int roomCount) {
  if (!m_dirty && !EILight::m_haveLightsChanged) {
    EvaluateLights();
    if (m_rebuilds > 1) {
      if (m_framesSince < 2) ++m_framesSince;
      else m_stable = 1;
    }
    return;
  }
  m_dirty = 0;
  EILight::m_haveLightsChanged = 0;
  m_framesSince = 0;
  ++m_rebuilds;
  if (lights.empty() || roomCount == 0) return;
  m_portalFirst = 0;
  m_pointFirst = 0x100;
  m_portalEnd = 0;
  m_pointEnd = 0x100;
  m_roomCount = roomCount;
  for (int room = 0; room < roomCount && room < kMaxRooms; ++room) {
    RoomData& r = m_rooms[room];
    r.m_portalFirst = m_portalEnd;
    r.m_pointFirst = m_pointEnd;
    for (EILight* light : lights) {
      short here = 0, other = 0;
      if (light->m_thisSideFn) light->m_thisSideFn(light->m_thisSideData, false, here, other);
      if (other != room) continue;
      switch (light->GetLightType()) {
        case EILight::kPortal: {
          short a = -1, b = -1;
          if (light->m_otherSideFn) light->m_otherSideFn(light->m_otherSideData, false, a, b);
          if (other != b && b >= 0 && b < roomCount && m_portalEnd < 0x100) m_lights[m_portalEnd++].m_light = light;
          break;
        }
        case EILight::kAmbient: r.m_ambient.m_light = light; break;
        case EILight::kDirectional: r.m_sun.m_light = light; break;
        case EILight::kPoint:
          if (m_pointEnd < 0x200) m_lights[m_pointEnd++].m_light = light;
          break;
        default: break;
      }
    }
    r.m_portalEnd = m_portalEnd;
    r.m_pointEnd = m_pointEnd;
    EvaluatePointLights(r.m_pointFirst, m_pointEnd);
    r.EvaluateAmbientAndDirectionalLights();
  }
  for (int room = 0; room < roomCount && room < kMaxRooms; ++room) {
    RoomData& r = m_rooms[room];
    const float lumina = ConnectPortalLights(&r);
    if (room == 0) {
      r.m_portalScale = 1.0f;
    } else {
      Room* rm = RoomManager::GetRoomManager()->GetRoom(static_cast<unsigned short>(room));
      const int area = rm ? rm->GetArea() : 0;
      float k = lumina / static_cast<float>(area);
      if (1.0f < k) k = 1.0f;
      r.m_portalScale = k * ERLevel::s_levelLightingTuning.m_field00[16];
    }
    EvaluatePortalLights(r.m_portalFirst, r.m_portalEnd);
  }
  AddLightsToVertices();
  EvaluateOuterLotLights();
  // PC development aid: SIMMY_LIGHT_TRACE=1 logs each rebuild (lights per room, vertices in rooms).
  static const bool trace = std::getenv("SIMMY_LIGHT_TRACE") != nullptr;
  if (trace) {
    int vertices = 0, lit = 0;
    for (auto& column : m_vertices)
      for (VertexData& v : column)
        if (v.m_rooms[0] != -1) {
          ++vertices;
          if (v.m_lights[0][0] != -1) ++lit;
        }
    std::printf("[light] grid rebuild %d: %d rooms, %d point lights, %d portal lights, %d vertices in rooms (%d lit)\n",
                m_rebuilds, roomCount, m_pointEnd - 0x100, m_portalEnd, vertices, lit);
    for (int room = 0; room < roomCount && room < kMaxRooms; ++room) {
      const RoomData& r = m_rooms[room];
      for (int i = r.m_pointFirst; i < r.m_pointEnd; ++i)
        std::printf("[light]   room %d point light at %.2f %.2f %.2f range %.2f colour %.2f %.2f %.2f\n", room,
                    m_lights[i].m_dir.x, m_lights[i].m_dir.y, m_lights[i].m_dir.z, m_lights[i].m_range,
                    m_lights[i].m_color.x, m_lights[i].m_color.y, m_lights[i].m_color.z);
    }
  }
}

// 8022FFE0
// Each of the room's portal lights finds the eight lights nearest it in the room beyond; returns the
// brightness its windows let in from outside.
float ELightGrid::ConnectPortalLights(RoomData* room) {
  float lumina = 0.0f;
  for (int i = room->m_portalFirst; i < room->m_portalEnd; ++i) {
    EILight* light = m_lights[i].m_light;
    PortalData& portal = m_portals[i];
    EVec3 pos;
    light->GetPosition(pos);
    LightWeight nearest[8];
    short a = -1, beyond = -1;
    if (light->m_otherSideFn) light->m_otherSideFn(light->m_otherSideData, false, a, beyond);
    if (beyond == 0) {
      short b = 0, here = 0;
      if (light->m_thisSideFn) light->m_thisSideFn(light->m_thisSideData, false, b, here);
      if (here != beyond) lumina += static_cast<EIPortalPointLight*>(light)->GetLumina();
    }
    if (beyond >= 0 && beyond < kMaxRooms) {
      RoomData& other = m_rooms[beyond];
      other.FindClosestLights(pos, other.m_pointFirst, other.m_pointEnd, m_lights, nearest, 8);
    }
    for (int k = 0; k < 8; ++k) portal.m_lights[k] = nearest[k].m_index == -1 ? nullptr : &m_lights[nearest[k].m_index];
  }
  return lumina;
}

// 802301AC
// The `count` strongest of lights [first, end) at `pos` (range squared over the squared distance in
// the ground plane), strongest first, into `out` (which keeps what it held when nothing beats it).
void ELightGrid__RoomData::FindClosestLights(EVec3& pos, int first, int end, ELightGrid__LightData* lights,
                                             ELightGrid__LightWeight* out, int count) {
  for (int i = first; i < end; ++i) {
    const ELightGrid__LightData& l = lights[i];
    const float dy = pos.y - l.m_dir.y, dx = pos.x - l.m_dir.x;
    float sq = dx * dx + dy * dy;
    if (sq == 0.0f) sq = 0.0001f;
    const float w = (l.m_range * l.m_range) / sq;
    for (int k = 0; k < count; ++k) {
      if (w <= out[k].m_weight) continue;
      for (int j = count - 1; j > k; --j) out[j] = out[j - 1];
      out[k].m_index = static_cast<short>(i);
      out[k].m_weight = w;
      break;
    }
  }
}

// 802302B4
// Point lights: position, range and colour (each channel kept in 0..2).
int ELightGrid::EvaluatePointLights(int first, int end) {
  for (int i = first; i < end; ++i) {
    LightData& l = m_lights[i];
    EVec3 unused;
    l.m_light->LightingParameters(unused, l.m_range, l.m_dir, l.m_color);
    for (int c = 0; c < 3; ++c) {
      if (l.m_color[c] < 0.0f) l.m_color[c] = 0.0f;
      else if (l.m_color[c] > 2.0f) l.m_color[c] = 2.0f;
    }
  }
  return 0;
}

// 802303D4
// Every inner vertex, for each room touching it: up to two of the room's point lights, then its
// portal lights, the four strongest in order.
void ELightGrid::AddLightsToVertices() {
  for (int x = 2; x < 0x3e; ++x) {
    for (int y = 2; y < 0x3e; ++y) {
      EVec3 pos{static_cast<float>(x), static_cast<float>(y), 0.0f};
      VertexData& v = m_vertices[x][y];
      for (int slot = 0; slot < 4; ++slot) {
        const int room = v.m_rooms[slot];
        if (room == -1) break;
        LightWeight best[4];
        for (LightWeight& b : best) b.m_index = -1, b.m_weight = -1.0f;
        RoomData& r = m_rooms[room];
        r.FindClosestLights(pos, r.m_pointFirst, r.m_pointEnd, m_lights, best, 4);
        int points = 0;
        if (best[0].m_index != -1)
          while (++points <= 3 && best[points].m_index != -1) {}
        if (points > 2) points = 2;
        r.FindClosestLights(pos, r.m_portalFirst, r.m_portalEnd, m_lights, best + points, 4 - points);
        for (int k = 0; k < 4; ++k) {
          int pick = 0;
          float w = -2.0f;
          for (int j = 0; j < 4; ++j)
            if (w < best[j].m_weight) pick = j, w = best[j].m_weight;
          best[pick].m_weight = -2.0f;
          v.m_lights[slot][k] = best[pick].m_index;
        }
      }
    }
  }
}

// 80230608
void ELightGrid::EvaluateLights() {
  EvaluatePointLights(m_pointFirst, m_pointEnd);
  EvaluateAmbientAndDirectionalLights();
  EvaluatePortalLights(m_portalFirst, m_portalEnd);
  EvaluateOuterLotLights();
}

// 8023065C
// The outer lot (room 0) lights the outside: its ambient colour and its sun.
void ELightGrid::EvaluateOuterLotLights() {
  if (m_roomCount == 0) return;
  const RoomData& outside = m_rooms[0];
  for (int i = 0; i < 3; ++i) {
    m_outside.m_ambient[i] = outside.m_ambient.m_color[i];
    m_outside.m_dir[0].m_color[i] = outside.m_sun.m_color[i];
    m_outside.m_dir[0].m_dir[i] = outside.m_sun.m_dir[i];
  }
  m_outside.m_numPoint = 0;
  m_outside.m_numDir = 1;
}

// 802306E4
void ELightGrid::EvaluateAmbientAndDirectionalLights() {
  for (int i = 0; i < m_roomCount && i < kMaxRooms; ++i) m_rooms[i].EvaluateAmbientAndDirectionalLights();
}

// 8023073C
void ELightGrid__RoomData::EvaluateAmbientAndDirectionalLights() {
  if (m_ambient.m_light)
    m_ambient.m_light->LightingParameters(m_ambient.m_dir, m_ambient.m_range, m_ambient.m_dir, m_ambient.m_color);
  if (m_sun.m_light) {
    EVec3 unused;
    m_sun.m_light->LightingParameters(m_sun.m_dir, m_sun.m_range, unused, m_sun.m_color);
  }
}

// 802307CC
void ELightGrid::EvaluatePortalLights(int first, int end) {
  for (int i = first; i < end; ++i) EvaluatePortalLight(&m_lights[i], &m_portals[i]);
}

// 80230838
// A portal light's colour: from outside, the outside ambient (x the portal scaler) and sun; from
// another room, its nearest lights (each by range squared over the squared distance, at most 1);
// normalised so its strongest channel is k_maxPortalColor. Then its position, range and colour.
void ELightGrid::EvaluatePortalLight(LightData* l, PortalData* portal) {
  EVec3 c{0.0f, 0.0f, 0.0f};
  auto* light = static_cast<EIPortalPointLight*>(l->m_light);
  short a = -1, beyond = -1;
  if (light->m_otherSideFn) light->m_otherSideFn(light->m_otherSideData, false, a, beyond);
  bool fromOutside = false;
  if (light->GetRoomId() == 0) {
    if (beyond != 0) {
      const RoomData& r = m_rooms[beyond];
      c.x = r.m_ambient.m_color.x * sAmbientPortalScaler + 0.0f;
      c.y = r.m_ambient.m_color.y * sAmbientPortalScaler + 0.0f;
      c.z = r.m_ambient.m_color.z * sAmbientPortalScaler + 0.0f;
    } else {
      fromOutside = true;
    }
  } else if (beyond == 0) {
    fromOutside = true;
  }
  if (fromOutside) {
    const RoomData& r = m_rooms[0];
    c.x += r.m_sun.m_color.x;
    c.y += r.m_sun.m_color.y;
    c.z += r.m_sun.m_color.z;
  }
  const EVec3 p = l->m_dir;
  for (int k = 0; k < 8 && portal->m_lights[k]; ++k) {
    const LightData* n = portal->m_lights[k];
    const EVec3 d{n->m_dir.x - p.x, n->m_dir.y - p.y, n->m_dir.z - p.z};
    float sq = d.x * d.x + d.y * d.y + d.z * d.z;
    if (sq == 0.0f) sq = 9.999999747378752e-05f;
    float w = (n->m_range * n->m_range) / sq;
    if (1.0f <= w) w = 1.0f;
    c.x += w * n->m_color.x;
    c.y += w * n->m_color.y;
    c.z += w * n->m_color.z;
  }
  float m = k_maxPortalColor;
  if (m < c.x) m = c.x;
  if (m < c.y) m = c.y;
  if (m < c.z) m = c.z;
  const float k = k_maxPortalColor / m;
  light->m_color = EVec3{c.x * k, c.y * k, c.z * k};
  EVec3 unused;
  light->LightingParameters(unused, l->m_range, l->m_dir, l->m_color);
}

namespace {
// 80230B38
// The lights of `room` at vertex `v`, each with `weight`, merged into `out` (adding to a light
// already there).
void MergeLightWeights(float weight, int room, const ELightGrid__VertexData* v, ELightGrid__LightWeight* out, int& count) {
  int slot = 0;
  while (v->m_rooms[slot] != room)
    if (++slot == 4) return;
  for (int k = 0; k < 4; ++k) {
    const short idx = v->m_lights[slot][k];
    if (idx == -1) return;
    int j = 0;
    while (j < count && out[j].m_index != idx) ++j;
    if (j < count) {
      out[j].m_weight += weight;
    } else {
      out[count].m_index = idx;
      out[count].m_weight = weight;
      ++count;
    }
  }
}
}  // namespace

// 80230C2C
// The lights of `room` at `pos` weighted by the triangle of the grid cell it falls in (barycentric:
// the corner diagonal from (0,0)/(1,1) and the two others).
void ELightGrid::GetLightWeightsAtLocation(EVec3& pos, int room, LightWeight* out) {
  const int x = static_cast<int>(pos.x), y = static_cast<int>(pos.y);
  float fx = pos.x - static_cast<float>(x);
  const float fy = pos.y - static_cast<float>(y);
  if (static_cast<unsigned int>(x) > 0x3e || y < 0 || y > 0x3e) return;
  // PC: the last row and column have no cell beyond them (GC read past the grid).
  if (x == 0x3e || y == 0x3e) return;
  float wy = 1.0f - fx;
  float w0 = wy - fy;
  const VertexData* corner;
  if (w0 <= 0.0f) {
    fx = 1.0f - fy;
    w0 = -w0;
    corner = &m_vertices[x + 1][y + 1];
  } else {
    corner = &m_vertices[x][y];
    wy = fy;
  }
  int count = 0;
  for (int slot = 0; slot < 4; ++slot) {
    if (corner->m_rooms[slot] != room) continue;
    for (int k = 0; k < 4 && corner->m_lights[slot][k] != -1; ++k) {
      out[count].m_index = corner->m_lights[slot][k];
      out[count].m_weight = w0;
      ++count;
    }
    break;
  }
  MergeLightWeights(fx, room, &m_vertices[x + 1][y], out, count);
  MergeLightWeights(wy, room, &m_vertices[x][y + 1], out, count);
}

short ELightGrid::m_lightVertexDummy[4] = {-1, -1, -1, -1};  // 804FFC44 (set by the constructor)

// 80230E28
// The lights (up to four indices into the light table, -1 ended) of the grid vertex nearest `pos`
// for `room`.
const short* ELightGrid::GetLightsAtLocation(EVec3& pos, int room) const {
  int x = static_cast<int>(pos.x + 0.5f), y = static_cast<int>(pos.y + 0.5f);
  if (x < 0) x = 0;
  else if (x > 0x3e) x = 0x3e;
  if (y < 0) y = 0;
  else if (y > 0x3e) y = 0x3e;
  const VertexData& v = m_vertices[x][y];
  for (int slot = 0; slot < 4; ++slot)
    if (v.m_rooms[slot] == room) return v.m_lights[slot];
  return m_lightVertexDummy;
}

// 80230EF0
int ELightGrid::IsRoomAtLocation(EVec3& pos, int room) {
  const int x = static_cast<int>(pos.x + 0.5f), y = static_cast<int>(pos.y + 0.5f);
  if (static_cast<unsigned int>(x) < 0x3f && y >= 0 && y < 0x3f)
    for (signed char r : m_vertices[x][y].m_rooms)
      if (r == room) return 1;
  return 0;
}

// 80230FFC
void ELightGrid::EvaluateLamps() {
  EvaluatePointLights(m_pointFirst, m_pointEnd);
  EvaluatePortalLights(m_portalFirst, m_portalEnd);
}

// 80231040
// The lamps at their full colour (kept in 0..2), then the portal lights.
void ELightGrid::EvaluateLampsOn() {
  for (int i = m_pointFirst; i < m_pointEnd; ++i) {
    LightData& l = m_lights[i];
    for (int c = 0; c < 3; ++c) {
      const float v = l.m_light->m_color[c];
      l.m_color[c] = v < 0.0f ? 0.0f : (v > 2.0f ? 2.0f : v);
    }
  }
  EvaluatePortalLights(m_portalFirst, m_portalEnd);
}
