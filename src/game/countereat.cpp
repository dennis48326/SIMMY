// PC mod: mods. Eating at a counter on a bar stool (the PC game's bar stools; the GameCube has none).
//
// How the GameCube decides where a sim eats (food_carry_sg "Eat", run by the person with the food as
// stack object): holding the plate, "Do - Eating Surface" asks for the best eating surface
// (TryFindFunctionalObject function 7: every object with tree table entry 0x19 whose check tree passes,
// scored by its data 0x26 less 3 per tile of distance). Only tables have one (table_sg "Function -
// Eating Surface", check "... TEST": the part's middle slot free and "Find adjacent good chair": a chair
// on a side tile, in the same room, facing the part, free or holding the eater, with the part's slot
// on that side free; the food's data 4 flag 3). Its tree routes to the part with seat goals (the
// routing finds those chairs; the chair's entry 0x1A "Function - Sit" sits the eater), plays the put-down
// (misc animation 9) and drops the plate into that side's slot; "Eat Sitting Down" then pulls the chair
// in (global "Sit - Scoot - In": its state 1) and eats. No table: "Do - Sit" (any chair, the plate in the
// lap), else "Eat Standing Up". Food already on a table is eaten from the chair on its side (the
// table's named tree ct_chair_id_from_food_id), else picked up and the search runs again.
//
// Here a counter (IsCounterTop) with a counter seat (a mod bar stool, ModObject::counterSeat) on a side
// tile facing it is such a surface too: TryFindFunctionalObject / TryCallFunctionalTree take kEatTest /
// kEatTree for it (score kScore), and TryCallNamedTree kChairFromFood for ct_chair_id_from_food_id. The
// trees are SimAntics bytecode in the GameCube's format, built below, run like the disc's (the person's
// stack, the counter's behaviour for its file's globals); primitive 0x60 (unused on the disc) finds and
// checks the stool. The plate goes into an eating slot of the counter on the stool's side: its slot 0
// (counter height, a mod counter's own top) moved 1/4 tile that way, as a table's side slots are; its
// index (kCounterEatSlot + side) is past the counter's slot list, so the counter's own trees, its slot
// count and placement checks never see it. Nothing here runs while no loaded mod is a counter seat.
#include "simmy/countereat.hpp"

