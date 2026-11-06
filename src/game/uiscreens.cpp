// Port of game/uiscreenmanager.cpp, game/uireflow.cpp (the reflow blocks) and the main menu target
// MMUTarget (game/targets_s2c.cpp; the development menu's lists with SIMMY_DEV_MENU=1).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void* SimsAptMalloc(unsigned int size);
void SimsAptFree(void* p);

// DAT_804761D0: the development menu (off in this build). PC dev switch: SIMMY_DEV_MENU=1 turns it on.
int g_mmuDevMenu = std::getenv("SIMMY_DEV_MENU") ? std::atoi(std::getenv("SIMMY_DEV_MENU")) : 0;
// NghFileNames (80439A34) and DevelopmentNghNames (80439A94): the same six neighbourhood files.
const char* const kNghFileNames[6] = {"DEFAULT.NGH", "Test.ngh", "Wren.ngh", "Design.ngh", "Temp.ngh", "author.ngh"};
int g_mmuStartLotOverride = 0;  // DAT_80476150 (a debug start lot, 1..16; none)
const char* GetHouseNameText(int house);  // 800236C8 (famtarget.cpp)

namespace {
AptViewer* TheAptViewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
UIScreenManager* TheScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
size_t Len16(const unsigned short* s) {
  size_t n = 0;
  while (s && s[n]) ++n;
  return n;
}
// A UTF-16 copy in an SimsAptMalloc block, as the localizable commands return.
unsigned short* MallocText(const unsigned short* text) {
  const size_t n = Len16(text);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc(static_cast<unsigned int>((n + 1) * 2)));
  for (size_t i = 0; i < n; ++i) out[i] = text[i];
  out[n] = 0;
  return out;
}
unsigned short* MallocAscii(const char* text) {
  std::vector<unsigned short> w(std::strlen(text) + 1, 0);
  for (size_t i = 0; text[i]; ++i) w[i] = static_cast<unsigned char>(text[i]);
  return MallocText(w.data());
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// UIReflow

// 8017D234
UIReflow::UIReflow() = default;
// 8017D254
UIReflow::~UIReflow() = default;

// 8017D4A0
// A one-off reflow of an element's position (outside an open block).
void UIReflow::ResetPosition(const char* id, int x, int y, int player) {
  if (m_open) return;
  std::string b = "<reflow><id>";
  b += player ? "1" : "0";
  b += id;
  b += "</id><x>" + std::to_string(x) + "</x><y>" + std::to_string(y) + "</y></reflow>";
  if (TheAptViewer()) TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "Reflow", b.c_str());
}

// 8017D75C
void UIReflow::ResetVisibility(const char* id, bool visible, int player) {
  if (m_open) return;
  std::string b = "<reflow><id>";
  b += player ? "1" : "0";
  b += id;
  b += "</id><vis>";
  b += visible ? "1" : "0";
  b += "</vis></reflow>";
  if (TheAptViewer()) TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "Reflow", b.c_str());
}

// 8017DCAC
// The text's font size again (the movie redraws the text field, so it also picks up a changed value).
void UIReflow::ResetFontSizeOverride(const char* id, unsigned int size, int player) {
  if (m_open) return;
  std::string b = "<reflow><id>";
  b += player ? "1" : "0";
  b += id;
  b += "</id><fnt>" + std::to_string(size) + "</fnt></reflow>";
  if (TheAptViewer()) TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "Reflow", b.c_str());
}

// 8017E028
// 8017E0B0 (ReallocateBlock): the block grows as a std::string.
void UIReflow::AddToBlock(const char* text) {
  if (m_open) m_block += text;
}
void UIReflow::ReallocateBlock() {}

// 8017E920
void UIReflow::OpenReflowBlock() {
  if (m_open) return;
  m_open = true;
  m_block.clear();
  AddToBlock("<reflow>");
}

// 8017E998
void UIReflow::SubmitReflowBlock() {
  if (!m_open) return;
  AddToBlock("</reflow>");
  if (TheAptViewer())
    TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "Reflow", m_block.c_str());
  m_block.clear();
  m_open = false;
}

// 8017EA1C
// Ids not starting with a player digit get the player's ("0" or "1").
void UIReflow::Reflow_Id(const char* id, int player) {
  AddToBlock("<id>");
  if (static_cast<unsigned>(static_cast<unsigned char>(id[0]) - '0') > 1) AddToBlock(player ? "1" : "0");
  AddToBlock(id);
  AddToBlock("</id>");
}

// 8017EBB0
void UIReflow::Reflow_Visibility(bool visible) {
  AddToBlock("<vis>");
  AddToBlock(visible ? "1" : "0");
  AddToBlock("</vis>");
}

// 8017E838
void UIReflow::Tween_ColorOnlyOverride(unsigned int rgb) {
  char buf[16];
  AddToBlock("<clr>");
  AddToBlock("0x");
  std::snprintf(buf, sizeof buf, "%6.6x", rgb);
  AddToBlock(buf);
  AddToBlock("</clr>");
}

// 8017ECBC
void UIReflow::Reflow_ColorOnlyOverride(unsigned int rgb) { Tween_ColorOnlyOverride(rgb); }

// ---------------------------------------------------------------------------------------------------
// UIScreenManager

extern const char* const Widget_G2D[];
extern const char* const Widget_H2D[];
extern const char* const Widget_M2M[];
extern const char* const Widget_ActionQueue[];
extern int g_loadedGameSlot;   // DAT_80476214 (game/memcardscreens.cpp)
extern int g_loadedGameSlot2;  // DAT_80476218
extern const char* const Widget_BuildBuyHUD[];
extern const char* const Widget_CSIM[];
extern const char* const Widget_Credits[];
extern const char* const Widget_FireCodeMeter[];
extern const char* const Widget_InterestMeter[];
extern const char* const Widget_IntroThoughtBalloon[];
extern const char* const Widget_MainMenu[];
extern const char* const Widget_ModelessDialog[];
extern const char* const Widget_Mood[];
extern const char* const Widget_Motives[];
extern const char* const Widget_RewardMomentDialog[];
extern const char* const Widget_Skills[];
extern const char* const Widget_WantsAndFears[];  // widgetlayout.cpp

int UIScreenManager::m_groupNumber = 0;

// ---------------------------------------------------------------------------------------------------
// PC co-op: private screens (platform.hpp, "Co-op private screens").
// On the GC, food creation (INGTarget, screen 0x19, a fridge's "Get Ingredients"), its recipe book
// (RCPTarget, 0x1A) and the help pages and exit prompt they show in the dialog box covered the whole
// TV: loading them suspended every screen (both players' HUDs), their button filters took both pads,
// the fridge's call waited modally (generic sim call 0x27 returned 4, which stops the whole
// simulation at that object each tick: the lot froze) and the dialog box paused the simulator.
// With two players the PC keeps them to the player who opened them: only that player's HUD is
// suspended and only their pad is taken; the other player's HUD and controls stay; the screens are
// drawn in that player's window / half only; the opener's sim waits without stopping the simulation
// (ingamecas.cpp) and the dialog box they show does not pause it, so the lot keeps going.
namespace {
int s_privatePlayer = -1;  // the player with food creation up (INGTarget), -1 none
}  // namespace

