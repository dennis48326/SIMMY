// Port of cXPersonImpl::TrySocialMode (game/person.cpp, primitive 0x3E) and the social-mode parts of the
// person (SetFirst/SecondPlayerInSocialMode, SetWaitingForSocialMode, Is*, GetSocialModeTarget,
// TryTestInteractingWith), with a PC stand-in for InteractorModule::SocialModeInteractor (interactor 7:
// the social menu and the chosen interaction; game/socialmodeinteractor.cpp).
//
// The social interactions (social_interactions_* objects) run on both sims' stacks: "Socialize" (person_sg)
// makes the social object, whose trees put the two sims in social mode (operand +6: 0 the player's sim
// starts it with the other, 4 the other joins, 2 the social menu: the person_sg tree table entries listed
// in the constants 0x200E + local, each check tree passing, shown as the player's pie menu; the choice
// (its entry) goes to temp 0, 3 leaves the menu (the other sim's turn), 1 ends social mode).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
// PC stand-in for the player's social-mode interactor (GC InteractorModule::SocialModeInteractor).
struct SocialInteractor {
  bool active = false;  // InteractorManager: interactor 7 is the player's
  short chosen = -1;    // +0x60 the chosen entry (-1 none yet, -2 backed out)
  int timeout = -1;     // the tick the menu gives up at (-1 never)
};
SocialInteractor s_interactor[2];
int s_autoPicks[2] = {};  // PC: SIMMY_SOCIAL_PICK choices in this social mode (SIMMY_SOCIAL_ROUNDS)

int PlayerOf(cXPersonImpl* p) {
  for (int i = 0; i < 2; ++i) {
    cXPerson* s = _globals.GetSelectedPerson(i);
    if (s && s->AsObject() == p) return i;
  }
  return -1;
}
cXPersonImpl* PersonFromID(short id) {
  cXObject* o = Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(id) : nullptr;
  cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
  return impl && impl->GetType() == 2 ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
}
int Ticks() { return Globs_pSimulator ? Globs_pSimulator->GetTicks() : 0; }
}  // namespace

// PC: InteractorModule::SocialModeInteractor::ChooseAction (8021F548) for a player in social mode, called
// by FlashPiMenu::ChooseAction / BackedOutOfMenu: the entry chosen (null: backed out). False when the
// player is not in social mode (the direct interactor takes the choice).
bool SocialModeChooseAction(int player, const MenuInteraction* m) {
  if (player < 0 || player > 1 || !s_interactor[player].active) return false;
  s_interactor[player].chosen = m ? static_cast<short>(m->entry) : static_cast<short>(-2);
  std::printf("[social] player %d chooses %d\n", player + 1, s_interactor[player].chosen);
  return true;
}
bool PlayerInSocialMode(int player) { return player >= 0 && player < 2 && s_interactor[player].active; }

