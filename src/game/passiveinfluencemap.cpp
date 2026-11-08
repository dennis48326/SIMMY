// Port of game/passiveinfluencemap.cpp: passive influence (see passiveinfluence.hpp), and the
// passive-influence part of the person's awareness check (game/awarenessmanager.cpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/placement.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

PassiveInfluenceMap* PassiveInfluenceMap::s_passiveInfluenceMap = nullptr;      // 804FDCF8
PassiveInfluenceSystem* PassiveInfluenceSystem::s_passiveInfluenceSystem = nullptr;  // 804FDCFC

namespace {
// PC development aid: SIMMY_INFLUENCE_TRACE=1 prints influences added and removed, the targets'
// trees and (=2) the motive changes.
int TraceInfluence() {
  static const char* env = std::getenv("SIMMY_INFLUENCE_TRACE");
  return env ? std::atoi(env) : 0;
}

cXObjectImpl* Impl(cXObject* o) { return o ? o->GetObjectImplementation() : nullptr; }
const char* Name(cXObject* o) {
  cXObjectImpl* impl = Impl(o);
  return impl ? impl->DebugName() : "?";
}

// Tile coordinates as GC divides (truncating toward zero): FTilePt +4 (m_location[1]) is x.
int TileX(cXObjectImpl* o) { return o->m_location[1] / 16; }
int TileY(cXObjectImpl* o) { return o->m_location[0] / 16; }
}  // namespace

// ---------------------------------------------------------------------------------------------------
// PassiveInfluenceMap

// 801133E8
PassiveInfluenceMap::PassiveInfluenceMap(int xSize, int ySize) : m_xSize(xSize), m_ySize(ySize) {
  m_tiles.resize(static_cast<size_t>(xSize * ySize));
  s_passiveInfluenceMap = this;
}

// 80113470
PassiveInfluenceMap::~PassiveInfluenceMap() {
  EmptyMap();
  m_tiles.clear();
  s_passiveInfluenceMap = nullptr;
}

// 80113520
// The item's tiles worked out again and the item added to each tile's list.
void PassiveInfluenceMap::AddItemToMap(PassiveInfluenceItem* item) {
  if (!item->GetArea().empty()) RemoveItemFromMap(item);
  CalculateAffectedArea(item);
  for (const std::pair<int, int>& t : item->GetArea()) m_tiles[static_cast<size_t>(t.first * m_ySize + t.second)].push_back(item);
}

// 80113740
// PC: tiles outside the map are skipped (an area from an earlier map of another size).
void PassiveInfluenceMap::RemoveItemFromMap(PassiveInfluenceItem* item) {
  for (const std::pair<int, int>& t : item->GetArea()) {
    std::vector<PassiveInfluenceItem*>* tile = GetListForTile(t.first, t.second);
    if (!tile) continue;
    std::vector<PassiveInfluenceItem*>& list = *tile;
    auto it = std::find(list.begin(), list.end(), item);
    if (it != list.end()) list.erase(it);
  }
  std::vector<std::pair<int, int>>().swap(item->GetArea());
}

// 801138F8
std::vector<PassiveInfluenceItem*>* PassiveInfluenceMap::GetListForTile(int x, int y) {
  if (x < 0 || y < 0 || x >= m_xSize || y >= m_ySize) return nullptr;
  return &m_tiles[static_cast<size_t>(x * m_ySize + y)];
}

// 80113944
void PassiveInfluenceMap::EmptyMap() {
  for (std::vector<PassiveInfluenceItem*>& list : m_tiles) std::vector<PassiveInfluenceItem*>().swap(list);
}

// 80113A58
PassiveInfluenceItem* PassiveInfluenceMap::NotifySimulator(cXPerson* person, int x, int y) {
  PassiveInfluenceSystem* system = PassiveInfluenceSystem::s_passiveInfluenceSystem;
  return system ? system->NotifySimulator(person, x, y) : nullptr;
}

