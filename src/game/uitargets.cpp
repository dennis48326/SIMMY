// Port of the Flash UI's game side: game/uibase.cpp (UIObjectBase), uicore.cpp (the command
// tables), uiupdate.cpp / uidraw.cpp (the update and draw tables), uirouter.cpp, uiflow.cpp
// (FlowGotoTarget), uiqd.cpp (UIQDTarget) and uidb.cpp (UIDB, UIDBTarget).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// The tables' record pools (AddRecord/RemoveRecord) are std::vectors here; entries are told apart
// by the target that installed them instead of by comparing CBFunctors:
// 80177EE8 GetVariableCommandTable::AddRecord
// 80177F4C GetVariableCommandTable::RemoveRecord
// 801783AC SetVariableCommandTable::AddRecord
// 80178410 SetVariableCommandTable::RemoveRecord
// 80178874 GetLocalizableCommandTable::AddRecord
// 801788D8 GetLocalizableCommandTable::RemoveRecord
// 801819D0 UIUpdateTable::AddRecord
// 80181A9C UIUpdateTable::RemoveRecord
// 80181C58 UIUpdateTable::ContainsEntry
// 8017A794 UIDrawTable::AddRecord
// 8017A860 UIDrawTable::RemoveRecord
// 8017AA1C UIDrawTable::ContainsEntry
// The tables' constructors are defaulted in uitarget.hpp:
// 80177DA8 GetVariableCommandTable::GetVariableCommandTable
// 8017826C SetVariableCommandTable::SetVariableCommandTable
// 80178734 GetLocalizableCommandTable::GetLocalizableCommandTable
// 80181888 UIUpdateTable::UIUpdateTable
// 8017A64C UIDrawTable::UIDrawTable

static AptViewer* TheAptViewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
void* SimsAptMalloc(unsigned int size);
void SimsAptFree(void* p);
int UCS2ToUTF8(const unsigned short* src, int count, char* dst);
int UTF8ToUCS2(const char* src, int count, unsigned short* dst);

