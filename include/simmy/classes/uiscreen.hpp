// @class WidgetScreenFormat after UIObjectBase
// @class UIButtonImages after UIObjectBase
// @class UI2D after UIObjectBase
// @class UIReflow after UIObjectBase
// @class UIScreenManager after UIObjectBase
// @class UIDialog after UIObjectBase
// @class G2DTarget after UIObjectBase UIReflow UIDialog
// @class MMUTarget after UIObjectBase UIReflow UIDialog
// Flash screens (game/uiscreenmanager.cpp, uireflow.cpp) and the main menu target (MMUTarget,
// game/targets_s2c.cpp). The screen manager asks s2c_root to load, suspend, restore and remove a
// screen movie (cmdProc "LoadCustomScreen"/"SuspendScreen"/...) and keeps a record per screen;
// UIReflow builds the "<reflow>" XML blocks that restyle a screen's elements.
#pragma once

#include <deque>
#include <functional>
#include <string>
#include <vector>

// Widget screens: a layout (widgetlayout.cpp) of XML tags and values joined into one "<scrn>"
// document with the player's digit on ids, then created by s2c_root ("LoadWidget").
struct WidgetScreenFormat {
  enum IterationType : int { kValidate = 1, kMeasure = 2, kBuild = 3 };
  WidgetScreenFormat() = default;  // 8018230C
  ~WidgetScreenFormat() = default;  // 80182328
  void LaunchWidgetScreenLayout(const char* const* layout, int player, int inputMode);  // 8018237C
  bool IterateScreenLayout(const char* const* layout, int type);  // 80182458
  void IterateTags(int type);  // 801824D8
  const char* GetNextTag(int type);  // 801826D4
  bool IsValidTag(const char* tag);  // 80182904
  RET_UNKNOWN IsDataValid(char *);  // 80182990
  bool IsPlayerSpecificTag(const char* tag);  // 80182E3C

  int m_result = 0;                       // +0x00 1 done, 2 bad layout
  const char* const* m_pos = nullptr;     // +0x04
  const char* m_value = nullptr;          // +0x08
  size_t m_size = 0;                      // +0x0C
  std::string m_out;                      // +0x10/+0x14
  const char* m_tag = "";                 // +0x18 the tag the values belong to
  int m_player = 0;                       // +0x1C
};

// The pad button pictures that "<BUTTON_xx>" in UI text shows (game/uisupport.cpp).
struct UIButtonImages {
  enum : int { kButtons = 14 };
  static void Startup();  // 801811A0
  static void Shutdown();  // 801811E8
  UIButtonImages();  // 80181244
  virtual ~UIButtonImages();  // 801812AC
  void DefineIDs();  // 801812E0
  void AddRefAll();  // 8018155C
  void DelRefAll();  // 801815B0
  static ERShader* GetButtonShader(int button);  // 80181604
  static double GetButtonMinSize(int button);  // 80181678
  static void LocalizeXBoxImages();  // 801816A0

  unsigned int m_ids[kButtons] = {};       // +0x00 shader ids
  ERShader* m_shaders[kButtons] = {};      // +0x38
  float m_minSize[kButtons] = {};          // +0x70
  int m_referenced = 0;                    // +0xA8
  int m_platform = 3;                      // +0xAC 3 GameCube, 2 Xbox, else PS2
  static UIButtonImages* instance;
};

// Named 2D shaders for the movies' custom-rendered rectangles ("CCR2D" clips, game/ui2d.cpp).
struct UI2D {
  struct Record {
    std::string name;      // +0x08
    unsigned int id = 0;   // +0x0C shader resource id (0: given as a shader)
    int state = 0;         // +0x10 multi-shader state
    std::string group;     // +0x14 the target that installed it
    ERShader* shader = nullptr;  // +0x18
    bool async = false;    // +0x1C added by id (AddRefAsync)
    int multi = 0;         // +0x20 a UI multi-state shader still switching to `state`
    unsigned int stateId = 0;  // +0x24 the state's shader (referenced while the entry lives)
    int stateOk = 0;       // +0x28 SetShaderASyncForState accepted the state
  };
  UI2D() = default;  // 80175FB0
  virtual ~UI2D();  // 8017606C
  void AddEntry(const char* name, unsigned int id, ERShader* shader, const char* group, int state);  // 8017610C
  void RemoveEntry(size_t index);  // 80176274
  bool InstallEntry(char* name, unsigned int id, char* group, int state);  // 801763B8
  bool InstallEntry(char* name, ERShader* shader, char* group, int state);  // 8017643C
  Record* FindEntry(const char* name);  // 801764CC
  bool UnInstallEntry(char* name);  // 80176584
  bool UnInstallAllEntries(char* group);  // 801765C8
  bool ContainsEntry(char* name);  // 80176628
  ERShader* GetShader(const char* name);  // 80176658
  static UI2D* Instance();  // 801767F8
  static void Shutdown();  // 80176834

