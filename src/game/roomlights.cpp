// The rooms' lights: game/rooms.cpp's light functions of the room manager (a room's light colour
// scale, set by the lighting primitive and applied by the house), game/eorhouse.cpp's
// EHouse::UpdateRoomLights, game/e_rlevel.cpp's ERLevel::UpdateLightsIntensityScale and
// game/ifloor.cpp's EvaluateFloorLights.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/epointlight.hpp"

#include <cstdlib>

// 801360B0
// The room's statistics (and its lights) are to be recomputed.
void RoomManager::RoomLightingChanged(int id) {
  auto it = m_rooms.find(static_cast<unsigned short>(id));
  if (it == m_rooms.end()) return;
  it->second->m_statsDirty = 1;
  m_dirty = 1;
}

// 8013614C
void RoomManager::AllRoomsLightingChanged() {
  for (auto& r : m_rooms)
    if (r.second->m_inUse) r.second->m_statsDirty = 1;
  m_dirty = 1;
}

// 80137234
// Every room in use of lighting group `group` (or with `others`, of every other group) takes the
// light scale `scale` (all channels).
void RoomManager::SetRoomIntensityScale(int group, bool others, float scale) {
  RoomManager* mgr = GetRoomManager();
  for (auto& it : mgr->m_rooms) {
    Room* r = it.second;
    if (!r || !r->m_inUse) continue;
    const bool same = r->m_id == static_cast<unsigned short>(group);
    if ((same && !others) || (!same && others)) {
      r->m_field38 = scale;
      r->m_field40 = scale;
      r->m_field3C = scale;
      r->m_field44 = 1;  // GC: the two shorts at +0x44: 0, 1
    }
  }
}

// 80137318
// As SetRoomIntensityScale for one channel (0..2).
void RoomManager::SetRoomIntensityColorScale(int group, bool others, int channel, float scale) {
  RoomManager* mgr = GetRoomManager();
  for (auto& it : mgr->m_rooms) {
    Room* r = it.second;
    if (!r || !r->m_inUse) continue;
    const bool same = r->m_id == static_cast<unsigned short>(group);
    if ((same && !others) || (!same && others)) {
      float c[3] = {r->m_field38, r->m_field3C, r->m_field40};
      if (channel >= 0 && channel < 3) c[channel] = scale;
      r->m_field38 = c[0];
      r->m_field40 = c[2];
      r->m_field3C = c[1];
      r->m_field44 = 1;
    }
  }
}

// 80138854
// The room whose lights light room `id`: its lighting group's room.
RET_UNKNOWN GetLightRoomID(unsigned short id) {
  RoomManager* mgr = Globs_pRoomManager;
  if (Room* r = mgr ? mgr->GetRoom(id) : nullptr)
    if (Room* g = mgr->GetRoom(r->m_lightingGroup)) return g->m_id;
  return id;
}

// 80024F98
// The rooms' overhead lights by the hour (on outside 7:00..18:59 off); each room whose light scale
// changed passes it to the lights located in it.
void EHouse::UpdateRoomLights() {
  // PC development aid: SIMMY_HOUR=<0..23> sets the clock's hour once, at the first update (to look
  // at the lights by night).
  static const char* setHour = std::getenv("SIMMY_HOUR");
  if (setHour && Globs_pSimulator) {
    Globs_pSimulator->SetGlobal(cSimulatorImpl::kHour, static_cast<short>(std::atoi(setHour)));
    setHour = nullptr;
  }
  if (m_level && m_level->m_field29D70) m_level->m_field29D70 = static_cast<int>(ERLevel::s_levelLightingTuning.m_field00[14] == 0.0f);
  const int hour = SimulatorHour();
  const float overhead = static_cast<unsigned int>(hour - 7) < 0xc ? 0.0f : 1.0f;
  RoomManager* mgr = Globs_pRoomManager;
  if (!mgr) return;
  for (auto& it : mgr->m_rooms) {
    Room* r = it.second;
    if (!r) continue;
    r->m_field48 = overhead;
    Room* group = mgr->GetRoom(r->m_lightingGroup);
    if (!r->m_inUse || !group || !group->m_field44) continue;
    // GC: the outside's (room 0) scale is also kept at ERLevel +0x2E38C (read by nothing).
    EVec3 scale{group->m_field38, group->m_field3C, group->m_field40};
    if (m_level) m_level->UpdateLightsIntensityScale(group->m_id, scale);
    group->m_field44 = 0;
  }
}

// 80236E88
// The point lights located in `room` take the intensity scale.
void ERLevel::UpdateLightsIntensityScale(int room, EVec3 scale) {
  if (room < 0 || m_roomCount < room) return;
  const ELightGrid__RoomData& r = m_lightGrid.m_rooms[room];
  for (int i = r.m_pointFirst; i < r.m_pointEnd; ++i) {
    EILight* light = m_lightGrid.m_lights[i].m_light;
    light->m_intensityScale = scale;
    light->m_useIntensityScale = 1;
  }
}

// 80236F0C
int ERLevel::IsRoomAt(EVec3& pos, int room) {
  if (room < 0 || m_roomCount <= room) return 0;
  return m_lightGrid.IsRoomAtLocation(pos, room);
}

// 800461E0
// A room's floor light: its ambient colour, and its sun's colour scaled by how much it shines down.
void EvaluateFloorLights(ELightGrid& grid, int room, EVec3& ambient, EVec3& diffuse) {
  const ELightGrid__RoomData& r = grid.m_rooms[room];
  if (!r.m_ambient.m_light) ambient = EVec3{0.0f, 0.0f, 0.0f};
  else ambient = r.m_ambient.m_color;
  if (!r.m_sun.m_light) {
    diffuse = EVec3{0.0f, 0.0f, 0.0f};
  } else {
    float down = -r.m_sun.m_dir.z;
    if (down < 0.0f) down = 0.0f;
    diffuse = EVec3{down * r.m_sun.m_color.x, down * r.m_sun.m_color.y, down * r.m_sun.m_color.z};
  }
}
