// Port of the memory card screens: game/simsmemcardwrap.cpp (SimsMemCardWrap), the M2MTarget part of
// game/targets_s2c.cpp, game/loadgametarget.cpp and game/savegametarget.cpp. The save file itself is
// NghResFile's (nghsave.cpp) and OptionsRecon's (optionsrecon.cpp); the neighbourhood
// (Globs::pNeighborhood, neighborhood.cpp) saves the game into the NghResFile and loads it from it.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>

float AptAuxParagraphHeight(const unsigned short* text, float width, float fontSize);  // aptauxpceorgl.cpp

extern int MUSlotStatus;  // 804858D8, the card slot in use (game/mustates.cpp)
// DAT_80476214 / DAT_80476218: the loaded game's player slot flags (neighborhood.cpp, urbzmode.cpp).
int g_loadedGameSlot = 0;
int g_loadedGameSlot2 = 0;

namespace {

AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
G2DTarget* G2D() { return static_cast<G2DTarget*>(_globals.m_g2dTarget); }
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
MMUTarget* MainMenu() { return static_cast<MMUTarget*>(_gd.m_mainMenuTarget); }
OptionsRecon* Options() { return static_cast<OptionsRecon*>(_globals.m_optionsRecon); }  // DAT_80475F14

int gNumDiskErrors = 0;             // engine: DVD read errors (none on PC)
int g_backgroundLoadInProgress = 0;
int g_backgroundSaveInProgress = 0;
std::atomic<int> s_bBGComplete{1};  // BG::s_bBGComplete: the background routine has finished

// A G2D dialog is between states (loading, closing): the screens wait (G2DTarget +0x134).
bool DialogBusy() {
  G2DTarget* g = G2D();
  return g && g->m_state != 0 && g->m_state != 3;
}

std::vector<unsigned short> MemCardText(const char* key) {
  std::vector<unsigned short> v;
  const unsigned short* const* s = key ? _globals.GetMemCardString(key) : nullptr;
  for (const unsigned short* t = s ? *s : nullptr; t && *t; ++t) v.push_back(*t);
  return v;
}

// The common part of every dialog these screens spawn (the UIDialog fields set before SpawnDialog):
// mode 3, no icon, no title.
void SetDialog(UIDialog& d, int numSelections, int preselect, int continueType, int field1C) {
  d.m_numSelections = numSelections;
  d.m_preselect = preselect;
  d.m_mode = 3;
  d.m_continueType = continueType;
  d.m_field10 = 0;
  d.m_field14 = 0;
  d.m_field1C = field1C;
  d.m_icon = nullptr;
  d.m_iconW = d.m_iconH = 0;
}
void SetTexts(UIDialog& d, const char* body, const char* s0, const char* s1, const char* s2) {
  d.m_body = MemCardText(body);
  d.m_sel[0] = MemCardText(s0);
  d.m_sel[1] = MemCardText(s1);
  d.m_sel[2] = MemCardText(s2);
}
// Only the player using the screen answers (player 2: G2D's player 2).
void SetPlayers(UIDialog& d, int player) { d.m_players = player == 1 ? 2 : 1; }

// UIObjectBase::pushAptButtonFilterALL (80177B44): a filter passing every command.
unsigned int PushFilterAll(int player, const char* name) {
  AptViewer* v = Viewer();
  unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}

}  // namespace

// The game's save file on the card (803DAB08; s_saveFileName and s_configFileName point to it), laid
// out as NghResFile's s_nghLayout (nghsave.cpp).
char s_saveFileName[] = "Sims 2 Save";

// 80074DE0
// Asks the card layer how many handles the file still has (a debug check).
void CheckForFileDescriptorLeaks(char* name, int card) { _pMemoryCard->GetFileRefCount(name, static_cast<unsigned int>(card)); }
// 80074E20
// Nothing on GameCube.
RET_UNKNOWN InvalidatePS2SavedGame(int) { return 1; }
// 80074E28
bool IsConfigFileValid(int card, OptionsRecon& options) { return options.ReadIn(card) == 1; }

// ---------------------------------------------------------------------------------------------------
// SimsMemCardWrap

// 80074E60
// The game code and the two save types: the config file (one block) and the game.
void SimsMemCardWrap::Init() {
  _pMemoryCard->SetGameCode(const_cast<char*>("BASLUS-21265"));
  _pMemoryCard->SetupSaveTypes(EMC_SaveType{}, 0x2000);  // type 1
  _pMemoryCard->SetupSaveTypes(EMC_SaveType{}, static_cast<unsigned int>(GetTotalSaveGameSize()));  // type 2
  const unsigned int size = GetSpaceRequiredForSave();
  std::fprintf(stderr, "\n----> Save Game Size is %u bytes\n      (i.e. %u NGC blocks)\n\n", size, size >> 13);
}
// 80074F2C
int SimsMemCardWrap::InitPolling() {
  int r = 1;
  bool present = false;
  for (int card = 0; card < 2 && (r = IsCardInSlot(card, present)) == 1; ++card) {
  }
  return r;
}
// 80074F7C
void SimsMemCardWrap::PollCards(bool force) { _pMemoryCard->PollCards(force); }
// 80074FB8
int SimsMemCardWrap::IsPolledCardInSlot(int card, bool& present) {
  present = _pMemoryCard->IsPolledCardAvailable(static_cast<unsigned int>(card)) != 0;
  return 1;
}
// 80075008
int SimsMemCardWrap::IsCardInSlot(int card, bool& present) {
  present = _pMemoryCard->IsCardAvailable(static_cast<unsigned int>(card)) != 0;
  return 1;
}
// 80075058
int SimsMemCardWrap::IsWrongDevice(int card, bool& wrong) {
  wrong = _pMemoryCard->IsWrongDevice(static_cast<unsigned int>(card));
  return 1;
}
// 800750A8
int SimsMemCardWrap::IsCardFormatted(int card, bool& formatted) {
  return _pMemoryCard->IsCardFormated(static_cast<unsigned int>(card), formatted);
}
// 800750E8
int SimsMemCardWrap::IsCardCorrupted(int card, bool& corrupted) {
  bool formatted = false;
  corrupted = _pMemoryCard->IsCardFormated(static_cast<unsigned int>(card), formatted) == -4;
  return 1;
}
// 80075150
int SimsMemCardWrap::IsCardDamaged(int card, bool& damaged) {
  bool formatted = false;
  damaged = _pMemoryCard->IsCardFormated(static_cast<unsigned int>(card), formatted) == -8;
  return 1;
}
// 800751B4
int SimsMemCardWrap::HasSavedGame(int card, bool& has, int) {
  return _pMemoryCard->DoesFileExist(s_saveFileName, static_cast<unsigned int>(card), has);
}
// 800751F8
int SimsMemCardWrap::HasValidSavedGame(int card, bool& has, int) {
  bool exists = false;
  int r = _pMemoryCard->DoesFileExist(s_saveFileName, static_cast<unsigned int>(card), exists);
  if (r != 1) return r;
  if (exists) {
    bool valid = false;
    r = static_cast<int>(NghResFile::IsSaveGameValid(s_saveFileName, card, valid));
    if (r != 1) return r;
    if (!valid) {
      has = false;
      return 1;
    }
  }
  has = exists;
  return 1;
}
// 800752B4
int SimsMemCardWrap::HasInvalidGame(int card, bool& invalid, int) {
  bool valid = true;
  NghResFile::IsSaveGameValid(s_saveFileName, card, valid);
  invalid = !valid;
  return 1;
}
// 80075304
int SimsMemCardWrap::HasSpaceForSave(int card, bool& space, unsigned int* needed) {
  if (needed) *needed = 0;
  static_cast<ENgcMemoryCard*>(_pMemoryCard)->IsEnoughFiles(static_cast<unsigned int>(card), 1, space);
  if (space) _pMemoryCard->IsSpaceAvailable(static_cast<unsigned int>(card), GetSpaceRequiredForSave(), space);
  return 1;
}
// 80075394
// In the slot, the right device, not damaged or corrupt, formatted, and holding a save or room for one.
int SimsMemCardWrap::CanBeUsed(int card, bool& usable, int) {
  bool present = false;
  int r = IsCardInSlot(card, present);
  usable = false;
  if (!present) return r;
  bool flag = false;
  if ((r = IsWrongDevice(card, flag)), flag) return r;
  if ((r = IsCardDamaged(card, flag)), flag) return r;
  if ((r = IsCardCorrupted(card, flag)), flag) return r;
  bool formatted = false;
  r = IsCardFormatted(card, formatted);
  if (!formatted) return r;
  bool saved = false;
  HasSavedGame(card, saved, 0xabcdef);
  if (!saved) {
    bool space = false;
    r = HasSpaceForSave(card, space, nullptr);
    if (!space) return r;
  }
  usable = true;
  return 1;
}
// 800754CC
// Is the card another one than the loaded game came from (always, while the game is the default
// neighbourhood).
int SimsMemCardWrap::IsDifferentCard(int card, bool& different) {
  NghResFile* ngh = Globs_pNghResFile;
  if (!ngh || ngh->m_field20 == 1) {
    different = true;
    return 1;
  }
  const int r = ngh->HasCardChanged(card, different);
  if (r != 1) different = true;
  CheckForFileDescriptorLeaks(s_saveFileName, card);
  return r;
}
// 80075548
int SimsMemCardWrap::LoadConfigFile(int card) {
  OptionsRecon* o = Options();
  if (!o) return 0;
  const int r = static_cast<int>(o->ReadIn(card));
  o->PreservePrefsForOnline();
  return r;
}
// 80075590
int SimsMemCardWrap::SaveConfigFile(int card) {
  OptionsRecon* o = Options();
  const int r = o ? static_cast<int>(o->WriteOut()) : 0;
  if (r != 1) InvalidatePS2SavedGame(card);
  return r;
}
// 800755DC
void SimsMemCardWrap::PreservePrefs() {
  if (OptionsRecon* o = Options()) o->PreservePreferences();
}
// 80075604
void SimsMemCardWrap::RestorePrefs() {
  if (OptionsRecon* o = Options()) o->RestorePreferences();
}
// 8007562C
// OptionsRecon +0xC.
int SimsMemCardWrap::IsFreeplayUnlocked() { return Options() ? Options()->m_opt0C : 0; }
// 8007563C
int SimsMemCardWrap::FormatCard(int card) { return _pMemoryCard->FormatCardS(static_cast<unsigned int>(card)); }
// 80075678
int SimsMemCardWrap::DeleteFile(int card, int) { return _pMemoryCard->DeleteDataS(s_saveFileName, static_cast<unsigned int>(card)); }
// 800756B8
void SimsMemCardWrap::XboxOnNoMemory() {}
// 800756BC
unsigned int SimsMemCardWrap::GetSpaceRequiredForSave() { return static_cast<unsigned int>(GetTotalSaveGameSize()); }
// 800756DC
// A new neighbourhood into the card cache (house 9 being entered, saved into the file's resources)
// and the options.
int SimsMemCardWrap::SaveNewGame(int card) {
  if (Globs_pNeighborhood && Globs_pNghResFile) {
    Globs_pNeighborhood->SetNeighborhoodVar(NeighborhoodImpl::kVarEnteredHouse, 9);
    Globs_pNeighborhood->Save(Globs_pNghResFile, Globs_iSaveFileVersion);
  }
  int r = Globs_pNghResFile ? Globs_pNghResFile->WriteToMemoryCard(s_saveFileName, static_cast<unsigned char>(card)) : 0;
  if (r == 1) r = SaveConfigFile(card);
  CheckForFileDescriptorLeaks(s_saveFileName, card);
  return r;
}
// 8007578C
// GC: first copied the options' two names (+0xA0, +0x58) to the player's slots (DAT_804858E8/F0,
// online play; not ported).
int SimsMemCardWrap::PreloadGame(int card) {
  const int r = static_cast<int>(NghResFile::ReadChecksumFromCard(s_saveFileName, card));
  CheckForFileDescriptorLeaks(s_saveFileName, card);
  return r;
}
// 8007581C
// The card cache, the options (checked first) and the neighbourhood file.
int SimsMemCardWrap::LoadGame(int card, int) {
  int r = LoadCacheFromMemoryCard(card) ? 1 : 0;
  if (r == 1) {
    OptionsRecon options;
    if (!IsConfigFileValid(card, options)) {
      r = -4;
    } else if (Globs_pNghResFile) {
      r = Globs_pNghResFile->ReadFromMemoryCard(s_saveFileName, static_cast<unsigned char>(card));
      if (r == 1 && Options()) *Options() = options;
    }
    CheckForFileDescriptorLeaks(s_saveFileName, card);
  }
  return r;
}
// 800758BC
// The game into the card cache (the neighbourhood saves itself into the NghResFile, which writes its
// sections), the options, then (`toCard`) the cache onto the card. `pristine` (or DAT_80475C74) first
// resets the neighbourhood (HoodManager::ResetToPristine). In story mode the player's
// family lives in the house being played.
int SimsMemCardWrap::SaveGame(int card, bool toCard, bool pristine, int) {
  if (Globs_pNeighborhood && Globs_pNghResFile) {
    if (_globals.m_house && !_gd.m_fieldE8) Globs_pNeighborhood->SetPlayerFamilyInHouse(_globals.m_house->m_house);
    Globs_pNeighborhood->Save(Globs_pNghResFile, Globs_iSaveFileVersion);
  }
  // PC development aid: SIMMY_PRISTINE_SAVE=1 saves as author mode would (the pristine reset first).
  if (std::getenv("SIMMY_PRISTINE_SAVE")) pristine = true;
  if ((pristine || _gd.m_fieldF0) && !HoodManager::ResetToPristine()) return 0;
  if (!Globs_pNghResFile) return 0;
  int r = Globs_pNghResFile->WriteToMemoryCard(s_saveFileName, static_cast<unsigned char>(card));
  if (r == 1) {
    r = SaveConfigFile(card);
    CheckForFileDescriptorLeaks(s_saveFileName, card);
    if (toCard) r = SaveAllMemoryCardCache(card) ? 1 : 0;
  }
  return r;
}
// 800759D4
int SimsMemCardWrap::LoadHouse(int house) {
  int r = 0;
  if (Globs_pNghResFile) {
    if (!_gd.m_fieldE8 && !_gd.m_fieldF0) Globs_pNghResFile->FlushAllHouseContents();
    r = static_cast<int>(Globs_pNghResFile->LoadHouseContents(static_cast<unsigned int>(house), nullptr, false));
  }
  _pEngine->m_resetAllowed = 1;
  return r;
}
// 80075A64
int SimsMemCardWrap::ReloadHouseAndGlobals(int card, int house) {
  const int r = Globs_pNghResFile ? static_cast<int>(Globs_pNghResFile->ReloadHouseAndGlobals(
                                        static_cast<unsigned char>(card), static_cast<unsigned int>(house)))
                                  : 0;
  _pEngine->m_resetAllowed = 1;
  CheckForFileDescriptorLeaks(s_saveFileName, card);
  return r;
}
// 80075ABC
int SimsMemCardWrap::SaveHouseAndGlobals(int card, int house) {
  const int r = Globs_pNghResFile ? static_cast<int>(Globs_pNghResFile->SaveHouseAndGlobals(
                                        static_cast<unsigned char>(card), static_cast<unsigned int>(house)))
                                  : 0;
  CheckForFileDescriptorLeaks(s_saveFileName, card);
  return r;
}