bool PcCoopPrivateScreens() { return _gd.m_fieldEC != 0; }  // DAT_80475C70: two players
bool PcPrivateDialogShown();  // g2dtarget.cpp: the dialog box shows a private screen's dialog
int PcPrivateScreenPlayer() { return s_privatePlayer; }
void PcSetPrivateScreenPlayer(int player) { s_privatePlayer = player; }

namespace {
// The player a per-player HUD screen (0x32-0x3F: CSIM, mood, motives, skills, wants and fears, action
// queue, interest meter, in player 1 / player 2 pairs) belongs to; -1 for any other screen.
int ScreenPlayer(int id) { return id >= 0x32 && id <= 0x3f ? id & 1 : -1; }
// PC co-op: a private screen of `owner` being loaded with GC suspend mode `mode` (1 all, 3 the HUD
// widgets): the active screens that would be suspended, less the other player's HUD and the screens
// both players' play needs (the HUD's buttons H2D 0x31, the fire code meter 0x40, the intro balloon
// 0x41, the modeless dialogs 0x42, player 1's mood widget 0x34 with the money). Ends with 0x45
// (SuspendScreenList's list end).
std::vector<int> PrivateSuspendList(const UIScreenManager& usm, int owner, int mode) {
  std::vector<int> ids;
  for (const auto& r : usm.m_records) {
    if (r.state != 1 || !r.id) continue;
    const int player = ScreenPlayer(r.id);
    if (player >= 0 && player != owner) continue;
    if (r.id == 0x31 || r.id == 0x40 || r.id == 0x41 || r.id == 0x42) continue;
    // Player 1's mood widget also holds the money shown in player 2's half (the clock-and-money bar):
    // it stays; pc_hud.cpp leaves out its parts in the owner's window / half.
    if (r.id == 0x34) continue;
    if (mode == 3 && player < 0 && r.id != 0x43) continue;  // mode 3: the HUD widgets only
    ids.push_back(r.id);
  }
  ids.push_back(0x45);
  return ids;
}
}  // namespace

// UIScreenDataTable (80424720): ends with id 0x45.
const UIScreenData UIScreenManager::UIScreenDataTable[] = {
    {0x02, 1, 1, 0, Widget_MainMenu, nullptr}, {0x03, 1, 1, 0, nullptr, "CGE"}, {0x04, 1, 1, 0, nullptr, "CBA"},
    {0x05, 1, 1, 0, nullptr, "CBO"}, {0x06, 1, 1, 0, nullptr, "CMB"}, {0x07, 1, 1, 0, nullptr, "CHD"},
    {0x08, 1, 1, 0, nullptr, "CMH"}, {0x09, 1, 1, 0, nullptr, "CTA"}, {0x0A, 1, 1, 0, nullptr, "CFA"},
    {0x0B, 1, 1, 0, nullptr, "CHS"}, {0x0C, 1, 1, 0, nullptr, "CTO"}, {0x0D, 1, 1, 0, nullptr, "CHA"},
    {0x0E, 1, 1, 0, nullptr, "CBS"}, {0x0F, 1, 1, 0, nullptr, "CSO"}, {0x10, 1, 1, 0, nullptr, "CSL"},
    {0x11, 1, 1, 0, nullptr, "CFC"}, {0x12, 1, 1, 0, nullptr, "CSM"}, {0x13, 1, 1, 0, nullptr, "CST"},
    {0x14, 1, 1, 0, nullptr, "CAP"}, {0x15, 1, 1, 0, nullptr, "CPR"}, {0x16, 1, 1, 0, nullptr, "CKB"},
    {0x17, 1, 1, 0, nullptr, "K2Y"}, {0x18, 1, 1, 0, nullptr, "CAR"}, {0x2B, 2, 1, 0, nullptr, "OPT"},
    {0x2C, 1, 1, 0, nullptr, "PAZ"}, {0x19, 1, 1, 0, nullptr, "ING"}, {0x1A, 1, 1, 0, nullptr, "RCP"},
    {0x1E, 4, 1, 0, nullptr, "PCT"}, {0x22, 1, 1, 0, nullptr, "R2L"}, {0x23, 1, 1, 0, nullptr, "CSP"},
    {0x1F, 2, 1, 0, nullptr, "PWF"}, {0x01, 1, 1, 0, nullptr, "TEM"}, {0x27, 1, 1, 0, nullptr, "WXF"},
    {0x24, 1, 1, 0, nullptr, "E2E"}, {0x25, 1, 1, 0, nullptr, "O2T"}, {0x26, 1, 1, 0, nullptr, "MMU"},
    {0x28, 1, 1, 0, nullptr, "PRG"}, {0x2A, 1, 1, 0, Widget_Credits, nullptr},
    {0x29, 2, 1, 0, Widget_BuildBuyHUD, nullptr}, {0x2F, 3, 1, 0, Widget_G2D, nullptr},
    {0x30, 1, 1, 0, Widget_M2M, nullptr}, {0x31, 1, 1, 0, Widget_H2D, nullptr},
    {0x34, 2, 0, 0, Widget_Mood, nullptr}, {0x35, 2, 0, 1, Widget_Mood, nullptr},
    {0x32, 2, 0, 0, Widget_CSIM, nullptr}, {0x33, 2, 0, 1, Widget_CSIM, nullptr},
    {0x36, 2, 0, 0, Widget_Motives, nullptr}, {0x37, 2, 0, 1, Widget_Motives, nullptr},
    {0x38, 2, 0, 0, Widget_Skills, nullptr}, {0x39, 2, 0, 1, Widget_Skills, nullptr},
    {0x3A, 2, 0, 0, Widget_WantsAndFears, nullptr}, {0x3B, 2, 0, 1, Widget_WantsAndFears, nullptr},
    {0x3C, 2, 0, 0, Widget_ActionQueue, nullptr}, {0x3D, 2, 0, 1, Widget_ActionQueue, nullptr},
    {0x3E, 2, 0, 0, Widget_InterestMeter, nullptr}, {0x3F, 2, 0, 1, Widget_InterestMeter, nullptr},
    {0x41, 1, 0, 0, Widget_IntroThoughtBalloon, nullptr}, {0x42, 2, 0, 0, Widget_ModelessDialog, nullptr},
    {0x43, 1, 1, 0, Widget_RewardMomentDialog, nullptr}, {0x40, 2, 0, 0, Widget_FireCodeMeter, nullptr},
    {0x44, 1, 1, 0, nullptr, "FAM"}, {0x45, 0, 0, 0, nullptr, nullptr},
};

// 8017F0D0
UIScreenManager::UIScreenManager() {
  for (EAHeap* heap : {RootHeap(), MainHeap(), ResourceHeap(), AudioHeap(), VMHeap(), ShaderHeap(), TextureHeap(),
                       AnimationHeap(), ModelHeap(), FlashesHeap(), AptHeap(), DebugHeap(), CUIHeap()})
    heap->Compact();
  EAHeap::SetNextCheckPoint();
  AttachTables("UIScreenManager");
  m_setTable->InstallCommand("USM_screen_unloaded", [this](char* n, char* v) { SetVariable(n, v); }, this);
  // The screens' mask clips ("CCRMA" / "CCRMS": aptauxpceorgl.cpp's customControlRender) draw this shader
  // into the stencil.
  UI2D::Instance()->InstallEntry(const_cast<char*>("USM_maskAdd"), 0x20b8427du, const_cast<char*>(m_name), 0);
  UI2D::Instance()->InstallEntry(const_cast<char*>("USM_maskSub"), 0x20b8427du, const_cast<char*>(m_name), 0);
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
}