// 80113A8C
// An object influence of the target's type on one of its tiles that has not run the target's tree
// lately: the target's tree for it (ApplyInfluence's answer). 1 when there is none.
int PassiveInfluenceMap::CheckObjectInfluence(PassiveInfluenceTarget* target) {
  for (const std::pair<int, int>& t : target->GetArea()) {
    std::vector<PassiveInfluenceItem*>& list = m_tiles[static_cast<size_t>(t.first * m_ySize + t.second)];
    for (PassiveInfluenceItem* item : list) {
      if (item->GetType() != 1) continue;
      auto* influence = static_cast<PassiveInfluenceObject*>(item);
      if (influence->m_targetType != target->m_type) continue;
      bool applied = false;
      for (const PassiveInfluenceTarget::Applied& a : target->m_applied)
        if (a.m_object == influence->GetObject()) {
          applied = true;
          break;
        }
      if (!applied) return ApplyInfluence(influence, target);
    }
  }
  return 1;
}

// 80113C68
// The target's tree entry 0x0D with the influencing object as the stack object; when it says true
// the influence is remembered for the target's temp 0 ticks (none: 30). 1: it said true.
int PassiveInfluenceMap::ApplyInfluence(PassiveInfluenceObject* influence, PassiveInfluenceTarget* target) {
  const short id = static_cast<short>(influence->GetObject()->GetID());
  cXObjectImpl* object = Impl(target->GetObject());
  const int r = object->RunTree(0xd, id, nullptr);
  if (TraceInfluence())
    std::printf("[influence] %s on target %s: tree 0x0D -> %d\n", Name(influence->GetObject()), object->DebugName(), r);
  if (r != 1) return 0;
  int ticks = object->m_temps[0];
  if (ticks == 0) ticks = 0x1e;
  target->m_applied.push_back({influence->GetObject(), ticks});
  return 1;
}

// 80113DA8
int PassiveInfluenceMap::ValidateMissing(PassiveInfluenceItem*) { return 1; }

// 801151C0
// A target covers its own tile.
void PassiveInfluenceMap::CalculateTargetArea(PassiveInfluenceItem* item) {
  if (item->GetType() != 2) return;
  cXObjectImpl* o = Impl(item->GetObject());
  item->GetArea().push_back({TileX(o), TileY(o)});
}

// 80115428
// Tiles x0..x1 of row y (clipped to the map) that are in the object's room.
void PassiveInfluenceMap::FilledArc_Line(int x0, int x1, int y, PassiveInfluenceItem* item) {
  cFixedWorldImpl* world = Globs_pFixedWorld;
  cXObjectImpl* o = Impl(item->GetObject());
  CTilePt tile;
  if (y < 0 || y >= m_ySize || x1 < 0 || x0 >= m_xSize) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= m_xSize) x1 = m_xSize - 1;
  for (; x0 <= x1; ++x0) {
    tile.Set(x0, y);
    if (static_cast<unsigned short>(o->GetData(0x1d)) == world->GetRoom(tile)) item->GetArea().push_back({x0, y});
  }
}

// 80115700
void PassiveInfluenceMap::FilledArc_Polygon(int sides, int radius, int angle, int cx, int cy, int* count, float* verts) {
  int n = 0, ring = 0;
  if (angle > 359) angle %= 360;
  int a = angle + sides * 30;
  if (sides < 3) {
    // A wedge: the centre first.
    n = 1;
    verts[0] = static_cast<float>(cx) + 0.5f;
    verts[1] = static_cast<float>(cy) + 0.5f;
    if (sides == 2) ring = 3;
    else if (sides == 1) ring = 2;
  } else if (sides == 3) {
    ring = 4;
  } else if (sides == 6) {
    ring = 6;
  }
  if (ring != 0) {
    float* v = verts + n * 2;
    const float r = static_cast<float>(radius);
    for (; ring != 0; --ring) {
      ++n;
      const float rad = static_cast<float>(static_cast<float>(static_cast<double>(a) * static_cast<double>(3.14159f)) / 180.0);
      a -= 60;
      v[0] = static_cast<float>(static_cast<float>(r * std::cos(rad) + static_cast<float>(cx)) + 0.5);
      v[1] = static_cast<float>(static_cast<float>(r * std::sin(rad) + static_cast<float>(cy)) + 0.5);
      v += 2;
    }
  }
  *count = n;
}

