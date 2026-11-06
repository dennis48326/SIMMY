// Port of game/mustates.cpp: MUStateMachine ('MUST'), which runs the in-game memory-card ("memory
// unit") requests beside the game flow, and its states.
//
// A request (MURequest) is picked up by MUStatesProcessRequests, which pauses the game flow ('SIMS')
// and calls the state that runs it; MUPollForResult reports 1 (done), 2 (failed or cancelled) or 3
// (refused because a quit to the main menu is pending).
//
// The dialogs went through an MUWrapper (game/muwrapper.cpp: a Wrapper pane whose texts are all
// "deprecated_string_message" in this build), but MUStateMachine::MUWrapperStartup has no callers, so
// the wrapper is never created: the states' writes to it went through a null pointer into low memory
// on the GameCube and are skipped here (Wrap() below), and the Display state does nothing, so a
// request that hits a card error waits there as it did on the GameCube.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

int MUSlotStatus = 0;  // 804858D8 the card slot in use (the first word of a 0x4C-byte object; the rest
                       // is MUStateMachine::s_curPlayer ...)

int ValidSaveGameFoundFlag = 0;  // 804FF8D0
int _bgThreadCompleteMU = 0;     // 804FF8D4
int _bgMUSaveGood = 0;           // 804FF8D8

int MUStateMachine::m_pendingRequest = 0;
int MUStateMachine::m_resultRequest = 0;
int MUStateMachine::m_result = 0;
int MUStateMachine::m_houseNumber = 0;
int MUStateMachine::s_curPlayer = 0;
int MUStateMachine::s_playerCard[2] = {};
const unsigned short* MUStateMachine::s_strings[4] = {};
int MUStateMachine::s_field8F8[2] = {};
int MUStateMachine::s_field900 = 0;
int MUStateMachine::s_field904 = 0;
int MUStateMachine::s_field908 = 0;
int MUStateMachine::s_field90C = 0;
int MUStateMachine::s_field910 = 0;
int MUStateMachine::s_field914 = 0;
int MUStateMachine::s_field918 = 0;
int MUStateMachine::s_cardUsed[2] = {};

namespace {
constexpr int kSimsMachine = 0x53494d53;  // 'SIMS'
constexpr int kSaveSlot = 0xabcdef;       // the save slot id the game uses for its one saved game

void PauseGameFlow(int paused) {
  if (StateMachine* sims = StateMachineManager::s_pStateMachineManager->FindMachineById(kSimsMachine))
    sims->m_paused = paused;
}

// The dialog set-up the states did through the wrapper: the screen configuration (wrapper +0xAC) and
// one of SetDisplayOkType / SetQueryType / SetInfoType. Nothing to do without a wrapper (see above).
enum class DialogKind { DisplayOk, Query, Info };
void Wrap(StateMachineState* state, int config, DialogKind kind, int type) {
  if (state->m_owner && static_cast<MUStateMachine*>(state->m_owner)->m_wrapper)
    SIMMY_STUB("MUWrapper (screen configuration and dialog type; game/muwrapper.cpp not ported)", 0x80082500);
  (void)config;
  (void)kind;
  (void)type;
}
// The same, followed by calling the Display state.
void ShowDialog(StateMachineState* state, int config, DialogKind kind, int type) {
  Wrap(state, config, kind, type);
  state->OwnerCallState(MUStateMachine::kStateDisplay);
}

int& ReturnVal(StateMachineState* state) { return state->m_owner->m_returnVal; }
}  // namespace

// THREADPOLL_* / THREADEXEC_* / THREADCALL_*: a new game is saved on the background thread.
// 80091654
int THREADPOLL_MU_IsComplete() { return _bgThreadCompleteMU; }
// 8009165C
void THREADPOLL_MU_Start() { _bgThreadCompleteMU = 0; }
// 80091668
void THREADPOLL_MU_End() { _bgThreadCompleteMU = 1; }
// 80091674
void THREADEXEC_MU_SaveNewGame() {
  _bgMUSaveGood = 0;
  if (SimsMemCardWrap::SaveNewGame(MUSlotStatus) == 1) _bgMUSaveGood = 1;
  THREADPOLL_MU_End();
}
// 800916B4
void THREADCALL_MU_SaveNewGame() {
  THREADPOLL_MU_Start();
  _pBackground->ExecuteRoutine(THREADEXEC_MU_SaveNewGame);
}

// ---------------------------------------------------------------------------------------------------
// MUStatesProcessRequests (state 0)

// 80093BDC
MUStatesProcessRequests::MUStatesProcessRequests(int id) : StateMachineState(id) {}
// 80093C14
MUStatesProcessRequests::~MUStatesProcessRequests() = default;
// 80093C48
MUWrapper* MUStatesProcessRequests::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }

// 80091464
// Reports the finished request's result (the called state's 2 as 1, its 1 as 2) and resumes the game
// flow, or starts the next request.
void MUStatesProcessRequests::Update(float) {
  const int ret = ReturnVal(this);
  if (ret == 2 || ret == 1) {
    if (ret == 2) {
      if (m_step == 1) m_step = 0;
      else MUStateMachine::m_result = 1;
    } else if (m_step != 1) {
      MUStateMachine::m_result = 2;
    }
    PauseGameFlow(0);
    ReturnVal(this) = 0;
    return;
  }
  if (ret == 0) m_step = 0;
  if (MUStateMachine::m_pendingRequest == 0 || MUStateMachine::m_pendingRequest >= 9) return;
  MUStateMachine::m_resultRequest = MUStateMachine::m_pendingRequest;
  MUStateMachine::m_pendingRequest = 0;
  if (_gd.m_field9C != 0) {  // a quit to the main menu is pending
    MUStateMachine::m_result = 3;
    if (MUWrapper* w = GetWrapper()) w->CleanUp();
    PauseGameFlow(0);
    ReturnVal(this) = 0;
    m_step = 0;
    return;
  }
  PauseGameFlow(1);
  switch (MUStateMachine::m_resultRequest) {
    case MUStateMachine::kRequestLoadGame: OwnerCallState(MUStateMachine::kStateLoadGame); break;
    case MUStateMachine::kRequestLoadHouse: OwnerCallState(MUStateMachine::kStateLoadHouse); break;
    case MUStateMachine::kRequestAutoSaveExisting: OwnerCallState(MUStateMachine::kStateAutoSaveExisting); break;
    case MUStateMachine::kRequestAutoSaveNew: OwnerCallState(MUStateMachine::kStateAutoSaveNew); break;
    case MUStateMachine::kRequestSaveExistingGame: OwnerCallState(MUStateMachine::kStateSaveExistingGame); break;
    case MUStateMachine::kRequestTransitionAutoSave:
      PauseGameFlow(0);
      OwnerCallState(MUStateMachine::kStateTransitionAutoSaveExisting);
      break;
    case MUStateMachine::kRequestLoadConfig: OwnerCallState(MUStateMachine::kStateLoadConfig); break;
    default: break;  // 6: nothing (the game flow stays paused)
  }
}

// ---------------------------------------------------------------------------------------------------
// MUStatesFormat (state 1): asks to format the card, formats it and reports the result.

// 80093C54
MUStatesFormat::MUStatesFormat(int id) : StateMachineState(id) {}
// 80093C8C
MUStatesFormat::~MUStatesFormat() = default;
// 80093CC0
MUWrapper* MUStatesFormat::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }

// 800916F8
void MUStatesFormat::Reset() {
  m_step = 0;
  m_drawWrapper = 0;
  ShowDialog(this, 2, DialogKind::Query, 0x11);  // "format the card?"
  m_drawWrapper = 0;
  m_step = 1;
}

// 80091764
void MUStatesFormat::Draw(ERC* rc) {
  if (m_drawWrapper == 1)
    if (MUWrapper* w = GetWrapper()) w->Draw(rc);
}

// 800917AC
void MUStatesFormat::Update(float) {
  const int ret = ReturnVal(this);
  if (ret == 0) {
    bool present = false;
    SimsMemCardWrap::IsCardInSlot(MUSlotStatus, present);
    if (!present) {
      m_owner->ReturnFromState(1);
      return;
    }
    if (m_step == 1) return;
    ShowDialog(this, 4, DialogKind::Query, 10);
    m_drawWrapper = 0;
    m_step = 2;
    return;
  }
  if (ret == 1) {
    if (m_step == 1 || m_step == 2) {
      ShowDialog(this, 3, DialogKind::Query, 6);  // "are you sure?"
      m_step = 3;
      return;
    }
    if (m_step != 3) return;
    ReturnVal(this) = 0;
    return;
  }
  if (ret == 2) {
    switch (m_step) {
      case 1:
        ShowDialog(this, 4, DialogKind::Query, 10);
        m_step = 2;
        m_drawWrapper = 0;
        return;
      case 2:  // "formatting...", drawn for at least two seconds before the format
        Wrap(this, 8, DialogKind::Info, 4);
        if (MUWrapper* w = GetWrapper()) w->Reset();
        m_step = 6;
        m_drawWrapper = 1;
        return;
      case 6: {
        if (m_owner->GetTimeInState() < 2.0f) return;
        bool ok = false;
        if (SimsMemCardWrap::FormatCard(MUSlotStatus) == 1) {
          bool formatted = false;
          SimsMemCardWrap::IsCardFormatted(MUSlotStatus, formatted);
          ok = formatted;
        }
        m_drawWrapper = 0;
        if (ok) {
          ShowDialog(this, 6, DialogKind::DisplayOk, 4);  // formatted
          m_step = 5;
        } else {
          ShowDialog(this, 6, DialogKind::DisplayOk, 3);  // format failed
          m_step = 4;
        }
        return;
      }
      case 3:
      case 7: m_owner->ReturnFromState(1); return;
      case 5: m_owner->ReturnFromState(2); return;
      case 4:
        m_owner->ReturnFromState(1);
        ReturnVal(this) = 0;
        return;
      default: return;
    }
  }
  if (ret == 3) {
    m_step = 0;
    ReturnVal(this) = 0;
  }
}