// 8017F688
UIScreenManager::~UIScreenManager() {
  if (!m_installed) return;
  m_getTable->UnInstallAllCommands(this);
  m_setTable->UnInstallAllCommands(this);
  m_locTable->UnInstallAllCommands(this);
  m_updateTable->UnInstallEntry(this);
  m_drawTable->UnInstallEntry(this);
}

// 8017F838
const UIScreenData* UIScreenManager::FindDataByID(int id) {
  for (const UIScreenData* d = UIScreenDataTable; d->id != 0x45; ++d)
    if (d->id == id) return d;
  return nullptr;
}

// 8017F884
// A screen already on stage is queued until it goes (Update loads it then). Otherwise the screens
// its suspend mode names are suspended, the movie is asked to load it ("LoadCustomScreen <id>
// <input mode>"; widget layouts through WidgetScreenFormat) and it gets a record.
int UIScreenManager::LoadUIScreen(int id, int inputMode) {
  if (IsScreenManaged(id)) {
    for (int& q : m_queue) {
      if (q == 0) {
        q = id;
        return 0;
      }
      if (q == id) return 0;
    }
    return 0;
  }
  const UIScreenData* data = FindDataByID(id);
  if (!data) return 0;
  int group = 0;
  // PC co-op: a private screen suspends only its player's screens (see PcCoopPrivateScreens).
  int privateOwner = -1;
  if (PcPrivateScreenPlayer() >= 0 && (id == 0x19 || id == 0x1a || (id == 0x2f && PcPrivateDialogShown())))
    privateOwner = PcPrivateScreenPlayer();
  if (privateOwner >= 0 && (data->suspendMode == 1 || data->suspendMode == 3)) {
    group = SuspendScreenList(PrivateSuspendList(*this, privateOwner, data->suspendMode).data());
  } else if (data->suspendMode == 1) {
    group = SuspendAllActiveScreens();
  } else if (data->suspendMode == 3) {
    // the HUD widgets (lbl_803E3AD4)
    static const int kHud[] = {0x34, 0x35, 0x38, 0x39, 0x3A, 0x3B, 0x36, 0x37, 0x3E,
                               0x3F, 0x3C, 0x3D, 0x32, 0x33, 0x43, 0x42, 0x45};
    group = SuspendScreenList(kHud);
  } else if (data->suspendMode == 4) {
    std::vector<int> ids;
    for (int i = 1; i <= 0x44; ++i)
      if (i != 0x29 && i != 0x32 && i != 0x33 && i != 0x40) ids.push_back(i);
    ids.push_back(0x45);
    group = SuspendScreenList(ids.data());
  }
  if (!data->widgetLayout) {
    const char mode[2] = {static_cast<char>('0' + inputMode), 0};
    TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 3, "LoadCustomScreen",
                                    data->flashName, mode);
  } else {
    WidgetScreenFormat format;
    format.LaunchWidgetScreenLayout(data->widgetLayout, data->player, inputMode);
  }
  SetHandlesInput(id, data->handlesInput != 0);
  AddScreen(id, group);
  return 1;
}

// 8017FAE4
// Restores the screens its loading suspended, asks the movie to remove it, or drops it from the queue.
int UIScreenManager::UnloadUIScreen(int id) {
  bool notOnStage = true;
  char name[128];
  if (IsScreenManaged(id)) {
    Record& r = m_records[FindRecordIndexByID(id)];
    if (r.state != 3) {
      notOnStage = false;
      if (r.suspendedGroup) {
        for (Record& o : m_records)
          if (o.group == r.suspendedGroup) {
            if (o.state != 3) {
              GetScreenFlashName(name, o.id);
              TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "RestoreScreen", name);
              o.state = 1;
            }
            o.group = 0;
          }
        r.suspendedGroup = 0;
      }
      GetScreenFlashName(name, r.id);
      TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "RemoveScreen", name);
      r.state = 3;
    }
  }
  if (notOnStage)
    for (int i = 0; i < 8; ++i)
      if (m_queue[i] == id) {
        PopQueue(i);
        break;
      }
  return 0;
}

// 8017FC74
bool UIScreenManager::IsScreenManaged(int id) { return FindRecordIndexByID(id) != -1; }

// 8017FCB0
int UIScreenManager::SuspendAllActiveScreens() {
  int group = 0;
  ++m_groupNumber;
  char name[32];
  for (Record& r : m_records)
    if (r.state == 1) {
      r.state = 2;
      r.group = m_groupNumber;
      GetScreenFlashName(name, r.id);
      TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "SuspendScreen", name);
      group = m_groupNumber;
    }
  return group;
}

// 8017FD58
int UIScreenManager::SuspendScreenList(const int* ids) {
  int group = 0;
  ++m_groupNumber;
  char name[32];
  for (Record& r : m_records) {
    if (r.state != 1) continue;
    for (const int* p = ids; *p <= 0x44; ++p)
      if (*p == r.id) {
        r.state = 2;
        r.group = m_groupNumber;
        GetScreenFlashName(name, r.id);
        TheAptViewer()->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "SuspendScreen", name);
        group = m_groupNumber;
        break;
      }
  }
  return group;
}

// 8017FE64
int UIScreenManager::AddScreen(int id, int group) {
  for (Record& r : m_records)
    if (r.state == 0) {
      r.id = id;
      r.state = 1;
      r.suspendedGroup = group;
      return 1;
    }
  return 0;
}

// 8017FEB8
int UIScreenManager::FindRecordIndexByID(int id) {
  for (int i = 0; i < 24; ++i)
    if (m_records[i].id == id) return i;
  return -1;
}

// 8017FEF8
bool UIScreenManager::RemoveScreen(int id) {
  const int i = FindRecordIndexByID(id);
  if (i != -1) m_records[i].state = 0;
  return i != -1;
}

// 8017FF44
// A custom screen's movie id, or a widget screen's "<player digit><scrnid>" from its layout.
int UIScreenManager::GetScreenFlashName(char* out, int id) {
  const UIScreenData* d = FindDataByID(id);
  if (!d) return 0;
  if (d->widgetLayout) {
    for (const char* const* t = d->widgetLayout; std::strcmp(*t, "</scrn>") != 0; ++t)
      if (std::strcmp(*t, "<scrnid>") == 0) {
        out[0] = static_cast<char>('0' + d->player);
        std::strcpy(out + 1, t[1]);
        return 1;
      }
    return 0;
  }
  if (!d->flashName) {
    std::strcpy(out, "BOGUS");
    return 0;
  }
  std::strcpy(out, d->flashName);
  return 1;
}

// 801800BC
int UIScreenManager::SetHandlesInput(int id, bool handles) {
  char name[64] = {};
  GetScreenFlashName(name, id);
  char* argv[3] = {const_cast<char*>("SetHandlesInput"), name, const_cast<char*>(handles ? "1" : "0")};
  TheAptViewer()->NewCallFunction2(const_cast<char*>("cmdProc"), nullptr, nullptr, 3, argv);
  return 1;
}

