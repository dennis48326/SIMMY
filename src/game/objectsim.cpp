// Port of game/objectsim.cpp (partial): an object's behaviour primitives (cXObjectImpl::TryElement and
// the primitives it runs), the expression primitive and its variable scopes (InterpValue), and
// game/objecterr.cpp (Error, HandleError). Primitives not ported log their opcode once and succeed.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/roomresolve.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/inventory_pc.hpp"
#include "simmy/sn_qsort.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
short tempDataPrim = 0;     // tempDataPrim: what unreadable scopes read and write
short lastObjectPrimCode = 0;  // lastObjectPrimCode

int Be16(const unsigned char* p, int i) { return static_cast<short>(simmy::be16(p + 2 * i)); }

// A primitive not ported: logged once by opcode (with its handler's address), then "true".
int Unported(int op, const char* name, unsigned int addr) {
  static char names[0x80][80];
  if (op >= 0 && op < 0x80 && !names[op][0]) std::snprintf(names[op], sizeof names[op], "primitive 0x%02x %s", op, name);
  SIMMY_STUB(op >= 0 && op < 0x80 ? names[op] : name, addr);
  return 1;
}

// 80111588 ObjSelector::CountTypeAttributes / 801115E8 GetTypeAttributes: the object type's 'TATT'
// block (by the definition's +0xB8 master GUID, else its GUID); its count, *values its values.
int TypeAttributesOf(cXObjectImpl* o, short** values) {
  *values = nullptr;
  const ObjDefinition* def = o && o->m_selector ? o->m_selector->m_def : nullptr;
  if (!def || !Globs_pObjectFolder) return 0;
  int guid = static_cast<int>(ERQuickdata::U32(def, 0xb8));
  if (guid == 0) guid = static_cast<int>(objdef::GUID(def));
  ObjectFolderImpl::TypeAttrBlock* block = Globs_pObjectFolder->GetTypeAttrBlock(guid);
  if (!block || block->m_values.empty()) return 0;
  *values = block->m_values.data();
  return static_cast<int>(block->m_values.size());
}

// PC development aid: SIMMY_VM_TRACE=1 traces every primitive, SIMMY_VM_TRACE=<id> one object's.
bool TraceVM(int id = -1) {
  static const char* env = std::getenv("SIMMY_VM_TRACE");
  static const int only = env ? std::atoi(env) : 0;
  if (!env) return false;
  return only <= 1 || id < 0 || id == only;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// The behaviour stack

int cXObjectImpl::TreeSimError() const { return m_treeSim ? m_treeSim->GetError() : 0; }
void cXObjectImpl::ClearTreeSimError() {
  if (m_treeSim) m_treeSim->ClearError();
}

const char* cXObjectImpl::DebugName() {
  static char name[96];
  const char* file = m_selector && m_selector->m_fileName ? m_selector->m_fileName : "?";
  std::snprintf(name, sizeof name, "%s #%d", file, m_id);
  return name;
}

// 800EB338
Behavior* cXObjectImpl::GetBehavior() { return Behavior::For(m_selector); }

// 800EAC88
// GC: through ObjSelector::GetFnTable (an ObjFnTableQuickData of ResFile +0x2C, id = definition +0xBC;
// without one, BuildFromOldEntries gives the global init and main trees).
short cXObjectImpl::GetTreeID(int entry) {
  const unsigned char* file = SelectorResFile(m_selector);
  if (entry < 0 || entry >= 0x1e || !file || !m_def) return 0;
  const auto* list = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x2c));
  if (!list) return 0;
  const int id = ERQuickdata::S16(m_def, 0xbc);
  const unsigned int n = ERQuickdata::U32(list - 4, 0);
  for (unsigned int i = 0; i < n; ++i) {
    const unsigned char* e = list + i * 0x7c;
    if (ERQuickdata::S32(e, 0) == id) return ERQuickdata::S16(e, 4 + entry * 2);
  }
  SIMMY_STUB("cXObjectImpl::GetTreeID: ObjFnTableQuickData::BuildFromOldEntries (the global init/main trees)",
             0x80110158);
  return 0;
}

// 800E2F14
// The tree of my function table's entry `entry` run at once to its end on my stack (TreeSim vtable 0xA4
// RunOneTickTree) with that stack object; its result (0 without such a tree).
int cXObjectImpl::RunTree(int entry, short stackObject, short* params) {
  const short tree = GetTreeID(entry);
  if (!tree || !m_treeSim) return 0;
  return m_treeSim->RunOneTickTree(GetBehavior(), tree, stackObject, params);
}

void cXObjectImpl::StartBehavior() {
  if (!m_treeSim) {
    m_treeSim = new TreeSimImpl(this);
    m_treeSim->Initialize(0, m_temps);
  }
  // GC cXObjectImpl::Reset (800DA848): an object without a main tree has no main tree sim (it idles).
  m_treeSim->SetNoMainTreeSim(GetTreeID(1) == 0);
  m_treeSim->Reset(GetBehavior(), GetTreeID(1));
  ResumeBehavior();
}

void cXObjectImpl::ResumeBehavior() {
  if (!m_treeSim) {
    StartBehavior();
    return;
  }
  m_treeSim->SetNoMainTreeSim(GetTreeID(1) == 0);
  EnableSim(true);
  if (Globs_pObjectModule) Globs_pObjectModule->SetIdleStatus(m_id, -1);
  // PC development aid: SIMMY_STORY=0 keeps the story mode's objects (the scenario and the tutorial)
  // still, so interactions can be tried on their own (the sim where SIMMY_SIM_START puts it, no
  // dialogs, the saved motives).
  static const char* story = std::getenv("SIMMY_STORY");
  if (story && story[0] == '0' && m_selector && m_selector->m_fileName &&
      (std::strncmp(m_selector->m_fileName, "scenario_", 9) == 0 || std::strcmp(m_selector->m_fileName, "tutorial") == 0))
    EnableSim(false);
}

// 800EB7BC
cXObject* cXObjectImpl::GetObjectFromID(short id) {
  return Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(id) : nullptr;
}

// 800EAA48
void cXObjectImpl::SetMiscFlag(cXObject__MiscFlag flag, bool on) {
  m_miscFlags &= ~static_cast<int>(flag);
  if (on) m_miscFlags |= static_cast<int>(flag);
}
// 800EAA68
int cXObjectImpl::GetMiscFlag(cXObject__MiscFlag flag) { return (m_miscFlags & static_cast<int>(flag)) != 0; }

// 800EAF80
int cXObjectImpl::SimEnabled() { return Globs_pObjectModule ? Globs_pObjectModule->GetSimFlag(m_id, 1) : 0; }
// 800EAFC0
void cXObjectImpl::EnableSim(bool on) {
  if (Globs_pObjectModule) Globs_pObjectModule->SetSimFlag(m_id, 1, on);
}
// 800EB004
int cXObjectImpl::GetIdleStatus() { return Globs_pObjectModule ? Globs_pObjectModule->GetIdleStatus(m_id) : -1; }
// 800EB040
void cXObjectImpl::SetIdleStatus(int ticks) {
  if (Globs_pObjectModule) Globs_pObjectModule->SetIdleStatus(m_id, ticks);
}
// 800EB080
void cXObjectImpl::ClearIdleStatus() { SetIdleStatus(-1); }

// 800EBA98
// A tree error: flagged (misc 0x100) and the object waits for a reset (sim flag 4).
void cXObjectImpl::Error(short code) {
  SetMiscFlag(0x100, true);
  if (Globs_pObjectModule) Globs_pObjectModule->SetSimFlag(m_id, 4, true);
  static int logged = 0;
  if (TraceVM(m_id) || logged < 40) {
    ++logged;
    short tree = -1, node = -1;
    if (m_treeSim) m_treeSim->GetCurrentNode(&tree, &node);
    std::printf("[vm] TREEASSERT error %d: %s tree %04x node %d; stack", code, DebugName(), static_cast<unsigned short>(tree),
                node);
    for (int i = 0; m_treeSim && i < m_treeSim->GetStackSize(); ++i) {
      StackElem* e = m_treeSim->GetNthElem(static_cast<short>(i));
      std::printf(" %04x:%d(obj %d", static_cast<unsigned short>(e->GetTreeID()), e->m_node, e->m_stackObject);
      for (int k = 0; k < e->m_numParams && k < 4; ++k) std::printf(" %d", e->GetParams()[k]);
      std::printf(")");
    }
    std::printf("\n");
  }
}

// 800EBB30
// GC: logs "TREEASSERT:" with the tree and node, tells the module (people let go of it), and resets the
// object. PC: an object that keeps failing at once after its reset is switched off (its tree needs
// primitives that are not ported), so the clock goes on.
void cXObjectImpl::HandleError() {
  if (!GetMiscFlag(0x100)) return;
  SetMiscFlag(0x100, false);
  if (!TreeSimError()) return;
  // The people let go of it (ObjectModule vtable 0x134 CleanupPeople).
  if (Globs_pObjectModule) Globs_pObjectModule->CleanupPeople(this);
  // PC: errors less than 300 ticks apart count as repeats (a tree that fails, resets, runs a while
  // and fails again would otherwise stop every later object's tick for good).
  const int now = Globs_pSimulator ? Globs_pSimulator->GetTicks() : 0;
  if (m_errorRepeats && now - m_lastErrorTick > 300) m_errorRepeats = 0;
  m_lastErrorTick = now;
  if (++m_errorRepeats >= 3) {
    std::printf("[vm] %s keeps failing: its behaviour is switched off\n", DebugName());
    m_treeSim->m_stack.Reset();
    m_treeSim->ClearError();
    EnableSim(false);
    if (Globs_pObjectModule) Globs_pObjectModule->SetSimFlag(m_id, 4, false);
    return;
  }
  // Cleanup(nullptr) through the cXObjectImpl vtable (0x1C): an object without a cleanup tree is put
  // back as made (Reset), else runs it; a person (cXPersonImpl::Cleanup) lets go of everything.
  Cleanup(nullptr);
}

// 800DA848
// Back to as made. Out of its site while its data is reset (kept: its room 0x1D, direction 1, value
// 0x29, purchase date 0x2D-0x2F, hour 0x18, minute 0x15, slot count 0x43; 0xB its id, 0x3A the
// definition's +0x62) and its "placed from the inventory" flag; attributes and temps 0; the tree sim
// back to its main tree (none: no main tree sim); its site again, data 2 / 3 its container and slot
// (GetPlacementSpec); its age; its idle status cleared; its passive influences gone. Not a multi-tile part
// (cXMTObjectImpl::Reset does those for the group): its init tree at once, with `run` (unless author
// mode) one tick of its main tree when it has one, then its sim flags.
void cXObjectImpl::Reset(bool run) {
  // GC: the sync object (+0xDC..+0xE4) cleared (PC: not kept).
  HierSite site;
  ::HierGetSite(this, site);
  ::HierSever(this);
  const int fromInventory = ObjGetWasPlacedFromInventory(this);
  static const int kKept[] = {0x1d, 1, 0x29, 0x2d, 0x2e, 0x2f, 0x18, 0x15, 0x43};
  short kept[9];
  for (int i = 0; i < 9; ++i) kept[i] = m_data[kKept[i]];
  m_data.Reset();
  for (int i = 0; i < 9; ++i) m_data.WriteVar(kKept[i], kept[i]);
  m_data.WriteVar(0xb, m_id);
  m_data.WriteVar(0x3a, m_def ? ERQuickdata::S16(m_def, 0x62) : 0);
  ObjSetWasPlacedFromInventory(this, fromInventory != 0);
  for (int i = 0; i < m_attrCount; ++i) m_attrs[i] = 0;
  for (short& t : m_temps) t = 0;
  const short mainTree = GetTreeID(1);
  if (!m_treeSim) {  // PC: the tree sim is made with the behaviour (GC: Initialize)
    m_treeSim = new TreeSimImpl(this);
    m_treeSim->Initialize(0, m_temps);
  }
  m_treeSim->SetNoMainTreeSim(mainTree == 0);
  m_treeSim->Reset(GetBehavior(), mainTree);
  ::HierSetSite(this, site);
  PlaceSpec spec(false);
  ObjGetPlacementSpec(this, spec);
  m_data.WriteVar(2, static_cast<short>(spec.container ? spec.container->m_id : 0));
  m_data.WriteVar(3, static_cast<short>(spec.slot));
  UpdateAge();
  SetIdleStatus(-1);
  if (PassiveInfluenceSystem* influence = Globs_pSimulator ? Globs_pSimulator->GetPassiveInfluenceSystem() : nullptr)
    influence->RemoveObjectFromMap(this);
  if (!IsMultiTile()) {
    RunTree(0, 0, nullptr);
    if (run && _gd.m_fieldF0 == 0 && m_treeSim->HasMainTreeSim()) Simulate(0);  // DAT_80475C74: author mode
    UpdateSimFlags();
  }
}

// 800DB16C
// What it does when everything is cleaned up (`object` null: after a tree error; GC does nothing for an
// object going). Without a cleanup tree (entry 3): picked up, then (unless misc flag 0x40) reset and put
// back where it was. With one: a single object runs it at once, its user count (data 0x3E) 0, data 8
// flag 0x20 off, its stack back to its main tree; a multi-tile group's parts with one each do (their
// stacks to this object's main tree). Then its idle status cleared and its sounds quietened. GC: also
// the module's tutorial object forgotten if it is this one (PC: never set).
void cXObjectImpl::Cleanup(cXObject* object) {
  if (object) return;
  if (GetTreeID(3) == 0) {
    PlaceSpec spec(false);
    ObjGetPlacementSpec(this, spec);  // GetPlacementInfo (800DCACC)
    ObjPickup(this);
    if (!GetMiscFlag(cXObject__MiscFlag{0x40})) {
      Reset(true);  // through the cXObjectImpl vtable (0x2C)
      if (spec.inWorld) ObjPlace(this, spec.pos, spec.level, spec.container, spec.slot);
    }
  } else if (!IsMultiTile()) {
    RunTree(3, 0, nullptr);
    m_data.WriteVar(0x3e, 0);
    m_data.WriteVar(8, static_cast<short>(m_data[8] & ~0x20));
    if (m_treeSim) m_treeSim->Reset(GetBehavior(), GetTreeID(1));
  } else if (auto* mt = dynamic_cast<cXMTObjectImpl*>(this)) {
    for (cXMTObjectImpl* p = mt->GetFirstMultiTileObject(); p; p = p->GetNextMultiTileObject()) {
      if (p->GetTreeID(3) == 0) continue;
      p->RunTree(3, 0, nullptr);
      p->m_data.WriteVar(0x3e, 0);
      p->m_data.WriteVar(8, static_cast<short>(p->m_data[8] & ~0x20));
      if (p->m_treeSim) p->m_treeSim->Reset(GetBehavior(), GetTreeID(1));
    }
  }
  if (Globs_pObjectModule) Globs_pObjectModule->SetIdleStatus(m_id, -1);
  if (Globs_pSound) Globs_pSound->QuietBySourceID(m_id);
}

// 800F9E6C
// Idle ticks can be skipped by the simulator (SetIdleStatus) unless it is a person, its data 0x19 runs, or
// a sprite slot is busy (PC: sprite slots are not ported).
bool cXObjectImpl::AllowIdleOptimization() { return GetType() != 2 && m_data[0x19] == 0; }

// 801096C4
int cXObjectImpl::Simulate(int) {
  if (!m_treeSim) return 1;
  const int r = m_treeSim->Simulate();
  if (r) {
    if (m_data[0x19] != 0) m_data.WriteVar(0x19, static_cast<short>(m_data[0x19] - 1));
    for (SpriteSlot& slot : m_spriteSlots) slot.Tick();
  }
  return r;
}