// ---------------------------------------------------------------------------------------------------
// M2MTarget

namespace {
char szItemID[] = "M2M_itemN";
char szItemButtonID[] = "M2M_item_btnN";
char szItemTextID[] = "M2M_item_textN";
char szOptionButtonID[] = "M2M_optbutN";
}  // namespace

// 801BF058
// Loads the screen and starts by MEM_source: 0 the main menu (load, or the new game's boot check),
// 2 leaving create-a-sim, 4 save from the pause menu, 7 quitting the game.
M2MTarget::M2MTarget() {
  AttachTables("0M2MTarget");
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  m_onSelection = [this](int s) { SelectionCallback(s); };
  m_onZeroInput = [this] { ZeroInputCallback(); };
  m_onClosed = [this] { DialogClosedCallback(); };
  for (const char* n : {"0M2M_exists", "0M2M_swf_boxWidth", "0M2M_swf_boxHeight", "0M2M_swf_boxHeight2", "0M2M_body_text"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"0M2M_DPAD_UP", "0M2M_DPAD_DOWN", "0M2M_X_BUTTON", "0M2M_SELECT_BUTTON", "0M2M_CANCEL_BUTTON"})
    m_setTable->InstallCommand(n, set, this);
  m_locTable->InstallCommand("0M2M_body_text", loc, this);
  for (int i = 0; i < 8; ++i) m_locTable->InstallCommand(("0M2M_item_text" + std::to_string(i)).c_str(), loc, this);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0M2M_icon"), 0x9f93ff74u, const_cast<char*>(m_name), 0);
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("M2M_NAVIGATE_SOUND"), 0x64ac15u, const_cast<char*>(m_name));
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("M2M_SELECT_SOUND"), 0x374bdceau, const_cast<char*>(m_name));
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("M2M_BADSELECT_SOUND"), 0x48ae94fu, const_cast<char*>(m_name));
  const int source = UIDB::UIDBGetInt(const_cast<char*>("MEM_source"));
  m_fromCAS = source == 2;
  m_player = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
  PushButtonFilters();
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  UIScreenManager* usm = ScreenManager();
  if (usm && !usm->IsScreenManaged(0x30)) usm->LoadUIScreen(0x30, m_player);
  UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), -1);
  if (source == 2) {
    m_flow = 0xb;
    SpawnSaveBeforeQuitCASDialog();
  } else if (source == 0) {
    MMUTarget* mmu = MainMenu();
    if (!mmu || mmu->m_topLevelChoice != 2) {
      m_flow = 0;
      m_pending = 1;
    } else {
      StartNGCMemoryCardBootChecks();
    }
  } else if (source == 4) {
    m_flow = 0xf;
    m_widgetState = kFill;
  } else if (source == 7) {
    // A player in social mode or the simulator busy (global 0x18) also rules out saving.
    const bool busy = _globals.IsPlayerInSocialMode(0) || _globals.IsPlayerInSocialMode(1) ||
                      (Globs_pSimulator && Globs_pSimulator->GetGlobal(0x18) != 0);
    if (!EGlobal::IsSaveGameEnabled() || busy) {
      m_flow = 0xc;
      SpawnNoSaveBeforeQuitDialog();
    } else {
      m_flow = 0xb;
      SpawnSaveBeforeQuitGameDialog();
    }
  }
  UIDB::UIDBSetInt(const_cast<char*>("MEM_result"), 0);
  m_bodyText.clear();
  for (auto& item : m_items) item.clear();
}

// 801BFCDC
M2MTarget::~M2MTarget() {
  if (_gd.m_casTarget) _gd.m_casTarget->SetHandleInput(true);  // DAT_80475BC0: back in CAS, it takes the pad again
  if (m_widgetState != kDone) UnloadDialog();
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
  delete m_op;
  m_op = nullptr;
}

// 801BFEE8
// The dialog box swf's size (432 wide; the heights from SetupWidgetLayout).
char* M2MTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  char* out = static_cast<char*>(SimsAptMalloc(0x80));
  out[0] = 0;
  if (std::strcmp("M2M_exists", name) == 0) std::strcpy(out, "loaded");
  else if (std::strcmp("M2M_swf_boxWidth", name) == 0) std::snprintf(out, 0x80, "%i", 0x1b0);
  else if (std::strcmp("M2M_swf_boxHeight", name) == 0) std::snprintf(out, 0x80, "%i", m_boxHeight + 32);
  else if (std::strcmp("M2M_swf_boxHeight2", name) == 0) std::snprintf(out, 0x80, "%i", m_boxHeight2 + 32);
  return out;
}

// 801C00A0
// The body ("choose a memory card to load from / save to") and the list's items, with tokens
// substituted; nothing while the widget is hidden.
unsigned short* M2MTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  std::vector<unsigned short> text(0x1000, 0);
  if (m_widgetState != kHidden) {
    if (std::strcmp("M2M_body_text", name) == 0) {
      if (m_flow == 9) {
        if (m_result.device == 0xff) m_bodyText = MemCardText("ngc_loadgame_cardselect_body");
        else if (m_result.saveId == 0xff) m_bodyText = MemCardText(nullptr);
      } else if (m_flow == 0xf) {
        if (m_result.device == 0xff) m_bodyText = MemCardText("ngc_savegame_cardselect_body");
        else if (m_result.saveId == 0xff) m_bodyText = MemCardText(nullptr);
      }
      std::copy(m_bodyText.begin(), m_bodyText.end(), text.begin());
    } else if (std::strncmp("M2M_item_text", name, 13) == 0) {
      if (m_result.device == 0xff) GetItemTextMemCardSelect(name[13] - '0', text.data());
      else if (m_result.saveId == 0xff) GetItemTextSaveGameSelect(name[13] - '0', text.data());
    }
  }
  UIQDTarget::UIQDSubstitutionStrings(text.data());
  size_t n = 0;
  while (text[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::copy(text.begin(), text.begin() + static_cast<long>(n) + 1, out);
  return out;
}

// 801C02EC
// X picks the selected slot; up/down move; cancel backs out.
void M2MTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (std::strcmp("M2M_X_BUTTON", name) == 0) {
    if (IsItemEnabled(m_selection) && m_active && m_widgetState == kShown) {
      OnSlotSelectItemSelected(m_selection, m_result);
      UIDB::UIDBSetInt(const_cast<char*>("SelectedMemoryCard"), m_result.device);
      MUSlotStatus = m_result.device;
      HideWidget();
    }
  } else if (std::strcmp("M2M_DPAD_UP", name) == 0) {
    if (m_widgetState == kShown) UpdateSelection(-1);
  } else if (std::strcmp("M2M_DPAD_DOWN", name) == 0) {
    if (m_widgetState == kShown) UpdateSelection(1);
  } else if (std::strcmp("M2M_SELECT_BUTTON", name) != 0 && std::strcmp("M2M_CANCEL_BUTTON", name) == 0) {
    if (m_active && m_widgetState == kShown) OnCancel();
  }
}