// 8018019C
// "USM_screen_unloaded" = <movie id>: that screen's record is free again.
void UIScreenManager::SetVariable(char* name, char* value) {
  if (std::strcmp("USM_screen_unloaded", name) != 0) return;
  char flash[128];
  for (const UIScreenData* d = UIScreenDataTable; d->id != 0x45; ++d) {
    GetScreenFlashName(flash, d->id);
    if (std::strcmp(flash, value) == 0) {
      const int i = FindRecordIndexByID(d->id);
      if (i != -1) m_records[i] = Record();
      return;
    }
  }
}

// 80180274
// Queued screens load once their earlier instance has gone.
void UIScreenManager::Update() {
  int i = 0;
  while (i < 8 && m_queue[i] != 0) {
    if (!IsScreenManaged(m_queue[i])) {
      LoadUIScreen(m_queue[i], 0);
      PopQueue(i);
    } else {
      ++i;
    }
  }
}

// 80180308
void UIScreenManager::PopQueue(int index) {
  for (int i = index; i < 7; ++i) m_queue[i] = m_queue[i + 1];
  m_queue[7] = 0;
}

// ---------------------------------------------------------------------------------------------------
// MMUTarget

MMUTarget* MMUTarget::m_MMUSingleton = nullptr;

// 801C49C0
// Installs the "MMU_..." variables, loads the MMU screen, starts in the select-gameplay state and
// lets Apt read the pads.
MMUTarget::MMUTarget() {
  m_MMUSingleton = this;
  AttachTables("MMUTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  UIDrawTable::InstallEntry(0x19, [this](ERC* rc) { Draw(rc); }, m_name, this);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  for (const char* n : {"MMU_exists", "MMU_GETVAR_CURRENT_STATE", "MMU_GETVAR_NUMBER_OF_CHOICES_FOR_CURRENT_STATE",
                        "MMU_Is_Dev_Menu_Enabled", "MMU_Is_Two_Player_Enabled", "MMU_Shorten_Lot_List"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"MMU_Main_Menu_Choice", "MMU_Gameplay_Menu_Choice", "MMU_Number_Of_Players_Menu_Choice",
                        "MMU_SET_MENU_STATE", "MMU_GO_UP_TREE", "MMU_INCREMENT_CURRENT_CHOICE",
                        "MMU_DECREMENT_CURRENT_CHOICE", "MMU_SET_CURRENT_CHOICE", "MMU_CANCEL_BUTTON",
                        "MMU_LEFT_BUTTON", "MMU_RIGHT_BUTTON", "MMU_UP_BUTTON", "MMU_DOWN_BUTTON", "MMU_X_BUTTON",
                        "MMU_CIRCLE_BUTTON", "MMU_SQUARE_BUTTON"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"MMU_Main_Menu_Text", "MMU_Top_Level_Choice_Text_0", "MMU_Top_Level_Choice_Text_1",
                        "MMU_Select_Gameplay_Text", "MMU_Story_Mode_Text", "MMU_Freeplay_Text", "MMU_Credits_Text",
                        "MMU_Number_Of_Players_Text", "MMU_One_Or_Two_Player_Text", "MMU_One_Player_Text",
                        "MMU_Two_Player_Text", "MMU_Author_Mode"})
    m_locTable->InstallCommand(n, loc, this);
  for (int i = 0; i < 16; ++i) m_locTable->InstallCommand(("MMU_List_" + std::to_string(i)).c_str(), loc, this);
  for (const char* n : {"MMU_SET_DEVMENU_STATE", "MMU_NGH_INDEX", "MMU_LOT_INDEX"})
    m_setTable->InstallCommand(n, set, this);
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("MMU_UI_BROWSE_SAMPLE"), 0x64ac15u, const_cast<char*>(m_name));
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("MMU_UI_SELECT_SAMPLE"), 0x374bdceau, const_cast<char*>(m_name));
  UIAUDIO::Instance()->InstallEntry(const_cast<char*>("MMU_UI_BACK_SAMPLE"), 0x48ae94fu, const_cast<char*>(m_name));
  if (UIScreenManager* usm = TheScreenManager()) usm->LoadUIScreen(0x26, 0);
  _app.m_field48C = 1;  // map player 1 to the first pad pressing start
  _app.m_field490 = 0;  // not player 2 (yet)
  _app.SetNghName(const_cast<char*>("DEFAULT.NGH"));
  UpdateText();
  m_stateStack.push_back(0);
  _pCtrlMan->UnmapPlayer(0);
  _pCtrlMan->UnmapPlayer(1);
  if (AptViewer* viewer = TheAptViewer()) {
    viewer->m_field3AC = 0;
    viewer->m_field3A8 = 0;
    viewer->AllowControllerReading(true);
  }
  if (_fontman.IsValid(static_cast<unsigned int>(EGlobal::GetGameFontID())))
    m_font = static_cast<ERFont*>(_fontman.AddRef(static_cast<unsigned int>(EGlobal::GetGameFontID()), nullptr, 0));
}

// 801C5830
MMUTarget::~MMUTarget() {
  m_MMUSingleton = nullptr;
  if (m_creditsTarget) {
    static_cast<CRDTarget*>(m_creditsTarget)->HideCredits();
    delete m_creditsTarget;
    m_creditsTarget = nullptr;
  }
  if (UIScreenManager* usm = TheScreenManager()) usm->UnloadUIScreen(0x26);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    m_drawTable->UnInstallEntry(this);
  }
}

// 801C59E0
MMUTarget* MMUTarget::GetSingleton() { return m_MMUSingleton; }
// 801C59E8
// The credits have faded out: MMUTarget::Update deletes them and goes back up the menu.
void MMUTarget::OnCreditsClose() { m_fieldFC = 1; }

// 801C59F4
// The menu movie's commands (a leading player digit is skipped). The development menu's
// neighbourhood and lot lists only work with the development menu enabled (DAT_804761D0, off;
// SIMMY_DEV_MENU=1).
void MMUTarget::SetVariable(char* name, char* value) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  const std::string n = name;
  if (n == "MMU_SET_DEVMENU_STATE") {
    if (g_mmuDevMenu) UpdateDevMenuState(name, value);
    return;
  }
  if (n == "MMU_NGH_INDEX") {
    if (g_mmuDevMenu) OnNGHSelect(name, value);
    return;
  }
  if (n == "MMU_LOT_INDEX") {
    if (g_mmuDevMenu) LaunchLot(std::atoi(value));
    return;
  }
  if (n == "MMU_Main_Menu_Choice") UpdateTopLevelChoice(name, value);
  else if (n == "MMU_Gameplay_Menu_Choice") UpdateGameplayChoice(name, value);
  else if (n == "MMU_Number_Of_Players_Menu_Choice") UpdateNumberOfPlayersChoice(name, value);
  else if (n == "MMU_SET_MENU_STATE") UpdateMenuState(name, value);
  else if (n == "MMU_GO_UP_TREE") GoUpMenuTree(name, value);
  else if (n == "MMU_INCREMENT_CURRENT_CHOICE") IncrementChoice(name, value);
  else if (n == "MMU_DECREMENT_CURRENT_CHOICE") DecrementChoice(name, value);
  else if (n == "MMU_SET_CURRENT_CHOICE") SetCurrentChoice(value);
  else if (n == "MMU_X_BUTTON") {
    if (m_field8C == 0) OnXKeyPressed(name, value);
  } else if (n == "MMU_UP_BUTTON") OnUpKeyPressed(name, value);
  else if (n == "MMU_DOWN_BUTTON") OnDownKeyPressed(name, value);
  else if (n == "MMU_CIRCLE_BUTTON") OnCircleKeyPressed(name, value);
  else if (n == "MMU_LEFT_BUTTON") OnLeftKeyPressed(name, value);
  else if (n == "MMU_RIGHT_BUTTON") OnRightKeyPressed(name, value);
  else if (n == "MMU_CANCEL_BUTTON") OnCancelKeyPressed(name, value);
  else if (n == "MMU_SQUARE_BUTTON") UpdateText();
}