// 80109DF0
// `tree` of `object`'s behaviour on this one's stack, with `object` as its stack object.
int cXObjectImpl::GosubObjectTree(cXObject* object, const short* params, short tree, bool) {
  ClearIdleStatus();
  if (!m_treeSim || !object) return 0;
  cXObjectImpl* impl = object->GetObjectImplementation();
  if (!m_treeSim->Gosub(impl ? impl->GetBehavior() : nullptr, params, tree)) return 0;
  m_treeSim->GetCurElem()->m_stackObject = static_cast<short>(object->GetID());
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// Primitives

// 80109770
int cXObjectImpl::TryElement(StackElem* frame, short op, const unsigned char* params) {
  lastObjectPrimCode = op;
  int r;
  switch (op) {
    case 0x00: r = TryIdle(frame, params); break;
    case 0x01: r = TryGenericSimCall(frame, params); break;
    case 0x02: r = TryExpression(params); break;
    case 0x04: r = TryGrab(frame, params); break;
    case 0x05: r = TryDrop(); break;
    case 0x07: r = TryUpdate(frame, params); break;
    case 0x08: r = TryRandom(frame, params); break;
    case 0x09: r = TryBurn(frame, params); break;
    case 0x0a: r = Unported(op, "TryTutorial", 0x800FF1DC); break;
    case 0x0b: r = TryDistanceTo(frame, params); break;
    case 0x0c: r = TryDirectionTo(frame, params); break;
    case 0x0d: r = TryPushAction(frame, params); break;
    case 0x0e: r = TryFindFunctionalObject(frame, params); break;
    case 0x0f: r = TryTreeBreak(frame, params); break;
    case 0x10: r = TryFindGoodLocation(frame, params); break;
    case 0x12: {
      short id = 0;
      r = TryKillObject(frame, params, &id);
      break;
    }
    case 0x13: r = TryIsPersonInArea(frame, params); break;
    case 0x14: r = TryCallFunctionalTree(frame, params); break;
    case 0x15: r = Unported(op, "TryShowString", 0x80105420); break;
    case 0x17: r = static_cast<int>(TryPlaySound(frame, (PlaySoundParam*)params)); break;  // game/soundprims.cpp
    case 0x18: r = TryRelationship(frame, params); break;
    case 0x19: r = TryBudget(frame, params); break;
    case 0x1a: r = TryRelationship2(frame, params); break;
    case 0x1c: r = TryCallNamedTree(frame, params); break;
    case 0x1d: r = TrySetMotiveDelta(frame, params); break;
    case 0x1f: r = TrySetToNext(frame, params); break;
    case 0x20: r = TryTestObjectType(frame, params); break;
    case 0x21: r = TryFind5WorstMotives(frame, params); break;
    case 0x22: r = Unported(op, "TryUIEffect", 0x800FA1E8); break;
    case 0x23: r = TryUserEvent(frame, params); break;
    case 0x24: r = TryDialog(frame, params, false); break;
    case 0x27: r = TryInventoryCommand(frame, params); break;
    case 0x28: r = TryCameraControl(frame, params); break;
    case 0x29: r = TrySetBalloon(frame, params); break;  // sprites.cpp
    case 0x2a: r = TryCreateObject(frame, params); break;
    case 0x2b: r = TryDropOnto(frame, params); break;
    case 0x2c: r = TryAnimate(frame, params); break;
    case 0x2e: r = TrySnap(frame, params); break;
    case 0x30: r = static_cast<int>(TryKillSounds(frame, (KillSoundsParam*)params)); break;  // game/soundprims.cpp
    case 0x31: r = TryNotifyStackObject(frame, params); break;
    case 0x32: r = TryMakeActionString(frame, params); break;
    case 0x33: r = TryPreloadObject(frame, params); break;
    case 0x34: r = TryRoomEffect(frame, params); break;
    case 0x35: r = TryCheckObject(frame, params); break;
    case 0x36: r = TryGbaCommunication(frame, params); break;
    case 0x37: r = TryAnimateObject(frame, params); break;
    case 0x38: r = TryCompleteGoal(frame, params); break;
    case 0x39: r = Unported(op, "TryHUDCreate", 0x8010E674); break;
    case 0x3a: r = Unported(op, "TryHUDAddItem", 0x8010E8B4); break;
    case 0x3b: r = Unported(op, "TryHUDManage", 0x8010EC30); break;
    case 0x3c: r = Unported(op, "TryHUDData", 0x8010ED00); break;
    case 0x3d: r = Unported(op, "TryParticleEffect", 0x80108384); break;
    case 0x3f: r = TryMemory(frame, params); break;
    case 0x41: r = TryPassiveInfluence(frame, params); break;
    case 0x42: r = TryWantsAndFearsNotify(frame, params); break;
    case 0x43: r = TryChangeWantFearTree(frame, params); break;
    default:
      if (m_treeSim) m_treeSim->SetError(5);
      Error(5);
      return -1;
  }
  lastObjectPrimCode = static_cast<short>(-1000 - op);
  // PC dev trace: SIMMY_VM_TRACE_TREES=<hex>,<hex>... the primitives of those trees (any object).
  static const char* trees = std::getenv("SIMMY_VM_TRACE_TREES");
  bool inTree = false;
  if (trees && m_treeSim) {
    short tree = -1, node = -1;
    m_treeSim->GetCurrentNode(&tree, &node);
    char hex[8];
    std::snprintf(hex, sizeof hex, "%04x", static_cast<unsigned short>(tree));
    inTree = std::strstr(trees, hex) != nullptr;
    // SIMMY_VM_TRACE_WHO=<text>: only objects whose name contains it.
    static const char* who = std::getenv("SIMMY_VM_TRACE_WHO");
    if (inTree && who && !std::strstr(DebugName(), who)) inTree = false;
  }
  if (TraceVM(m_id) || inTree) {
    short tree = -1, node = -1;
    if (m_treeSim) m_treeSim->GetCurrentNode(&tree, &node);
    std::printf("[vm] %s %04x:%d prim %02x -> %d\n", DebugName(), static_cast<unsigned short>(tree), node, op, r);
  }
  return r;
}

// 801065B0
// Operands: a flags word (0x1000000 the stack object instead of me, 0x2000000 no event local, 0x4000000
// only preload it (0x8000000: then wait (vtable 0xEC) for operand +6 ticks, else vtable 0xDC); the event
// local in bits 16..23, the animation's scope in the low half) and the animation's data in +4. The
// object's model plays the animation state (ISimsObjectModel::TryAnimateObject): 2 while it plays, an
// event of it (false, the event in the local), true at its end.
int cXObjectImpl::TryAnimateObject(StackElem* frame, const unsigned char* params) {
  const unsigned int word = simmy::be32(params);
  cXObjectImpl* target = this;
  if (word & 0x1000000) {
    cXObject* x = GetObjectFromID(frame->m_stackObject);
    target = x ? x->GetObjectImplementation() : nullptr;
  }
  auto* model = target && target->m_instance ? dynamic_cast<ISimsObjectModel*>(target->m_instance) : nullptr;
  short anim = 0;
  if (target && InterpValue(static_cast<short>(word & 0xffff), Be16(params, 2), nullptr, nullptr, &anim) == -1) return -1;
  if (!model) {
    m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  if (!(word & 0x4000000)) {
    short* event = (word & 0x2000000) ? nullptr : &frame->GetLocals()[(word >> 16) & 0xff];
    return model->TryAnimateObject(anim, event);
  }
  SIMMY_STUB("primitive 0x37 TryAnimateObject: the preload's wait (cXObject vtable 0xDC / 0xEC)", 0x801065B0);
  return model->PreloadAnimation(anim);
}

PcMenuBuild* g_pcMenuBuild = nullptr;
// edithdialogprim.cpp
std::u16string ActionStringText(StackElem* frame, int set, int index, cXObjectImpl* me);

// 800FD890
// Names the action: string `params[4]` of the tree's string set `params[0]`, parsed for the UI
// (ParseUIString: $Me, $Object, ...). Built while a menu is being made (ObjTestSim::IsMenuInProgress:
// a new menu item with the interaction's stack vars, MakeNewMenuItem) or in the main simulation (the
// person's current action's text, SetCurrentActionText, for the queue in the HUD); never in other check
// trees. True.
int cXObjectImpl::TryMakeActionString(StackElem* frame, const unsigned char* params) {
  // A menu being built (ObjTestSim::IsMenuInProgress): a new menu item (ObjTestSim::MakeNewMenuItem) with
  // the text and stack vars {the frame's stack object, 0, 0, 0} (ParseUIString may set them; PC: not).
  if (PcMenuBuild* build = g_pcMenuBuild) {
    PcMenuItem item;
    item.name = ActionStringText(frame, Be16(params, 0), params[4], this);
    item.vars[0] = frame ? frame->m_stackObject : 0;
    if (frame && frame->m_numLocals > 0) item.locals.assign(frame->GetLocals(), frame->GetLocals() + frame->m_numLocals);
    build->push_back(item);
    return 1;
  }
  if (!TreeSimImpl::sInMainSim) return 1;
  // In the main simulation, a person's current action takes the text as its name.
  if (auto* person = dynamic_cast<cXPersonImpl*>(this)) person->SetCurrentActionText(ActionStringText(frame, Be16(params, 0), params[4], this));
  return 1;
}

// 801058E0
// Tells the world an object changed: operand +0 which (0 me, 1 the stack object), +2 what (0 nothing, 1
// its graphics/room (GlobalDispatch 0xEE: ObjectModule re-sends 0xED with its room: the room's
// statistics and light are recomputed), 2 its room's score, 3 its 3D instance placed again, 4 the
// house's lot data (HouseImpl::ComputeAndStoreLotData)). True.
int cXObjectImpl::TryUpdate(StackElem* frame, const unsigned char* params) {
  const int who = Be16(params, 0), what = Be16(params, 1);
  cXObjectImpl* o = nullptr;
  if (who == 0) {
    o = this;
  } else if (who == 1) {
    cXObject* x = GetObjectFromID(frame->m_stackObject);
    o = x ? x->GetObjectImplementation() : nullptr;
    if (!o) {  // GC: true in "stupid mode" (Globs::bStupidMode, set only while a placement is tested)
      m_treeSim->SetError(0x15);
      Error(0x15);
      return -1;
    }
  }
  if (!o || what < 0 || what > 4) {
    m_treeSim->SetError(3);
    Error(3);
    return -1;
  }
  switch (what) {
    case 1: ObjectModuleDispatch(0xee, o->m_id); break;  // GlobalDispatch(0xEE, the object's id)
    case 2:
      // The object's room's score (not the 0xFFFB "no room").
      if (o->GetRoom() != 0xfffb)
        if (RoomManager* rooms = RoomManager::GetRoomManager()) rooms->RoomScoreChanged(o->GetRoom());
      break;
    case 3:  // its 3D instance placed again (IBaseSimInstance vtable 0x14: SetObjOrient)
      if (o->m_instance) o->m_instance->SetObjOrient();
      break;
    case 4:  // Globs::pHouse vtable 0x8C: the lot data
      if (Globs_pHouse) Globs_pHouse->ComputeAndStoreLotData();
      break;
    default: break;
  }
  return 1;
}

// 80101110
// Sets the target (the stack object, or with flag 0x80 the variable in operands 5 and 7) to the next
// object after it of a kind (flags & 0xF): 0 any object, 1 a person, 2 a non-person, 3 the next part of
// its multi-tile group (round the group), 4 objects of the operand GUID, 6 objects whose data 0x3B is
// param 0, 8 objects on the target's tile (round the tile), 9 the objects east, south, west and north
// of the object in a local (in its room), 0xC objects in the world; 5 / 7 neighbours, 0xA careers, 0xB
// neighbour houses. Flag 0x40: interaction tiles only (not kinds 1, 0xC). False: no more (kinds 1, 3,
// 4, 5, 7: the target 0; the others keep it); kinds 0xD..0xF: error 0x1E.
// PC: objects in id order (GC: the module's object list for kinds 0, 2, 6, 0xC and 0xD..0xF, the
// module's person list for kind 1, the tile's object list for kind 8).
int cXObjectImpl::TrySetToNext(StackElem*, const unsigned char* params) {
  const unsigned char flags = params[4];
  const int scope = flags & 0x80 ? static_cast<signed char>(params[5]) : 10;
  const int data = flags & 0x80 ? static_cast<signed char>(params[7]) : 0;
  short* target = nullptr;
  if (InterpValue(scope, data, &target, nullptr, nullptr) == -1) return -1;
  if (!target) {
    m_treeSim->SetError(0x13);
    Error(0x13);
    return -1;
  }
  const int kind = flags & 0xf;
  ObjectModuleImpl* module = Globs_pObjectModule;
  auto set = [&](int id) {
    InterpWriteValue(scope, data, &target, static_cast<short>(id));
    return id != 0 ? 1 : 0;
  };
  if (kind == 3) {
    // The next part of the target's chain (after the last: the lead; a lone lead: none). Flag 0x40: the
    // next one that is an interaction tile (round the group).
    cXObject* o = GetObjectFromID(*target);
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    auto* part = impl && impl->IsMultiTile() ? dynamic_cast<cXMTObjectImpl*>(impl) : nullptr;
    if (!part) return set(0);
    auto after = [](cXMTObjectImpl* p) {
      cXMTObjectImpl* n = p->GetNextMultiTileObject();
      return n ? n : p->m_lead;
    };
    cXMTObjectImpl* next = after(part);
    if (flags & 0x40) {
      for (; next && next != part; next = after(next))
        if (next->IsInteractionTile()) break;
      if (next == part) return 0;  // round the group to the target: false, the target kept
    }
    return set(next ? next->m_id : 0);
  }
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  if (kind == 5) {
    // The next neighbour (id).
    return set(hood->GetNextNeighborID(*target));
  }
  if (kind == 7) {
    // The next neighbour of the operand GUID.
    const unsigned int guid = simmy::be32(params);
    for (;;) {
      InterpWriteValue(scope, data, &target, static_cast<short>(hood->GetNextNeighborID(*target)));
      if (*target == 0) return 0;
      Neighbor* n = hood->FindNeighborByID(*target);
      if (n && n->m_selector && n->m_selector->GetGUID() == guid) return 1;
    }
  }
  if (kind == 10) {
    // The next career (id), in the careers' order.
    CareersImpl* careers = static_cast<CareersImpl*>(Globs_pCareers);
    if (!careers) return 0;
    const int count = careers->GetNumCareers();
    const Career* cur = careers->GetCareerByID(*target);
    const int next = cur ? careers->GetIndexByCareer(cur) + 1 : 0;
    if (next >= count) return 0;
    InterpWriteValue(scope, data, &target, static_cast<short>(career::Id(careers->GetCareerByIndex(next))));
    return 1;
  }
  if (kind == 0xb) {
    // The next neighbour house (round the list).
    const int count = hood->GetNumNeighborHouses();
    int i = 0, house = 0;
    if (*target == 0) {
      house = hood->GetNeighborHouseByIndex(0);
    } else {
      for (; i < count; ++i) {
        if (hood->GetNeighborHouseByIndex(i) == *target) {
          if (++i >= count) i = 0;
          house = hood->GetNeighborHouseByIndex(i);
          break;
        }
      }
    }
    if (i == count) return 0;
    InterpWriteValue(scope, data, &target, static_cast<short>(house));
    return 1;
  }
  if (kind == 9) {
    // The next object round the object in local +6 (its tile's object east, south, west, north of
    // it, after the target's; flag 0x40: interaction tiles only) in its room. False: none left (the
    // target kept). Errors: the local out of range 8, no such object 10.
    StackElem* frame = m_treeSim->GetCurElem();
    const int local = params[6];
    if (!frame || local >= frame->m_numLocals) {
      m_treeSim->SetError(8);
      Error(8);
      return -1;
    }
    cXObject* cur = GetObjectFromID(*target);
    cXObject* c = GetObjectFromID(frame->GetLocals()[local]);
    cXObjectImpl* center = c ? c->GetObjectImplementation() : nullptr;
    if (!center) {
      m_treeSim->SetError(10);
      Error(10);
      return -1;
    }
    const CTilePt at = center->GetCTilePt();
    int dx = 1, dy = 0;
    bool first = cur == nullptr;
    if (cur) {
      const CTilePt t = cur->GetCTilePt();
      dx = static_cast<signed char>(t.GetX() - at.GetX());
      dy = static_cast<signed char>(t.GetY() - at.GetY());
    }
    for (;;) {
      if (!first) {
        if (dx >= 1) {
          dx = 0, dy = 1;
        } else if (dx < 0) {
          dx = 0, dy = -1;
        } else if (dy < 1) {
          return 0;
        } else {
          dx = -1, dy = 0;
        }
      }
      first = false;
      CTilePt t(at.GetX() + dx, at.GetY() + dy, at.GetLevel());
      if (Globs_pFixedWorld && Globs_pFixedWorld->OutOfGrid(t)) continue;
      cXObject* found = module ? module->GetObjectFromID(module->GetTileObjectID(t)) : nullptr;
      cXObjectImpl* f = found ? found->GetObjectImplementation() : nullptr;
      if (!f || static_cast<unsigned short>(f->GetData(0x1d)) != static_cast<unsigned short>(center->GetData(0x1d))) continue;
      if ((flags & 0x40) && !f->IsInteractionTile()) continue;
      return set(f->m_id);
    }
  }
  // Flag 0x40 (interaction tiles only) does not apply to kinds 1 and 0xC.
  const bool tilesOnly = (flags & 0x40) && kind != 0xc && kind != 1;
  if (kind != 0 && kind != 1 && kind != 2 && kind != 4 && kind != 6 && kind != 8 && kind != 0xc) {
    // Kinds 0xD..0xF walk the object list and fail on its first object: error 0x1E (none: false).
    cXObject* t = *target ? GetObjectFromID(*target) : nullptr;
    const bool any = *target ? t != nullptr && module && module->GetNumObjects() > *target : module && module->GetNumObjects() > 0;
    if (!any) return 0;
    m_treeSim->SetError(0x1e);
    Error(0x1e);
    return -1;
  }
  cXObject* start = GetObjectFromID(*target);
  cXObjectImpl* startImpl = start ? start->GetObjectImplementation() : nullptr;
  const CTilePt startTile = start ? start->GetCTilePt() : CTilePt();
  // Kind 8: the target must be an object in the world (GC: from the target's tile's object list).
  if (kind == 8 && (!startImpl || !ObjIsInWorld(startImpl))) return 0;
  // Kinds 0, 2, 6, 0xC continue from the target (gone: false, the target kept).
  if ((kind == 0 || kind == 2 || kind == 6 || kind == 0xc) && *target != 0 && !start) return 0;
  const unsigned int guid = simmy::be32(params);
  const int count = module ? module->GetNumObjects() : 0;
  auto matches = [&](cXObjectImpl* impl) {
    const bool isPerson = impl->GetType() == 2;
    bool match = true;
    switch (kind) {
      case 1: match = isPerson && dynamic_cast<cXPersonImpl*>(impl) != nullptr; break;
      case 2: match = !isPerson; break;
      case 4: match = impl->m_def && objdef::GUID(impl->m_def) == guid; break;
      case 6: match = impl->GetData(0x3b) == m_treeSim->GetCurElem()->GetParams()[0]; break;
      case 8: {
        const CTilePt t = impl->GetCTilePt();
        match = t.GetX() == startTile.GetX() && t.GetY() == startTile.GetY() && t.GetLevel() == startTile.GetLevel();
        break;
      }
      case 0xc: match = ObjIsInWorld(impl) != 0; break;
      default: break;
    }
    return match && (!tilesOnly || impl->IsInteractionTile());
  };
  // Kind 4 starts again from the first object when the target is past the last (index target - 1
  // out of range, unsigned).
  const int first =
      kind == 4 && static_cast<unsigned int>(*target - 1) >= static_cast<unsigned int>(count) ? 1 : *target + 1;
  for (int id = first; id <= count; ++id) {
    cXObject* o = module->GetObjectFromID(id);
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    if (impl && matches(impl)) return set(id);
  }
  if (kind == 8) {
    // Past the tile's last object: round to its first (GC: the tile's ObjectIterator starts again), the
    // target itself when it is alone there (flag 0x40: back at the target is false).
    for (int id = 1; id <= *target && id <= count; ++id) {
      cXObject* o = module->GetObjectFromID(id);
      cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
      if (!impl) continue;
      if ((flags & 0x40) && id == *target) return 0;
      if (matches(impl)) return set(id);
    }
    return 0;
  }
  // None left: kinds 1 and 4 clear the target; the object-list kinds keep it.
  if (kind == 1 || kind == 4) return set(0);
  return 0;
}

namespace {
// sCameraAnimPending: a camera animation each player's shot waits for (loading).
unsigned int sCameraAnimPending[2] = {};
// g_cameraDefaultDummy (8047xxxx, 0x58 bytes a player): the dummy shots attach to, its one point's
// matrix the identity (PC: that matrix); g_cameraObjectMat: an object's matrix in the camera's axes.
EMat4 g_cameraDummyMat[2];
EMat4 g_cameraObjectMat[2];
}  // namespace

// 80106D7C
// Primitive 0x28, camera control. Operands: +0 the shot (0 cut, 1 interpolate, 2 release to the game
// camera, 3 interpolate to it), +1 the animation in a two-player game (else +3), +2 what the shot looks
// at (0 nothing, 1..8 a corner of the object's bones' box toward its centre, 9 the object's
// transform, 10.. a named dummy of string set 0x191), +3 the animation (1-based, the namespace's
// AnimTable 0x192), +4 flags (1 the stick may cancel it, 2 the global namespace's strings, 4 the
// global AnimTable, 8 StartAnim's flag, 0x10 attached to the stack object (its tile and facing),
// 0x20 do not wait for the animation, 0x40 only ask whether the director is free, 0x80 no letterbox,
// and skipped for a selected sim in classic control unless +6 has 4), +5 the time (+6 bit 2: a
// quarter of it, as a length), +6 flags (1 not cancellable, 8 start at frame temp 0 / 16, 0x10 temp 1
// := 1 (2 when the interpolation said so), 0x20 release when the animation ends), +7 the camera noise
// setting + 2. The camera and director are the player's whose selected sim this is (else player 1's).
// Over ticks (the frame's primitive state): 0 set the shot up, 3 wait for the cut or interpolation,
// 1 start the animation, 2 wait for it to end.
int cXObjectImpl::TryCameraControl(StackElem* frame, const unsigned char* p) {
  int player = 0;
  bool otherSim = false;
  if (GetType() == 2 && dynamic_cast<cXPersonImpl*>(this)) {
    auto selected = [](int i) -> cXObjectImpl* {
      cXPerson* s = _globals.GetSelectedPerson(i);
      cXObject* o = s ? s->AsObject() : nullptr;
      return o ? o->GetObjectImplementation() : nullptr;
    };
    if (selected(0) != this) {
      if (selected(1) == this) player = 1;
      else otherSim = true;
    }
  }
  const bool isSim = GetType() == 2;
  if (sCameraAnimPending[player] != 0) {
    if (!_animman.GetRef(sCameraAnimPending[player])) return 2;
    _animman.DelRef(sCameraAnimPending[player], EResourceManager__DelRefMode{1});
    sCameraAnimPending[player] = 0;
  }
  ESimsCam* cam = _gd.m_players[player].m_cam;  // CameraManager::GetCamera
  CameraDirector* director = _globals.CameraDirectorOf(player);
  if (!director) return 0;
  if (p[4] & 0x40) {
    // Free when idle or the game's; not while player 1's director holds timed.
    const bool active = director->m_state != CameraDirector::kIdle && director->m_state != CameraDirector::kGame;
    CameraDirector* first = _globals.CameraDirectorOf(0);
    if (!active || (first && first->m_state == CameraDirector::kHoldTimed)) {
      frame->m_primState = 0;
      return 1;
    }
    return 0;
  }
  if (p[4] & 1) director->m_cancelMode = 1;
  float startFrame = -1.0f;
  if ((p[6] & 8) && m_temps[0] >= 1) startFrame = static_cast<float>(m_temps[0]) * 0.0625f;
  const bool result = (p[6] & 0x10) != 0;
  if (result) m_temps[1] = 0;
  if ((p[6] & 1) || p[0] == 2 || p[0] == 3) director->m_cancelMode = 0;
  if (director->m_cancelMode == 2) return 0;
  int animIndex = (_gd.m_fieldEC != 0 && p[1] != 0) ? p[1] : p[3];  // DAT_80475C70: two players
  if (isSim && !otherSim && (p[4] & 0x80) && !(p[6] & 4) && PlayerInteractorType(player) == 1) return 0;
  if (static_cast<signed char>(p[7]) != 0 && cam) cam->SetActiveNoiseSetting(static_cast<signed char>(p[7]) - 2);
  // The animation: entry animIndex - 1 of AnimTable 0x192 of the tree's (or the global) namespace.
  AnimRef* ref = nullptr;
  unsigned int anim = 0;
  if (animIndex != 0) {
    Behavior* b = frame->m_behavior;
    const unsigned char* file = b ? ((p[4] & 4) ? b->m_globalResFile : b->m_resFile) : nullptr;
    ref = const_cast<AnimRef*>(AnimTableEntry(file, 0x192, animIndex - 1));
    if (ref) anim = AnimRefId(ref);
  }
  static const bool camTrace = std::getenv("SIMMY_SIM_TRACE") != nullptr;
  if (camTrace && frame->m_primState == 0)
    std::printf("[cam] %s: shot %d target %d anim %d (%08x) flags %02x/%02x, player %d director state %d\n", DebugName(), p[0],
                p[2], animIndex, anim, p[4], p[6], player + 1, director->m_state);
  switch (frame->m_primState) {
    case 0: {
      EMat4* dummy = nullptr;
      EMat4* mat = nullptr;
      if (p[0] == 0 || p[0] == 1) {
        sCameraAnimPending[player] = anim;
        if (anim != 0 && !_animman.IsLoaded(anim)) {
          _animman.AddRefAsync(anim);
          return 2;
        }
        sCameraAnimPending[player] = 0;
        bool attached = false;
        cXObjectImpl* target = this;
        if (p[4] & 0x10) {
          cXObject* so = GetObjectFromID(frame->m_stackObject);
          if (!so) {
            m_treeSim->SetError(0x15);  // (GC also g_bTreeAssert)
            Error(0x15);
            return -1;
          }
          attached = true;
          target = so->GetObjectImplementation();
        }
        const int kind = p[2];
        if (kind != 0 && !attached) {
          if (kind < 10) {
            if (m_instance) {
              if (kind == 9) {
                g_cameraDummyMat[player].Id();
              } else {
                SIMMY_STUB("cXObjectImpl::TryCameraControl: a corner of the model's bone box (targets 1..8)", 0x80106D7C);
                g_cameraDummyMat[player].Id();
              }
              dummy = &g_cameraDummyMat[player];
              m_instance->GetOrient(g_cameraObjectMat[player]);  // GC: the model's matrix (vtable 0x164)
              mat = &g_cameraObjectMat[player];
            }
          } else {
            SIMMY_STUB("cXObjectImpl::TryCameraControl: a named dummy of string set 0x191 (targets 10..)", 0x80106D7C);
          }
        }
        if (attached) {
          // The stack object's tile and facing, in the camera's axes (x and y swapped, x mirrored).
          g_cameraDummyMat[player].Id();
          dummy = &g_cameraDummyMat[player];
          EMat4& m = g_cameraObjectMat[player];
          const float angle = static_cast<float>(target->GetData(1)) * 3.1415927f * 0.25f;
          EHouse* house = _globals.m_house;
          m.Id();
          m.RotateZ(angle);
          EVec3 t{static_cast<float>(target->m_location[1] >> 4) + (house ? house->m_posY : 0.0f),
                  static_cast<float>(target->m_location[0] >> 4) + (house ? house->m_posX : 0.0f), 0.0f};
          m.PostTranslate(t);
          for (int row = 0; row < 4; ++row) std::swap(m.m[row][0], m.m[row][1]);
          EVec3 flip{-1.0f, 1.0f, 1.0f};
          m.PreScale(flip);
          mat = &m;
        }
      }
      switch (p[0]) {
        case 0:
        case 1:
          director->RegisterCurrentCamera(cam, false);
          director->AttachDummy(dummy, dummy ? mat : nullptr);
          if (p[0] == 1) {
            if (anim == 0) {
              director->SetCameraInterp(nullptr, 0.0f, CamDirInterpType{0});
            } else {
              bool interpolated = false;
              const float time = static_cast<signed char>(p[5]);
              if (p[6] & 2) director->SetCameraInterpAnim(static_cast<int>(anim), time * 0.25f, CamDirInterpType{1}, startFrame, &interpolated);
              else director->SetCameraInterpAnim(static_cast<int>(anim), time, CamDirInterpType{0}, startFrame, &interpolated);
              if (result) m_temps[1] = interpolated ? 2 : 1;
            }
          } else {
            if (anim == 0) director->SetCameraCut(nullptr);
            else director->SetCameraCutAnim(static_cast<int>(anim));
          }
          frame->m_primState = dummy ? 3 : 1;
          return 2;
        case 2:
          director->ReleaseToGameCamera();
          if (_gd.m_fieldEC != 0)
            if (CameraDirector* other = _globals.CameraDirectorOf(player ^ 1))
              if (_gd.m_fieldE8 != 0 || other->m_field320 != 0) other->ReleaseToGameCamera();
          break;
        case 3: {
          const float time = static_cast<signed char>(p[5]);
          if (p[6] & 2) director->InterpToGameCamera(time * 0.25f, CamDirInterpType{1});
          else director->InterpToGameCamera(time, CamDirInterpType{0});
          break;
        }
        default:
          frame->m_primState = 0;
          return 0;
      }
      frame->m_primState = 3;
      return 2;
    }
    case 3: {
      const int state = director->m_state;
      if (state == CameraDirector::kHold || state == CameraDirector::kHoldTimed) {
        frame->m_primState = 1;
        return 2;
      }
      if (state == CameraDirector::kIdle || state == CameraDirector::kGame) {
        frame->m_primState = 0;
        return 1;
      }
      return 2;
    }
    case 1:
      if (anim == 0) {
        frame->m_primState = 0;
        return 1;
      }
      director->LetterBox(!(p[4] & 0x80));
      if (ref) director->StartAnim(ref, (p[4] >> 3) & 1, (p[6] >> 5) & 1);
      else director->StartAnim(anim, (p[4] >> 3) & 1, (p[6] >> 5) & 1);
      frame->m_primState = 2;
      if (p[4] & 0x20) return 1;
      return 2;
    case 2:
      if (director->m_state == CameraDirector::kHold) {
        frame->m_primState = 0;
        return 1;
      }
      return 2;
    default:
      frame->m_primState = 0;
      return 0;
  }
}

// 80105780
// Idle for the ticks param `params[0]` holds (counting it down); objects let the simulator skip the ticks.
int cXObjectImpl::TryIdle(StackElem* frame, const unsigned char* params) {
  const int index = Be16(params, 0);
  if (index < 0 || index >= frame->m_numParams) {
    m_treeSim->SetError(8);
    Error(8);
    return -1;
  }
  short& ticks = frame->GetParams()[index];
  if (GetIdleStatus() == 0) {
    ticks = 0;
    SetIdleStatus(-1);
  }
  if (ticks < 0) ticks = 0;
  if (ticks == 0) return 1;
  if (!AllowIdleOptimization()) {
    --ticks;
    return 2;
  }
  SetIdleStatus(ticks - 1);
  return 2;
}

// 80105D70
// GC: a debugger breakpoint; carries on.
int cXObjectImpl::TryTreeBreak(StackElem*, const unsigned char*) { return 1; }

// 80105DD4
// Operands: the destination (data, scope), the range (data, scope): a random number below the range.
int cXObjectImpl::TryRandom(StackElem*, const unsigned char* params) {
  const int destData = Be16(params, 0), destScope = Be16(params, 1);
  const int rangeData = Be16(params, 2), rangeScope = Be16(params, 3);
  short* ref = nullptr;
  float* fref = nullptr;
  short range = 0;
  if (InterpValue(destScope, destData, &ref, &fref, nullptr) == -1) return -1;
  if (InterpValue(rangeScope, rangeData, nullptr, nullptr, &range) == -1) return -1;
  if (range == 0) {
    m_treeSim->SetError(4);
    Error(4);
    return -1;
  }
  const unsigned int r = GetNextRandomNumber();
  // PC: SIMMY_DET_RANDLOG=1 logs every draw (the determinism harness, platform/pc_det.cpp).
  static const bool randLog = std::getenv("SIMMY_DET_RANDLOG") != nullptr;
  if (randLog) {
    short tree = 0, node = 0;
    m_treeSim->GetCurrentNode(&tree, &node);
    std::fprintf(stderr, "[rand] frame %d obj %d tree %04x node %d range %d in-tick %d -> %u\n", _framecount, m_id,
                 static_cast<unsigned short>(tree), node, range, Globs_pSimulator ? Globs_pSimulator->m_tickObject : -1, r);
  }
  const short value = static_cast<short>(r - (r / static_cast<unsigned int>(static_cast<int>(range))) * range);
  if (ref) {
    InterpWriteValue(destScope, destData, &ref, value);
  } else if (fref) {
    *fref = static_cast<float>(value);
  } else {
    m_treeSim->SetError(0x13);
    Error(0x13);
    return -1;
  }
  return 1;
}

// 80109F54
// Operands: lhs data, rhs data, (signed), operator, lhs scope, rhs scope. Comparisons give their truth;
// assignments write the lhs (motives clamped to -100..100) and give true.
int cXObjectImpl::TryExpression(const unsigned char* params) {
  const int lhsData = Be16(params, 0), rhsData = Be16(params, 1);
  const int op = static_cast<signed char>(params[5]);
  const int lhsScope = static_cast<signed char>(params[6]), rhsScope = static_cast<signed char>(params[7]);
  short rhs = 0, lhs = 0;
  short* ref = nullptr;
  float* fref = nullptr;
  bool wrote = false;
  int result = 0;
  auto fail = [&](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  if (rhsScope == 7) {
    rhs = static_cast<short>(rhsData);
  } else if (InterpValue(rhsScope, rhsData, nullptr, nullptr, &rhs) == -1) {
    return -1;
  }
  if (lhsScope == 7) {
    lhs = static_cast<short>(lhsData);
  } else if (InterpValue(lhsScope, lhsData, &ref, &fref, &lhs) == -1) {
    return -1;
  }
  auto write = [&](short v) { InterpWriteValue(lhsScope, lhsData, &ref, v); };
  switch (op) {
    case 0: result = rhs < lhs; break;    // >
    case 1: result = lhs < rhs; break;    // <
    case 2: result = lhs == rhs; break;   // ==
    case 3:                               // +=
      if (ref) write(static_cast<short>(*ref + rhs));
      else if (fref) *fref += rhs;
      else return fail(0x13);
      wrote = true, result = 1;
      break;
    case 4:                               // -=
      if (ref) write(static_cast<short>(*ref - rhs));
      else if (fref) *fref -= rhs;
      else return fail(0x13);
      wrote = true, result = 1;
      break;
    case 5:                               // :=
      if (ref) write(rhs);
      else if (fref) *fref = rhs;
      else if (lhsScope == 0x2a) return 1;
      else return fail(0x13);
      wrote = true, result = 1;
      break;
    case 6:                               // *=
      if (ref) write(static_cast<short>(*ref * rhs));
      else if (fref) *fref *= rhs;
      else return fail(0x13);
      wrote = true, result = 1;
      break;
    case 7:                               // /=
      if (rhs == 0) return fail(0x26);
      if (ref) write(static_cast<short>(*ref / rhs));
      else if (fref) *fref /= rhs;
      else return fail(0x13);
      wrote = true, result = 1;
      break;
    case 8:                               // flag set?
      if (rhs > 16) return fail(0xd);
      result = (lhs >> (rhs - 1) & 1) != 0;
      break;
    case 9:                               // set flag
      if (rhs > 16) return fail(0xd);
      if (!ref) return fail(0x13);
      write(static_cast<short>(*ref | (1 << (rhs - 1))));
      wrote = true, result = 1;
      break;
    case 10:                              // clear flag
      if (rhs > 16) return fail(0xd);
      if (!ref) return fail(0x13);
      write(static_cast<short>(*ref & ~(1 << (rhs - 1))));
      result = 1;
      break;
    case 11:                              // ++ and <
      if (ref) write(static_cast<short>(*ref + 1));
      else if (fref) *fref += 1.0f;
      else return fail(0x13);
      lhs = ref ? *ref : static_cast<short>(*fref);
      wrote = true, result = lhs < rhs;
      break;
    case 12:                              // %=
      if (!ref) return fail(0x13);
      if (rhs == 0) return fail(0x26);
      if (*ref < 0) write(static_cast<short>(-*ref));
      if (rhs < 0) rhs = static_cast<short>(-rhs);
      write(static_cast<short>(*ref % rhs));
      wrote = true, result = 1;
      break;
    case 13:                              // &=
      if (!ref) return fail(0x13);
      write(static_cast<short>(*ref & rhs));
      wrote = true, result = 1;
      break;
    case 14: result = rhs <= lhs; break;  // >=
    case 15: result = lhs <= rhs; break;  // <=
    case 16: result = lhs != rhs; break;  // !=
    case 17:                              // -- and >
      if (ref) write(static_cast<short>(*ref - 1));
      else if (fref) *fref -= 1.0f;
      else return fail(0x13);
      lhs = ref ? *ref : static_cast<short>(*fref);
      wrote = true, result = rhs < lhs;
      break;
    default: return fail(1);
  }
  if (wrote) {
    if (lhsScope > 0xd && (lhsScope < 0x10 || lhsScope == 0x11) && fref) {
      if (*fref < -100.0f) *fref = -100.0f;
      if (*fref > 100.0f) *fref = 100.0f;
    }
    // Object data 0 (the graphic) was written (EdithVariableSet::WriteVar): my model (lhs scope 3) or the
    // stack object's (scope 4) restarts its animation. GC quirk kept: the flag is global (any data-0
    // write since the last assigning expression, e.g. by another primitive, counts).
    if (EdithVariableSet::s_bTaggedGraphicField == 1) {
      if (lhsScope == 4) {
        StackElem* frame = m_treeSim ? m_treeSim->GetCurElem() : nullptr;
        cXObject* so = frame ? GetObjectFromID(frame->m_stackObject) : nullptr;
        cXObjectImpl* o = so ? so->GetObjectImplementation() : nullptr;
        if (o) o->TagGraphicsField();  // PC: GC does not check
      } else if (lhsScope == 3) {
        TagGraphicsField();
      }
      EdithVariableSet::s_bTaggedGraphicField = 0;
    }
  }
  // PC dev trace (SIMMY_VM_TRACE): the values compared or written.
  if (TraceVM(m_id))
    std::printf("[vm] %s expr scope %x[%d] (%d) op %d scope %x[%d] (%d) -> %d\n", DebugName(), lhsScope, lhsData,
                ref ? *ref : lhs, op, rhsScope, rhsData, rhs, result);
  return result;
}

// 8010AA24
void cXObjectImpl::GetShortComboValue(signed char combo, short* scope, short* data) {
  const unsigned int c = static_cast<unsigned char>(combo);
  if (c == 0) {
    *scope = 3, *data = 0xb;
  } else if (c == 1) {
    *scope = 10, *data = 0;
  } else if (((c - 2) & 0xff) < 8) {
    *scope = 8, *data = static_cast<short>(c - 2);
  } else if (((c - 10) & 0xff) < 8) {
    *scope = 0x19, *data = static_cast<short>(c - 10);
  } else if (((c - 0x12) & 0xff) < 8) {
    *scope = 9, *data = static_cast<short>(c - 0x12);
  } else {
    *scope = 7, *data = static_cast<short>(static_cast<short>(combo) - 0x7e);
  }
}

// 8010AAA4 cXObjectImpl::InterpValue(short, short, short **, float **, short *): the overload with
// no extra out-parameter (`extra` defaults to null).

// 8010AAC8
// The scopes: 0 my attributes, 1 the stack object's, 3 my data, 4 the stack object's data, 6 the
// simulator's globals, 7 a literal, 8 my temps, 9 params, 0xA the stack object's id, 0xB temp[temp],
// 0xD the stack object's temps, 0xE / 0xF my / the stack object's motives, 0x10 / 0x14 slots, 0x11 the
// stack object's motive[temp], 0x12 / 0x13 my / the stack object's person data, 0x15 the stack object's
// definition, 0x16 the stack object's attribute[param], 0x17 my room, 0x19 locals, 0x1A / 0x2D tree
// constants, 0x1B the stack object's attribute[temp], 0x1E my person data[temp], 0x1F the stack object's
// person data[temp], 0x23 whether the stack object has an entry point, 0x2A nothing, 0x0C / 0x1C / 0x1D
// the check tree's ad copy (range, personality, min; autonomy only). Any other scope: error 9.
int cXObjectImpl::InterpValue(int scope, int data, short** ref, float** fref, short* value, short** extra) {
  short* dummy = nullptr;
  if (!ref) ref = &dummy;
  *ref = nullptr;
  if (fref) *fref = nullptr;
  if (extra) *extra = nullptr;
  short v = 0;
  StackElem* frame = m_treeSim ? m_treeSim->GetCurElem() : nullptr;
  auto fail = [&](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  auto stackObject = [&]() -> cXObjectImpl* {
    cXObject* o = frame ? GetObjectFromID(frame->m_stackObject) : nullptr;
    return o ? o->GetObjectImplementation() : nullptr;
  };
  auto person = [](cXObjectImpl* o) -> cXPersonImpl* {
    return o && o->GetType() == 2 ? static_cast<cXPersonImpl*>(o) : nullptr;
  };
  auto at = [&](short* p) {
    *ref = p;
    v = *p;
  };
  auto motive = [&](cXPersonImpl* p, int index) {
    v = static_cast<short>(std::lround(p->GetMotive(index)));
    if (fref) *fref = &p->m_motives.m_values[index];
    // GC: ObjectModule::MotiveAccessed (vtable 0x194; empty in the release build).
  };
  auto personData = [&](cXPersonImpl* p, int index) {
    at(&p->m_personData[index]);
    // GC: personality (2..7) and skill (9..0x12) data tell ObjectModule::PersonalityAccessed /
    // SkillAccessed (empty in the release build).
  };
  switch (scope) {
    case 0:
      if (data < 0 || data >= m_attrCount) return fail(6);
      at(&m_attrs[data]);
      break;
    case 1: {
      cXObjectImpl* o = stackObject();
      if (!o) return fail(10);
      if (data < 0 || data >= o->m_attrCount) return fail(6);
      at(&o->m_attrs[data]);
      break;
    }
    case 3:
      if (data < 0 || data >= EdithVariableSet::kCount) return fail(7);
      at(&m_data[data]);
      if (data == 0x19) ClearIdleStatus();
      break;
    case 4: {
      cXObjectImpl* o = stackObject();
      if (!o) return fail(10);
      if (data < 0 || data >= EdithVariableSet::kCount) return fail(7);
      at(&o->m_data[data]);
      if (data == 0x19) o->ClearIdleStatus();
      break;
    }
    case 6:
      if (static_cast<unsigned short>(data) >= cSimulatorImpl::kCount || !Globs_pSimulator) return fail(0x40);
      if (Globs_pSimulator->GetGlobalRef(static_cast<short>(data), ref)) v = **ref;
      else v = static_cast<short>(Globs_pSimulator->GetGlobal(static_cast<short>(data)));
      break;
    case 7: v = static_cast<short>(data); break;
    case 8:
      if (static_cast<unsigned short>(data) >= 8) return fail(2);
      at(&m_temps[data]);
      break;
    case 9:
      if (!frame || data < 0 || data >= frame->m_numParams) return fail(8);
      at(&frame->GetParams()[data]);
      break;
    case 10:
      if (data != 0 || !frame) return fail(0xf);
      at(&m_treeSim->GetNthElem(static_cast<short>(m_treeSim->GetStackSize() - 1))->m_stackObject);
      break;
    case 11:
      if (static_cast<unsigned short>(data) >= 8 || static_cast<unsigned short>(m_temps[data]) >= 8) return fail(2);
      at(&m_temps[m_temps[data]]);
      break;
    case 12:
    case 0x1c:
    case 0x1d:
      // The check tree's copy of the entry's ads (ObjTestSim::sCheckTreeModEntry) by motive: 0x0C its
      // range, 0x1C its personality ad, 0x1D its min (another motive than 0..15: error 0x1A). Outside
      // autonomy's ad check (cXPerson::sbAllowingCheckTreeAds clear): a scratch 0.
      if (!gAllowingCheckTreeAds) {
        tempDataPrim = 0;
        at(&tempDataPrim);
        break;
      }
      if (data < 0 || data >= 16) return fail(0x1a);
      at(scope == 12 ? &gCheckTreeModEntry.m_range[data]
                     : scope == 0x1c ? &gCheckTreeModEntry.m_personality[data] : &gCheckTreeModEntry.m_min[data]);
      break;
    case 0xd: {
      cXObjectImpl* o = stackObject();
      if (!o) return fail(10);
      if (static_cast<unsigned short>(data) >= 8) return fail(2);
      at(&o->m_temps[data]);
      break;
    }
    case 0xe: {
      if (static_cast<unsigned short>(data) >= 0x10) return fail(0x1a);
      cXPersonImpl* p = person(this);
      if (!p) return fail(0x1c);
      motive(p, data);
      break;
    }
    case 0xf: {
      if (static_cast<unsigned short>(data) >= 0x10) return fail(0x1a);
      cXObjectImpl* o = stackObject();
      if (!o) return fail(0x15);
      cXPersonImpl* p = person(o);
      if (!p) return fail(0x1c);
      motive(p, data);
      break;
    }
    case 0x11: {
      if (static_cast<unsigned short>(data) >= 8) return fail(2);
      const int index = m_temps[data];
      if (static_cast<unsigned short>(index) >= 0x10) return fail(0x1a);
      cXObjectImpl* o = stackObject();
      if (!o) return fail(0x15);
      cXPersonImpl* p = person(o);
      if (!p) return fail(0x1c);
      motive(p, index);
      break;
    }
    case 0x12: {
      cXPersonImpl* p = person(this);
      if (!p) return fail(0x1c);
      if (static_cast<unsigned short>(data) >= 0x50) return fail(0x21);
      personData(p, data);
      break;
    }
    case 0x13: {
      if (static_cast<unsigned short>(data) >= 0x50) return fail(0x21);
      cXObjectImpl* o = stackObject();
      if (!o) return fail(0x15);
      cXPersonImpl* p = person(o);
      if (!p) return fail(0x1c);
      personData(p, data);
      break;
    }
    case 0x15: {
      if (data >= 0x62) return fail(0x23);
      cXObjectImpl* o = stackObject();
      if (!o || !o->m_def) return fail(10);
      v = ERQuickdata::S16(o->m_def, static_cast<unsigned int>(data) * 2);
      break;
    }
    case 0x16: {
      if (!frame || data < 0 || data >= frame->m_numParams) return fail(8);
      const int index = frame->GetParams()[data];
      cXObjectImpl* o = stackObject();
      if (!o) return fail(10);
      if (index < 0 || index >= o->m_attrCount) return fail(6);
      at(&o->m_attrs[index]);
      break;
    }
    case 0x17: {
      // The room whose id is in temp 0 (0xFFFB: none, out of the world): 0 its ambient light x 100,
      // 1 outside, 2 it is there, 3 its area, 4 its light score (outside: the room manager's outdoor
      // score). A room the manager does not have: error 0x28; any other data: 0x29.
      const short id = m_temps[0];
      RoomManager* manager = RoomManager::GetRoomManager();
      Room* room = manager ? manager->GetRoom(static_cast<unsigned short>(id)) : nullptr;
      if (data < 0 || data > 4) return fail(0x29);
      if (id == -5 && data != 4) {
        v = static_cast<short>(data == 1 ? 1 : 0);
        break;
      }
      if (!room) return fail(0x28);
      switch (data) {
        case 0: v = static_cast<short>(static_cast<int>(room->GetAmbientLight() * 100.0)); break;
        case 1: v = static_cast<short>(room->IsOutside() ? 1 : 0); break;
        case 2: v = 1; break;
        case 3: v = static_cast<short>(room->GetArea()); break;
        default: v = static_cast<short>(static_cast<int>(room->IsOutside() ? manager->m_outdoorScore : room->m_field7C)); break;
      }
      break;
    }
    case 0x19:
      if (!frame || data < 0 || data >= frame->m_numLocals) return fail(8);
      at(&frame->GetLocals()[data]);
      break;
    case 0x1a:
    case 0x2d: {
      short id = 0;
      int index = 0;
      if (scope == 0x1a) {
        // XObjLang::GetConstantsID (801523AC): bits 13-15 the file (0 private, 1 semi-global, 2 global),
        // bits 7-12 the constants' id offset, bits 0-6 the index (0x7F: temp 0).
        const unsigned int u = static_cast<unsigned short>(data);
        const unsigned int kind = u >> 13 & 7;
        if (kind > 2) return fail(0x2f);
        id = static_cast<short>((kind == 0 ? 0x1000 : kind == 1 ? 0x2000 : 0x100) + (u >> 7 & 0x3f));
        index = static_cast<int>(u & 0x7f);
        if (index == 0x7f) index = m_temps[0];
      } else {
        id = m_temps[0];
        index = data;
        if (data == 0x7f) index = m_temps[1];
      }
      const unsigned char* values = nullptr;
      int count = 0;
      Behavior* b = frame ? frame->m_behavior : nullptr;
      if (!b || !b->GetConstants(id, &values, &count) || index < 0 || index >= count) return fail(0x2f);
      // QuickDataBehaviorConstants: 8 bytes a constant (the value, padding, its label).
      v = static_cast<short>(simmy::be16(values + index * 8));
      break;
    }
    case 0x1b: {
      if (static_cast<unsigned short>(data) >= 8) return fail(2);
      cXObjectImpl* o = stackObject();
      if (!o) return fail(10);
      const int index = m_temps[data];
      if (index < 0 || index >= o->m_attrCount) return fail(0x32);
      at(&o->m_attrs[index]);
      break;
    }
    case 0x1e:
    case 0x1f: {
      cXObjectImpl* o = scope == 0x1e ? this : stackObject();
      if (!o) return fail(0x15);
      cXPersonImpl* p = person(o);
      if (!p) return fail(0x1c);
      if (static_cast<unsigned short>(data) >= 8) return fail(2);
      const int index = m_temps[data];
      if (static_cast<unsigned short>(index) >= 0x50) return fail(0x21);
      personData(p, index);
      break;
    }
    case 0x23: {
      // Whether the stack object's function table has entry point `data`.
      cXObjectImpl* o = stackObject();
      if (!o) return fail(10);
      v = o->GetTreeID(data) != 0 ? 1 : 0;
      break;
    }
    case 0x2a: v = 0; break;
    case 0x10:
    case 0x14: {
      // The object in slot `data` of the stack object (0x10) or mine (0x14). GC: the container's
      // ObjectSlot (vtable 0x2BC GetSlot) holds the contained object's id (+4); PC: the object whose
      // container (data 2) and slot (data 3) say so (TrySnap sets them).
      cXObjectImpl* owner = scope == 0x10 ? stackObject() : this;
      if (!owner) return fail(0x15);
      cXObjectImpl* contained = owner->GetContainedObject(data);
      v = contained ? contained->m_id : 0;
      tempDataPrim = v;
      at(&tempDataPrim);
      break;
    }
    case 0x18:
      // The stack object names a neighbour (by id): NeighborhoodImpl::GetNeighborData.
      v = static_cast<short>(Globs_pNeighborhood->GetNeighborData(frame ? frame->m_stackObject : 0,
                                                                  static_cast<short>(data), ref));
      break;
    case 0x20:
    case 0x26: {
      // The stack object's neighbour's person data (0x20) or its object definition's field (0x26).
      Neighbor* n = Globs_pNeighborhood->FindNeighborByID(frame ? frame->m_stackObject : 0);
      if (!n) return fail(0x38);
      if (scope == 0x20) {
        if (static_cast<unsigned short>(data) >= 0x50) return fail(0x21);
        v = n->m_pers[data];
      } else {
        if (static_cast<unsigned short>(data) >= 0x62) return fail(0x23);
        if (!n->m_selector || !n->m_selector->m_def) return fail(0x38);
        v = ERQuickdata::S16(n->m_selector->m_def, static_cast<unsigned int>(data) * 2);
      }
      break;
    }
    case 0x21: {
      // Career temp 0 (-1: level 0), level temp 1: CareersImpl::GetBehCareerData field `data`.
      CareersImpl* careers = static_cast<CareersImpl*>(Globs_pCareers);
      const Career* c = careers ? careers->GetCareerByID(m_temps[0]) : nullptr;
      if (!c) return fail(0x3a);
      const int level = m_temps[0] == -1 ? 0 : m_temps[1];
      if (!careers->GetBehCareerData(c, level, data, &v)) return fail(0x3b);
      break;
    }
    case 0x22:
      // The neighbourhood's variables 1..0x23.
      if (static_cast<unsigned int>(data - 1) >= 0x23) return fail(0x3d);
      at(Globs_pNeighborhood->GetNeighborhoodVarRef(data));
      break;
    case 0x24:
    case 0x25: {
      // My (0x24) or the stack object's (0x25) type attribute `data` (the object type's 'TATT' block).
      cXObjectImpl* o = scope == 0x24 ? this : stackObject();
      if (!o) return fail(10);
      short* attrs = nullptr;
      const int count = TypeAttributesOf(o, &attrs);
      if (data < 0 || data >= count) return fail(0x41);
      at(&attrs[data]);
      break;
    }
    case 0x27: v = 0; break;
    case 0x28:
    case 0x29: {
      // Whether family 1 has unlocked lot (0x28, unlock kind 2) or kind-4 item (0x29) `data`.
      GoalUnlock unlocks = FamilyGoalUnlock(1);
      const int kind = scope == 0x28 ? 2 : 4;
      if (!unlocks.m_bits || data < 0 || data >= unlocks.GetUnlockCount(IGoalUnlock__UnlockType{kind}))
        return fail(0x3d);
      v = unlocks.IsUnlocked(IGoalUnlock__UnlockType{kind}, static_cast<short>(data)) != 0 ? 1 : 0;
      break;
    }
    case 0x2b:
    case 0x2c: {
      // The stack object person's look: 9 (0x2B) / 0 (0x2C) with a sim description.
      cXObjectImpl* o = stackObject();
      if (!o) return fail(0x15);
      cXPersonImpl* p = person(o);
      if (!p || !p->GetSimDescription()) return fail(0x1c);
      v = scope == 0x2b ? 9 : 0;
      break;
    }
    case 0x2e: {
      // The stack object's neighbour's family's statistic `data` (PlayerFamilyImpl::GetStatRef).
      Neighbor* n = Globs_pNeighborhood->FindNeighborByID(frame ? frame->m_stackObject : 0);
      if (!n) return fail(0x38);
      FamilyImpl* f = n->GetFamily();
      if (f && f->GetStatRef(data, ref) && *ref) v = **ref;
      break;
    }
    case 0x2f: {
      // The stack object person's queued interaction data / 2: even data its stack object's id, odd
      // its tree table entry; -1 for none.
      cXObjectImpl* o = stackObject();
      cXPersonImpl* p = person(o);
      if (!p) return fail(0x1c);
      *ref = nullptr;
      if (static_cast<unsigned short>(data) >= 4) return fail(7);
      Interaction* action = p->GetIndAction(data / 2, true);
      if (!action) {
        v = -1;
      } else if (data & 1) {
        v = static_cast<short>(action->m_index);
      } else {
        cXObject* so = action->GetStackObject();
        v = so ? static_cast<short>(so->GetID()) : 0;
      }
      break;
    }
    default:
      // No such scope (2, 5, above 0x2F): error 9.
      return fail(9);
  }
  if (value) *value = v;
  return 1;
}

// 8010D160
// Writes through the reference InterpValue gave; my and the stack object's data go through WriteVar.
int cXObjectImpl::InterpWriteValue(int scope, int data, short** ref, short value) {
  if (scope == 3 || scope == 4) {
    cXObjectImpl* o = this;
    if (scope == 4) {
      StackElem* frame = m_treeSim ? m_treeSim->GetCurElem() : nullptr;
      cXObject* so = frame ? GetObjectFromID(frame->m_stackObject) : nullptr;
      o = so ? so->GetObjectImplementation() : nullptr;
      if (!o) {
        m_treeSim->SetError(10);
        Error(10);
        return -1;
      }
    }
    if (data < 0 || data >= EdithVariableSet::kCount) {
      m_treeSim->SetError(7);
      Error(7);
      return -1;
    }
    o->m_data.WriteVar(data, value);
    *ref = &o->m_data[data];
    return 1;
  }
  if (!*ref) {
    m_treeSim->SetError(9);
    Error(9);
    return -1;
  }
  **ref = value;
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// ObjectModuleImpl's sim flags (game/objectmodule.cpp)

namespace {
unsigned int* SimFlagWord(ObjectModuleImpl* m, int id) {
  if (id < 1 || id > 0x7fff) return nullptr;
  if (m->m_simFlags.size() < static_cast<size_t>(id)) m->m_simFlags.resize(static_cast<size_t>(id), 0xffffu);
  return &m->m_simFlags[static_cast<size_t>(id) - 1];
}
}  // namespace

// 800F9578
void ObjectModuleImpl::SetSimFlag(int id, ObjectModule__SimFlag flag, bool on) {
  if (unsigned int* w = SimFlagWord(this, id)) {
    *w &= ~(static_cast<unsigned int>(static_cast<int>(flag)) << 16);
    if (on) *w |= static_cast<unsigned int>(static_cast<int>(flag)) << 16;
  }
}
// 800F95B0
int ObjectModuleImpl::GetSimFlag(int id, ObjectModule__SimFlag flag) {
  unsigned int* w = SimFlagWord(this, id);
  return w && (*w & static_cast<unsigned int>(static_cast<int>(flag)) << 16) ? 1 : 0;
}
// 800F95D8
int ObjectModuleImpl::GetIdleStatus(int id) {
  unsigned int* w = SimFlagWord(this, id);
  return w ? static_cast<short>(*w & 0xffff) : -1;
}
// 800F95F0
void ObjectModuleImpl::SetIdleStatus(int id, int ticks) {
  if (unsigned int* w = SimFlagWord(this, id)) *w = (*w & 0xffff0000u) | (static_cast<unsigned int>(ticks) & 0xffff);
}
// 800F9820
int ObjectModuleImpl::GetNumObjects() { return static_cast<int>(m_byId.size()); }

namespace {
// ObjectModuleImpl +0x2034: the objects that lock buy and build mode (PC: kept here, not in the module).
std::vector<cXObjectImpl*> s_buyBuildLocks;
}  // namespace

// 800F7C6C
// GC: a change of state goes to GlobalDispatch 0x104 (the pause menu's tabs; PC: they ask
// IsBuyAndBuildDisabled).
void ObjectModuleImpl::DisableBuyAndBuild(cXObject* object) {
  s_buyBuildLocks.push_back(object ? object->GetObjectImplementation() : nullptr);
  std::printf("[story] buy and build locked\n");
}
// 800F7E34
void ObjectModuleImpl::EnableBuyAndBuild(cXObject* object) {
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  for (auto it = s_buyBuildLocks.begin(); it != s_buyBuildLocks.end(); ++it)
    if (*it == impl) {
      s_buyBuildLocks.erase(it);
      break;
    }
  if (s_buyBuildLocks.empty()) std::printf("[story] buy and build unlocked\n");
}
// 800F9A10
bool ObjectModuleImpl::IsBuyAndBuildDisabled() { return !s_buyBuildLocks.empty(); }

// 8010D5B0
// Puts this object (a person) at operand +0's spot of the stack object, by operand +2: 0 the routing
// slot param N names, 3 routing slot N, 4 global routing slot N, 1 the object's own location, 2 the tile in
// front of it. Routing slots that snap to a direction (0x1000) also turn it to the slot's first
// direction from the object's facing; otherwise the facing is kept; a slot with a snap target puts it
// in that slot of the object (a seat: data 2 the container, 3 the slot). Operand +4: 1 the animator keeps
// the displacement (SetAnimDisplacements, empty on GC), 2 a person in the way is asked to move.
// GC: the place is checked (cXObjectImpl::TrySnap(FTilePt&, ...), CanPlace: data 0x34 / 0x36 the
// placement error and the object in the way); PC: a person's free place is checked with
// PersonCanStandAt (objects and walls), containers are not.
int cXObjectImpl::TrySnap(StackElem* frame, const unsigned char* params) {
  cXObjectImpl* target = static_cast<cXObjectImpl*>(GetObjectFromID(frame->m_stackObject));
  if (target) target = target->GetObjectImplementation();
  if (!target) {
    m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  const int index = static_cast<short>(simmy::be16(params));
  const int kind = static_cast<short>(simmy::be16(params + 2));
  int pos[2] = {target->m_location[0], target->m_location[1]};
  int dir = GetData(1);
  cXObjectImpl* container = nullptr;
  int containerSlot = 0;
  if (kind == 1) {
    // the object's own location
  } else if (kind == 2) {
    static const int step[8][2] = {{-16, 0}, {-16, 16}, {0, 16}, {16, 16}, {16, 0}, {16, -16}, {0, -16}, {-16, -16}};
    const int d = target->GetData(1) & 7;
    pos[0] += step[d][0];
    pos[1] += step[d][1];
  } else if (kind == 0 || kind == 3 || kind == 4) {
    int n = index;
    if (kind == 0) {
      if (index < 0 || index >= frame->m_numParams) {
        m_treeSim->SetError(8);
        Error(8);
        return -1;
      }
      n = frame->GetParams()[index];
    }
    const std::vector<RoutingSlot>& slots = kind == 4 ? GlobalRoutingSlots() : ObjectRoutingSlots(target);
    if (n < 0 || n >= static_cast<int>(slots.size())) {
      m_treeSim->SetError(0x1b);
      Error(0x1b);
      return -1;
    }
    const RoutingSlot& slot = slots[static_cast<size_t>(n)];
    if (slot.m_flags & RoutingSlot::kSnapToDirection) {
      int first = 0;
      while (first < 7 && !((slot.m_flags >> first) & 1)) ++first;
      if (!((slot.m_flags >> first) & 1)) first = 0;
      dir = (target->GetData(1) + first) & 7;
    }
    if (slot.m_snapTarget >= 0) {
      // Into the object's slot `snap target` (a seat): at the object's location, contained by it.
      container = target;
      containerSlot = slot.m_snapTarget;
    } else {
      RoutingSlotPosition(target, slot, pos);
    }
  } else {
    m_treeSim->SetError(0x24);
    Error(0x24);
    return -1;
  }
  // TrySnap(FTilePt&, ...): a free place must have room (CanPlace, vtable 0x134), else the snap is false
  // (GC: also no wall between the old and new tiles unless slot flag 0x800, data 0x34 2 for an object
  // in the way, 0x36 its id, people in the way asked to move with operand bit 2).
  if (!container && GetType() == 2 && !PersonCanStandAt(this, target, pos)) {
    m_data.WriteVar(0x34, 2);
    m_data.WriteVar(0x36, 0);
    return 0;
  }
  // CanPlace into a container's slot (cXObjectImpl::TestAndPlace 800DD6E8, the slot branch): a slot that
  // holds another object refuses (placement error 0xB); e.g. two sims getting into the same hot tub part
  // at once: the second's snap is false and it gets out ("SG_I_Get In" node 9's false branch).
  if (container) {
    cXObjectImpl* occupant = container->GetContainedObject(containerSlot);
    if (occupant && occupant != this) {
      gPlacementError = 0xb;
      return 0;
    }
  }
  const int was[2] = {m_location[0], m_location[1]};
  HierUnlink(this);  // Place: TestAndPlace out of the world
  InfluenceMapObjectLeaving(this);  // SetLocation
  m_location[0] = pos[0];
  m_location[1] = pos[1];
  m_data.WriteVar(1, static_cast<short>(dir));
  // cXPersonImpl::Place (vtable 0x13C): the container's id and slot (data 2, 3; GC: PlacementSpec).
  m_data.WriteVar(2, static_cast<short>(container ? container->m_id : 0));
  m_data.WriteVar(3, static_cast<short>(container ? containerSlot : -1));
  {
    PlaceSpec spec(pos, m_level, container, containerSlot);  // TestAndPlace: the new site
    HierLink(this, spec);
  }
  m_data.WriteVar(0x34, 0);
  UpdateObjectRoomID(this);  // Place -> SetLocation: ResolveRoomID, data 0x1D
  InfluenceMapObjectArrived(this);
  PassiveInfluenceLocationChanged(this, was[0], was[1]);
  if (GetType() == 2)
    if (auto* person = dynamic_cast<cXPersonImpl*>(this)) person->UpdateCurrentRoom();  // cXPersonImpl::Place
  // A person's animator takes the new place (TrySnap(FTilePt&, ...) 8010E084: SnapToGrid and the
  // person's direction), unless the container's object slot has flag 2.
  const int slotFlags = container ? ObjectSlotFlags(container, containerSlot) : 0;
  if (GetType() == 2 && !(slotFlags > 0 && (slotFlags & 2))) {
    if (SAnimator2* a = static_cast<cXPersonImpl*>(this)->m_animator) {
      a->SnapToGrid();
      a->setAnimationDirectionToPersonDirection();
    }
  }
  return 1;
}

// 80106270
// Is the object (operand +0 data, +2 scope) ...: kind (+4) 0 a sim a player steers (some player's
// selected sim whose interactor is not autonomous), 1 an object, 2 a person, 3 a neighbour (by
// neighbour id); 4 counts the person's relationships of type temp 0 into temp 0 (1..9, else error
// 0x14).
int cXObjectImpl::TryCheckObject(StackElem* frame, const unsigned char* params) {
  (void)frame;
  short id = -1;
  if (InterpValue(static_cast<short>(simmy::be16(params + 2)), static_cast<short>(simmy::be16(params)), nullptr, nullptr, &id) == -1)
    return -1;
  const int kind = static_cast<short>(simmy::be16(params + 4));
  switch (kind) {
    case 0: {
      cXObject* o = GetObjectFromID(id);
      cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
      if (!impl) return 0;
      for (int player = 0; player < 2; ++player) {
        cXPerson* s = _globals.GetSelectedPerson(player);
        if (!s || s->AsObject() != impl) continue;
        InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player);
        if (d && d->m_autonomous == 0) return 1;
      }
      return 0;
    }
    case 1: return GetObjectFromID(id) ? 1 : 0;
    case 2: {
      cXObject* o = GetObjectFromID(id);
      return o && o->GetObjectImplementation() && o->GetObjectImplementation()->GetType() == 2 ? 1 : 0;
    }
    case 3: return Globs_pNeighborhood && Globs_pNeighborhood->FindNeighborByID(id) ? 1 : 0;
    case 4: {
      const unsigned short type = static_cast<unsigned short>(m_temps[0]);
      if (type - 1u >= 9u) {
        m_treeSim->SetError(0x14);
        Error(0x14);
        return -1;
      }
      short count = 0;
      cXObject* o = GetObjectFromID(id);
      auto* p = o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
      if (p && p->m_neighbor) count = static_cast<short>(p->m_neighbor->CountRelationshipsOfType(type));
      m_temps[0] = count;
      return 1;
    }
    default: return 0;
  }
}

// 8010816C
// Wakes the stack object: if its current node idles (primitive 0, its ticks in the param operand +0
// names) or waits for input (0x11), the wait ends (and an idle's skipped ticks: SetIdleStatus 0); a
// person's idle-animate (0x40) count goes to 0.
int cXObjectImpl::TryNotifyStackObject(StackElem* frame, const unsigned char*) {
  cXObject* o = GetObjectFromID(frame->m_stackObject);
  cXObjectImpl* target = o ? o->GetObjectImplementation() : nullptr;
  if (!target) {
    m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  StackElem* cur = target->m_treeSim && target->m_treeSim->GetStackSize() ? target->m_treeSim->GetCurElem() : nullptr;
  const unsigned char* node = nullptr;
  if (!cur || !cur->m_behavior || !cur->m_behavior->GetNode(static_cast<short>(cur->GetTreeID()), cur->m_node, &node))
    return 1;
  const int op = bhav::Opcode(node);
  if (op == 0 || op == 0x11) {
    const int index = bhav::Operand16(node, 0);
    if (index < 0 || index >= cur->m_numParams) {
      m_treeSim->SetError(8);
      Error(8);
      return -1;
    }
    cur->GetParams()[index] = 0;
    if (op == 0) target->SetIdleStatus(0);
  } else if (op == 0x40) {
    // An idle-animate (TryIdleAnimate): its count ends (the next tick finishes it); not a person:
    // error 0x1C.
    cXPersonImpl* person = target->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(target) : nullptr;
    if (!person) {
      m_treeSim->SetError(0x1c);
      Error(0x1c);
      return -1;
    }
    static int logged = 0;
    if (logged++ < 10) {
      const std::string me = DebugName();
      std::printf("[vm] %s ends %s's idle-animate (count %d)\n", me.c_str(), person->DebugName(), person->m_idleAnimateCount);
    }
    person->m_idleAnimateCount = 0;
  }
  return 1;
}

namespace {
// The FTilePt one tile from `pos` in direction `dir` (eighths).
void StepToward(int pos[2], int dir) {
  static const int step[8][2] = {{-16, 0}, {-16, 16}, {0, 16}, {16, 16}, {16, 0}, {16, -16}, {0, -16}, {-16, -16}};
  pos[0] += step[dir & 7][0];
  pos[1] += step[dir & 7][1];
}

// cXObjectImpl::Place (vtable 0x13C), PC: `object` at `pos` on `level`, in `container`'s slot `slot`
// (data 2 / 3) or on its tile: it leaves its site and takes the new one in the slot hierarchy
// (TestAndPlace's two halves: the tile chain or the container's slot); then cXObjectImpl::SimObjectPlaced
// (800EA6B0): it and what it holds are no longer carried (cursor flags 0x40 / 0x80) and take their
// place. GC: the slot's position and height, the room's impact and lighting.
void PlaceObject(cXObjectImpl* object, const int pos[2], int level, cXObjectImpl* container, int slot = 0) {
  HierUnlink(object);
  const int was[2] = {object->m_location[0], object->m_location[1]};
  InfluenceMapObjectLeaving(object);  // SetLocation
  object->m_location[0] = pos[0];
  object->m_location[1] = pos[1];
  object->m_level = level;
  object->m_data.WriteVar(2, static_cast<short>(container ? container->m_id : 0));
  object->m_data.WriteVar(3, static_cast<short>(container ? slot : -1));
  if (container || (pos[0] >= 0 && pos[1] >= 0)) {
    PlaceSpec spec(pos, level, container, slot);
    HierLink(object, spec);
  }
  UpdateObjectRoomID(object);  // GC Place -> SetLocation: ResolveRoomID, data 0x1D
  InfluenceMapObjectArrived(object);
  PassiveInfluenceLocationChanged(object, was[0], was[1]);
  if (object->GetType() == 2) {
    if (object->m_instance) object->m_instance->SetObjOrient();
    return;
  }
  int depth = 0;
  for (cXObjectImpl* o = object; o && depth < 8; o = o->GetContainedObject(0), ++depth) {
    if (!o->m_instance) continue;
    o->m_instance->SetCursFlags(o->m_instance->GetCursFlags() & ~0xc0u);
    o->m_instance->SetObjOrient();
  }
}
}  // namespace

// 800EB230
cXObjectImpl* cXObjectImpl::GetContainedObject(int slot) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  const int count = module ? module->GetNumObjects() : 0;
  for (int id = 1; id <= count; ++id) {
    cXObject* x = module->GetObjectFromID(id);
    cXObjectImpl* o = x ? x->GetObjectImplementation() : nullptr;
    if (o && o != this && o->m_data[2] == m_id && o->m_data[3] == slot) return o;
  }
  return nullptr;
}

// 80105AFC
// Picks up the stack object: it goes into my slot 0 (my hand; GC: if CanPlace allows), and it and what
// it holds are drawn carried (cursor flags 0x40 / 0x80; operand +0 == 1: also 0x100, held turned with
// the hand). False: no stack object.
int cXObjectImpl::TryGrab(StackElem* frame, const unsigned char* params) {
  cXObject* x = GetObjectFromID(frame->m_stackObject);
  cXObjectImpl* held = x ? x->GetObjectImplementation() : nullptr;
  if (!held) return 0;
  // GC: Globs::bStupidMode is off while it places; CanPlace (vtable 0x134) into me, slot 0.
  const int pos[2] = {m_location[0], m_location[1]};
  PlaceObject(held, pos, m_level, this);
  ISimInstance* model = held->m_instance;
  if (m_instance && model) {
    unsigned int flags = model->GetCursFlags() | 0xc0;
    if (params[0] < 2 && (params[0] & 1)) flags |= 0x100;
    model->SetCursFlags(flags);
    model->SetObjOrient();
    int depth = 0;
    for (cXObjectImpl* o = held->GetContainedObject(0); o && depth < 8; o = o->GetContainedObject(0), ++depth) {
      if (!o->m_instance) continue;
      unsigned int f = model->GetCursFlags() | 0xc0;
      if (params[0] < 2 && (params[0] & 1)) f |= 0x100;
      o->m_instance->SetCursFlags(f);
      o->m_instance->SetObjOrient();
    }
  }
  return 1;
}

namespace {
// PC stand-in for cXObjectImpl::CanPlace (vtable 0x134) of an object on a tile: on the lot, not a
// blocked tile (flag 0x10), no other object the tile's, no wall across it (a diagonal).
bool ObjectCanPlaceAt(cXObjectImpl* object, const CTilePt& at) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  ObjectModuleImpl* module = Globs_pObjectModule;
  CTilePt t = at;
  if (!w || !module || w->OutOfBounds(t) || (w->GetFlags(t) & 0x10)) return false;
  const int id = module->GetTileObjectID(t);
  return id == 0 || id == object->m_id;
}

// 800E0F40 cXObjectImpl::FindGoodLocation (FindGoodLocationParams: +4 the start, +0xC its level, +0x10
// a direction (-1 none), +0x14 the same room only, +0x18 no other object on the tile): a spiral out
// from the start tile (kept a tile inside the lot) for a tile CanPlace takes; past 0x40 tiles out the
// lot's fallback tile (4, 14). PC: the spiral is the same for a direction.
bool FindGoodLocationFor(cXObjectImpl* object, const int start[2], int level, bool sameRoom, bool emptyTile, int out[2]) {
  cFixedWorldImpl* w = Globs_pFixedWorld;
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!w || !module) return false;
  int x = std::max(1, std::min(w->m_xSize - 2, start[1] >> 4));
  int y = std::max(1, std::min(w->m_ySize - 2, start[0] >> 4));
  CTilePt origin(x, y, level);
  const unsigned short room = w->GetRoom(origin);
  // the GC's no-direction search walks a square spiral; PC: ring by ring, the same tiles in order of
  // distance.
  for (int r = 0; r <= 0x40; ++r) {
    for (int dy = -r; dy <= r; ++dy) {
      for (int dx = -r; dx <= r; ++dx) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        CTilePt t(x + dx, y + dy, level);
        if (w->OutOfBounds(t) || w->GetWall(t).HasDiagonal()) continue;
        if ((emptyTile && module->GetTileObjectID(t) != 0) || (sameRoom && w->GetRoom(t) != room)) continue;
        if (!ObjectCanPlaceAt(object, t)) continue;
        out[0] = t.GetY() * 16 + 8;
        out[1] = t.GetX() * 16 + 8;
        return true;
      }
    }
  }
  out[0] = 14 * 16 + 8;
  out[1] = 4 * 16 + 8;
  return true;
}
}  // namespace

// PC: FindGoodLocationFor for other callers (FlashPiMenu's go-here object: the person's
// FindGoodLocation, cXObject vtable 0x194).
bool ObjFindGoodLocation(cXObjectImpl* object, const int start[2], int level, bool sameRoom, bool emptyTile, int out[2]) {
  return FindGoodLocationFor(object, start, level, sameRoom, emptyTile, out);
}

// 800FADF4
// Puts the stack object somewhere it fits: operand +0 how (0 near the reference, 1 out of the world, 2
// at the reference turned to face from its front-left, 3 / 4 near the reference along its facing (4: a
// quarter turn on), 5 next to its multi-tile partner, 6 a random tile of the lot), +2 flags (1 the
// reference is the object in local +1, else 16,16; 2 tiles with an object allowed, 4 (random) not on
// flag-0x20 tiles). True: placed. GC: the outputs (the action, place, level, object) go to callers that
// ignore them; CanPlace is the real placement test (PC: ObjectCanPlaceAt).
int cXObjectImpl::TryFindGoodLocation(StackElem* frame, const unsigned char* params) {
  cXObject* x = GetObjectFromID(frame->m_stackObject);
  cXObjectImpl* object = x ? x->GetObjectImplementation() : nullptr;
  auto fail = [this](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  if (!object) return fail(0x15);
  cXObjectImpl* ref = nullptr;
  int pos[2] = {0x10, 0x10};
  int level = 1;
  const unsigned char flags = params[2];
  if (flags & 1) {
    if (params[1] >= frame->m_numLocals) return fail(0x34);
    cXObject* r = GetObjectFromID(frame->GetLocals()[params[1]]);
    ref = r ? r->GetObjectImplementation() : nullptr;
    if (!ref) return fail(0x17);
    pos[0] = ref->m_location[0], pos[1] = ref->m_location[1];
    level = ref->m_level;
  }
  CTilePt at(pos[1] >> 4, pos[0] >> 4, level);
  switch (params[0]) {
    case 1:
      // Pickup (vtable 0x12C: the "picked up" tree, out of the world, its room data 0x1D with it); a
      // player's selected sim picked up closes that player's object menu (its TreeSim GetPerson,
      // HUDTarget::CloseMenu). GC: first a dynamic multi-tile object leaves its group (PC: not kept).
      ObjPickup(object);
      if (object->GetType() == 2 && _gd.m_hudTarget)
        for (int player = 0; player < 2; ++player) {
          cXPerson* selected = _globals.GetSelectedPerson(player);
          if (selected && selected->AsObject() == static_cast<cXObject*>(object)) {
            _gd.m_hudTarget->CloseMenu(player);
            break;
          }
        }
      return 1;
    case 2: {
      if (!ref) ref = this;
      pos[0] = ref->m_location[0], pos[1] = ref->m_location[1];
      level = ref->m_level;
      const int dir = (ref->GetData(1) + 5) & 7;
      object->m_data.WriteVar(1, static_cast<short>(dir & ~1));  // GC: Turn by quarter turns
      if (object->GetData(1) != dir) return fail(0x53);
      CTilePt t(pos[1] >> 4, pos[0] >> 4, level);
      if (!ObjectCanPlaceAt(object, t)) return 0;
      PlaceObject(object, pos, level, nullptr);
      return 1;
    }
    case 3: case 4: case 0: {
      if (params[0] != 0 && !ref) return fail(0x3e);
      int out[2];
      if (!FindGoodLocationFor(object, pos, level, true, !(flags & 2), out)) return 0;
      CTilePt t(out[1] >> 4, out[0] >> 4, level);
      if (!ObjectCanPlaceAt(object, t)) return 0;
      PlaceObject(object, out, level, nullptr);
      return 1;
    }
    case 5: {
      // Next to the reference as the two groups' interaction leaders' offsets say: the object turned to
      // the reference's direction (by quarter turns), placed at the reference leader's location plus the
      // offsets' difference turned by that turn, on the reference leader's level.
      if (!ref) return fail(0x3e);
      const int turn = ((ref->GetData(1) & 7) - object->GetData(1) + 8) & 7;
      if (turn > 0) object->m_data.WriteVar(1, static_cast<short>((object->GetData(1) + turn) & 7));  // GC: Turn(turn / 2)
      cXObjectImpl* refLead = ref->GetInteractionLeader();
      cXObjectImpl* objLead = object->GetInteractionLeader();
      if (!refLead || !objLead) return fail(0x3e);
      const int lvl = refLead->m_level;
      const short refSub = objdef::SubIndex(refLead->m_selector->m_def), objSub = objdef::SubIndex(objLead->m_selector->m_def);
      const int dx = (objSub >> 8) - (refSub >> 8), dy = (objSub & 0xff) - (refSub & 0xff);
      int p[2] = {refLead->m_location[0], refLead->m_location[1]};  // [0] y, [1] x
      switch (turn) {
        case 0: p[0] += dy * 16, p[1] += dx * 16; break;
        case 2: p[0] += dx * 16, p[1] -= dy * 16; break;
        case 4: p[0] -= dy * 16, p[1] -= dx * 16; break;
        case 6: p[0] -= dx * 16, p[1] += dy * 16; break;
        default: break;
      }
      CTilePt t(p[1] >> 4, p[0] >> 4, lvl);
      if (!ObjectCanPlaceAt(object, t)) return 0;
      PlaceObject(object, p, lvl, nullptr);  // GC: Place, then vtable 0x1C4
      return 1;
    }
    case 6: {
      cFixedWorldImpl* w = Globs_pFixedWorld;
      ObjectModuleImpl* module = Globs_pObjectModule;
      for (int i = 0; w && module && i < 10; ++i) {
        // The game's random numbers (GetNextRandomNumber: the simulator's sequence during a tick), x first.
        const int tx = static_cast<int>(GetNextRandomNumber() % static_cast<unsigned int>(std::max(1, w->m_xSize)));
        const int ty = static_cast<int>(GetNextRandomNumber() % static_cast<unsigned int>(std::max(1, w->m_ySize)));
        CTilePt t(tx, ty, 1);
        if ((flags & 4) && (w->GetFlags(t) & 0x20)) continue;
        if (!(flags & 2) && module->GetTileObjectID(t) != 0) continue;
        if (!ObjectCanPlaceAt(object, t)) continue;
        const int p[2] = {ty * 16 + 8, tx * 16 + 8};
        PlaceObject(object, p, 1, nullptr);
        return 1;
      }
      return 0;
    }
    default: return 0;
  }
  (void)at;
}

// 8010D3C8
// Puts down what is in my slot 0 on the tile in front of me (my level), if CanPlace (vtable 0x134)
// allows it there (the tile's floor and objects). False: nothing held, or it cannot go there (still
// held: "do the drop" then finds a place with a destination_for_drop).
int cXObjectImpl::TryDrop() {
  cXObjectImpl* held = GetContainedObject(0);
  if (!held) return 0;
  int pos[2] = {m_location[0], m_location[1]};
  StepToward(pos, GetData(1));
  if (!ObjCanPlace(held, pos, m_level, nullptr, 0)) return 0;
  PlaceObject(held, pos, m_level, nullptr);
  return 1;
}

namespace {
short TreeIDByName(Behavior* b, const char* name);
}  // namespace

// PC development aid (SIMMY_PLACE_TEST "create:<object file>@x,y"): the object made and put on the tile
// as TryCreateObject does (a dynamic multi-tile object in the tile's middle, then MergeInPlace). Its id.
int TestCreateObjectAt(const char* fileName, int x, int y) {
  ObjectFolderImpl* folder = Globs_pObjectFolder;
  ObjectModuleImpl* module = Globs_pObjectModule;
  ObjSelector* selector = nullptr;
  for (ObjSelector* s = folder ? folder->GetNextSelector(nullptr) : nullptr; s && !selector; s = folder->GetNextSelector(s))
    if (s->m_fileName && std::strcmp(s->m_fileName, fileName) == 0 && !s->GetIsMultiTileSubObject()) selector = s;
  short id = 0;
  if (!selector || !module || !module->MakeNewOutOfWorldObject(selector, &id)) return 0;
  cXObject* x0 = module->GetObjectFromID(id);
  cXObjectImpl* made = x0 ? x0->GetObjectImplementation() : nullptr;
  if (!made) return 0;
  int pos[2] = {y * 16 + 8, x * 16 + 8};
  auto* dynamic = made->IsMultiTile() ? dynamic_cast<cXMTObjectImpl*>(made) : nullptr;
  if (dynamic && !dynamic->IsDynamic()) dynamic = nullptr;
  PlaceObject(made, pos, 1, nullptr);
  if (dynamic) dynamic->MergeInPlace();
  return id;
}

// 800FFF24
// Operands: +0 the GUID (flag 4 of +5: a person for the neighbour in the frame's stack object instead),
// +4 where: 0 in front of me, 1 / 2 in me (my slot 0), 5 where I am, 3 in front of the stack object, 4 in the stack
// object (its container), 6 out of the world, 7 / 8 next to the object in param 0 / local +6, 9 next
// to me in the direction local +6 says; +5 flags: 1 fail if one is there already, 2 / 0x10 pass my id
// to its main / init tree, 8 only on an empty tile, 0x20 face as the stack object. True: made, and the
// frame's stack object is it; a visitor (flag 4) runs its "init visitor" tree. GC: the place is checked
// (CanPlace; people find a safe spot) and multi-tile objects are made whole.
int cXObjectImpl::TryCreateObject(StackElem* frame, const unsigned char* params) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  const unsigned int guid = simmy::be32(params);
  // Flag 4: a person for the neighbour the stack object names (its selector, GetNeighborSelector).
  Neighbor* neighbor = nullptr;
  ObjSelector* selector = nullptr;
  if (params[5] & 4) {
    neighbor = Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(frame->m_stackObject) : nullptr;
    selector = Globs_pNeighborhood ? Globs_pNeighborhood->GetNeighborSelector(frame->m_stackObject) : nullptr;
  } else {
    selector = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(guid)) : nullptr;
  }
  if (!selector || !module) {
    std::printf("[vm] %s: no object %08x to create\n", DebugName(), (params[5] & 4) ? static_cast<unsigned int>(frame->m_stackObject) : guid);
    return 0;
  }
  int pos[2] = {m_location[0], m_location[1]};
  int level = m_level;
  cXObjectImpl* container = nullptr;
  auto objectAt = [&](int id) -> cXObjectImpl* {
    cXObject* o = GetObjectFromID(static_cast<short>(id));
    return o ? o->GetObjectImplementation() : nullptr;
  };
  switch (static_cast<signed char>(params[4])) {
    case 0: StepToward(pos, GetData(1)); break;
    case 1:
    case 2: container = this; break;  // in me (my slot 0: a person's hand); 5: where I am
    case 3: {
      cXObjectImpl* o = objectAt(frame->m_stackObject);
      if (!o) return 0;
      pos[0] = o->m_location[0], pos[1] = o->m_location[1], level = o->m_level;
      StepToward(pos, o->GetData(1));
      break;
    }
    case 4: {
      cXObjectImpl* o = objectAt(frame->m_stackObject);
      if (!o) return 0;
      pos[0] = o->m_location[0], pos[1] = o->m_location[1], level = o->m_level;
      container = o;
      break;
    }
    case 6: pos[0] = pos[1] = -16; level = 1; break;
    case 7: case 8: {
      const int index = static_cast<signed char>(params[6]);
      int id = 0;
      if (params[4] == 7) {
        if (frame->m_numParams == 0) {
          m_treeSim->SetError(8);
          Error(8);
          return -1;
        }
        id = frame->GetParams()[0];
      } else {
        if (index < 0 || index >= frame->m_numLocals) {
          m_treeSim->SetError(8);
          Error(8);
          return -1;
        }
        id = frame->GetLocals()[index];
      }
      cXObjectImpl* o = objectAt(id);
      if (!o) return 0;
      pos[0] = o->m_location[0], pos[1] = o->m_location[1], level = o->m_level;
      break;
    }
    case 9: {
      short d = 0;
      if (InterpValue(0x19, static_cast<signed char>(params[6]), nullptr, nullptr, &d) == -1) return -1;
      StepToward(pos, d + GetData(1));
      break;
    }
    default: break;
  }
  if (params[5] & 1) {
    // One of these already there: none made.
    for (int id = 1; id <= module->GetNumObjects(); ++id) {
      cXObjectImpl* o = objectAt(id);
      if (o && o->m_selector == selector && o->m_location[0] >> 4 == pos[0] >> 4 && o->m_location[1] >> 4 == pos[1] >> 4)
        return 0;
    }
  }
  short id = 0;
  if (neighbor) {
    // GC: MakeNewOutOfWorldObject (a cXPersonImpl for the type-2 selector).
    cXPersonImpl* person = MakeNeighborPerson(neighbor, selector);
    id = person ? person->m_id : 0;
  } else if (!module->MakeNewOutOfWorldObject(selector, &id)) {
    return 0;
  }
  cXObjectImpl* made = objectAt(id);
  if (!made) return 0;
  if (params[5] & 0x20)
    if (cXObjectImpl* o = objectAt(frame->m_stackObject)) made->m_data.WriteVar(1, static_cast<short>(o->GetData(1)));
  if (static_cast<signed char>(params[4]) != 6) {
    // A dynamic multi-tile object (flood puddles) goes to its tile's middle and, placed, merges with
    // the same objects beside it (cXMTObjectImpl::MergeInPlace, cXMTObject vtable 0x4C).
    auto* dynamic = made->IsMultiTile() ? dynamic_cast<cXMTObjectImpl*>(made) : nullptr;
    if (dynamic && !dynamic->IsDynamic()) dynamic = nullptr;
    if (dynamic) {
      pos[0] = (pos[0] & ~0xf) | 8;
      pos[1] = (pos[1] & ~0xf) | 8;
    }
    PlaceObject(made, pos, level, container);
    if (dynamic) dynamic->MergeInPlace();
  }
  if (neighbor)
    if (auto* p = dynamic_cast<cXPersonImpl*>(made))
      if (p->m_animator) p->m_animator->m_resetPos = 1;  // PC: the animator takes the new place
  // Flag 4: a visitor runs its "init visitor" tree at once (RunOneTickTree, itself the stack object).
  if (params[5] & 4)
    if (auto* p = made->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(made) : nullptr)
      if (p->IsVisitor() && p->m_treeSim) {
        Behavior* b = p->GetBehavior();
        const short tree = TreeIDByName(b, "init visitor");
        if (tree) {
          const int r = p->m_treeSim->RunOneTickTree(b, tree, static_cast<short>(p->m_id), nullptr);
          static int logged = 0;
          if (logged++ < 10) std::printf("[vm] %s runs \"init visitor\" (%04x): %d\n", p->DebugName(), static_cast<unsigned short>(tree), r);
        }
      }
  // Its main tree's frame (its init tree has run): flag 2 gets my id as param 0 and my stack object as
  // its stack object, flag 0x10 my temp 0 as param 0 (both: error 0x37).
  if ((params[5] & 2) && (params[5] & 0x10)) {
    m_treeSim->SetError(0x37);
    Error(0x37);
    return -1;
  }
  if ((params[5] & 0x12) && made->m_treeSim && made->m_treeSim->GetStackSize() > 0)
    if (StackElem* main = made->m_treeSim->GetNthElem(0)) {
      if (main->m_numParams > 0) main->GetParams()[0] = (params[5] & 2) ? static_cast<short>(m_id) : m_temps[0];
      if (params[5] & 2) main->m_stackObject = frame->m_stackObject;
    }
  static int logged = 0;
  if (logged++ < 20 || std::getenv("SIMMY_SIM_TRACE")) {
    const std::string who = DebugName();
    std::printf("[vm] %s creates %s\n", who.c_str(), made->DebugName());
  }
  frame->m_stackObject = id;
  return 1;
}

// 80100D4C
// Puts what is in my slot (operand +2: literal, or with +0 set, the param it names) into the stack
// object's slot (+6, or with +4 set the param it names). True: placed; false: nothing held (or it does
// not fit: GC CanPlace). Errors: a param out of range (8), no stack object (0x15). GC: my slot must exist
// (HierGetSlot, else 0x1B; PC: slots are not read, the hand is always there).
int cXObjectImpl::TryDropOnto(StackElem* frame, const unsigned char* params) {
  auto param = [&](int modeIndex, int valueIndex, short* out) {
    const short v = static_cast<short>(Be16(params, valueIndex));
    if (Be16(params, modeIndex) == 0) {
      *out = v;
      return true;
    }
    if (v < 0 || v >= frame->m_numParams) return false;
    *out = frame->GetParams()[v];
    return true;
  };
  short mySlot = 0, theirSlot = 0;
  if (!param(0, 1, &mySlot) || !param(2, 3, &theirSlot)) {
    m_treeSim->SetError(8);
    Error(8);
    return -1;
  }
  cXObject* x = GetObjectFromID(frame->m_stackObject);
  cXObjectImpl* target = x ? x->GetObjectImplementation() : nullptr;
  if (!target) {
    m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  cXObjectImpl* held = GetContainedObject(mySlot);
  if (!held) return 0;
  const int pos[2] = {target->m_location[0], target->m_location[1]};
  PlaceObject(held, pos, target->m_level, target, theirSlot);  // GC: CanPlace then Place (vtable 0x134 / 0x13C)
  return 1;
}

// 8010553C
// Operand +0: whom (0 me, 1 the stack object, 2 nobody); +2 flags (2 clear: at once; 1: myself without
// waiting). Killing myself from my own tree waits a tick (the kill is queued).
int cXObjectImpl::TryKillObject(StackElem* frame, const unsigned char* params, short* id) {
  if (frame->m_primState == 1) {
    frame->m_primState = 0;
    return 1;
  }
  const int who = simmy::be16(params);
  if (who > 2) {
    m_treeSim->SetError(9);
    Error(9);
    return -1;
  }
  *id = who == 0 ? m_id : who == 1 ? frame->m_stackObject : 0;
  if (!GetObjectFromID(*id)) return 0;
  static const bool trace = std::getenv("SIMMY_SIM_TRACE") != nullptr;  // PC dev trace
  if (trace) {
    cXObjectImpl* victim = GetObjectFromID(*id)->GetObjectImplementation();
    std::printf("[vm] %s kills %s (tree %04x node %d)\n", DebugName(), victim ? victim->DebugName() : "?",
                static_cast<unsigned short>(frame->GetTreeID()), frame->m_node);
  }
  ObjectModuleImpl* module = Globs_pObjectModule;
  const bool queued = (params[2] & 2) != 0;
  if (*id == m_id) {
    module->AddToKillQueue(*id, false);
    if (!(params[2] & 1)) {
      frame->m_primState = 1;
      return 2;
    }
    return 1;
  }
  module->AddToKillQueue(*id, !queued);
  return 1;
}

// 800FA1F4
// Operands: +0 a GUID, +4 / +6 the variable (data, scope) naming an object. True: the object is of
// that GUID, or its multi-tile group's master is (GC: ObjSelector::GetMasterSelector; PC: the
// definition of the same object file with the group's master id and no sub index).
int cXObjectImpl::TryTestObjectType(StackElem*, const unsigned char* params) {
  const unsigned int guid = simmy::be32(params);
  short id = 0;
  if (InterpValue(static_cast<signed char>(params[6]), Be16(params, 2), nullptr, nullptr, &id) == -1) return -1;
  cXObject* o = GetObjectFromID(id);
  cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
  if (!impl) {
    m_treeSim->SetError(0x17);
    Error(0x17);
    return -1;
  }
  if (!impl->m_def) return 0;
  if (objdef::GUID(impl->m_def) == guid) return 1;
  const short master = objdef::MasterID(impl->m_def);
  const unsigned char* file = master ? SelectorResFile(impl->m_selector) : nullptr;
  const auto* defs = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0)) : nullptr;
  const unsigned int n = defs ? ERQuickdata::U32(defs - 4, 0) : 0;
  for (unsigned int i = 0; i < n; ++i) {
    const auto* d = static_cast<const ObjDefinition*>(ERQuickdata::Ptr(defs, i * 4));
    if (d && objdef::MasterID(d) == master && static_cast<unsigned short>(objdef::SubIndex(d)) == 0xffff)
      return objdef::GUID(d) == guid ? 1 : 0;
  }
  return 0;
}