// ---------------------------------------------------------------------------------------------------
// MUStatesSlotSelectSave (4) / MUStatesSlotSelectSaveNoSkip (11): pick the card slot to save to. The
// first always uses slot A; the second asks.

// 80093CCC
MUStatesSlotSelectSave::MUStatesSlotSelectSave(int id) : StateMachineState(id) {}
// 80093D04
MUStatesSlotSelectSave::~MUStatesSlotSelectSave() = default;
// 80093D38
MUWrapper* MUStatesSlotSelectSave::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }

// 80091A7C
void MUStatesSlotSelectSave::Update(float) {
  const int ret = ReturnVal(this);
  if (ret == 0) {
    MUSlotStatus = 0;
    MUStateMachine::s_playerCard[0] = 0;
    m_owner->ReturnFromState(2);
  } else if (ret == 1) {
    m_owner->ReturnFromState(1);
  } else if (ret == 2) {
    m_owner->ReturnFromState(2);
  }
}

// 80093D44
MUStatesSlotSelectSaveNoSkip::MUStatesSlotSelectSaveNoSkip(int id) : StateMachineState(id) {}
// 80093D7C
MUStatesSlotSelectSaveNoSkip::~MUStatesSlotSelectSaveNoSkip() = default;
// 80093DB0
MUWrapper* MUStatesSlotSelectSaveNoSkip::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }

// 80091AF0
void MUStatesSlotSelectSaveNoSkip::Update(float) {
  const int ret = ReturnVal(this);
  if (ret == 0) {
    ShowDialog(this, 7, DialogKind::Query, 9);  // the slot menu
  } else if (ret == 1) {
    m_owner->ReturnFromState(1);
  } else if (ret == 2) {
    m_owner->ReturnFromState(2);
  }
}

// ---------------------------------------------------------------------------------------------------
// MUStatesLoadGame (state 2): checks the card and loads the saved game.

// 80093DBC
MUStatesLoadGame::MUStatesLoadGame(int id) : StateMachineState(id) {}
// 80093DF4
MUStatesLoadGame::~MUStatesLoadGame() = default;
// 80093E28
MUWrapper* MUStatesLoadGame::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }
// 80091B78
void MUStatesLoadGame::Reset() { m_step = 0; }
// 80091B84
void MUStatesLoadGame::Draw(ERC*) {}

// 80091B88
void MUStatesLoadGame::Update(float dt) {
  const int ret = ReturnVal(this);
  if (ret == 1) return;
  if (ret == 2) {
    if ((m_step >= 1 && m_step <= 6) || m_step == 11 || m_step == 7) {
      m_owner->ReturnFromState(1);
    } else if (m_step == 10) {
      m_step = 0;
      ReturnVal(this) = 0;
    }
    return;
  }
  if (ret != 0) return;
  if (m_step == 8) {
    if (SimsMemCardWrap::LoadGame(MUSlotStatus, kSaveSlot) != 1) {
      ShowDialog(this, 6, DialogKind::DisplayOk, 9);  // load failed
      m_step = 7;
      return;
    }
    m_step = 13;
    return;
  }
  if (m_step == 13) {
    m_timer -= dt;
    if (m_timer > 0.0f) return;
    m_owner->ReturnFromState(2);
    return;
  }
  if (m_step != 0 && m_step != 12) return;

  bool present = false;
  SimsMemCardWrap::IsCardInSlot(MUSlotStatus, present);
  if (!present) {
    ShowDialog(this, 6, DialogKind::DisplayOk, 6);  // no card
    m_step = 1;
    return;
  }
  if (m_step == 12) m_step = 0;
  bool wrong = false;
  SimsMemCardWrap::IsWrongDevice(MUSlotStatus, wrong);
  if (wrong) {
    ShowDialog(this, 9, DialogKind::DisplayOk, 0x18);
    m_step = 10;
    return;
  }
  bool damaged = true;
  if (SimsMemCardWrap::IsCardDamaged(MUSlotStatus, damaged) == 1) {
    if (damaged) {
      ShowDialog(this, 6, DialogKind::DisplayOk, 1);
      m_step = 2;
      return;
    }
    bool corrupted = false;
    if (SimsMemCardWrap::IsCardCorrupted(MUSlotStatus, corrupted) == 1) {
      if (corrupted) {
        ShowDialog(this, 6, DialogKind::DisplayOk, 2);
        m_step = 3;
        return;
      }
      bool formatted = false;
      if (SimsMemCardWrap::IsCardFormatted(MUSlotStatus, formatted) == 1) {
        if (!formatted) {
          ShowDialog(this, 6, DialogKind::DisplayOk, 5);
          m_step = 4;
          return;
        }
        bool has = false;
        if (SimsMemCardWrap::HasSavedGame(MUSlotStatus, has, kSaveSlot) == 1) {
          if (!has) {
            ShowDialog(this, 6, DialogKind::DisplayOk, 5);  // no saved game
            m_step = 5;
            return;
          }
          bool invalid = false;
          if (SimsMemCardWrap::HasInvalidGame(MUSlotStatus, invalid, kSaveSlot) == 1) {
            if (invalid) {
              ShowDialog(this, 6, DialogKind::DisplayOk, 5);
              m_step = 11;
              return;
            }
            bool different = false;
            if (SimsMemCardWrap::IsDifferentCard(MUSlotStatus, different) == 1) {
              if (different) {
                ShowDialog(this, 6, DialogKind::DisplayOk, 8);
                m_step = 6;
                return;
              }
              m_step = 8;
              m_timer = 0.01f;
              return;
            }
          }
        }
      }
    }
  }
  m_step = 0;
  ReturnVal(this) = 0;
}

