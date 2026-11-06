// @class UIObjectBase after ECTRL_CMD
// @class GetVariableCommandTable after UIObjectBase
// @class SetVariableCommandTable after UIObjectBase
// @class GetLocalizableCommandTable after UIObjectBase
// @class UIUpdateTable after UIObjectBase
// @class UIDrawTable after UIObjectBase
// @class UIRouter after UIObjectBase
// @class FlowGotoTarget after UIObjectBase
// @class UIQDTarget after UIObjectBase
// @class UIDB after UIObjectBase
// @class UIDBTarget after UIObjectBase
// The Flash UI's game side (game/uibase.cpp, uicore.cpp, uirouter.cpp, uiflow.cpp, uiqd.cpp, uidb.cpp,
// uiupdate.cpp, uidraw.cpp). A UI object answers the movie's getVariable/setVariable calls
// (AptAuxPCEorGL's setExternVariable/getExternVariable): the UIRouter is the registered handler and
// looks the variable up in the global command tables, where each target installed the names it
// serves ("flowGoto", "qd:", "uidb:", ...). GC: the tables held CBFunctors bound to the target's
// virtuals; here they hold std::function.
#pragma once

#include <functional>
#include <string>
#include <vector>

struct UIObjectBase {
  UIObjectBase() = default;
  virtual ~UIObjectBase();                       // 803A066C
  virtual void SetVariable(char* name, char* value);  // 803A06A0
  virtual char* GetVariable(char* name);         // 803A06A4 (an SimsAptMalloc'd string, or null)
  virtual unsigned short* GetLocalizable(char* name);  // 803A05E8
  virtual void Update();                         // 803A06AC
  virtual void Draw(ERC* rc);                    // 803A06B0

  // The constructors' common part (inlined on GC): fetch the shared tables.
  void AttachTables(const char* name);

  static void pushAptButtonFilter(int, char *);  // 80177A64
  static void popAptButtonFilter(int, unsigned int);  // 80177A98
  static void addCmdToAptButtonFilter(int, unsigned int, ECTRL_CMD);  // 80177ACC
  RET_UNKNOWN removeCmdFromAptButtonFilter(int, unsigned int, ECTRL_CMD);  // 80177B08
  RET_UNKNOWN pushAptButtonFilterALL(int, char *);  // 80177B44
  static bool makeNumberString(int, unsigned short *);  // 80177BA4
  static bool makeMoneyString(int, unsigned short *, bool);  // 80177C30
  void doUIQDStringSubstitution(unsigned short *);  // 80177CA8
  static void UIRouter_SetVariable(char* name, char* value);  // 80177CC8
  static void UIRouter_FlowGoto(char* state);  // 80177D0C
  static void AptViewer_CallFunction(char* function, char* unused, char* target, int argc, char** argv);  // 80177D54

  int m_installed = 0;                                   // +0x00
  GetVariableCommandTable* m_getTable = nullptr;         // +0x04
  SetVariableCommandTable* m_setTable = nullptr;         // +0x08
  GetLocalizableCommandTable* m_locTable = nullptr;      // +0x0C
  UIUpdateTable* m_updateTable = nullptr;                // +0x10
  UIDrawTable* m_drawTable = nullptr;                    // +0x14
  // +0x18..+0x24 UIUserDrawCBTable, UI2D, UI3D, UIAUDIO (not ported)
  // +0x28..+0x7B the CBFunctors bound to the virtuals (the lambdas the tables hold here)
  const char* m_name = "";                               // +0x7C
};

// "<name>" -> GetVariable functor (uicore.cpp); the GC list moved a hit to the front.
struct GetVariableCommandTable {
  using Command = std::function<char*(char*)>;
  struct Record {
    std::string name;
    Command command;
    const void* owner;  // the target that installed it (GC: compared the functors)
  };
  GetVariableCommandTable() = default;  // 80177DA8
  virtual ~GetVariableCommandTable();  // 80177E48
  bool InstallCommand(const char* name, Command command, const void* owner = nullptr);  // 80177FDC
  bool UnInstallCommand(const char* name);  // 80178060
  bool ContainsCommand(const char* name);  // 80178138
  char* ExecuteCommand(const char* name, char* variable);  // 80178198
  static GetVariableCommandTable* Instance();  // 80178BF8
  static void Shutdown();  // 80178C34
  // 80177EE8 AddRecord, 80177F4C RemoveRecord: the record pool (a std::vector here)
  void UnInstallAllCommands(const void* owner);  // 801780D8