// ---------------------------------------------------------------------------------------------------
// Functional objects (the function table's entry points 0x0E..0x1D: eat on, sit on, ...)

namespace {

// ObjFnTableQuickData::GetCheckTreeID (80110018): the function table's check tree for `entry`.
short FnCheckTreeID(cXObjectImpl* o, int entry) {
  const unsigned char* file = SelectorResFile(o->m_selector);
  if (entry < 0 || entry >= 0x1e || !file || !o->m_def) return 0;
  const auto* list = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x2c));
  if (!list) return 0;
  const int id = ERQuickdata::S16(o->m_def, 0xbc);
  const unsigned int n = ERQuickdata::U32(list - 4, 0);
  for (unsigned int i = 0; i < n; ++i) {
    const unsigned char* e = list + i * 0x7c;
    if (ERQuickdata::S32(e, 0) == id) return ERQuickdata::S16(e, 0x40 + entry * 2);
  }
  return 0;
}
}  // namespace

// 800DFE38
double cXObjectImpl::CalcShortDistance(cXObject* other) {
  cXObjectImpl* o = other ? other->GetObjectImplementation() : nullptr;
  if (!o) return 0.0;
  const int dx = m_location[0] - o->m_location[0], dy = m_location[1] - o->m_location[1];
  return std::sqrt(static_cast<float>(dx * dx + dy * dy) * 0.00390625f);
}

