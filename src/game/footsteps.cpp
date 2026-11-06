// Port of the footstep sounds of game/sanimator2.cpp: an animation's footstep note ("1", "2" a step,
// "walk", "run", "jump", "scuf") plays "footstep_<surface>_<shoe>_<step>" for the sim, the surface from an
// object on the tile or the floor under its feet, the shoe from its costume and shoe model.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

#include <cstdio>
#include <cstring>

namespace {
// 80423E10 soundTypeTable
const char* const kSoundTypes[10] = {"grass", "trash", "dirt", "sand", "puddle", "wood", "tile", "carpet", "metal", "concrete"};

// The floor pattern's sound kind ("FloorSet" row +8; GC: cFixedWorld vtable 0x8C GetFloor, DAT_80475EA0).
int FloorSoundKind(CTilePt& tile) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  if (!w || !w->m_floors || !_globals.m_floorSet) return -1;
  if (tile.GetX() < 0 || tile.GetY() < 0 || tile.GetX() >= w->m_xSize || tile.GetY() >= w->m_ySize) return -1;
  const unsigned int pattern = *w->m_floors->At(tile.GetX(), tile.GetY());
  const auto* rows = static_cast<const unsigned char*>(ERQuickdata::Ptr(_globals.m_floorSet, 0));
  const unsigned int count = rows ? ERQuickdata::U32(rows - 4) : 0;
  if (pattern >= count) return -1;
  return static_cast<int>(ERQuickdata::U32(ERQuickdata::Ptr(rows, pattern * 4), 8));
}
}  // namespace

// 800619BC
// The shoe: "bare" in the costumes 1, 2 and 5, "heel" for four shoe models (the sim's shoes part, SimModel
// part 16), else "shoe".
void SAnimator2::playFootprint(char* step) {
  char buf[200];
  const unsigned int surface = static_cast<unsigned int>(getFootSound(buf));
  if (10 <= surface) return;
  if (std::strcmp(step, "1") == 0 || std::strcmp(step, "2") == 0) step = const_cast<char*>("step");
  const char* shoe;
  if (static_cast<unsigned int>(m_lastCostume - 1) < 2u || m_lastCostume == 5) {
    shoe = "bare";
  } else {
    shoe = "shoe";
    if (m_person && m_sim && m_sim->m_simModel) {
      const EIStaticModel* shoes = m_sim->m_simModel->m_parts[16];
      const unsigned int id = shoes ? shoes->m_modelId : 0u;
      if (id == 0x85247A5Au || id == 0xE7C08848u || id == 0x27CDDD8Fu || id == 0xC963D71Bu) shoe = "heel";
    }
  }
  char name[256];
  std::snprintf(name, sizeof name, "footstep_%s_%s_%s", kSoundTypes[surface], shoe, step);
  cXObject* self = m_person ? m_person->AsObject() : nullptr;
  if (Globs_pSound && self) Globs_pSound->PlayBySource(name, static_cast<short>(self->GetID()));
}

// 80061B3C
// The surface under the sim (-1 off the world).
RET_UNKNOWN SAnimator2::getFootSound(char* out) {
  if (m_pos[0] < 0.0f || m_pos[1] < 0.0f) return -1;
  cXObject* self = m_person ? m_person->AsObject() : nullptr;
  cXObjectImpl* impl = self ? self->GetObjectImplementation() : nullptr;
  CTilePt tile(static_cast<int>(m_pos[0] + 0.5f) >> 4, static_cast<int>(m_pos[1] + 0.5f) >> 4, impl ? impl->m_level : 1);
  return GetFootSound(tile, out);
}

// 80066470
void SAnimator2::footstepEvent(char* step) { playFootprint(step); }

// 80068680
// The surface of `tile`: an object there whose GUID (or its master's) has a sound of its own, the most
// important first (1 trash, 2 puddle, 3 rugs: carpet, 4 wood, 5 metal, 6 dirt); else the floor pattern's
// kind: 0 wood, 1 tile, 2 carpet, 3/4 grass, 5 trash, 6 dirt, 7 sand, 8 puddle, 9 wood, 0xA tile,
// 0xB carpet, 0xC metal, 0xD concrete.
RET_UNKNOWN SAnimator2::GetFootSound(CTilePt& tile, char*) {
  ObjectIterator it;
  it.init(tile, ObjectIterator__IterateType{ObjectIterator__IterateType::kAll});
  unsigned int rank = 9999;
  int surface = -1;
  for (; it.m_current; ++it) {
    cXObject* obj = it.m_current;
    if (obj->GetContainer()) continue;
    ObjSelector* sel = obj->GetSelector();
    if (!sel) continue;
    switch (sel->GetGUID()) {
      case 0x7F907075u: if (1 < rank) { surface = 1; rank = 1; } break;
      case 0x3E7470F6u: if (2 < rank) { surface = 4; rank = 2; } break;
      case 0x51696045u: if (4 < rank) { surface = 5; rank = 4; } break;
      case 0x51424EBDu: if (5 < rank) { surface = 8; rank = 5; } break;
      case 0x63416BA1u: if (6 < rank) { surface = 2; rank = 6; } break;
      default: break;
    }
    auto* master = reinterpret_cast<ObjSelector*>(sel->GetMasterSelector());
    if (!master) continue;
    switch (master->GetGUID()) {
      case 0x301E9EB1u: case 0xF9F00064u: case 0xD5A2B52Bu: case 0x0522C6DCu: case 0x263292ABu:
      case 0x5690F385u: case 0x514A105Cu: case 0x549FBAACu: case 0x594A30DDu: case 0x79FA7E6Bu:
        if (3 < rank) { surface = 7; rank = 3; }
        break;
      default: break;
    }
  }
  if (surface != -1) {
    simmy::SoundTrace("footstep on an object: surface %s", kSoundTypes[surface]);
    return surface;
  }
  switch (FloorSoundKind(tile)) {
    case 0: return 5;
    case 1: return 6;
    case 2: return 7;
    case 3: case 4: return 0;
    case 5: return 1;
    case 6: return 2;
    case 7: return 3;
    case 8: return 4;
    case 9: return 5;
    case 0xA: return 6;
    case 0xB: return 7;
    case 0xC: return 8;
    case 0xD: return 9;
    default: return -1;
  }
}

// 80069220
int SAnimator2::GetLastCostume() { return m_lastCostume; }
// 80069228
void SAnimator2::SetLastCostume(int costume) { m_lastCostume = costume; }