  std::vector<Record> m_records;
  static UI2D* s_pInstance;
};

struct UIReflow {
  UIReflow();  // 8017D234
  ~UIReflow();  // 8017D254
  void SetFocus(UIScreenID);  // 8017D2B0
  static void ResetSize(char *, int, int, int);  // 8017D320
  void ResetPosition(const char* id, int x, int y, int player);  // 8017D4A0
  RET_UNKNOWN ResetZ(char *, int, int);  // 8017D620
  void ResetVisibility(const char* id, bool visible, int player);  // 8017D75C
  void ResetAlpha(char *, unsigned int, int);  // 8017D898
  void ResetText(char *, char *, int);  // 8017D9E4
  void ResetColorOverride(char *, unsigned int, int);  // 8017DB04
  void ResetFontSizeOverride(const char* id, unsigned int size, int player);  // 8017DCAC
  void ResetSwfAnimation(const char* id, const char* animation, int player);  // 8017DDE8
  void ResetSwfFileName(char *, char *, int);  // 8017DF08
  void AddToBlock(const char* text);  // 8017E028
  void ReallocateBlock();  // 8017E0B0
  void OpenTweenBlock(const char* id, unsigned int ms, unsigned int method, const char* callback, int player);  // 8017E138
  void SubmitTweenBlock();  // 8017E318
  void Tween_X(int x);  // 8017E39C
  void Tween_Y(int y);  // 8017E408
  void Tween_Z(unsigned int);  // 8017E474
  void Tween_H(unsigned int h);  // 8017E4E0
  void Tween_W(unsigned int w);  // 8017E54C
  void Tween_XScale(float);  // 8017E5B8
  void Tween_YScale(float);  // 8017E638
  void Tween_Rotation(float);  // 8017E6B8
  void Tween_Alpha(unsigned int alpha);  // 8017E72C
  void Tween_ColorOverride(unsigned int rgba);  // 8017E7A8
  void Tween_ColorOnlyOverride(unsigned int rgb);  // 8017E838
  void Tween_FontSizeOverride(unsigned int);  // 8017E8B4
  void OpenReflowBlock();  // 8017E920
  void SubmitReflowBlock();  // 8017E998
  void Reflow_Id(const char* id, int player);  // 8017EA1C
  void Reflow_X(int x);  // 8017EAB0
  void Reflow_Y(int y);  // 8017EAD0
  void Reflow_Z(unsigned int);  // 8017EAF0
  void Reflow_H(unsigned int h);  // 8017EB10
  void Reflow_W(unsigned int w);  // 8017EB30
  void Reflow_XScale(float);  // 8017EB50
  void Reflow_YScale(float);  // 8017EB70
  void Reflow_Rotation(float);  // 8017EB90
  void Reflow_Visibility(bool visible);  // 8017EBB0
  void Reflow_Alpha(unsigned int alpha);  // 8017EC28
  void Reflow_StringChange(char *);  // 8017EC48
  void Reflow_ColorOverride(unsigned int rgba);  // 8017EC9C
  void Reflow_ColorOnlyOverride(unsigned int rgb);  // 8017ECBC
  void Reflow_FontSizeOverride(unsigned int);  // 8017ECDC
  void Reflow_SwfAnimation(char *);  // 8017ECFC
  void Reflow_SwfFile(char *);  // 8017ED50
  static void Debug_PrintBlock();  // 8017EDA4

  std::string m_block;   // +0x00..+0x10 GC: a MainHeap buffer with a write pointer and size
  bool m_open = false;
};

// One entry of UIScreenDataTable (24 bytes): screen id, how loading it suspends others (1 all,
// 2 none, 3 the HUD widgets, 4 everything but a few), whether it takes input, the widget layout XML
// (WidgetScreenFormat) or the custom movie's three-letter id.
struct UIScreenData {
  int id;
  int suspendMode;
  int handlesInput;
  int player;
  const char* const* widgetLayout;
  const char* flashName;
};

