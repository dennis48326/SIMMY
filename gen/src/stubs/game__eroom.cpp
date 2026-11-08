// GENERATED stubs for unported functions of game/eroom.cpp. Do not edit.
#include "simmy/types_gen.hpp"

RET_UNKNOWN HasWallsNotFences(TileWalls &) { SIMMY_STUB("HasWallsNotFences(TileWalls &)", 0x80027358); return {}; }
RET_UNKNOWN EIWallPart::IsOpaque() { SIMMY_STUB("EIWallPart::IsOpaque(void)", 0x800274A4); return {}; }
RET_UNKNOWN ERoomWall::IsWallsOpaque(int) { SIMMY_STUB("ERoomWall::IsWallsOpaque(int)", 0x800295AC); return {}; }
RET_UNKNOWN ERoomWall::CalWallsOpaque(int) { SIMMY_STUB("ERoomWall::CalWallsOpaque(int)", 0x800295BC); return {}; }
double ERoomWall::CalShortDistToCam(int) { SIMMY_STUB("ERoomWall::CalShortDistToCam(int)", 0x80029654); return {}; }
RET_UNKNOWN operator>>(EStream &, EIFenceWall *&) { SIMMY_STUB("operator>>(EStream &, EIFenceWall *&)", 0x80029810); return {}; }
RET_UNKNOWN operator>>(EFile &, EIFenceWall *&) { SIMMY_STUB("operator>>(EFile &, EIFenceWall *&)", 0x80029850); return {}; }
RET_UNKNOWN operator>>(EStream &, EIWallPart *&) { SIMMY_STUB("operator>>(EStream &, EIWallPart *&)", 0x80029CAC); return {}; }
RET_UNKNOWN operator>>(EFile &, EIWallPart *&) { SIMMY_STUB("operator>>(EFile &, EIWallPart *&)", 0x80029CEC); return {}; }
void LightVertex(EVec4 &, EVec4 &, float, unsigned int *, ELights &, EMat4 *) { SIMMY_STUB("LightVertex(EVec4 &, EVec4 &, float, unsigned int *, ELights &, EMat4 *)", 0x8002AB14); }
RET_UNKNOWN Light(bool, float, unsigned int *, ESMSStrip *, ELights &, EMat4 *) { SIMMY_STUB("Light(bool, float, unsigned int *, ESMSStrip *, ELights &, EMat4 *)", 0x8002AF58); return {}; }
RET_UNKNOWN EIWallPart::GetAdjacentRoom() { SIMMY_STUB("EIWallPart::GetAdjacentRoom(void)", 0x8002B99C); return {}; }
RET_UNKNOWN ERoomWall::DrawWall(ERC *) { SIMMY_STUB("ERoomWall::DrawWall(ERC *)", 0x8002CE4C); return {}; }
RET_UNKNOWN ERoomWall::getRoomIdFromPoint(CTilePt &) { SIMMY_STUB("ERoomWall::getRoomIdFromPoint(CTilePt &)", 0x8002D1F8); return {}; }
RET_UNKNOWN ERoomWall::GetWallPaperCost(int, unsigned short) { SIMMY_STUB("ERoomWall::GetWallPaperCost(int, unsigned short)", 0x8002DE70); return {}; }
RET_UNKNOWN ERoom::GetWallPaperCost(unsigned int, unsigned short) { SIMMY_STUB("ERoom::GetWallPaperCost(unsigned int, unsigned short)", 0x8002DFEC); return {}; }
RET_UNKNOWN ERoom::IsRoomWallsOpaque(int, int) { SIMMY_STUB("ERoom::IsRoomWallsOpaque(int, int)", 0x8002E168); return {}; }
void ERoom::CalAllRoomOpaque(int) { SIMMY_STUB("ERoom::CalAllRoomOpaque(int)", 0x8002E1C4); }
double ERoom::GetShortDistToCam(int, int) { SIMMY_STUB("ERoom::GetShortDistToCam(int, int)", 0x8002E268); return {}; }
double ERoom::GetOccludeAlpha(int, int) { SIMMY_STUB("ERoom::GetOccludeAlpha(int, int)", 0x8002E2C4); return {}; }
RET_UNKNOWN ERoom::IsRoomRoofOpaque(int, int) { SIMMY_STUB("ERoom::IsRoomRoofOpaque(int, int)", 0x8002E324); return {}; }
void ERoom::CalRoofOpaque(int) { SIMMY_STUB("ERoom::CalRoofOpaque(int)", 0x8002E380); }
void ERoom::CalShortDistToCam(int) { SIMMY_STUB("ERoom::CalShortDistToCam(int)", 0x8002E74C); }
void EIWallPart::RegisterType(unsigned short) { SIMMY_STUB("EIWallPart::RegisterType(unsigned short)", 0x800305A8); }
void EIWallPart::CreateCopy() const { SIMMY_STUB("EIWallPart::CreateCopy(void) const", 0x800305FC); }
RET_UNKNOWN EIWallPart::IsDiagonal() { SIMMY_STUB("EIWallPart::IsDiagonal(void)", 0x80030624); return {}; }
bool EIWallPart::isDiagonal(TileWallsSegment) { SIMMY_STUB("EIWallPart::isDiagonal(TileWallsSegment)", 0x8003063C); return {}; }
void EIFenceWall::RegisterType(unsigned short) { SIMMY_STUB("EIFenceWall::RegisterType(unsigned short)", 0x80030760); }
void EIFenceWall::CreateCopy() const { SIMMY_STUB("EIFenceWall::CreateCopy(void) const", 0x800307B4); }
double ERoomWall::GetShortDistToCam(int) { SIMMY_STUB("ERoomWall::GetShortDistToCam(int)", 0x80030954); return {}; }
