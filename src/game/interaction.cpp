// Port of game/interaction.cpp and the ActionQueue part of game/person.cpp: interactions (an object's
// tree table entry chosen for a person) and a person's queue of them. Not ported: names (BString2,
// SetName), streaming, text colours, the HUD's queue icons (AddActionToHUD / RemoveActionFromHUD).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

namespace {
int sNextInteractionID = 0;  // 804FDC08

// The tree table (TTAB) entry `index` of `object` (its ResFile +0x24: the tables, the first one's
// entries, 0x24 bytes each). GC: cXObject::GetTreeTab (vtable 0x364) then ITreeTable::GetEntry (0x8C).
const unsigned char* TreeTabEntry(cXObject* object, int index) {
  cXObjectImpl* impl = object ? object->GetObjectImplementation() : nullptr;
  const unsigned char* file = impl ? SelectorTreeTabResFile(impl->m_selector) : nullptr;
  const auto* tables = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x24)) : nullptr;
  if (!tables || ERQuickdata::U32(tables - 4, 0) == 0) return nullptr;
  const auto* entries = static_cast<const unsigned char*>(ERQuickdata::Ptr(tables, 0));
  const unsigned int n = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
  if (index < 0 || static_cast<unsigned int>(index) >= n) return nullptr;
  return entries + index * 0x24;
}
}  // namespace

// 800C1068
Interaction& Interaction::operator=(const Interaction& other) {
  m_type = other.m_type;
  m_person = other.m_person;
  m_stackObject = other.m_stackObject;
  m_iconObject = other.m_iconObject;
  m_index = other.m_index;
  for (int i = 0; i < 4; ++i) m_stackVars[i] = other.m_stackVars[i];
  m_localVars = other.m_localVars;
  m_priority = other.m_priority;
  m_tree = other.m_tree;
  m_attenuation = other.m_attenuation;
  m_id = other.m_id;
  m_flags = other.m_flags;
  return *this;
}

// 800C1140
Interaction::Interaction() {}

// 800C11D0
Interaction::Interaction(const Interaction& other) { *this = other; }

// 800C122C
// GC: no entry lookup (the menu fills the tree in); PC: the entry's action tree too, so the menu stand-in
// can queue it directly.
Interaction::Interaction(cXPerson* person, cXObject* object, int index) {
  m_type = 2;
  m_priority = 100;
  m_person = person;
  if (object) {
    m_iconObject = object;
    m_index = index;
  }
  m_stackObject = object;
  if (const unsigned char* e = GetEntry()) m_tree = ERQuickdata::S16(e, 2);
}

// 801492EC CalculateAttenuationValue (treetab.cpp): how fast an advertisement fades with distance: 0 the
// entry's own value, 1 none, 2 low, 3 moderate, 4 high, else 0.02 (visitors have their own table).
// The values are the autonomy tuning's (AutonomyConstantsClient::UpdateConstants, autonomy.cpp).
float gLowAttenuation = 0.002f, gModerateAttenuation = 0.02f, gHighAttenuation = 0.1f;           // 804FDD94..
float gVisLowAttenuation = 0.002f, gVisModerateAttenuation = 0.02f, gVisHighAttenuation = 0.1f;  // 804FDDA0..
double CalculateAttenuationValue(float custom, bool visitor, int code) {
  switch (code) {
    case 0: return custom;
    case 1: return 0.0;
    case 2: return visitor ? gVisLowAttenuation : gLowAttenuation;
    case 3: return visitor ? gVisModerateAttenuation : gModerateAttenuation;
    case 4: return visitor ? gVisHighAttenuation : gHighAttenuation;
    default: return 0.019999999552965164;
  }
}

// 800C1574
// The attenuation is the entry's (TreeTableEntry::GetAttenuationValue 801493EC: +8 its own value, +0xC
// the code) for a visitor or not (cXPersonImpl::IsVisitor: person data 0x20).
Interaction::Interaction(cXPerson* person, cXObject* object, int index, int priority) {
  m_priority = priority;
  m_person = person;
  m_type = 0;
  if (object) {
    m_iconObject = object;
    m_index = index;
  }
  m_stackObject = object;
  const unsigned char* e = GetEntry();
  if (!e) {
    m_tree = 0;
    m_flags &= ~(kAutoFirstSelect | kMustRun);
    m_attenuation = 0.0f;
    return;
  }
  m_tree = ERQuickdata::S16(e, 2);
  const bool visitor = person && person->GetPersonData(0x20) != 0;
  m_attenuation = static_cast<float>(CalculateAttenuationValue(ERQuickdata::F32(e, 8), visitor, ERQuickdata::S32(e, 0xc)));
  const unsigned int entryFlags = ERQuickdata::U32(e, 0x10);
  m_flags &= ~kAutoFirstSelect;
  if (entryFlags & 0x100) m_flags |= kAutoFirstSelect;
  m_flags &= ~kMustRun;
  if (entryFlags & 0x800) m_flags |= kMustRun;
}

// 800C171C
Interaction::~Interaction() {}

// 800C1780
void Interaction::SetStackVars(short* vars) {
  for (int i = 0; i < 4; ++i) m_stackVars[i] = vars[i];
}

// 800C182C
const unsigned char* Interaction::GetEntry() const { return TreeTabEntry(m_stackObject, m_index); }