// 801C0464
// The selected item shows its button and white text (red when disabled), the others blue (dark red).
void M2MTarget::ReflowButtonSelection() {
  for (int i = 0; i < m_numItems; ++i) {
    szItemButtonID[12] = static_cast<char>('0' + i);
    szItemTextID[13] = szItemButtonID[12];
    const bool enabled = IsItemEnabled(i) != 0;
    const bool selected = m_selection == i;
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id(szItemButtonID, 0);
    m_reflow.Reflow_Visibility(selected);
    m_reflow.Reflow_Id(szItemTextID, 0);
    m_reflow.Reflow_ColorOverride(selected ? (enabled ? 0xffffffffu : 0xff0000ffu) : (enabled ? 0x050671ffu : 0x7f0000ffu));
    m_reflow.SubmitReflowBlock();
  }
}

// 801C05A4
// Waits for the dialog box; shows or hides the list; acts on a pending flow state (load or new game
// from the main menu, no space, no card); polls the slots while the list is up; and takes the
// running load or save's result.
void M2MTarget::Update() {
  if (m_widgetState == kDone) return;
  if (DialogBusy()) return;
  G2DTarget* g2d = G2D();
  if ((!g2d || !g2d->IsG2DSpawned()) && m_lastWidgetState != m_widgetState) UpdateWidgetState();
  if (m_widgetState == kUnloading) {
    UIScreenManager* usm = ScreenManager();
    if (usm && !usm->IsUIScreenUnloaded(0x30)) return;
    UnloadDialog();
    m_widgetState = kDone;
    return;
  }
  if (m_pending) {
    m_pending = 0;
    switch (m_flow) {
      case 7: SpawnNoSpaceDialog(); break;
      case 0: {
        MMUTarget* mmu = MainMenu();
        if (!mmu || mmu->m_topLevelChoice == 0) PrepareLoadGame();
        else if (mmu->m_topLevelChoice == 2) PrepareNewGame();
        break;
      }
      case 4: SpawnConfirmStartNewGameDialog(); break;
      case 8:
        m_flow = 6;
        SpawnContinueNoMemCardDialog();
        break;
      case 0xd:
        m_flow = 0xe;
        SpawnContinueNoMemCardDialog();
        break;
      default: break;
    }
  }
  if (m_flow == 10) StartLoadedGame();
  if (m_flow == 6 && m_widgetState != kFill && DoesMemoryDeviceExist()) {
    HideDialog();
    g_loadedGameSlot2 = 0;
    g_loadedGameSlot = 0;
    m_flow = 9;
    m_widgetState = kFill;
  }
  if (m_widgetState == kShown && m_active && m_result.device == 0xff && (m_flow == 9 || m_flow == 0xf)) {
    bool a = false, b = false;
    SimsMemCardWrap::IsCardInSlot(0, a);
    SimsMemCardWrap::IsCardInSlot(1, b);
    if (m_flow == 9 && !a && !b) {
      m_flow = 6;
      SpawnContinueNoMemCardDialog();
    } else if (static_cast<int>(a) != m_cardPresent[0] || static_cast<int>(b) != m_cardPresent[1]) {
      m_widgetState = kFill;
    }
  }
  if (!m_op) return;
  const int result = m_op->GetResult();
  if (result == 4) {
    if (m_flow == 0xf || m_flow == 2) {
      OnSaveLoadSucceed();
      return;
    }
  } else {
    if (result < 5) {
      if (result == 1) {
        OnSaveLoadSucceed();
        return;
      }
      if (result < 1) return;
    } else {
      if (result == 6) {
        ReturnFailureDestination();
        return;
      }
      if (result > 5 && result != 7) {
        if (result != 8) return;
        // the card was swapped: back to choosing a slot
        delete m_op;
        m_op = nullptr;
        if (m_flow == 9) {
          m_flow = 8;
        } else if (m_flow != 0xf) {
          StartNGCMemoryCardBootChecks();
          return;
        } else {
          m_flow = 0xd;
        }
        HideWidget();
        m_pending = 1;
        m_result.saveId = 0xff;
        m_result.device = 0xff;
        return;
      }
    }
    if (m_flow == 2) m_bootResults[m_result.device & 1] = m_op->GetResult();
  }
  OnSaveLoadFailed();
}

// 801C09A0
// The answers to this screen's own dialogs.
void M2MTarget::SelectionCallback(int selection) {
  m_pending = 1;
  const int source = UIDB::UIDBGetInt(const_cast<char*>("MEM_source"));
  auto leave = [this](int destination, bool setDestination) {
    if (setDestination) UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), destination);
    HideWidget();
    m_widgetState = kLeaving;
  };
  auto retryLoad = [this](int noCardFlow, int pickFlow) {
    if (!DoesMemoryDeviceExist()) {
      m_flow = noCardFlow;
      HideWidget();
    } else {
      m_widgetState = kFill;
      m_flow = pickFlow;
    }
  };
  switch (m_flow) {
    case 3:  // no memory card at boot: retry / continue without saving / (3 cancel)
      if (selection == 1) {
        m_flow = 0;
        m_result.saveId = 0xff;
        m_result.device = 0xff;
      } else if (selection == 3) {
        m_flow = 1;
        m_pending = 0;
      } else if (selection == 0) {
        StartNGCMemoryCardBootChecks();
        m_pending = 0;
      }
      return;
    case 4:  // start a new game? yes / no
      if (selection == 0) m_flow = 5;
      else if (selection == 1 || selection == 3) m_flow = 1;
      return;
    case 6:
    case 9:  // no card: retry / cancel
      if (selection == 0) retryLoad(8, 9);
      else if (selection == 1 || selection == 3) m_flow = 1;
      return;
    case 0xb:  // save before quitting? yes / no / cancel
      if (selection == 0) {
        m_widgetState = kFill;
        m_flow = 0xf;
      } else if (selection == 1) {
        UIDB::UIDBSetInt(const_cast<char*>("MEM_result"), 1);
        if (source == 2) {
          leave(9, true);
        } else {
          if (source == 7) {
            if (Globs_pNghResFile) Globs_pNghResFile->FlushAllData();
            UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 0);
          }
          leave(0, false);
        }
      } else if (selection <= 3) {
        UIDB::UIDBSetInt(const_cast<char*>("MEM_result"), 0);
        if (source == 2) {
          leave(10, true);
        } else {
          if (source == 7) UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 0xb);
          leave(0, false);
        }
      }
      return;
    case 0xc:  // quit without saving? yes / no
      if (selection == 0) {
        if (Globs_pNghResFile) Globs_pNghResFile->FlushAllData();
        leave(0, true);
      } else if (selection == 1 || selection == 3) {
        leave(0xb, true);
      }
      return;
    case 0xe:
    case 0xf:  // no card (save): retry / cancel
      if (m_flow == 0xe || m_flow == 0xf) {
        if (selection == 1 || selection == 3) {
          m_flow = 0xf;
          ReturnFailureDestination();
        } else if (selection == 0) {
          retryLoad(0xd, 0xf);
        }
      }
      return;
    default: return;
  }
}

// 801C0C9C
void M2MTarget::ZeroInputCallback() {}

// 801C0CA0
void M2MTarget::DialogClosedCallback() {
  if (m_flow == 1) {
    ReturnFailureDestination();
  } else if (m_flow == 5) {
    UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 7);
    HideWidget();
    m_widgetState = kLeaving;
  }
}

// 801C0D08
// Cancel on the slot list: leaves (or, choosing a save, back to the slots).
void M2MTarget::OnCancel() {
  if (m_flow < 0xb || (m_flow > 0xc && m_flow != 0xf)) {
    if (m_result.device == 0xff) {
      m_flow = 1;
      HideWidget();
      m_pending = 1;
    } else if (m_result.saveId == 0xff) {
      m_result.device = 0xff;
      HideWidget();
    }
  } else {
    ReturnFailureDestination();
  }
}

// 801C0D94
void M2MTarget::OnSaveLoadSucceed() {
  delete m_op;
  m_op = nullptr;
  if (m_flow == 9) {
    m_flow = 10;
    HideWidget();
  } else if (m_flow == 2) {  // the boot check passed
    m_pending = 1;
    m_result.saveId = 0xff;
    m_flow = 0;
    m_result.device = 0xff;
  } else if (m_flow == 0xf) {
    UIDB::UIDBSetInt(const_cast<char*>("MEM_result"), 1);
    const int source = UIDB::UIDBGetInt(const_cast<char*>("MEM_source"));
    if (source == 4) {
      UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 0xb);
    } else if (source == 2) {
      UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 9);
    } else if (source == 7) {
      if (Globs_pNghResFile) Globs_pNghResFile->FlushAllData();
      UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 0);
    }
    HideWidget();
    m_widgetState = kLeaving;
  }
}

// 801C0EC8
// A failed boot check tries slot B, then the slot that has a card with the full checks (0x196), or
// says there is no card; other failures go back to the slot list.
void M2MTarget::OnSaveLoadFailed() {
  delete m_op;
  m_op = nullptr;
  if (m_flow == 9) {
    m_pending = 1;
    m_result.saveId = 0xff;
    m_flow = 0;
    m_result.device = 0xff;
    return;
  }
  if (m_flow == 0xf) {
    ReturnFailureDestination();
    return;
  }
  if (m_flow != 2) {
    m_result.saveId = 0xff;
    m_result.device = 0xff;
    if (m_flow == 10) m_flow = 9;
    m_widgetState = kFill;
    return;
  }
  if (m_result.device != 1) {
    m_result.device = 1;
    m_result.saveId = -1;
    UIDB::UIDBSetInt(const_cast<char*>("SelectedMemoryCard"), 1);
    if (!m_op) m_op = new SaveGameTarget(this, GetMemCardPort(), 0xfa);
    return;
  }
  if (m_bootResults[0] == 5) {
    if (m_bootResults[1] == 5) {
      m_flow = 3;
      SpawnStartNGCNoMemCardDialog();
      UIDB::UIDBSetInt(const_cast<char*>("SelectedMemoryCard"), m_result.device);
      return;
    }
    m_result.device = 1;
  } else {
    m_result.device = 0;
  }
  m_op = new SaveGameTarget(this, GetMemCardPort(), 0x196);
  UIDB::UIDBSetInt(const_cast<char*>("SelectedMemoryCard"), m_result.device);
}