// 80115928
// The polygon filled a row at a time from its top vertex, walking its edges both ways round.
void PassiveInfluenceMap::FilledArc(int sides, int radius, int angle, int cx, int cy, PassiveInfluenceItem* item) {
  float v[16] = {};
  int n = 0;
  FilledArc_Polygon(sides, radius, angle, cx, cy, &n, v);
  if (radius < 1) {
    FilledArc_Line(cx, cx, cy, item);
    return;
  }
  int top = 0;
  for (int i = 1; i < n; ++i)
    if (v[i * 2 + 1] < v[top * 2 + 1]) top = i;
  int y = static_cast<int>(v[top * 2 + 1]);
  float lx = 0.0f, rx = 0.0f, ldx = 0.0f, rdx = 0.0f;
  int lFrom = top, lTo = top;   // the left edge (indices going up)
  int rFrom = top, rTo = top;   // the right edge (indices going down)
  for (;;) {
    if (y == static_cast<int>(v[lTo * 2 + 1])) {
      do {
        lFrom = lTo;
        if (++lTo == n) lTo = 0;
        if (lTo == rFrom) {
          FilledArc_Line(static_cast<int>(lx), static_cast<int>(rx), y, item);
          return;
        }
      } while (y == static_cast<int>(v[lTo * 2 + 1]));
      lx = v[lFrom * 2];
      ldx = (v[lTo * 2] - lx) / (v[lTo * 2 + 1] - v[lFrom * 2 + 1]);
    }
    if (y == static_cast<int>(v[rTo * 2 + 1])) {
      do {
        rFrom = rTo;
        if (--rTo < 0) rTo = n - 1;
        if (rTo == lFrom) {
          FilledArc_Line(static_cast<int>(lx), static_cast<int>(rx), y, item);
          return;
        }
      } while (y == static_cast<int>(v[rTo * 2 + 1]));
      rx = v[rFrom * 2];
      rdx = (v[rTo * 2] - rx) / (v[rTo * 2 + 1] - v[rFrom * 2 + 1]);
    }
    const int x0 = static_cast<int>(lx), x1 = static_cast<int>(rx);
    lx = lx + ldx;
    rx = rx + rdx;
    FilledArc_Line(x0, x1, y, item);
    ++y;
  }
}

