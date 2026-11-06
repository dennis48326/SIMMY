// Port of game/objtestsim.cpp (part): ObjTestSim, the test of one interaction for a person. The menu
// and autonomy lists (AppendInteractionsForMenu / ForAuto, RunMenuCheckTree, MakeNewMenuItem) are
// still done by the PC stand-ins in flashpimenu.cpp, autonomy.cpp and objectsim.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
cXPersonImpl* PersonImpl(cXPerson* person) {
  cXObject* o = person ? person->AsObject() : nullptr;
  return o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
}

// cXObjectImpl::IsBurning (800EB5F8): object data 8 bit 0x200.
bool ObjectIsBurning(cXObjectImpl* o) { return (static_cast<unsigned short>(o->GetData(8)) >> 9 & 1) != 0; }

// cXObject::GetTreeTab (vtable 0x364) -> TreeTableQuickData::GetPrefixCheckTreeID (8015E92C): the
// object's tree table (ResFile +0x24's first table) +8.
short PrefixCheckTree(cXObjectImpl* o) {
  const unsigned char* file = SelectorTreeTabResFile(o->m_selector);
  const auto* tables = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x24)) : nullptr;
  if (!tables || ERQuickdata::U32(tables - 4, 0) == 0) return 0;
  return ERQuickdata::S16(tables, 8);
}
}  // namespace

// 80112048
ObjTestSim::ObjTestSim(cXPerson* person) : m_person(person) {}

// 80112060
ObjTestSim::ObjTestSim(cXPerson* person, cXObject* object) : m_person(person) { SetStackObject(object); }

// 801120A4
// The person's temps as they were before the first check tree ran.
ObjTestSim::~ObjTestSim() {
  if (!m_tempsOwner) return;
  for (int i = 0; i < 8; ++i) m_tempsOwner->SetTemp(i, m_savedTemps[i]);
  m_tempsOwner = nullptr;
}

// 80112138
void ObjTestSim::SetStackObject(cXObject* object) {
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  if (impl) impl = impl->GetInteractionLeader();
  m_stackObject = impl;
}

// 80112188
void ObjTestSim::TestInteraction(Interaction* action, bool autonomous, bool prefix, bool check, TTabScratchEntry** mod) {
  action->m_flags = (action->m_flags & ~(Interaction::kChecked | Interaction::kAvailable)) | Interaction::kChecked;
  cXPersonImpl* person = PersonImpl(action->GetPerson());
  cXObject* stack = action->GetStackObject();
  if (!stack) return;
  const unsigned char* e = action->GetEntry();
  if (!e) return;
  cXObjectImpl* object = stack->GetObjectImplementation();
  if (!person || !object) return;  // PC: GC dereferences them
  const unsigned int entryFlags = ERQuickdata::U32(e, 0x10);
  if (prefix && (entryFlags & 0x400)) prefix = false;  // GetIgnorePrefixTree
  // The check trees' params: autonomous, a pushed interaction (type 1).
  short params[4] = {static_cast<short>(autonomous ? 1 : 0), static_cast<short>(action->m_type == 1 ? 1 : 0), 0, 0};
  int ok = 1;
  person->m_data.WriteVar(0x32, 0);
  if (ObjectIsBurning(object) || (autonomous && object->GetData(0x19) > 0)) ok = 0;
  // GetAvailableToGhosts (flag 0x10) / GetAvailableToAdults (flag 0x40 clear).
  const bool forMe = person->IsGhost() ? (entryFlags & 0x10) != 0 : (entryFlags & 0x40) == 0;
  if (!forMe) ok = 0;
  bool noColours = !check;
  if (ok) {
    if (mod) SIMMY_STUB("ObjTestSim::TestInteraction: the check-tree advertisement copy (sCheckTreeModEntry)", 0x80112188);
    const short checkTree = check ? ERQuickdata::S16(e, 0) : 0;  // GetCheckTreeID
    if (checkTree == 0) noColours = true;
    if (checkTree == 0 && !prefix) {
      ok = 1;
    } else {
      if (!m_tempsOwner) {
        m_tempsOwner = person;
        for (int i = 0; i < 8; ++i) m_savedTemps[i] = person->m_temps[i];
      }
      TreeSimImpl* sim = person->m_treeSim;
      if (!sim) {
        ok = 0;
      } else {
        if (prefix) {
          if (const short p = PrefixCheckTree(object))
            ok = sim->RunCheckTree(object->GetBehavior(), p, static_cast<short>(object->m_id), params);
        }
        if (ok && checkTree != 0 && !noColours)
          ok = sim->RunCheckTree(object->GetBehavior(), checkTree, static_cast<short>(object->m_id), params);
      }
      if (action->m_flags & Interaction::kDebugOverride) {
        ok = 1;
        person->GetData(0x32);
        person->m_data.WriteVar(0x32, 0);
      }
    }
  }
  action->m_flags &= ~Interaction::kAvailable;
  if (ok) action->m_flags |= Interaction::kAvailable;
  if (!noColours) SetupInteractionColors(action, static_cast<short>(person->GetData(0x32)));
}

// 801126E0
// Person data 0x32 from the check tree: 1 hides the entry (0x10), 2 greys it (0x800).
void ObjTestSim::SetupInteractionColors(Interaction* action, short mode) {
  action->m_flags &= ~Interaction::kHidden;
  if (mode == 1) action->m_flags |= Interaction::kHidden;
  action->m_flags &= ~Interaction::kDisabled;
  if (mode == 2) action->m_flags |= Interaction::kDisabled;
}