#include "simmy/classes/placement.hpp"
#include "simmy/platform.hpp"
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace simmy::countereat {
namespace {

const int kXDir[8] = {0, 1, 1, 1, 0, -1, -1, -1};
const int kYDir[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

bool Trace() {
  static const bool on = std::getenv("SIMMY_COUNTER_EAT_TRACE") != nullptr;
  return on;
}

bool IsSeatMod(const ModObject& m) { return m.counterSeat == 1 || (m.counterSeat == -1 && m.seatOffset >= 0.08f); }

// A counter seat: a single-tile chair from a mod that is one.
bool IsCounterSeat(cXObjectImpl* o) {
  const ModObject* m = o && o->m_selector ? o->m_selector->m_modObject : nullptr;
  return m && IsSeatMod(*m) && !o->IsMultiTile() && o->IsChair();
}

// The tile's objects (its site chain), as objectplace.cpp's UpdateChairFacing walks it.
cXObjectImpl* SeatAt(const CTilePt& tile) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module) return nullptr;
  CTilePt t = tile;
  cXObject* first = module->GetObjectFromID(module->GetTileObjectID(t));
  int n = 0;
  for (cXObjectImpl* o = first ? first->GetObjectImplementation() : nullptr; o && n < 64; o = HierGetChild(o, 0), ++n)
    if (o->GetLevel() == tile.GetLevel() && IsCounterSeat(o)) return o;
  return nullptr;
}

bool InWorld(cXObjectImpl* o) { return o && o->m_location[0] >= 0 && o->m_location[1] >= 0 && !o->GetContainer(); }

// The counter seat on side `d` (0, 2, 4, 6) of the counter, facing it, in its room.
cXObjectImpl* SeatOnSide(cXObjectImpl* counter, int d) {
  const CTilePt here = counter->GetCTilePt();
  CTilePt n(here.GetX() + kXDir[d], here.GetY() + kYDir[d], here.GetLevel());
  if (Globs_pFixedWorld && Globs_pFixedWorld->OutOfBounds(n)) return nullptr;
  cXObjectImpl* seat = SeatAt(n);
  if (!seat || seat->GetData(1) != ((d + 4) & 7)) return nullptr;
  if (static_cast<unsigned short>(seat->GetData(0x1d)) != static_cast<unsigned short>(counter->GetData(0x1d))) return nullptr;
  return seat;
}

int SlotForSide(cXObjectImpl* counter, int d) { return kCounterEatSlot + (((d - counter->GetData(1)) & 7) >> 1); }
int SideForSlot(cXObjectImpl* counter, int slot) { return (((slot - kCounterEatSlot) << 1) + counter->GetData(1)) & 7; }

// The side's spot is free for `person`: the seat free or theirs, the counter's eating slot on that side
// empty, nothing in its middle slot (an appliance), nor (the front) in its prep slot.
bool SpotFree(cXObjectImpl* counter, int d, cXObjectImpl* seat, cXObjectImpl* person) {
  cXObjectImpl* occupant = seat->GetContainedObject(0);
  if (occupant && occupant != person) return false;
  if (counter->GetContainedObject(SlotForSide(counter, d))) return false;
  if (counter->GetContainedObject(0)) return false;
  if (((d - counter->GetData(1)) & 7) == 0 && counter->GetContainedObject(1)) return false;
  return true;
}

// ---- The PC trees ---------------------------------------------------------------------------------

// Node targets: a node index, or these.
constexpr unsigned char kTrue = 0xfe, kFalse = 0xff, kErr = 0xfd;
// Expression scopes (cXObjectImpl::InterpValue) and operators (TryExpression).
enum : unsigned char { sMy = 3, sSo = 4, sLit = 7, sTemp = 8, sParam = 9, sSoId = 0xa, sMySlot = 0x14, sLocal = 0x19, sConst = 0x1a };
enum : unsigned char { oEq = 2, oSet = 5, oFlag = 8, oNe = 16 };

struct Builder {
  std::vector<unsigned char> b;
  Builder(int nodes, int params, int locals) {
    // ObjectDataBehaviorTree: signature (0x8007: params and locals as given), node count, type 0,
    // params, locals, flags, version.
    b = {0x80, 0x07, static_cast<unsigned char>(nodes >> 8), static_cast<unsigned char>(nodes), 0,
         static_cast<unsigned char>(params), static_cast<unsigned char>(locals), 0, 0, 0, 0, 0};
  }
  void Node(int op, unsigned char t, unsigned char f, std::array<unsigned char, 8> operands) {
    b.push_back(static_cast<unsigned char>(op >> 8));
    b.push_back(static_cast<unsigned char>(op));
    b.push_back(t);
    b.push_back(f);
    b.insert(b.end(), operands.begin(), operands.end());
  }
  // Primitive 2: lhs (scope, data) op rhs (scope, data).
  void Expr(unsigned char ls, int ld, unsigned char op, unsigned char rs, int rd, unsigned char t, unsigned char f = kErr) {
    Node(0x02, t, f,
         {static_cast<unsigned char>(ld >> 8), static_cast<unsigned char>(ld), static_cast<unsigned char>(rd >> 8),
          static_cast<unsigned char>(rd), 0, op, ls, rs});
  }
  void Call(int tree, unsigned char t, unsigned char f, std::array<unsigned char, 8> operands = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}) {
    Node(tree, t, f, operands);
  }
};