struct UIScreenManager : UIObjectBase {
  UIScreenManager();  // 8017F0D0
  ~UIScreenManager() override;  // 8017F688
  const UIScreenData* FindDataByID(int id);  // 8017F838
  RET_UNKNOWN process_cheats(UIScreenID);  // 8017F87C
  int LoadUIScreen(int id, int inputMode);  // 8017F884
  int UnloadUIScreen(int id);  // 8017FAE4
  bool IsScreenManaged(int id);  // 8017FC74
  int SuspendAllActiveScreens();  // 8017FCB0
  int SuspendScreenList(const int* ids);  // 8017FD58
  int AddScreen(int id, int group);  // 8017FE64
  int FindRecordIndexByID(int id);  // 8017FEB8
  bool RemoveScreen(int id);  // 8017FEF8
  int GetScreenFlashName(char* out, int id);  // 8017FF44
  bool IsUIScreenUnloaded(int id);  // 8018003C
  static bool IsUIScreenMarkedForUnloading(UIScreenID);  // 80180068
  int SetHandlesInput(int id, bool handles);  // 801800BC
  void SetVariable(char* name, char* value) override;  // 8018019C
  void Update() override;  // 80180274
  void PopQueue(int index);  // 80180308

  struct Record {
    int id = 0;
    int state = 0;    // 1 active, 2 suspended, 3 being removed
    int group = 0;    // the suspend group it was suspended with
    int suspendedGroup = 0;  // the group its loading suspended
  };
  Record m_records[24];  // +0x84
  int m_queue[8] = {};   // +0x204 screens waiting for a managed one to go
  static int m_groupNumber;
  static const UIScreenData UIScreenDataTable[];
};

// A dialog's contents and callbacks (game/targets_s2c.cpp), shown by the G2DTarget.
struct UIDialog {
  UIDialog();  // 801E6F2C
  virtual ~UIDialog();  // 801B7100
  void SpawnDialog();  // 801B71B4
  void ChangeContinueType(int type);  // 801B71E0
  void HideDialog();  // 801B7208
  virtual void SelectionCallback(int selection);  // 801E7044
  virtual void ZeroInputCallback();  // 801E7048
  virtual void DialogClosedCallback();  // 801E704C

  int m_numSelections = 0;      // +0x00 0..3 buttons
  int m_preselect = 0;          // +0x04
  int m_mode = 0;               // +0x08
  int m_continueType = 0;       // +0x0C
  int m_field10 = 0;            // +0x10
  int m_field14 = 1;            // +0x14
  int m_timeoutMs = -1;         // +0x18 (-1: g_g2dDialogTimeout)
  int m_field1C = 0;            // +0x1C
  int m_field20 = 0;            // +0x20
  int m_field24 = 0;            // +0x24
  std::vector<unsigned short> m_body;     // +0x28 BString2
  std::vector<unsigned short> m_title;    // +0x2C
  std::vector<unsigned short> m_sel[3];   // +0x30
  ERShader* m_icon = nullptr;   // +0x3C
  short m_iconW = 0;            // +0x48
  short m_iconH = 0;            // +0x4A
  int m_players = 3;            // +0x4C 1 player 1, 2 player 2, 3 both
  std::function<void(int)> m_onSelection;   // +0x50
  std::function<void()> m_onZeroInput;      // +0x60
  std::function<void()> m_onClosed;         // +0x70
};