// 801C1094
void M2MTarget::OnSlotSelectItemSelected(int item, ResultStruct& result) {
  if (result.device == 0xff) {
    result.device = item;
    result.saveId = -1;
  } else if (result.saveId == 0xff) {
    result.saveId = ~item;
  }
}
// 801C10C8
int M2MTarget::OnGetNumItems(ResultStruct& result) {
  if (result.device == 0xff) return 2;
  if (result.saveId == 0xff) return 8;
  return result.flag ? 0 : 4;
}
// 801C1108
int M2MTarget::GetDevice() { return m_result.device; }
// 801C1110
void M2MTarget::SetDevice(int device) {
  m_result.device = device;
  UIDB::UIDBSetInt(const_cast<char*>("SelectedMemoryCard"), device);
}
// 801C113C
int M2MTarget::GetSaveGameID() { return m_result.saveId; }
// 801C1144
int M2MTarget::GetMemCardPort() { return m_result.device != -1 ? m_result.device : m_result.saveId; }

// 801C115C
void M2MTarget::SpawnNoSpaceDialog() {
  HideDialog();
  SetTexts(*this, "ngc_ok_nospace_body", "ngc_continue", "ngc_manage_memcard", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  m_field20 = 1;
  SetPlayers(*this, m_player);
  SpawnDialog();
}
// 801C12A8
void M2MTarget::SpawnConfirmStartNewGameDialog() {
  SetTexts(*this, "ngc_yesno_loadgame_confirm_newgame_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 1, 0, 1);
  m_field20 = 1;
  SetPlayers(*this, m_player);
  SpawnDialog();
}
// 801C13EC
void M2MTarget::SpawnContinueNoMemCardDialog() {
  HideDialog();
  HideWidget();
  SetTexts(*this, "ngc_yesno_loadgame_continue_nomem_body", "retry", "cancel", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  m_field20 = 1;
  SetPlayers(*this, m_player);
  SpawnDialog();
}
// 801C1540
void M2MTarget::SpawnSaveBeforeQuitCASDialog() {
  SetTexts(*this, "ngc_yesno_savebeforecasexit_body", "yes", "no", "cancel");
  SetDialog(*this, 3, 0, 0, 1);
  m_field20 = 0;
  SetPlayers(*this, m_player);
  SpawnDialog();
}
// 801C16AC
void M2MTarget::SpawnSaveBeforeQuitGameDialog() {
  SetTexts(*this, "ngc_yesno_savebeforequit_body", "yes", "no", "cancel");
  SetDialog(*this, 3, 0, 0, 1);
  m_field20 = 0;
  SetPlayers(*this, m_player);
  SpawnDialog();
}
// 801C1818
void M2MTarget::SpawnNoSaveBeforeQuitDialog() {
  SetTexts(*this, "ngc_yesno_nosavebeforequit_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 1, 0, 1);
  m_field20 = 0;
  SetPlayers(*this, m_player);
  SpawnDialog();
}
// 801C195C
void M2MTarget::SpawnStartNGCNoMemCardDialog() {
  SetTexts(*this, "ngc_boot_nomemcard_body", "retry", "ngc_continue", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  m_field20 = 1;
  SetPlayers(*this, m_player);
  SpawnDialog();
}

// 801C1AA0
// "Memory Card Slot A" / "Memory Card Slot B".
void M2MTarget::GetItemTextMemCardSelect(int item, unsigned short* out) {
  int index = item;
  if (item == 0) m_items[0] = MemCardText("ngc_info_cardselect_memcard1");
  else if (item == 1) m_items[1] = MemCardText("ngc_info_cardselect_memcard2");
  else if (item == 2) m_items[2] = MemCardText(nullptr);
  else if (item >= 0 && item < 8) m_items[item].clear();
  else index = 0;
  const std::vector<unsigned short>& t = m_items[index];
  std::copy(t.begin(), t.end(), out);
  out[t.size()] = 0;
}
// 801C1BE8
void M2MTarget::GetItemTextSaveGameSelect(int item, unsigned short* out) {
  if (item >= 0 && item < 8) m_items[item].clear();
  out[0] = 0;
}
// 801C1C40
// A slot is enabled while a card is in it (remembered to notice cards coming and going).
int M2MTarget::IsItemEnabledMemCardSelect(int item) {
  bool present = false;
  if (item == 0 || item == 1) {
    if (SimsMemCardWrap::IsCardInSlot(item, present) != 1) present = false;
    m_cardPresent[item] = present;
    return present;
  }
  return item == 2 ? 1 : 0;
}
// 801C1D00
int M2MTarget::IsItemEnabledSaveGameSelect(int) { return 0; }
// 801C1D08
int M2MTarget::DoesMemoryDeviceExist() {
  bool a = false, b = false;
  SimsMemCardWrap::IsCardInSlot(0, a);
  SimsMemCardWrap::IsCardInSlot(1, b);
  return a || b;
}
// 801C1D70
void M2MTarget::StartLoadedGame() {
  g_loadedGameSlot = m_result.flag;
  g_loadedGameSlot2 = 0;
  SerializeUIGameData::SetupPlayerUIData();
  m_icon = nullptr;
  m_iconW = m_iconH = 0;
  UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 5);
  HideWidget();
  m_widgetState = kLeaving;
}
// 801C1DE0
// Leaving without loading or saving: the main menu (or the boot check's cancel), the pause menu.
void M2MTarget::ReturnFailureDestination() {
  if (m_flow == 0xf) {
    UIDB::UIDBSetInt(const_cast<char*>("MEM_result"), 0);
    const int source = UIDB::UIDBGetInt(const_cast<char*>("MEM_source"));
    if (source == 2) UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 10);
    else if (source == 4 || source == 7) UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 0xb);
  } else {
    MMUTarget* mmu = MainMenu();
    if (!mmu || mmu->m_topLevelChoice != 2) {
      if (Globs_pNghResFile) Globs_pNghResFile->FlushAllData();
      UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 6);
    } else {
      UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 8);
    }
  }
  HideWidget();
  m_widgetState = kLeaving;
}
// 801C1EC4
// The new game's boot check: can slot A (then B) hold the save? (SaveGameTarget, checks only.)
void M2MTarget::StartNGCMemoryCardBootChecks() {
  m_result.device = 0;
  m_bootResults[0] = m_bootResults[1] = 0;
  m_flow = 2;
  m_result.saveId = -1;
  UIDB::UIDBSetInt(const_cast<char*>("SelectedMemoryCard"), 0);
  if (!m_op) m_op = new SaveGameTarget(this, GetMemCardPort(), 0xfa);
}

// 801C1F64
// Shows the list (its items, the selection) or fills it, then enters the new widget state.
void M2MTarget::UpdateWidgetState() {
  if (m_widgetState == kShown) {
    HideDialog();
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id("M2M", 0);
    m_reflow.Reflow_Visibility(true);
    m_reflow.Reflow_Id("M2M_items", 0);
    m_reflow.Reflow_Visibility(true);
    m_reflow.Reflow_Id("M2M_body", 0);
    m_reflow.Reflow_Visibility(true);
    m_reflow.SubmitReflowBlock();
    for (int i = 0; i < 8; ++i) {
      szItemID[8] = static_cast<char>('0' + i);
      m_reflow.OpenReflowBlock();
      m_reflow.Reflow_Id(szItemID, 0);
      m_reflow.Reflow_Visibility(i < m_numItems);
      m_reflow.SubmitReflowBlock();
    }
    ReflowButtonSelection();
  } else if (m_widgetState == kFill) {
    FillWidget();
  }
  m_lastWidgetState = m_widgetState;
  SetCurrState(static_cast<WidgetState>(m_widgetState));
}
// 801C2104
void M2MTarget::FillWidget() {
  m_selection = -1;
  CalcNumItems();
  for (int i = 0; i < m_numItems; ++i)
    if (IsItemEnabled(i) && m_selection == -1) m_selection = i;
  if (m_selection == -1) m_selection = 0;
  SetupWidgetLayout();
  ReflowButtonSelection();
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("M2M_swf", 0);
  m_reflow.Reflow_SwfAnimation(const_cast<char*>("a0"));
  m_reflow.SubmitReflowBlock();
}
// 801C21D8
void M2MTarget::HideWidget() {
  m_widgetState = kHidden;
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("M2M", 0);
  m_reflow.Reflow_Visibility(false);
  m_reflow.Reflow_Id("M2M_items", 0);
  m_reflow.Reflow_Visibility(false);
  m_reflow.Reflow_Id("M2M_body", 0);
  m_reflow.Reflow_Visibility(false);
  m_reflow.SubmitReflowBlock();
}
// 801C2278
// The box around the body (AptParagraph at 12 points in 368) and the items, 30 apart; centred on
// line 224 like the dialog box.
void M2MTarget::SetupWidgetLayout() {
  if (unsigned short* t = GetLocalizable(const_cast<char*>("M2M_body_text"))) SimsAptFree(t);
  std::vector<unsigned short> body = m_bodyText;
  body.push_back(0);
  const float width = 368.0f;
  const float textHeight = AptAuxParagraphHeight(body.data(), width, 12.0f);
  float height = textHeight;
  if (m_numItems > 0) height += static_cast<float>(m_numItems) * 30.0f + 22.0f;
  m_boxHeight = static_cast<int>(height + 24.0f);
  m_boxHeight2 = static_cast<int>(height - static_cast<float>(m_numItems) * 30.0f);
  m_reflow.OpenReflowBlock();
  m_reflow.Reflow_Id("M2M_swf_container", 0);
  m_reflow.Reflow_X(0x68);
  m_reflow.Reflow_Y(static_cast<int>(224.0f - (static_cast<float>(m_boxHeight) + 32.0f) * 0.5f));
  const float top = static_cast<float>(0xe0 - (static_cast<unsigned int>(m_boxHeight) >> 1)) + 12.0f;
  m_reflow.Reflow_Id("M2M_body", 0);
  m_reflow.Reflow_X(136);
  m_reflow.Reflow_Y(static_cast<int>(top));
  m_reflow.Reflow_W(static_cast<unsigned int>(width));
  if (m_numItems > 0) {
    m_reflow.Reflow_Id("M2M_items", 0);
    m_reflow.Reflow_X(0x74);
    m_reflow.Reflow_Y(static_cast<int>(top + textHeight + 33.0f));
  }
  for (int i = 0; i < m_numItems; ++i) {
    const char c = static_cast<char>('0' + i);
    szItemButtonID[12] = c;
    m_reflow.Reflow_Id(szItemButtonID, 0);
    m_reflow.Reflow_X(-4);
    m_reflow.Reflow_Y(2);
    szOptionButtonID[10] = c;
    m_reflow.Reflow_Id(szOptionButtonID, 0);
    m_reflow.Reflow_X(0x2c);
    m_reflow.Reflow_Y(0);
    szItemTextID[13] = c;
    m_reflow.Reflow_Id(szItemTextID, 0);
    m_reflow.Reflow_X(0);
    m_reflow.Reflow_Y(7);
    m_reflow.Reflow_W(400);
  }
  m_reflow.SubmitReflowBlock();
}
// 801C294C
void M2MTarget::UpdateSelection(int delta) {
  const int old = m_selection;
  if (delta < 0) {
    if (--m_selection < 0) m_selection = m_numItems - 1;
  } else if (delta > 0) {
    if (++m_selection >= m_numItems) m_selection = 0;
  }
  if (old != m_selection) ReflowButtonSelection();
}
// 801C29C4
int M2MTarget::IsItemEnabled(int item) {
  if (m_widgetState == kHidden) return 0;
  if (m_result.device == 0xff) return IsItemEnabledMemCardSelect(item);
  if (m_result.saveId == 0xff) return IsItemEnabledSaveGameSelect(item);
  return 1;
}
// 801C2A24
void M2MTarget::CalcNumItems() { m_numItems = m_widgetState == kHidden ? 1 : OnGetNumItems(m_result); }