// 80115BD8
// The influence's tiles from its shape, range and its object's direction (data 1), in its room.
void PassiveInfluenceMap::CalculateAffectedArea(PassiveInfluenceItem* item) {
  static const int kAngles[8] = {270, 315, 0, 45, 90, 135, 180, 225};  // 803DC410, by direction
  cFixedWorldImpl* world = Globs_pFixedWorld;
  item->GetArea().clear();
  cXObjectImpl* o = Impl(item->GetObject());
  const int cx = TileX(o), cy = TileY(o);
  if (cx < 0 || cx >= m_xSize || cy < 0 || cy >= m_ySize) return;
  int shape = 5, range = 0, angle = 0;
  if (item->GetType() == 0) {
    auto* p = static_cast<PassiveInfluencePerson*>(item);
    angle = kAngles[o->GetData(1) & 7];
    range = p->m_range;
    shape = static_cast<unsigned char>(p->m_shape);
  } else if (item->GetType() == 1) {
    auto* p = static_cast<PassiveInfluenceObject*>(item);
    angle = kAngles[o->GetData(1) & 7];
    shape = static_cast<unsigned char>(p->m_shape);
    range = p->m_range;
  }
  switch (shape) {
    case 0: FilledArc(1, range, angle, cx, cy, item); return;
    case 1: FilledArc(2, range, angle, cx, cy, item); return;
    case 2: FilledArc(3, range, angle, cx, cy, item); return;
    case 3: FilledArc(6, range, 0, cx, cy, item); return;
    case 4: break;
    default: return;
  }
  // A square of side range + 1 (rounded to odd) in front of it.
  int fx = 0, fy = 0;
  switch (o->GetData(1) & 7) {
    case 0: fy = -1; break;
    case 1: fx = 1, fy = -1; break;
    case 2: fx = 1; break;
    case 3: fx = 1, fy = 1; break;
    case 4: fy = 1; break;
    case 5: fx = -1, fy = 1; break;
    case 6: fx = -1; break;
    case 7: fx = -1, fy = -1; break;
  }
  const int half = (range + 1) / 2;
  const int mx = cx + fx * half, my = cy + fy * half;
  CTilePt tile;
  for (int x = mx - half; x <= mx + half; ++x)
    for (int y = my - half; y <= my + half; ++y) {
      if (x < 0 || x >= m_xSize || y < 0 || y >= m_ySize) continue;
      tile.Set(x, y);
      if (static_cast<unsigned short>(o->GetData(0x1d)) == world->GetRoom(tile)) item->GetArea().push_back({x, y});
    }
}

// 8011623C
// GC: checks every tile list lives in the resource heap (a debug aid).
void PassiveInfluenceMap::Validate() {}

// ---------------------------------------------------------------------------------------------------
// PassiveInfluenceSystem

// 80113DB0
// GC: the first system made stays the one the map reports to.
PassiveInfluenceSystem::PassiveInfluenceSystem() {
  if (!s_passiveInfluenceSystem) s_passiveInfluenceSystem = this;
}

// 80113E48
// GC: the items are not deleted (their lists' nodes are); PC: they are.
PassiveInfluenceSystem::~PassiveInfluenceSystem() {
  if (s_passiveInfluenceSystem == this) {
    s_passiveInfluenceSystem = nullptr;
    if (PassiveInfluenceMap::s_passiveInfluenceMap) PassiveInfluenceMap::s_passiveInfluenceMap->EmptyMap();
  }
  for (PassiveInfluenceTarget* t : m_targets) delete t;
  for (PassiveInfluenceObject* o : m_objects) delete o;
  for (PassiveInfluencePerson* p : m_persons) delete p;
}

// 80113F04
void PassiveInfluenceSystem::FlushSystem() {
  if (s_passiveInfluenceSystem == this && PassiveInfluenceMap::s_passiveInfluenceMap)
    PassiveInfluenceMap::s_passiveInfluenceMap->EmptyMap();
}

void PassiveInfluenceSystem::ForgetObject(cXObject* object) {
  for (auto it = m_targets.begin(); it != m_targets.end();) {
    PassiveInfluenceTarget* t = *it;
    if (t->GetObject() == object) {
      it = m_targets.erase(it);
      delete t;
      continue;
    }
    t->m_applied.remove_if([object](const PassiveInfluenceTarget::Applied& a) { return a.m_object == object; });
    ++it;
  }
}

// 80113F3C
// Nothing is saved: the objects' load trees put their influences back.
void PassiveInfluenceSystem::DoStream(ReconBuffer*, int) {}

