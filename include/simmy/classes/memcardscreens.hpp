// @class SimsMemCardWrap
// @class MemCardSaveLoad
// @class M2MTarget after UIObjectBase UIDialog UIReflow MemCardSaveLoad
// @class LoadGameTarget after UIObjectBase UIDialog MemCardSaveLoad M2MTarget
// @class SaveGameTarget after UIObjectBase UIDialog MemCardSaveLoad M2MTarget
// The memory card screens (game/simsmemcardwrap.cpp, targets_s2c.cpp M2MTarget, loadgametarget.cpp,
// savegametarget.cpp). M2MTarget is the slot select screen (screen 0x30 "M2M", a widget screen on
// s2c_comp_dialogbox.swf): it lists the memory card slots, runs the main menu's boot check (can a
// new game be saved?) and hands a chosen slot to a LoadGameTarget or SaveGameTarget, which check the
// card (wrong device, damaged, corrupt, unformatted, no save, no space) with G2D dialogs, format it
// on request and load or save the game. SimsMemCardWrap is the game's layer over EMemoryCard.
#pragma once

#include <string>
#include <vector>

// The game's memory card calls (all static). Results as EMemoryCard's (1 OK, -1 no card, ...).
struct SimsMemCardWrap {
  static void Init();  // 80074E60
  static int InitPolling();  // 80074F2C
  static void PollCards(bool force);  // 80074F7C
  static int IsPolledCardInSlot(int card, bool& present);  // 80074FB8
  static int IsCardInSlot(int card, bool& present);  // 80075008
  static int IsWrongDevice(int card, bool& wrong);  // 80075058
  static int IsCardFormatted(int card, bool& formatted);  // 800750A8
  static int IsCardCorrupted(int card, bool& corrupted);  // 800750E8
  static int IsCardDamaged(int card, bool& damaged);  // 80075150
  static int HasSavedGame(int card, bool& has, int slot);  // 800751B4
  static int HasValidSavedGame(int card, bool& has, int slot);  // 800751F8
  static int HasInvalidGame(int card, bool& invalid, int slot);  // 800752B4
  static int HasSpaceForSave(int card, bool& space, unsigned int* needed);  // 80075304
  static int CanBeUsed(int card, bool& usable, int slot);  // 80075394
  static int IsDifferentCard(int card, bool& different);  // 800754CC
  static int LoadConfigFile(int card);  // 80075548
  static int SaveConfigFile(int card);  // 80075590
  static void PreservePrefs();  // 800755DC
  static void RestorePrefs();  // 80075604
  static int IsFreeplayUnlocked();  // 8007562C
  static int FormatCard(int card);  // 8007563C
  static int DeleteFile(int card, int slot);  // 80075678
  static void XboxOnNoMemory();  // 800756B8
  static unsigned int GetSpaceRequiredForSave();  // 800756BC
  static int SaveNewGame(int card);  // 800756DC
  static int PreloadGame(int card);  // 8007578C
  static int LoadGame(int card, int slot);  // 8007581C
  static int SaveGame(int card, bool all, bool pristine, int slot);  // 800758BC
  static int LoadHouse(int house);  // 800759D4
  static int ReloadHouseAndGlobals(int card, int house);  // 80075A64
  static int SaveHouseAndGlobals(int card, int house);  // 80075ABC
};

// The interface M2MTarget polls a running load or save through (vtable MemCardSaveLoad).
struct MemCardSaveLoad {
  virtual ~MemCardSaveLoad() = default;
  virtual int GetResult() = 0;
};