// ---------------------------------------------------------------------------------------------------
// MUStatesLoadHouse (state 3): reloads the current house from the card after a short delay.

// 80093E34
MUStatesLoadHouse::MUStatesLoadHouse(int id) : StateMachineState(id) {}
// 80093E6C
MUStatesLoadHouse::~MUStatesLoadHouse() = default;
// 80093EA0
MUWrapper* MUStatesLoadHouse::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }
// 8009200C
void MUStatesLoadHouse::Reset() { m_step = 0; }
// 80092018
void MUStatesLoadHouse::Draw(ERC*) {}

// 8009201C
void MUStatesLoadHouse::Update(float dt) {
  const int ret = ReturnVal(this);
  if (ret == 0) {
    if (m_step == 0) {
      m_step = 8;
      m_timer = 0.01f;
    } else if (m_step == 8) {
      m_timer -= dt;
      if (m_timer <= 0.005f) m_step = 9;
    } else if (m_step == 9) {
      // GC: SimsMemCardWrap::LoadHouse(the current EHouse (_globals +0xC8)'s number, +0x54).
      SIMMY_STUB("MUStatesLoadHouse: the current EHouse (_globals+0xC8) is not ported", 0x8009201C);
      m_step = 10;
    } else if (m_step == 10) {
      m_timer -= dt;
      if (m_timer <= 0.0f) m_owner->ReturnFromState(2);
    }
  } else if (ret == 2) {
    if ((m_step >= 1 && m_step <= 7) || m_step == 11 || m_step == 13) {
      m_owner->ReturnFromState(1);
    } else if (m_step == 12) {
      m_step = 0;
      ReturnVal(this) = 0;
    }
  }
}

// ---------------------------------------------------------------------------------------------------
// MUStatesAutoSaveNew (state 5): picks the cards for a new game (player 1 on slot A, the other
// player on the other slot).

// 80093EAC
MUStatesAutoSaveNew::MUStatesAutoSaveNew(int id) : StateMachineState(id) {}
// 80093EE4
MUStatesAutoSaveNew::~MUStatesAutoSaveNew() = default;
// 80093F18
MUWrapper* MUStatesAutoSaveNew::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }

// 8009217C
void MUStatesAutoSaveNew::Update(float) {
  int* card = MUStateMachine::s_playerCard;
  const int other = MUStateMachine::s_curPlayer == 0 ? 1 : 0;
  const int ret = ReturnVal(this);
  if (ret == 0) {
    MUSlotStatus = 0;
    card[0] = 0;
    card[other] = 1;
    m_owner->ReturnFromState(2);
    return;
  }
  if (ret == 1) {
    if (m_step == 1) {
      OwnerCallState(MUStateMachine::kStateSlotSelectSaveNoSkip);
      m_step = 2;
      return;
    }
    if (m_step == 2) {
      ShowDialog(this, 1, DialogKind::Query, 5);
      m_step = 3;
      return;
    }
    if (m_step != 3) return;
  } else if (ret == 2) {
    if (m_step == 1) {
      card[MUStateMachine::s_curPlayer] = MUSlotStatus;
      card[other] = MUSlotStatus == 0 ? 1 : 0;
      m_owner->ReturnFromState(2);
      return;
    }
    if (m_step != 2) {
      if (m_step == 3) m_owner->ReturnFromState(1);
      return;
    }
  } else {
    return;
  }
  ReturnVal(this) = 0;
}

// ---------------------------------------------------------------------------------------------------
// MUStatesSaveNewGame (state 6): checks the card and space, then saves a new game on the background
// thread.

// 80093F24
MUStatesSaveNewGame::MUStatesSaveNewGame(int id) : StateMachineState(id) {}
// 80093F6C
MUStatesSaveNewGame::~MUStatesSaveNewGame() = default;
// 80093FA0
MUWrapper* MUStatesSaveNewGame::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }

// 800922DC
void MUStatesSaveNewGame::Reset() {
  m_field2C = 0;
  m_step = 0;
  m_overwriteOk = 0;
  m_field28 = 0;
}

// 800922F4
void MUStatesSaveNewGame::Draw(ERC* rc) {
  if (m_step >= 9 && m_step <= 11)
    if (MUWrapper* w = GetWrapper()) w->Draw(rc);
}

// 80092340
void MUStatesSaveNewGame::Update(float dt) {
  const int ret = ReturnVal(this);
  bool checkStart = false;
  if (ret == 0) {
    if (m_step == 0) {
      MUStateMachine::s_curPlayer = 0;
      bool present = false;
      SimsMemCardWrap::IsCardInSlot(MUSlotStatus, present);
      if (!present) {
        ShowDialog(this, 1, DialogKind::Query, 3);
        m_step = 1;
        return;
      }
      bool flag = false;
      if (SimsMemCardWrap::IsWrongDevice(MUSlotStatus, flag) != 1) goto failed;
      if (flag) {
        ShowDialog(this, 9, DialogKind::DisplayOk, 0x17);
        m_step = 13;
        return;
      }
      if (SimsMemCardWrap::IsCardDamaged(MUSlotStatus, flag) != 1) goto failed;
      if (flag) {
        ShowDialog(this, 6, DialogKind::DisplayOk, 1);
        m_step = 2;
        return;
      }
      if (SimsMemCardWrap::IsCardCorrupted(MUSlotStatus, flag) != 1) goto failed;
      bool formatted = false;
      if (!flag) {
        if (SimsMemCardWrap::IsCardFormatted(MUSlotStatus, formatted) != 1) goto failed;
      }
      if (flag || !formatted) {  // offer to format
        ShowDialog(this, 10, DialogKind::Query, 1);
        m_step = 4;
        return;
      }
      bool has = false;
      if (SimsMemCardWrap::HasSavedGame(MUSlotStatus, has, kSaveSlot) != 1) {
        Reset();
        ReturnVal(this) = 0;
        return;
      }
      if (has && m_overwriteOk == 0) {
        bool invalid = false;
        if (SimsMemCardWrap::HasInvalidGame(MUSlotStatus, invalid, kSaveSlot) != 1) goto failed;
        if (!invalid) {  // "overwrite the saved game?"
          ShowDialog(this, 2, DialogKind::Query, 2);
          m_step = 6;
          return;
        }
      }
      if (!has) {
        bool space = false;
        if (SimsMemCardWrap::HasSpaceForSave(MUSlotStatus, space, nullptr) != 1) goto failed;
        if (!space) {
          ShowDialog(this, 6, DialogKind::DisplayOk, 0x1a);
          m_step = 7;
          return;
        }
      }
      checkStart = true;
    }
  } else if (ret == 1) {
    if (m_step == 1 || m_step == 4 || m_step == 6 || m_step == 15 || m_step == 5 || m_step == 7) {
      m_owner->ReturnFromState(1);
      return;
    }
    checkStart = true;
  } else if (ret == 2) {
    switch (m_step) {
      case 7: m_owner->ReturnFromState(1); return;
      case 1:
      case 5:
        m_step = 0;
        ReturnVal(this) = 0;
        return;
      case 2: case 3: case 8: case 14: case 16: case 12:
        m_owner->ReturnFromState(1);
        return;
      case 4:
        OwnerCallState(MUStateMachine::kStateFormat);
        m_step = 5;
        return;
      case 6:
        m_overwriteOk = 1;
        m_step = 0;
        ReturnVal(this) = 0;
        return;
      case 13:
        m_step = 0;
        ReturnVal(this) = 0;
        return;
      default: break;
    }
    checkStart = true;
  } else if (ret == 3) {
    if (m_step == 4) {
      ShowDialog(this, 6, DialogKind::DisplayOk, 6);
      m_step = 16;
      return;
    }
    m_owner->ReturnFromState(1);
    return;
  } else {
    checkStart = true;
  }
  if (checkStart && m_step == 0) {
    m_step = 9;
    m_timer = 0.01f;
    ReturnVal(this) = 0;
    return;
  }

  if (m_step == 9) {
    bool has = false, space = false;
    SimsMemCardWrap::HasSavedGame(MUSlotStatus, has, kSaveSlot);
    SimsMemCardWrap::HasSpaceForSave(MUSlotStatus, space, nullptr);
    if (has || space) {
      PauseGameFlow(0);
      m_saveStartTime = static_cast<float>(_pClockMan->GetSec(&_sysclock));
      THREADCALL_MU_SaveNewGame();
      m_step = 10;
      return;
    }
    goto failed;
  }
  if (m_step == 10 || m_step == 15) {
    if (!THREADPOLL_MU_IsComplete()) return;
    if (!_bgMUSaveGood) {
      PauseGameFlow(1);
      ShowDialog(this, 6, DialogKind::DisplayOk, 10);  // save failed
      m_step = 8;
      return;
    }
    m_step = 11;
    return;
  }
  if (m_step == 11) {
    m_timer -= dt;
    if (m_timer > 0.0f) return;
    m_owner->ReturnFromState(2);
  }
  return;

failed:  // a card call failed: start over
  Reset();
  ReturnVal(this) = 0;
}