// 80113F40
// The object's influence (flags bit 4: on objects, else on people), unless it has one already, the
// range is below 1 or the shape is 5 (remove). 1: added.
int PassiveInfluenceSystem::AddObjectToMap(cXObject* object, signed char skillKind, signed char shape, signed char range,
                                           signed char flags) {
  if (!object || range < 1 || shape == 5) return 0;
  PassiveInfluenceItem* item = nullptr;
  if ((flags & 4) == 0) {
    for (PassiveInfluencePerson* p : m_persons)
      if (p->GetObject() == object) return 0;
    auto* p = new PassiveInfluencePerson;
    p->m_object = object;
    p->m_skillKind = skillKind;
    p->m_shape = shape;
    p->m_range = range;
    p->m_flags = static_cast<unsigned char>(flags);
    m_persons.push_back(p);
    item = p;
  } else {
    for (PassiveInfluenceObject* o : m_objects)
      if (o->GetObject() == object) return 0;
    auto* o = new PassiveInfluenceObject;
    o->m_object = object;
    o->m_targetType = skillKind;
    o->m_shape = shape;
    o->m_range = range;
    o->m_flags = static_cast<unsigned char>(flags);
    m_objects.push_back(o);
    item = o;
  }
  if (PassiveInfluenceMap::s_passiveInfluenceMap) PassiveInfluenceMap::s_passiveInfluenceMap->AddItemToMap(item);
  if (TraceInfluence())
    std::printf("[influence] %s added: %s, shape %d, range %d, flags 0x%x, %zu tiles\n", Name(object),
                (flags & 4) ? "on objects" : "on people", shape, range, static_cast<unsigned char>(flags),
                item->GetArea().size());
  return 1;
}

// 8011416C
int PassiveInfluenceSystem::RemoveObjectFromMap(cXObject* object) {
  PassiveInfluenceMap* map = PassiveInfluenceMap::s_passiveInfluenceMap;
  for (auto it = m_persons.begin(); it != m_persons.end(); ++it) {
    if ((*it)->GetObject() != object) continue;
    PassiveInfluencePerson* p = *it;
    if (map) map->RemoveItemFromMap(p);
    m_persons.erase(it);
    delete p;
    if (TraceInfluence()) std::printf("[influence] %s removed (on people)\n", Name(object));
    break;
  }
  for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
    if ((*it)->GetObject() != object) continue;
    PassiveInfluenceObject* o = *it;
    if (map) map->RemoveItemFromMap(o);
    m_objects.erase(it);
    delete o;
    if (TraceInfluence()) std::printf("[influence] %s removed (on objects)\n", Name(object));
    break;
  }
  return 1;
}

// 80114344
// The object moved: its influences' tiles again; it becomes a target (in the world, definition +0x0A
// 0x2B, 0x1B or 0x3E), its target tile moves, or it stops being one (out of the world).
void PassiveInfluenceSystem::MoveObjectInWorld(cXObject* object) {
  PassiveInfluenceMap* map = PassiveInfluenceMap::s_passiveInfluenceMap;
  for (PassiveInfluencePerson* p : m_persons)
    if (p->GetObject() == object) {
      map->RemoveItemFromMap(p);
      map->AddItemToMap(p);
      break;
    }
  for (PassiveInfluenceObject* o : m_objects)
    if (o->GetObject() == object) {
      map->RemoveItemFromMap(o);
      map->AddItemToMap(o);
      break;
    }
  cXObjectImpl* impl = Impl(object);
  auto it = m_targets.begin();
  for (; it != m_targets.end(); ++it)
    if ((*it)->GetObject() == object) break;
  if (it == m_targets.end()) {
    if (impl && ObjIsInWorld(impl) == 1 && impl->m_def) {
      const short type = ERQuickdata::S16(impl->m_def, 0xa);
      if (type == 0x2b || type == 0x1b || type == 0x3e) {
        auto* t = new PassiveInfluenceTarget;
        t->m_type = static_cast<signed char>(type);
        t->m_object = object;
        map->CalculateTargetArea(t);
        m_targets.push_back(t);
      }
    }
  } else if (!impl || !ObjIsInWorld(impl)) {
    PassiveInfluenceTarget* t = *it;
    m_targets.erase(it);
    delete t;
  } else {
    (*it)->GetArea().clear();
    map->CalculateTargetArea(*it);
  }
}