// The slot select screen. Widget states (+0x168): 0 hidden, 1 to be filled, 2 shown, 3 leaving
// (MEM_destination decides the flow), 4 unloading, 5 done. Flow states (+0x11C) as the game's:
// 0 main menu entry, 1 cancelled, 2 boot check, 3 no card at boot, 4 confirm new game, 5 new game,
// 6 no card (continue), 7 no space, 8 no card, 9 load: pick a slot, 10 loaded, 11/12 save before
// quitting, 13/14 no card (save), 15 save: pick a slot.
struct M2MTarget : UIObjectBase, UIDialog {
  enum WidgetState : int { kHidden, kFill, kShown, kLeaving, kUnloading, kDone };
  struct ResultStruct {
    int device = 0xff;   // +0x10C slot (0xff: not chosen)
    int saveId = 0xff;   // +0x110 save game in the slot (0xff: not chosen, -1 the only one)
    int flag = 1;        // +0x114
  };
  M2MTarget();  // 801BF058
  ~M2MTarget() override;  // 801BFCDC
  char* GetVariable(char* name) override;  // 801BFEE8
  unsigned short* GetLocalizable(char* name) override;  // 801C00A0
  void SetVariable(char* name, char* value) override;  // 801C02EC
  void ReflowButtonSelection();  // 801C0464
  void Update() override;  // 801C05A4
  void SelectionCallback(int selection) override;  // 801C09A0
  void ZeroInputCallback() override;  // 801C0C9C
  void DialogClosedCallback() override;  // 801C0CA0
  void OnCancel();  // 801C0D08
  void OnSaveLoadSucceed();  // 801C0D94
  void OnSaveLoadFailed();  // 801C0EC8
  void OnSlotSelectItemSelected(int item, ResultStruct& result);  // 801C1094
  int OnGetNumItems(ResultStruct& result);  // 801C10C8
  int GetDevice();  // 801C1108
  void SetDevice(int device);  // 801C1110
  int GetSaveGameID();  // 801C113C
  int GetMemCardPort();  // 801C1144
  void SpawnNoSpaceDialog();  // 801C115C
  void SpawnConfirmStartNewGameDialog();  // 801C12A8
  void SpawnContinueNoMemCardDialog();  // 801C13EC
  void SpawnSaveBeforeQuitCASDialog();  // 801C1540
  void SpawnSaveBeforeQuitGameDialog();  // 801C16AC
  void SpawnNoSaveBeforeQuitDialog();  // 801C1818
  void SpawnStartNGCNoMemCardDialog();  // 801C195C
  void GetItemTextMemCardSelect(int item, unsigned short* out);  // 801C1AA0
  void GetItemTextSaveGameSelect(int item, unsigned short* out);  // 801C1BE8
  int IsItemEnabledMemCardSelect(int item);  // 801C1C40
  int IsItemEnabledSaveGameSelect(int item);  // 801C1D00
  static int DoesMemoryDeviceExist();  // 801C1D08
  void StartLoadedGame();  // 801C1D70
  void ReturnFailureDestination();  // 801C1DE0
  void StartNGCMemoryCardBootChecks();  // 801C1EC4
  void UpdateWidgetState();  // 801C1F64
  void FillWidget();  // 801C2104
  void HideWidget();  // 801C21D8
  void SetupWidgetLayout();  // 801C2278
  void UpdateSelection(int delta);  // 801C294C
  int IsItemEnabled(int item);  // 801C29C4
  void CalcNumItems();  // 801C2A24
  void SetCurrState(WidgetState state);  // 801C2A74
  void UnloadDialog();  // 801C2D80
  void PrepareNewGame();  // 801C2E28
  void PrepareLoadGame();  // 801C2E78
  void PushButtonFilters();  // 801C2EF4
  void PopButtonFilters();  // 801C2F6C
  int GetPlayerNum();  // 801E7090

  ResultStruct m_result;                 // +0x10C
  MemCardSaveLoad* m_op = nullptr;       // +0x118 the running LoadGameTarget / SaveGameTarget
  int m_flow = 0;                        // +0x11C
  UIReflow m_reflow;                     // +0x120
  std::vector<unsigned short> m_bodyText;  // +0x134
  std::vector<unsigned short> m_items[8];  // +0x138
  int m_bootResults[2] = {0, 0};         // +0x158 boot check result per slot
  int m_boxHeight = 0;                   // +0x160
  int m_boxHeight2 = 0;                  // +0x164
  int m_widgetState = kHidden;           // +0x168
  int m_lastWidgetState = kHidden;       // +0x16C
  unsigned int m_filters[2] = {0, 0};    // +0x170
  int m_cardPresent[2] = {0, 0};         // +0x178
  int m_pending = 0;                     // +0x180 act on m_flow in Update
  int m_active = 0;                      // +0x184 the list takes input
  int m_numItems = 0;                    // +0x188
  int m_selection = 0;                   // +0x18C
  int m_player = 0;                      // +0x190
  int m_fromCAS = 0;                     // +0x194 MEM_source 2 (create a sim)
};

struct LoadGameTarget : UIObjectBase, UIDialog, MemCardSaveLoad {
  LoadGameTarget(M2MTarget* owner, int port, int flags);  // 80189AD0
  ~LoadGameTarget() override;  // 8018A200
  void SelectionCallback(int selection) override;  // 8018A374
  void ZeroInputCallback() override;  // 8018A6BC
  void Update() override;  // 8018A708
  int GetResult() override;  // 8018AAE8
  static void ImportLoad(NghResFile__SectionID, NghResFile__SaveRecord&);  // 8018AAF0
  int PerformMemoryCardChecks();  // 8018AAF4
  int PerformLoad();  // 8018AD38
  void SpawnCheckingDialog();  // 8018AE94
  void SpawnWrongDeviceDialog();  // 8018AFC4
  void SpawnCardDamagedDialog();  // 8018B1EC
  void SpawnCardCorruptDialog();  // 8018B454
  void SpawnWrongSaveDialog();  // 8018B68C
  void SpawnNoSaveDialog();  // 8018B8C4
  void SpawnLoadConfirmDialog();  // 8018BA3C
  void SpawnLoadingDialog();  // 8018BBE0
  void SpawnLoadSucceededDialog();  // 8018BD08
  void SpawnLoadFailedDialog();  // 8018BE24
  void SpawnMemCardRemovedDialog();  // 8018C05C
  void SpawnSameCardDialog();  // 8018C1C0
  void SpawnFormatDecideDialog();  // 8018C2DC
  void SpawnFormatConfirmDialog();  // 8018C420
  void SpawnFormattingDialog();  // 8018C564
  void SpawnFormatCancelDialog();  // 8018C634
  void SpawnFormatFailDialog();  // 8018C778
  void SpawnFormatSuccessDialog();  // 8018C97C
  void BackgroundPerformLoad();  // 8018CA90
  void BGCall_PerformLoad();  // 8018CC40
  static void BGExec_PerformLoad(void* self);  // 8018CC8C
  static void ImportSave(NghResFile__SectionID, NghResFile__SaveRecord&);  // 8018CCBC
  static void ImportSaveComplete(NghResFile__SectionID, NghResFile__SaveRecord&);  // 8018CCC0
  int GetDevice();  // 8018CCC4
  int GetPlayerNum();  // 8018CCE8
  int GetSaveGameID();  // 8018CCF4