// ---------------------------------------------------------------------------------------------------
// MUStatesAutoSaveExisting (state 7) / MUStatesTransitionAutoSaveExisting (state 9): save over the
// existing game (states 8 / 10), retrying through a dialog; after four failures the player is offered
// to quit to the main menu.

// 80093FAC
MUStatesAutoSaveExisting::MUStatesAutoSaveExisting(int id) : StateMachineState(id) {}
// 80093FE4
MUStatesAutoSaveExisting::~MUStatesAutoSaveExisting() = default;
// 80094018
MUWrapper* MUStatesAutoSaveExisting::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }

// 80092988
void MUStatesAutoSaveExisting::Reset() {
  ValidSaveGameFoundFlag = 0;
  m_retries = 0;
}

// 80092998
void MUStatesAutoSaveExisting::Update(float) {
  const int ret = ReturnVal(this);
  if (ret == 0) {
    OwnerCallState(MUStateMachine::kStateSaveExistingGame);
    m_step = 1;
  } else if (ret == 1) {
    if (m_step == 1) {
      bool present = false;
      SimsMemCardWrap::IsCardInSlot(MUSlotStatus, present);
      if (!present) {
        Wrap(this, -1, DialogKind::DisplayOk, 0x12);  // GC: the configuration was left as it was
      } else {
        if (++m_retries > 3) {
          ShowDialog(this, 5, DialogKind::Query, 0xc);  // quit to the main menu?
          m_step = 3;
          return;
        }
        Wrap(this, 6, DialogKind::DisplayOk, 7);
      }
      OwnerCallState(MUStateMachine::kStateDisplay);
      m_step = 2;
    } else if (m_step == 3) {
      MUStateMachine::SetupQuitToMainMenu();
      m_owner->ReturnFromState(1);
    }
  } else if (ret == 2) {
    if (m_step == 1) m_owner->ReturnFromState(2);
    else if (m_step == 2 || m_step == 3) ReturnVal(this) = 0;
  }
}

// 8009409C
MUStatesTransitionAutoSaveExisting::MUStatesTransitionAutoSaveExisting(int id) : StateMachineState(id) {}
// 800940D4
MUStatesTransitionAutoSaveExisting::~MUStatesTransitionAutoSaveExisting() = default;
// 80094108
MUWrapper* MUStatesTransitionAutoSaveExisting::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }

// 80092D44
void MUStatesTransitionAutoSaveExisting::Reset() {
  ValidSaveGameFoundFlag = 0;
  m_retries = 0;
}

// 80092D54
void MUStatesTransitionAutoSaveExisting::Update(float) {
  const int ret = ReturnVal(this);
  if (ret == 0) {
    OwnerCallState(MUStateMachine::kStateTransitionSaveExistingGame);
    m_step = 1;
  } else if (ret == 1) {
    if (m_step == 1) {
      PauseGameFlow(1);
      bool present = false;
      SimsMemCardWrap::IsCardInSlot(MUSlotStatus, present);
      if (!present) {
        Wrap(this, 6, DialogKind::DisplayOk, 0x12);
      } else {
        if (++m_retries > 3) {
          ShowDialog(this, 5, DialogKind::Query, 0xc);
          m_step = 3;
          return;
        }
        Wrap(this, 6, DialogKind::DisplayOk, 7);
      }
      OwnerCallState(MUStateMachine::kStateDisplay);
      m_step = 2;
    } else if (m_step == 3) {
      MUStateMachine::SetupQuitToMainMenu();
      m_owner->ReturnFromState(1);
    }
  } else if (ret == 2) {
    if (m_step == 1) m_owner->ReturnFromState(2);
    else if (m_step == 2 || m_step == 3) ReturnVal(this) = 0;
  }
}

// ---------------------------------------------------------------------------------------------------
// MUStatesSaveExistingGame (state 8) / MUStatesTransitionSaveExistingGame (state 10): save over the
// game on player 1's card.

// 80094024
MUStatesSaveExistingGame::MUStatesSaveExistingGame(int id) : StateMachineState(id) {}
// 8009405C
MUStatesSaveExistingGame::~MUStatesSaveExistingGame() = default;
// 80094090
MUWrapper* MUStatesSaveExistingGame::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }
// 80092B08
void MUStatesSaveExistingGame::Reset() { m_step = 0; }
// 80092B14
void MUStatesSaveExistingGame::Draw(ERC*) {}