// kEatTree (the person's stack, the counter its stack object; as table_sg's "Function - Eating Surface"
// after its route): locals 0 the put-down's event, 1 the counter, 2 its eating slot, 3 the food, 4 the
// stool. Sit on the stool (its entry 0x1A, "Function - Sit": walks there and sits), check the spot
// again, then the table's put-down: misc animation 9 (preloaded first) over the carry, the plate into
// the eating slot at its event, the carry let go, "ct_put" on the food; true if the food is in the
// counter. Standard Entry / Exit mark the counter in use meanwhile (data 8 flag 6), as on a table.
std::vector<unsigned char> BuildEatTree() {
  Builder t(30, 1, 5);
  t.Expr(sLocal, 1, oSet, sSoId, 0, 1);                                   // 0  counter
  t.Node(0x60, 2, kFalse, {0, 0, 0, 0, 0, 0, 0, 0});                      // 1  find a stool: temp 0, slot temp 1
  t.Expr(sLocal, 4, oSet, sTemp, 0, 3);                                   // 2
  t.Expr(sLocal, 2, oSet, sTemp, 1, 4);                                   // 3
  t.Expr(sSoId, 0, oSet, sLocal, 4, 5);                                   // 4  the stool
  t.Node(0x14, 6, kFalse, {0, 8, 0, 0, 0, 0, 0, 0});                      // 5  its "Function - Sit"
  t.Expr(sTemp, 0, oSet, sLocal, 4, 7);                                   // 6
  t.Expr(sTemp, 1, oSet, sLocal, 2, 8);                                   // 7
  t.Expr(sSoId, 0, oSet, sLocal, 1, 9);                                   // 8  the counter
  t.Node(0x60, 10, kFalse, {0, 1, 0, 0, 0, 0, 0, 0});                     // 9  seated there, slot free
  t.Expr(sSo, 8, oFlag, sLit, 6, kFalse, 11);                             // 10 the counter in use
  t.Expr(sParam, 0, oSet, sLocal, 2, 12);                                 // 11 the slot to drop into
  t.Call(0x0110, 13, kErr);                                               // 12 Standard Entry
  t.Expr(sLocal, 3, oSet, sMySlot, 0, 14);                                // 13 the food
  t.Node(0x2c, 16, 15, {0, 9, 2, 0, 3, 0xa0, 1, 0});                      // 14 preload misc 9
  t.Call(0x0118, 14, kErr, {0, 2, 0, 0, 0, 0, 0, 0});                     // 15 idle
  t.Node(0x2c, 24, 17, {0, 9, 0, 0, 3, 0x31, 1, 0});                      // 16 put down (events: local 0)
  t.Expr(sLocal, 0, oEq, sLit, 0, 18, 16);                                // 17
  t.Node(0x2b, 19, 16, {0, 0, 0, 0, 0, 1, 0, 0});                         // 18 drop onto slot param 0
  t.Node(0x2c, 20, 20, {0, 0, 0, 0, 0, 0x10, 0, 0});                      // 19 carry off
  t.Node(0x2c, 21, 21, {0, 0, 0, 2, 0, 0, 0, 0});                         // 20
  t.Expr(sSoId, 0, oSet, sLocal, 3, 22);                                  // 21 the food
  t.Node(0x1c, 23, 23, {0x01, 0x2f, 0, 1, 0x25, 2, 0, 0});                // 22 its "ct_put" (global name 37)
  t.Expr(sSoId, 0, oSet, sLocal, 1, 16);                                  // 23
  t.Node(0x2c, 25, 25, {0, 0, 0, 0, 0, 0x10, 0, 0});                      // 24 animation over: carry off
  t.Node(0x2c, 26, 26, {0, 0, 0, 2, 0, 0, 0, 0});                         // 25
  t.Expr(sSoId, 0, oSet, sLocal, 1, 27);                                  // 26
  t.Call(0x0111, 28, kErr, {0, 0, 0, 0, 0, 0, 0, 0});                     // 27 Standard Exit
  t.Expr(sSoId, 0, oSet, sLocal, 3, 29);                                  // 28
  t.Expr(sSo, 2, oEq, sLocal, 1, kTrue, kFalse);                          // 29 the food is in the counter
  return t.b;
}