// 800DFEE0
// The distance to `other` in 1/16 tiles (ApproxSqrt), from tile centres (people: from where they are;
// a contained object: as its container).
double cXObjectImpl::CalcShortDistanceIn16thsOfTiles(cXObject* other) {
  cXObjectImpl* o = other ? other->GetObjectImplementation() : nullptr;
  if (!o) return 0.0;
  auto centred = [](cXObjectImpl* x) {
    cXObject* c = x->GetContainer();
    cXObjectImpl* t = c ? c->GetObjectImplementation() : nullptr;
    return (t ? t : x)->GetType() != 2;
  };
  int x0 = m_location[0], y0 = m_location[1], x1 = o->m_location[0], y1 = o->m_location[1];
  if (centred(this)) x0 += 8, y0 += 8;
  if (centred(o)) x1 += 8, y1 += 8;
  return std::sqrt(static_cast<float>((y0 - y1) * (y0 - y1) + (x0 - x1) * (x0 - x1)));
}

// 80107CDC
// Operand +2 (< 8): my temporary to get the distance from the stack object to the object whose id is
// in (flag 0x100: scope +0 / data +4, else my data 0xB, i.e. me); in tiles, or (flag 0x200) 1/16
// tiles. Either object missing: -1, true.
int cXObjectImpl::TryDistanceTo(StackElem* frame, const unsigned char* params) {
  const unsigned int temp = simmy::be16(params);
  const unsigned int flags = simmy::be16(params + 2);
  if (temp >= 8) {
    if (m_treeSim) m_treeSim->SetError(2);
    Error(2);
    return -1;
  }
  const int scope = (flags & 0x100) ? (flags & 0xff) : 3;
  const short data = (flags & 0x100) ? static_cast<short>(simmy::be16(params + 4)) : 0xb;
  short id = 0;
  if (InterpValue(static_cast<signed char>(scope), data, nullptr, nullptr, &id) == -1) return -1;
  cXObject* from = GetObjectFromID(id);
  cXObject* to = GetObjectFromID(frame->m_stackObject);
  cXObjectImpl* f = from ? from->GetObjectImplementation() : nullptr;
  short& out = m_temps[temp];
  if (!f || !to) {
    out = -1;
    return 1;
  }
  out = static_cast<short>((flags & 0x200) ? f->CalcShortDistanceIn16thsOfTiles(to) : f->CalcShortDistance(to));
  return 1;
}