namespace {
void SetSelf(MMUTarget* t, const char* name, int value) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d", value);
  t->SetVariable(const_cast<char*>(name), buf);
}
// 801C6FC4 SetupCAS: player 1 creates the sims, with the default names.
}  // namespace

// 801C6FC4
void SetupCAS() {
  UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), 0);
  UIDB::UIDBSetInt(const_cast<char*>("CAS_mode"), 0);
  UIDB::UIDBSetString(const_cast<char*>("Player1FirstName"), EGlobal::NewUIText("default_player1_first_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player1LastName"), EGlobal::NewUIText("default_player1_last_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player2FirstName"), EGlobal::NewUIText("default_player2_first_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player2LastName"), EGlobal::NewUIText("default_player2_last_name"));
}

// 801C6E5C
// The players screen's choice; in the development menu the neighbourhood (6, wrapping) or lot.
void MMUTarget::OnUpKeyPressed(char* name, char* value) {
  if (m_state == 4) {
    DecrementChoice(name, value);
  } else if (m_field8C == 1) {
    if (--m_devNgh < 0) m_devNgh = 5;
    UpdateNGHListItems();
  } else if (m_field8C == 2) {
    if (--m_devLot < 0) m_devLot = m_devLotCount - 1;
    UpdateLotListItems();
  }
}
// 801C6EE8
void MMUTarget::OnDownKeyPressed(char* name, char* value) {
  if (m_state == 4) {
    IncrementChoice(name, value);
  } else if (m_field8C == 1) {
    m_devNgh = (m_devNgh + 1) % 6;
    UpdateNGHListItems();
  } else if (m_field8C == 2) {
    m_devLot = (m_devLot + 1) % m_devLotCount;
    UpdateLotListItems();
  }
}
// 801C6F84
void MMUTarget::OnLeftKeyPressed(char* name, char* value) { DecrementChoice(name, value); }
// 801C6FA4
void MMUTarget::OnRightKeyPressed(char* name, char* value) { IncrementChoice(name, value); }

// 801C70F0
// A new game picks its memory card slot first; otherwise the sims are created (CAS).
void MMUTarget::EndPlayerNumberChooser() {
  if (m_topLevelChoice == 2) {
    UIDB::UIDBSetInt(const_cast<char*>("MEM_source"), 0);
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("SlotSelect"));
  } else {
    UIObjectBase::UIRouter_SetVariable(const_cast<char*>("flowGoto"), const_cast<char*>("CAS_"));
  }
}

// 801C6DF0
// The development menu's free play: a new game in one-player mode through Create-a-Sim.
void MMUTarget::LaunchFreeplay() {
  _gd.m_fieldE4 = 1;  // DAT_80475C68
  _gd.m_field84 = 1;  // DAT_80475C08
  _gd.SetTwoPlayerMode(false);
  UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("CAS_"));
  m_field8C = 0;
  UpdateNGHListItems();
  UpdateLotListItems();
}

// 801C6644
unsigned short* MMUTarget::GetListItemName(char* name) {
  if (m_field8C == 1) return GetNGHListItemName(name);
  if (m_field8C == 2) return GetLotListItemName(name);
  return nullptr;
}

// 801C668C
// O on the main menu opens the neighbourhood list (the menu hint hidden); X there opens its lots.
void MMUTarget::ChangeScreenMode() {
  if (m_field8C == 0) {
    UIReflow r;
    r.OpenReflowBlock();
    r.Reflow_Id("MMU_Development_Menu", 0);
    r.Reflow_Visibility(false);
    r.SubmitReflowBlock();
    m_field90 = 0;
    m_field8C = 1;
    UpdateListItems();
  } else if (m_field8C == 1) {
    _app.SetNghName(const_cast<char*>(kNghFileNames[m_devNgh]));
    m_field90 = 1;
    m_field8C = 2;
    UpdateListItems();
  }
}

// 801C6758
void MMUTarget::UpdateListItems() {
  if (m_field8C == 0 || m_field8C == 1) UpdateNGHListItems();
  else if (m_field8C == 2) UpdateLotListItems();
}

// 801C679C
// "MMU_List_0".."15": the six neighbourhoods shown in the neighbourhood list (the highlighted one red).
void MMUTarget::UpdateNGHListItems() {
  const int mode = m_field8C;
  UIReflow r;
  r.OpenReflowBlock();
  for (int i = 0; i < 0x10; ++i) {
    const std::string id = "MMU_List_" + std::to_string(i);
    r.Reflow_Id(id.c_str(), 0);
    r.Reflow_Visibility(i < 6 && mode == 1);
    r.Reflow_ColorOnlyOverride(m_devNgh == i ? 0xff0000u : 0xffffffu);
  }
  r.SubmitReflowBlock();
}

// 801C68E8
// The lots (the highlighted one red).
void MMUTarget::UpdateLotListItems() {
  UIReflow r;
  r.OpenReflowBlock();
  for (int i = 0; i < 0x10; ++i) {
    const std::string id = "MMU_List_" + std::to_string(i);
    r.Reflow_Id(id.c_str(), 0);
    r.Reflow_Visibility(i < m_devLotCount);
    r.Reflow_ColorOnlyOverride(m_devLot == i ? 0xff0000u : 0xffffffu);
  }
  r.SubmitReflowBlock();
}

// 801C6A0C
// The neighbourhood file's name ("MMU_List_0".."5").
unsigned short* MMUTarget::GetNGHListItemName(char* name) {
  int i = 0;
  for (int k = 0; k < 6; ++k)
    if (std::strcmp(name, ("MMU_List_" + std::to_string(k)).c_str()) == 0) i = k;
  return MallocAscii(kNghFileNames[i]);
}

// 801C6B54
// The house's name ("HouseData" "house NN").
unsigned short* MMUTarget::GetLotListItemName(char* name) {
  int lot = 0;
  if (std::strncmp("MMU_List_", name, 9) == 0) lot = std::atoi(name + 9);
  const unsigned short* const* text = _globals.HouseNameText(GetHouseNameText(lot + 1));
  return MallocText(text ? *text : nullptr);
}

