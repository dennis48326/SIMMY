// Room ids of objects (game/object.cpp ResolveRoomID and the room part of cXObjectImpl::SetLocation;
// game/objectmodule.cpp ObjectModuleImpl::UpdateRooms). See src/game/roomresolve.cpp.
#pragma once

struct cXObjectImpl;

// 800DF228 ResolveRoomID(FTilePt&, int): the room at a point. PC: the FTilePt as the GC's two words,
// pt[0] = y and pt[1] = x in 1/16 tiles (cXObjectImpl +0x68 / +0x6C). 0xFFFB: off the lot / no room.
short ResolveRoomID(const int pt[2], int level);

// 800DF784 cXObjectImpl::SetLocation (its room part): data 0x1D from the object's location and level,
// GlobalDispatch 0xEF for the old and new rooms when it changed (data 7 set), and the same for the
// objects in its slots. PC: called where the port moves objects and people (the GC did it in
// SetLocation, vtable 0x334).
void UpdateObjectRoomID(cXObjectImpl* object);