// 80092B18
void MUStatesSaveExistingGame::Update(float dt) {
  const int ret = ReturnVal(this);
  if (ret == 0) {
    if (m_step == 0) {
      MUSlotStatus = MUStateMachine::s_playerCard[0];
      MUStateMachine::s_curPlayer = 0;
      bool wrong = false;
      if (SimsMemCardWrap::IsWrongDevice(MUSlotStatus, wrong) != 1) {
        m_step = 0;
        ReturnVal(this) = 0;
        return;
      }
      if (wrong) {
        ShowDialog(this, 9, DialogKind::DisplayOk, 0x18);
        m_step = 11;
        return;
      }
      m_step = 8;
      return;
    }
    if (m_step == 8) {
      _bgMUSaveGood = 0;
      const int r = SimsMemCardWrap::SaveGame(MUSlotStatus, false, false, kSaveSlot);
      ValidSaveGameFoundFlag = 1;
      if (r == 1) _bgMUSaveGood = 1;
      if (!_bgMUSaveGood) {
        ShowDialog(this, 6, DialogKind::DisplayOk, 10);  // save failed
        m_step = 7;
        return;
      }
      ValidSaveGameFoundFlag = 0;
      m_step = 9;
      m_timer = 0.01f;
      return;
    }
    if (m_step == 9) {
      m_timer -= dt;
      if (m_timer > 0.0f) return;
    }
    m_owner->ReturnFromState(2);
    return;
  }
  if (ret == 1) {
    if (m_step == 1) m_owner->ReturnFromState(1);
    return;
  }
  if (ret != 2) return;
  if (m_step == 2 || m_step == 10 || m_step == 3 || m_step == 4 || m_step == 5 || m_step == 6 ||
      m_step == 12 || m_step == 7) {
    m_owner->ReturnFromState(1);
    return;
  }
  if (m_step != 1 && m_step != 11) return;
  m_step = 0;
  ReturnVal(this) = 0;
}

// 80094114
MUStatesTransitionSaveExistingGame::MUStatesTransitionSaveExistingGame(int id) : StateMachineState(id) {}
// 8009414C
MUStatesTransitionSaveExistingGame::~MUStatesTransitionSaveExistingGame() = default;
// 80094180
MUWrapper* MUStatesTransitionSaveExistingGame::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }
// 80092ED8
void MUStatesTransitionSaveExistingGame::Reset() { m_step = 0; }

// 80092EE4
// The wrapper is drawn without its background (wrapper +0xC8) in steps 8 and 9.
void MUStatesTransitionSaveExistingGame::Draw(ERC* rc) {
  if (m_step == 8 || m_step == 9)
    if (MUWrapper* w = GetWrapper()) w->Draw(rc);
}

// 80092F5C
void MUStatesTransitionSaveExistingGame::Update(float dt) {
  const int ret = ReturnVal(this);
  if (ret == 1) return;
  if (ret == 2) {
    if (m_step != 1) {
      if (m_step == 10 || m_step == 2 || m_step == 3 || m_step == 4 || m_step == 5 || m_step == 6 ||
          m_step == 12 || m_step == 7) {
        m_owner->ReturnFromState(1);
        return;
      }
      if (m_step != 11) return;
    }
    m_step = 0;
    ReturnVal(this) = 0;
    return;
  }
  if (ret != 0) return;
  if (m_step == 0) {
    SimsMemCardWrap::SaveGame(MUSlotStatus, false, false, kSaveSlot);
  } else if (m_step == 8) {
    PauseGameFlow(0);
    ValidSaveGameFoundFlag = 1;
    m_step = 9;
    return;
  } else if (m_step == 9) {
    m_timer -= dt;
    if (m_timer > 0.0f) return;
    if (SimsMemCardWrap::SaveGame(MUSlotStatus, false, false, kSaveSlot) != 1) {
      PauseGameFlow(1);
      ShowDialog(this, 6, DialogKind::DisplayOk, 10);
      m_step = 7;
      return;
    }
  } else {
    return;
  }
  m_owner->ReturnFromState(2);
}

// ---------------------------------------------------------------------------------------------------
// MUStatesLoadConfig (state 13) and MUStatesDisplay (state 14)

// 8009418C
MUStatesLoadConfig::MUStatesLoadConfig(int id) : StateMachineState(id) {}
// 800941C4
MUStatesLoadConfig::~MUStatesLoadConfig() = default;
// 800941F8
MUWrapper* MUStatesLoadConfig::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }
// 80093118
void MUStatesLoadConfig::Reset() { m_step = 0; }
// 80093124
void MUStatesLoadConfig::Draw(ERC* rc) {
  if (m_step == 1 || m_step == 2)
    if (MUWrapper* w = GetWrapper()) w->Draw(rc);
}
// 80093170
void MUStatesLoadConfig::Update(float) {
  SimsMemCardWrap::LoadConfigFile(MUSlotStatus);
  m_owner->ReturnFromState(2);
}