// 80114704
// The person-influences on tile (x, y) not hidden (data 0x22), and (flags 2) not held by a person or
// (flags 1) not held by this one: per motive, the mean of their deltas scaled to the frame and the
// simulator's speed, moved toward but not past their furthest limit. Unless person data 0x1D. The one
// with the strongest delta (for the awareness animation), none when nothing is on the tile.
PassiveInfluenceItem* PassiveInfluenceSystem::NotifySimulator(cXPerson* person, int x, int y) {
  PassiveInfluenceMap* map = PassiveInfluenceMap::s_passiveInfluenceMap;
  std::vector<PassiveInfluenceItem*>* list = map ? map->GetListForTile(x, y) : nullptr;
  if (!list || list->empty()) return nullptr;
  unsigned int count[16] = {};
  float sum[16] = {}, upper[16] = {}, lower[16] = {};
  float strongest = 0.0f;
  PassiveInfluenceItem* best = nullptr;
  cXObject* me = person->AsObject();
  for (PassiveInfluenceItem* item : *list) {
    if (item->GetType() != 0) continue;
    auto* p = static_cast<PassiveInfluencePerson*>(item);
    cXObjectImpl* o = Impl(p->GetObject());
    if (o->GetData(0x22) != 0) continue;
    cXObject* holder = o->GetContainer();
    cXObjectImpl* h = Impl(holder);
    if (h && h->GetType() == 2) {  // TreeSim::GetPerson
      if (p->m_flags & 2) continue;
      if ((p->m_flags & 1) && me && me->GetID() == holder->GetID()) continue;
    }
    for (const PassiveInfluenceMotiveDelta& d : p->m_deltas) {
      const int m = d.m_motive;
      ++count[m];
      sum[m] += d.m_delta;
      if (strongest < std::fabs(d.m_delta)) {
        strongest = std::fabs(d.m_delta);
        best = p;
      }
      if (d.m_delta <= 0.0f) {
        if (d.m_limit < lower[m]) lower[m] = d.m_limit;
      } else if (upper[m] < d.m_limit) {
        upper[m] = d.m_limit;
      }
    }
  }
  if (person->GetPersonData(0x1d) == 0) {
    const double speed = Globs_pSimulator ? Globs_pSimulator->GetSpeedMultiplier() : 0.0;
    for (int m = 0; m < 16; ++m) {
      if (count[m] == 0) continue;
      double value = person->GetMotive(m);
      sum[m] = sum[m] / static_cast<float>(count[m]);
      const float d = sum[m] * static_cast<float>(static_cast<float>(static_cast<double>(_dt) * speed) / 0.0333000011742115);
      sum[m] = d;
      if (d <= 0.0f) {
        const float moved = static_cast<float>(value + d);
        if (lower[m] < moved) value = moved;
      } else {
        const float moved = static_cast<float>(value + d);
        if (moved < upper[m]) value = moved;
      }
      if (TraceInfluence() >= 2)
        std::printf("[influence] person %d motive %d: %.3f -> %.3f (%d influences)\n", me ? me->GetID() : -1, m,
                    person->GetMotive(m), value, count[m]);
      if (auto* impl = dynamic_cast<cXPersonImpl*>(me)) impl->SetMotive(m, static_cast<float>(value));  // vtable 0x74
    }
  }
  return best;
}