// 801C6BFC
// Straight into lot `lot` (a debug start lot overrides it) as a new one-player game: the default
// names, then the HUD's flow.
void MMUTarget::EnterLot(int lot) {
  _gd.m_fieldE4 = 1;  // DAT_80475C68
  _gd.m_field84 = 1;  // DAT_80475C08
  _gd.SetTwoPlayerMode(false);
  UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), 0);
  UIDB::UIDBSetInt(const_cast<char*>("CAS_mode"), 0);
  UIDB::UIDBSetInt(const_cast<char*>("MEM_source"), 0);
  UIDB::UIDBSetString(const_cast<char*>("Player1FirstName"), EGlobal::NewUIText("default_player1_first_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player1LastName"), EGlobal::NewUIText("default_player1_last_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player2FirstName"), EGlobal::NewUIText("default_player2_first_name"));
  UIDB::UIDBSetString(const_cast<char*>("Player2LastName"), EGlobal::NewUIText("default_player2_last_name"));
  if (static_cast<unsigned int>(g_mmuStartLotOverride - 1) < 0x10) lot = g_mmuStartLotOverride;
  _app.SetStartLot(lot);
  UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("HUD_"));
}

// 801C6D90
// The highlighted lot (-1) or lot `index` (0-based); the menu leaves the development mode.
void MMUTarget::LaunchLot(int index) {
  EnterLot(index == -1 ? m_devLot + 1 : index + 1);
  m_field8C = 0;
  UpdateNGHListItems();
  UpdateLotListItems();
}

// 801C85D0
void MMUTarget::UpdateDevMenuState(char*, char* value) {
  if (g_mmuDevMenu) ChangeScreenMode(std::atoi(value));
}

// 801C861C
// A neighbourhood picked in the list: its file, then its lots (author mode with free play: free play).
void MMUTarget::OnNGHSelect(char*, char* value) {
  const int i = std::atoi(value);
  if (i >= 0 && i < 6) _app.SetNghName(const_cast<char*>(kNghFileNames[i]));
  m_devNgh = i;
  if (_gd.m_fieldF0 == 0 || _gd.m_fieldE8 == 0) {  // DAT_80475C74, DAT_80475C6C
    m_field8C = 2;
    m_field90 = 1;
  } else {
    LaunchFreeplay();
  }
  UpdateListItems();
}

// 801C86B4
void MMUTarget::ChangeScreenMode(int mode) {
  const int prev = m_field8C;
  m_field8C = mode;
  m_field90 = prev;
}

// 801C7168
// X on the gameplay screen (state 0) picks story mode, free play (state 1 next) or the credits;
// on the main screen (state 1) "load" goes to the memory card slots and "new" asks first; on the
// players screen (state 4) the choice is taken (player 2 must press start for two players).
void MMUTarget::OnXKeyPressed(char*, char*) {
  if (m_field8C == 2) {
    LaunchLot(-1);
    return;
  }
  if (m_field8C == 1) {
    if (_gd.m_fieldF0 && _gd.m_fieldE8) {  // DAT_80475C74, DAT_80475C6C: author mode with free play
      LaunchFreeplay();
      return;
    }
    ChangeScreenMode();
    return;
  }
  if (m_state == 1) {
    if (m_topLevelChoice == 0) {
      _gd.m_field84 = 0;
      SetupCAS();
      UIDB::UIDBSetInt(const_cast<char*>("MEM_source"), 0);
      UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("SlotSelect"));
      return;
    }
    if (m_topLevelChoice != 2) return;
    // "Start a new game?" (memory card text), no / yes
    m_dialog.m_continueType = 0;
    m_dialog.m_field14 = 0;
    m_dialog.m_numSelections = 2;
    auto text = [](const unsigned short* t) {
      std::vector<unsigned short> v;
      for (size_t i = 0; t && t[i]; ++i) v.push_back(t[i]);
      v.push_back(0);
      return v;
    };
    m_dialog.m_sel[0] = text(EGlobal::NewUIText("no"));
    m_dialog.m_sel[1] = text(EGlobal::NewUIText("yes"));
    const unsigned short* const* body = _globals.GetMemCardString("ngc_newgame_body");
    m_dialog.m_body = text(body ? *body : nullptr);
    m_dialog.m_title = text(nullptr);
    m_dialog.m_field20 = 0;
    m_dialog.m_onSelection = [this](int sel) { OnDialogClose(sel); };
    m_dialog.m_onZeroInput = [this] { OnCancelDialog(); };
    UIDialog* dialog = &m_dialog;
    m_dialog.m_onClosed = [dialog] { dialog->DialogClosedCallback(); };
    m_dialog.SpawnDialog();
    return;
  }
  if (m_state == 0) {
    if (m_gameplayChoice == 0) {
      _gd.m_field00 = 3;  // story mode
      _gd.m_fieldE8 = 0;
      SetSelf(this, "MMU_SET_MENU_STATE", 1);
    } else if (m_gameplayChoice == 1) {
      _gd.m_field00 = 4;  // free play
      _gd.m_fieldE8 = 1;
      SetSelf(this, "MMU_SET_MENU_STATE", 1);
    } else if (!m_creditsTarget && m_gameplayChoice == 2) {
      SetSelf(this, "MMU_SET_MENU_STATE", 2);
    }
    return;
  }
  if (m_state != 4) {
    if (m_creditsTarget && m_gameplayChoice == 2) OnCancelKeyPressed(nullptr, nullptr);
    return;
  }
  if (m_playersChoice != 0 && m_controllers < 2) return;
  _gd.SetTwoPlayerMode(m_playersChoice == 2);
  if (m_playersChoice == 0) {
    if (_pCtrlMan->GetPlayerMapped(1)) {
      _app.m_field490 = 0;
      if (AptViewer* v = TheAptViewer()) {
        v->m_field3AC = -1;
        v->m_field3A8 = 0;
      }
      _pCtrlMan->UnmapPlayer(1);
    }
  } else if (!_pCtrlMan->GetPlayerMapped(1)) {
    // "Player 2, press START": player 2's pad gets mapped by the first other pad pressing start
    // (ESimsApp m_field490); Update closes the dialog then, or when player 1 cancels.
    _app.m_field490 = 1;
    m_field190 = 1;
    m_dialog.m_continueType = 1;
    m_dialog.m_field1C = 1;
    m_dialog.m_numSelections = 0;
    m_dialog.m_field14 = 0;
    m_dialog.m_field20 = 1;
    m_dialog.m_body.clear();
    for (const unsigned short* t = EGlobal::NewUIText("front_two_player_press_start"); t && *t; ++t)
      m_dialog.m_body.push_back(*t);
    m_dialog.m_title.clear();
    m_dialog.m_onSelection = [this](int sel) { OnDialog2Close(sel); };
    m_dialog.m_onZeroInput = [this] { OnCancelDialog2(); };
    m_dialog.m_onClosed = [this] { Dialog2ClosedCallback(); };
    m_dialog.SpawnDialog();
    return;
  }
  EndPlayerNumberChooser();
}

// 801C77CC
void MMUTarget::OnCancelKeyPressed(char*, char*) {
  if (m_field8C == 0) SetVariable(const_cast<char*>("MMU_GO_UP_TREE"), nullptr);
}

// 801C781C
// O opens the development menu (when enabled).
void MMUTarget::OnCircleKeyPressed(char*, char*) {
  if (m_field8C == 0 && g_mmuDevMenu) {
    ChangeScreenMode();
    UpdateText();
  }
}

