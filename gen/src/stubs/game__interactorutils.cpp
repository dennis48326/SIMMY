// GENERATED stubs for unported functions of game/interactorutils.cpp. Do not edit.
#include "simmy/types_gen.hpp"

void InteractorModule::InitializeInteractorModule() { SIMMY_STUB("InteractorModule::InitializeInteractorModule(void)", 0x80210260); }
void InteractorModule::ShutdownInteractorModule() { SIMMY_STUB("InteractorModule::ShutdownInteractorModule(void)", 0x80210464); }
void InteractorModule::GetObjectPosition(EVec3 *, cXObject *) { SIMMY_STUB("InteractorModule::GetObjectPosition(EVec3 *, cXObject *)", 0x80210634); }
void InteractorModule::GetCameraRelativeStickAngle(EVec2 &, ESimsCam *, float *) { SIMMY_STUB("InteractorModule::GetCameraRelativeStickAngle(EVec2 &, ESimsCam *, float *)", 0x80210804); }
RET_UNKNOWN InteractorModule::IsPosOutsideWorld(EVec3 &) { SIMMY_STUB("InteractorModule::IsPosOutsideWorld(EVec3 &)", 0x8021109C); return {}; }
void InteractorModule::GetSnapPos(int &, int &, EVec3 &) { SIMMY_STUB("InteractorModule::GetSnapPos(int &, int &, EVec3 &)", 0x80211194); }
RET_UNKNOWN InteractorModule::GetSnapPosWithOffset(EVec3 &) { SIMMY_STUB("InteractorModule::GetSnapPosWithOffset(EVec3 &)", 0x8021128C); return {}; }
RET_UNKNOWN InteractorModule::CreateObjectFromSelector(ObjSelector *) { SIMMY_STUB("InteractorModule::CreateObjectFromSelector(ObjSelector *)", 0x8021133C); return {}; }
void InteractorModule::GetCursorIntersectionObjects(vector<short, allocator<short>> &, int) { SIMMY_STUB("InteractorModule::GetCursorIntersectionObjects(vector<short, allocator<short> > &, int)", 0x80211660); }
RET_UNKNOWN InteractorModule::GetObjectFromInt16(short) { SIMMY_STUB("InteractorModule::GetObjectFromInt16(short)", 0x80211868); return {}; }
void InteractorModule::GetObjectInstancesList(cXObject *, TNodeList<ISimInstance *> &) { SIMMY_STUB("InteractorModule::GetObjectInstancesList(cXObject *, TNodeList<ISimInstance *> &)", 0x80212650); }
RET_UNKNOWN InteractorModule::GetAdjacentTileCoordinates(TilePtDir, int, EVec3 &) { SIMMY_STUB("InteractorModule::GetAdjacentTileCoordinates(TilePtDir, int, EVec3 &)", 0x80212FE8); return {}; }
RET_UNKNOWN InteractorModule::GetRoomIdFromPoint(CTilePt &) { SIMMY_STUB("InteractorModule::GetRoomIdFromPoint(CTilePt &)", 0x802131F4); return {}; }
RET_UNKNOWN InteractorModule::GetInteractorResourceSet(InteractorModule__Interactor *) { SIMMY_STUB("InteractorModule::GetInteractorResourceSet(InteractorModule::Interactor *)", 0x802136CC); return {}; }
RET_UNKNOWN InteractorModule::IsCameraDirectorInControl(int) { SIMMY_STUB("InteractorModule::IsCameraDirectorInControl(int)", 0x80213700); return {}; }
RET_UNKNOWN InteractorModule::ClearCameraFilter(int) { SIMMY_STUB("InteractorModule::ClearCameraFilter(int)", 0x802137C0); return {}; }
void InteractorModule::IncrementSellCountForObjectGuid(int, vector<InteractorModule__InvStock, allocator<InteractorModule__InvStock>> &) { SIMMY_STUB("InteractorModule::IncrementSellCountForObjectGuid(int, vector<InteractorModule::InvStock, allocator<InteractorModule::InvStock> > &)", 0x80213A44); }