// kEatTest (a check tree: the person, the counter its stack object), as the table's: holding
// something, a stool and slot for this person, and the held food's data 4 flag (global constant
// 0x101 #2) as the table's test asks.
std::vector<unsigned char> BuildEatTest() {
  Builder t(4, 0, 0);
  t.Expr(sMySlot, 0, oNe, sLit, 0, 1, kFalse);
  t.Node(0x60, 2, kFalse, {0, 0, 0, 0, 0, 0, 0, 0});
  t.Expr(sSoId, 0, oSet, sMySlot, 0, 3);
  t.Expr(sSo, 4, oFlag, sConst, 0x4082, kTrue, kFalse);
  return t.b;
}

// kChairFromFood (the person's stack, the counter its stack object, the food in temp 0): the stool on
// the side of the food's eating slot, free or the person's, in temp 0.
std::vector<unsigned char> BuildChairFromFood() {
  Builder t(1, 1, 0);
  t.Node(0x60, kTrue, kFalse, {0, 2, 0, 0, 0, 0, 0, 0});
  return t.b;
}

// kPickupTree (the person's stack, the counter its stack object, param 0 what to pick up: the counter's
// "Pickup Function" entry 0x17, called by global "Reach and Grab" for a thing in it) for a thing in an
// eating slot with a stool on that side (the counter's own goes to its front only): as table_sg
// "Function - Pickup" from a chair: sit on the stool, misc animation 9 (preloaded) with the grab at its
// event ("ct_grab" on the thing, then the grab), true if it is in the hand. Locals: 0 the event, 1 the
// counter, 2 the stool.
std::vector<unsigned char> BuildPickupTree() {
  Builder t(24, 1, 3);
  t.Expr(sLocal, 1, oSet, sSoId, 0, 1);                                   // 0  counter
  t.Expr(sMySlot, 0, oEq, sLit, 0, 2, kFalse);                            // 1  hands free
  t.Expr(sTemp, 0, oSet, sParam, 0, 3);                                   // 2  the thing
  t.Node(0x60, 4, kFalse, {0, 2, 0, 0, 0, 0, 0, 0});                      // 3  the stool on its side
  t.Expr(sLocal, 2, oSet, sTemp, 0, 5);                                   // 4
  t.Expr(sSoId, 0, oSet, sLocal, 2, 6);                                   // 5
  t.Node(0x14, 7, kFalse, {0, 8, 0, 0, 0, 0, 0, 0});                      // 6  its "Function - Sit"
  t.Expr(sSoId, 0, oSet, sLocal, 1, 8);                                   // 7
  t.Expr(sSo, 8, oFlag, sLit, 6, kFalse, 9);                              // 8  the counter in use
  t.Expr(sSoId, 0, oSet, sParam, 0, 10);                                  // 9
  t.Expr(sSo, 2, oEq, sLocal, 1, 11, kFalse);                             // 10 still in the counter
  t.Expr(sSoId, 0, oSet, sLocal, 1, 12);                                  // 11
  t.Call(0x0110, 13, kErr);                                               // 12 Standard Entry
  t.Node(0x2c, 15, 14, {0, 9, 1, 0, 3, 0xa0, 1, 0});                      // 13 preload misc 9
  t.Call(0x0118, 13, kErr, {0, 2, 0, 0, 0, 0, 0, 0});                     // 14 idle
  t.Node(0x2c, 21, 16, {0, 9, 0, 0, 3, 0x31, 1, 0});                      // 15 reach (events: local 0)
  t.Expr(sLocal, 0, oEq, sLit, 0, 17, 15);                                // 16
  t.Expr(sSoId, 0, oSet, sParam, 0, 18);                                  // 17 the thing
  t.Node(0x1c, 19, 19, {0x01, 0x2f, 0, 1, 0x35, 2, 0, 0});                // 18 its "ct_grab" (global name 53)
  t.Node(0x04, 20, 15, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}); // 19 grab
  t.Expr(sSoId, 0, oSet, sLocal, 1, 15);                                  // 20
  t.Expr(sSoId, 0, oSet, sLocal, 1, 22);                                  // 21 animation over
  t.Call(0x0111, 23, kErr, {0, 0, 0, 0, 0, 0, 0, 0});                     // 22 Standard Exit
  t.Expr(sMySlot, 0, oEq, sParam, 0, kTrue, kFalse);                      // 23 in the hand
  return t.b;
}