  std::vector<Record> m_records;
  static GetVariableCommandTable* s_pInstance;
};

struct SetVariableCommandTable {
  using Command = std::function<void(char*, char*)>;
  struct Record {
    std::string name;
    Command command;
    const void* owner;  // the target that installed it (GC: compared the functors)
  };
  SetVariableCommandTable() = default;  // 8017826C
  virtual ~SetVariableCommandTable();  // 8017830C
  bool InstallCommand(const char* name, Command command, const void* owner = nullptr);  // 801784A0
  bool UnInstallCommand(const char* name);  // 80178524
  bool ContainsCommand(const char* name);  // 801785FC
  void ExecuteCommand(const char* name, char* variable, char* value);  // 8017865C
  static SetVariableCommandTable* Instance();  // 80178C80
  static void Shutdown();  // 80178CBC
  // 801783AC AddRecord, 80178410 RemoveRecord
  void UnInstallAllCommands(const void* owner);  // 8017859C

  std::vector<Record> m_records;
  static SetVariableCommandTable* s_pInstance;
};

struct GetLocalizableCommandTable {
  using Command = std::function<unsigned short*(char*)>;
  struct Record {
    std::string name;
    Command command;
    const void* owner;  // the target that installed it (GC: compared the functors)
  };
  GetLocalizableCommandTable() = default;  // 80178734
  virtual ~GetLocalizableCommandTable();  // 801787D4
  bool InstallCommand(const char* name, Command command, const void* owner = nullptr);  // 80178968
  bool UnInstallCommand(const char* name);  // 801789EC
  bool ContainsCommand(const char* name);  // 80178AC4
  unsigned short* ExecuteCommand(const char* name, char* variable);  // 80178B24
  static GetLocalizableCommandTable* Instance();  // 80178D08
  static void Shutdown();  // 80178D44
  // 80178874 AddRecord, 801788D8 RemoveRecord
  void UnInstallAllCommands(const void* owner);  // 80178A64

  std::vector<Record> m_records;
  static GetLocalizableCommandTable* s_pInstance;
};

// Per-frame callbacks around the Apt update, sorted by priority: above 50 before Flash, the rest
// after (uiupdate.cpp).
struct UIUpdateTable {
  struct Record {
    int priority;
    std::function<void()> update;
    std::string name;
    const void* owner;
  };
  UIUpdateTable() = default;  // 80181888
  virtual ~UIUpdateTable();  // 80181930
  static bool InstallEntry(int priority, std::function<void()> update, const char* name, const void* owner = nullptr);  // 80181B38
  bool UnInstallAllName(const char* name);  // 80181BE8
  void ExecuteUpdatesPreFlash();  // 80181C8C
  void ExecuteUpdatesPostFlash();  // 80181CF0
  static UIUpdateTable* Instance();  // 80181D4C
  static void Shutdown();  // 80181D88
  // 801819D0 AddRecord, 80181A9C RemoveRecord
  bool UnInstallEntry(const void* owner);  // 80181B94

  std::vector<Record> m_records;
  static UIUpdateTable* s_pInstance;
};

struct UIDrawTable {
  struct Record {
    int priority;
    std::function<void(ERC*)> draw;
    std::string name;
    const void* owner;
  };
  UIDrawTable() = default;  // 8017A64C
  virtual ~UIDrawTable();  // 8017A6F4
  static bool InstallEntry(int priority, std::function<void(ERC*)> draw, const char* name, const void* owner = nullptr);  // 8017A8FC
  bool UnInstallAllName(const char* name);  // 8017A9AC
  void ExecuteDrawsPreFlash(ERC* rc);  // 8017AA50
  void ExecuteDrawsPostFlash(ERC* rc);  // 8017AAB4
  static UIDrawTable* Instance();  // 8017AB18
  static void Shutdown();  // 8017AB54
  // 8017A794 AddRecord, 8017A860 RemoveRecord
  bool UnInstallEntry(const void* owner);  // 8017A958