// 8012B82C
void cXPersonImpl::SetFirstPlayerInSocialMode(bool on) { m_firstInSocialMode = on ? 1 : 0; }
// 8012B834
// The player whose selected sim this is gets its direct interactor's idle time reset (GC:
// InteractorManager::GetPlayerInteractor(player, 0), vtable 0x84 ResetIdleTime).
void cXPersonImpl::SetSecondPlayerInSocialMode(bool on) {
  for (int player = 0; player < 2; ++player) {
    cXPerson* s = _globals.GetSelectedPerson(player);  // DAT_80475E88
    if (!s || s->AsObject() != this) continue;
    if (InteractorModule__DirectInteractor* d = PlayerDirectInteractor(player)) d->ResetIdleTime();
  }
  m_secondInSocialMode = on ? 1 : 0;
}
// 8012B8C4
void cXPersonImpl::SetWaitingForSocialMode(bool on) { m_waitingForSocialMode = on ? 1 : 0; }
// 8012B8CC
// The player's sim went into social mode (it is the one that started it, or joined it: TrySocialMode
// steps 0/4) but runs no interaction any more (the Socialize was cancelled or failed before the social
// menu came up, e.g. while the partner was still busy): it leaves social mode and stands. Called by
// DirectInteractor::Update while IsFirstPlayerInSocialMode.
void cXPersonImpl::CheckFirstPlayerForFailedSocialModeEntry() {
  if (GetCurrentInteractionStackObject() != 0) return;
  SetFirstPlayerInSocialMode(false);
  SetPersonData(0x28, 0);
  if (m_animator) m_animator->StartDefaultIdle();
}
// 8012B978
// Two players: this sim was waiting to join the other player's social mode (CheckNewSimInSocialMode)
// but runs no interaction any more: both leave social mode, the other player's sim's actions are
// cancelled and both stand. Called by the direct and classic interactors' Update while
// IsWaitingForSocialMode.
void cXPersonImpl::CheckSecondPlayerForFailedSocialModeEntry() {
  if (_gd.m_fieldEC == 0) return;  // DAT_80475C70: two players
  cXPerson* first = _globals.GetSelectedPerson(0);
  const bool mine = first && first->AsObject() == this;
  cXPerson* other = _globals.GetSelectedPerson(mine ? 1 : 0);
  if (!other || GetCurrentInteractionStackObject() != 0) return;
  SetWaitingForSocialMode(false);
  SetSecondPlayerInSocialMode(false);
  if (m_animator) m_animator->StartDefaultIdle();
  SetPersonData(0x28, 0);
  cXObjectImpl* impl = other->AsObject() ? other->AsObject()->GetObjectImplementation() : nullptr;
  cXPersonImpl* p = impl ? dynamic_cast<cXPersonImpl*>(impl) : nullptr;
  if (!p) return;
  p->CancelAllActions();
  p->SetFirstPlayerInSocialMode(false);
  p->SetPersonData(0x28, 0);
  if (p->m_animator) p->m_animator->StartDefaultIdle();
}
// 8012BB20
// The person person data 0x28 names (the social partner), or null.
RET_UNKNOWN cXPersonImpl::GetSocialModeTarget() {
  if (GetPersonData(0x28) == 0) return 0;
  return reinterpret_cast<RET_UNKNOWN>(PersonFromID(GetPersonData(0x28)));
}
// 8012CA2C
// Person data 0x28 (the social partner's id) is set: true for both sims of a social mode.
RET_UNKNOWN cXPersonImpl::IsInSocialMode() { return GetPersonData(0x28) != 0 ? 1 : 0; }
// 8012CA44
RET_UNKNOWN cXPersonImpl::IsSecondPlayerInSocialMode() { return m_secondInSocialMode; }
// 8012CA4C
RET_UNKNOWN cXPersonImpl::IsFirstPlayerInSocialMode() { return m_firstInSocialMode; }
// 8012CA54
RET_UNKNOWN cXPersonImpl::IsWaitingForSocialMode() { return m_waitingForSocialMode; }

// 8012640C
// The stack object of the interaction it runs (the queue's head while it runs); 0 for none.
RET_UNKNOWN cXPersonImpl::GetCurrentInteractionStackObject() {
  if (m_hasCurrentAction == 0) return 0;
  return reinterpret_cast<RET_UNKNOWN>(m_actionQueue.At(0).GetStackObject());
}

// 80126640
// Whether one of the object trees it runs is the stack object's.
int cXPersonImpl::TryTestInteractingWith(StackElem* frame) {
  for (const GosubObject& g : m_gosubObjects)
    if (g.m_object && g.m_object->m_id == frame->m_stackObject) return 1;
  return 0;
}