struct Trees {
  std::vector<unsigned char> eat = BuildEatTree(), test = BuildEatTest(), chair = BuildChairFromFood(),
                             pickup = BuildPickupTree();
};
const Trees& AllTrees() {
  static const Trees t;
  return t;
}

}  // namespace

bool Active() {
  static const bool active = [] {
    for (const ModObject& m : mods_objects())
      if (IsSeatMod(m)) return true;
    return false;
  }();
  return active;
}

bool IsCounterTop(cXObjectImpl* o) {
  return o && o->m_def && !o->IsMultiTile() && o->GetTreeID(0x12) != 0 && o->GetTreeID(0x14) != 0;
}

bool IsEatSurface(cXObjectImpl* o) {
  if (!Active() || !IsCounterTop(o) || !InWorld(o)) return false;
  for (int d = 0; d < 8; d += 2)
    if (SeatOnSide(o, d)) return true;
  return false;
}

const unsigned char* Tree(short id) {
  switch (id) {
    case kEatTree: return AllTrees().eat.data();
    case kEatTest: return AllTrees().test.data();
    case kChairFromFood: return AllTrees().chair.data();
    case kPickupTree: return AllTrees().pickup.data();
    default: return nullptr;
  }
}

const char* TreeName(short id) {
  switch (id) {
    case kEatTree: return "PC Counter - Eating Surface";
    case kEatTest: return "PC Counter - Eating Surface TEST";
    case kChairFromFood: return "PC Counter - ct_chair_id_from_food_id";
    case kPickupTree: return "PC Counter - Pickup From Eating Slot";
    default: return nullptr;
  }
}

bool IsEatSlotPickup(cXObjectImpl* counter, cXObjectImpl* me) {
  if (!Active() || !me || !IsCounterTop(counter) || !InWorld(counter)) return false;
  cXObject* x = me->GetObjectFromID(me->m_temps[0]);
  cXObjectImpl* thing = x ? x->GetObjectImplementation() : nullptr;
  if (!thing || thing->GetData(2) != counter->m_id) return false;
  const int slot = thing->GetData(3);
  return slot >= kCounterEatSlot && slot < kCounterEatSlot + 4 && SeatOnSide(counter, SideForSlot(counter, slot));
}

short NamedTree(cXObjectImpl* target, const char* name) {
  if (!name || std::strcmp(name, "ct_chair_id_from_food_id") != 0) return 0;
  return Active() && IsCounterTop(target) ? kChairFromFood : 0;
}