namespace {
size_t Len16(const unsigned short* s) {
  size_t n = 0;
  while (s && s[n]) ++n;
  return n;
}
char* MallocCopy(const char* s) {
  const size_t n = std::strlen(s) + 1;
  char* out = static_cast<char*>(SimsAptMalloc(static_cast<unsigned int>(n)));
  std::memcpy(out, s, n);
  return out;
}
// "<prefix>:<a>:<b>:<c>" split at the colons (fields 1..3); ':' separators only.
void SplitColons(const char* name, std::string field[4]) {
  int colons = 0;
  for (const char* p = name; *p; ++p) {
    if (*p == ':') ++colons;
    else if (colons < 4) field[colons] += *p;
  }
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// UIObjectBase

// 803A066C
UIObjectBase::~UIObjectBase() = default;
// 803A06A0
void UIObjectBase::SetVariable(char*, char*) {}
// 803A06A4
char* UIObjectBase::GetVariable(char*) { return nullptr; }
// 803A05E8
unsigned short* UIObjectBase::GetLocalizable(char*) { return nullptr; }
// 803A06AC
void UIObjectBase::Update() {}
// 803A06B0
void UIObjectBase::Draw(ERC*) {}

void UIObjectBase::AttachTables(const char* name) {
  m_name = name;
  m_getTable = GetVariableCommandTable::Instance();
  m_setTable = SetVariableCommandTable::Instance();
  m_locTable = GetLocalizableCommandTable::Instance();
  m_updateTable = UIUpdateTable::Instance();
  m_drawTable = UIDrawTable::Instance();
  m_installed = 1;
}

// 80177CA8
// The quickdata tags (<unlock_aspiration_points>, ...) in a target's text.
void UIObjectBase::doUIQDStringSubstitution(unsigned short* text) { UIQDTarget::UIQDSubstitutionStrings(text); }

// 80177CC8
// Through the router (_globals' UI router, DAT_80475EF8) as if the movie had set it.
void UIObjectBase::UIRouter_SetVariable(char* name, char* value) {
  if (auto* router = static_cast<UIRouter*>(_globals.m_uiRouter)) router->SetVariable(name, value);
}

// 80177D0C
void UIObjectBase::UIRouter_FlowGoto(char* state) {
  if (auto* router = static_cast<UIRouter*>(_globals.m_uiRouter)) router->SetVariable(const_cast<char*>("flowGoto"), state);
}

// ---------------------------------------------------------------------------------------------------
// Command tables

GetVariableCommandTable* GetVariableCommandTable::s_pInstance = nullptr;
SetVariableCommandTable* SetVariableCommandTable::s_pInstance = nullptr;
GetLocalizableCommandTable* GetLocalizableCommandTable::s_pInstance = nullptr;
UIUpdateTable* UIUpdateTable::s_pInstance = nullptr;
UIDrawTable* UIDrawTable::s_pInstance = nullptr;

namespace {
template <class Table>
bool Install(Table& t, const char* name, typename Table::Command command, const void* owner) {
  for (auto& r : t.m_records)
    if (r.name == name) return false;
  t.m_records.push_back({name, std::move(command), owner});
  return true;
}
template <class Table>
bool UnInstall(Table& t, const char* name) {
  for (auto it = t.m_records.begin(); it != t.m_records.end(); ++it)
    if (it->name == name) {
      t.m_records.erase(it);
      return true;
    }
  return false;
}
template <class Table>
void UnInstallOwner(Table& t, const void* owner) {
  t.m_records.erase(std::remove_if(t.m_records.begin(), t.m_records.end(), [owner](const auto& r) { return r.owner == owner; }),
                    t.m_records.end());
}
template <class Table>
typename Table::Record* Find(Table& t, const char* name) {
  for (auto& r : t.m_records)
    if (r.name == name) return &r;
  return nullptr;
}
}  // namespace

// 80177E48
GetVariableCommandTable::~GetVariableCommandTable() = default;
// 80177FDC
bool GetVariableCommandTable::InstallCommand(const char* name, Command command, const void* owner) {
  return Install(*this, name, std::move(command), owner);
}
// 80178060
bool GetVariableCommandTable::UnInstallCommand(const char* name) { return UnInstall(*this, name); }
// 801780D8
void GetVariableCommandTable::UnInstallAllCommands(const void* owner) { UnInstallOwner(*this, owner); }
// 80178138
bool GetVariableCommandTable::ContainsCommand(const char* name) { return Find(*this, name) != nullptr; }
// 80178198
char* GetVariableCommandTable::ExecuteCommand(const char* name, char* variable) {
  Record* r = Find(*this, name);
  char* v = r ? r->command(variable) : nullptr;
  static const bool trace = std::getenv("SIMMY_UI_TRACE") != nullptr;  // PC: command trace on stderr
  if (trace) std::fprintf(stderr, "[ui get %d] %s -> %s\n", _framecount, name, v ? v : "(none)");
  return v;
}
// 80178BF8
GetVariableCommandTable* GetVariableCommandTable::Instance() {
  if (!s_pInstance) s_pInstance = new GetVariableCommandTable;
  return s_pInstance;
}
// 80178C34
void GetVariableCommandTable::Shutdown() {
  delete s_pInstance;
  s_pInstance = nullptr;
}

// 8017830C
SetVariableCommandTable::~SetVariableCommandTable() = default;
// 801784A0
bool SetVariableCommandTable::InstallCommand(const char* name, Command command, const void* owner) {
  return Install(*this, name, std::move(command), owner);
}
// 80178524
bool SetVariableCommandTable::UnInstallCommand(const char* name) { return UnInstall(*this, name); }
// 8017859C
void SetVariableCommandTable::UnInstallAllCommands(const void* owner) { UnInstallOwner(*this, owner); }
// 801785FC
bool SetVariableCommandTable::ContainsCommand(const char* name) { return Find(*this, name) != nullptr; }
void simmy_dev_note_ui_set(const char* name, const char* value);  // PC dev aid (pc_devtools.cpp)

// 8017865C
void SetVariableCommandTable::ExecuteCommand(const char* name, char* variable, char* value) {
  static const bool trace = std::getenv("SIMMY_UI_TRACE") != nullptr;  // PC: command trace on stderr
  if (trace) std::fprintf(stderr, "[ui set %d] %s %s = %s\n", _framecount, name, variable ? variable : "", value ? value : "");
  simmy_dev_note_ui_set(name, value);  // PC dev aid: the pad sequence sees the UI react
  if (Record* r = Find(*this, name)) r->command(variable, value);
}
// 80178C80
SetVariableCommandTable* SetVariableCommandTable::Instance() {
  if (!s_pInstance) s_pInstance = new SetVariableCommandTable;
  return s_pInstance;
}
// 80178CBC
void SetVariableCommandTable::Shutdown() {
  delete s_pInstance;
  s_pInstance = nullptr;
}

// 801787D4
GetLocalizableCommandTable::~GetLocalizableCommandTable() = default;
// 80178968
bool GetLocalizableCommandTable::InstallCommand(const char* name, Command command, const void* owner) {
  return Install(*this, name, std::move(command), owner);
}
// 801789EC
bool GetLocalizableCommandTable::UnInstallCommand(const char* name) { return UnInstall(*this, name); }
// 80178A64
void GetLocalizableCommandTable::UnInstallAllCommands(const void* owner) { UnInstallOwner(*this, owner); }
// 80178AC4
bool GetLocalizableCommandTable::ContainsCommand(const char* name) { return Find(*this, name) != nullptr; }
// 80178B24
unsigned short* GetLocalizableCommandTable::ExecuteCommand(const char* name, char* variable) {
  Record* r = Find(*this, name);
  return r ? r->command(variable) : nullptr;
}
// 80178D08
GetLocalizableCommandTable* GetLocalizableCommandTable::Instance() {
  if (!s_pInstance) s_pInstance = new GetLocalizableCommandTable;
  return s_pInstance;
}
// 80178D44
void GetLocalizableCommandTable::Shutdown() {
  delete s_pInstance;
  s_pInstance = nullptr;
}

// ---------------------------------------------------------------------------------------------------
// UIUpdateTable / UIDrawTable

// 80181930
UIUpdateTable::~UIUpdateTable() = default;
// 80181B38
// Inserted before the first entry of higher priority.
bool UIUpdateTable::InstallEntry(int priority, std::function<void()> update, const char* name, const void* owner) {
  UIUpdateTable* t = Instance();
  auto it = std::find_if(t->m_records.begin(), t->m_records.end(), [priority](const Record& r) { return priority < r.priority; });
  t->m_records.insert(it, Record{priority, std::move(update), name ? name : "", owner});
  return true;
}
// 80181B94
bool UIUpdateTable::UnInstallEntry(const void* owner) {
  const size_t n = m_records.size();
  m_records.erase(std::remove_if(m_records.begin(), m_records.end(), [owner](const Record& r) { return r.owner == owner; }),
                  m_records.end());
  return m_records.size() != n;
}
// 80181BE8
bool UIUpdateTable::UnInstallAllName(const char* name) {
  const size_t n = m_records.size();
  m_records.erase(std::remove_if(m_records.begin(), m_records.end(), [name](const Record& r) { return r.name == name; }),
                  m_records.end());
  return m_records.size() != n;
}
// 80181C8C
void UIUpdateTable::ExecuteUpdatesPreFlash() {
  // An update may uninstall others (a target deleting the one it runs): those are skipped.
  auto installed = [this](const Record& r) {
    for (const Record& x : m_records)
      if (x.owner == r.owner && x.name == r.name) return true;
    return false;
  };
  std::vector<Record> records = m_records;
  for (Record& r : records)
    if (r.priority > 50 && r.update && installed(r)) r.update();
}
// 80181CF0
void UIUpdateTable::ExecuteUpdatesPostFlash() {
  // An update may uninstall others (a target deleting the one it runs): those are skipped.
  auto installed = [this](const Record& r) {
    for (const Record& x : m_records)
      if (x.owner == r.owner && x.name == r.name) return true;
    return false;
  };
  std::vector<Record> records = m_records;
  for (Record& r : records)
    if (r.priority <= 50 && r.update && installed(r)) r.update();
}
// 80181D4C
UIUpdateTable* UIUpdateTable::Instance() {
  if (!s_pInstance) s_pInstance = new UIUpdateTable;
  return s_pInstance;
}
// 80181D88
void UIUpdateTable::Shutdown() {
  delete s_pInstance;
  s_pInstance = nullptr;
}

// 8017A6F4
UIDrawTable::~UIDrawTable() = default;
// 8017A8FC
bool UIDrawTable::InstallEntry(int priority, std::function<void(ERC*)> draw, const char* name, const void* owner) {
  UIDrawTable* t = Instance();
  auto it = std::find_if(t->m_records.begin(), t->m_records.end(), [priority](const Record& r) { return priority < r.priority; });
  t->m_records.insert(it, Record{priority, std::move(draw), name ? name : "", owner});
  return true;
}
// 8017A958
bool UIDrawTable::UnInstallEntry(const void* owner) {
  const size_t n = m_records.size();
  m_records.erase(std::remove_if(m_records.begin(), m_records.end(), [owner](const Record& r) { return r.owner == owner; }),
                  m_records.end());
  return m_records.size() != n;
}
// 8017A9AC
bool UIDrawTable::UnInstallAllName(const char* name) {
  const size_t n = m_records.size();
  m_records.erase(std::remove_if(m_records.begin(), m_records.end(), [name](const Record& r) { return r.name == name; }),
                  m_records.end());
  return m_records.size() != n;
}
// 8017AA50
void UIDrawTable::ExecuteDrawsPreFlash(ERC* rc) {
  std::vector<Record> records = m_records;
  for (Record& r : records)
    if (r.priority > 50 && r.draw) r.draw(rc);
}
// 8017AAB4
void UIDrawTable::ExecuteDrawsPostFlash(ERC* rc) {
  std::vector<Record> records = m_records;
  for (Record& r : records)
    if (r.priority <= 50 && r.draw) r.draw(rc);
}
// 8017AB18
UIDrawTable* UIDrawTable::Instance() {
  if (!s_pInstance) s_pInstance = new UIDrawTable;
  return s_pInstance;
}
// 8017AB54
void UIDrawTable::Shutdown() {
  delete s_pInstance;
  s_pInstance = nullptr;
}

// ---------------------------------------------------------------------------------------------------
// UIRouter

// 8017EDA8
UIRouter::UIRouter() {
  m_routerGet = GetVariableCommandTable::Instance();
  m_routerSet = SetVariableCommandTable::Instance();
  if (TheAptViewer()) TheAptViewer()->NewSetMessageHandler(this);
}

// 8017EEA4
UIRouter::~UIRouter() {
  if (TheAptViewer()) TheAptViewer()->NewRemoveMessageHandler(this);
}

// 8017EF08
// "qd:..." and "uidb:..." go to their prefix's entry, anything else to its full name.
char* UIRouter::GetVariable(char* name) {
  const size_t n = std::strlen(name);
  if (n == 0) return nullptr;
  std::string key(name, std::min<size_t>(n, 0x3f));
  if (std::strncmp("qd:", name, 3) == 0) key = "qd:";
  else if (std::strncmp("uidb:", name, 5) == 0) key = "uidb:";
  return m_routerGet->ExecuteCommand(key.c_str(), name);
}

// 8017EFE0
void UIRouter::SetVariable(char* name, char* value) {
  const size_t n = std::strlen(name);
  if (n == 0) return;
  std::string key(name, std::min<size_t>(n, 0x3f));
  if (std::strncmp("uidb:", name, 4) == 0) key = "uidb:";
  else if (std::strncmp("audio:", name, 6) == 0) key = "audio:";
  m_routerSet->ExecuteCommand(key.c_str(), name, value);
}

// ---------------------------------------------------------------------------------------------------
// FlowGotoTarget

// 8017ABA0
FlowGotoTarget::FlowGotoTarget() {
  AttachTables("FlowGotoTarget");
  m_getTable->InstallCommand("flowGoto", [this](char* v) { return GetVariable(v); }, this);
  if (m_installed) m_setTable->InstallCommand("flowGoto", [this](char* n, char* v) { SetVariable(n, v); }, this);
  if (m_installed) m_getTable->InstallCommand("flowGoto_exists", [this](char* v) { return GetVariable(v); }, this);
}

// 8017B02C
FlowGotoTarget::~FlowGotoTarget() {
  if (!m_installed) return;
  m_getTable->UnInstallAllCommands(this);
  m_setTable->UnInstallAllCommands(this);
  m_locTable->UnInstallAllCommands(this);
  m_updateTable->UnInstallEntry(this);
  m_drawTable->UnInstallEntry(this);
}

// 8017B170
// The screen names the movie uses for "flowGoto"; unknown names (and "testbox", "LoadingBar",
// "switch") leave the request as it is.
void FlowGotoTarget::SetVariable(char* name, char* value) {
  if (std::strcmp("flowGoto", name) != 0) return;
  struct Entry {
    const char* name;
    int state;
    bool prefix;
  };
  static const Entry kStates[] = {
      {"MainMenu", 1, false}, {"Freeplay", 2, false}, {"HUD_transition", 9, false}, {"ingame", 8, false},
      {"HUD_", 8, true}, {"Movie", 10, false}, {"CAS_", 3, true}, {"CAF_", 4, true}, {"Wardrobe", 5, false},
      {"INLEVEL", 6, false}, {"KEYBOARD", 7, false}, {"SlotSelect", 0x17, false},
      {"MainMenuBootCheckAccept", 0x30, false}, {"MainMenuBootCheckCancel", 0x31, false},
      {"CreateASimSaveAccept", 0x2c, false}, {"CreateASimSaveCancel", 0x2d, false}, {"CASBackoutP1", 0x32, false},
      {"CASBackoutP2", 0x33, false}, {"MainMenuLoadAccept", 0x2e, false}, {"MainMenuLoadCancel", 0x2f, false},
  };
  for (const Entry& e : kStates)
    if (e.prefix ? std::strncmp(value, e.name, std::strlen(e.name)) == 0 : std::strcmp(value, e.name) == 0) {
      m_request = e.state;
      return;
    }
  if (std::strncmp(value, "PDA_", 4) == 0) {
    static const Entry kPda[] = {
        {"PDA_Customize", 0xd, false}, {"PDA_Customize_Eyetoy", 0xe, false}, {"PDA_Customize_Skins", 0xf, false},
        {"PDA_Customize_Ringtones", 0x10, false}, {"PDA_Goals", 0x11, false}, {"PDA_Inventory", 0x12, false},
        {"PDA_Messaging", 0x14, false}, {"PDA_Phone", 0x13, false}, {"PDA_Relationships", 0xc, false},
    };
    m_request = 0xb;  // any other PDA screen
    for (const Entry& e : kPda)
      if (std::strcmp(value, e.name) == 0) m_request = e.state;
    return;
  }
  static const Entry kOthers[] = {
      {"Pause", 0x15, false}, {"Options", 0x16, false}, {"Credits", 0x18, false}, {"testbox", 0, false},
      {"LoadingBar", 0, false}, {"switch", 0, false}, {"MixIngredients", 0x19, false},
      {"PAUSE_SUBMENU_WAF", 0x1a, false}, {"PAUSE_WANTS", 0x1b, false}, {"PAUSE_FEARS", 0x1c, false},
      {"PAUSE_PROGRESS", 0x1d, false}, {"PAUSE_SUBMENU_CAREER", 0x1e, false}, {"PAUSE_CAREER", 0x1f, false},
      {"PAUSE_SKILLS", 0x20, false}, {"PAUSE_PERSONALITY", 0x21, false}, {"PAUSE_STATS", 0x22, false},
      {"PAUSE_RELATIONSHIPS", 0x23, false}, {"PAUSE_DEATH_TOMBSTONE_PLAYER1", 0x2a, false},
      {"PAUSE_DEATH_TOMBSTONE_PLAYER2", 0x2b, false}, {"PAUSE_BUY", 0x24, false}, {"PAUSE_BUILD", 0x25, false},
      {"PAUSE_EYETOY", 0x26, false}, {"PAUSE_SUBMENU_GAME", 0x27, false}, {"PAUSE_OPTIONS", 0x28, false},
      {"PAUSE_SAVE", 0x29, false},
  };
  for (const Entry& e : kOthers)
    if (std::strcmp(value, e.name) == 0) {
      if (e.state) m_request = e.state;
      return;
    }
}

// 8017B87C
char* FlowGotoTarget::GetVariable(char* name) {
  char* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  if (std::strcmp("flowGoto_exists", name) == 0) std::strcpy(out, "loaded");
  else if (std::strcmp("flowGoto_request", name) == 0) std::snprintf(out, 0x20, "%d", m_request);
  else if (std::strcmp("flowGoto_current", name) == 0) std::snprintf(out, 0x20, "%d", m_current);
  return out;
}

// 8017B960
bool FlowGotoTarget::IsStateRequestPending() { return m_request != m_current; }
// 8017B980
int FlowGotoTarget::GetStateRequest() { return m_request; }
// 8017B988
void FlowGotoTarget::SetPendingStateCurrent() { m_current = m_request; }

// ---------------------------------------------------------------------------------------------------
// UIQDTarget

// 8017BF44
UIQDTarget::UIQDTarget() {
  AttachTables("UIQDTarget");
  m_getTable->InstallCommand("qd:", [this](char* v) { return GetVariable(v); }, this);
}

// 8017C384
UIQDTarget::~UIQDTarget() {
  if (!m_installed) return;
  m_getTable->UnInstallAllCommands(this);
  m_setTable->UnInstallAllCommands(this);
  m_locTable->UnInstallAllCommands(this);
  m_updateTable->UnInstallEntry(this);
  m_drawTable->UnInstallEntry(this);
}

namespace {
// "qd:<table>::<row>" (fields 1 and 3).
const unsigned short* QuickdataText(const std::string& table, const std::string& row) {
  const unsigned short* const* handle = nullptr;
  if (table == "HUDStrings") handle = _globals.GetHUDUIString(row.c_str());
  else if (table == "NewUIStrings") handle = _globals.GetNewUIString(row.c_str());
  else if (table == "PDAStrings") handle = _globals.GetPDAString(row.c_str());
  else if (table == "HouseData") handle = _globals.HouseNameText(row.c_str());
  else return nullptr;
  return handle ? *handle : nullptr;
}
}  // namespace

// 8017C4C8
// The string with its <tokens> substituted, as UTF-8.
char* UIQDTarget::GetVariable(char* name) {
  if (!name || std::strlen(name) < 7) return nullptr;
  std::string f[4];
  SplitColons(name, f);
  std::vector<unsigned short> text(0x1000, 0);
  if (const unsigned short* t = QuickdataText(f[1], f[3])) {
    const size_t n = std::min<size_t>(Len16(t), text.size() - 1);
    std::copy(t, t + n, text.begin());
  }
  UIQDSubstitutionStrings(text.data());
  char* out = static_cast<char*>(SimsAptMalloc(0x2000));
  UCS2ToUTF8(text.data(), static_cast<int>(Len16(text.data())) + 1, out);
  return out;
}

// 8017C72C
const unsigned short* UIQDTarget::GetQuickdataText(char* name) {
  if (!name || std::strlen(name) < 7) return nullptr;
  std::string f[4];
  SplitColons(name, f);
  return QuickdataText(f[1], f[3]);
}

// 8017C92C
// Replaces the tokens in a string: <money>/<money2> (simoleons), <#> (memory card slot A/B),
// <size> (save size), player and family names (<p1_first_name>, <first_name>, <kb_first_name>...),
// motive and object values, the clock (<hour>, <hours24>, <minutes>, <seconds>, <meridian>) and
// <unlock_aspiration_points>. Unknown tags stay as they are.
void UIQDTarget::UIQDSubstitutionStrings(unsigned short* text) {
  bool hasTag = false;
  for (unsigned short* p = text; *p; ++p)
    if (*p == '<') hasTag = true;
  if (!hasTag) return;
  const int kbMode = K2YTarget::m_Mode;  // the keyboard screen's name being typed
  std::vector<unsigned short> out;
  bool changed = false;
  auto starts = [](const unsigned short* p, const char* tag) {
    for (size_t i = 0; tag[i]; ++i)
      if (p[i] != static_cast<unsigned char>(tag[i])) return false;
    return true;
  };
  auto append = [&](const unsigned short* t) {
    for (size_t i = 0; t && t[i]; ++i) out.push_back(t[i]);
    changed = true;
  };
  auto appendDb = [&](const char* name) { append(UIDB::UIDBGetString(const_cast<char*>(name))); };
  auto appendNumber = [&](const char* format, int v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, format, v);
    for (size_t i = 0; buf[i]; ++i) out.push_back(static_cast<unsigned char>(buf[i]));
    changed = true;
  };
  static const struct {
    const char* tag;
    const char* var;
  } kStrings[] = {{"<money2>", "simoleonsP2"}, {"<money>", "simoleons"}, {"<p1_first_name>", "Player1FirstName"},
                  {"<p2_first_name>", "Player2FirstName"}, {"<p1_last_name>", "Player1LastName"},
                  {"<p2_last_name>", "Player2LastName"}, {"<motive_name>", "Motive_Name"},
                  {"<motive_name2>", "Motive_NameP2"}, {"<motive_value>", "Motive_Value"},
                  {"<motive_value2>", "Motive_ValueP2"}, {"<object_value>", "Object_Value"},
                  {"<object_value2>", "Object_ValueP2"}, {"<meridian>", "meridian"},
                  {"<unlock_aspiration_points>", "unlock_aspiration_points"}};
  size_t i = 0;
  const size_t n = [&] {
    size_t k = 0;
    while (text[k]) ++k;
    return k;
  }();
  while (i < n) {
    const unsigned short* p = text + i;
    if (*p != '<') {
      out.push_back(*p);
      ++i;
      continue;
    }
    bool matched = false;
    for (const auto& e : kStrings)
      if (starts(p, e.tag)) {
        appendDb(e.var);
        i += std::strlen(e.tag);
        matched = true;
        break;
      }
    if (matched) continue;
    if (starts(p, "<#>")) {
      out.push_back(UIDB::UIDBGetInt(const_cast<char*>("SelectedMemoryCard")) == 0 ? 'A' : 'B');
      changed = true;
      i += 3;
    } else if (starts(p, "<size>")) {
      appendNumber("%d", UIDB::UIDBGetInt(const_cast<char*>("MemoryCardRecordSize")));
      i += 6;
    } else if (starts(p, "<first_name>")) {
      appendDb(UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber")) == 0 ? "Player1FirstName" : "Player2FirstName");
      i += 12;
    } else if (starts(p, "<last_name>")) {
      appendDb(UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber")) == 0 ? "Player1LastName" : "Player2LastName");
      i += 11;
    } else if (starts(p, "<kb_first_name>") || starts(p, "<kb_last_name>")) {
      const bool first = starts(p, "<kb_first_name>");
      i += first ? 15 : 14;
      const char* var = nullptr;
      if (kbMode == 2 || kbMode == 3) var = first ? "Player1FirstName" : "Player1LastName";
      else if (kbMode == 4 || kbMode == 5) var = first ? "Player2FirstName" : "Player2LastName";
      else if (kbMode == 6) var = "FamilyName";
      else if (kbMode == 7 || kbMode == 8) var = first ? "FamilyFirstName" : "FamilyLastName";
      if (var) {
        appendDb(var);
      } else {
        out.push_back(' ');
        changed = true;
      }
    } else if (starts(p, "<hour>")) {
      appendNumber("%d", UIDB::UIDBGetInt(const_cast<char*>("hours")));
      i += 6;
    } else if (starts(p, "<hours24>")) {
      appendNumber("%02d", UIDB::UIDBGetInt(const_cast<char*>("hours24")));
      i += 9;
    } else if (starts(p, "<minutes>")) {
      appendNumber("%02d", UIDB::UIDBGetInt(const_cast<char*>("mins")));
      i += 9;
    } else if (starts(p, "<seconds>")) {
      appendNumber("%02d", UIDB::UIDBGetInt(const_cast<char*>("secs")));
      i += 9;
    } else {
      out.push_back(*p);
      ++i;
    }
  }
  if (!changed) return;
  const size_t m = std::min<size_t>(out.size(), 0x7ff);
  for (size_t k = 0; k < m; ++k) text[k] = out[k];
  text[m] = 0;
}

// ---------------------------------------------------------------------------------------------------
// UIDB

std::vector<UIDB::Record> UIDB::s_UIDBTable;
int UIDB::s_UIDBInitialized = 0;

// 80178D90
// The variables the UI movies read and write, with their start values.
void UIDB::UIDBInit() {
  if (s_UIDBInitialized) return;
  auto i = [](const char* n, int v) { s_UIDBTable.push_back(Record{n, kInt, v, 0.0f, {}}); };
  auto s = [](const char* n, const unsigned short* v) {
    Record r{n, kString, 0, 0.0f, std::vector<unsigned short>(0x40, 0)};  // 0x80 bytes
    const size_t len = std::min<size_t>(Len16(v), 0x3f);
    std::copy(v, v + len, r.text.begin());
    s_UIDBTable.push_back(std::move(r));
  };
  auto lit = [&](const char* n, const char* ascii) {
    unsigned short buf[0x40] = {};
    for (int k = 0; ascii[k] && k < 0x3f; ++k) buf[k] = static_cast<unsigned char>(ascii[k]);
    s(n, buf);
  };
  auto qd = [&](const char* n, const char* row) { s(n, EGlobal::NewUIText(row)); };
  s_UIDBTable.clear();
  i("hours", 12);
  i("mins", 34);
  i("secs", 56);
  lit("meridian", "PM");
  i("hours24", 12);
  lit("simoleons", "");
  lit("simoleonsP2", "");
  i("SelectedMemoryCard", 0);
  i("MemoryCardRecordSize", 0x390);
  qd("Player1FirstName", "default_player1_first_name");
  qd("Player1LastName", "default_player1_last_name");
  qd("Player2FirstName", "default_player2_first_name");
  qd("Player2LastName", "default_player2_last_name");
  qd("FamilyName", "default_player1_last_name");
  qd("FamilyFirstName", "default_player1_first_name");
  qd("FamilyLastName", "default_player1_last_name");
  i("CAS_mode", 0);
  i("CAS_gender", 0);
  i("CAS_lotnumber", 0);
  i("CAS_neighborID", -1);
  lit("CAS_cfcModListStr", "0000000");
  lit("Motive_Name", "Motive_Name");
  lit("Motive_NameP2", "Motive_NameP2");
  lit("Motive_Value", "Motive_Value");
  lit("Motive_ValueP2", "Motive_ValueP2");
  lit("Object_Value", "OV1");
  lit("Object_ValueP2", "OV2");
  static const char* const kCounts[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"};
  static const char* const kItems[] = {"Item_Count00", "Item_Count01", "Item_Count02", "Item_Count03",
                                       "Item_Count04", "Item_Count05", "Item_Count06", "Item_Count07",
                                       "Item_Count08", "Item_Count09", "Item_Count10", "Item_Count11"};
  static const char* const kItemsP2[] = {"Item_Count00P2", "Item_Count01P2", "Item_Count02P2", "Item_Count03P2",
                                         "Item_Count04P2", "Item_Count05P2", "Item_Count06P2", "Item_Count07P2",
                                         "Item_Count08P2", "Item_Count09P2", "Item_Count10P2", "Item_Count11P2"};
  for (int k = 0; k < 12; ++k) lit(kItems[k], kCounts[k]);
  for (int k = 0; k < 12; ++k) lit(kItemsP2[k], kCounts[k]);
  for (const char* n : {"PDA_Mode", "PDA_ModeP2", "PDA_skin", "PDA_skinP2", "PDA_autolaunch", "PDA_autolaunchP2",
                        "PlayerNumber", "TwoPlayerGame"})
    i(n, 0);
  i("P1RingtoneSample", 0x457536c3);
  i("P2RingtoneSample", 0x457536c3);
  i("INV_Mode", 0);
  i("INV_ModeP2", 0);
  i("INV_LotNumber", 0);
  i("MEM_source", -1);
  i("MEM_destination", -1);
  i("MEM_result", 0);
  i("DisableAnalogNavigation", 0);
  i("ButtonHelpIndex", 0);
  i("ButtonHelpIndexP2", 0);
  i("ScreenWidth", static_cast<int>(_pGfx->GetActualScreenXSize()));
  i("ScreenHeight", static_cast<int>(_pGfx->GetActualScreenYSize()));
  lit("INV_BLD_item_count", "0");
  i("ING_ModeP1", 0);
  i("ING_ModeP2", 0);
  lit("unlock_aspiration_points", "");
  s_UIDBInitialized = 1;
}

// 80179A54
void UIDB::UIDBShutdown() {
  s_UIDBTable.clear();
  s_UIDBInitialized = 0;
}

// 80179D04
UIDB::Record* UIDB::UIDBFindRecord(const char* name, int type) {
  for (Record& r : s_UIDBTable)
    if (r.type == type && std::strcmp(r.name, name) == 0) return &r;
  return nullptr;
}

// 80179AC8
int UIDB::UIDBGetInt(char* name) {
  if (!s_UIDBInitialized) return 0;
  Record* r = UIDBFindRecord(name, kInt);
  return r ? r->intValue : 0;
}
// 80179B18
double UIDB::UIDBGetFloat(char* name) {
  if (!s_UIDBInitialized) return 0.0;
  Record* r = UIDBFindRecord(name, kFloat);
  return r ? r->floatValue : 0.0;
}
// 80179B60
const unsigned short* UIDB::UIDBGetString(char* name) {
  if (!s_UIDBInitialized) return nullptr;
  Record* r = UIDBFindRecord(name, kString);
  return r ? r->text.data() : nullptr;
}
// 80179BB0
bool UIDB::UIDBSetInt(char* name, int value) {
  if (!s_UIDBInitialized) return false;
  Record* r = UIDBFindRecord(name, kInt);
  if (!r) return false;
  r->intValue = value;
  return true;
}
// 80179C10
bool UIDB::UIDBSetFloat(char* name, float value) {
  if (!s_UIDBInitialized) return false;
  if (Record* r = UIDBFindRecord(name, kFloat)) r->floatValue = value;
  return true;
}
// 80179C68
// At most 63 characters are kept.
bool UIDB::UIDBSetString(char* name, const unsigned short* value) {
  if (!s_UIDBInitialized) return false;
  Record* r = UIDBFindRecord(name, kString);
  if (!r) return false;
  const size_t n = std::min<size_t>(Len16(value), 0x3f);
  std::fill(r->text.begin(), r->text.end(), static_cast<unsigned short>(0));
  std::copy(value, value + n, r->text.begin());
  return true;
}

// 80179D90
UIDBTarget::UIDBTarget() {
  AttachTables("UIDBTarget");
  m_getTable->InstallCommand("uidb:", [this](char* v) { return GetVariable(v); }, this);
  if (m_installed) m_setTable->InstallCommand("uidb:", [this](char* n, char* v) { SetVariable(n, v); }, this);
}

// 8017A1F4
UIDBTarget::~UIDBTarget() {
  if (!m_installed) return;
  m_getTable->UnInstallAllCommands(this);
  m_setTable->UnInstallAllCommands(this);
  m_locTable->UnInstallAllCommands(this);
  m_updateTable->UnInstallEntry(this);
  m_drawTable->UnInstallEntry(this);
}

// 8017A338
// "uidb:<type>:<name>" = value.
void UIDBTarget::SetVariable(char* name, char* value) {
  if (!name || !*name || !value) return;
  std::string f[4];
  int colons = 0;
  for (const char* p = name; *p; ++p)
    if (*p == ':') ++colons;
  SplitColons(name, f);
  if (colons != 2 || f[1].empty() || f[2].empty()) return;
  if (f[1][0] == 'i') {
    int v = 0;
    std::sscanf(value, "%d", &v);
    UIDB::UIDBSetInt(const_cast<char*>(f[2].c_str()), v);
  } else if (f[1][0] == 'f') {
    UIDB::UIDBSetFloat(const_cast<char*>(f[2].c_str()), static_cast<float>(std::atof(value)));
  } else if (f[1][0] == 's') {
    std::vector<unsigned short> text(std::strlen(value) + 1, 0);
    UTF8ToUCS2(value, static_cast<int>(text.size()), text.data());
    UIDB::UIDBSetString(const_cast<char*>(f[2].c_str()), text.data());
  }
}

// 8017A4A4
char* UIDBTarget::GetVariable(char* name) {
  if (!name || !*name) return nullptr;
  std::string f[4];
  SplitColons(name, f);
  char buf[0x20] = {};
  if (!f[1].empty() && f[1][0] == 'i') {
    std::snprintf(buf, sizeof buf, "%d", UIDB::UIDBGetInt(const_cast<char*>(f[2].c_str())));
  } else if (!f[1].empty() && f[1][0] == 'f') {
    std::snprintf(buf, sizeof buf, "%f", UIDB::UIDBGetFloat(const_cast<char*>(f[2].c_str())));
  } else if (!f[1].empty() && f[1][0] == 's') {
    const unsigned short* t = UIDB::UIDBGetString(const_cast<char*>(f[2].c_str()));
    if (!t) return nullptr;
    char* out = static_cast<char*>(SimsAptMalloc(0x2000));
    UCS2ToUTF8(t, static_cast<int>(Len16(t)) + 1, out);
    return out;
  } else {
    return nullptr;
  }
  return MallocCopy(buf);
}
