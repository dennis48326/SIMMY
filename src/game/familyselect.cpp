// Switching the player's selected sim within the family (game/global.cpp: EGlobal::AdvanceSelectedPerson
// and kin; game/directinteractor.cpp: the cancel timer; game/interactorutils.cpp:
// CheckNewSimInSocialMode; game/interactor.cpp: Interactor::SnapToPos).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>

int ObjIsInWorld(cXObjectImpl* o);  // 800DCB50 (objectplace.cpp)

namespace {
// PC stand-in for ObjectModuleImpl::GetPersonByGUID (800F4A14, objectmodule.cpp: the module's people
// whose selector has the GUID): the people by id.
cXPerson* PersonByGUID(ObjectModuleImpl* module, int guid) {
  const int count = module ? module->GetNumObjects() : 0;
  for (int i = 1; i <= count; ++i) {
    cXObject* o = module->GetObjectFromID(i);
    cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
    auto* p = impl && impl->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
    if (p && p->GetSelector() && static_cast<int>(p->GetSelector()->GetGUID()) == guid) return p;
  }
  return nullptr;
}

// PC stand-in for ObjectModuleImpl::IsSelectionSwitchingAllowed (800F9290, objectmodule.cpp): with a
// neighbourhood, in free play (DAT_80475C6C) or author mode (DAT_80475C74), or once the simulator has run
// more than 0x5B ticks.
bool SelectionSwitchingAllowed() {
  if (!Globs_pNeighborhood) return false;
  if (_gd.m_fieldE8 || _gd.m_fieldF0) return true;
  return Globs_pSimulator && Globs_pSimulator->GetTicks() > 0x5b;
}

// The person family member `index` stands for, if one of the family's members can be taken: its object
// is the object its data 0xB names, it is not the player's sim already, not away (misc flag 0x40), on
// the lot, not frozen (data 0x22), not the other player's and selectable by this player.
cXPerson* SelectableMember(EGlobal* g, FamilyImpl* family, int index, int player) {
  static const bool trace = std::getenv("SIMMY_SWITCH_TRACE") != nullptr;
  auto no = [&](const char* why) -> cXPerson* {
    if (trace) std::printf("[switch]   member %d: %s\n", index, why);
    return nullptr;
  };
  int* guid = family->GetIndexedMember(index);
  cXPerson* person = guid ? PersonByGUID(Globs_pObjectModule, *guid) : nullptr;
  if (!person) return no("not on the lot");
  auto* impl = dynamic_cast<cXPersonImpl*>(person);
  if (!impl) return no("no person");
  if (impl->m_id != impl->m_data[0xb]) return no("data 0xB is not its id");
  if (person == g->GetSelectedPerson(player)) return no("the player's sim");
  if (impl->GetMiscFlag(cXObject__MiscFlag{0x40})) return no("away (misc flag 0x40)");  // vtable 0x7C
  if (!ObjIsInWorld(impl)) return no("off the lot");                                    // vtable 0x1AC (IsInWorld)
  if (impl->GetData(0x22) != 0) return no("frozen (data 0x22)");                      // vtable 0x264
  if (g->SimInUseByOtherPlayer(player, person)) return no("the other player's");
  if (!impl->IsSelectableByPlayer(player)) return no("not selectable");
  return person;
}
}  // namespace

// 800407AC
bool EGlobal::SimInUseByOtherPlayer(int player, cXPerson* person) {
  return player != 0 ? person == m_selectedPerson[0] : person == m_selectedPerson[1];
}

// 80040A20
// The person's place among the family's members (-1: not one of them).
int EGlobal::GetFamilyIndexOfPerson(ObjectModuleImpl* module, Family* family, cXPerson* person) {
  if (!person) return -1;
  auto* f = static_cast<FamilyImpl*>(family);
  const int count = f->CountMembers();
  for (int i = 0; i < count; ++i) {
    int* guid = f->GetIndexedMember(i);
    if (guid && PersonByGUID(module, *guid) == person) return i;
  }
  return -1;
}

// 80040AE0
// The first member after `index` that can be taken becomes the player's sim.
bool EGlobal::SelectNextInFamily(Family* family, int index, int player) {
  auto* f = static_cast<FamilyImpl*>(family);
  const int count = f->CountMembers();
  for (int i = index + 1; i < count; ++i)
    if (cXPerson* p = SelectableMember(this, f, i, player)) {
      SetSelectedPerson(player, p);
      return true;
    }
  return false;
}

// 80040E7C
// The last member before `index` (-1: from the end) that can be taken becomes the player's sim.
bool EGlobal::SelectPrevInFamily(Family* family, int index, int player) {
  auto* f = static_cast<FamilyImpl*>(family);
  if (index == -1) index = f->CountMembers();
  for (int i = index - 1; i >= 0; --i)
    if (cXPerson* p = SelectableMember(this, f, i, player)) {
      SetSelectedPerson(player, p);
      return true;
    }
  return false;
}