// The modal dialog box (screen 0x2F "G2D", s2c_comp_dialogbox.swf): a title, a body and up to three
// buttons; states 1 spawned, 2 loaded, 3 shown, 4 closing, 5 fading out, 6 unloading.
struct G2DTarget : UIObjectBase {
  G2DTarget();  // 801B4490
  ~G2DTarget() override;  // 801B4BE8
  char* GetVariable(char* name) override;  // 801B4D9C
  unsigned short* GetLocalizable(char* name) override;  // 801B4FAC
  void SetVariable(char* name, char* value) override;  // 801B511C
  void ReflowButtonSelection();  // 801B5410
  void Update() override;  // 801B5724
  void HideDialog();  // 801B5960
  void HideDialogRunCallbacks();  // 801B59C0
  int IsG2DActive();  // 801B5A04
  int IsG2DSpawned();  // 801B5A0C
  void PushAptButtonFilters();  // 801B5A14
  void PushAptButtonFiltersForPlayer(int player);  // 801B5A50
  void PopAptButtonFilters();  // 801B5AAC
  void SpawnDialog(UIDialog* dialog);  // 801B5B14
  void CalculateBackgroundSize();  // 801B5E38
  void SetupWidgets();  // 801B6234
  void UnloadDialog();  // 801B690C
  void SetIcon(ERShader* icon, short w, short h);  // 801B6A60
  void ExecuteSelectionCallback(int selection);  // 801B6B24
  void ExecuteZeroInputCallback();  // 801B6B68
  void ExecuteDialogClosedCallback();  // 801B6BAC
  void ChangeContinueType(int type);  // 801B6C44
  void SetupDialogData(UIDialog* dialog);  // 801B6CC0
  void UnloadUI();  // 801B7000
  bool CheckPlayerInput(const char* value);  // 801B7040
  int ServiceG2DShutdown();  // 801B708C
  static int IsInitComplete();  // 801E7050
  bool HasPendingTransition();  // 801E7058

  int m_spawned = 0;            // +0x84
  int m_active = 0;             // +0x88
  int m_visible = 0;            // +0x8C
  int m_frames = 0;             // +0x90
  int m_field94 = 0;            // +0x94
  UIDialog* m_pending = nullptr;  // +0x98 a dialog to show after this one
  int m_player = 0;             // +0x9C 0/1, 2 either
  int m_mode = 0;               // +0xA0
  int m_numSelections = 1;      // +0xA4
  int m_preselect = 0;          // +0xA8
  int m_continueType = 0;       // +0xAC
  int m_selection = 0;          // +0xB0
  int m_fieldB4 = 0;            // +0xB4
  int m_fieldB8 = 1;            // +0xB8
  int m_fieldBC = 0;            // +0xBC
  int m_cancelAllowed = 1;      // +0xC0
  int m_closedEnabled = 0;      // +0xC4
  int m_selectAllowed = 0;      // +0xC8
  ERShader* m_icon = nullptr;   // +0xCC
  short m_iconW = 0, m_iconH = 0;  // +0xD0
  int m_boxHeight = 0;          // +0xD4
  int m_boxHeight2 = 0;         // +0xD8
  float m_box[8] = {};          // +0xDC centre x/y, text w/h, icon w/h, text x/y
  std::vector<unsigned short> m_body, m_title, m_sel[3];  // +0xFC
  std::function<void(int)>* m_onSelection = nullptr;   // +0x110
  std::function<void()>* m_onZeroInput = nullptr;      // +0x114
  std::function<void()>* m_onClosed = nullptr;         // +0x118
  int m_players = 1;            // +0x11C
  unsigned int m_filters[2] = {0xffffffffu, 0xffffffffu};  // +0x120
  float m_timeout = 0;          // +0x128
  float m_timer = 0;            // +0x12C
  int m_savedPlayerNumber = -1; // +0x130
  int m_state = 0;              // +0x134
  UIReflow m_reflow;            // +0x138
  int m_savedAnalog = 0;        // +0x14C
  unsigned int m_shown[9] = {}; // +0x150 last reflowed button/text/select states
  bool m_pcPrivate = false;     // PC co-op: the dialog belongs to a player's private screen (no pause)
  static int m_bInitComplete;
};

