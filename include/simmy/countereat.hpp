// PC mod: mods. Eating at a counter on a bar stool (game/countereat.cpp).
//
// The GameCube's sims eat at tables only: the food's "Eat" tree (food_carry_sg) asks for the best
// eating surface (TryFindFunctionalObject function 7: objects with tree table entry 0x19, the tables'
// table_sg "Function - Eating Surface"), whose tree routes to a chair facing it, sits, and puts the
// plate in the table's slot on that chair's side. A counter (food_counter_sg) has no such entry. With a
// mod bar stool (ModObject::counterSeat) facing a counter, the counter offers one: PC trees (BHAV
// bytecode written here, ids 0x1F00..) do what the table's do with the stool and a slot of the counter
// on the stool's side (indices kCounterEatSlot.., not in its slot list: ObjectSlotDescriptor makes
// them). Nothing changes while no loaded mod object is a counter seat.
#pragma once

struct cXObjectImpl;
struct StackElem;

namespace simmy::countereat {

constexpr short kEatTree = 0x1F00;       // the counter's "eating surface" (entry 0x19) action tree
constexpr short kEatTest = 0x1F01;       // its check tree
constexpr short kChairFromFood = 0x1F02; // stands in for the tables' named tree ct_chair_id_from_food_id
constexpr short kPickupTree = 0x1F03;    // the counter's pickup (entry 0x17) for a thing in an eating slot
constexpr int kCounterEatSlot = 0x60;    // the counter's eating slots: + (side - its facing) / 2
constexpr int kScore = 65;               // its eating-surface score (data 0x26 of the cheapest GC table)

// Some loaded mod object is a counter seat (bar stool).
bool Active();
// A counter top (tree table entries 0x12 "prep" and 0x14 "surface": the GameCube's counters, the mods'
// counters and islands copied from them).
bool IsCounterTop(cXObjectImpl* o);
// `o` is a counter with a counter seat on a side, facing it (Active() only).
bool IsEatSurface(cXObjectImpl* o);
// The thing `me` picks up (its temp 0: the pickup function's param 0) is in an eating slot of `counter`
// with a stool on that side: kPickupTree instead of the counter's own (its front only).
bool IsEatSlotPickup(cXObjectImpl* counter, cXObjectImpl* me);
// The PC trees by id (0x1F00..0x1F0F; null: not one), and their names.
const unsigned char* Tree(short id);
const char* TreeName(short id);
// A named tree a counter lacks that the tables have (ct_chair_id_from_food_id): the PC stand-in, else 0.
short NamedTree(cXObjectImpl* target, const char* name);
// Primitive 0x60 (PC trees only), see countereat.cpp.
int TryCounterSeat(cXObjectImpl* me, StackElem* frame, const unsigned char* params);
// A counter's eating slot `index` (kCounterEatSlot..+3): its slot 0 moved 1/4 tile toward that side.
const unsigned char* SlotDescriptor(cXObjectImpl* counter, int index);

}  // namespace simmy::countereat
