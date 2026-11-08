// Port of the CAS target (CASTarget, game/targets_cas.cpp): the Create-a-Sim screens' Apt commands,
// the navigation between the screens (m_casNavigationTable, m_casConditional), the dialogs, and the
// mediator with the sims being edited. The screens' own commands go to the sub-targets (genetics,
// body, selection, fashion, misc, morph, personal, tattoo, roommates, keyboard).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern int _ngcUseWeightBlending;  // 804FF350 (engine/e_ngcrenderer_geom.cpp)

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }

// UIObjectBase::pushAptButtonFilterALL / popAptButtonFilter (each command for the player's screens).
unsigned int PushFilterAll(int player, const char* name) {
  auto* v = static_cast<AptViewer*>(_globals.m_aptViewer);
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}
void PopFilter(int player, unsigned int id) {
  if (auto* v = static_cast<AptViewer*>(_globals.m_aptViewer)) v->PopAptButtonFilter(player, id);
}

// m_casNavigationTable (80436C20): per screen, "next", "prev", whether choosing an option remembers the
// screen as the base (GotoBase), and options 0..9. Values below 0x100 are screens; 0x101 the main
// menu, 0x102 the game, 0x103 the base screen; 0x104 and up conditional navs.
struct NavEntry {
  int screen, next, prev, setBase;
  int option[10];
};
const NavEntry kNavigation[17] = {
    {3, 0, 0x104, 1, {0, 0, 0, 0, 0x108, 0x10a, 0x108, 0, 0, 0}},
    {4, 0, 0x10b, 1, {5, 10, 20, 21, 0x10c, 0x10d, 0, 0, 0, 0}},
    {5, 0, 4, 1, {0, 0, 0x10f, 0x110, 0x111, 0x111, 0x112, 4, 0, 0}},
    {6, 0, 0x113, 1, {0, 0, 0, 0x114, 0, 0, 0, 0, 0, 0}},
    {8, 0, 0x115, 1, {0, 0, 0, 0, 0, 0x116, 0, 0, 0, 0}},
    {9, 0, 0x117, 1, {0x118, 0x118, 0x118, 0x119, 0, 0, 0, 0, 0, 0}},
    {10, 0, 0x11a, 1, {13, 12, 14, 11, 0x11f, 0, 0, 0, 0, 0}},
    {11, 0, 10, 1, {0x122, 0x122, 0x122, 0x123, 10, 0, 0, 0, 0, 0}},
    {12, 0, 10, 1, {0x124, 0x124, 0x124, 10, 0, 0, 0, 0, 0, 0}},
    {13, 0, 10, 1, {0x125, 0x125, 0x125, 0x125, 10, 0, 0, 0, 0, 0}},
    {14, 0, 10, 1, {0x126, 0x126, 10, 0, 0, 0, 0, 0, 0, 0}},
    {16, 0, 0x103, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {17, 0, 0x128, 0, {0, 0, 0, 0, 0, 0, 0, 0x129, 0, 0}},
    {20, 0, 4, 1, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {21, 0, 4, 1, {0x12a, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {23, 0x12b, 0x12c, 1, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {24, 0, 0x12f, 1, {0x130, 0x131, 0x132, 0x102, 0, 0, 0, 0, 0, 0}},
};
// m_casConditional (80436FD8): a conditional nav, then where it goes when it succeeds and when not.
struct Conditional {
  int nav, ok, fail;
};
const Conditional kConditional[48] = {
    {0x104, 0x105, 0x107}, {0x105, 0x106, 0x3},   {0x106, 0x18, 0x18},   {0x107, 0x101, 0x3},
    {0x108, 0x109, 0x3},   {0x109, 0x3, 0x3},     {0x10a, 0x4, 0x4},     {0x10b, 0x3, 0x3},
    {0x10c, 0x17, 0x17},   {0x10d, 0x102, 0x10e}, {0x10e, 0x18, 0x18},   {0x10f, 0x6, 0x6},
    {0x110, 0x8, 0x8},     {0x111, 0x10, 0x10},   {0x112, 0x9, 0x9},     {0x113, 0x5, 0x5},
    {0x114, 0x5, 0x5},     {0x115, 0x5, 0x5},     {0x116, 0x5, 0x5},     {0x117, 0x5, 0xb},
    {0x118, 0x10, 0x10},   {0x119, 0x5, 0xb},     {0x11a, 0x11b, 0x4},   {0x11b, 0x11c, 0x11d},
    {0x11c, 0x102, 0xa},   {0x11d, 0x11e, 0xa},   {0x11e, 0x18, 0x18},   {0x11f, 0x4, 0x120},
    {0x120, 0x102, 0x121}, {0x121, 0x18, 0x18},   {0x122, 0x10, 0x10},   {0x123, 0x9, 0x9},
    {0x124, 0x10, 0x10},   {0x125, 0x10, 0x10},   {0x126, 0x10, 0x10},   {0x127, 0x11, 0x103},
    {0x128, 0x10, 0x10},   {0x129, 0x103, 0x5},   {0x12a, 0x4, 0x15},    {0x12b, 0x4, 0x4},
    {0x12c, 0x12d, 0x12e}, {0x12d, 0x4, 0x4},     {0x12e, 0x17, 0x17},   {0x12f, 0x101, 0x18},
    {0x130, 0x3, 0x18},    {0x131, 0xa, 0xa},     {0x132, 0x133, 0x18},  {0x133, 0x18, 0x18},
};

const unsigned short* Text(const char* row) { return EGlobal::NewUIText(row); }
const unsigned short kNoTitle[1] = {0};

void CopyText(unsigned short* dst, const unsigned short* src, size_t max) {
  size_t i = 0;
  for (; src && src[i] && i < max - 1; ++i) dst[i] = src[i];
  dst[i] = 0;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// Construction

// 80190EA0
// The mediator, four sim states and renderers; the target's Apt commands and sounds; weighted (skinned)
// geometry for the renderer; then Init.
CASTarget::CASTarget(bool skipUI) {
  AttachTables("CASTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  for (const char* n : {"CAS_ScreenTitle", "CAS_SubselectName0", "CAS_SubselectName1", "CAS_SubselectName2",
                        "CAS_SubselectName3", "CAS_SubselectName4", "CAS_SubselectName5", "CAS_SubselectName6",
                        "CAS_SubselectName7", "CAS_done"})
    m_locTable->InstallCommand(n, loc, this);
  for (const char* n : {"CAS_previousOptNum", "CAS_transition_ready", "CAS_exists"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"CAS_nav", "CAS_subnav", "CAS_gender", "CAS_tattoolocation", "CAS_drawstore",
                        "CAS_loadingscreen"})
    m_setTable->InstallCommand(n, set, this);
  const struct {
    const char* name;
    unsigned int id;
  } kSounds[] = {{"CAS_PANEL_SCROLL", 0x93914889u}, {"CAS_SELECT", 0xe5c52c1au}, {"CAS_BROWSE", 0xd2ea5ce5u},
                 {"CAS_CHANGE", 0xee6038fau},       {"CAS_CLOSE", 0xf5ad110cu},  {"CAS_ENTER_MORPH", 0x7ebef3aeu},
                 {"CAS_MORPH", 0x194a1fdeu},        {"CAS_OPEN", 0x6bbc4acbu},   {"CAS_ERROR", 0x3804219fu}};
  for (const auto& s : kSounds) UIAUDIO::Instance()->InstallEntry(const_cast<char*>(s.name), s.id, const_cast<char*>(m_name));
  m_mode = UIDB::UIDBGetInt(const_cast<char*>("CAS_mode"));
  _ngcUseWeightBlending = 1;
  m_flags = 0;
  Init(skipUI);
}

// 80191A00
CASTarget::~CASTarget() {
  while (!IsShutdown()) {}
  delete m_keyboard;
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    m_drawTable->UnInstallEntry(this);
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
  _ngcUseWeightBlending = 0;
}

// 80191C94
// The scene (unless the UI is skipped), the sub-targets, the mode's start lot and the player, the
// mediator with the first sim; a new game starts with the default names.
void CASTarget::Init(bool skipUI) {
  ESim::DeletePlayerCachedModel();
  if (!skipUI) m_scene = CreateScene(m_mode);
  m_genetics = new CASGeneticsTarget;
  if (!skipUI && m_genetics->GetCasGenetics()) m_genetics->GetCasGenetics()->LoadGrandparentPortraits();
  m_morph = new CASMorphTarget;
  m_body = new CASBodyTarget;
  m_selection = new CASSelectionTarget;
  m_misc = new CASMiscTarget;
  m_fashion = new CASFashionTarget;
  m_personal = new CASPersonalTarget;
  m_tattoo = new CASTattooTarget;
  m_roommate = new CASRoommateTarget;
  m_keyboard = new K2YTarget;
  m_mediator.SetCasMode(UIDB::UIDBGetInt(const_cast<char*>("CAS_mode")));
  _globals.m_fieldB0 = m_mode;  // DAT_80475E7C: back in live mode, the intro camera plays the CAS exit shot
  if (m_mode == 0) {
    _app.SetStartLot(9);
  } else {
    int lot = UIDB::UIDBGetInt(const_cast<char*>("CAS_lotnumber"));
    if (lot == 0) lot = GetLotNumberFromMode(m_mode);
    _app.SetStartLot(lot);
  }
  m_uiPlayer = m_player = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
  // The CAS player's screens take all its commands (player 2's when it is player 2's CAS).
  m_filters[0] = m_uiPlayer == 1 ? 0 : PushFilterAll(0, m_name);
  m_filters[1] = m_uiPlayer == 1 ? PushFilterAll(1, m_name) : 0;
  m_mediator.SetControllerFilterId(m_filters[0], 0);
  m_mediator.SetControllerFilterId(m_filters[1], 1);
  if (Globs_pSound) Globs_pSound->SetGameMode(cSoundPlayer::kCAS);
  UIDB::UIDBSetInt(const_cast<char*>("MEM_source"), 2);
  StartupMediator();
  m_curSim = 0;
  m_familyName.clear();
  if (m_mode == 3) {
    // the family's sims, last slot first (the first ends up edited)
    for (int i = kMaxSims - 1; i >= 0; --i) {
      m_flags &= ~(1u << (i % 4));
      if (_gd.m_customChars && _gd.m_customChars->m_used[i]) ChangeMediator(i);
    }
    if (_gd.m_customChars && !_gd.m_customChars->m_familyName.empty()) m_familyName = _gd.m_customChars->m_familyName;
  } else {
    m_flags &= ~(1u << (m_curSim % 4));
    ChangeMediator(m_curSim);
  }
  for (int i = 0; i < kMaxSims; ++i) {
    m_renderers[i].m_positionNum = static_cast<unsigned int>(i);
    m_renderers[i].SetMediator(&m_mediator);
  }
  if (m_mode == 0) {
    UIDB::UIDBSetString(const_cast<char*>("Player1FirstName"), Text("default_player1_first_name"));
    UIDB::UIDBSetString(const_cast<char*>("Player1LastName"), Text("default_player1_last_name"));
    UIDB::UIDBSetString(const_cast<char*>("Player2FirstName"), Text("default_player2_first_name"));
    UIDB::UIDBSetString(const_cast<char*>("Player2LastName"), Text("default_player2_last_name"));
    UIDB::UIDBSetInt(const_cast<char*>("CAS_gender"), 0);
    // the genetics screen (CASGeneticsTarget) makes the first sim and shows it
    if (!skipUI) {
      m_flags |= kNewGame;
      return;
    }
    CasEventInitSim init;
    if (CasSimDescriptionS2C* desc = m_mediator.GetEditSimDescription()) init.m_desc = *desc;
    m_mediator.PostEvent(init);
  } else if (m_mode == 3 || m_mode == 1) {
    CasSimDescriptionS2C* desc = m_mediator.GetEditSimDescription();
    UIDB::UIDBSetInt(const_cast<char*>("CAS_gender"), desc && desc->m_male != 0);
  }
  if (!skipUI) m_flags |= kNewGame;
}

// 80192298
// Leaving: the button filters go, the loading screen comes up.
void CASTarget::Shutdown() {
  if (m_shuttingDown) return;
  m_shuttingDown = 1;
  m_shutDown = 0;
  if (m_filters[0]) PopFilter(0, m_filters[0]);
  if (m_filters[1]) PopFilter(1, m_filters[1]);
  m_filters[0] = m_filters[1] = 0;
  LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{5});
}

// 80192360
// Sim `slot` into the neighbourhood's family slot (GameData's custom characters): a new game's typed
// names (overwritten by the description copied after them, as on GC), the description, the
// personality points x100 and the aspiration, the portrait; editing a neighbour replaces its data.
void CASTarget::SaveSimToFamilySlot(int slot) {
  NeighborhoodCustomChars* table = _gd.m_customChars;
  if (!table || slot < 0 || slot >= kMaxSims) return;
  table->m_used[slot] = 1;
  ENeighborhoodCustomChar& c = table->m_chars[slot];
  if (m_mode == 0) {
    const bool p1 = m_uiPlayer == 0;
    CopyText(c.m_desc.m_firstName, UIDB::UIDBGetString(const_cast<char*>(p1 ? "Player1FirstName" : "Player2FirstName")), 0x20);
    c.m_desc.m_firstName[0x1f] = 0;
    CopyText(c.m_desc.m_lastName, UIDB::UIDBGetString(const_cast<char*>(p1 ? "Player1LastName" : "Player2LastName")), 0x20);
    c.m_desc.m_lastName[0x1f] = 0;
  }
  const CasSimDescriptionS2C& d = m_simStates[slot].m_desc;
  c.m_desc = d;
  if (m_mode == 0 || m_mode == 3) {
    const signed char* pers = reinterpret_cast<const signed char*>(d.m_personality);
    c.m_outgoing = static_cast<short>(pers[0] * 100);
    c.m_nice = static_cast<short>(pers[1] * 100);
    c.m_playful = static_cast<short>(pers[2] * 100);
    c.m_neat = static_cast<short>(pers[3] * 100);
    c.m_active = static_cast<short>(pers[4] * 100);
    c.m_aspiration = d.m_zodiac;  // +0x121 (the aspiration)
  }
  c.m_faceImage = m_renderers[slot].CreateFaceImage(0x20, 0x20, 8, false, 0);  // 32x32 C8
  if (m_mode == 3 || !Globs_pNeighborhood) return;
  // Editing a neighbour (the slot's): its data replaced; player 1's sim of family 1 renames the family.
  const int id = static_cast<int>(table->m_neighborIds[slot]);
  Neighbor* n = Globs_pNeighborhood->FindNeighborByID(id);
  if (!n) return;
  FamilyImpl* f = n->GetFamily();
  const int number = f ? f->GetNumber() : 0;
  bool head = false;
  int index = slot;
  if (slot == 0) {
    head = number == 1;
    if (m_uiPlayer == 1 && _gd.m_fieldEC) index = 1;
  }
  _gd.ReplaceSimData(id, head, slot, index);
}

// 80192658
// Into the neighbourhood (unless the memory card screens came from elsewhere, MEM_source 5): a
// family (mode 3) saves each sim in its slot and empties the others, names the family and stores it;
// otherwise the player's sim goes into slot 0. 0 when a family has no sims.
int CASTarget::SaveCASDataToNeighborhood() {
  if (UIDB::UIDBGetInt(const_cast<char*>("MEM_source")) == 5) return 1;
  NeighborhoodCustomChars* table = _gd.m_customChars;
  if (m_mode == 3) {
    int first = kMaxSims;
    for (int i = 0; i < kMaxSims; ++i) {
      if (!(m_flags >> (i % 4) & 1)) {
        if (table) table->m_used[i] = 0;
      } else {
        if (i < first) first = i;
        SaveSimToFamilySlot(i);
      }
    }
    if (first >= kMaxSims) return 0;
    if (table) {
      table->m_familyName.clear();
      if (m_familyName.empty()) {
        for (const unsigned short* p = table->m_chars[first].m_desc.m_lastName; *p; ++p) table->m_familyName.push_back(*p);
      } else {
        table->m_familyName = m_familyName;
      }
    }
    if (_gd.SaveCreateAFamily() && Globs_pNeighborhood)
      Globs_pNeighborhood->GetImpl()->Save(Globs_pNghResFile, Globs_iSaveFileVersion);
  } else {
    SaveSimToFamilySlot(0);
    SIMMY_STUB("CASTarget::SaveCASDataToNeighborhood: the representation shaders (CasSimRenderer::UpdateRepShaders)",
               0x80192658);
    if (Globs_pNeighborhood) Globs_pNeighborhood->GetImpl()->Save(Globs_pNghResFile, Globs_iSaveFileVersion);
  }
  return 1;
}

// 8019282C
// Once the loading screen is up: the mediator, the sub-targets and the scene go, then the last UI
// screen; done when it is unloaded.
int CASTarget::IsShutdown() {
  if (!LoadingScreenStateMachine::s_pSM || !(LoadingScreenStateMachine::s_pSM->m_fadeFlags & LoadingScreenStateMachine::kFadedIn))
    return 0;
  if (!m_shutDown) {
    m_shutDown = 1;
    ShutdownMediator();
    delete m_genetics;
    m_genetics = nullptr;
    delete m_morph;
    m_morph = nullptr;
    delete m_body;
    m_body = nullptr;
    delete m_selection;
    m_selection = nullptr;
    delete m_misc;
    m_misc = nullptr;
    delete m_fashion;
    m_fashion = nullptr;
    delete m_personal;
    m_personal = nullptr;
    delete m_tattoo;
    m_tattoo = nullptr;
    delete m_roommate;
    m_roommate = nullptr;
    delete m_keyboard;
    m_keyboard = nullptr;
    delete m_scene;
    m_scene = nullptr;
    UnloadLastScreen();
    return 0;
  }
  UIScreenManager* usm = ScreenManager();
  if (!m_lastScreen || !usm || !usm->IsScreenManaged(m_lastScreen)) {
    m_lastScreen = 0;
    return 1;
  }
  return 0;
}

// 800C8A58 (game/neighbor.cpp)
ENeighborhoodCustomChar::ENeighborhoodCustomChar() : m_desc(false) {}

// 80192ACC
// The scene and the target listen to the mediator.
void CASTarget::StartupMediator() {
  if (m_flags & kMediatorStarted) return;
  if (m_scene) m_mediator.m_scene = m_scene;
  m_mediator.m_target = this;
  if (m_scene) m_mediator.AddListener(*m_scene);
  m_mediator.AddListener(m_listener);
  // GC: also the keyboard target (not a listener here)
  CasListener* targets[] = {m_genetics, m_morph, m_body, m_selection, m_misc, m_fashion, m_personal, m_tattoo, m_roommate};
  for (CasListener* t : targets)
    if (t) m_mediator.AddListener(*t);
  m_flags |= kMediatorStarted;
}

// 80192C50
void CASTarget::ShutdownMediator() {
  m_mediator.RemoveAllListeners();
  m_mediator.m_scene = nullptr;
  m_mediator.m_renderer = nullptr;
  m_mediator.m_editSim = nullptr;
  m_flags &= ~(kMediatorStarted | kMediatorChanged);
}

// 80192DB8
// Sim `sim` becomes the edited one: its state and renderer listen instead of the previous sim's.
// Editing modes (1, 3) post the family member's description and show it.
void CASTarget::ChangeMediator(int sim) {
  if (!(m_flags & kMediatorStarted)) return;
  if (sim == m_curSim && (m_flags & kMediatorChanged)) return;
  if (m_flags & kMediatorChanged) {
    m_mediator.RemoveListener(m_renderers[m_curSim]);
    m_mediator.RemoveListener(m_simStates[m_curSim]);
  }
  m_mediator.AddListener(m_simStates[sim]);
  m_mediator.AddListener(m_renderers[sim]);
  m_mediator.m_renderer = &m_renderers[sim];
  m_mediator.m_editSim = &m_simStates[sim];
  m_renderers[m_curSim].SetMediator(&m_mediator);
  m_renderers[sim].SetMediator(&m_mediator);
  if ((m_mode == 1 || m_mode == 3) && _gd.m_customChars) {
    // the family member's description from its slot
    CasEventInitSim init;
    init.m_desc = _gd.m_customChars->m_chars[sim].m_desc;
    m_mediator.PostEvent(init);
    if (m_mode == 3) m_renderers[sim].m_model.UpdateReweldCAS(true);
    m_flags |= 1u << (sim % 4);
  }
  m_curSim = sim;
  m_flags |= kMediatorChanged;
}

// 801930DC
CasScene* CASTarget::CreateScene(int mode) {
  if (mode < 0 || mode >= 4) return nullptr;
  auto* scene = new CasSceneDefault();
  scene->Init();
  return scene;
}

// 8019314C
// Editing an existing family starts on the neighbourhood's current lot (+1); a new game on lot 9.
int CASTarget::GetLotNumberFromMode(int mode) {
  if (mode > 0 && mode < 6) {
    SIMMY_STUB("CASTarget::GetLotNumberFromMode: Globs::pNghResFile's current lot", 0x8019314C);
    return 1;
  }
  return 9;
}

// ---------------------------------------------------------------------------------------------------
// Apt commands

// 80193174
// "CAS_nav" / "CAS_subnav" move between the UI screens and their items; "CAS_gender" (re)starts the sim
// of the other gender two frames later; "CAS_loadingscreen" brings the loading screen (0: and hides
// the UI).
void CASTarget::SetVariable(char* name, char* value) {
  if (!std::strcmp("CAS_nav", name)) {
    SetVariableNav(value);
  } else if (!std::strcmp("CAS_subnav", name)) {
    SetVariableSubnav(value);
  } else if (!std::strcmp("CAS_gender", name)) {
    const int gender = UIDB::UIDBGetInt(const_cast<char*>("CAS_gender"));
    if (gender != 1) m_flags |= 0x80;
    else m_flags &= ~0x80u;
    m_initFrames = 2;
    m_flags |= 0x100;
    m_genderTime = static_cast<float>(_pClockMan->GetSec(&_sysclock));
    UIDB::UIDBSetInt(const_cast<char*>("CAS_gender"), gender != 1);
  } else if (!std::strcmp("CAS_loadingscreen", name)) {
    const int on = std::atoi(value);
    if (m_mediator.m_mode != 0 || m_mediator.m_player != 0 || !_gd.m_fieldEC) {
      SIMMY_STUB("CASTarget::SetVariable: CAS_loadingscreen (gbDrawUI, the stencil state)", 0x80193174);
      (void)on;
      LoadingScreenStateMachine::StartingLoad(CLoadingScreen__eLoadingScreenMode{4});
    }
  }
}

// 80193334
// The screen's title, the sub-selection names (from the screen's sub-target) and "done".
unsigned short* CASTarget::GetLocalizable(char* name) {
  unsigned short text[0x1000];
  text[0] = 0;
  if (static_cast<unsigned>(*name - '0') < 2) ++name;
  if (!std::strcmp("CAS_ScreenTitle", name)) {
    const char* row = nullptr;
    switch (m_screen) {
      case 3: row = "cas_genetics"; break;
      case 4: row = "cas_main"; break;
      case 5: row = "cas_body"; break;
      case 6: row = "cas_title_bodymorph"; break;
      case 8: row = "cas_title_headmorph"; break;
      case 9: row = "cas_title_tattoo_area"; break;
      case 10: row = "cas_title_fashion"; break;
      case 11: row = "cas_title_accessories"; break;
      case 12: row = "cas_title_torso"; break;
      case 13: row = "cas_title_head_acc"; break;
      case 14: row = "cas_title_bottoms"; break;
      case 16: row = "cas_title_select_item"; break;
      case 17: row = "cas_title_fashion_design"; break;
      case 20: row = "cas_title_aspirations"; break;
      case 21: row = "cas_title_personality"; break;
      case 23: row = "cas_title_name"; break;
      case 24: row = "cas_family_title"; break;
      default: break;
    }
    if (row) CopyText(text, Text(row), 0x1000);
  } else if (!std::strncmp("CAS_SubselectName", name, 0x11)) {
    const int item = std::atoi(name + 0x11);
    switch (m_screen) {
      case 3:
        if (m_genetics) m_genetics->GetLocalizableSubnav(item, text);
        break;
      case 5:
        if (m_body) m_body->GetLocalizableSubnav(item, text);
        break;
      case 6:
      case 8:
        if (m_morph) m_morph->GetLocalizableSubnav(UIScreenID(m_screen), item, text);
        break;
      case 4:
      case 11:
      case 12:
      case 13:
      case 14:
        if (m_misc) m_misc->GetLocalizableSubnav(UIScreenID(m_screen), item, text);
        break;
      case 9:
        if (m_tattoo) m_tattoo->GetLocalizableSubnav(item, text);
        break;
      case 10:
      case 17:
        if (m_fashion) m_fashion->GetLocalizableSubnav(UIScreenID(m_screen), item, text);
        break;
      case 20:
        if (m_personal) m_personal->GetLocalizableSubnav(UIScreenID(0x14), item, text);
        break;
      case 24:
        if (m_roommate) m_roommate->GetLocalizableSubnav(item, text);
        break;
      default: break;
    }
  } else if (!std::strcmp("CAS_done", name)) {
    CopyText(text, Text("cas_done"), 0x1000);
  } else {
    return nullptr;
  }
  UIQDTarget::UIQDSubstitutionStrings(text);
  size_t n = 0;
  while (text[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, text, (n + 1) * 2);
  return out;
}

// 80193928
// "CAS_previousOptNum": the option chosen before on the screen gone back to (popped); "CAS_fromWardrobe";
// "CAS_exists" (once the new game's first sim is there); "CAS_transition_ready" (the camera has
// arrived).
char* CASTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("CAS_previousOptNum", name)) {
    int option = 0;
    if (static_cast<unsigned>(m_screen - 0x10) > 1 && !m_prevOptions.empty()) {
      option = m_prevOptions.back();
      m_prevOptions.pop_back();
    }
    if (option != -1) {
      std::snprintf(out, 0x20, "%d", option);
      return out;
    }
    if (m_screen != 0xd) return out;
    std::strcpy(out, ShowAllCFAOptions() ? "0" : "1");
    return out;
  }
  if (!std::strcmp("CAS_fromWardrobe", name)) {
    std::strcpy(out, m_mode != 1 ? "0" : "1");
  } else if (!std::strncmp("CAS_exists", name, 10)) {
    if (m_flags & kNewGame) std::strcpy(out, "loaded");
  } else if (!std::strcmp("CAS_transition_ready", name)) {
    std::strcpy(out, m_mediator.IsTransitionComplete() ? "1" : "0");
  }
  return out;
}

// 80193B78
// The scene and the current sim's renderer (all four for roommates) update; shaders animate. A gender
// change stores the sim and restarts it in the other gender two frames later.
void CASTarget::Update() {
  if (m_scene) {
    m_scene->Update(_dt);
    if (m_mode == 3) {
      for (CasSimRendererDynamic& r : m_renderers) r.Update(_dt);
    } else {
      m_renderers[m_curSim].Update(_dt);
    }
    if (m_npcEditor) m_npcEditor->Update(_dt);
    if (m_genetics) m_genetics->Update();
  }
  ERShader::UpdateAll(0.0f, 0, 0, 0);
  EShader::UpdateAll(_dt);
  if (m_initFrames == 1) {
    CasEventStoreSim store;
    m_mediator.PostEvent(store);
    CasEventResetSim reset;
    reset.m_male = static_cast<int>(m_flags >> 7 & 1);
    m_mediator.PostEvent(reset);
  }
  m_initFrames = m_initFrames - 1 < 0 ? 0 : m_initFrames - 1;
}

// 80193D70
// The scene, then the current sim (every family member in the roommate view).
void CASTarget::Draw(ERC* rc) {
  if (!m_scene) return;
  m_scene->Draw(rc);
  if (!IsViewingCAR()) {
    m_renderers[m_curSim].Draw(rc);
  } else {
    for (CasSimRendererDynamic& r : m_renderers) r.Draw(rc);
    if (m_roommate) m_roommate->Draw(rc);
  }
}

// 80193F2C
int CASTarget::UnloadLastScreen() {
  if (!m_screen) return 1;
  UIScreenManager* usm = ScreenManager();
  if (!usm) return 1;
  if (usm->IsUIScreenUnloaded(m_screen)) return 0;
  if (usm->IsUIScreenMarkedForUnloading(m_screen)) return 0;
  usm->UnloadUIScreen(m_screen);
  m_lastScreen = m_screen;
  m_screen = 0;
  return 1;
}

// 80193FB0
// "next" / "prev" / "option[_]<n>" of the current screen (kNavigation). Choosing an option remembers it
// for "CAS_previousOptNum" (except on the screens and options that come straight back), and -1 when
// the target is a screen of its own.
void CASTarget::SetVariableNav(char* value) {
  if (!m_screen) return;
  const NavEntry* e = nullptr;
  for (const NavEntry& n : kNavigation)
    if (n.screen == m_screen) {
      e = &n;
      break;
    }
  if (!e) return;
  int nav;
  if (!std::strcmp("next", value)) {
    nav = e->next;
  } else if (!std::strcmp("prev", value)) {
    nav = e->prev;
  } else {
    if (std::strncmp(value, "option", 6)) return;
    if (e->setBase) m_field98 = m_screen;
    if (std::strlen(value) < 7) return;
    const char* digits = value[6] == '_' ? value + 7 : value + 6;
    const unsigned int option = static_cast<unsigned int>(std::atoi(digits));
    if (option > 9) return;
    const int s = m_screen;
    nav = e->option[option];
    const bool noMemory = s == 3 || s == 0x18 || (s == 4 && nav == 0x10d) || (s == 5 && nav == 4) ||
                          (s == 6 && nav == 0x114) || (s == 8 && nav == 0x116) || (s == 9 && nav == 0x119) ||
                          (s == 10 && nav == 0x11f) || (s == 0xb && nav == 10) || (s == 0xc && nav == 10) ||
                          (s == 0xd && nav == 10) || (s == 0x11 && nav == 0x11) || (s == 0xe && nav == 10) ||
                          (s == 0x11 && nav == 0x129) || (s == 0x15 && nav == 0x12a) || (s == 0x17 && nav == 299);
    if (!noMemory) {
      m_prevOptions.push_back(static_cast<int>(option));
      if (nav != 0x10 && nav != 0x111 && nav != 0x118 && nav != 0x122 && nav != 0x124 && nav != 0x125 &&
          nav != 0x126)
        m_prevOptions.push_back(-1);
    }
  }
  if (nav) HandleNav(nav);
}

// 801942B8
// "SSOO": the item (sub-nav) and its option, for the screen's sub-target; a new item moves the focus.
void CASTarget::SetVariableSubnav(char* value) {
  char a[3] = {}, b[3] = {};
  std::strncpy(a, value, 2);
  std::strncpy(b, value + 2, 2);
  const int item = std::atoi(a), option = std::atoi(b);
  if (!m_screen) return;
  if (item != m_subnav) {
    m_subnav = item;
    BroadcastNewFocus();
  }
  switch (m_screen) {
    case 3:
      if (m_genetics) m_genetics->SetVariableSubnav(item, option);
      break;
    case 5:
      if (m_body) m_body->SetVariableSubnav(item, option);
      break;
    case 6:
    case 8:
      if (m_morph) m_morph->SetVariableSubnav(UIScreenID(m_screen), item, option);
      break;
    case 10:
    case 17:
      if (m_fashion) m_fashion->SetVariableSubnav(UIScreenID(m_screen), item, option);
      break;
    case 20:
      if (m_personal) m_personal->SetVariableSubnav(UIScreenID(0x14), item, option);
      break;
    case 24:
      if (m_roommate) m_roommate->SetVariableSubnav(item, option);
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------------------------------------
// Focus

namespace {
// The UI focus of a screen's sub-nav item. GC: one decision tree; an item a screen does not have falls
// through to the next screen's list (as here).
int FocusFrom(const int* table, int count, int item, int (*next)(int)) {
  if (item >= 0 && item < count) return table[item];
  return next ? next(item) : 0;
}
int F994(int s) { static const int t[] = {0x3f, 0x40, 0x41, 0x42}; return FocusFrom(t, 4, s, nullptr); }
int F11(int s) { static const int t[] = {0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e}; return FocusFrom(t, 8, s, F994); }
int F8ac(int s) { static const int t[] = {0x32, 0x33, 0x34, 0x35, 0x36}; return FocusFrom(t, 5, s, F11); }
int F86c(int s) { static const int t[] = {0x2f, 0x30, 0x31}; return FocusFrom(t, 3, s, F8ac); }
int F81c(int s) { static const int t[] = {0x2b, 0x2c, 0x2d, 0x2e}; return FocusFrom(t, 4, s, F86c); }
int F7bc(int s) { static const int t[] = {0x26, 0x27, 0x28, 0x29, 0x2a}; return FocusFrom(t, 5, s, F81c); }
int F10(int s) { static const int t[] = {0x21, 0x22, 0x23, 0x24, 0x25}; return FocusFrom(t, 5, s, F7bc); }
int F6f0(int s) { static const int t[] = {0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20}; return FocusFrom(t, 6, s, F10); }
int F6a0(int s) { static const int t[] = {0x17, 0x18, 0x19, 0x1a}; return FocusFrom(t, 4, s, F6f0); }
int F618(int s) { static const int t[] = {0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16}; return FocusFrom(t, 8, s, F6a0); }
int F4(int s) { static const int t[] = {9, 10, 0xb, 0xc, 0xd, 0xe}; return FocusFrom(t, 6, s, F618); }
}  // namespace

// 80194428
// The UI focus (which part of the sim the screen and item are about).
int CASTarget::GetCurrentUIFocus() {
  const int s = m_subnav;
  switch (m_screen) {
    case 3: {
      static const int t[] = {2, 3, 4, 5, 6, 7, 6};
      return s >= 0 && s < 7 ? t[s] : 0;
    }
    case 4: return F4(s);
    case 5: return F618(s);
    case 6: return F6a0(s);
    case 8: return F6f0(s);
    case 9: return F994(s);
    case 10: return F10(s);
    case 0xb: return F8ac(s);
    case 0xc: return F81c(s);
    case 0xd: return F7bc(s);
    case 0xe: return F86c(s);
    case 0x10: return 1;
    case 0x11: return F11(s);
    case 0x18: {
      static const int t[] = {0x43, 0x44, 0x45, 0x46};
      return s >= 0 && s < 4 ? t[s] : 0;
    }
    default: return 0;
  }
}

// 801949EC
void CASTarget::BroadcastNewFocus() {
  CasEventChangeFocus e;
  e.m_uiFocus = GetCurrentUIFocus();
  if (!e.m_uiFocus) return;
  e.m_eventFocus = ConvertUIFocusToEventFocus(e.m_uiFocus);
  e.m_prevFocus = m_prevFocus;
  e.m_prevPartFocus = m_prevPartFocus;
  m_mediator.PostEvent(e);
}

// 80194A78
// The scene's camera position for a UI focus (the selection screens use the item's).
int CASTarget::ConvertUIFocusToEventFocus(int f) {
  if (static_cast<unsigned>(m_screen - 0x10) < 2) f = m_prevPartFocus;
  if (f < 2) return 0x15;
  if (f <= 8) return 0x30;
  if (f <= 0xe) return 0x15;
  switch (f) {
    case 0xf: return 0x16;
    case 0x10: case 0x11: case 0x12: return 0x15;
    case 0x13: return 8;
    case 0x14: return 0xe;
    case 0x15: case 0x16: return 0x15;
    case 0x17: return 0;
    case 0x18: return 0;
    case 0x19: return 2;
    case 0x1a: return 0x15;
    case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x20: return 0x23;
    case 0x21: return 8;
    case 0x22: return 0;
    case 0x23: return 2;
    case 0x24: case 0x25: return 0x15;
    case 0x26: return 8;
    case 0x27: return 0xf;
    case 0x28: return 0x10;
    case 0x29: return 0xe;
    case 0x2a: return 0x15;
    case 0x2b: case 0x2c: case 0x2d: case 0x2e: return 0;
    case 0x2f: return 2;
    case 0x30: return 3;
    case 0x31: return 2;
    case 0x32: return 0;
    case 0x33: return 7;
    case 0x34: return 0;
    case 0x35: case 0x36: return 0x15;
    case 0x37: return 0;
    case 0x38: return 0;
    case 0x39: return 0x2d;
    case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: return 0x15;
    case 0x3f: return 0;
    case 0x40: return 0;
    case 0x41: return 2;
    case 0x42: return 0x15;
    case 0x43: return 0x31;
    case 0x44: return 0x32;
    case 0x45: return 0x33;
    case 0x46: return 0x34;
    default: return 0x15;
  }
}

// ---------------------------------------------------------------------------------------------------
// Navigation

// 80194CD0
// Below 0x101 a screen; 0x101 back to the main menu (sounds faded), 0x102 into the game, 0x103 back
// to the base screen; from 0x104 a conditional nav.
void CASTarget::HandleNav(int nav) {
  if (!m_handleInput) return;
  if (nav >= 0x104) {
    HandleConditionalNav(nav);
  } else if (nav == 0x102) {
    GotoGame();
  } else if (nav == 0x103) {
    GotoBase();
  } else if (nav == 0x101) {
    SoundFadeOutAndSetMode(cSoundPlayer::kLoading);
    GotoMainMenu();
  } else {
    GotoScreen(nav);
  }
}

// 80194DA4
void CASTarget::GotoScreen(int screen) {
  UIScreenManager* usm = ScreenManager();
  if (!usm || usm->IsScreenManaged(screen) || screen == m_screen) return;
  const int focus = GetCurrentUIFocus();
  m_prevFocus = focus;
  if (screen == 0x10 && m_screen != 0x11) m_prevPartFocus = focus;
  if (!UnloadLastScreen()) return;
  m_screen = screen;
  usm->LoadUIScreen(screen, m_uiPlayer);
  m_subnav = 0;
  BroadcastNewFocus();
}

// 80194E4C
// A new family starts on the genetics screen (3); editing modes on theirs.
void CASTarget::GotoInitialScreen() {
  int screen = 3;
  if (m_mode == 1 || m_mode == 5) screen = 10;
  else if (m_mode == 3) screen = 0x18;
  GotoScreen(screen);
}

// 80194EBC
void CASTarget::SetHandleInput(bool on) { m_handleInput = on; }

// 80194EC4
void CASTarget::GotoGame() {
  SaveCASDataToNeighborhood();
  UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("SlotSelect"));
  SetHandleInput(false);
}

// 80194F08
void CASTarget::GotoMainMenu() {
  UnloadLastScreen();
  if (m_mode == 3) {
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("Freeplay"));
  } else {
    const int player = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
    if (player == 0) UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("CASBackoutP1"));
    else if (player == 1) UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("CASBackoutP2"));
  }
}

// 80194F8C
void CASTarget::GotoBase() { GotoScreen(m_field98); }

// 80194FB0
// Back to the name screen: the keyboard starts on the player's first name.
void CASTarget::ReturnForwardToKeyboard() {
  const int player = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
  if (player == 0) K2YTarget::SetStartMode(2);
  else if (player == 1) K2YTarget::SetStartMode(4);
}

// 80195000
// The names typed in go into the sim.
void CASTarget::SaveNameEnterCBA() {
  const unsigned int player = static_cast<unsigned int>(UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber")));
  const unsigned short* first = nullptr;
  const unsigned short* last = nullptr;
  if (player == 0) {
    first = UIDB::UIDBGetString(const_cast<char*>("Player1FirstName"));
    last = UIDB::UIDBGetString(const_cast<char*>("Player1LastName"));
  } else if (player == 1) {
    first = UIDB::UIDBGetString(const_cast<char*>("Player2FirstName"));
    last = UIDB::UIDBGetString(const_cast<char*>("Player2LastName"));
  }
  if (player < 2)
    if (CasSimDescriptionS2C* d = m_mediator.GetEditSimDescription()) {
      CopyText(d->m_firstName, first, 0x20);
      CopyText(d->m_lastName, last, 0x20);
    }
}

// 801950C4
// Cancelled: the names go back to what they were before the keyboard.
void CASTarget::DontSaveNameEnterCBA() {
  const int player = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
  if (player == 0) {
    UIDB::UIDBSetString(const_cast<char*>("Player1FirstName"), K2YTarget::m_savedFirstName);
    UIDB::UIDBSetString(const_cast<char*>("Player1LastName"), K2YTarget::m_savedLastName);
  } else if (player == 1) {
    UIDB::UIDBSetString(const_cast<char*>("Player2FirstName"), K2YTarget::m_savedFirstName);
    UIDB::UIDBSetString(const_cast<char*>("Player2LastName"), K2YTarget::m_savedLastName);
  }
}

// 80195154
// The conditional navs (kConditional): each does its check or work, then goes on to the entry's
// success or failure nav. Some ask first (a yes/no dialog that comes back through
// DialogYesNoSelectionCallback).
void CASTarget::HandleConditionalNav(int nav) {
  const Conditional* c = nullptr;
  for (const Conditional& e : kConditional)
    if (e.nav == nav) {
      c = &e;
      break;
    }
  if (!c) return;
  bool ok = false;
  auto ask = [&](const char* row) { SpawnYesNoDialog(static_cast<unsigned int>(nav), Text(row), kNoTitle, 1); };
  switch (nav) {
    case 0x104: ok = m_mode == 4; break;
    case 0x105: ask("cas_genetics_exit_prompt"); return;
    case 0x106:
      m_mediator.SetCasMode(3);
      m_flags &= ~(1u << (m_curSim % 4));
      DeallocateMorphResources(m_curSim);
      ResetSimRotations();
      ok = true;
      break;
    case 0x107: ask("cas_genetics_exit_prompt"); return;
    case 0x108:
      if (m_genetics && m_genetics->GetWarnOnGenerate()) {
        ask("cas_genetics_gen2_warning");
        return;
      }
      ok = true;
      break;
    case 0x109:
      if (m_genetics) m_genetics->BeginGenerate();
      ok = true;
      break;
    case 0x10a:  // on to the main screen: dice no longer make the default sim
      if (m_genetics) m_genetics->SetGenerateDefaultSim(false);
      ok = true;
      ResetSimRotations();
      break;
    case 0x10b:  // back to genetics: a new generation will ask first
      if (m_genetics) m_genetics->SetWarnOnGenerate(true);
      ok = true;
      ResetSimRotations();
      break;
    case 0x10c: {
      const int player = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
      K2YTarget::SetStartMode(player == 1 ? 4 : 2);
      K2YTarget::SaveName();
      ok = true;
      break;
    }
    case 0x10d:  // done
      if (m_mode == 0) {
        ok = true;
      } else if (m_mode == 4) {
        ok = false;
        if (m_familyName.empty()) {
          const unsigned short* last =
              UIDB::UIDBGetString(const_cast<char*>(m_uiPlayer == 0 ? "Player1LastName" : "Player2LastName"));
          if (last && *last)
            for (const unsigned short* p = last; *p; ++p) m_familyName.push_back(*p);
        }
      } else {
        return;
      }
      break;
    case 0x10e:
    case 0x121:
      m_mediator.SetCasMode(3);
      DeallocateMorphResources(m_curSim);
      ResetSimRotations();
      ok = true;
      break;
    case 0x10f:
    case 0x110:
      if (m_morph) m_morph->EnterMorphScreen();
      ok = true;
      break;
    case 0x111:
    case 0x118:
    case 0x122:
    case 0x124:
    case 0x125:
    case 0x126:
      if (m_selection) m_selection->InitScreen();
      ok = false;
      break;
    case 0x112:
    case 0x123:
      if (m_tattoo) m_tattoo->RemoveClothesForTattoo();
      ok = true;
      break;
    case 0x113:
    case 0x115:
      if (m_morph) m_morph->ExitMorphScreenPrev();
      ok = true;
      break;
    case 0x114:
    case 0x116:
      if (m_morph) m_morph->ExitMorphScreenDone();
      ok = true;
      break;
    case 0x117:
    case 0x119:
      if (m_tattoo) m_tattoo->RestoreClothesForTattoo();
      ok = m_mode != 1 && m_mode != 5;
      break;
    case 0x11a: ok = m_mode == 1 || m_mode == 5; break;
    case 0x11b: ok = m_mode == 1; break;
    case 0x11c: ask("cas_genetics_exit_prompt"); return;
    case 0x11d: ask("cas_genetics_exit_prompt"); return;
    case 0x11e: {
      m_mediator.SetCasMode(3);
      CasEventResetSim reset;
      if (CasSimDescriptionS2C* d = m_mediator.GetEditSimDescription()) reset.m_male = d->m_male;
      m_mediator.PostEvent(reset);
      DeallocateMorphResources(m_curSim);
      ResetSimRotations();
      ok = true;
      break;
    }
    case 0x11f: ok = m_mode != 1 && m_mode != 5; break;
    case 0x120: ok = m_mode == 1; break;
    case 0x127: {
      char mods[9] = {};
      ConvertPartUIFocusToModList(mods);
      unsigned short wide[16] = {};
      UTF8ToUCS2(mods, static_cast<int>(std::strlen(mods)), wide);
      UIDB::UIDBSetString(const_cast<char*>("CAS_cfcModListStr"), wide);
      ok = true;
      break;
    }
    case 0x128: {
      CasEventResetSim reset;
      if (CasSimDescriptionS2C* d = m_mediator.GetEditSimDescription()) reset.m_male = d->m_male;
      m_mediator.PostEvent(reset);
      ok = true;
      break;
    }
    case 0x129: ok = !CFCShouldGotoBodyScreenNext(); break;
    case 0x12a: {
      if (!m_personal) {
        ok = true;
        break;
      }
      int total = 0;
      for (int i = 0; i < 5; ++i) total += m_personal->GetSavedPersVal(static_cast<signed char>(i));
      if (total == 0x19) {
        if (CasSimDescriptionS2C* d = m_mediator.GetEditSimDescription())
          for (int i = 0; i < 5; ++i)
            d->m_personality[i] = static_cast<unsigned char>(m_personal->GetSavedPersVal(static_cast<signed char>(i)));
        ok = true;
      } else if (total > 0x18) {
        ok = false;
      } else {
        SpawnOKDialog(Text("cas_personality_pts_prompt"), kNoTitle);
        ok = false;
      }
      break;
    }
    case 299:
      SaveNameEnterCBA();
      ok = true;
      break;
    case 300: ask("cas_confirm_cancel"); return;
    case 0x12d:
      DontSaveNameEnterCBA();
      ok = true;
      break;
    case 0x12e:
      ReturnForwardToKeyboard();
      ok = true;
      break;
    case 0x12f: ask("car_exit_prompt"); return;
    case 0x130: {  // a new roommate in the first free slot (mode 4), named after the family
      int found = kMaxSims;
      for (int i = 0; i < kMaxSims && found == kMaxSims; ++i) {
        if (m_flags >> (i % 4) & 1) continue;
        m_mediator.SetCasMode(4);
        ChangeMediator(i);
        ok = true;
        unsigned short* first = const_cast<unsigned short*>(Text("default_player1_first_name"));
        if (m_familyName.empty()) {
          m_simStates[i].SetSimName(first, const_cast<unsigned short*>(Text("default_player1_last_name")));
        } else {
          std::vector<unsigned short> family(m_familyName);
          family.push_back(0);
          m_simStates[i].SetSimName(first, family.data());
        }
        if (m_genetics) m_genetics->SetGenerateDefaultSim(true);
        UIDB::UIDBSetInt(const_cast<char*>("CAS_gender"), 0);
        found = i;
      }
      break;
    }
    case 0x131:  // editing the picked roommate (mode 5)
      m_mediator.SetCasMode(5);
      if (m_roommate) ChangeMediator(m_roommate->GetSelectedSlot());
      if (CasSimDescriptionS2C* d = m_mediator.GetEditSimDescription())
        UIDB::UIDBSetInt(const_cast<char*>("CAS_gender"), d->m_male != 0);
      ok = true;
      ResetSimRotations();
      break;
    case 0x132: ask("car_delete_sim_prompt"); return;
    case 0x133:  // the picked roommate leaves the family
      if (m_roommate) {
        m_flags &= ~(1u << (m_roommate->GetSelectedSlot() % 4));
        m_roommate->SetButtonState();
      }
      return;
    default: return;
  }
  const int next = ok ? c->ok : c->fail;
  if (next) HandleNav(next);
}

// 80195C1C
int CASTarget::CFCShouldGotoBodyScreenNext() { return m_mode == 0 && m_prevPartFocus == 0x13 ? 1 : 0; }

// 80195C40
int CASTarget::ShowAllCFAOptions() { return m_mode == 1 || m_mode == 5 ? 1 : 0; }

// 80195C80
// Which of the seven fashion options of the item have more than one choice, as "0"/"1" characters.
void CASTarget::ConvertPartUIFocusToModList(char* out) {
  unsigned int flags = 0;
  for (int i = 0; i < 7; ++i) {
    int change = 0;
    if (CASFashionTarget::ConvertPartUIFocusOptionToChangeEvent(m_prevPartFocus, i, change) && m_mediator.m_editSim) {
      if (m_mediator.m_editSim->GetMoreThanOneOption(static_cast<unsigned char>(change >> 0 & 0xff)))
        flags |= 1u << i;
      else
        flags &= ~(1u << i);
    }
  }
  CreateModStrFromValues(out, flags | 0x80);
}

// 80195D48
void CASTarget::CreateModStrFromValues(char* out, unsigned int flags) {
  std::memset(out, 0, 8);
  for (int i = 0; i < 7; ++i) out[i] = (flags >> i & 1) ? '1' : '0';
  out[8] = 0;
  out[7] = '1';
}

// ---------------------------------------------------------------------------------------------------
// Dialogs

// 80195DC4
// A yes/no question for conditional nav `nav` (input to the screen is off until it is answered).
void CASTarget::SpawnYesNoDialog(unsigned int nav, const unsigned short* body, const unsigned short* title, int preselect) {
  SetHandleInput(false);
  auto text = [](const unsigned short* t) {
    std::vector<unsigned short> v;
    for (; t && *t; ++t) v.push_back(*t);
    return v;
  };
  m_dialog.m_numSelections = 2;
  m_dialog.m_continueType = 1;
  m_dialog.m_sel[0] = text(Text("yes"));
  m_dialog.m_sel[1] = text(Text("no"));
  m_dialog.m_preselect = preselect;
  m_dialog.m_body = text(body);
  m_dialog.m_title = text(title);
  m_dialog.m_field20 = 0;
  m_dialog.m_onSelection = [this](int s) { DialogYesNoSelectionCallback(s); };
  m_dialog.m_onZeroInput = [this] { DialogNoSelectionCallback(); };
  m_dialog.m_onClosed = [this] { m_dialog.DialogClosedCallback(); };
  if (m_uiPlayer == 0) m_dialog.m_players = 1;
  else if (m_uiPlayer == 1) m_dialog.m_players = 2;
  m_pendingNav = static_cast<int>(nav);
  m_dialog.SpawnDialog();
}

// 80196040
// Yes (0) takes the conditional's success nav, no its failure nav.
void CASTarget::DialogYesNoSelectionCallback(int selection) {
  SetHandleInput(true);
  for (const Conditional& c : kConditional)
    if (c.nav == m_pendingNav) {
      m_dialog.HideDialog();
      HandleNav(selection == 0 ? c.ok : c.fail);
      break;
    }
  m_pendingNav = 0;
}

// 801960F4
void CASTarget::DialogNoSelectionCallback() {}

// 801960F8
void CASTarget::SpawnOKDialog(const unsigned short* body, const unsigned short* title) {
  SetHandleInput(false);
  auto text = [](const unsigned short* t) {
    std::vector<unsigned short> v;
    for (; t && *t; ++t) v.push_back(*t);
    return v;
  };
  m_dialog.m_numSelections = 1;
  m_dialog.m_continueType = 1;
  m_dialog.m_sel[0] = text(Text("ok"));
  m_dialog.m_body = text(body);
  m_dialog.m_title = text(title);
  m_dialog.m_field20 = 0;
  m_dialog.m_onSelection = [this](int s) { DialogOKSelectionCallback(s); };
  m_dialog.m_onZeroInput = [this] { DialogNoSelectionCallback(); };
  m_dialog.m_onClosed = [this] { m_dialog.DialogClosedCallback(); };
  if (m_uiPlayer == 0) m_dialog.m_players = 1;
  else if (m_uiPlayer == 1) m_dialog.m_players = 2;
  m_dialog.SpawnDialog();
}

// 8019632C
void CASTarget::DialogOKSelectionCallback(int) {
  SetHandleInput(true);
  m_dialog.HideDialog();
}

// ---------------------------------------------------------------------------------------------------
// Views and sims

// 80196364
bool CASTarget::IsViewingCAR() { return m_mode == 3; }

// 80196378
int CASTarget::IsViewingMirror() {
  if (m_mode == 3) return 0;
  const int focus = GetCurrentUIFocus();
  return (focus > 8 || focus < 2) ? 1 : 0;
}

// 801963CC
int CASTarget::IsViewingGenetics() {
  if (m_mode == 3 || m_mode == 5) return 0;
  const int focus = GetCurrentUIFocus();
  return (focus > 8 || focus < 2) ? 0 : 1;
}

// 80196420
// The sims shown face the camera again.
void CASTarget::ResetSimRotations() {
  for (int i = 0; i < kMaxSims; ++i)
    if (m_flags >> (i % 4) & 1) m_renderers[i].m_userRotation = 0.0f;
}

// 80196474
void CASTarget::DeallocateMorphResources(int sim) {
  if (sim >= 0 && sim < kMaxSims) m_renderers[sim].m_model.DeallocateMorphResources();
}

// 801A07F8
int CASTarget::IsShuttingdown() { return m_shuttingDown; }
// 801A0800
int CASTarget::GetPrevUIFocus() { return m_prevFocus; }
// 801A0808
int CASTarget::GetCurPartUIFocus() { return m_prevPartFocus; }
// 801A0810
int CASTarget::GetCurrentSimNumber() { return m_curSim; }
// 801A0818
int CASTarget::HasSimInSlot(int slot) const { return static_cast<int>(m_flags >> (slot % 4) & 1); }
// 801A0840
void CASTarget::SetHasSimInSlot(int slot, bool has) {
  if (has) m_flags |= 1u << (slot % 4);
  else m_flags &= ~(1u << (slot % 4));
}
// 801A0890
CasSimRendererDynamic* CASTarget::GetSimRenderer(int sim) { return &m_renderers[sim]; }
// 801A08A8
CASGeneticsTarget* CASTarget::GetGeneticsTarget() { return m_genetics; }
// 801A08B0
CasNpcEditor* CASTarget::GetNPCEditor() { return m_npcEditor; }