// 801C7D80
// Sets the menu state (and remembers it for GO_UP_TREE) and tells the movie ("resetState").
void MMUTarget::UpdateMenuState(char*, char* value) {
  const int state = value ? std::atoi(value) : 0;
  if (state == 1) {
    m_lastState = m_state;
    m_state = 1;
    UpdateText();
  } else if (state == 0) {
    m_creditsTarget = nullptr;
    m_lastState = m_state;
    m_state = 0;
    if (m_lastState == 2) m_gameplayChoice = 0;
    UpdateText();
  } else if (state == 2) {
    if (!m_creditsTarget && m_gameplayChoice == 2) {
      auto* credits = new CRDTarget;
      m_creditsTarget = credits;
      credits->ShowCredits();
      m_lastState = m_state;
      m_state = state;
    }
  } else if (state == 4) {
    m_lastState = m_state;
    m_state = 4;
    m_controllers = GetNumberOfControllers();
    UpdateText();
  }
  if (m_pushState == 1) m_stateStack.push_back(state);
  m_pushState = 1;
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d", m_state);
  char* argv[1] = {buf};
  if (AptViewer* v = TheAptViewer()) v->NewCallFunction2(const_cast<char*>("resetState"), nullptr, nullptr, 1, argv);
}

// 801C7F24
// Back to the previous state; leaving for the gameplay screen frees both players' pads.
void MMUTarget::GoUpMenuTree(char*, char*) {
  if (m_stateStack.size() == 1) return;
  m_pushState = 0;
  m_stateStack.pop_back();
  const int previous = m_stateStack.back();
  if (previous == 0) {
    _pCtrlMan->UnmapPlayer(0);
    _pCtrlMan->UnmapPlayer(1);
    if (AptViewer* v = TheAptViewer()) {
      v->m_field3AC = 0;
      v->m_field3A8 = 0;
      v->AllowControllerReading(true);
    }
  }
  SetSelf(this, "MMU_SET_MENU_STATE", previous);
}

// 801C8228
void MMUTarget::IncrementChoice(char*, char*) {
  if (m_field8C) return;
  if (m_state == 1) SetSelf(this, "MMU_Main_Menu_Choice", m_topLevelChoice + 1 > 1 ? 0 : m_topLevelChoice + 1);
  if (m_state == 0) SetSelf(this, "MMU_Gameplay_Menu_Choice", m_gameplayChoice + 1 > 2 ? 0 : m_gameplayChoice + 1);
  if (m_state == 4)
    SetSelf(this, "MMU_Number_Of_Players_Menu_Choice", m_playersChoice + 1 > 1 ? 0 : m_playersChoice + 1);
}

// 801C8420
void MMUTarget::DecrementChoice(char*, char*) {
  if (m_field8C) return;
  if (m_state == 1) SetSelf(this, "MMU_Main_Menu_Choice", m_topLevelChoice - 1 < 0 ? 1 : m_topLevelChoice - 1);
  if (m_state == 0) SetSelf(this, "MMU_Gameplay_Menu_Choice", m_gameplayChoice - 1 < 0 ? 2 : m_gameplayChoice - 1);
  if (m_state == 4)
    SetSelf(this, "MMU_Number_Of_Players_Menu_Choice", m_playersChoice - 1 < 0 ? 1 : m_playersChoice - 1);
}

// 801C86C4
void MMUTarget::PollControllersForNumOfPlayersState() {
  const int n = GetNumberOfControllers();
  if (m_controllers == n) return;
  m_controllers = n;
  if (AptViewer* v = TheAptViewer()) v->NewCallFunction2(const_cast<char*>("checkForTwoPlayer"), nullptr, nullptr, 0, nullptr);
}

// 801C8720
// "Yes" to a new game: player 1 creates the family, then the number of players is asked.
void MMUTarget::OnDialogClose(int selection) {
  if (selection != 1) return;
  _gd.m_fieldF0 = 0;
  _gd.m_field84 = 1;
  SetupCAS();
  g_loadedGameSlot2 = 0;
  g_loadedGameSlot = 1;
  OnStartGameComplete(true);
}
// 801C8788
void MMUTarget::OnCancelDialog() {}
// 801C878C
void MMUTarget::OnDialog2Close(int) {}
// 801C8790
void MMUTarget::OnCancelDialog2() {}
// 801C8794
// The press-start dialog closed: with player 2 in, the players are chosen.
void MMUTarget::Dialog2ClosedCallback() {
  if (!_pCtrlMan->GetPlayerMapped(1)) _app.m_field490 = 0;
  else EndPlayerNumberChooser();
  m_field190 = 0;
}

// 801C5DD0
char* MMUTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  char* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  const std::string n = name;
  if (n == "MMU_exists") {
    m_ready = 1;
    std::strcpy(out, "loaded");
  } else if (n == "MMU_Is_Dev_Menu_Enabled") {
    std::strcpy(out, g_mmuDevMenu ? "1" : "0");  // DAT_804761D0
  } else if (n == "MMU_Is_Two_Player_Enabled") {
    std::strcpy(out, GetNumberOfControllers() < 2 ? "0" : "1");
  } else if (n == "MMU_GETVAR_CURRENT_STATE") {
    std::snprintf(out, 0x20, "%d", m_state);
  } else if (n == "MMU_GETVAR_NUMBER_OF_CHOICES_FOR_CURRENT_STATE") {
    std::snprintf(out, 0x20, "%d", GetNumberOfChoicesInState(m_state));
  } else if (n == "MMU_Shorten_Lot_List") {
    std::strcpy(out, "1");  // GC: "0" with the author mode or free-play lots shown
  }
  return out;
}

// 801C5F80
unsigned short* MMUTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  const std::string n = name;
  const char* row = nullptr;
  if (n == "MMU_Main_Menu_Text") return MallocAscii("Main Menu");
  if (n == "MMU_Development_Menu" || n == "MMU_Author_Mode") return MallocAscii("");
  if (n == "MMU_Top_Level_Choice_Text_0") row = "front_game_load";
  else if (n == "MMU_Top_Level_Choice_Text_1") row = "front_game_new";
  else if (n == "MMU_Credits_Text") row = "front_credits";
  else if (n == "MMU_Select_Gameplay_Text") return MallocAscii("Select Gameplay Mode");
  else if (n == "MMU_Story_Mode_Text") row = "front_story";  // GC: or the story-mode override string
  else if (n == "MMU_Freeplay_Text") row = "front_freeplay";
  else if (n == "MMU_Number_Of_Players_Text") row = m_gameplayChoice == 0 ? "front_story" : "front_freeplay";
  else if (n == "MMU_One_Or_Two_Player_Text") return MallocAscii("One Player or Two Player?");
  else if (n == "MMU_One_Player_Text") row = "front_one_player";
  else if (n == "MMU_Two_Player_Text") row = "front_two_player";
  else if (n.compare(0, 8, "MMU_List") == 0) {
    unsigned short* item = GetListItemName(const_cast<char*>(n.c_str()));
    return item ? item : MallocAscii("");
  } else {
    return MallocAscii("");
  }
  std::vector<unsigned short> text(0x1000, 0);
  const unsigned short* t = EGlobal::NewUIText(row);
  for (size_t i = 0; t && t[i] && i < text.size() - 1; ++i) text[i] = t[i];
  UIQDTarget::UIQDSubstitutionStrings(text.data());
  return MallocText(text.data());
}

