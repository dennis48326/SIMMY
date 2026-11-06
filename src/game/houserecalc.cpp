// The house rebuilt after the build tools change its walls or floors: game/eorhouse.cpp's
// EHouse::ReCalcHouse and game/global.cpp's EGlobal::RecalcHouse. (Kept out of livemode.cpp, where the
// rest of EHouse is: a file shared with the world side.)
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>

// 80024E04
// The walls, floors and room lights thrown away and made again from the world (the rooms as last
// computed): one more room than the highest inside room, the level's wall range reset, a new ERoom
// (ERoom::Init: the wall runs and their parts), the walls grouped; in a house (not a lot) the lights'
// locations, the level's room count, the room lights and the floors; then the object models placed
// again (ReOrientHouse).
void EHouse::ReCalcHouse() {
  if (!m_level || !m_ready) return;
  _pGfx->Flush();
  CleanUpRoomLights();
  DestroyWalls();
  EIFloor::DestroyFloors();
  unsigned int highest = 0;
  if (Globs_pRoomManager)
    for (auto& entry : Globs_pRoomManager->m_rooms) {
      Room* room = entry.second;
      if (room && room->m_inUse && !room->IsOutside() && highest < entry.first) highest = entry.first;
    }
  m_roomCount = static_cast<int>(highest) + 1;
  m_level->ClearWallIndexSaves();  // GC: level +0x2C2BC / +0x2C2C0 = -1
  m_room = new ERoom;
  m_room->Init();
  ERLevel::GroupWallInstances();
  if (!m_isLot) {
    m_level->ResetLightLocations();
    if (m_roomCount != m_level->m_roomCount) m_level->SetRoomCount(m_roomCount);
    InitRoomLighting();
    EIFloor::CreateFloors(this);
    // GC: Globs::pObjectModule vtable 0x27C UpdateInteractionInfluences (the world side's; not ported).
  }
  if (m_objectMan) m_objectMan->ReOrientHouse(false);
  // PC development aid: SIMMY_BUILD_TRACE=1 reports the rebuilt house.
  if (std::getenv("SIMMY_BUILD_TRACE"))
    std::printf("[build] house rebuilt: %d wall runs, %d wall tiles, %d fence tiles, %d rooms, level instances %d (walls "
                "%d..%d)\n",
                ERoomWall::m_wallCount, ERoomWall::m_tileCount, EIFenceWall::m_nInstances, m_roomCount,
                m_level->GetNumInstances(), m_level->m_wallFirst, m_level->m_wallLast);
}

// 80041A7C
// The current house rebuilt, then its objects' models placed again (EIObjectMan::PostLoad).
void EGlobal::RecalcHouse() {
  if (!m_house) return;
  m_house->ReCalcHouse();
  if (m_house->m_objectMan) m_house->m_objectMan->PostLoad();
}