// 801282E0
// Operands: +2 the temp with the menu's time limit (x20 ticks; <= 0 none), +4 the local naming the
// other sim (+7 bit 0; else the stack object) or the constants for the menu, +6 the step (above).
// GC: also the social camera and the interactor switch (InteractorManager pushes interactor 7 for the
// player), the idle states (vtable 0x74 4) and the plumb bob; PC: the player's SocialInteractor.
int cXPersonImpl::TrySocialMode(StackElem* frame, const unsigned char* params) {
  auto fail = [this](short code) {
    m_treeSim->SetError(code);
    Error(code);
    return -1;
  };
  if (!m_instance) return 2;  // GC: IsModelLoaded
  // Only a player's selected sim (else error 0x55).
  const int player = PlayerOf(this);
  if (player < 0) return fail(0x55);
  const int step = params[6];
  // Steps 1 and 2: the partner is the sim my person data 0x28 names, whose 0x28 names me.
  cXPersonImpl* partner = nullptr;
  if (step != 0 && step != 3 && step != 4) {
    const int id = GetPersonData(0x28);
    if (id == 0) return fail(0x56);
    partner = PersonFromID(static_cast<short>(id));
    if (!partner) return fail(0x1c);
    if (partner->GetPersonData(0x28) != m_id) return fail(0x56);
  }
  auto other = [&]() -> cXPersonImpl* {
    short id = frame->m_stackObject;
    if (params[7] & 1) {
      const unsigned int local = simmy::be16(params + 4);
      if (local >= frame->m_numLocals) return nullptr;
      id = frame->GetLocals()[local];
    }
    return PersonFromID(id);
  };
  switch (step) {
    case 0: {
      // The player's sim starts social mode with the other sim.
      cXPersonImpl* p = other();
      if (!p) return fail(0x1c);
      const int mine = GetPersonData(0x28), theirs = p->GetPersonData(0x28);
      if ((mine != 0 && mine != p->m_id) || theirs != 0) return fail(0x56);
      SetPersonData(0x28, p->m_id);
      SetFirstPlayerInSocialMode(true);
      p->SetPersonData(0x28, m_id);
      p->SetWaitingForSocialMode(false);
      if (player >= 0) {
        s_interactor[player].active = true;
        s_interactor[player].chosen = -1;
        s_autoPicks[player] = 0;
      }
      const std::string me = DebugName();
      std::printf("[social] %s starts social mode with %s (player %d)\n", me.c_str(), p->DebugName(), player + 1);
      return 1;
    }
    case 4: {
      // The other player's sim joins.
      cXPersonImpl* p = other();
      if (!p) return fail(0x1c);
      if (GetPersonData(0x28) != 0) return fail(0x56);
      SetPersonData(0x28, p->m_id);
      SetFirstPlayerInSocialMode(true);
      return 1;
    }
    case 3:
      // The menu's turn passes to the other sim.
      SetFirstPlayerInSocialMode(false);
      SetSecondPlayerInSocialMode(true);
      SetWaitingForSocialMode(true);
      if (player >= 0 && _gd.m_hudTarget) _gd.m_hudTarget->CloseMenu(player);
      return 1;
    case 1: {
      // Social mode ends for both.
      cXPersonImpl* p = partner;
      SetPersonData(0x28, 0);
      SetFirstPlayerInSocialMode(false);
      if (p) {
        p->SetPersonData(0x28, 0);
        p->SetSecondPlayerInSocialMode(false);
      }
      if (player >= 0) {
        s_interactor[player].active = false;
        if (_gd.m_hudTarget) _gd.m_hudTarget->CloseMenu(player);
      }
      std::printf("[social] %s leaves social mode\n", DebugName());
      return 1;
    }
    case 2: {
      if (!s_interactor[player].active) return 0;
      SocialInteractor& si = s_interactor[player];
      if (frame->m_primState == 2) {
        cXObject* so = GetObjectFromID(frame->m_stackObject);
        cXObjectImpl* stack = so ? so->GetObjectImplementation() : nullptr;
        const bool timedOut = si.timeout > 0 && si.timeout == Ticks();
        if (si.chosen == -2 || timedOut || (stack && stack->GetData(0xd) == 1)) {
          si.chosen = -1;
          frame->m_primState = 0;
          return 0;
        }
        if (si.chosen == -1) return 2;
        m_temps[0] = si.chosen;
        si.chosen = -1;
        frame->m_primState = 0;
        return 1;
      }
      // The menu: the other sim's tree table entries the constants list.
      const unsigned int t = simmy::be16(params + 2);
      if (t >= 8) return fail(2);
      si.timeout = m_temps[t] < 1 ? -1 : m_temps[t] * 20 + Ticks();
      const unsigned int local = simmy::be16(params + 4);
      if (local >= frame->m_numLocals) return fail(0x34);
      cXPersonImpl* p = partner;
      Behavior* mine = GetBehavior();
      const unsigned char* values = nullptr;
      int count = 0;
      if (!p || !mine || !mine->GetConstants(static_cast<short>(frame->GetLocals()[local] + 0x200e), &values, &count)) return 0;
      const unsigned char* file = SelectorTreeTabResFile(p->m_selector);
      const auto* tables = file ? static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x24)) : nullptr;
      const auto* entries = tables ? static_cast<const unsigned char*>(ERQuickdata::Ptr(tables, 0)) : nullptr;
      const unsigned int n = entries ? ERQuickdata::U32(entries - 4, 0) : 0;
      MenuInteractionList list;
      for (int i = 0; i < count; ++i) {
        const int index = static_cast<short>(simmy::be16(values + i * 8));
        if (index < 0 || static_cast<unsigned int>(index) >= n) continue;
        const unsigned char* e = entries + index * 0x24;
        // The entry's check tree on my stack with the partner as the stack object (an entry without one
        // fails it, as GC's RunOneTickTree of tree 0).
        const short check = ERQuickdata::S16(e, 0);
        if (!m_treeSim->RunCheckTree(GetBehavior(), check, static_cast<short>(p->m_id), nullptr)) continue;
        MenuInteraction m;
        m.person = this;
        m.object = p;
        m.icon = p;
        m.entry = index;
        m.actionTree = ERQuickdata::S16(e, 2);
        for (const unsigned short* s = ERQuickdata::LocString(e, 0x1c); s && *s; ++s) m.name += static_cast<char16_t>(*s);
        list.push_back(m);
      }
      {
        const std::string me = DebugName();
        std::printf("[social] %s: social menu of %zu interactions with %s\n", me.c_str(), list.size(), p->DebugName());
        for (const MenuInteraction& m : list) {
          std::string n;
          for (char16_t c : m.name) n += c < 0x80 ? static_cast<char>(c) : '?';
          std::printf("[social]   %d \"%s\" (action %04x)\n", m.entry, n.c_str(), static_cast<unsigned short>(m.actionTree));
        }
      }
      if (list.empty()) return 0;
      // PC development aid: SIMMY_SOCIAL_PICK=<text> chooses the first social whose name contains it at
      // once (headless runs have no menu).
      // "a,b,...": the first of them the menu has, in that order; "@a,b,...": each time the next of them
      // (round the list) that the menu has.
      // SIMMY_SOCIAL_ROUNDS=<n>: after n choices the menu is backed out of (social mode ends).
      static const int rounds = std::getenv("SIMMY_SOCIAL_ROUNDS") ? std::atoi(std::getenv("SIMMY_SOCIAL_ROUNDS")) : 0;
      if (std::getenv("SIMMY_SOCIAL_PICK") && rounds > 0 && s_autoPicks[player] >= rounds) {
        std::printf("[social] player %d backs out after %d choices (SIMMY_SOCIAL_ROUNDS)\n", player + 1, rounds);
        si.chosen = -1;
        frame->m_primState = 0;
        return 0;
      }
      if (const char* pick = std::getenv("SIMMY_SOCIAL_PICK")) {
        static int turn = 0;
        std::string order = pick;
        if (!order.empty() && order[0] == '@') {
          std::vector<std::string> parts;
          for (size_t at = 1; at <= order.size();) {
            const size_t end = order.find(',', at);
            parts.push_back(order.substr(at, end == std::string::npos ? std::string::npos : end - at));
            at = end == std::string::npos ? order.size() + 1 : end + 1;
          }
          order.clear();
          for (size_t i = 0; i < parts.size(); ++i) order += (i ? "," : "") + parts[(turn + i) % parts.size()];
          ++turn;
        }
        for (const char* p = order.c_str(); *p;) {
          const char* end = std::strchr(p, ',');
          const std::string part(p, end ? end : p + std::strlen(p));
          p = end ? end + 1 : p + std::strlen(p);
          for (const MenuInteraction& m : list) {
            std::string n;
            for (char16_t c : m.name) n += c < 0x80 ? static_cast<char>(c) : '?';
            if (part.empty() || n.find(part) == std::string::npos) continue;
            si.chosen = static_cast<short>(m.entry);
            std::printf("[social] player %d chooses %d (SIMMY_SOCIAL_PICK)\n", player + 1, si.chosen);
            ++s_autoPicks[player];
            frame->m_primState = 2;
            return 2;
          }
        }
      }
      // SocialModeInteractor::SetupInteractionMenu (8021F480): the player's pie menu, shown.
      si.chosen = -1;
      HUDTarget* hud = _gd.m_hudTarget;
      if (!hud || !hud->IsMenuAvailable(player)) return 0;
      if (FlashPiMenu* pi = _gd.m_players[player & 1].m_piMenu)
        if (pi->CreateInteractionMenu(p, list)) {
          const int shown = hud->ShowMenu(player);
          std::printf("[social] player %d's social menu %s\n", player + 1, shown ? "shown" : "not shown");
          UIAUDIO::Instance()->PlaySound(0x374bdcea);
        }
      frame->m_primState = 2;
      return 2;
    }
    default:
      return fail(0x56);
  }
}
