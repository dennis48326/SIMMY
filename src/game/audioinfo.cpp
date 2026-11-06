// Port of game/audioinfo.cpp: cAudioInfo, what the sound director asks the simulation (object positions
// and the track VM's "source data": a person's voice, age, motives, the room's size, the hour).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
cXObject* Object(int id) { return Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(id) : nullptr; }

// GC cXPersonImpl::IsChild (8012B42C) / IsMale (8012B4AC): from the sim's description; PC: a person without
// one (a service NPC: GetServiceNPC's record, not ported) counts as an adult male.
bool IsChild(cXPerson* p) {
  (void)p;
  return false;
}
bool IsMale(cXPerson* p) {
  if (CasSimDescriptionS2C* d = p->GetSimDescription()) return d->m_male != 0;
  return true;
}
}  // namespace

// 800AAC34
int cAudioInfo::CurrentSimSpeed() { return Globs_pSimulator ? Globs_pSimulator->GetSpeed() : 0; }

// 800AAC6C
// The object's tile position (GetLocation: 1/16 tiles).
bool cAudioInfo::GetObjectPosition(int id, cAudioWorldCoord& out) {
  cXObject* object = Object(id);
  if (!object) return false;
  cXObjectImpl* impl = object->GetObjectImplementation();
  if (!impl) return false;
  out.level = 1;
  out.x = static_cast<float>(impl->m_location[0]) * 0.0625f;
  out.y = static_cast<float>(impl->m_location[1]) * 0.0625f;
  return true;
}

// 800AAD5C
// Fields (cAudioInfo::DataIdx): 0 the voice (0 female, 1 male, 2 child), 1 adult, 4 the hour, 0xD the
// room's area (100 without a room), 0xE the energy motive / 10; the others a person datum / 10
// (2: 10, 3: 9, 5: 0xF, 6: 0xB, 7: 0xC, 8: 0xD, 9: 0xE, 0xA: 0x10, 0xB: 0x11, 0xC: 0x12, 0xF: 2, 0x10: 3,
// 0x11: 4, 0x12: 5, 0x13: 6, 0x14: 7). 0 for objects that are not people.
int cAudioInfo::GetObjectData(int id, int field) {
  cXObject* object = Object(id);
  cXPerson* person = object ? dynamic_cast<cXPerson*>(object) : nullptr;
  static const signed char kPersonData[0x15] = {-1, -1, 10, 9, -1, 0xF, 0xB, 0xC, 0xD, 0xE, 0x10,
                                                0x11, 0x12, -1, -1, 2, 3, 4, 5, 6, 7};
  switch (field) {
    case 0:
      if (!person) return 0;
      if (IsChild(person)) return 2;
      return IsMale(person) ? 0 : 1;  // IsFemale
    case 1: return person ? 1 : 0;   // IsAdult (always true on GC)
    case 4: return Globs_pSimulator ? Globs_pSimulator->GetGlobal(cSimulatorImpl::kHour) : 0;
    case 0xD:
      // The object's room (its data 0x1D, cXObjectImpl::GetRoom, vtable 0x30C): the room's area in tiles.
      if (object && Globs_pRoomManager) {
        if (Room* room = Globs_pRoomManager->GetRoom(static_cast<unsigned short>(object->GetData(0x1D))))
          return room->GetArea();
      }
      return 100;
    case 0xE: return person ? static_cast<int>(person->GetMotive(3) / 10.0) : 0;
    default: break;
  }
  if (field < 0 || field >= 0x15 || kPersonData[field] < 0 || !person) return 0;
  return static_cast<short>(person->GetPersonData(kPersonData[field]) / 10);
}

// 800AB120
// The share of the lot's tiles that are outside, in percent: 32 diagonals of 32 tiles from (0, 0), each
// starting one tile further along x or y in turn; tiles off the world do not count.
int cAudioInfo::OutdoorTileRatio() {
  if (!Globs_pFixedWorld) return 0;  // PC (GC: always a world here)
  // (GC: CTilePt::SetX/SetY on the tiles; here the coordinates are kept as ints.)
  int sx = 0, sy = 0;
  int outside = 0, inside = 0;
  for (int i = 0; i < 0x20; ++i) {
    for (int n = 0; n < 0x20; ++n) {
      CTilePt tile(sx + n, sy + n, 1);
      if (!Globs_pFixedWorld->OutOfBounds(tile)) {
        if (Globs_pFixedWorld->IsOutside(tile)) ++outside;
        else ++inside;
      }
    }
    sx += i & 1;
    sy += (i + 1) & 1;
  }
  if (inside + outside == 0) return 0;  // PC: GC divided 0 by 0
  return static_cast<int>(static_cast<float>(outside) / static_cast<float>(inside + outside) * 100.0f);
}
