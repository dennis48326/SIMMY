// Port of game/pregamestate.cpp's InLevelCreateASimState (in-game Create-a-Sim: a wardrobe's "Change
// Clothes") and the UI calls of cXObjectImpl::TryGenericSimCall (game/objectsim.cpp) that open screens:
// 0x22 the wardrobe (flow 5 "Wardrobe") and 0x27 food creation (GameData::GotoXamMode, flow 0x19).
// InLevelBaseState's side of the wardrobe (leaving the lot for CAS) is in inlevel.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>

// livemode.cpp: DAT_80475F24 (the family has just moved in: the move-in wants and fears) and
// DAT_80475C54 / DAT_80475C58 (the players' sims' neighbour ids, selected again after a house load).
int& LiveModeFamilyJustMovedIn();
int* LiveModePlayerNeighbors();

// ---------------------------------------------------------------------------------------------------
// InLevelCreateASimState

// 800953BC
InLevelCreateASimState::InLevelCreateASimState(int id) : CreateASimBaseState(id) {}

// 80094DB0
// CAS edits the neighbour the wardrobe named (CAS_neighborID).
void InLevelCreateASimState::Reset() {
  CreateASimBaseState::Reset();
  _gd.LoadCreateASim(UIDB::UIDBGetInt(const_cast<char*>("CAS_neighborID")), 0);
}

// 80094DF0
// Once CAS is done (accepted or left), live mode comes back behind the loading screen (mode 4): the
// lot saved on the way in is loaded again (no move-in this time) and the state returns 2 (accepted)
// or 1 (left).
void InLevelCreateASimState::Update(float dt) {
  CreateASimBaseState::Update(dt);
  const bool accepted = !m_waitFadeIn && m_accepted;
  const bool canceled = !m_waitFadeIn && m_canceled;
  if (!accepted && !canceled) return;
  if (!LoadingScreenStateMachine::LoadInProgress()) {
    LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{4});
  } else if (LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn) {
    GameData::GotoLiveMode();
    LiveModeFamilyJustMovedIn() = 0;  // DAT_80475F24
    gbDrawUI = 0;
    gbUpdateUI = 0;
    std::printf("[cas] in-game CAS %s: back to live mode\n", accepted ? "accepted" : "left");
    OwnerReturnFromState(accepted ? 2 : 1);
  }
}

// ---------------------------------------------------------------------------------------------------
// cXObjectImpl::TryGenericSimCall (80102A24): the screens

int PcPrivateScreenPlayer();  // uiscreens.cpp (PC co-op private screens)

// 80102A24 (call 0x22)
// The wardrobe: Create-a-Sim on neighbour temp 2 (CAS_neighborID) at lot temp 1, for player 2 when the
// neighbour is player 2's sim (or, in a story, player 2's own neighbour), else player 1; each player's
// selected sim is remembered (their neighbour ids, selected again on the lot's reload). While the HUD
// is already on its way to CAS (HUDTarget +0x100 = 3) the primitive yields.
int GenericSimCallWardrobe(cXObjectImpl* self) {
  const int neighbor = self->m_temps[2];
  auto neighborOf = [](cXPerson* p) { return p ? static_cast<int>(p->GetPersonData(0x1f)) : 0; };  // vtable 0x1B4
  const int sim1 = neighborOf(_globals.GetSelectedPerson(0));
  const int sim2 = neighborOf(_globals.GetSelectedPerson(1));
  int player = 0;
  if (_gd.m_fieldEC) {  // DAT_80475C70: two players
    if (sim2 == neighbor) {
      player = 1;
    } else if (!_gd.m_fieldE8) {  // DAT_80475C6C: a story
      Neighbor* n = _gd.m_players[1].GetPrimaryNeighbor();
      if (n && n->m_id == neighbor) player = 1;
    }
  }
  if (!_gd.m_hudTarget) return 1;
  if (_gd.m_hudTarget->m_quit == 3) return 2;
  // PC co-op: not while a player has food creation up (uiscreens.cpp; on the GC the lot stood still
  // meanwhile): Create-a-Sim leaves the lot, so the sim waits until it is closed.
  if (PcPrivateScreenPlayer() >= 0) return 2;
  UIDB::UIDBSetInt(const_cast<char*>("CAS_mode"), 1);
  UIDB::UIDBSetInt(const_cast<char*>("CAS_lotnumber"), self->m_temps[1]);
  UIDB::UIDBSetInt(const_cast<char*>("CAS_neighborID"), neighbor);
  UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), player);
  UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("Wardrobe"));
  LiveModePlayerNeighbors()[0] = sim1;  // DAT_80475C54
  LiveModePlayerNeighbors()[1] = sim2;  // DAT_80475C58
  std::printf("[cas] wardrobe: neighbour %d (player %d) at lot %d\n", neighbor, player + 1, self->m_temps[1]);
  return 1;
}