// 80040CC8
// When switching is allowed: the next sim after the player's in its family, else the first that can
// be taken in each following family in turn (wrapping round to the same family); none: the sim stays.
void EGlobal::AdvanceSelectedPerson(int player) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module || !SelectionSwitchingAllowed()) return;
  cXPerson* current = m_selectedPerson[player];
  int index = -1;
  const int families = Globs_pNeighborhood->GetNumFamilies();
  int family = 0;
  if (current) {
    for (family = 0; family < families; ++family) {
      index = GetFamilyIndexOfPerson(module, Globs_pNeighborhood->GetFamilyByIndex(family), current);
      if (index >= 0) break;
    }
    if (index < 0) family = 0, index = -1;
  }
  // PC dev trace: SIMMY_SWITCH_TRACE=1 prints the family searched.
  static const bool trace = std::getenv("SIMMY_SWITCH_TRACE") != nullptr;
  if (trace)
    std::printf("[switch] player %d: family index %d (number %d, %d members), member %d\n", player + 1, family,
                Globs_pNeighborhood->GetFamilyByIndex(family) ? Globs_pNeighborhood->GetFamilyByIndex(family)->GetNumber() : -1,
                Globs_pNeighborhood->GetFamilyByIndex(family) ? Globs_pNeighborhood->GetFamilyByIndex(family)->CountMembers() : 0,
                index);
  if (SelectNextInFamily(Globs_pNeighborhood->GetFamilyByIndex(family), index, player)) return;
  for (int i = 1; i < families + 1; ++i)
    if (SelectNextInFamily(Globs_pNeighborhood->GetFamilyByIndex((family + i) % families), -1, player)) return;
  SetSelectedPerson(player, current);
}

// 80041068
// The same backwards.
void EGlobal::ReverseSelectedPerson(int player) {
  ObjectModuleImpl* module = Globs_pObjectModule;
  if (!module || !SelectionSwitchingAllowed()) return;
  cXPerson* current = m_selectedPerson[player];
  int index = -1;
  const int families = Globs_pNeighborhood->GetNumFamilies();
  int family = 0;
  for (family = 0; family < families; ++family) {
    index = GetFamilyIndexOfPerson(module, Globs_pNeighborhood->GetFamilyByIndex(family), current);
    if (index >= 0) break;
  }
  if (index < 0) family = 0, index = -1;
  if (SelectPrevInFamily(Globs_pNeighborhood->GetFamilyByIndex(family), index, player)) return;
  int f = family + families;
  for (int i = 1; i < families + 1; ++i) {
    --f;
    if (SelectPrevInFamily(Globs_pNeighborhood->GetFamilyByIndex(f % families), -1, player)) return;
  }
  SetSelectedPerson(player, current);
}

// 80213C60
// Two players: the player's new sim already has a social mode target (it is in one): it takes part as
// the second player. Else, when the other player's sim leads a social mode (first player) whose target
// is the new sim, the new sim joins it as the second player and waits for it. 1: the new sim is in a
// social mode (no cancel timer). (cXPerson vtables 0x36C GetSocialModeTarget, 0x34C
// IsFirstPlayerInSocialMode, 0x344 SetSecondPlayerInSocialMode, 0x364 SetWaitingForSocialMode.)
RET_UNKNOWN InteractorModule::CheckNewSimInSocialMode(unsigned int player) {
  if (_gd.m_fieldEC != 1) return 0;  // DAT_80475C70
  auto impl = [](cXPerson* p) -> cXPersonImpl* {
    cXObject* o = p ? p->AsObject() : nullptr;
    return o ? dynamic_cast<cXPersonImpl*>(o->GetObjectImplementation()) : nullptr;
  };
  cXPersonImpl* sim = impl(_globals.GetSelectedPerson(static_cast<int>(player)));
  if (!sim) return 0;
  if (sim->GetSocialModeTarget() != 0) {
    sim->SetSecondPlayerInSocialMode(true);
    return 1;
  }
  cXPersonImpl* other = impl(_globals.GetSelectedPerson(static_cast<int>((player + 1) & 1)));
  if (!other || !other->IsFirstPlayerInSocialMode()) return 0;
  if (reinterpret_cast<cXPersonImpl*>(other->GetSocialModeTarget()) != sim) return 0;
  sim->SetSecondPlayerInSocialMode(true);
  sim->SetWaitingForSocialMode(true);
  std::printf("[switch] player %u's new sim joins the other player's social mode\n", player + 1);
  return 1;
}

// 80204F0C
void InteractorModule__DirectInteractor::StartCancelTimer() {
  m_cancelTimerOn = 1;
  m_cancelTime = 0.0f;
}

// 80204F3C
// Half a second after a switch the new sim's queued actions go (all of them while it is routing, else
// all but the last).
void InteractorModule__DirectInteractor::CancelQueuedActions() {
  if (!m_person) return;
  auto* impl = dynamic_cast<cXPersonImpl*>(m_person);
  if (!impl) return;
  SAnimator2* anim = m_person->GetSAnimator();
  if (!anim || anim->m_routing != 1) impl->CancelAllButLastActions();
  else impl->CancelAllActions();
}

// 80209534
// The interactor is put back here by the next update (control status 2).
void InteractorModule__Interactor::SnapToPos(EVec3& pos) {
  m_savedPos = pos;
  m_restorePos = 1;
}