// 80094204
MUStatesDisplay::MUStatesDisplay(int id) : StateMachineState(id) {}
// 8009423C
MUStatesDisplay::~MUStatesDisplay() = default;
// 80094270
MUWrapper* MUStatesDisplay::GetWrapper() { return static_cast<MUStateMachine*>(m_owner)->m_wrapper; }
// 800931B0
void MUStatesDisplay::Startup() {}
// 800931B4
void MUStatesDisplay::Shutdown() { Wrapper::WrapperRemoveMessageHandler(); }
// 800931DC
void MUStatesDisplay::Reset() {
  if (MUWrapper* w = GetWrapper()) w->Reset();
}
// 8009320C
void MUStatesDisplay::Update(float) {
  if (MUWrapper* w = GetWrapper()) w->Update();
}
// 80093250
void MUStatesDisplay::Draw(ERC* rc) {
  if (MUWrapper* w = GetWrapper()) w->Draw(rc);
}

// ---------------------------------------------------------------------------------------------------
// MUStateMachine

// 80093294
// Sets up the memory card save types and the states, then resets.
void MUStateMachine::Startup() {
  SimsMemCardWrap::Init();
  AddState(new MUStatesProcessRequests(kStateProcessRequests));
  AddState(new MUStatesFormat(kStateFormat));
  AddState(new MUStatesLoadGame(kStateLoadGame));
  AddState(new MUStatesLoadHouse(kStateLoadHouse));
  AddState(new MUStatesAutoSaveNew(kStateAutoSaveNew));
  AddState(new MUStatesSaveNewGame(kStateSaveNewGame));
  AddState(new MUStatesSlotSelectSave(kStateSlotSelectSave));
  AddState(new MUStatesAutoSaveExisting(kStateAutoSaveExisting));
  AddState(new MUStatesSaveExistingGame(kStateSaveExistingGame));
  AddState(new MUStatesTransitionAutoSaveExisting(kStateTransitionAutoSaveExisting));
  AddState(new MUStatesTransitionSaveExistingGame(kStateTransitionSaveExistingGame));
  AddState(new MUStatesSlotSelectSaveNoSkip(kStateSlotSelectSaveNoSkip));
  AddState(new MUStatesLoadConfig(kStateLoadConfig));
  AddState(new MUStatesDisplay(kStateDisplay));
  SetNextState(kStateProcessRequests, 0.0f);
  Reset();
}

// 80093684
void MUStateMachine::Shutdown() {
  MUWrapperShutdown();
  m_status.ShutdownCurState();
  DeleteAllStates();
}

// 800936C0
void MUStateMachine::Reset() {
  MUSlotStatus = 0;
  s_field914 = 0;
  s_field90C = 1;
  s_curPlayer = 0;
  s_playerCard[0] = 0;
  s_playerCard[1] = 1;
  s_field900 = 0;
  s_field904 = 0;
  s_field908 = 1;
  s_field910 = 0;
  s_field8F8[1] = s_field8F8[0] = 0;
  s_field918 = 0;
  static const unsigned short kEmpty[1] = {0};  // BString2::s_empty
  for (const unsigned short*& s : s_strings) s = kEmpty;
  SetNextState(kStateProcessRequests, 0.0f);
  m_houseNumber = 0;
}

// 80093798
void MUStateMachine::Update(float dt) { StateMachine::Update(dt); }

// 800937B8
void MUStateMachine::Draw(ERC* rc) {
  if (_gd.m_drawTopmost) StateMachine::Draw(rc);
}

// 800937E8
// Queues a request when none is waiting; returns it, or 0 when refused.
int MUStateMachine::MURequest(int request) {
  if (m_pendingRequest != 0 || static_cast<unsigned>(request - 1) >= 8) return 0;
  m_resultRequest = 0;
  m_result = 0;
  m_pendingRequest = request;
  return request;
}

// 8009381C
int MUStateMachine::MUPollForResult(int request) { return m_resultRequest == request ? m_result : 0; }

// 80093958
void MUStateMachine::MUWrapperShutdown() {
  if (!m_wrapper) return;
  m_wrapper->Shutdown();
  delete m_wrapper;
  m_wrapper = nullptr;
}

// 800939C0
int MUStateMachine::GetCurRequest() { return m_pendingRequest; }
// 800939C8
void MUStateMachine::ClearResult() { m_result = 0; }
// 800939D4
void MUStateMachine::SetHouseNumber(int house) { m_houseNumber = house; }

// 800939DC
// Asks the game flow to go back to the main menu, remembering the player and which players' cards
// were (state 2) in use.
void MUStateMachine::SetupQuitToMainMenu() {
  _gd.m_fieldA0 = s_curPlayer;
  _gd.m_field9C = 1;
  _gd.m_fieldA4 = (s_field908 == 2 ? 1 : 0) | (s_field90C == 2 ? 2 : 0);
}

// 80093A30
MUStateMachine::MUStateMachine(int id, int machineId) : StateMachine(id, machineId) {}
// 80093B3C
MUStateMachine::~MUStateMachine() = default;
// 80093BCC
int MUStateMachine::GetHouseNumber() { return m_houseNumber; }
// 80093BD4
MUWrapper* MUStateMachine::GetMUWrapper() { return m_wrapper; }