// 80114B9C
// Once a tick: each target's remembered influences count down; then the first target an influence
// applies to runs its tree (which may move or remove it, so the walk stops there). A target whose tree
// said no, still where it was, stops being a target.
int PassiveInfluenceSystem::CheckObjects(int ticks) {
  PassiveInfluenceMap* map = PassiveInfluenceMap::s_passiveInfluenceMap;
  if (!map) return 0;
  for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
    PassiveInfluenceTarget* t = *it;
    for (auto a = t->m_applied.begin(); a != t->m_applied.end();) {
      a->m_ticks -= ticks;
      if (a->m_ticks < 1) a = t->m_applied.erase(a);
      else ++a;
    }
    cXObjectImpl* o = Impl(t->GetObject());
    const int was[2] = {o->m_location[0], o->m_location[1]};
    const int r = map->CheckObjectInfluence(t);
    const bool same = o->m_location[0] == was[0] && o->m_location[1] == was[1];
    if (r == 0) {
      if (ObjIsInWorld(o) && same) {
        m_targets.erase(it);
        delete t;
      }
      break;
    }
    if (!ObjIsInWorld(o) || !same) break;
  }
  return 1;
}

// 80114E1C
// A motive delta (a frame) and limit for the object's person-influence. 0: it has none.
int PassiveInfluenceSystem::SetInfluenceMotiveDelta(cXObject* object, unsigned char motive, float delta, short limit) {
  for (PassiveInfluencePerson* p : m_persons)
    if (p->GetObject() == object) {
      p->m_deltas.push_back({motive, delta, static_cast<float>(limit)});
      return 1;
    }
  return 0;
}

// 801150A0
int PassiveInfluenceSystem::ClearInfluenceMotiveDeltas(cXObject* object) {
  for (PassiveInfluencePerson* p : m_persons)
    if (p->GetObject() == object) {
      p->m_deltas.clear();
      break;
    }
  return 0;
}

// 8011513C
bool PassiveInfluenceSystem::SetSkillNameID(cXObject* object, const AnimRef* skill) {
  for (PassiveInfluencePerson* p : m_persons)
    if (p->GetObject() == object) {
      p->m_skill = skill;
      return true;
    }
  return false;
}

// ---------------------------------------------------------------------------------------------------
// The awareness check's passive influence (GC 80012140 AwarenessManager::AwarenessCheck, 80013D4C
// PassiveInfluenceAwarenessCheck)

void PassiveInfluenceAwarenessCheck(SAnimator2* animator) {
  cXPerson* person = animator ? animator->m_person : nullptr;
  cXObject* object = person ? person->AsObject() : nullptr;
  cXObjectImpl* impl = Impl(object);
  // AwarenessCheck: a person in the world with its model, the simulator running.
  if (!impl || !ObjIsInWorld(impl) || !animator->m_sim || !animator->m_sim->m_modelReady) return;
  if (!Globs_pSimulator || Globs_pSimulator->GetSpeedMultiplier() == 0.0) return;
  SIMMY_STUB("AwarenessManager::AwarenessCheck: memory awareness, rain (auto-carry), the awareness angle", 0x80012140);
  // PassiveInfluenceAwarenessCheck.
  PassiveInfluenceMap* map = Globs_pFixedWorld ? Globs_pFixedWorld->GetPassiveInfluenceMap() : nullptr;
  if (!map) return;  // GC: StopPassiveInfluenceAnimation
  PassiveInfluenceItem* best = PassiveInfluenceMap::NotifySimulator(person, impl->m_location[1] >> 4, impl->m_location[0] >> 4);
  if (animator->m_animState == 0x10) return;
  if (best)
    SIMMY_STUB("AwarenessManager::PassiveInfluenceAwarenessCheck: the reaction animation (Start/StopPassiveInfluenceAnimation)",
               0x80013D4C);
}

// GC 800DF784 cXObjectImpl::SetLocation's last step (PC: called by the port's location setters).
void PassiveInfluenceLocationChanged(cXObjectImpl* object, int oldX, int oldY) {
  if (object->m_location[0] == oldX && object->m_location[1] == oldY) return;
  if (PassiveInfluenceSystem* influence = Globs_pSimulator ? Globs_pSimulator->GetPassiveInfluenceSystem() : nullptr)
    influence->MoveObjectInWorld(object);
}