// 80107EA0
// The direction (0..7, eighths of a turn: 0 -y, 2 +x, 4 +y, 6 -x) from the object whose id is in
// (flag 0x1000000 of operand +4: scope +5 / data +6, else my data 0xB) to the stack object, written to
// scope +2 / data +0.
int cXObjectImpl::TryDirectionTo(StackElem* frame, const unsigned char* params) {
  const short outData = static_cast<short>(simmy::be16(params));
  const signed char outScope = static_cast<signed char>(static_cast<short>(simmy::be16(params + 2)));
  short* ref = nullptr;
  short dummy = 0;
  if (InterpValue(outScope, outData, &ref, nullptr, &dummy) == -1) return -1;
  const unsigned int op1 = simmy::be32(params + 4);
  const int scope = (op1 & 0x1000000) ? static_cast<int>((op1 >> 16) & 0xff) : 3;
  const short data = (op1 & 0x1000000) ? static_cast<short>(op1 & 0xffff) : 0xb;
  short id = 0;
  if (InterpValue(static_cast<signed char>(scope), data, nullptr, nullptr, &id) == -1) return -1;
  cXObject* fromObj = GetObjectFromID(id);
  if (!fromObj) {
    if (m_treeSim) m_treeSim->SetError(0x17);
    Error(0x17);
    return -1;
  }
  cXObject* toObj = GetObjectFromID(frame->m_stackObject);
  if (!toObj) {
    if (m_treeSim) m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  cXObjectImpl* a = fromObj->GetObjectImplementation();
  cXObjectImpl* b = toObj->GetObjectImplementation();
  const int dy = b->m_location[1] - a->m_location[1], dx = b->m_location[0] - a->m_location[0];
  const int ady = dy < 0 ? -dy : dy, adx = dx < 0 ? -dx : dx;
  unsigned int dir;
  if (ady * 2 < adx) dir = 4;
  else dir = adx * 2 < ady ? 2 : 3;
  if (dy < 0) {
    if (dir == 2) dir = 6;
    else if (dir == 3) dir = 5;
  }
  if (dx < 0) {
    if (dir == 4) dir = 0;
    else if (dir == 3) dir = 1;
    else if (dir == 5) dir = 7;
  }
  InterpWriteValue(outScope, outData, &ref, static_cast<short>((dir | 8) - 8));
  return 1;
}

// 80102418
// Operand +0 the function (0 food, 1 sit, 2 eat surface, 3 ..., 6, 7, 8, 10..14): the best object
// offering it becomes the frame's stack object. An object scores its data for the function (copied to
// my data 0x42; function 8: 50), if its check tree for it passes (and leaves 0x42 set), less three
// times the distance (at least 1). Objects in use (data 0x19) or burning are skipped. False: none.
int cXObjectImpl::TryFindFunctionalObject(StackElem* frame, const unsigned char* params) {
  int scoreData, entry;
  switch (static_cast<short>(simmy::be16(params))) {
    case 0: scoreData = 0x1f, entry = 0x12; break;
    case 1: scoreData = 0x20, entry = 0x13; break;
    case 2: scoreData = 0x21, entry = 0x14; break;
    case 3: scoreData = 0x24, entry = 0x15; break;
    case 6: scoreData = 0x25, entry = 0x18; break;
    case 7: scoreData = 0x26, entry = 0x19; break;
    case 8: scoreData = -1, entry = 0x1a; break;
    case 10: scoreData = 0x3d, entry = 0xe; break;
    case 11: scoreData = 0x27, entry = 0x1c; break;
    case 12: scoreData = 0x40, entry = 0x10; break;
    case 13: scoreData = 0x41, entry = 0x11; break;
    case 14: scoreData = 0xf, entry = 0x1d; break;
    default: return 0;
  }
  cXObjectImpl* best = nullptr;
  float bestScore = 0.0f;
  for (cXObjectImpl* o = Globs_pObjectModule ? Globs_pObjectModule->m_first : nullptr; o; o = o->m_nextObject) {
    // In use, or burning (cXObjectImpl::IsBurning 800EB5F8: data 8 bit 0x200).
    if (o->GetData(0x19) >= 1 || (static_cast<unsigned short>(o->GetData(8)) >> 9 & 1)) continue;
    if (!o->GetTreeID(entry)) continue;
    int score = 0x32;
    if (scoreData != -1) score = o->GetData(scoreData);
    m_data.WriteVar(0x42, static_cast<short>(score));
    if (score == 0) continue;
    if (const short check = FnCheckTreeID(o, entry)) {
      if (!m_treeSim || !m_treeSim->RunCheckTree(o->GetBehavior(), check, static_cast<short>(o->m_id), nullptr))
        continue;
      if (GetData(0x42) == 0) continue;
    }
    float s = static_cast<float>(GetData(0x42)) -
              static_cast<float>(CalcShortDistance(o)) * gFunctionalScoreDistanceAttenuation;
    if (s < 1.0f) s = 1.0f;
    if (bestScore < s) {
      best = o;
      bestScore = s;
    }
  }
  if (!best) return 0;
  frame->m_stackObject = static_cast<short>(best->m_id);
  return 1;
}

// 80102840
// Runs the stack object's tree for function operand +0 (as TryFindFunctionalObject numbers them, 4, 5
// and 9 too) on my stack (operand +2 bit 0: as the action's icon). False: it has none.
int cXObjectImpl::TryCallFunctionalTree(StackElem* frame, const unsigned char* params) {
  cXObject* x = GetObjectFromID(frame->m_stackObject);
  if (!x) {
    m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  static const int kEntries[15] = {0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0xe, 0x1c, 0x10, 0x11, 0x1d};
  const int fn = static_cast<short>(simmy::be16(params));
  if (fn < 0 || fn > 14) return 0;
  cXObjectImpl* o = x->GetObjectImplementation();
  const short tree = o ? o->GetTreeID(kEntries[fn]) : 0;
  if (!tree) return 0;
  return GosubObjectTree(x, nullptr, tree, (simmy::be16(params + 2) & 1) != 0) ? 3 : 0;
}

// ---------------------------------------------------------------------------------------------------
// Relationships (primitive 0x1A)

// 800FE28C
// Operands: +0 the relationship variable (0 short term, 1 long term; between people 2 is an error), +1 whose towards whom
// (0 mine towards the stack object, 1 the stack object's towards me, 2 local +3's towards the stack
// object, 3 the stack object's towards local +3), +2 flags (1 fail rather than make the variable, 2 the
// stack object and I as neighbour ids, 4 set it from the value, else read it into the value), the value
// +4 scope, +6 data. People's relationships are their neighbours'. GC: between objects (not people) the
// object module keeps them (vtable 0x1A4; not ported), and the module hears of every access (0x1AC).
int cXObjectImpl::TryRelationship2(StackElem* frame, const unsigned char* params) {
  auto fail = [this](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  const int index = params[0], who = params[1], flags = params[2];
  auto neighborOf = [&](cXObjectImpl* o) -> Neighbor* {
    cXPersonImpl* p = o && o->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(o) : nullptr;
    return p ? hood->FindNeighborByID(p->GetPersonData(0x1f)) : nullptr;
  };
  Neighbor* from = nullptr;
  Neighbor* to = nullptr;
  if (flags & 2) {
    // The stack object (and local +3) are neighbour ids: 0 mine towards the stack object's, 1 the
    // stack object's towards mine, 2 the stack object's towards the local's, 3 the local's towards the
    // stack object's.
    Neighbor* other = hood->FindNeighborByID(frame->m_stackObject);
    if (who == 0 || who == 1) {
      if (GetType() != 2) return fail(0x1c);
      Neighbor* mine = neighborOf(this);
      from = who == 0 ? mine : other;
      to = who == 0 ? other : mine;
    } else if (who == 2 || who == 3) {
      if (params[3] >= frame->m_numLocals) return fail(0x34);
      Neighbor* local = hood->FindNeighborByID(frame->GetLocals()[params[3]]);
      from = who == 2 ? other : local;
      to = who == 2 ? local : other;
    }
  } else {
    auto object = [&](short id) -> cXObjectImpl* {
      cXObject* o = GetObjectFromID(id);
      return o ? o->GetObjectImplementation() : nullptr;
    };
    cXObjectImpl* a = nullptr;
    cXObjectImpl* b = nullptr;
    if (who == 0 || who == 1) {
      cXObjectImpl* stack = object(frame->m_stackObject);
      a = who == 0 ? this : stack;
      b = who == 0 ? stack : this;
    } else if (who == 2 || who == 3) {
      if (params[3] >= frame->m_numLocals) return fail(0x34);
      cXObjectImpl* local = object(frame->GetLocals()[params[3]]);
      cXObjectImpl* stack = object(frame->m_stackObject);
      a = who == 3 ? local : stack;
      b = who == 3 ? stack : local;
    }
    if (!a || !b) {
      if (TraceVM(m_id)) std::printf("[vm] %s: relationship %d of %d / %d: no object\n", DebugName(), who, frame->m_stackObject, params[3] < frame->m_numLocals ? frame->GetLocals()[params[3]] : -1);
      return fail(0x15);
    }
    if (a->GetType() != 2 || b->GetType() != 2) {
      // Between objects: a's relationship matrix, keyed by b's id (the module's RelationshipAccessed,
      // vtable 0x1A4, does nothing). Any variable (GC: only between two people is variable 2 an error).
      RelMatrixImpl* rel = a->GetRelMatrix();
      const int key = b->m_id;
      if (rel->GetArraySize(key) <= index) {
        if (flags & 1) return 0;
        rel->SetArraySize(key, index + 1);
        if (rel->GetArraySize(key) <= index) return 1;
      }
      short* ref = nullptr;
      short value = 0;
      if (InterpValue(static_cast<signed char>(params[4]), Be16(params, 3), &ref, nullptr, &value) == -1) return -1;
      if (!(flags & 4)) {
        if (!ref) return fail(0x13);
        InterpWriteValue(static_cast<signed char>(params[4]), Be16(params, 3), &ref, static_cast<short>(rel->GetValue(key, index)));
      } else {
        rel->SetValue(key, index, value);
      }
      return 1;
    }
    from = neighborOf(a);
    to = neighborOf(b);
  }
  if (!from || !to) {
    if (TraceVM(m_id)) std::printf("[vm] %s: relationship %d (flags %d) with %d: no neighbour\n", DebugName(), who, flags, frame->m_stackObject);
    return fail(0x15);
  }
  if (index == 2) return fail(0x57);
  RelMatrixImpl* rel = from->GetRelations();
  if (rel->GetArraySize(to->m_id) <= index) {
    if (flags & 1) return 0;
    rel->SetArraySize(to->m_id, index + 1);
    if (rel->GetArraySize(to->m_id) <= index) return 1;
  }
  short* ref = nullptr;
  short value = 0;
  if (InterpValue(static_cast<signed char>(params[4]), Be16(params, 3), &ref, nullptr, &value) == -1) return -1;
  if (!(flags & 4)) {
    if (!ref) return fail(0x13);
    InterpWriteValue(static_cast<signed char>(params[4]), Be16(params, 3), &ref, static_cast<short>(rel->GetValue(to->m_id, index)));
  } else {
    const int old = rel->GetValue(to->m_id, index);
    rel->SetValue(to->m_id, index, value);
    hood->RelationshipsChanged(from);
    if (old != value)
      std::printf("[rel] %s (neighbour %d%s) -> %s (neighbour %d%s): relationship %d %d -> %d\n", from->m_name, from->m_id,
                  from->IsCharacter() ? ", character" : "", to->m_name, to->m_id, to->IsCharacter() ? ", character" : "",
                  index, old, value);
  }
  return 1;
}

// 800FEAD4
// The old relationship primitive (0x18). Operands: +0 0 read / else write, +1 the relationship variable,
// +2 the param that holds the value, +3 whose towards whom (0 mine towards the stack object, 1 the stack
// object's towards me, 2 the stack object's towards param 0's, 3 param 0's towards the stack object),
// +4 flags (1 fail rather than make the variable, 2 the stack object and param 0 are neighbour ids).
// Between people their neighbours' relationships (keyed by neighbour id), else the first object's own
// (keyed by object id). The module hears of each access (RelationshipAccessed: nothing).
int cXObjectImpl::TryRelationship(StackElem* frame, const unsigned char* params) {
  auto fail = [this](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  const bool write = params[0] != 0;
  const int index = static_cast<signed char>(params[1]);
  const int who = static_cast<signed char>(params[3]);
  const int flags = params[4];
  const short param0 = frame->m_numParams > 0 ? frame->GetParams()[0] : 0;
  RelMatrixImpl* rel = nullptr;
  int key = 0;
  Neighbor* from = nullptr;
  if (flags & 2) {
    auto* me = GetType() == 2 ? dynamic_cast<cXPersonImpl*>(this) : nullptr;
    Neighbor* to = nullptr;
    switch (who) {
      case 0:
        if (!me) return fail(0x1c);
        from = hood->FindNeighborByID(me->GetPersonData(0x1f));
        to = hood->FindNeighborByID(frame->m_stackObject);
        break;
      case 1:
        if (!me) return fail(0x1c);
        from = hood->FindNeighborByID(frame->m_stackObject);
        to = hood->FindNeighborByID(me->GetPersonData(0x1f));
        break;
      case 2:
        from = hood->FindNeighborByID(frame->m_stackObject);
        to = hood->FindNeighborByID(param0);
        break;
      case 3:
        from = hood->FindNeighborByID(param0);
        to = hood->FindNeighborByID(frame->m_stackObject);
        break;
      default: break;
    }
    if (!from || !to) return fail(0x15);
    rel = from->GetRelations();
    key = to->m_id;
  } else {
    auto object = [&](short id) -> cXObjectImpl* {
      cXObject* o = GetObjectFromID(id);
      return o ? o->GetObjectImplementation() : nullptr;
    };
    cXObjectImpl* a = nullptr;  // whose
    cXObjectImpl* b = nullptr;  // towards whom
    switch (who) {
      case 0: a = this, b = object(frame->m_stackObject); break;
      case 1: a = object(frame->m_stackObject), b = this; break;
      case 2: a = object(frame->m_stackObject), b = object(param0); break;
      case 3: a = object(param0), b = object(frame->m_stackObject); break;
      default: break;
    }
    if (!a || !b) return fail(0x15);
    auto* pa = a->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(a) : nullptr;
    auto* pb = b->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(b) : nullptr;
    if (pa && pb) {
      from = hood->FindNeighborByID(pa->GetPersonData(0x1f));
      if (!from) return fail(0x15);
      rel = from->GetRelations();
      key = pb->GetPersonData(0x1f);
    } else {
      rel = a->GetRelMatrix();
      key = b->m_id;
    }
  }
  if (rel->GetArraySize(key) <= index) {
    if (flags & 1) return 0;
    rel->SetArraySize(key, index + 1);
    if (rel->GetArraySize(key) <= index) return 1;
  }
  const int p = static_cast<signed char>(params[2]);
  if (p < 0 || frame->m_numParams == 0) return fail(8);
  short& value = frame->GetParams()[p & 7];
  if (!write) {
    value = static_cast<short>(rel->GetValue(key, index));
  } else {
    const int old = rel->GetValue(key, index);
    rel->SetValue(key, index, value);
    if (from) {
      if (old != value) std::printf("[rel] %s (neighbour %d) -> neighbour %d: relationship %d %d -> %d (primitive 0x18)\n", from->m_name, from->m_id, key, index, old, value);
    }
  }
  return 1;
}

// 80108450
// Operands: +0 the operation (0 read, 1 set, 2 add, 3 subtract, 4 the dominant memory: the first at 7),
// +1 whose of whom (0 mine of the stack object, 1 the stack object's of me, 2 the stack object's of local
// +4's, 3 local +4's of the stack object), +2 the memory (0..7; flag 1: the temporary it names), +3 flags
// (1 the memory from a temporary, 2 the stack object and local are neighbour ids), the value +5 scope,
// +6 data (read and dominant: written to it). Memories are 0..7.
int cXObjectImpl::TryMemory(StackElem* frame, const unsigned char* params) {
  auto fail = [this](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  const int op = params[0], who = params[1], flags = params[3];
  const bool result = op == 0 || op == 4;
  int from = 0, to = 0;  // neighbour ids
  auto personOf = [&](short id) -> cXPersonImpl* {
    cXObject* o = GetObjectFromID(id);
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    return impl && impl->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
  };
  auto* me = GetType() == 2 ? dynamic_cast<cXPersonImpl*>(this) : nullptr;
  if (flags & 2) {
    switch (who) {
      case 0:
        if (!me) return fail(0x1c);
        from = me->GetPersonData(0x1f), to = frame->m_stackObject;
        break;
      case 1:
        if (!me) return fail(0x1c);
        from = frame->m_stackObject, to = me->GetPersonData(0x1f);
        break;
      case 2:
        if (params[4] >= frame->m_numLocals) return fail(0x34);
        from = frame->m_stackObject, to = frame->GetLocals()[params[4]];
        break;
      case 3:
        if (params[4] >= frame->m_numLocals) return fail(0x34);
        from = frame->GetLocals()[params[4]], to = frame->m_stackObject;
        break;
      default: break;
    }
  } else {
    cXPersonImpl* a = nullptr;  // whose memory
    cXPersonImpl* b = nullptr;  // of whom
    if (who == 0 || who == 1) {
      if (!GetObjectFromID(frame->m_stackObject)) return fail(0x15);
      cXPersonImpl* stack = personOf(frame->m_stackObject);
      a = who == 0 ? me : stack;
      b = who == 0 ? stack : me;
    } else if (who == 2 || who == 3) {
      if (params[4] >= frame->m_numLocals) return fail(0x34);
      const short local = frame->GetLocals()[params[4]];
      if (!GetObjectFromID(local) || !GetObjectFromID(frame->m_stackObject)) return fail(0x15);
      cXPersonImpl* l = personOf(local);
      cXPersonImpl* stack = personOf(frame->m_stackObject);
      a = who == 2 ? stack : l;
      b = who == 2 ? l : stack;
    }
    if (!a || !b) return fail(0x1c);
    from = a->GetPersonData(0x1f), to = b->GetPersonData(0x1f);
  }
  short* ref = nullptr;
  short value = 0;
  if (InterpValue(static_cast<signed char>(params[5]), Be16(params, 3), &ref, nullptr, &value) == -1) return -1;
  if (result && !ref) return fail(0x13);
  int index = params[2];
  if (flags & 1) {
    if (index > 7) return fail(2);
    index = m_temps[index];
  }
  if (op != 4 && static_cast<unsigned int>(index) >= 8) return fail(0x14);
  ObjectModuleImpl* module = Globs_pObjectModule;
  auto clamp = [](int v) { return v < 0 ? 0 : (v > 7 ? 7 : v); };
  switch (op) {
    case 0: value = static_cast<short>(module->GetSimMemory(from, to, index)); break;
    case 1:
      if (value > 7) return fail(0x14);
      module->SetSimMemory(from, to, index, clamp(value));
      break;
    case 2: module->SetSimMemory(from, to, index, clamp(static_cast<int>(module->GetSimMemory(from, to, index)) + value)); break;
    case 3: module->SetSimMemory(from, to, index, clamp(static_cast<int>(module->GetSimMemory(from, to, index)) - value)); break;
    case 4: value = static_cast<short>(module->GetDominantMemory(from, to, 7)); break;
    default: break;
  }
  if (result) InterpWriteValue(static_cast<signed char>(params[5]), Be16(params, 3), &ref, value);
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// Calling a tree by name (primitive 0x1C)

namespace {
// The char string set `id` (ResFile +0x10: {strings, 'STR#', id}) line `index` (1-based, as QuickStringSet::GetString) of a file.
const char* FileCharString(const unsigned char* file, unsigned short id, int index) {
  const auto* sets = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x10)) : nullptr;
  const unsigned int n = sets ? ERQuickdata::U32(sets - 4, 0) : 0;
  for (unsigned int i = 0; i < n; ++i) {
    if (ERQuickdata::U16(sets + i * 12, 8) != id) continue;
    const auto* strings = static_cast<const unsigned char*>(ERQuickdata::Ptr(sets + i * 12, 0));
    const unsigned int count = strings ? ERQuickdata::U32(strings - 4, 0) : 0;
    // QuickStringSet::GetString (800A76AC): 1-based (0: none).
    if (index < 1 || static_cast<unsigned int>(index) > count) return nullptr;
    return ERQuickdata::String(strings, static_cast<unsigned int>(index - 1) * 4);
  }
  return nullptr;
}
// Behavior::GetTreeIDByNameFast (800AC470): the behaviour's tree of that name (its own file's, then the
// semi-global and global ones), 0 for none.
short TreeIDByName(Behavior* b, const char* name) {
  if (!b || !name) return 0;
  for (short cls : {static_cast<short>(0x1000), static_cast<short>(0x2000), static_cast<short>(0x100)})
    if (EREdithTreeSet* set = b->TreeSetFor(cls))
      for (const EREdithTreeSet::Tree& t : set->m_trees)
        if (t.m_name && std::strcmp(t.m_name, name) == 0) return static_cast<short>(t.m_id);
  return 0;
}
}  // namespace

// 800FBF14
// Operands: +0 the char string set, +2 its namespace (1 global, 2 semi-global, 3 private, else the
// running tree's file then the other of private / semi-global), +4 the line: a tree name. +5: 0 / 1 run
// it as a check tree of the stack object (its result), 2 call it on my stack (the stack object's tree;
// its result when it returns; 1 runs the check on the stack object's own stack). False: no such name or
// tree.
int cXObjectImpl::TryCallNamedTree(StackElem* frame, const unsigned char* params) {
  if (frame->m_primState == 1) return m_treeSim->GetLastResult() ? 1 : 0;
  const int ns = Be16(params, 1);
  Behavior* b = frame->m_behavior;
  short first = -1, second = -1;
  if (ns == 1) first = 0x100;
  else if (ns == 2) first = 0x2000;
  else if (ns == 3) first = 0x1000;
  else {
    const int cls = Behavior::GetTreeClass(static_cast<short>(frame->GetTreeID()));
    if (cls == 2) first = 0x1000, second = 0x2000;
    else if (cls == 3) first = 0x2000, second = 0x1000;
  }
  if (first == -1 || !b) {
    m_treeSim->SetError(0x2d);
    Error(0x2d);
    return -1;
  }
  const unsigned short set = simmy::be16(params);
  const char* name = FileCharString(b->GetResFileID(first), set, params[4]);
  if ((!name || !*name) && second != -1) name = FileCharString(b->GetResFileID(second), set, params[4]);
  if (!name || !*name) return 0;
  cXObject* so = GetObjectFromID(frame->m_stackObject);
  cXObjectImpl* target = so ? so->GetObjectImplementation() : nullptr;
  if (!target) {
    m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  const int mode = params[5];
  if (mode > 2) {
    m_treeSim->SetError(0x2d);
    Error(0x2d);
    return -1;
  }
  Behavior* tb = target->GetBehavior();
  const short tree = TreeIDByName(tb, name);
  if (TraceVM(m_id)) std::printf("[vm] %s calls \"%s\" (%04x) of %s, mode %d\n", DebugName(), name, static_cast<unsigned short>(tree), target->DebugName(), mode);
  if (!tree) return 0;
  if (mode < 2) {
    // 0: on my stack; 1: on the stack object's own. Run at once to its end (RunOneTickTree:
    // RunCheckTree), also from inside a check tree. No params (GC: 0): the tree sim's default ones.
    TreeSimImpl* sim = mode == 1 ? target->m_treeSim : m_treeSim;
    if (!sim) return 0;  // PC: GC does not check
    const int r = sim->RunOneTickTree(tb, tree, static_cast<short>(target->m_id), nullptr);
    // PC diagnostic: a named tree that did not finish in one go (GC: error 0x44F, the object's tree
    // assert) - a primitive that waits where the GC's did not.
    if (sim->GetError() == 0x44f) {
      StackElem* e = sim->GetCurElem();
      const std::string me = DebugName();
      std::printf("[vm] %s: named tree \"%s\" (%04x) of %s did not finish (%s, in tree %04x node %d)\n", me.c_str(), name,
                  static_cast<unsigned short>(tree), target->DebugName(), TreeSimImpl::sInMainSim ? "main" : "check",
                  e ? static_cast<unsigned short>(e->GetTreeID()) : 0xffff, e ? e->m_node : -1);
    }
    return r ? 1 : 0;
  }
  // 2: on my stack as an object tree (GosubObjectTree: a person's controlling object is then the
  // target, whose animations its primitives play).
  if (!GosubObjectTree(so, nullptr, tree, false)) return 0;
  frame->m_primState = 1;
  return 3;
}

// ---------------------------------------------------------------------------------------------------
// Dialogs (primitive 0x24)

int cXObjectImpl::overrideDialogReturnCode = 4;  // 804FDCCC

namespace {
bool s_modalDialogWaiting = false;  // PC: TryDialog's wait (GC: a local static)
int s_modalDialogCount = 0;         // PC: modal dialogs shown so far
}  // namespace
// PC (dev aid, game/padgate.cpp): a modal dialog waits for its answer; how many have been shown.
bool PcModalDialogWaiting() { return s_modalDialogWaiting; }
int PcPrivateScreenPlayer();  // uiscreens.cpp (PC co-op private screens)
int PcModalDialogCount() { return s_modalDialogCount; }

// 8010502C
// Lines given as -1 are temps' values + 1: the message temp 0, the title 1, the buttons 2 / 3 / 4.
void cXObjectImpl::RewriteDialogParamIndices(unsigned char* param) {
  if (static_cast<signed char>(param[2]) == -1) param[2] = static_cast<unsigned char>(m_temps[0] + 1);
  if (static_cast<signed char>(param[3]) == -1) param[3] = static_cast<unsigned char>(m_temps[2] + 1);
  if (static_cast<signed char>(param[4]) == -1) param[4] = static_cast<unsigned char>(m_temps[3] + 1);
  if (static_cast<signed char>(param[6]) == -1) param[6] = static_cast<unsigned char>(m_temps[1] + 1);
  if (static_cast<signed char>(param[0]) == -1) param[0] = static_cast<unsigned char>(m_temps[4] + 1);
}

// 8010515C
// Shows the dialog (EdithDialogPrimitive::SetDialog) and, for a modal one, waits (4) until it is
// answered: then the answer goes to temp (operand +7 bits 4-6) and the primitive gives the callback's
// code (true). With the family asleep or away (ObjectModule::IsFamilyMemberAwakeAndVisible) a modal one
// is skipped (temp 0 = 0). GC: also skipped while a player's interactor is busy (InteractorManager
// +0x160 / +0x234) and while the EORGlobals hold one (+0x350); PC: the interactors are PC stand-ins
// without those fields, so never busy.
int cXObjectImpl::TryDialog(StackElem* frame, const unsigned char* params, bool testing) {
  if (!params) return 0;
  unsigned char p[8];
  for (int i = 0; i < 8; ++i) p[i] = params[i];
  RewriteDialogParamIndices(p);
  bool& waiting = s_modalDialogWaiting;
  if (overrideDialogReturnCode != 4) {
    const int code = overrideDialogReturnCode;
    m_temps[(p[7] >> 4) & 7] = static_cast<short>(TheEdithDialog()->GetModalDialogResult());
    overrideDialogReturnCode = 4;
    waiting = false;
    return code;
  }
  if (waiting) return 4;
  const bool busy = false;
  const int awake = Globs_pObjectModule && Globs_pObjectModule->IsFamilyMemberAwakeAndVisible() ? 1 : 0;  // vtable 0xBC
  const int kind = p[5] & 0xf0;
  bool modal = kind == 0x40;
  if ((p[5] & 0xf0) == 0 && (static_cast<signed char>(p[3]) != -2 || static_cast<signed char>(p[0]) != -2)) modal = true;
  bool show = false;
  if (modal) {
    if (busy || !awake) m_temps[0] = 0;
    else show = true;
  } else if ((kind == 0x10 || kind == 0x20 || kind == 0x50 || !busy) && awake) {
    show = true;
  }
  if (!show) return 1;
  if (kind == 0 || kind <= 0x50) {
    if (TheEdithDialog()->SetDialog(frame, p, this, testing) && (kind == 0 || kind == 0x30 || kind == 0x40)) {
      waiting = true;
      ++s_modalDialogCount;
      return 4;
    }
  } else {
    TheEdithDialog()->SetDialog(frame, params, this, false);
  }
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// Wants and fears, goals (primitives 0x42, 0x43, 0x38)

// 80109048
// Operand +0 the event; +2 / +3 the target / count (short combo values) when the event has them. The
// stack object names who is told: -1 every neighbour, -2 the people on the lot, -3 the residents, else
// that neighbour (0: error). Temp 7 = the answer (0 nothing, 1 moved on, 2 an active one fulfilled).
int cXObjectImpl::TryWantsAndFearsNotify(StackElem* frame, const unsigned char* params) {
  auto fail = [this](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  WantFear__Notification n;
  n.m_event = static_cast<short>(simmy::be16(params));
  const unsigned char* event = TheWantFearManager()->GetEvent(static_cast<unsigned short>(n.m_event));
  if (!event) return fail(0x59);
  short scope = 0, data = 0;
  if (event[10] != 0) {
    GetShortComboValue(static_cast<signed char>(params[2]), &scope, &data);
    InterpValue(scope, data, nullptr, nullptr, &n.m_target);
  }
  if (event[0xb] != 0) {
    GetShortComboValue(static_cast<signed char>(params[3]), &scope, &data);
    InterpValue(scope, data, nullptr, nullptr, &n.m_count);
  }
  NeighborhoodImpl* hood = Globs_pNeighborhood;
  const short who = frame->m_stackObject;
  int answer = 0;
  if (who == -2) {
    answer = hood->NotifyAllPersons(n, nullptr);
  } else if (who == -1) {
    answer = hood->NotifyAllNeighbors(n, nullptr);
  } else if (who == -3) {
    answer = hood->NotifyAllResidents(n, nullptr);
  } else {
    Neighbor* nb = who != 0 ? hood->FindNeighborByID(who) : nullptr;
    if (!nb) return fail(0x38);
    answer = nb->Notify(n, true);
  }
  m_temps[7] = static_cast<short>(answer);
  return 1;
}

// 80109264
// Operand +0 the want tree; the neighbour is the value of (+4 scope, +2 data). False: not started.
int cXObjectImpl::TryChangeWantFearTree(StackElem*, const unsigned char* params) {
  short id = 0;
  if (InterpValue(static_cast<signed char>(params[4]), Be16(params, 1), nullptr, nullptr, &id) == -1) return -1;
  Neighbor* n = Globs_pNeighborhood->FindNeighborByID(id);
  if (!n) {
    m_treeSim->SetError(0x38);
    Error(0x38);
    return -1;
  }
  return n->ForceNewTree(simmy::be16(params)) ? 1 : 0;
}

// 80100F88
// Money: the amount (operand +0: 0 a literal, 1 a parameter, 2 a local, else the scope +1; the value +2)
// is spent as expense type +6 (0..8), from player 2's funds with flag 4; flag 2 makes it income. False
// (nothing spent) when the funds do not cover it; flag 1 only checks.
int cXObjectImpl::TryBudget(StackElem*, const unsigned char* params) {
  int scope = params[0];
  if (scope == 0) scope = 7;
  else if (scope == 1) scope = 9;
  else if (scope == 2) scope = 0x19;
  else scope = params[1];
  const int type = params[6];
  if (type >= 9) {
    if (m_treeSim) m_treeSim->SetError(0x39);
    Error(0x39);
    return -1;
  }
  const unsigned short flags = static_cast<unsigned short>(Be16(params, 2));
  short value = 0;
  if (InterpValue(static_cast<signed char>(scope), Be16(params, 1), nullptr, nullptr, &value) == -1) return -1;
  const cSimulator__tFundsPlayerNumber player{(flags & 4) ? 1 : 0};
  const int funds = Globs_pSimulator ? Globs_pSimulator->GetFunds(player) : 0;
  int amount = value;
  if (flags & 2) amount = -amount;
  if (amount >= 0 && amount > funds) return 0;
  if (!(flags & 1) && Globs_pSimulator) {
    Globs_pSimulator->Spend(ExpenseType{type}, amount, player);
    std::printf("[money] %s: %s %d (type %d), funds %d -> %d\n", DebugName(), amount < 0 ? "earns" : "spends",
                amount < 0 ? -amount : amount, type, funds, Globs_pSimulator->GetFunds(player));
  }
  return 1;
}

// 80106798
// The story family's goals and unlocks (family 1): operand +7 the kind, the index the value of (+5
// scope, +2 data); +6 flag 1: is it unlocked?; else unlock it (false: it was), flag 2 with the reward
// dialog (H2DTarget::SpawnRewardMomentDialog; none can be made yet: wait). Operands +0 or +4 set: true
// at once.
int cXObjectImpl::TryCompleteGoal(StackElem*, const unsigned char* params) {
  if (Be16(params, 0) != 0 || params[4] != 0) return 1;
  GoalUnlock unlocks = FamilyGoalUnlock(1);
  const int kind = params[7];
  if (!unlocks.m_bits) return 0;
  short index = 0;
  if (InterpValue(static_cast<signed char>(params[5]), Be16(params, 1), nullptr, nullptr, &index) == -1) return -1;
  if (index < 0 || index >= unlocks.GetUnlockCount(IGoalUnlock__UnlockType{kind})) {
    m_treeSim->SetError(0x49);
    Error(0x49);
    return -1;
  }
  // PC development aid: SIMMY_UNLOCK_LOTS=2,3 counts those locations (kind 2) as unlocked (granted on the
  // first goal primitive: the story's GOLD/PLATINUM wants unlock them through the move-a-tron).
  static const char* devLots = std::getenv("SIMMY_UNLOCK_LOTS");
  static bool devLotsDone = false;
  if (devLots && !devLotsDone) {
    devLotsDone = true;
    for (const char* c = devLots; *c;) {
      const int lot = std::atoi(c);
      if (lot >= 0 && lot < unlocks.GetUnlockCount(IGoalUnlock__UnlockType{2})) unlocks.GrantUnlock(IGoalUnlock__UnlockType{2}, static_cast<short>(lot));
      while (*c && *c != ',') ++c;
      if (*c == ',') ++c;
    }
  }
  const int unlocked = unlocks.IsUnlocked(IGoalUnlock__UnlockType{kind}, index);
  if (params[6] & 1) return unlocked ? 1 : 0;
  if (unlocked) return 0;
  // With the reward dialog: while another one is up (or waiting) the primitive waits (4).
  if ((params[6] & 2) && _gd.m_h2dTarget && !_gd.m_h2dTarget->SpawnRewardMomentDialog(kind, index)) return 4;
  unlocks.GrantUnlock(IGoalUnlock__UnlockType{kind}, index);
  // Fashion (kind 1): the clothing group of that number unlocks, marked new.
  if (kind == 1) {
    NeighborhoodImpl* hood = Globs_pNeighborhood;
    if (auto* family = hood ? dynamic_cast<PlayerFamilyImpl*>(hood->GetFamily(1)) : nullptr)
      family->GetClothingUnlocks()->UnlockGroup(static_cast<unsigned int>(index), true);
  }
  std::printf("[goal] %s unlocks kind %d #%d\n", DebugName(), kind, index);
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// The generic sim call (primitive 0x01)

// 80102A24
// Operand +0 the call (true unless a call says otherwise; temps are mine). 0x02 my current action's
// icon = the stack object; 0x03 the stack object (a player's selected sim) died: that player's
// tombstone screen; 0x04 the stack neighbour moves in (story: joins the player family, taking the last
// name of neighbour temp 0; free play: the player's roommate); 0x05 funds += family temp 0's net worth;
// 0x06 the stack neighbour moves out (story: leaves its family; false if it cannot) / the roommate
// leaves; 0x0A / 0x0B buy and build locked / unlocked; 0x0C temp 0 = 1 (the stack object exists);
// 0x0D the people using the stack object stop (CleanupPeople); 0x0E simulator global 0x1F = temp 0;
// 0x0F a person leaves the object (person data 0x48 = temp 0, 0x49 = 0); 0x10, 0x23 nothing;
// 0x13 / 0x14 / 0x15 / 0x19 simulator globals 0x25 / 0x26 / 0x27 (the free-will override) / 0x28 =
// temp 0, 0x18 global 0x29 = (temp 0 == 0); 0x16 the game leaves the lot; 0x1A / 0x1B player 1 / 2
// takes the previous sim; 0x1E the players swap sims; 0x1F / 0x20 player 1 / 2 selects person temp 0;
// 0x22 the wardrobe; 0x24 the ringtone of player temp 0; 0x25 message temp 1 to player temp 0's
// messages; 0x26 temp 1 = object temp 0 is in the world (none: false); 0x27 food creation; 0x28 temp 0
// = player temp 0's message temp 1 is open; 0x29 a person check; 0x2A temp 0 = player temp 0's camera
// director is free; 0x2B temp 0 = the HUD's modeless dialogs; 0x2C / 0x2E / 0x2F the HUD; 0x2D the
// player's interactor; 0x30 family 1's aspiration points += temp 0 (and its unlocks); 0x31 the stack
// neighbour's wants and fears. 0, 8, 9, 0x11, 0x12, 0x1D: error 5; any other: error 0x2A.
// Not ported: 0x01 (two people swap seats; no tree on the disc uses it) and 0x21 (ShowAllObjectDialogs:
// only the debug object zz_move_a__tron).
int cXObjectImpl::TryGenericSimCall(StackElem* frame, const unsigned char* params) {
  const int call = static_cast<short>(simmy::be16(params));
  auto fail = [this](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  // PC development aid: SIMMY_GSC_TRACE=1 logs each call id's first use (which tree, which object).
  static const bool trace = std::getenv("SIMMY_GSC_TRACE") != nullptr;
  if (trace && call >= 0 && call < 0x40) {
    static bool used[0x40];
    if (!used[call]) {
      used[call] = true;
      StackElem* cur = m_treeSim->GetCurElem();
      std::printf("[vm] generic sim call 0x%02x first used by %s (tree %#x)\n", call, DebugName(), cur ? cur->GetTreeID() : 0);
    }
  }
  switch (call) {
    case 0x0f: {
      if (GetType() != 2) {
        m_treeSim->SetError(0x1c);
        Error(0x1c);
        return -1;
      }
      cXPersonImpl* person = static_cast<cXPersonImpl*>(this);
      person->SetPersonData(0x48, m_temps[0]);
      person->SetPersonData(0x49, 0);
      // GC: with temp 0 clear, ComputeRect (vtable 0x1BC) from its location; PC: the footprint is
      // computed where it is needed (collision, routing).
      return 1;
    }
    case 0x0a:
    case 0x0b:
      // Buy and build locked (0xA) / unlocked (0xB) by this object.
      if (call == 0x0a) Globs_pObjectModule->DisableBuyAndBuild(this);
      else Globs_pObjectModule->EnableBuyAndBuild(this);
      return 1;
    case 0x26: {
      // Temp 1 = whether object temp 0 is in the world (cXObjectImpl::IsInWorld, 800DCB50: on the lot);
      // no such object: false (global 011f "Verify stack obj id" fails on it).
      cXObject* o = GetObjectFromID(m_temps[0]);
      cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
      if (!impl) return 0;
      m_temps[1] = (impl->m_location[1] >> 4) > 0 && (impl->m_location[0] >> 4) > 0 ? 1 : 0;
      return 1;
    }
    case 0x16:
      // The game leaves the lot: TransitionToNewHouse(temp 0 the house, temp 1 the mode).
      // PC co-op: not while a player has food creation up (uiscreens.cpp, PcCoopPrivateScreens; on the
      // GC the lot stood still meanwhile): the sim waits (2) until it is closed.
      if (PcPrivateScreenPlayer() >= 0) return 2;
      TransitionToNewHouse(0, m_temps[0], m_temps[1]);
      return 1;
    case 0x18:
      if (Globs_pSimulator) Globs_pSimulator->SetGlobal(0x29, static_cast<short>(m_temps[0] == 0 ? 1 : 0));
      return 1;
    case 0x19:
      if (Globs_pSimulator) Globs_pSimulator->SetGlobal(0x28, m_temps[0]);
      return 1;
    case 0x2d:
      // GC: the player whose sim this is: its interactor runs command operand +2 (vtable 0x9C with the
      // object temp 0 names, 0xA4 for temp 0 == -1). PC: the interactor's UI is not ported.
      if (simmy::be16(params + 2) != 0)
        SIMMY_STUB("cXObjectImpl::TryGenericSimCall: 0x2D the player interactor's UI command", 0x80102A24);
      return 1;
    case 0x1a:
    case 0x1b:
      // Player 1 (0x1A) / 2 (0x1B) takes the previous sim of the family (ChangeSelectedSimL: e.g. the
      // one going to work hands over).
      ChangeSelectedSimL(call - 0x1a);
      return 1;
    case 0x02: {
      // My current action's icon becomes the stack object (none running: nothing).
      cXObject* icon = GetObjectFromID(frame->m_stackObject);
      if (!icon) {
        m_treeSim->SetError(10);
        Error(10);
        return -1;
      }
      cXPersonImpl* person = dynamic_cast<cXPersonImpl*>(this);
      if (!person) {
        m_treeSim->SetError(0x1c);
        Error(0x1c);
        return -1;
      }
      Interaction* current = person->GetCurrentAction();
      if (current->m_id != 0) current->SetIconObject(icon);
      return 1;
    }
    case 0x0e:
      // Simulator global 0x1F = temp 0.
      if (Globs_pSimulator) Globs_pSimulator->SetGlobal(0x1f, m_temps[0]);
      return 1;
    case 0x29:
      // Temp 0 must be 0 or 1 (else error 0x14), and my tree sim a person's (else 0x4D).
      if (static_cast<unsigned short>(m_temps[0]) >= 2) {
        m_treeSim->SetError(0x14);
        Error(0x14);
        return -1;
      }
      if (!dynamic_cast<cXPersonImpl*>(this)) {
        m_treeSim->SetError(0x4d);
        Error(0x4d);
        return -1;
      }
      return 1;
    case 0x2c:
      // The HUD's modeless dialogs go.
      if (_gd.m_h2dTarget) _gd.m_h2dTarget->CleanupModelessDialogs();
      return 1;
    case 0x2e:
      // The HUD's interaction panel animates: player temp 0, daily temp 1, lifetime temp 2, bonus temp 3.
      if (_gd.m_h2dTarget && static_cast<unsigned short>(m_temps[0]) < 2)  // PC: a player index H2D can take
        _gd.m_h2dTarget->TriggerInteractionAnimation(m_temps[0], static_cast<unsigned int>(m_temps[1]),
                                                     static_cast<unsigned int>(m_temps[2]), static_cast<unsigned int>(m_temps[3]));
      return 1;
    case 0x2f:
      // The HUD's interaction panel values: player temp 0, change temp 1, relationship temp 2.
      if (_gd.m_h2dTarget && static_cast<unsigned short>(m_temps[0]) < 2)  // PC: a player index H2D can take
        _gd.m_h2dTarget->SetInteractionDisplayValues(m_temps[0], static_cast<unsigned int>(m_temps[1]),
                                                     static_cast<unsigned int>(m_temps[2]));
      return 1;
    case 0x22: return GenericSimCallWardrobe(this);  // the wardrobe: in-game CAS (ingamecas.cpp)
    case 0x27: return GenericSimCallXamMode(this);   // food creation (ingamecas.cpp)
    case 0x31: {
      // The stack object's neighbour (a neighbour id) shuffles: temp 0 = 1 all its wants and fears,
      // 2 its four wants, 3 nothing.
      Neighbor* n = Globs_pNeighborhood ? Globs_pNeighborhood->FindNeighborByID(frame->m_stackObject) : nullptr;
      if (!n) {
        m_treeSim->SetError(0x38);
        Error(0x38);
        return -1;
      }
      if (m_temps[0] == 1) n->ShuffleWantFear(-1);
      else if (m_temps[0] == 2) for (int slot = 0; slot < 4; ++slot) n->ShuffleWantFear(slot);
      else if (m_temps[0] != 3) {
        m_treeSim->SetError(0x14);
        Error(0x14);
        return -1;
      }
      return 1;
    }
    case 0x03: {
      // The stack object (a player's selected sim) died: that player's tombstone screen.
      cXObject* o = GetObjectFromID(frame->m_stackObject);
      cXObjectImpl* oi = o ? o->GetObjectImplementation() : nullptr;  // PC: GC dereferences it
      auto* dead = oi ? dynamic_cast<cXPersonImpl*>(oi) : nullptr;
      if (!dead) return fail(0x1c);
      if (static_cast<cXPerson*>(dead) == _globals.GetSelectedPerson(0))
        UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("PAUSE_DEATH_TOMBSTONE_PLAYER1"));
      else if (static_cast<cXPerson*>(dead) == _globals.GetSelectedPerson(1))
        UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("PAUSE_DEATH_TOMBSTONE_PLAYER2"));
      else
        return fail(0x14);
      return 1;
    }
    case 0x04: {
      // The stack neighbour moves in. Story: it leaves its family and joins the player's (false if
      // either fails), taking neighbour temp 0's last name (its selector and its sim on the lot).
      // Free play: it becomes the player's roommate.
      NeighborhoodImpl* hood = Globs_pNeighborhood;
      Neighbor* n = hood->FindNeighborByID(frame->m_stackObject);
      if (!n) return fail(0x38);
      if (!_gd.m_fieldE8) {
        hood->AddAsPlayerRoommate(n);
        return 1;
      }
      FamilyImpl* family = hood->GetFamily(_gd.m_fieldD8);
      if (n->m_pers[0x3d] != 0 && hood->RemoveFromFamily(n) != 0) return 0;  // +0xEA its family
      if (hood->AddToFamily(n, family) != 0) return 0;
      Neighbor* other = hood->FindNeighborByID(m_temps[0]);
      if (!other) return 1;
      n->m_selector->SetUserLastName(other->m_selector->GetUserLastName());
      cXPersonImpl* person = NeighborPerson(n);  // GC: the neighbour's person (+0x18); PC: by search
      if (!person) return 1;
      if (auto* desc = reinterpret_cast<CasSimDescriptionS2C*>(person->GetNormalSimDescription())) {
        const std::u16string& last = other->m_selector->GetUserLastName();
        // wcsncpy(+0x44, last, 0x20): copied up to its end, the rest zeroed.
        size_t i = 0;
        for (; i < 0x20 && i < last.size() && last[i]; ++i) desc->m_lastName[i] = static_cast<unsigned short>(last[i]);
        for (; i < 0x20; ++i) desc->m_lastName[i] = 0;
      }
      return 1;
    }
    case 0x05: {
      // The funds gain family temp 0's net worth (Spend type 2 of its negative); no family: false.
      FamilyImpl* family = Globs_pNeighborhood->GetFamily(m_temps[0]);
      if (!family) return 0;
      Globs_pSimulator->Spend(ExpenseType{2}, -family->GetNetWorth(), cSimulator__tFundsPlayerNumber{0});
      return 1;
    }
    case 0x06: {
      // The stack neighbour moves out. Story: it leaves its family (false: it could not); free play:
      // the player's roommate leaves.
      NeighborhoodImpl* hood = Globs_pNeighborhood;
      Neighbor* n = hood->FindNeighborByID(frame->m_stackObject);
      if (!n) return fail(0x38);
      if (_gd.m_fieldE8) return hood->RemoveFromFamily(n) != 0 ? 0 : 1;
      hood->RemovePlayerRoommate();
      return 1;
    }
    case 0x0c:
      // Temp 0 = 1: the stack object exists.
      if (!GetObjectFromID(frame->m_stackObject)) return fail(10);
      m_temps[0] = 1;
      return 1;
    case 0x0d: {
      // The people using the stack object stop.
      cXObject* o = GetObjectFromID(frame->m_stackObject);
      if (!o) return fail(10);
      Globs_pObjectModule->CleanupPeople(o);
      return 1;
    }
    case 0x10:
    case 0x23:
      return 1;
    case 0x13:
    case 0x14:
    case 0x15:
      // Simulator global 0x25 / 0x26 / 0x27 (the free-will override, ELiveMode::Update) = temp 0.
      Globs_pSimulator->SetGlobal(static_cast<short>(0x25 + call - 0x13), m_temps[0]);
      return 1;
    case 0x1e:
      // The players swap their selected sims.
      _globals.SwapSelectedSims();
      return 1;
    case 0x1f:
    case 0x20: {
      // Player 1 (0x1F) / 2 (0x20) selects person temp 0.
      cXObject* o = Globs_pObjectModule->GetObjectFromID(m_temps[0]);
      cXObjectImpl* oi = o ? o->GetObjectImplementation() : nullptr;
      if (auto* person = oi ? dynamic_cast<cXPersonImpl*>(oi) : nullptr) _globals.SetSelectedPerson(call - 0x1f, person);
      return 1;
    }
    case 0x24: {
      // The ringtone of player temp 0 (1 or 2; else the default) as a UI sound.
      unsigned int tone = 0;
      if (m_temps[0] == 1) tone = static_cast<unsigned int>(UIDB::UIDBGetInt(const_cast<char*>("P1RingtoneSample")));
      else if (m_temps[0] == 2) tone = static_cast<unsigned int>(UIDB::UIDBGetInt(const_cast<char*>("P2RingtoneSample")));
      if (tone == 0) tone = 0x457536c3;
      UIAUDIO::Instance()->PlaySound(tone);
      return 1;
    }
    case 0x25:
      // Message temp 1 to player temp 0's messages (MSGTarget; saved with the game).
      MSGTarget::DeliverXamMessage(m_temps[0], static_cast<unsigned short>(m_temps[1]));
      return 1;
    case 0x28: {
      // Temp 0 = player temp 0's (1 or 2) message temp 1 (1..13, else 10) is open.
      extern unsigned char PDATarget_m_pdaOpenMessages[2][0xe];  // tsupport.cpp
      const int player = static_cast<unsigned short>(m_temps[0] - 1) > 1 ? 0 : m_temps[0] - 1;
      const int message = static_cast<unsigned int>(m_temps[1] - 1) > 0xc ? 10 : m_temps[1];
      m_temps[0] = PDATarget_m_pdaOpenMessages[player][message] != 0 ? 1 : 0;
      return 1;
    }
    case 0x2a: {
      // Temp 0 = player temp 0's (1 or 2) camera director is free (idle or with the game camera).
      const int player = static_cast<unsigned short>(m_temps[0] - 1) > 1 ? 0 : m_temps[0] - 1;
      const CameraDirector* d = _globals.CameraDirectorOf(player);
      const bool busy = d && d->m_state != CameraDirector::kIdle && d->m_state != CameraDirector::kGame;  // PC: GC has both
      m_temps[0] = busy ? 0 : 1;
      return 1;
    }
    case 0x2b:
      // Temp 0 = the HUD's modeless dialogs.
      m_temps[0] = static_cast<short>(_gd.m_h2dTarget ? _gd.m_h2dTarget->CountModelessDialogs() : 0);
      return 1;
    case 0x30: {
      // Family 1's aspiration points += temp 0 (at most kTotalAspirationMax), then its object unlocks.
      auto* family = dynamic_cast<PlayerFamilyImpl*>(Globs_pNeighborhood->GetFamily(1));
      if (!family) return 1;
      int total = family->m_totalAspirationScore + m_temps[0];
      if (total > kTotalAspirationMax) total = kTotalAspirationMax;
      family->m_totalAspirationScore = total;
      FamilyGoalUnlock(1).GrantObjectUnlocks(total);
      return 1;
    }
    case 0x00:
    case 0x08:
    case 0x09:
    case 0x11:
    case 0x12:
    case 0x1d:
      return fail(5);
    case 0x01:
    case 0x21: {
      static bool seen[0x40];
      if (!seen[call]) {
        seen[call] = true;
        std::printf("[vm] %s: generic sim call 0x%x is not ported (true)\n", DebugName(), call);
      }
      return 1;
    }
    default:
      return fail(0x2a);
  }
}

// 800FDAA8
// Queues an interaction for the person that is the frame's stack object: operand +0 the tree table
// entry (flag 0x40: temp N names it) of the object param (flag 2: local) operand +1 names; +2 the
// priority (0 mine (person data 0x21), 1 100, 2 2, 3 0x32); +3 flags: 1 the icon object in local
// operand +4, 4 a continuation (an error while the person is idle), 8 carry the name over, 0x10 to the
// head, 0x20 run as a sub-action. True: queued.
int cXObjectImpl::TryPushAction(StackElem* frame, const unsigned char* params) {
  auto fail = [this](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  const unsigned int slot = params[1];
  const short* vars = nullptr;
  if (!(params[3] & 2)) {
    if (slot > frame->m_numParams) return fail(8);
    vars = frame->GetParams();
  } else {
    if (frame->m_numLocals < slot) return fail(0x34);
    vars = frame->GetLocals();
  }
  cXObject* object = vars ? GetObjectFromID(vars[slot]) : nullptr;
  if (!object) return fail(0x17);
  int index = params[0];
  if (params[3] & 0x40) {
    if (7 < params[0]) return fail(2);
    index = m_temps[params[0]];
  }
  cXObjectImpl* impl = object->GetObjectImplementation();
  const unsigned char* file = impl ? SelectorTreeTabResFile(impl->m_selector) : nullptr;
  if (!file || !ERQuickdata::Ptr(file, 0x24)) return fail(0x1f);
  cXObject* target = GetObjectFromID(frame->m_stackObject);
  if (!target) return fail(0x15);
  cXObjectImpl* targetImpl = target->GetObjectImplementation();
  if (!targetImpl || targetImpl->GetType() != 2) return fail(0x1c);
  cXPersonImpl* person = static_cast<cXPersonImpl*>(targetImpl);
  int priority = 1;
  switch (params[2]) {
    case 0:
      if (GetType() == 2) priority = static_cast<cXPersonImpl*>(this)->GetPersonData(0x21);
      break;
    case 1: priority = 100; break;
    case 2: priority = 2; break;
    case 3: priority = 0x32; break;
    default: return fail(0x2e);
  }
  Interaction action(person, object, index, priority);
  if (!action.GetEntry()) return fail(0x16);
  action.m_flags &= ~Interaction::kCarryNameOver;
  if (params[3] & 8) action.m_flags |= Interaction::kCarryNameOver;
  if (params[3] & 1) {
    if (frame->m_numLocals <= params[4]) return fail(0x34);
    action.SetIconObject(GetObjectFromID(frame->GetLocals()[params[4]]));
  }
  action.m_flags &= ~Interaction::kContinuation;
  if (params[3] & 4) action.m_flags |= Interaction::kContinuation;
  action.m_flags &= ~Interaction::kPushToHead;
  if (params[3] & 0x10) action.m_flags |= Interaction::kPushToHead;
  action.m_flags &= ~Interaction::kRunAsSub;
  if (params[3] & 0x20) action.m_flags |= Interaction::kRunAsSub;
  // GC: Globs::bStupidMode is off while it is queued.
  if ((action.m_flags & Interaction::kContinuation) && person->IsIdle()) return fail(0x4c);
  return person->AddAction(&action) ? 1 : 0;
}

// 8010FAB0
// The player's previous family sim becomes the selected one; the player's camera centres on it.
void cXObjectImpl::ChangeSelectedSimL(int player) {
  _globals.ReverseSelectedPerson(player);
  if (ESimsCam* cam = _gd.m_players ? _gd.m_players[player & 1].m_cam : nullptr) cam->CenterOnSelectedSim();
}

// ---------------------------------------------------------------------------------------------------
// Primitives: people near doors, user events, motives, inventory, preloading, room effects, the GBA,
// passive influence, fire

namespace {
// The lot's people in the order ObjectModuleImpl::GetNextPerson (800F9860) walks them (GC: the module's
// people list, in the order they were added; PC: by id).
std::vector<cXPersonImpl*> LotPeople() {
  std::vector<cXPersonImpl*> people;
  ObjectModuleImpl* module = Globs_pObjectModule;
  const int count = module ? module->GetNumObjects() : 0;
  for (int id = 1; id <= count; ++id) {
    cXObject* o = module->GetObjectFromID(id);
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    if (impl && impl->GetType() == 2)
      if (auto* p = dynamic_cast<cXPersonImpl*>(impl)) people.push_back(p);
  }
  return people;
}

// The step (x, y in tiles, FTilePt order: +0, +4) in front of direction `dir` (0..7) as
// TryIsPersonInArea and TryBurn build it.
void DirectionStep(int dir, int* a, int* b) {
  static const int kA[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  static const int kB[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  *a = kA[dir & 7];
  *b = kB[dir & 7];
}

PassiveInfluenceSystem* InfluenceSystem() { return Globs_pSimulator ? Globs_pSimulator->GetPassiveInfluenceSystem() : nullptr; }

constexpr unsigned int kFireGUID = 0x24c95f99;  // efx_fire
}  // namespace

// 800FA314
// Operands: +0 / +2 the variable naming a door (a portal; anything else: false); +3 who: 1 people who
// are no player's primary sim, 2 primary sims only, 3 a player's selected sim under direct control and
// not autonomous, else anyone. My temp 0: the range (1/16 tiles, 0: 0x1C). True: such a person within
// the range of the middle between the door and its other side, in either side's room, nearer than a
// person's width or within 30 degrees of the door's axis (either way) and facing it (within 30 degrees).
// GC quirks kept: a ghost met in the walk ends it (false); once one person has passed the range test,
// the rooms the others are tested against are 0.
int cXObjectImpl::TryIsPersonInArea(StackElem*, const unsigned char* params) {
  int range = static_cast<unsigned short>(m_temps[0]);
  if (range == 0) range = 0x1c;
  short id = 0;
  if (InterpValue(static_cast<signed char>(params[2]), Be16(params, 0), nullptr, nullptr, &id) == -1) return -1;
  cXObject* o = GetObjectFromID(id);
  cXObjectImpl* door = o ? o->GetObjectImplementation() : nullptr;
  if (!door) return -1;
  if (door->GetType() != 8) return 0;
  bool tested = false;
  float midA = 0, midB = 0, axisA = 0, axisB = 0;
  for (cXPersonImpl* p : LotPeople()) {
    if (p->IsGhost()) return 0;
    const int who = params[3];
    if (who == 1) {
      if (p->IsPrimarySim(0) || p->IsPrimarySim(1)) continue;
    } else if (who == 2) {
      if (!p->IsPrimarySim(0) && !p->IsPrimarySim(1)) continue;
    } else if (who == 3) {
      int player = -1;
      if (p == _globals.GetSelectedPerson(0)) player = 0;
      else if (p == _globals.GetSelectedPerson(1)) player = 1;
      if (player < 0) continue;
      InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player);  // GetPlayerInteractor(player, 0)
      if (!d || d->m_autonomous != 0) continue;
    }
    int roomA = 0, roomB = 0;
    if (!tested) {
      // The door's other side (cXPortal::GetOtherSide): the next part round its group with a portal tree.
      auto* mt = IsPortalObject(door) ? dynamic_cast<cXMTObjectImpl*>(door) : nullptr;
      if (!mt) break;
      cXMTObjectImpl* other = mt;
      do {
        cXMTObjectImpl* next = other->GetNextMultiTileObject();
        other = next ? next : other->m_lead;
      } while (other && other != mt && other->GetTreeID(0xf) == 0);
      if (!other || other == mt) break;
      midA = static_cast<float>((door->m_location[1] + other->m_location[1]) >> 1);
      midB = static_cast<float>((door->m_location[0] + other->m_location[0]) >> 1);
      roomA = static_cast<short>(door->GetData(0x1d));
      roomB = static_cast<short>(other->GetData(0x1d));
    }
    const int room = static_cast<unsigned short>(p->GetData(0x1d));  // GetRoom: unsigned (the door's two: signed)
    if (room != roomA && room != roomB) continue;
    const float dA = static_cast<float>(p->m_location[1]) - midA;
    const float dB = static_cast<float>(p->m_location[0]) - midB;
    const double dist = std::sqrt(dA * dA + dB * dB);
    if (static_cast<double>(static_cast<float>(range)) < dist) continue;
    if (dist < static_cast<double>(static_cast<float>(6))) return 1;  // cXObject::GetPersonWidth: gPersonWidth 6
    if (!tested) {
      int a = 0, b = 0;
      DirectionStep(door->GetData(1) & 7, &a, &b);
      axisA = static_cast<float>(static_cast<double>(static_cast<float>(b << 4)) * 0.0625);
      axisB = static_cast<float>(static_cast<double>(static_cast<float>(a << 4)) * 0.0625);
      tested = true;
    }
    const float along = static_cast<float>(static_cast<double>(dA * axisA + dB * axisB) / dist);
    if (along < 0.866f && -0.866f < along) continue;
    SAnimator2* anim = p->GetSAnimator();
    const double dir = anim ? anim->GetRealDir() : 0.0;
    const float turned = static_cast<float>(dir + 1.5707963705062866);
    const float s = std::sin(turned), c = std::cos(turned);
    if (static_cast<float>(static_cast<double>(s * -dA + c * -dB) / dist) < 0.866f) continue;
    return 1;
  }
  return 0;
}

// 800F9F44
// Three ticks: the simulator steps once (global 0xB); then (the stack object must exist, error 0x17) the
// value (operand +0: literal, or with flag 0x20 of +4 the local it names) is read, GlobalDispatch 0x108
// unless flag 0x40 (nothing in the game listens to it), and the frame's tree's string set 0x131 entry
// +5 is formatted (ParseUIString; the text is not used); then true.
int cXObjectImpl::TryUserEvent(StackElem* frame, const unsigned char* params) {
  if (frame->m_primState == 0) {
    if (Globs_pSimulator) Globs_pSimulator->SetGlobal(cSimulatorImpl::kStepOnce, 1);
    frame->m_primState = 1;
    return 2;
  }
  if (frame->m_primState != 1) return 1;
  frame->m_primState = 2;
  if (!GetObjectFromID(frame->m_stackObject)) {
    m_treeSim->SetError(0x17);
    Error(0x17);
    return -1;
  }
  short value = 0;
  if (InterpValue((params[4] & 0x20) ? 0x19 : 7, Be16(params, 0), nullptr, nullptr, &value) == -1) return -1;
  return 2;
}

// 800FE068
// Operand +4: whose (1 the stack object's, 2 none: error 0x0A, else mine); +6 below 3 (else error 0x1E).
// My temps 0..4: the five lowest of the motives 5, 6, 7, 8, 9, 0xD, 0xE, 0xF (sorted by the game's qsort
// with _MotiveSort, which counts values within 0.01 as equal).
int cXObjectImpl::TryFind5WorstMotives(StackElem* frame, const unsigned char* params) {
  const unsigned int whose = simmy::be16(params + 4), kind = simmy::be16(params + 6);
  struct Entry {
    short index;
    short motive;
  };
  Entry motives[8] = {{0, 5}, {0, 6}, {0, 7}, {0, 8}, {0, 9}, {0, 0xd}, {0, 0xe}, {0, 0xf}};  // 803DC0A0
  cXObjectImpl* o = nullptr;
  if (whose == 1) {
    cXObject* so = GetObjectFromID(frame->m_stackObject);
    o = so ? so->GetObjectImplementation() : nullptr;
  } else if (whose != 2) {
    o = this;
  }
  cXPersonImpl* p = o && o->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(o) : nullptr;
  if (!p) {
    m_treeSim->SetError(10);
    Error(10);
    return -1;
  }
  if (kind >= 3) {
    m_treeSim->SetError(0x1e);
    Error(0x1e);
    return -1;
  }
  const float* values = p->GetMotiveRef(0);  // sCurMotiveTabToSort
  simmy::SnQsort(motives, 8, [values](const Entry& a, const Entry& b) {  // _MotiveSort (800FE014)
    const float d = values[a.motive] - values[b.motive];
    if (d < -0.01f) return -1;
    return 0.01f < d ? 1 : 0;
  });
  for (int i = 0; i < 5; ++i) m_temps[i] = motives[i].motive;
  return 1;
}

// 801069B4
// The GUID (an ingredient id): operand +1 the scope, +2 the data; none (0) with flag 0x80 of +4.
int cXObjectImpl::InventoryCommandParam_GetGuid(const unsigned char* params) {
  if (params[4] & 0x80) return 0;
  short v = -1;
  if (InterpValue(params[1], Be16(params, 1), nullptr, nullptr, &v) == -1) return 0;
  return v;
}

// 80106A28
// The count: operand +5 the scope, +6 the data.
int cXObjectImpl::InventoryCommandParam_GetCount(const unsigned char* params) {
  short v = 0;
  if (InterpValue(static_cast<signed char>(params[5]), Be16(params, 3), nullptr, nullptr, &v) == -1) return 0;
  return v;
}

// 80106A8C
// The player's family inventory, from a person's tree (an object's: error 0x4D); operand +4 & 0x3F: 0 whether the GUID is in it (true with the
// "all items" cheat, DAT_804761BC), 1 my temp 0 = its count, 2 nothing (false), 3 add the count
// (temp 0: those added), 4 remove (temp 0: those removed), 5 empty it. Errors: no GUID 0x4F, a count
// below 1 0x4E. False without an inventory.
int cXObjectImpl::TryInventoryCommand(StackElem*, const unsigned char* params) {
  auto fail = [&](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  // TreeSim::GetPerson: only a person's tree may use the inventory.
  if (GetType() != 2 || !dynamic_cast<cXPersonImpl*>(this)) return fail(0x4d);
  PlayerFamilyImpl* inv = inventory::PlayerInventory();
  if (!inv) return 0;
  switch (params[4] & 0x3f) {
    case 0: {
      const int guid = InventoryCommandParam_GetGuid(params);
      if (guid == 0) return fail(0x4f);
      // DAT_804761BC: a cheat (all ingredients) the release build never sets.
      return inventory::IsItemInInventory(inv, guid) ? 1 : 0;
    }
    case 1: {
      const int guid = InventoryCommandParam_GetGuid(params);
      if (guid == 0) return fail(0x4f);
      m_temps[0] = static_cast<short>(inventory::CountItemsByGuid(inv, guid));
      return 1;
    }
    case 3: {
      const int guid = InventoryCommandParam_GetGuid(params);
      const short count = static_cast<short>(InventoryCommandParam_GetCount(params));
      if (count < 1) return fail(0x4e);
      m_temps[0] = static_cast<short>(inventory::AddItemsByGuid(inv, guid, count, 0));
      return 1;
    }
    case 4: {
      const int guid = InventoryCommandParam_GetGuid(params);
      const short count = static_cast<short>(InventoryCommandParam_GetCount(params));
      if (count < 1) return fail(0x4e);
      m_temps[0] = static_cast<short>(inventory::RemoveItemsByGuid(inv, guid, count, 0));
      return 1;
    }
    case 5:
      inventory::Clear(inv);
      return 1;
    default: return 0;
  }
}

// 80100C84
// Operand +4 flag 1: the object folder frees what it no longer needs (PC: nothing to free), true.
// Else the GUID (+0) has its data loaded (ObjectFolderImpl::ForceDataPreload; 2 while it loads). PC:
// object data is read when it is needed, so a known GUID is ready at once; an unknown one is false.
int cXObjectImpl::TryPreloadObject(StackElem*, const unsigned char* params) {
  if (simmy::be16(params + 4) & 1) return 1;  // ObjectFolderImpl::FreeUnusedData
  const unsigned int guid = simmy::be32(params);
  if (guid == 0 || !Globs_pObjectFolder || !Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(guid))) return 0;
  return 1;
}

// 80105F54
// Operand +0 the effect, +1 flags (1 every other room, 2 the stack object's room instead of mine), two
// values (scope +2 / data +4, scope +3 / data +6). 0 the room's light scale (value / 512), 1 one colour
// channel (the first value, below 3) to the second / 512, 2 lightning (EHouse::TriggerLightningEffect:
// empty in the release build), 3 rain, 4 fair weather. Rooms with bit 0x8000 (outside): false. Errors:
// no such room 0x28, a channel above 2 0x0D.
int cXObjectImpl::TryRoomEffect(StackElem* frame, const unsigned char* params) {
  const unsigned char flags = params[1];
  const bool others = (flags & 1) != 0;
  const unsigned int effect = params[0];
  unsigned int room = static_cast<unsigned short>(GetData(0x1d));
  cXObject* so = GetObjectFromID(frame->m_stackObject);
  if (so && (flags >> 1 & 1)) room = static_cast<unsigned short>(so->GetObjectImplementation()->GetData(0x1d));
  short a = 0, b = 0;
  short* ra = nullptr;
  short* rb = nullptr;
  float* fa = nullptr;
  float* fb = nullptr;
  if (InterpValue(static_cast<signed char>(params[2]), Be16(params, 2), &ra, &fa, &a) == -1) return -1;
  if (InterpValue(static_cast<signed char>(params[3]), Be16(params, 3), &rb, &fb, &b) == -1) return -1;
  const float va = ra ? static_cast<float>(*ra) : fa ? *fa : static_cast<float>(a);
  const float vb = rb ? static_cast<float>(*rb) : fb ? *fb : static_cast<float>(b);
  if (room & 0x8000) return 0;
  RoomManager* manager = RoomManager::GetRoomManager();
  if (!manager || !manager->GetRoom(static_cast<unsigned short>(room))) {
    m_treeSim->SetError(0x28);
    Error(0x28);
    return -1;
  }
  EHouse* house = _globals.m_house;
  switch (effect) {
    case 0: RoomManager::SetRoomIntensityScale(static_cast<int>(room), others, static_cast<float>(va * 0.001953125)); break;
    case 1:
      if (3.0 <= va) {
        m_treeSim->SetError(0xd);
        Error(0xd);
        return -1;
      }
      RoomManager::SetRoomIntensityColorScale(static_cast<int>(room), others, static_cast<int>(va), static_cast<float>(vb * 0.001953125));
      break;
    case 2:
      if (house) EHouse::TriggerLightningEffect();
      break;
    case 3:
      if (house) house->Rain();
      break;
    case 4:
      if (house) house->Fair();
      break;
    default: return 0;
  }
  return 1;
}

// 80106568
// True only in a one-player game with EyeToy hardware (EyeToyClient::IsEyeToyHardwarePresent; the GBA
// link is checked through it). PC: no EyeToy, so false, as on a GameCube with nothing connected.
int cXObjectImpl::TryGbaCommunication(StackElem*, const unsigned char*) { return 0; }

// 80108EC8
// Operands: +0 / +2 the variable naming the object; +3 flags (bit 2: it influences objects of type +3,
// else people; 1 / 2 see PassiveInfluenceSystem::NotifySimulator); +4 the shape (5: removed); +5 a combo
// (GetShortComboValue) giving the range in tiles (at most 0x20). True.
int cXObjectImpl::TryPassiveInfluence(StackElem* frame, const unsigned char* params) {
  short id = 0;
  if (InterpValue(static_cast<signed char>(params[2]), Be16(params, 0), nullptr, nullptr, &id) == -1) {
    frame->m_stackObject = 0;
    return -1;
  }
  cXObject* object = GetObjectFromID(id);
  PassiveInfluenceSystem* system = InfluenceSystem();
  if (static_cast<signed char>(params[4]) == 5) {
    if (system) system->RemoveObjectFromMap(object);
    return 1;
  }
  short scope = 0, data = 0, range = 0;
  GetShortComboValue(static_cast<signed char>(params[5]), &scope, &data);
  if (InterpValue(scope, data, nullptr, nullptr, &range) == -1) return -1;
  if (range > 0x20) range = 0x20;
  if (system)
    system->AddObjectToMap(object, static_cast<signed char>(params[3]), static_cast<signed char>(params[4]),
                           static_cast<signed char>(range), static_cast<signed char>(params[3]));
  return 1;
}

// 80109344
// An object's passive-influence motive delta. Operand +3 flags: 2 mine, 4 the stack object's (neither:
// -1); 1 clears them. Else +2 the motive (4..9, 0xE, 0xF; else error 0x1A), the delta a game hour
// (scope +0 / data +4; per frame: / 60 / 30) and its limit (scope +1 / data +6).
int cXObjectImpl::TrySetMotiveDelta(StackElem* frame, const unsigned char* params) {
  const unsigned char flags = params[3];
  if ((flags & 6) == 0) return -1;
  auto target = [&]() -> cXObject* {
    if (flags & 2) return this;
    if (flags & 4) return GetObjectFromID(frame->m_stackObject);
    return nullptr;
  };
  PassiveInfluenceSystem* system = InfluenceSystem();
  if (flags & 1) {
    if (system) system->ClearInfluenceMotiveDeltas(target());
    return 1;
  }
  const unsigned int motive = params[2];
  if (motive < 4 || (motive > 9 && (motive > 0xf || motive < 0xe))) {
    m_treeSim->SetError(0x1a);
    Error(0x1a);
    return -1;
  }
  short delta = 0, limit = 0;
  if (InterpValue(static_cast<signed char>(params[0]), Be16(params, 2), nullptr, nullptr, &delta) == -1) return -1;
  if (InterpValue(static_cast<signed char>(params[1]), Be16(params, 3), nullptr, nullptr, &limit) == -1) return -1;
  const float perFrame = static_cast<float>(static_cast<double>(static_cast<float>(delta)) / 60.0 / 30.0);
  if (system) system->SetInfluenceMotiveDelta(target(), static_cast<unsigned char>(motive), perFrame, limit);
  return 1;
}

// 8010958C
// The animate primitive on an object: only its passive influence's reaction (kinds 7 mine, 8 the stack
// object's; operand +3 = kind - 3, or from the flags of +5), else error 5. The animation
// (resolveSkillForPassiveInfluence) is kept for the influence (SetSkillNameID).
int cXObjectImpl::TryAnimate(StackElem* frame, const unsigned char* params) {
  int kind = params[3];
  if (kind == 0) {
    kind = (params[5] & 1) != 0 ? 1 : 0;
    if (params[5] & 0x10) kind |= 2;
  } else {
    kind += 3;
  }
  cXObject* object = nullptr;
  if (kind == 7) {
    object = this;
  } else if (kind == 8) {
    object = GetObjectFromID(frame->m_stackObject);
  } else {
    m_treeSim->SetError(5);
    Error(5);
    return -1;
  }
  const AnimRef* skill = nullptr;
  const int r = resolveSkillForPassiveInfluence(object, frame, params, skill);
  if (object)
    if (PassiveInfluenceSystem* system = InfluenceSystem()) system->SetSkillNameID(object, skill);
  return r;
}

// 800FF2F8
// A fire (efx_fire) at the object's tile centre, unless one is there already or it cannot be placed.
// 1: started.
int StartFireAtObjectLoc(cXObject* object, ObjSelector* fire) {
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!impl || !fire || !module) return 0;
  CTilePt tile = impl->GetCTilePt();
  ObjectIterator it;
  for (it.init(tile, ObjectIterator__IterateType{ObjectIterator__IterateType::kAll}); it.m_current; ++it)
    if (it.m_current->GetSelector() == fire) return 0;
  const short id = static_cast<short>(module->MakeNewOutOfWorldObject(fire, nullptr));
  if (id == 0) return 0;
  cXObject* made = module->GetObjectFromID(id);
  cXObjectImpl* f = made ? made->GetObjectImplementation() : nullptr;
  if (!f) return 0;
  const int pos[2] = {(impl->m_location[0] & ~0xf) | 8, (impl->m_location[1] & ~0xf) | 8};
  if (ObjCanPlace(f, pos, impl->m_level, nullptr, 0)) {
    ObjPlace(f, pos, impl->m_level, nullptr, 0);
    if (std::getenv("SIMMY_SIM_TRACE")) {
      const std::string fireName = f->DebugName();
      std::printf("[vm] fire %s starts at %s\n", fireName.c_str(), impl->DebugName());
    }
    return 1;
  }
  module->KillObject(id);
  return 0;
}

// 800FF520
// Fire spreads to the stack object's tile (operand +0 = 1: the tile in front of it, through no wall but
// a low one (styles 3, 5, 6, 0xF, 0x17) and no diagonal). Not off the lot, not onto a fireproof object
// or a fire, at most 30 fires. On a tile with nothing that burns (object data 0x28 bit 0x20) a fire is
// put there; else the last thing that burns catches fire (unless operand +1 bit 1 is clear and it is in
// use, object data 8 bit 0x20, or occupied, data 0x3E): a person is let go of everything and stood in
// the middle of its tile first; a multi-tile object (not a dynamic one) burns on every part. 1: a fire
// started. Errors: no stack object 0x15, the fire not loaded 0x45 (PC: object data is always ready).
int cXObjectImpl::TryBurn(StackElem* frame, const unsigned char* params) {
  cXObject* so = GetObjectFromID(frame->m_stackObject);
  cXObjectImpl* target = so ? so->GetObjectImplementation() : nullptr;
  if (!target) {
    m_treeSim->SetError(0x15);
    Error(0x15);
    return -1;
  }
  if (!ObjIsInWorld(target)) return 0;
  ObjSelector* fire = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(kFireGUID)) : nullptr;
  int pos[2] = {target->m_location[0], target->m_location[1]};
  const int level = target->m_level;
  if (params[0] == 1) {
    int a = 0, b = 0;
    DirectionStep(target->GetData(1) & 7, &a, &b);
    pos[0] += a * 16;
    pos[1] += b * 16;
  }
  CTilePt tile(pos[1] >> 4, pos[0] >> 4, level);  // CTilePt(FTilePt&, level)
  cFixedWorldImpl* world = Globs_pFixedWorld;
  if (!world || world->OutOfBounds(tile)) return 0;
  if (params[0] == 1) {
    TileWalls walls = world->GetWall(tile);
    const TileWallsSegment segment{static_cast<int>(TileWalls::RotateSegment(TileWallsSegment{8}, target->GetData(1) / 2))};
    if (walls.HasWall(segment)) {
      const int style = static_cast<int>(walls.GetStyle(segment));
      if (style != 3 && style != 5 && style != 6 && style != 0xf && style != 0x17) return 0;
    }
    if (walls.HasDiagonal()) return 0;
  }
  cXObjectImpl* burn = nullptr;
  ObjectIterator it;
  for (it.init(tile, ObjectIterator__IterateType{ObjectIterator__IterateType::kAll}); it.m_current; ++it) {
    cXObjectImpl* o = it.m_current->GetObjectImplementation();
    if (!o) continue;
    if (o->GetData(0x28) >> 5 & 1) burn = o;                                    // CanBurn
    if ((o->GetData(8) >> 0xb & 1) || o->GetSelector() == fire) return 0;     // IsFireproof
  }
  int fires = 0;
  for (cXObjectImpl* o = Globs_pObjectModule ? Globs_pObjectModule->m_first : nullptr; o; o = o->m_nextObject)
    if (o->m_def && objdef::GUID(o->m_def) == kFireGUID) ++fires;
  if (fires + 1 >= 0x1f) return 0;
  if (!burn) {
    if (!fire) return 0;  // GC: ObjSelector::IsPreloaded (error 0x45); PC: always loaded
    ObjectModuleImpl* module = Globs_pObjectModule;
    const short id = static_cast<short>(module->MakeNewOutOfWorldObject(fire, nullptr));
    if (id == 0) return 0;
    cXObject* made = module->GetObjectFromID(id);
    cXObjectImpl* f = made ? made->GetObjectImplementation() : nullptr;
    if (f && ObjCanPlace(f, pos, level, nullptr, 0)) {
      ObjPlace(f, pos, level, nullptr, 0);
      if (std::getenv("SIMMY_SIM_TRACE")) {
        const std::string me = DebugName();
        std::printf("[vm] %s: fire %s spreads\n", me.c_str(), f->DebugName());
      }
      return 1;
    }
    module->KillObject(id);
    return 0;
  }
  if (!(burn->GetData(0x28) >> 5 & 1)) return 0;
  if (!(params[1] & 1) && ((burn->GetData(8) >> 5 & 1) || burn->GetData(0x3e) >= 1)) return 0;  // IsOccupied
  if (!burn->IsMultiTile()) {
    if (burn->GetType() == 2) {
      // A person: let go of everything (its outfit kept), then stood in the middle of its tile.
      cXPersonImpl* person = dynamic_cast<cXPersonImpl*>(burn);
      const int at[2] = {(burn->m_location[0] & ~0xf) | 8, (burn->m_location[1] & ~0xf) | 8};
      const int personLevel = burn->m_level;
      SAnimator2* anim = person ? person->GetSAnimator() : nullptr;
      const int costume = anim ? anim->m_lastCostume : 0;
      burn->Cleanup(nullptr);
      if (anim) anim->m_lastCostume = costume;
      PlaceObject(burn, at, personLevel, nullptr);  // cXPersonImpl::Place
      if (anim) anim->m_resetPos = 1;
    }
    return StartFireAtObjectLoc(burn, fire);
  }
  auto* mt = dynamic_cast<cXMTObjectImpl*>(burn);
  if (!mt || mt->IsDynamic()) return 0;
  int parts = 0;
  for (cXMTObjectImpl* p = mt->GetFirstMultiTileObject(); p; p = p->GetNextMultiTileObject()) ++parts;
  if (parts + fires >= 0x1f) return 0;
  bool started = false;
  for (cXMTObjectImpl* p = mt->GetFirstMultiTileObject(); p; p = p->GetNextMultiTileObject())
    if (StartFireAtObjectLoc(p, fire) == 1) started = true;
  return started ? 1 : 0;
}