// 801C2A74
// Entering a widget state: shown takes input; hidden with a slot chosen starts the load or save;
// leaving goes where MEM_destination says (main menu flow states) and starts unloading.
void M2MTarget::SetCurrState(WidgetState state) {
  if (state == kShown) {
    HideDialog();
    m_active = 1;
    return;
  }
  if (state == kFill) {
    m_widgetState = kShown;
    m_active = 0;
    return;
  }
  if (state == kHidden) {
    if (m_flow == 1) {
      ReturnFailureDestination();
      return;
    }
    if (m_flow != 9 && m_flow != 0xf) return;
    if (m_result.device == 0xff || m_result.saveId == 0xff) {
      m_widgetState = kFill;
      return;
    }
    if (m_op) return;
    if (m_flow == 9) m_op = new LoadGameTarget(this, GetMemCardPort(), 0x13);
    else m_op = new SaveGameTarget(this, GetMemCardPort(), m_fromCAS ? 1 : 0);
    return;
  }
  if (state != kLeaving) return;
  HideWidget();
  HideDialog();
  switch (UIDB::UIDBGetInt(const_cast<char*>("MEM_destination"))) {
    case 0: UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MainMenu")); break;
    case 1: UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("Pause")); break;
    case 2:
    case 9: UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("ingame")); break;
    case 5: UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MainMenuLoadAccept")); break;
    case 6: UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MainMenuLoadCancel")); break;
    case 7: UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MainMenuBootCheckAccept")); break;
    case 8: UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("MainMenuBootCheckCancel")); break;
    case 10:
      UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("ingame"));
      if (_gd.m_casTarget) _gd.m_casTarget->SetHandleInput(true);
      break;
    case 0xb: UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("PAUSE_SUBMENU_GAME")); break;
    default: break;
  }
  m_widgetState = kUnloading;
}

// 801C2D80
void M2MTarget::UnloadDialog() {
  delete m_op;
  m_op = nullptr;
  PopButtonFilters();
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x30);
  m_bodyText.clear();
  for (auto& item : m_items) item.clear();
}
// 801C2E28
void M2MTarget::PrepareNewGame() {
  m_pending = 1;
  m_flow = 5;
  UIDB::UIDBSetInt(const_cast<char*>("MEM_destination"), 7);
  m_widgetState = kLeaving;
}
// 801C2E78
void M2MTarget::PrepareLoadGame() {
  m_pending = 1;
  if (Globs_pNghResFile) Globs_pNghResFile->FlushAllData();
  g_loadedGameSlot2 = 0;
  g_loadedGameSlot = 0;
  if (!DoesMemoryDeviceExist()) {
    m_flow = 8;
    HideWidget();
  } else {
    m_widgetState = kFill;
    m_flow = 9;
  }
}
// 801C2EF4
// The screen's player takes all the pad's commands.
void M2MTarget::PushButtonFilters() {
  if (!_gd.m_fieldEC || m_player != 1) {
    m_filters[0] = PushFilterAll(0, m_name);
    m_filters[1] = 0;
  } else {
    m_filters[0] = 0;
    m_filters[1] = PushFilterAll(1, m_name);
  }
}
// 801C2F6C
void M2MTarget::PopButtonFilters() {
  for (int p = 0; p < 2; ++p)
    if (m_filters[p]) {
      if (AptViewer* v = Viewer()) v->PopAptButtonFilter(p, m_filters[p]);
      m_filters[p] = 0;
    }
}
// 801E7090
int M2MTarget::GetPlayerNum() { return m_player; }

// ---------------------------------------------------------------------------------------------------
// LoadGameTarget
// States (+0x12C): -1 start, 0 checking, 1 check the card, 2 confirm, 3 load, 4 loading, 5 load
// failed, 6 card removed, 8 wrong device, 9 damaged, 10 corrupt, 12 same card, 13 wrong save,
// 14..20 formatting, 21 removed (dialog), 22 cancelled, 23 loaded, 24 delete the save, 25 no save.