// 800C189C
void Interaction::SetUniqueID() { m_id = ++sNextInteractionID; }

// 800C1A68
void Interaction::DoStream(ReconBuffer* b, int) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  b->ReconInt(&m_type, 1);
  b->Recon32(&m_id, 1);
  cXObject* personObject = m_person ? m_person->AsObject() : nullptr;
  cXPersonImpl* person = personObject ? dynamic_cast<cXPersonImpl*>(personObject->GetObjectImplementation()) : nullptr;
  cXObjectImpl* stack = m_stackObject ? m_stackObject->GetObjectImplementation() : nullptr;
  cXObjectImpl* icon = m_iconObject ? m_iconObject->GetObjectImplementation() : nullptr;
  if (module) {
    module->DoReconPerson(b, &person);
    module->DoReconObject(b, &stack);
    module->DoReconObject(b, &icon);
  }
  m_person = person;
  m_stackObject = stack;
  m_iconObject = icon;
  b->ReconInt(&m_index, 1);
  b->Recon16(m_stackVars, 4);
  b->ReconInt(&m_priority, 1);
  b->Recon16(&m_tree, 1);
  b->ReconFloat(&m_attenuation, 1);
  int flags = static_cast<int>(m_flags);
  b->ReconInt(&flags, 1);
  m_flags = static_cast<unsigned int>(flags);
  if (sNextInteractionID < m_id) sNextInteractionID = m_id;
}

// 800C1C74 Interaction::GetStackObject (inline in the header)
// 800C1C7C Interaction::GetPerson (inline)
// 800C1C84 Interaction::GetIconObject (inline)
// 800C1C8C Interaction::SetIconObject (inline)
// 800C1D44 Interaction::GetType (inline)
// 800C1D4C Interaction::GetAttenuation (inline)
// 800C1D54 Interaction::GetTreeID (inline)
// 800C1D5C Interaction::GetStackVars (inline)
// 800C1D74 Interaction::GetTreeTabEntryIndex (inline)
// 800C1D7C Interaction::GetPriority (inline)
// 800C1D84 Interaction::SetPriority (inline)
// 800C1DA4 Interaction::GetID (inline)
// 800C1DAC Interaction::SetID (inline)
// 800C1FF0 Interaction::GetFlags (inline)
// 800C1FF8 Interaction::SetFlags (inline)

const unsigned short* Interaction::NameText() const {
  const unsigned char* e = GetEntry();
  return e ? ERQuickdata::LocString(e, 0x1c) : nullptr;
}

// ---------------------------------------------------------------------------------------------------
// ActionQueue

// 80119E50
ActionQueue::ActionQueue() {}

// 80119EB0
// The visible actions, one per unique id.
int ActionQueue::CountVisible() const {
  int last = 0, count = 0;
  for (unsigned int i = 0; i < Count(); ++i) {
    const Interaction& a = At(i);
    if (IsVisibleAction(a) && last != a.m_id) {
      last = static_cast<short>(a.m_id);
      ++count;
    }
  }
  return count;
}

// 8011A210
int ActionQueue::IsVisibleAction(const Interaction& action) const {
  int visible = action.GetIconObject() && !(action.m_flags & Interaction::kHidden) ? 1 : 0;
  if (visible && (action.m_flags & Interaction::kContinuation)) {
    if (m_tail == m_head) visible = 0;
    else if (&action != &m_actions[m_head % 10]) visible = 0;
  }
  return visible;
}

// 8011A35C
int ActionQueue::UpdateAction(unsigned int index) {
  if (index >= Count()) return 0;
  if (!IsVisibleAction(At(index))) RemoveActionFromHUD(static_cast<int>(index));
  else AddActionToHUD(static_cast<int>(index));
  return 1;
}

// 8011A3F8
int ActionQueue::Insert(Interaction& action, unsigned int index) {
  unsigned int last = m_tail;
  if (m_tail - m_head >= 10) return 0;
  for (unsigned int i = m_tail; m_head + index < i; i = last) {
    last = i - 1;
    m_actions[i % 10] = m_actions[last % 10];
  }
  m_actions[last % 10] = action;
  ++m_tail;
  if (m_actions[(m_head + index) % 10].m_id == 0) m_actions[(m_head + index) % 10].SetUniqueID();
  AddActionToHUD(static_cast<int>(index));
  return 1;
}

// 8011A538
int ActionQueue::Remove(unsigned int index) {
  if (index >= Count()) return 0;
  RemoveActionFromHUD(static_cast<int>(index));
  const int visible = IsVisibleAction(At(index));
  if (index != 0) {
    unsigned int i = m_head + index;
    if (m_tail < i + 1) return 0;
    for (int n = static_cast<int>(m_tail - m_head - index); --n != 0; ++i) m_actions[i % 10] = m_actions[(i + 1) % 10];
    --m_tail;
  } else {
    ++m_head;
  }
  if (index == 0 && m_tail != m_head) {
    Interaction& head = m_actions[m_head % 10];
    if ((head.m_flags & Interaction::kContinuation) && visible) {
      head.m_flags &= ~Interaction::kHidden;
      AddActionToHUD(0);
    }
  }
  return 1;
}

// 8011A6E4
void ActionQueue::Enqueue(Interaction& action) { Insert(action, Count()); }