// 801C63C4
// Closed credits go back up the tree; on the players screen the pads are polled (or, waiting for
// player 2, the dialog closes once the second pad presses start).
void MMUTarget::Update() {
  if (m_fieldFC) {
    delete m_creditsTarget;
    m_fieldFC = 0;
    m_creditsTarget = nullptr;
    GoUpMenuTree(nullptr, nullptr);
  }
  if (m_state != 4) return;
  if (!m_field190) {
    PollControllersForNumOfPlayersState();
  } else {
    EController* pad = _pCtrlMan->GetPlayerController(0);
    if (!_pCtrlMan->GetPlayerMapped(1)) {
      if (!pad || !pad->GetCmdResult(6, pad->GetBtnPressedMask(0xffffffffu), pad->GetBtnDownMask(0xffffffffu), false))
        return;
    }
    m_dialog.HideDialog();
  }
}

// 801C64D0
// With the development menu enabled: the hint to open it ("Press O Key for ..."; nothing while it is
// open), yellow, size 15, at (0.02, 0.02).
void MMUTarget::Draw(ERC* rc) {
  if (!g_mmuDevMenu || !m_font) return;
  std::string text;
  if (m_field8C == 0) {
    text = "Press O Key for ";
    if (_gd.m_fieldF0 == 0) text += "Development Menu";
    else if (_gd.m_fieldE8 == 0) text += "Author Mode Menu";
    else text += "AuthorMode with Freeplay Menu";
  }
  m_font->Select(rc);
  EVec4 yellow{1.0f, 1.0f, 0.0f, 1.0f};  // _YELLOW
  m_font->SetColor(yellow);
  m_font->SetSize(15.0f, 1.0f, true);
  EVec2 pos{0.02f, 0.02f};
  m_font->Draw(rc, const_cast<char*>(text.c_str()), pos, EFontAlignX{0}, EFontAlignY{0}, nullptr, nullptr,
               EFontMatrixType{0}, nullptr, true);
}

// 801C786C
void MMUTarget::UpdateTopLevelChoice(char*, char* value) {
  m_lastTopLevelChoice = m_topLevelChoice;
  m_topLevelChoice = std::atoi(value);
  OnTopLevelChoiceChange();
}
// 801C78B0
void MMUTarget::UpdateGameplayChoice(char*, char* value) {
  m_lastGameplayChoice = m_gameplayChoice;
  m_gameplayChoice = std::atoi(value);
  OnGameplayLevelChoiceChange();
}
// 801C78F4
void MMUTarget::UpdateNumberOfPlayersChoice(char*, char* value) {
  m_lastPlayersChoice = m_playersChoice;
  m_playersChoice = std::atoi(value);
  OnNumberOfPlayersChoiceChange();
}
// 801C7938
void MMUTarget::OnTopLevelChoiceChange() { UpdateText(); }
// 801C7958
void MMUTarget::OnGameplayLevelChoiceChange() { UpdateText(); }
// 801C7978
void MMUTarget::OnNumberOfPlayersChoiceChange() { UpdateText(); }

// 801C7998
// All choice texts white, then the current state's container shown and its choice in red.
void MMUTarget::UpdateText() {
  UIReflow r;
  if (m_field8C) {
    r.OpenReflowBlock();
    for (const char* id : {"MMU_SELECT_GAMEPLAY_CONTAINER", "MMU_MAIN_MENU_CONTAINER", "MMU_NUMBER_OF_PLAYERS_CONTAINER",
                           "MMU_Author_Mode"}) {
      r.Reflow_Id(id, 0);
      r.Reflow_Visibility(false);
    }
    r.SubmitReflowBlock();
    return;
  }
  r.OpenReflowBlock();
  for (const char* id : {"MMU_Top_Level_Choice_Text_0", "MMU_Top_Level_Choice_Text_1", "MMU_Story_Mode_Text",
                         "MMU_Freeplay_Text", "MMU_Credits_Text", "MMU_One_Player_Text", "MMU_Two_Player_Text"}) {
    r.Reflow_Id(id, 0);
    r.Reflow_ColorOnlyOverride(0xffffff);
  }
  r.SubmitReflowBlock();
  r.OpenReflowBlock();
  auto containers = [&r](bool main, bool gameplay, bool players) {
    r.Reflow_Id("MMU_MAIN_MENU_CONTAINER", 0);
    r.Reflow_Visibility(main);
    r.Reflow_Id("MMU_SELECT_GAMEPLAY_CONTAINER", 0);
    r.Reflow_Visibility(gameplay);
    r.Reflow_Id("MMU_NUMBER_OF_PLAYERS_CONTAINER", 0);
    r.Reflow_Visibility(players);
  };
  const char* selected = nullptr;
  if (m_state == 1) {
    containers(true, false, false);
    if (m_topLevelChoice == 0) selected = "MMU_Top_Level_Choice_Text_0";
    else if (m_topLevelChoice == 2) selected = "MMU_Top_Level_Choice_Text_1";
  } else if (m_state == 0) {
    containers(false, true, false);
    if (m_gameplayChoice == 0) selected = "MMU_Story_Mode_Text";
    else if (m_gameplayChoice == 1) selected = "MMU_Freeplay_Text";
    else if (m_gameplayChoice == 2) selected = "MMU_Credits_Text";
  } else if (m_state == 4) {
    containers(false, false, true);
    if (m_playersChoice == 0) selected = "MMU_One_Player_Text";
    else if (m_playersChoice == 2) selected = "MMU_Two_Player_Text";
  }
  if (selected) {
    r.Reflow_Id(selected, 0);
    r.Reflow_ColorOnlyOverride(0xff0000);
  }
  r.SubmitReflowBlock();
}

// 801C80C4
// Pads whose status has bits 1 and 4 (connected and stable).
int MMUTarget::GetNumberOfControllers() {
  int n = 0;
  for (unsigned int i = 0; i < 4; ++i)
    if (EController* c = _pCtrlMan->GetController(i))
      if ((c->m_status & 5) == 5) ++n;
  return n;
}

// 801C81E0
int MMUTarget::GetNumberOfChoicesInState(int) {
  switch (m_state) {
    case 0: return 2;
    case 1: return 3;
    case 4: return 2;
    default: return 0;
  }
}

// 801C8364
void MMUTarget::SetCurrentChoice(char* value) {
  if (m_state == 1) SetVariable(const_cast<char*>("MMU_Main_Menu_Choice"), value);
  if (m_state == 0) SetVariable(const_cast<char*>("MMU_Gameplay_Menu_Choice"), value);
  if (m_state == 4) SetVariable(const_cast<char*>("MMU_Number_Of_Players_Menu_Choice"), value);
}

// 801C8550
void MMUTarget::OnStartGameComplete(bool loaded) {
  m_controllers = GetNumberOfControllers();
  if (loaded) {
    m_field190 = 0;
    SetVariable(const_cast<char*>("MMU_SET_MENU_STATE"), const_cast<char*>("4"));
  }
}
