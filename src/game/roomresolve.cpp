// Port of the objects' room ids: game/object.cpp ResolveRoomID and the room part of
// cXObjectImpl::SetLocation, game/objectmodule.cpp ObjectModuleImpl::UpdateRooms.
//
// An object's room (object data 0x1D) is what the renderer (ISimsObjectModel's rooms: the level's room
// lists, inside shadows and lights), the audio (the room's size) and the trees read. On GC every move
// went through cXObjectImpl::SetLocation (vtable 0x334), which resolved it; the port moves objects and
// people in several places (objectplace.cpp, the animator), which call UpdateObjectRoomID.
//
// Still stubbed: GlobalDispatch 0xEF (a light-giving object changed rooms: the rooms' lighting), the
// objects' entry point 7 tree after a room recompute (cXObjectImpl::RunTree is not offered by the
// port's TreeSim), cXPersonImpl::UpdateCurrentRoom (the rooms' overhead lights).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/roomresolve.hpp"

#include <cstdio>

// 800DF228
// The tile's room; a tile split by a diagonal wall has none of its own (0xFFFB): the point's side of
// the diagonal decides which neighbours give it (wall 0x10, "/": below it x-1 and y-1, else x+1 and
// y+1; wall 0x20, "\": x-1 and y+1 when the point is above it, else x+1 and y-1), the first neighbour
// on the lot with a room, else the second's.
short ResolveRoomID(const int pt[2], int level) {
  cFixedWorldImpl* world = Globs_pFixedWorld;
  if (!world) return static_cast<short>(0xfffb);
  // CTilePt(FTilePt&, int) 800B45FC: x from the second word, y from the first.
  CTilePt tile(pt[1] >> 4, pt[0] >> 4, level);
  if (world->OutOfBounds(tile)) return static_cast<short>(0xfffb);
  unsigned short room = world->GetRoom(tile);  // vtable 0xEC
  if (room != 0xfffb) return static_cast<short>(room);
  TileWalls walls = world->GetWall(tile);  // vtable 0xAC
  CTilePt a(tile), b(tile);
  const int fx = pt[1] & 0xf, fy = pt[0] & 0xf;
  if (walls.HasWall(0x10)) {
    if (fx < 0x10 - fy) {
      a.m_x = static_cast<signed char>(a.m_x - 1);
      b.m_y = static_cast<signed char>(b.m_y - 1);
    } else {
      a.m_x = static_cast<signed char>(a.m_x + 1);
      b.m_y = static_cast<signed char>(b.m_y + 1);
    }
  } else if (walls.HasWall(0x20)) {
    if (fx < fy) {
      a.m_x = static_cast<signed char>(a.m_x - 1);
      b.m_y = static_cast<signed char>(b.m_y + 1);
    } else {
      a.m_x = static_cast<signed char>(a.m_x + 1);
      b.m_y = static_cast<signed char>(b.m_y - 1);
    }
  }
  if (!world->OutOfBounds(a) && (room = world->GetRoom(a)) == 0xfffb && !world->OutOfBounds(b))
    room = world->GetRoom(b);
  return static_cast<short>(room);
}

// 800E2ACC
void cXObjectImpl::SetLevel(int level) { m_level = level; }

namespace {
// 800DF784 (part): the object's room id from its location; when it changed and the object has data 7
// (it lights its room), both rooms are told.
void WriteRoomID(cXObjectImpl* o, const int pt[2], int level) {
  const short room = ResolveRoomID(pt, level);
  const short old = o->m_data[0x1d];
  o->m_data.WriteVar(0x1d, room);
  if (old != room && o->m_data[7] != 0)
    SIMMY_STUB("cXObjectImpl::SetLocation: GlobalDispatch(0xEF, old room / new room) (the rooms' lights)",
               0x800DF784);
}
}  // namespace

// 800DF784 (the room part of cXObjectImpl::SetLocation, after ComputeRect)
// The object's room, then those of the objects in its slots (GC: SetLocation on each contained object
// with the same point; PC: the objects whose container is this one, data 2).
void UpdateObjectRoomID(cXObjectImpl* object) {
  if (!object) return;
  const int pt[2] = {object->m_location[0], object->m_location[1]};
  WriteRoomID(object, pt, object->m_level);
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module || object->m_id == 0) return;
  // The slot hierarchy, a few levels deep (a tray on a counter holding a plate).
  cXObjectImpl* parents[8] = {object};
  int count = 1;
  for (int i = 0; i < count; ++i)
    for (cXObjectImpl* o = module->m_first; o; o = o->m_nextObject)
      if (o != object && o->m_data[2] != 0 && o->m_data[2] == parents[i]->m_id) {
        WriteRoomID(o, pt, object->m_level);
        if (count < 8) parents[count++] = o;
      }
}

// 800F5B10
// After the world's rooms were recomputed (cFixedWorldImpl::ComputeRooms -> GlobalDispatch 0xEC ->
// HouseImpl::DoCommand): every object on `level` (0: all levels) gets its room again, a level 0 object
// is put on level 1, a person updates its current room; then each such object's entry point 7 tree.
void ObjectModuleImpl::UpdateRooms(int level) {
  // PC: the objects by id (the people the port makes are not in the module's object list).
  for (cXObject* x : m_byId) {
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    if (!o) continue;
    if (o->GetLevel() == 0) {
      o->SetLevel(1);  // vtable 0x1DC
      std::printf("Forcing level 0 object to level 1\n");  // ctgDump
    }
    if (level != 0 && o->GetLevel() != level) continue;
    const int pt[2] = {o->m_location[0], o->m_location[1]};
    o->m_data.WriteVar(0x1d, ResolveRoomID(pt, o->GetLevel()));
    if (o->GetType() == 2)
      SIMMY_STUB("ObjectModuleImpl::UpdateRooms: cXPersonImpl::UpdateCurrentRoom (TreeSim vtable 0x1AC)", 0x800F5B10);
  }
  for (cXObject* x : m_byId) {
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    if (o && (level == 0 || o->GetLevel() == level) && o->GetTreeID(7))
      SIMMY_STUB("ObjectModuleImpl::UpdateRooms: RunTree(7) (TreeSim::RunTree)", 0x800F5B10);
  }
}