  std::vector<Record> m_records;
  static UIDrawTable* s_pInstance;
};

// The message handler the movie talks to: routes a variable to the table entry for its name, or for
// its "qd:" / "uidb:" / "audio:" prefix.
struct UIRouter : UIObjectBase {
  UIRouter();  // 8017EDA8
  ~UIRouter() override;  // 8017EEA4
  char* GetVariable(char* name) override;  // 8017EF08
  void SetVariable(char* name, char* value) override;  // 8017EFE0

  GetVariableCommandTable* m_routerGet = nullptr;  // +0x84
  SetVariableCommandTable* m_routerSet = nullptr;  // +0x88
};

// "flowGoto": the screen the movie asks the game for (TheSimsStateMachine polls it).
struct FlowGotoTarget : UIObjectBase {
  enum : int {
    kMainMenu = 1, kFreeplay = 2, kCAS = 3, kCAF = 4, kWardrobe = 5, kInLevel = 6, kKeyboard = 7, kInGame = 8,
    kHUDTransition = 9, kMovie = 10, kSlotSelect = 0x17, kCredits = 0x18,
  };
  FlowGotoTarget();  // 8017ABA0
  ~FlowGotoTarget() override;  // 8017B02C
  void SetVariable(char* name, char* value) override;  // 8017B170
  char* GetVariable(char* name) override;  // 8017B87C
  bool IsStateRequestPending();  // 8017B960
  int GetStateRequest();  // 8017B980
  void SetPendingStateCurrent();  // 8017B988

  int m_request = 0;  // +0x84
  int m_current = 0;  // +0x88
};

// "qd:<Table>::<row>": quickdata strings (NewUIStrings, HUDStrings, PDAStrings, HouseData) as UTF-8.
struct UIQDTarget : UIObjectBase {
  UIQDTarget();  // 8017BF44
  ~UIQDTarget() override;  // 8017C384
  char* GetVariable(char* name) override;  // 8017C4C8
  static const unsigned short* GetQuickdataText(char* name);  // 8017C72C
  static void UIQDSubstitutionStrings(unsigned short* text);  // 8017C92C
  static RET_UNKNOWN UIQDRemoveEmbeddedControl(unsigned short *);  // 8017D10C
};

// The UI's small variable store ("uidb:i:<name>", "uidb:f:<name>", "uidb:s:<name>").
struct UIDB {
  enum Type : int { kInt = 'i', kFloat = 'f', kString = 's' };
  struct Record {
    const char* name;
    int type;
    int intValue;
    float floatValue;
    std::vector<unsigned short> text;  // GC: a 0x80-byte VMHeap block
  };
  static void UIDBInit();  // 80178D90
  static void UIDBShutdown();  // 80179A54
  static int UIDBGetInt(char* name);  // 80179AC8
  static double UIDBGetFloat(char* name);  // 80179B18
  static const unsigned short* UIDBGetString(char* name);  // 80179B60
  static bool UIDBSetInt(char* name, int value);  // 80179BB0
  static bool UIDBSetFloat(char* name, float value);  // 80179C10
  static bool UIDBSetString(char* name, const unsigned short* value);  // 80179C68
  static Record* UIDBFindRecord(const char* name, int type);  // 80179D04

  static std::vector<Record> s_UIDBTable;
  static int s_UIDBInitialized;
};

struct UIDBTarget : UIObjectBase {
  UIDBTarget();  // 80179D90
  ~UIDBTarget() override;  // 8017A1F4
  void SetVariable(char* name, char* value) override;  // 8017A338
  char* GetVariable(char* name) override;  // 8017A4A4
};