// 80189AD0
LoadGameTarget::LoadGameTarget(M2MTarget* owner, int port, int flags) : m_owner(owner), m_port(port), m_flags(flags) {
  AttachTables("LoadGameTarget");
  m_onSelection = [this](int s) { SelectionCallback(s); };
  m_onZeroInput = [this] { ZeroInputCallback(); };
  m_onClosed = [] {};  // UIDialog::DialogClosedCallback
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  if (m_owner->GetDevice() == -1) UIDB::UIDBSetInt(const_cast<char*>("MemoryCardRecordSize"), -m_port);
  m_diskErrors = gNumDiskErrors;
}
// 8018A200
LoadGameTarget::~LoadGameTarget() {
  if (m_installed) {
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 8018A374
void LoadGameTarget::SelectionCallback(int selection) {
  m_pending = 1;
  const bool mustHaveSave = m_flags & 1, cancelEnds = m_flags & 0x10;
  auto cancel = [&] {  // cancel: the M2M decides, or this load just ends
    if (cancelEnds) {
      m_pending = 0;
      m_result = 6;
    } else {
      m_state = 0x16;
    }
  };
  switch (m_state) {
    case 2:  // load from this card? yes / no
      if (selection == 0) {
        m_state = 3;
        m_confirmed = 1;
      } else if (selection == 1 || selection == 3) {
        m_state = 0x16;
      }
      return;
    case 5:
    case 0xd:  // load failed / wrong save: retry / continue (delete the save) / cancel
      if (selection == 0) {
        m_state = -1;
      } else if (!mustHaveSave) {
        if (selection == 1) m_result = 3;
        else if (selection == 2) {
          m_state = 0x18;
          m_pending = 1;
          SpawnCheckingDialog();
        } else if (selection == 3) {
          m_state = 0x16;
        }
      } else {
        if (selection == 1) {
          m_pending = 1;
          m_state = 0x18;
          SpawnCheckingDialog();
        } else if (selection == 2) {
          cancel();
        } else if (selection == 3) {
          m_state = 0x16;
        }
      }
      return;
    case 8:
    case 9:
    case 0x11:  // wrong device / damaged / format failed: retry / continue / cancel
      if (selection == 0) {
        m_state = -1;
      } else if (!mustHaveSave) {
        if (selection == 1) m_result = 3;
        else if (selection == 2) cancel();
        else if (selection == 3) m_state = 0x16;
      } else if (selection == 1) {
        cancel();
      } else if (selection == 3) {
        m_state = 0x16;
      }
      return;
    case 10:  // corrupt: retry / continue / format (or retry / format / cancel)
      if (selection == 0) {
        m_state = -1;
      } else if (mustHaveSave) {
        if (selection == 2) cancel();
        else if (selection == 3) m_state = 0x16;
      } else {
        if (selection == 1) m_result = 3;
        else if (selection == 3) m_state = 0x16;
      }
      return;
    case 0xe:
    case 0xf:  // format? / erase everything? yes / no
      if (selection == 1 || selection == 3) m_state = 0x10;
      return;
    case 0x10:  // stop formatting? yes / no
      if (selection == 0) cancel();
      else if (selection == 1 || selection == 3) m_state = 10;
      return;
    case 0x14:  // formatted
      m_state = 1;
      return;
    case 0x15:  // card removed: retry / cancel
      if (selection == 0) {
        m_state = -1;
        m_swapSlot = 1;
      } else if (selection == 1 || selection == 3) {
        m_state = 0x16;
      }
      return;
    case 0x19:  // no saved game
      m_result = 6;
      return;
    default:
      m_state = 0x16;
      return;
  }
}

// 8018A6BC
void LoadGameTarget::ZeroInputCallback() {
  if (m_state == 0) {
    m_pending = 1;
    m_state = 1;
  } else if (m_state == 4 || m_state == 0x13 || m_state == 0x18) {
    m_pending = 1;
  }
}

// 8018A708
// Runs while M2M waits for the result: watches the card (removed, swapped, disk errors) and steps
// through checking, confirming, loading and formatting.
void LoadGameTarget::Update() {
  if (m_result != 0) return;
  if (DialogBusy()) return;
  if (m_state == 6) {
    m_state = 0x15;
    SpawnMemCardRemovedDialog();
  } else if (m_state == -1) {
    m_state = 0;
    SpawnCheckingDialog();
  } else if (m_state == 0x16) {
    m_result = 2;
    return;
  }
  const int s = m_state;
  if (s != 0 && s != 0x18 && s != 4 && s != 0x13 && s != 0x17 && s != 0x16 && s != 0x15) {
    if (m_diskErrors != gNumDiskErrors) {
      HideDialog();
      m_state = -1;
      m_diskErrors = gNumDiskErrors;
      return;
    }
    bool present = false;
    SimsMemCardWrap::IsCardInSlot(m_port, present);
    if (!present) {
      if (m_swapSlot) {
        m_port = 1 - m_port;
        m_owner->SetDevice(m_port);
        m_swapSlot = 0;
        m_swapped = 1;
        return;
      }
      if (m_swapped) {
        m_swapped = 0;
        HideDialog();
        m_result = 8;
        return;
      }
      HideDialog();
      m_pending = 0;
      m_state = 6;
      return;
    }
    m_swapped = 0;
    m_swapSlot = 0;
  }
  if (!m_pending) return;
  m_pending = 0;
  switch (m_state) {
    case 1:
      if (PerformMemoryCardChecks()) {
        m_state = m_confirmed ? 3 : 2;
        m_pending = 1;
      }
      return;
    case 2: SpawnLoadConfirmDialog(); return;
    case 3:
      m_loadStep = 0;
      m_state = 4;
      SpawnLoadingDialog();
      return;
    case 4: BackgroundPerformLoad(); return;
    case 5: SpawnLoadFailedDialog(); return;
    case 10:
      m_state = 0xe;
      SpawnFormatDecideDialog();
      return;
    case 0xe:
      m_state = 0xf;
      SpawnFormatConfirmDialog();
      return;
    case 0xf:
      m_state = 0x13;
      SpawnFormattingDialog();
      return;
    case 0x10: SpawnFormatCancelDialog(); return;
    case 0x11: SpawnFormatFailDialog(); return;
    case 0x13: {
      const int r = SimsMemCardWrap::FormatCard(m_port);
      HideDialog();
      _pEngine->m_resetAllowed = 1;
      int next = 0x11;
      if (r == 1) {
        bool formatted = false;
        if (SimsMemCardWrap::IsCardFormatted(m_port, formatted) == -1) {
          HideDialog();
          m_state = 6;
          return;
        }
        if (formatted) next = 0x14;
      } else if (r == -1) {
        HideDialog();
        m_state = 6;
        return;
      }
      m_pending = 1;
      m_state = next;
      return;
    }
    case 0x14: SpawnFormatSuccessDialog(); return;
    case 0x17: SpawnLoadSucceededDialog(); return;
    case 0x18:
      SimsMemCardWrap::DeleteFile(m_port, 0xabcdef);
      m_pending = 1;
      m_state = 1;
      return;
    default: return;
  }
}

// 8018AAE8
int LoadGameTarget::GetResult() { return m_result; }
// 8018AAF0
void LoadGameTarget::ImportLoad(NghResFile__SectionID, NghResFile__SaveRecord&) {}

// 8018AAF4
// Wrong device, damaged, corrupt, unformatted or empty (nothing to load), not this game's save, or
// (importing) the same card: a dialog; 1 when the card holds a save to load.
int LoadGameTarget::PerformMemoryCardChecks() {
  bool flag = false;
  auto removed = [this] {
    HideDialog();
    m_state = 6;
    return 0;
  };
  auto nothing = [this] {
    if (m_flags & 1) {
      m_state = 0x19;
      SpawnNoSaveDialog();
      return 0;
    }
    HideDialog();
    m_result = 3;
    return 0;
  };
  if (SimsMemCardWrap::IsWrongDevice(m_port, flag) == -1) return removed();
  if (flag) {
    m_state = 8;
    SpawnWrongDeviceDialog();
    return 0;
  }
  flag = true;
  if (SimsMemCardWrap::IsCardDamaged(m_port, flag) == -1) return removed();
  if (flag) {
    m_state = 9;
    SpawnCardDamagedDialog();
    return 0;
  }
  flag = true;
  if (SimsMemCardWrap::IsCardCorrupted(m_port, flag) == -1) return removed();
  if (flag) {
    m_state = 10;
    SpawnCardCorruptDialog();
    return 0;
  }
  bool formatted = false;
  const int r = SimsMemCardWrap::IsCardFormatted(m_port, formatted);
  if (r == -1) return removed();
  if (r == -6) {
    m_state = 10;
    SpawnCardCorruptDialog();
    return 0;
  }
  if (!formatted) return nothing();
  bool saved = false;
  if (SimsMemCardWrap::HasSavedGame(m_port, saved, 0xabcdef) == -1) return removed();
  if (!saved) return nothing();
  bool invalid = true;
  if (SimsMemCardWrap::HasInvalidGame(m_port, invalid, 0xabcdef) == -1) return removed();
  if (invalid) {
    m_state = 0xd;
    SpawnWrongSaveDialog();
    return 0;
  }
  if (!(m_flags & 8)) return 1;
  bool different = true;
  if (SimsMemCardWrap::IsDifferentCard(m_port, different) == -1) return removed();
  if (different) return 1;
  m_state = 0xc;
  SpawnSameCardDialog();
  return 0;
}

// 8018AD38
// Loads the game (and, importing, the other card's save into a second NghResFile); runs on the
// background thread.
int LoadGameTarget::PerformLoad() {
  int r = 1;
  m_loaded = 1;
  NghResFile* loaded = nullptr;
  if (!(m_flags & 4)) {
    NghResFile::ResetChecksums();
    r = SimsMemCardWrap::LoadGame(m_port, 0xabcdef);
    if (r == 1) {
      GameData::LoadSavedNeighborhood();
      loaded = Globs_pNghResFile;
    } else if (Globs_pNghResFile) {
      Globs_pNghResFile->FlushAllData();
    }
  }
  if (r != 1) {
    m_loaded = 0;
  } else if ((m_flags & 2) && !loaded) {
    SIMMY_STUB("LoadGameTarget::PerformLoad: importing a save (NghResFile, OptionsRecon)", 0x8018AD38);
    m_loaded = 0;
  }
  return m_loaded;
}

// The dialogs. The body is for a memory card (device 0..7); GC's other devices (-1) have none.
// 8018AE94
void LoadGameTarget::SpawnCheckingDialog() {
  m_owner->HideWidget();
  HideDialog();
  SetTexts(*this, "ngc_info_checkingdata_body", nullptr, nullptr, nullptr);
  SetDialog(*this, 0, 0, 1, 0);
  SpawnDialog();
}
// 8018AFC4
void LoadGameTarget::SpawnWrongDeviceDialog() {
  HideDialog();
  if (m_flags & 1) {
    SetTexts(*this, "ngc_ok_wrongdevice_body", "retry", "cancel", nullptr);
    SetDialog(*this, 2, 0, 0, 1);
  } else {
    SetTexts(*this, "ngc_ok_wrongdevice_body", "retry", "ngc_continue", "cancel");
    SetDialog(*this, 3, 0, 0, 1);
  }
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018B1EC
void LoadGameTarget::SpawnCardDamagedDialog() {
  HideDialog();
  if (m_flags & 1) {
    SetTexts(*this, "ngc_ok_damaged_body", "retry", "cancel", nullptr);
    SetDialog(*this, 2, 0, 0, 1);
  } else {
    SetTexts(*this, "ngc_ok_damaged_body", "retry", "ngc_continue", "cancel");
    SetDialog(*this, 3, 0, 0, 1);
  }
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018B454
void LoadGameTarget::SpawnCardCorruptDialog() {
  HideDialog();
  if (m_flags & 1) SetTexts(*this, "ngc_ok_corrupt_body", "retry", "ngc_format", "cancel");
  else SetTexts(*this, "ngc_ok_corrupt_body", "retry", "ngc_continue", "ngc_format");
  SetDialog(*this, 3, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018B68C
void LoadGameTarget::SpawnWrongSaveDialog() {
  HideDialog();
  if (m_flags & 1) SetTexts(*this, "ngc_ok_wrongsavedgame_body", "retry", "ngc_deletefile", "cancel");
  else SetTexts(*this, "ngc_ok_wrongsavedgame_body", "retry", "ngc_continue", "ngc_deletefile");
  SetDialog(*this, 3, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018B8C4
void LoadGameTarget::SpawnNoSaveDialog() {
  HideDialog();
  SetTexts(*this, "ngc_ok_nosavedgame_body", "ok", nullptr, nullptr);
  SetDialog(*this, 1, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018BA3C
void LoadGameTarget::SpawnLoadConfirmDialog() {
  HideDialog();
  SetTexts(*this, "ngc_yesno_loadgame_confirm_memcard_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018BBE0
void LoadGameTarget::SpawnLoadingDialog() {
  HideDialog();
  SetTexts(*this, "ngc_info_loadingdata_body", nullptr, nullptr, nullptr);
  SetDialog(*this, 0, 0, 1, 0);
  SpawnDialog();
}
// 8018BD08
void LoadGameTarget::SpawnLoadSucceededDialog() {
  HideDialog();
  SetTexts(*this, "ngc_ok_loadsuccess_body", "ok", nullptr, nullptr);
  SetDialog(*this, 1, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018BE24
void LoadGameTarget::SpawnLoadFailedDialog() {
  HideDialog();
  if (m_flags & 1) SetTexts(*this, "ngc_ok_loadfailed_body", "retry", "ngc_deletefile", "cancel");
  else SetTexts(*this, "ngc_ok_loadfailed_body", "retry", "ngc_continue", "ngc_deletefile");
  SetDialog(*this, 3, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018C05C
void LoadGameTarget::SpawnMemCardRemovedDialog() {
  m_owner->HideWidget();
  SetTexts(*this, "ngc_ok_loadgame_memcardremoved_body", "retry", "cancel", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018C1C0
void LoadGameTarget::SpawnSameCardDialog() {
  HideDialog();
  SetTexts(*this, "ngc_ok_importsamecard_body", "ok", nullptr, nullptr);
  SetDialog(*this, 1, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018C2DC
void LoadGameTarget::SpawnFormatDecideDialog() {
  SetTexts(*this, "ngc_yesno_format_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 1, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018C420
void LoadGameTarget::SpawnFormatConfirmDialog() {
  SetTexts(*this, "ngc_yesno_formaterase_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 1, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018C564
void LoadGameTarget::SpawnFormattingDialog() {
  SetTexts(*this, "ngc_info_formatting_body", nullptr, nullptr, nullptr);
  SetDialog(*this, 0, 0, 1, 0);
  SpawnDialog();
}
// 8018C634
void LoadGameTarget::SpawnFormatCancelDialog() {
  SetTexts(*this, "ngc_yesno_cancelformat_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018C778
void LoadGameTarget::SpawnFormatFailDialog() {
  if (m_flags & 1) {
    SetTexts(*this, "ngc_ok_badformat_body", "retry", "cancel", nullptr);
    SetDialog(*this, 2, 0, 0, 1);
  } else {
    SetTexts(*this, "ngc_ok_badformat_body", "retry", "ngc_continue", "cancel");
    SetDialog(*this, 3, 0, 0, 1);
  }
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018C97C
void LoadGameTarget::SpawnFormatSuccessDialog() {
  SetTexts(*this, "ngc_ok_goodformat_body", "ok", nullptr, nullptr);
  SetDialog(*this, 1, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}

// 8018CA90
// The load runs on the background thread with the player's pad grabbed (step 0 starts it, step 1
// waits, step 2 reports).
void LoadGameTarget::BackgroundPerformLoad() {
  if (m_loadStep == 0) {
    // GC: also lowered the resource loader's priority (_pResLoader +0x104) while loading.
    if (s_bBGComplete.load()) {
      if (AptViewer* v = Viewer())
        if (EController* pad = v->GetPlayersController(m_owner->GetPlayerNum()))
          m_filter = pad->AddFilter(0xffffffffu, const_cast<char*>("BackGroundLoadGrabControl"), 100);
      g_backgroundLoadInProgress = 1;
      BGCall_PerformLoad();
      ++m_loadStep;
    }
    m_pending = 1;
  } else if (m_loadStep == 1) {
    if (s_bBGComplete.load()) {
      g_backgroundLoadInProgress = 0;
      if (AptViewer* v = Viewer())
        if (EController* pad = v->GetPlayersController(m_owner->GetPlayerNum())) pad->RemoveFilter(m_filter);
      ++m_loadStep;
    }
    m_pending = 1;
  } else if (m_loadStep == 2) {
    HideDialog();
    if (!m_loaded) {
      m_state = 5;
      m_pending = 1;
    } else {
      m_result = 1;
    }
    m_loadStep = 0;
  }
}
// 8018CC40
void LoadGameTarget::BGCall_PerformLoad() {
  s_bBGComplete = 0;
  if (_pBackground) _pBackground->ExecuteArgRoutine(BGExec_PerformLoad, this);
  else BGExec_PerformLoad(this);
}
// 8018CC8C
void LoadGameTarget::BGExec_PerformLoad(void* self) {
  auto* t = static_cast<LoadGameTarget*>(self);
  if (t) t->PerformLoad();
  s_bBGComplete = 1;
}
// 8018CCBC
void LoadGameTarget::ImportSave(NghResFile__SectionID, NghResFile__SaveRecord&) {}
// 8018CCC0
void LoadGameTarget::ImportSaveComplete(NghResFile__SectionID, NghResFile__SaveRecord&) {}
// 8018CCC4
int LoadGameTarget::GetDevice() { return m_owner->GetDevice(); }
// 8018CCE8
int LoadGameTarget::GetPlayerNum() { return m_owner->GetPlayerNum(); }
// 8018CCF4
int LoadGameTarget::GetSaveGameID() { return m_owner->GetSaveGameID(); }

// ---------------------------------------------------------------------------------------------------
// SaveGameTarget
// States (+0x12C): 0 checking, 1 check the card, 2 wrong device, 3 damaged, 4 corrupt, 7 overwrite?,
// 8 no space, 9 format?, 10 erase all?, 11 stop formatting?, 12 format failed, 13 formatting,
// 14 format now, 15 formatted, 17 save?, 18 saving (dialog), 19 save, 20 save failed, 21 card
// removed, 22 removed (dialog), 23 cancelled, 24 saved.

// 8018CD18
SaveGameTarget::SaveGameTarget(M2MTarget* owner, int port, int flags) : m_owner(owner), m_port(port), m_flags(flags) {
  AttachTables("SaveGameTarget");
  m_onSelection = [this](int s) { SelectionCallback(s); };
  m_onZeroInput = [this] { ZeroInputCallback(); };
  m_onClosed = [] {};  // UIDialog::DialogClosedCallback
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  if (m_owner->GetDevice() == -1) UIDB::UIDBSetInt(const_cast<char*>("MemoryCardRecordSize"), -m_port);
  else UIDB::UIDBSetInt(const_cast<char*>("MemoryCardRecordSize"), port + 1);
}
// 8018D478
SaveGameTarget::~SaveGameTarget() {
  if (m_installed) {
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 8018D5EC
void SaveGameTarget::SelectionCallback(int selection) {
  m_pending = 1;
  auto cancelOr17 = [this] {  // "cancel": ends the save (0x10) or goes back to the M2M
    if (m_flags & 0x10) {
      m_pending = 0;
      m_result = 6;
    } else {
      m_state = 0x17;
    }
  };
  // retry (0) / continue (1: manage, result 4) / reset the console (2) / cancel (3)
  auto problem = [&] {
    if (selection == 1) m_result = 4;
    else if (selection == 0) m_state = 0;
    else if (selection == 2) {
      // VISetBlack, PADRecalibrate, OSResetSystem: back to the start.
      std::fprintf(stderr, "[memcard] OSResetSystem requested from the card dialog: ignored on PC\n");
      m_pending = 0;
    } else if (selection == 3) {
      cancelOr17();
    }
  };
  switch (m_state) {
    case 9:  // format? yes / no
      if (selection == 0) m_state = 10;
      else if (selection == 1 || selection == 3) m_state = 0xb;
      return;
    case 4:  // corrupt: retry / continue / format
      if (selection == 1) m_result = 4;
      else if (selection == 0) m_state = 0;
      else if (selection == 2) m_state = 9;
      else if (selection == 3) cancelOr17();
      return;
    case 2:
    case 3:
    case 8: problem(); return;
    case 0xc:  // format failed: retry / continue
      if (selection == 1) m_result = 4;
      else if (selection == 0) m_state = 0;
      else if (selection == 3) cancelOr17();
      return;
    case 7:  // overwrite? yes / no
      if (selection == 0) m_state = 0x12;
      else if (selection == 1 || selection == 3) cancelOr17();
      return;
    case 10:  // erase everything? yes / no
      if (selection == 0) m_state = 0xd;
      else if (selection == 1 || selection == 3) m_state = 0xb;
      return;
    case 0xb:  // stop formatting? yes / no
      if (selection == 0) {
        if (m_flags & 0x10) {
          m_pending = 0;
          m_result = 6;
        } else {
          m_state = 0x17;
        }
      } else if (selection == 1 || selection == 3) {
        m_state = 9;
      }
      return;
    case 0x11:  // save? yes / no
      if (selection == 0) m_state = 0x12;
      else if (selection == 1 || selection == 3) cancelOr17();
      return;
    case 0xf:  // formatted
      m_state = 0;
      return;
    case 0x16:  // card removed: retry / cancel
      if (selection == 0) {
        if (!(m_flags & 0x80)) {
          m_state = 0;
          m_swapSlot = 1;
        } else {
          m_pending = 0;
          m_result = 8;
        }
      } else if (selection == 1 || selection == 3) {
        cancelOr17();
      }
      return;
    default:
      m_state = 0x17;
      return;
  }
}

// 8018D8E0
void SaveGameTarget::ZeroInputCallback() {
  if (m_state == 0xd) {
    m_formatted = 0;
    m_pending = 1;
    m_state = 0xe;
  } else if (m_state == 0) {
    bool present = true;
    if (m_flags & 8) SimsMemCardWrap::IsCardInSlot(m_port, present);
    if (!present) {
      HideDialog();
      m_pending = 0;
      m_result = 5;
    } else {
      m_pending = 1;
      m_state = 1;
    }
  } else if (m_state == 0x12) {
    m_pending = 1;
    m_state = 0x13;
  }
}

// 8018D9BC
void SaveGameTarget::Update() {
  if (m_result != 0) return;
  if (DialogBusy()) return;
  if (m_state == 0x15) {
    m_pending = 0;
    m_state = 0x16;
    SpawnMemCardRemovedDialog();
    return;
  }
  const int s = m_state;
  if (s != 0x16 && s != 0 && s != 0x12 && s != 0xd && s != 0x15 && s != 0x17 && s != 0x14 && s != 0xe && s != 0x18 &&
      !g_backgroundSaveInProgress) {
    bool present = true;
    const int r = SimsMemCardWrap::IsCardInSlot(m_port, present);
    if (m_state == 0x13 && !present) {
      m_removedWhileSaving = 1;
    } else if (r == 1 && !present) {
      if (m_swapSlot) {
        m_port = 1 - m_port;
        m_owner->SetDevice(m_port);
        m_swapped = 1;
        m_swapSlot = 0;
        return;
      }
      if (m_swapped) {
        m_swapped = 0;
        HideDialog();
        m_result = 8;
        return;
      }
      HideDialog();
      m_state = 0x15;
      return;
    } else {
      m_swapped = 0;
      m_swapSlot = 0;
    }
  }
  if (!m_pending) return;
  m_pending = 0;
  switch (m_state) {
    case 0xe: {  // format
      m_formatOK = 0;
      const int r = SimsMemCardWrap::FormatCard(m_port);
      m_formatted = 1;
      HideDialog();
      _pEngine->m_resetAllowed = 1;
      if (r == 1) {
        m_formatOK = 1;
        bool formatted = false;
        const int f = SimsMemCardWrap::IsCardFormatted(m_port, formatted);
        m_state = f == -1 ? 0x15 : (formatted ? 0xf : 0xc);
        m_pending = 1;
      } else if (r != -1) {
        m_pending = 1;
        m_state = 0xc;
      } else {
        m_state = 0x15;
      }
      return;
    }
    case 0x13: {
      const int step = m_saveStep;
      if (step < 6) {
        if (step < 3) {
          if (step != 0) return;
          m_pending = 1;
          m_startTime = static_cast<float>(_pClockMan->GetSec(&_sysclock));
          m_saved = PerformSaveGame();
          m_saveStep = 3;
        }
      } else if (step > 10 || step < 7) {
        return;
      }
      PerformSaveGame();
      return;
    }
    case 0x11: SpawnSaveConfirmDialog(); return;
    case 0xf: SpawnFormatSuccessDialog(); return;
    case 0x12:  // checks passed: done (checks only), or save
      if (m_flags & 2) {
        HideDialog();
        m_result = 1;
        return;
      }
      SpawnSavingDialog();
      m_saveStep = 0;
      m_removedWhileSaving = 0;
      return;
    case 0x17: m_result = 2; return;
    case 0x14: SpawnSaveFailedDialog(); return;
    case 0x18: SpawnSaveSucceededDialog(); return;
    case 10: SpawnFormatConfirmDialog(); return;
    case 0xc: SpawnFormatFailDialog(); return;
    case 0xb: SpawnFormatCancelDialog(); return;
    case 0xd: SpawnFormattingDialog(); return;
    case 1:
      if (PerformMemoryCardChecks()) {
        m_pending = 1;
        m_state = 0x12;
      }
      return;
    case 0: SpawnCheckingDialog(); return;
    case 9: SpawnFormatDecideDialog(); return;
    default: return;
  }
}

// 8018DDE4
void SaveGameTarget::Hide() { HideDialog(); }
// 8018DE08
int SaveGameTarget::GetResult() { return m_result; }

// 8018DE10
// Wrong device, damaged, corrupt, unformatted (format?), a save already there (overwrite?) or no room.
// The boot check (0x40) asks nothing: any problem just fails.
int SaveGameTarget::PerformMemoryCardChecks() {
  const bool quiet = m_flags & 0x40;
  auto removed = [this] {
    HideDialog();
    m_state = 0x15;
    return 0;
  };
  auto quietFail = [this] {
    HideDialog();
    m_pending = 1;
    m_state = 0x17;
    return 0;
  };
  bool flag = false;
  if (SimsMemCardWrap::IsWrongDevice(m_port, flag) == -1) return removed();
  if (flag) {
    if (quiet) return quietFail();
    m_state = 2;
    SpawnWrongDeviceDialog();
    return 0;
  }
  flag = true;
  if (SimsMemCardWrap::IsCardDamaged(m_port, flag) == -1) return removed();
  if (flag) {
    if (quiet) return quietFail();
    m_state = 3;
    SpawnCardDamagedDialog();
    return 0;
  }
  flag = true;
  if (SimsMemCardWrap::IsCardCorrupted(m_port, flag) == -1) return removed();
  if (flag) {
    if (quiet) return quietFail();
    m_state = 4;
    SpawnCardCorruptDialog();
    return 0;
  }
  bool formatted = false;
  const int r = SimsMemCardWrap::IsCardFormatted(m_port, formatted);
  if (r == -1) return removed();
  if (r == -6) {
    if (quiet) return quietFail();
    m_state = 4;
    SpawnCardCorruptDialog();
    return 0;
  }
  if (!formatted) {
    if (quiet) return quietFail();
    m_state = 9;
    SpawnFormatDecideDialog();
    return 0;
  }
  bool saved = false;
  if (SimsMemCardWrap::HasSavedGame(m_port, saved, 0xabcdef) == -1) return removed();
  if (saved) {
    if (m_flags & 2) return 1;
    m_state = 7;
    SpawnOverwriteDialog();
    return 0;
  }
  bool space = false;
  unsigned int needed = 0;
  if (SimsMemCardWrap::HasSpaceForSave(m_port, space, &needed) == -1) return removed();
  if (space) return 1;
  if (m_flags & 0x20) {
    HideDialog();
    m_result = 7;
    m_state = 0x17;
    return 0;
  }
  if (quiet) return quietFail();
  m_state = 8;
  SpawnNoSpaceDialog(kNoSpace);
  return 0;
}

// 8018E0A8
void SaveGameTarget::SpawnCheckingDialog() {
  m_owner->HideWidget();
  HideDialog();
  SetTexts(*this, "ngc_info_checkingdata_body", nullptr, nullptr, nullptr);
  SetDialog(*this, 0, 0, 1, 0);
  SpawnDialog();
}
// 8018E1D8
void SaveGameTarget::SpawnCardCorruptDialog() {
  HideDialog();
  SetTexts(*this, "ngc_ok_corrupt_body", "retry", "ngc_continue", "ngc_format");
  SetDialog(*this, 3, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018E3A4
void SaveGameTarget::SpawnWrongDeviceDialog() {
  HideDialog();
  SetTexts(*this, "ngc_ok_wrongdevice_body", "retry", "ngc_continue", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018E508
void SaveGameTarget::SpawnCardDamagedDialog() {
  HideDialog();
  SetTexts(*this, (m_flags & 0x100) ? "ngc_ok_damaged_bootup_body" : "ngc_ok_damaged_body", "retry", "ngc_continue",
           nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018E6DC
void SaveGameTarget::SpawnFormatDecideDialog() {
  HideDialog();
  SetTexts(*this, "ngc_yesno_format_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 1, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018E840
void SaveGameTarget::SpawnFormatConfirmDialog() {
  SetTexts(*this, "ngc_yesno_formaterase_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 1, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018E99C
void SaveGameTarget::SpawnFormattingDialog() {
  SetTexts(*this, "ngc_info_formatting_body", nullptr, nullptr, nullptr);
  SetDialog(*this, 0, 0, 1, 0);
  SpawnDialog();
}
// 8018EA84
void SaveGameTarget::SpawnFormatCancelDialog() {
  SetTexts(*this, "ngc_yesno_cancelformat_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018EBE0
void SaveGameTarget::SpawnFormatFailDialog() {
  SetTexts(*this, "ngc_ok_badformat_body", "retry", "ngc_continue", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018ED3C
void SaveGameTarget::SpawnFormatSuccessDialog() {
  SetTexts(*this, "ngc_ok_goodformat_body", "ok", nullptr, nullptr);
  SetDialog(*this, 1, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018EE6C
// No room: retry / continue (and manage the card when offered, 4).
void SaveGameTarget::SpawnNoSpaceDialog(NoSpaceType) {
  HideDialog();
  if (m_flags & 4) {
    SetTexts(*this, "ngc_ok_nospace_body", "retry", "ngc_continue", "ngc_manage_memcard");
    SetDialog(*this, 3, 0, 0, 1);
  } else {
    SetTexts(*this, "ngc_ok_nospace_body", "retry", "ngc_continue", nullptr);
    SetDialog(*this, 2, 0, 0, 1);
  }
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018F0D0
void SaveGameTarget::SpawnOverwriteDialog() {
  HideDialog();
  SetTexts(*this, "ngc_yesno_overwrite_body", "yes", "no", nullptr);
  SetDialog(*this, 2, 1, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018F274
void SaveGameTarget::SpawnSaveConfirmDialog() {
  HideDialog();
  SetTexts(*this, m_owner->GetDevice() == -1 ? "ps2_yesno_savegame_confirm_hdd_body" : "ps2_yesno_savegame_confirm_memcard_body",
           "yes", "no", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018F41C
void SaveGameTarget::SpawnSavingDialog() {
  HideDialog();
  SetTexts(*this, "ngc_info_savingdata_body", nullptr, nullptr, nullptr);
  SetDialog(*this, 0, 0, 1, 0);
  SpawnDialog();
}
// 8018F544
void SaveGameTarget::SpawnSaveSucceededDialog() {
  HideDialog();
  SetTexts(*this, "ngc_ok_savesuccess_body", "ok", nullptr, nullptr);
  SetDialog(*this, 1, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018F660
void SaveGameTarget::SpawnSaveFailedDialog() {
  HideDialog();
  SetTexts(*this, "ngc_ok_savefailed_body", "ok", nullptr, nullptr);
  SetDialog(*this, 1, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}
// 8018F7D8
void SaveGameTarget::SpawnMemCardRemovedDialog() {
  m_owner->HideWidget();
  HideDialog();
  SetTexts(*this, "ngc_ok_savegame_memcardremoved_body", "retry", "cancel", nullptr);
  SetDialog(*this, 2, 0, 0, 1);
  SetPlayers(*this, m_owner->GetPlayerNum());
  SpawnDialog();
}

// 8018F944
// Saving in steps, a frame apart: 0 pause the game and get it ready, 3 grab the pad and pause the
// sound, 5 start the background save, 7 wait for it, 9 release, 10 report (shown at least 3 s on GC).
// Step 0 pauses the simulator and, in game, saves the lot into the neighbourhood file (lotsave.cpp).
int SaveGameTarget::PerformSaveGame() {
  switch (m_saveStep) {
    case 0:
      m_savedFlag = _gd.m_fieldC0;  // DAT_80475C44
      _gd.m_fieldC0 = 0;
      m_simPaused = 0;
      if (Globs_pSimulator && !Globs_pSimulator->IsPaused()) {
        Globs_pSimulator->Pause();
        m_simPaused = 1;
      }
      // GC: EVibrate::StopAllVibration when DAT_80475EDC (vibration on).
      if (m_flags & 1) return 0;
      // In game: the lot's data, the pending funds and the lot itself into the neighbourhood file
      // (GC: Globs::pHouse vtable 0x8C ComputeAndStoreLotData, done in NeighborhoodImpl::SaveHouse's
      // stand-in on PC; simulator 0x18C; neighbourhood 0x4C).
      if (Globs_pSimulator) Globs_pSimulator->CommitPendingFunds();
      if (Globs_pNeighborhood && Globs_pNghResFile && _globals.m_house) Globs_pNeighborhood->SaveHouse(Globs_pNghResFile);
      return 0;
    case 3:
      if (!s_bBGComplete.load()) break;
      m_saveResult = 0;
      if (AptViewer* v = Viewer())
        if (EController* pad = v->GetPlayersController(m_owner->GetPlayerNum()))
          m_filter = pad->AddFilter(0xffffffffu, const_cast<char*>("BackGroundSaveGrabControl"), 100);
      // GC: also paused the audio, lowered the resource loader and froze the screen (_pGfx).
      m_frames = 1;
      m_saveStep = 4;
      break;
    case 4:
      if (m_frames-- > 0) break;
      m_saveStep = 5;
      return PerformSaveGame();
    case 5:
      g_backgroundSaveInProgress = 1;
      BGCall_SaveGame();
      m_pending = 1;
      m_saveStep = 7;
      return 0;
    case 7:
      if (!s_bBGComplete.load()) break;
      g_backgroundSaveInProgress = 0;
      m_frames = 1;
      m_saveStep = 8;
      return PerformSaveGame();
    case 8:
      if (m_frames-- > 0) break;
      m_saveStep = 9;
      return PerformSaveGame();
    case 9:
      if (AptViewer* v = Viewer())
        if (EController* pad = v->GetPlayersController(m_owner->GetPlayerNum())) pad->RemoveFilter(m_filter);
      m_pending = 1;
      m_saveStep = 10;
      return 0;
    case 10:
      if (m_simPaused && Globs_pSimulator) Globs_pSimulator->Resume();
      _gd.m_fieldC0 = m_savedFlag;
      if (m_saveResult == 1) {
        UIDB::UIDBSetInt(const_cast<char*>("SelectedMemoryCard"), m_port);
        m_saved = 1;
      }
      HideDialog();
      _pEngine->m_resetAllowed = 1;
      if (!m_saved) {
        if (m_saveResult == -1 || m_removedWhileSaving) {
          m_pending = 1;
          m_state = 0x14;
        } else {
          m_state = 3;
          SpawnCardDamagedDialog();
        }
      } else {
        m_result = 1;
      }
      m_saveStep = 0xb;
      return m_saved;
    default: return 0;
  }
  m_pending = 1;
  return 0;
}
// 8018FEF0
void SaveGameTarget::BGCall_SaveGame() {
  s_bBGComplete = 0;
  if (_pBackground) _pBackground->ExecuteArgRoutine(BGExec_SaveGame, this);
  else BGExec_SaveGame(this);
}
// 8018FF3C
void SaveGameTarget::BGExec_SaveGame(void* self) {
  if (auto* t = static_cast<SaveGameTarget*>(self)) t->m_saveResult = SimsMemCardWrap::SaveGame(t->m_port, true, false, 0xabcdef);
  s_bBGComplete = 1;
}
// 8018FF8C
int SaveGameTarget::GetDevice() { return m_owner->GetDevice(); }
// 8018FFB0
int SaveGameTarget::GetPlayerNum() { return m_owner->GetPlayerNum(); }
// 8018FFBC
int SaveGameTarget::GetSaveGameID() { return m_owner->GetSaveGameID(); }

// PC: g_backgroundSaveInProgress for the sound mode manager (game/boxx.cpp).
int BackgroundSaveInProgressForSound() { return g_backgroundSaveInProgress; }