// Primitive 0x60 (PC trees only). Operand +0 (16 bits), with the counter as stack object:
// 0 find: the counter seat for me (the one I sit on, else the nearest free one) with its spot free
//   (SpotFree): temp 0 the seat, temp 1 the counter's eating slot on its side;
// 1 check: I sit on seat temp 0, still on the side of slot temp 1 and facing the counter, and that
//   slot is empty;
// 2 chair from food: food temp 0 in one of the counter's eating slots: the seat on that side, free or
//   mine, into temp 0.
// False otherwise. Errors: no stack object (0x15).
int TryCounterSeat(cXObjectImpl* me, StackElem* frame, const unsigned char* params) {
  cXObject* x = me->GetObjectFromID(frame->m_stackObject);
  cXObjectImpl* counter = x ? x->GetObjectImplementation() : nullptr;
  if (!counter) {
    me->m_treeSim->SetError(0x15);
    me->Error(0x15);
    return -1;
  }
  const int op = static_cast<short>(be16(params));
  if (!IsCounterTop(counter) || !InWorld(counter)) return 0;
  if (op == 0) {
    cXObjectImpl* best = nullptr;
    int bestSide = 0, bestD2 = 0;
    for (int d = 0; d < 8; d += 2) {
      cXObjectImpl* seat = SeatOnSide(counter, d);
      if (!seat || !SpotFree(counter, d, seat, me)) continue;
      const int d0 = seat->m_location[0] - me->m_location[0], d1 = seat->m_location[1] - me->m_location[1];
      const int d2 = seat->GetContainedObject(0) == me ? -1 : d0 * d0 + d1 * d1;
      if (!best || d2 < bestD2) best = seat, bestSide = d, bestD2 = d2;
    }
    if (Trace()) {
      const std::string who = me->DebugName(), at = counter->DebugName();
      std::printf("[counter-eat] %s at %s: %s (slot %d)\n", who.c_str(), at.c_str(), best ? best->DebugName() : "no stool",
                  best ? SlotForSide(counter, bestSide) : -1);
    }
    if (!best) return 0;
    me->m_temps[0] = best->m_id;
    me->m_temps[1] = static_cast<short>(SlotForSide(counter, bestSide));
    return 1;
  }
  if (op == 1) {
    const int slot = me->m_temps[1];
    if (slot < kCounterEatSlot || slot >= kCounterEatSlot + 4) return 0;
    const int d = SideForSlot(counter, slot);
    cXObjectImpl* seat = SeatOnSide(counter, d);
    const bool ok = seat && seat->m_id == me->m_temps[0] && seat->GetContainedObject(0) == me && !counter->GetContainedObject(slot);
    if (Trace()) {
      const std::string who = me->DebugName();
      std::printf("[counter-eat] %s seated at %s slot %d: %d\n", who.c_str(), counter->DebugName(), slot, ok ? 1 : 0);
    }
    return ok ? 1 : 0;
  }
  if (op == 2) {
    cXObject* fx = me->GetObjectFromID(me->m_temps[0]);
    cXObjectImpl* food = fx ? fx->GetObjectImplementation() : nullptr;
    if (!food || food->GetData(2) != counter->m_id) return 0;
    const int slot = food->GetData(3);
    if (slot < kCounterEatSlot || slot >= kCounterEatSlot + 4) return 0;
    cXObjectImpl* seat = SeatOnSide(counter, SideForSlot(counter, slot));
    cXObjectImpl* occupant = seat ? seat->GetContainedObject(0) : nullptr;
    if (!seat || (occupant && occupant != me)) return 0;
    me->m_temps[0] = seat->m_id;
    return 1;
  }
  return 0;
}

const unsigned char* SlotDescriptor(cXObjectImpl* counter, int index) {
  if (index < kCounterEatSlot || index >= kCounterEatSlot + 4 || !IsCounterTop(counter)) return nullptr;
  const unsigned char* base = ObjectSlotDescriptor(counter, 0);
  if (!base) return nullptr;
  static std::mutex lock;
  static std::map<std::pair<const unsigned char*, int>, std::array<unsigned char, 0x48>> copies;
  std::lock_guard<std::mutex> guard(lock);
  auto it = copies.find({base, index});
  if (it == copies.end()) {
    // The table's side slots: 4/16 tile toward the side (local x +4 its right, y -4 its front).
    static const float kOffset[4][2] = {{0.0f, -4.0f}, {4.0f, 0.0f}, {0.0f, 4.0f}, {-4.0f, 0.0f}};
    std::array<unsigned char, 0x48> d;
    std::memcpy(d.data(), base, d.size());
    for (int k = 0; k < 2; ++k) {
      unsigned int bits = 0;
      std::memcpy(&bits, &kOffset[index - kCounterEatSlot][k], 4);
      unsigned char* p = d.data() + 4 + k * 4;
      p[0] = static_cast<unsigned char>(bits >> 24), p[1] = static_cast<unsigned char>(bits >> 16);
      p[2] = static_cast<unsigned char>(bits >> 8), p[3] = static_cast<unsigned char>(bits);
    }
    it = copies.emplace(std::make_pair(base, index), d).first;
  }
  return it->second.data();
}

}  // namespace simmy::countereat