  int m_result = 0;          // +0x10C 0 running, 1 loaded, 2 cancelled, 3 nothing to load, 6 card gone, 8 card swapped
  M2MTarget* m_owner;        // +0x118
  int m_swapSlot = 0;        // +0x11C
  int m_swapped = 0;         // +0x120
  int m_diskErrors = 0;      // +0x124
  int m_port;                // +0x128
  int m_state = -1;          // +0x12C
  int m_pending = 0;         // +0x130
  int m_loadStep = 0;        // +0x134
  int m_flags;               // +0x138 1 must have a save, 2 import, 4 skip the game, 8 other card, 0x10 cancel ends
  int m_field13C = 0;        // +0x13C
  int m_confirmed = 0;       // +0x140
  int m_loaded = 0;          // +0x144
  unsigned int m_filter = 0;  // +0x148
};

struct SaveGameTarget : UIObjectBase, UIDialog, MemCardSaveLoad {
  enum NoSpaceType : int { kNoSpace, kNoFiles, kNoSpaceAndFiles };
  SaveGameTarget(M2MTarget* owner, int port, int flags);  // 8018CD18
  ~SaveGameTarget() override;  // 8018D478
  void SelectionCallback(int selection) override;  // 8018D5EC
  void ZeroInputCallback() override;  // 8018D8E0
  void Update() override;  // 8018D9BC
  virtual void Hide();  // 8018DDE4
  int GetResult() override;  // 8018DE08
  int PerformMemoryCardChecks();  // 8018DE10
  void SpawnCheckingDialog();  // 8018E0A8
  void SpawnCardCorruptDialog();  // 8018E1D8
  void SpawnWrongDeviceDialog();  // 8018E3A4
  void SpawnCardDamagedDialog();  // 8018E508
  void SpawnFormatDecideDialog();  // 8018E6DC
  void SpawnFormatConfirmDialog();  // 8018E840
  void SpawnFormattingDialog();  // 8018E99C
  void SpawnFormatCancelDialog();  // 8018EA84
  void SpawnFormatFailDialog();  // 8018EBE0
  void SpawnFormatSuccessDialog();  // 8018ED3C
  void SpawnNoSpaceDialog(NoSpaceType type);  // 8018EE6C
  void SpawnOverwriteDialog();  // 8018F0D0
  void SpawnSaveConfirmDialog();  // 8018F274
  void SpawnSavingDialog();  // 8018F41C
  void SpawnSaveSucceededDialog();  // 8018F544
  void SpawnSaveFailedDialog();  // 8018F660
  void SpawnMemCardRemovedDialog();  // 8018F7D8
  int PerformSaveGame();  // 8018F944
  void BGCall_SaveGame();  // 8018FEF0
  static void BGExec_SaveGame(void* self);  // 8018FF3C
  int GetDevice();  // 8018FF8C
  int GetPlayerNum();  // 8018FFB0
  int GetSaveGameID();  // 8018FFBC

  int m_result = 0;          // +0x110 0 running, 1 done (saved / checks passed), 2 cancelled, 4 manage card,
                             // 5 no card, 6 cancel ends, 7 no space, 8 card swapped
  M2MTarget* m_owner;        // +0x118
  int m_swapSlot = 0;        // +0x11C
  int m_swapped = 0;         // +0x120
  int m_port;                // +0x128
  int m_state = 0;           // +0x12C
  int m_pending = 1;         // +0x130
  int m_saveStep = 0xb;      // +0x134
  int m_flags;               // +0x138 2 checks only, 4 offer card management, 8 card must stay, 0x10 cancel
                             // ends, 0x20 no space ends, 0x40 quiet (boot check), 0x80 removal ends,
                             // 0x100 boot-up text
  int m_saveResult = 0;      // +0x13C
  int m_saved = 0;           // +0x140
  int m_savedFlag = 0;       // +0x144
  int m_simPaused = 0;       // +0x148
  int m_formatted = 0;       // +0x14C
  int m_formatOK = 0;        // +0x150
  int m_removedWhileSaving = 0;  // +0x154
  unsigned int m_filter = 0;  // +0x158
  int m_frames = 0;          // +0x15C
  float m_startTime = 0;     // +0x164
};