// The main menu movie's game side ("MMU_..." variables): menu state, choices and texts.
struct MMUTarget : UIObjectBase {
  MMUTarget();  // 801C49C0
  ~MMUTarget() override;  // 801C5830
  static MMUTarget* GetSingleton();  // 801C59E0
  void OnCreditsClose();  // 801C59E8
  void SetVariable(char* name, char* value) override;  // 801C59F4
  char* GetVariable(char* name) override;  // 801C5DD0
  unsigned short* GetLocalizable(char* name) override;  // 801C5F80
  void Update() override;  // 801C63C4
  void Draw(ERC* rc) override;  // 801C64D0
  unsigned short* GetListItemName(char* name);  // 801C6644
  void ChangeScreenMode();  // 801C668C
  void UpdateListItems();  // 801C6758
  void UpdateNGHListItems();  // 801C679C
  void UpdateLotListItems();  // 801C68E8
  unsigned short* GetNGHListItemName(char* name);  // 801C6A0C
  unsigned short* GetLotListItemName(char* name);  // 801C6B54
  void EnterLot(int);  // 801C6BFC
  void LaunchLot(int);  // 801C6D90
  void LaunchFreeplay();  // 801C6DF0
  void OnUpKeyPressed(char* name, char* value);  // 801C6E5C
  void OnDownKeyPressed(char* name, char* value);  // 801C6EE8
  void OnLeftKeyPressed(char* name, char* value);  // 801C6F84
  void OnRightKeyPressed(char* name, char* value);  // 801C6FA4
  void EndPlayerNumberChooser();  // 801C70F0
  void OnXKeyPressed(char* name, char* value);  // 801C7168
  void OnCancelKeyPressed(char* name, char* value);  // 801C77CC
  void OnCircleKeyPressed(char* name, char* value);  // 801C781C
  void UpdateTopLevelChoice(char* name, char* value);  // 801C786C
  void UpdateGameplayChoice(char* name, char* value);  // 801C78B0
  void UpdateNumberOfPlayersChoice(char* name, char* value);  // 801C78F4
  void OnTopLevelChoiceChange();  // 801C7938
  void OnGameplayLevelChoiceChange();  // 801C7958
  void OnNumberOfPlayersChoiceChange();  // 801C7978
  void UpdateText();  // 801C7998
  void UpdateMenuState(char* name, char* value);  // 801C7D80
  void GoUpMenuTree(char* name, char* value);  // 801C7F24
  int GetNumberOfControllers();  // 801C80C4
  int GetNumberOfChoicesInState(int state);  // 801C81E0
  void IncrementChoice(char* name, char* value);  // 801C8228
  void SetCurrentChoice(char* value);  // 801C8364
  void DecrementChoice(char* name, char* value);  // 801C8420
  void OnStartGameComplete(bool loaded);  // 801C8550
  void UpdateDevMenuState(char* name, char* value);  // 801C85D0
  void OnNGHSelect(char* name, char* value);  // 801C861C
  void ChangeScreenMode(int mode);  // 801C86B4
  void PollControllersForNumOfPlayersState();  // 801C86C4
  void OnDialogClose(int selection);  // 801C8720
  void OnCancelDialog();  // 801C8788
  void OnDialog2Close(int selection);  // 801C878C
  void OnCancelDialog2();  // 801C8790
  void Dialog2ClosedCallback();  // 801C8794
  RET_UNKNOWN GetCurrentMainScreenChoice() const;  // 801E7078
  RET_UNKNOWN GetCurrentGameplayChoice() const;  // 801E7080

  int m_ready = 0;                // +0x84 the movie asked MMU_exists
  int m_field8C = 0;              // +0x8C the development menu's mode (0 off, 1 neighbourhoods, 2 lots)
  int m_field90 = 0;              // +0x90 its previous mode
  int m_devLot = 0;               // +0x98 the lot highlighted
  int m_devNgh = 0;               // +0x9C the neighbourhood highlighted
  int m_devLotCount = 0x10;       // +0xA0
  int m_topLevelChoice = 0;       // +0xA4
  int m_lastTopLevelChoice = 0;   // +0xA8
  UIObjectBase* m_creditsTarget = nullptr;  // +0xAC CRDTarget while the credits run
  int m_state = 0;                // +0xB0 MMU_MENU_STATE (0 select gameplay, 1 main, 2 credits, 4 players)
  int m_lastState = 0;            // +0xB4
  int m_gameplayChoice = 0;       // +0xB8
  int m_lastGameplayChoice = 0;   // +0xBC
  int m_playersChoice = 0;        // +0xC0
  int m_lastPlayersChoice = 0;    // +0xC4
  std::deque<int> m_stateStack;   // +0xC8
  int m_pushState = 0;            // +0xF8 push the new state on the stack (0 when going up)
  int m_fieldFC = 0;              // +0xFC credits closed: go up the tree
  UIDialog m_dialog;              // +0x104
  int m_controllers = 0;          // +0x100 controllers present when a game start finished
  int m_field190 = 0;             // +0x190 waiting for player 2 to press start
  ERFont* m_font = nullptr;       // +0x18C
  static MMUTarget* m_MMUSingleton;
};