namespace {
bool s_modalDialogDisplayed = false;  // sbModalDialogDisplayed_12373
// PC co-op (uiscreens.cpp, PcCoopPrivateScreens): the sim whose food creation is up and its answer
// (-1 none yet, 0 no dish, 1 a dish). The other player's sim keeps playing meanwhile, so the answer
// is not passed through cXObjectImpl::overrideDialogReturnCode, which an object dialog of theirs
// also uses; a second sim asking for food creation waits until the first one has its answer.
cXObjectImpl* s_xamOwner = nullptr;
int s_xamResult = -1;
int s_xamUnclaimed = 0;  // other sims' calls since the answer came (the owner takes it at its next tick)
}  // namespace
bool PcCoopPrivateScreens();  // uiscreens.cpp

// INGTarget::end_dialog in co-op: the answer for the sim that opened the screen.
void PcXamDialogDone(bool made) {
  s_xamResult = made ? 1 : 0;
  s_xamUnclaimed = 0;
}
// The screen went without an answer (INGTarget destroyed otherwise, e.g. leaving the game): no dish.
void PcXamDialogClosed() {
  if (s_xamOwner && s_xamResult < 0) PcXamDialogDone(false);
}

// 80102A24 (call 0x27)
// The inventory / food creation screen: GameData::GotoXamMode(player temp 0, mode temp 1, lot temp 2,
// refrigerator level temp 3). Food creation (modes 0xC, 0xD) is modal: the primitive waits (4) until
// the screen answers (cXObjectImpl::overrideDialogReturnCode, INGTarget::done), then temp 0 = whether a
// dish was made. A bad mode or player is error 0x14.
int GenericSimCallXamMode(cXObjectImpl* self) {
  if (s_xamOwner) {
    // PC co-op: food creation is up for (or has answered) s_xamOwner; any other sim waits its turn.
    // The sim waits on this node without stopping the simulator (2, as a sim waiting in an idle
    // does), unlike the GC's modal wait (4: the whole simulation stops at this object every tick
    // until the screen answers), so the other player's sim and the lot keep going.
    // An answer its sim never takes (the sim was reset or is gone) is dropped after a while.
    if (self != s_xamOwner && (s_xamResult < 0 || ++s_xamUnclaimed < 60)) return 2;
    if (self == s_xamOwner && s_xamResult < 0) return 2;
    if (self == s_xamOwner) {
      self->m_temps[0] = s_xamResult == 1 ? 1 : 0;
      s_xamOwner = nullptr;
      s_xamResult = -1;
      s_modalDialogDisplayed = false;
      return 1;
    }
    s_xamOwner = nullptr;
    s_xamResult = -1;
    s_modalDialogDisplayed = false;
  }
  if (cXObjectImpl::overrideDialogReturnCode != 4 && !PcCoopPrivateScreens()) {
    s_modalDialogDisplayed = false;
    self->m_temps[0] = cXObjectImpl::overrideDialogReturnCode == 1 ? 1 : 0;
    cXObjectImpl::overrideDialogReturnCode = 4;
    return 1;
  }
  if (s_modalDialogDisplayed) return 4;
  const int player = self->m_temps[0];
  const int mode = self->m_temps[1];
  if (static_cast<unsigned short>(mode) > 0xd || static_cast<unsigned short>(player) > 1) {
    // GC: g_bTreeAssert = 1 (a debug flag)
    self->m_treeSim->SetError(0x14);
    self->Error(0x14);
    return -1;
  }
  std::printf("[ing] %s: GotoXamMode(player %d, mode %#x, object %d, refrigerator level %d)\n", self->DebugName(), player,
              mode, self->m_temps[2], self->m_temps[3]);
  if (!_gd.GotoXamMode(player, mode, self->m_temps[2], self->m_temps[3])) {
    s_modalDialogDisplayed = false;
    return 1;
  }
  s_modalDialogDisplayed = mode >= 0xc;
  if (s_modalDialogDisplayed && PcCoopPrivateScreens()) {
    s_xamOwner = self;
    s_xamResult = -1;
    return 2;
  }
  return s_modalDialogDisplayed ? 4 : 1;
}
