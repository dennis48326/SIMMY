// Port of the pause menu's buy/build catalogue, PCTTarget (game/targets_s2c.cpp), and the helpers
// before it. See pcttarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/classes/placement.hpp"
#include "simmy/classes/buildtools.hpp"
#include "simmy/platform.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

PCTTarget* PCTTarget::s_placing = nullptr;
int g_unlockAllCheat = 0;     // DAT_8047618C
int g_freeBuildBuyCheat = 0;  // DAT_80476188
int g_catalogFileNames = 0;   // DAT_80476190

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }

unsigned int PushFilterAll(int player, const char* name) {  // UIObjectBase::pushAptButtonFilterALL
  AptViewer* v = Viewer();
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}
void InstallShader(PCTTarget* t, const char* name, unsigned int id) {
  if (t->m_installed) UI2D::Instance()->InstallEntry(const_cast<char*>(name), id, const_cast<char*>(t->m_name), 0);
}

constexpr unsigned int kBlank = 0x9f93ff74;       // get_blank_shader_id
constexpr unsigned int kNoThumbnail = 0xd59c7bb5;  // a cell without a thumbnail
constexpr unsigned int kLockedOverlay = 0x38424d2c;
constexpr unsigned int kNewOverlay = 0x90d567c2;
constexpr unsigned int kMoneyIcon = 0x917800cf;

// sPCT_TabInfo_Buy (803F783C) and sPCT_TabInfo_Build (803F7968).
const PCTTarget__PCTTabInfo kTabsBuy[9] = {
    {1, 0, 0x001, 0x63b0d282, 0x35df573a, "cat_tab_seating"},     {1, 0, 0x004, 0xb65be7b5, 0x81125dbc, "cat_tab_appliances"},
    {1, 0, 0x002, 0x24534c3a, 0x2853ec83, "cat_tab_surfaces"},    {1, 0, 0x010, 0xd443b4e3, 0x6bd9991d, "cat_tab_plumbing"},
    {1, 0, 0x008, 0x26786332, 0x98e07c14, "cat_tab_electronics"}, {1, 0, 0x080, 0x5ee39b83, 0xe7c4c932, "cat_tab_lighting"},
    {1, 0, 0x020, 0xbbe0332c, 0xca37b4d1, "cat_tab_decorative"},  {1, 0, 0x100, 0xa1284f43, 0x9f8944ae, "cat_tab_skill"},
    {1, 0, 0x040, 0x65665e43, 0x5d340bed, "cat_tab_misc"},
};
const PCTTarget__PCTTabInfo kTabsBuild[7] = {
    {2, 0, 0, 0xb0d6c6f9, 0xe704c360, "cat_tab_walls"},     {3, 0, 0, 0x53d45d76, 0x71d6adc3, "cat_tab_wallpaper"},
    {4, 0, 0, 0xdd4346c4, 0x70514a4e, "cat_tab_floors"},    {1, 1, 0, 0xa14edd1e, 0x1c4bd5b8, "cat_tab_doors"},
    {1, 2, 0, 0xb723f773, 0x002c5c10, "cat_tab_windows"},   {1, 5, 0, 0x0773cdfc, 0xa4878f90, "cat_tab_fire"},
    {1, 4, 0, 0x8df49a8e, 0x75d4ee34, "cat_tab_landscaping"},
};
// s_RatingIconIDs (803F4270): per catalogue rating kind.
const unsigned int kRatingIconIds[14] = {0x80e49746, 0x73fa9736, 0x37f1d2c4, 0xdc5505e4, 0x43427f9b, 0xc9c631d1, 0xfca321fc,
                                         0xbe9f08ee, 0x9d7b45aa, 0x47869e5d, 0xed7e661f, 0xca8dfc9b, 0xe899bc58, 0xee2fdfe6};

std::string Indexed(const char* prefix, int i) { return prefix + std::to_string(i); }

void CopySlot(unsigned short* out, const unsigned short* const* slot) {
  const unsigned short* s = slot ? *slot : nullptr;
  unsigned short* d = out;
  while (s && *s) *d++ = *s++;
  *d = 0;
}
void CopyText(unsigned short* out, const unsigned short* s) {
  unsigned short* d = out;
  while (s && *s) *d++ = *s++;
  *d = 0;
}
std::size_t Len(const unsigned short* s) {
  std::size_t n = 0;
  while (s && s[n]) ++n;
  return n;
}
int Compare(const unsigned short* a, const unsigned short* b) {
  if (!a || !b) return a ? 1 : b ? -1 : 0;
  while (*a && *a == *b) ++a, ++b;
  return static_cast<int>(*a) - static_cast<int>(*b);
}
const unsigned short* CatalogNameText(ObjSelector* s) {
  const auto* slot = reinterpret_cast<const unsigned short* const*>(s->GetCatalogName());
  return slot ? *slot : nullptr;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// The catalogue helpers (game/targets_s2c.cpp)

// 801D1A10
// The selector of the object's group that has the ResData (the master, else its parts in turn).
RET_UNKNOWN FindSelectorContainingResData(ObjSelector* selector) {
  ObjectFolderImpl* folder = selector->m_folder ? selector->m_folder : Globs_pObjectFolder;
  auto* s = reinterpret_cast<ObjSelector*>(selector->GetMasterSelector());
  for (int n = 0; s; ++n) {
    if (s->m_def && objdef::ResData(s->m_def)) return reinterpret_cast<RET_UNKNOWN>(s);
    s = folder ? reinterpret_cast<ObjSelector*>(folder->GetNthSubSelector(s, n)) : nullptr;
  }
  return 0;
}

// 801D1A90
RET_UNKNOWN GetResDataFromSelector(ObjSelector* selector) {
  auto* s = reinterpret_cast<ObjSelector*>(FindSelectorContainingResData(selector));
  return s && s->m_def ? reinterpret_cast<RET_UNKNOWN>(objdef::ResData(s->m_def)) : 0;
}

// 801D1AFC
// What the catalogue's 3D preview shows: the ResData's character (+0x20), its first ObjAnimDef's model
// (+0) and animation (+8).
bool GetObjSelector3dDisplayInfo(ObjSelector* selector, ObjectDefinition& def) {
  if (!selector || !selector->m_def) return false;
  const unsigned char* res = objdef::ResData(selector->m_def);
  if (!res) return false;
  const unsigned char* defs = objdef::AnimDefs(res);
  if (!objdef::AnimDefCount(defs)) return false;
  def.m_model = ERQuickdata::U32(defs, 0);
  def.m_character = ERQuickdata::U32(res, 0x20);
  def.m_anim = ERQuickdata::U32(defs, 8);
  def.m_field0C = 0;
  return def.m_model != 0;
}

// 801D1AD0
// Ratings 7..13 are skill flags, not motive values.
RET_UNKNOWN IsSkillCatalogRating(int kind) { return kind >= 0 && kind < 7 ? 0 : 1; }

// 801D1DDC
bool play_sound(unsigned int id) {
  UIAUDIO* audio = UIAUDIO::Instance();
  if (audio) audio->PlaySound(id);
  return audio != nullptr;
}

// 801D4B9C
// An object for the tab: not a part of a multi-tile group; a build sub-sort equal to the tab's, or
// (without one) a function flag of the tab's (objects without flags only in "misc" (0x40) with the
// unlock-all cheat).
RET_UNKNOWN filter_object(PCTTarget__PCTTabInfo* tab, ObjSelector* selector) {
  if (!selector || selector->GetIsMultiTileSubObject() || !selector->m_def) return 0;
  const ObjDefinition* def = selector->m_def;
  const short subsort = ERQuickdata::S16(def, 0x8a);
  if (subsort != 0) return subsort == tab->subsort ? 1 : 0;
  const unsigned short functions = ERQuickdata::U16(def, 0x50);
  if (functions == 0 && (!g_unlockAllCheat || tab->functions != 0x40)) return 0;
  return (tab->functions & functions) != 0 ? 1 : 0;
}

namespace {
// The memory meter (EMemoryMeterWin, GameData +0x04: _gd.m_memoryMeter) at its GC offsets.
float MeterF(int off) {
  float v;
  std::memcpy(&v, _gd.m_memoryMeter + off, 4);
  return v;
}
void SetMeterF(int off, float v) { std::memcpy(_gd.m_memoryMeter + off, &v, 4); }
void SetMeterI(int off, int v) { std::memcpy(_gd.m_memoryMeter + off, &v, 4); }
}  // namespace

// 80057A1C (game/memorymeterwin.cpp)
// How full the lot is (EIObjectMan::CountObjects over the house's object instances): objects / 300,
// weighted objects / 500 and the models' memory, (KB - 420) / 1190 (the house's own model counted too,
// EHouse +0x20); the fill (+0x08, kept in 0..1; +0x0C unclamped) is the largest share plus 0.3 times
// the middle one plus 0.1 times the smallest. The three shares and their counts (+0x10..+0x24) are kept
// for the debug meter. PC: the model sizes are the MODL data sizes the port's loader records (the same
// disc data, ERModel::LoadModel), so the memory share is the GameCube's.
void PollMemoryLevels() {
  EHouse* house = _globals.m_house;  // DAT_80475E94
  if (!house || !house->m_objectMan) return;
  float models = 0.0f, objects = 0.0f, weighted = 0.0f, memory = 0.0f;
  house->m_objectMan->CountObjects(models, objects, weighted, memory);
  if (house->m_model) memory = memory + static_cast<float>(static_cast<double>(house->m_model->m_flags >> 8));
  memory = memory * 0.0009765625f - 420.0f;
  if (memory < 0.0f) memory = 0.0f;
  const float other = weighted / 500.0f;
  const float obj = objects / 300.0f;
  SetMeterF(0x20, other);
  const float mem = memory / 1190.0f;
  SetMeterF(0x18, obj);
  SetMeterF(0x10, mem);
  SetMeterI(0x14, static_cast<int>(memory + 0.5f));
  SetMeterI(0x1c, static_cast<int>(objects + 0.5f));
  SetMeterI(0x24, static_cast<int>(weighted + 0.5f));
  float hi = obj, lo = other;
  if (obj < other) hi = other, lo = obj;
  float mid = hi, top = mem;
  if (mem < hi) mid = mem, top = hi;
  float m2 = mid;
  if (mid < lo) m2 = lo, lo = mid;
  const float fill = lo * 0.1f + m2 * 0.3f + top;
  SetMeterF(0x08, fill);
  SetMeterF(0x0c, fill);
  if (fill < 0.0f) SetMeterF(0x08, 0.0f);
  if (1.0f < MeterF(0x08)) SetMeterF(0x08, 1.0f);
  static const bool trace = std::getenv("SIMMY_FIRE_CODE_TRACE") != nullptr;
  if (trace)
    std::printf("[firecode] objects %.0f (%.3f) weighted %.1f (%.3f) models %.0f memory %.0f KB over 420 (%.3f) -> %.3f\n",
                objects, obj, weighted, other, models, memory, mem, MeterF(0x08));
}

// 800427A8 (game/global.cpp)
// The memory meter's fill (polled first when asked): how close the lot is to the fire code's limit;
// capped at 0.97 once full with the EOR config's fire code switch (EGlobal +0x39C, off in the release).
// PC: fire_code_limit=off in simmy.ini gives 0 (an empty meter, no limit); development aid:
// SIMMY_FIRE_CODE=<0..1> sets the value.
double GetFireCodeValue(bool poll) {
  static const char* test = std::getenv("SIMMY_FIRE_CODE");
  if (test) return std::atof(test);
  if (!simmy::settings_fire_code_limit()) return 0.0;
  if (poll) PollMemoryLevels();
  float v = MeterF(0x08);
  if (1.0f <= v && _globals.ConfigInt(0x39c) != 0) v = 0.97f;
  return v;
}

// 8004281C (game/global.cpp)
// The family's funds (the simulator's, vtable 0xA4).
RET_UNKNOWN GetSimFunds(unsigned int player) {
  return Globs_pSimulator ? Globs_pSimulator->GetFunds(cSimulator__tFundsPlayerNumber{player != 0 ? 1 : 0}) : 0;
}

// 80042874 (game/global.cpp)
bool SetSimFunds(unsigned int player, int funds) {
  if (!Globs_pSimulator) return false;
  Globs_pSimulator->SetFunds(funds, cSimulator__tFundsPlayerNumber{player != 0 ? 1 : 0});
  return true;
}

// ---------------------------------------------------------------------------------------------------
// PCTTarget::cCellInfo

// 801D1F30
PCTTarget__cCellInfo::PCTTarget__cCellInfo() {}

// 801D1F54
int PCTTarget__cCellInfo::Init(ObjSelector* selector, GoalUnlock* unlocks) {
  if (!selector) return 0;
  m_selector = selector;
  m_type = 1;
  m_id = selector->GetGUID();
  m_shader = kNoThumbnail;
  const int index = unlocks ? unlocks->GetObjectIndexFromGuid(static_cast<int>(m_id)) : -1;
  if (index == -1 || g_unlockAllCheat) {
    m_new = 0;
    m_locked = 0;
  } else {
    m_locked = unlocks->IsUnlocked(IGoalUnlock__UnlockType{5}, static_cast<short>(index)) ^ 1;
    if (!m_locked) m_new = unlocks->IsRecentlyUnlocked(IGoalUnlock__UnlockType{5}, static_cast<short>(index));
  }
  const auto* res = reinterpret_cast<const unsigned char*>(GetResDataFromSelector(selector));
  if (res && ERQuickdata::U32(res, 0x1c)) {
    m_shader = ERQuickdata::U32(res, 0x1c);
    return 1;
  }
  return 0;
}

// 801D2074
int PCTTarget__cCellInfo::Init(int id, const PCTFenceData* fence) {
  if (!fence) return 0;
  m_id = static_cast<unsigned int>(id);
  m_type = 2;
  m_data = fence;
  m_shader = fence->shader ? fence->shader : kNoThumbnail;
  return 1;
}
// 801D20B8
// A "WallSet" row: +0x08 its shader.
int PCTTarget__cCellInfo::Init(int id, const WallTile* row) {
  if (!row) return 0;
  m_id = static_cast<unsigned int>(id);
  m_type = 3;
  m_data = row;
  m_shader = ERQuickdata::U32(row, 8);
  if (!m_shader) m_shader = kNoThumbnail;
  return 1;
}
// 801D20FC
// A "FloorSet" row: +0x0C its shader.
int PCTTarget__cCellInfo::Init(int id, const FloorTile* row) {
  if (!row) return 0;
  m_id = static_cast<unsigned int>(id);
  m_type = 4;
  m_data = row;
  m_shader = ERQuickdata::U32(row, 0xc);
  if (!m_shader) m_shader = kNoThumbnail;
  return 1;
}

// 801D2140
// An object's: its master's catalogue price (definition +0x24). Walls: FenceData +4; wallpaper and
// floors: their row's first word.
int PCTTarget__cCellInfo::GetPrice() const {
  if (m_type == 2) return m_data ? static_cast<const PCTFenceData*>(m_data)->price : 0;
  if ((m_type == 3 || m_type == 4) && m_data) return ERQuickdata::S32(m_data, 0);
  if (m_type == 1 && m_selector) {
    auto* master = reinterpret_cast<ObjSelector*>(m_selector->GetMasterSelector());
    if (master && master->m_def) return ERQuickdata::S16(master->m_def, 0x24);
  }
  return 0;
}

// 801D21F4
// Walls: the FenceData's name (+0x08); wallpaper and floors: the row's (+0x04).
int PCTTarget__cCellInfo::GetName(unsigned short* out) const {
  if (m_type == 2 && m_data) {
    CopySlot(out, static_cast<const PCTFenceData*>(m_data)->name);
    return 1;
  }
  if ((m_type == 3 || m_type == 4) && m_data) {
    CopySlot(out, ERQuickdata::LocSlot(m_data, 4));
    return 1;
  }
  if (m_type == 1 && out && m_selector) {
    auto* master = reinterpret_cast<ObjSelector*>(m_selector->GetMasterSelector());
    if (!master) return 0;
    const unsigned short* name = CatalogNameText(master);
    if (!name) return 0;
    CopyText(out, name);
    return 1;
  }
  return 0;
}

// 801D22FC
int PCTTarget__cCellInfo::GetDesc(unsigned short* out) const {
  if (m_type != 1 || !out || !m_selector) return 0;
  auto* master = reinterpret_cast<ObjSelector*>(m_selector->GetMasterSelector());
  if (!master) return 0;
  CopySlot(out, reinterpret_cast<const unsigned short* const*>(master->GetCatalogDescription()));
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// PCTTarget

// 801D2388
// The catalogue for `player`: the player's open action menu backs out; the variables, tab, grid and
// motive icons (blank), the sounds; every selector of the database; the mode's tabs (their icons); the
// screen (0x1E); family 1's goals and unlocks; the fire code warnings already due. In two-player games
// the player becomes the build/buy player.
PCTTarget::PCTTarget(PCTTarget__ePCTMode mode, int player) : m_player(player), m_mode(mode) {
  m_actionMenu = GetActionMenu(static_cast<unsigned int>(player));
  if (m_actionMenu) m_actionMenu->BackOutMenu();
  // GC: DAT_80476254 = 9 (the camera's catalogue mode, see setup_interactor_camera).
  AttachTables("PCTTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  UIDrawTable::InstallEntry(0x19, [this](ERC* rc) { Draw(rc); }, m_name, this);
  m_bbh = new BBHTarget(m_player);
  m_filters[0] = PushFilterAll(0, "PCTTarget");
  m_filters[1] = _gd.m_fieldEC ? PushFilterAll(1, "PCTTarget") : 0;  // DAT_80475C70 two players
  if (ObjectFolderImpl* folder = Globs_pObjectFolder)
    for (ObjSelector* s = folder->GetNextSelector(nullptr); s; s = folder->GetNextSelector(s)) m_selectors.push_back(s);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"PCT_TabsReady", "PCT_CurrTab", "PCT_CurrCell", "PCT_Accept", "PCT_Cancel", "PCT_ShowHelp",
                        "PCT_EnterToolMode", "PCT_Scroll", "PCT_SetVisible"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"PCT_exists", "PCT_GetTabCount", "PCT_GetItemCount", "PCT_RotateItems", "PCT_ShowObjDisp2D",
                        "PCT_ShowObjDisp3D", "PCT_ShowArrowL", "PCT_ShowArrowR", "PCT_ShowDesc", "PCT_CanAffordObject",
                        "PCT_CancelEnabled", "PCT_CatalogMode"})
    m_getTable->InstallCommand(n, get, this);
  for (int i = 0; i < 12; ++i) m_getTable->InstallCommand(Indexed("PCT_GetItemLocked", i).c_str(), get, this);
  for (const char* n : {"PCT_TabName", "PCT_ObjName", "PCT_ObjCost", "PCT_ObjDescription", "PCT_MotiveValue0",
                        "PCT_MotiveValue1", "PCT_MotiveValue2", "PCT_Money", "PCT_Points", "PCT_PlaceButton"})
    m_locTable->InstallCommand(n, loc, this);
  for (int i = 0; i < 12; ++i) {
    InstallShader(this, Indexed("PCT_Overlay", i).c_str(), kBlank);
    InstallShader(this, Indexed("PCT_Icon", i).c_str(), kBlank);
  }
  for (int i = 0; i < 6; ++i) InstallShader(this, Indexed("PCT_OldIcon", i).c_str(), kBlank);
  for (int i = 0; i < 10; ++i) InstallShader(this, Indexed("PCT_TabIcon", i).c_str(), kBlank);
  for (const char* n : {"PCT_MotiveIcon0", "PCT_MotiveIcon1", "PCT_MotiveIcon2", "PCT_MoneyIcon"}) InstallShader(this, n, kBlank);
  UIAUDIO* audio = UIAUDIO::Instance();
  audio->InstallEntry(const_cast<char*>("PCT_SndTabChanged"), 0x649e20f6, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PCT_SndBrowse"), 0x6a7a1517, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PCT_SndError"), 0x3804219f, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PCT_SndSelect"), 0x5d5565e8, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PCT_SndCancel"), 0x76d81c2a, const_cast<char*>(m_name));
  audio->InstallEntry(const_cast<char*>("PCT_SndScroll"), 0x93914889, const_cast<char*>(m_name));
  if (m_mode == 1) {
    m_tabCount = 7;
    m_tabs = kTabsBuild;
  } else if (m_mode == 0 || m_mode == 2) {
    m_tabCount = 9;
    m_tabs = kTabsBuy;
  }
  for (unsigned int i = 0; m_tabs && i < m_tabCount; ++i) InstallShader(this, m_tabs[i].name, m_tabs[i].selectedIcon);
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x1e, m_player);
  m_unlocks = FamilyGoalUnlock(1);  // GC: Neighborhood vtable 0x124 (family 1), vtable 0x14C
  const double fire = GetFireCodeValue(true);
  if (0.66 < fire) {
    m_fireWarned1 = 1;
    if (0.95 < fire) m_fireWarned2 = 1;
  }
  // Two-player: only this player's view is drawn (full screen) while the catalogue is up (ELiveMode::DrawMain).
  if (_gd.m_fieldEC) _gd.SetBuildBuyPlayer(m_player);
  // PC development aid: SIMMY_BUY_FUNDS=<n> gives the family n simoleons (the story family has none).
  if (const char* funds = std::getenv("SIMMY_BUY_FUNDS"))
    if (GetSimFunds(0) < std::atoi(funds)) SetSimFunds(0, std::atoi(funds));
}

// 801D3334
// The variables and entries go, the screen, the build/buy HUD and the filters; the new unlocks looked
// at are no longer new; no build/buy player any more.
PCTTarget::~PCTTarget() {
  if (s_placing == this) {
    StopBuildTool(m_player);  // PC: a build tool reporting here goes with the catalogue
    s_placing = nullptr;
  }
  delete m_bbh;  // GC: +0x214 deleted (vtable +0x80 dtor)
  m_bbh = nullptr;
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    m_drawTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UI3D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x1e);
  if (AptViewer* v = Viewer()) {
    if (m_filters[0]) v->PopAptButtonFilter(0, m_filters[0]);
    if (m_filters[1]) v->PopAptButtonFilter(1, m_filters[1]);
  }
  m_filters[0] = m_filters[1] = 0;
  // GC: DAT_80476254 = 0.
  for (short index : m_seenNew) m_unlocks.SetRecentlyUnlocked(IGoalUnlock__UnlockType{5}, index, false);
  if (m_fireOver && _gd.m_h2dTarget && 0.95 < GetFireCodeValue(true))
    SIMMY_STUB("PCTTarget::~PCTTarget: the fire (H2DTarget +0x98)", 0x801D3334);
  _gd.SetBuildBuyPlayer(-1);
}

// 801D3618
void PCTTarget::SetVariable(char* name, char* value) {
  char text[0x40] = {};
  int n = 0;
  static const bool trace = std::getenv("SIMMY_UI_TRACE") != nullptr;  // PC dev trace
  if (trace) std::fprintf(stderr, "[pct] set %s = %s (cell %u page %u)\n", name, value ? value : "", m_cell, m_page);
  if (!std::strcmp("PCT_CurrCell", name)) {
    if (m_closing || std::sscanf(value, "%d", &n) != 1) return;
    if (n < 0) n = 0xb;
    else if (n > 0xb) n = 0;
    m_cell = static_cast<unsigned int>(n);
    on_selection_changed();
  } else if (!std::strcmp("PCT_Scroll", name)) {
    if (m_closing || std::sscanf(value, "%d", &n) != 1) return;
    on_PCT_Scroll(n);
  } else if (!std::strcmp("PCT_CurrTab", name)) {
    if (m_closing || std::sscanf(value, "%d", &n) != 1 || !m_tabCount) return;
    if (n < 0) n = static_cast<int>(m_tabCount) - 1;
    m_tab = static_cast<unsigned int>(n) % m_tabCount;
    on_tab_changed();
  } else if (!std::strcmp("PCT_TabsReady", name)) {
    if (m_mode == 2) {
      enter_place_tombstone_mode();
      return;
    }
    for (int i = 0; i < 10; ++i) InstallShader(this, Indexed("PCT_TabIcon", i).c_str(), kBlank);
  } else if (!std::strcmp("PCT_Cancel", name)) {
    m_closing = 1;
    if (m_help.m_active || m_fireDialog1 || m_fireDialog2 || m_fireDialog3 || m_dialogActive) return;
    if (m_closeOnCancel) {
      if (_gd.m_field74 == static_cast<UIObjectBase*>(this)) {
        _gd.m_field74 = nullptr;
        delete this;
      }
      return;
    }
    UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("Pause"));
  } else if (!std::strcmp("PCT_ShowHelp", name)) {
    if (m_closing || std::sscanf(value, "%d", &n) != 1) return;
    on_PCT_ShowHelp(n);
  } else if (!std::strcmp("PCT_Accept", name)) {
    if (m_closing) return;
    if (!on_PCT_EnterPlaceMode()) play_sound(0x3804219f);
  } else if (!std::strcmp("PCT_EnterToolMode", name)) {
    if (m_closing) return;
    if (!on_PCT_EnterToolMode()) play_sound(0x3804219f);
  } else if (!std::strcmp("PCT_SetVisible", name)) {
    if (std::sscanf(value, "%d", &n) != 1) return;
    on_PCT_SetVisible(n != 0);
  } else if (!std::strcmp("PCT_HideCallback", name)) {
    SetVariable(const_cast<char*>("PCT_SetVisible"), const_cast<char*>("0"));  // DAT_803EEB5C
  } else if (!std::strcmp("PCT_ShowCallback", name)) {
    SetVariable(const_cast<char*>("PCT_SetVisible"), const_cast<char*>("1"));  // DAT_803EEB58
  } else if (!std::strcmp("PCT_ACCEPT_BUTTON", name)) {
    SetVariable(const_cast<char*>("PCT_Accept"), nullptr);
  } else if (!std::strcmp("PCT_CANCEL_BUTTON", name)) {
    SetVariable(const_cast<char*>("PCT_Cancel"), nullptr);
  } else if (!std::strcmp("PCT_SQUARE_BUTTON", name)) {
    SetVariable(const_cast<char*>("PCT_EnterToolMode"), nullptr);
  } else if (!std::strcmp("PCT_CIRCLE_BUTTON", name)) {
  } else if (!std::strcmp("PCT_PAGE_LEFT", name)) {
    unsigned int tab = m_tab - 1;
    if (tab > m_tabCount) tab = m_tabCount - 1;
    std::snprintf(text, sizeof text, "%d", tab);
    SetVariable(const_cast<char*>("PCT_CurrTab"), text);
  } else if (!std::strcmp("PCT_PAGE_RIGHT", name)) {
    unsigned int tab = m_tab + 1;
    if (tab >= m_tabCount) tab = 0;
    std::snprintf(text, sizeof text, "%d", tab);
    SetVariable(const_cast<char*>("PCT_CurrTab"), text);
  } else if (!std::strcmp("PCT_DPAD_LEFT", name)) {
    // Six cells a column: left/right move a column (scrolling at the edges), up/down a cell.
    if (static_cast<int>(m_cell) - 6 < 0) std::snprintf(text, sizeof text, "%d", 1), SetVariable(const_cast<char*>("PCT_Scroll"), text);
    else std::snprintf(text, sizeof text, "%d", m_cell - 6), SetVariable(const_cast<char*>("PCT_CurrCell"), text);
  } else if (!std::strcmp("PCT_DPAD_RIGHT", name)) {
    if (0xb < m_cell + 6) std::snprintf(text, sizeof text, "%d", 3), SetVariable(const_cast<char*>("PCT_Scroll"), text);
    else std::snprintf(text, sizeof text, "%d", m_cell + 6), SetVariable(const_cast<char*>("PCT_CurrCell"), text);
  } else if (!std::strcmp("PCT_DPAD_UP", name)) {
    if (m_cell / 6 != (m_cell - 1u) / 6) std::snprintf(text, sizeof text, "%d", 2), SetVariable(const_cast<char*>("PCT_Scroll"), text);
    else std::snprintf(text, sizeof text, "%d", m_cell - 1), SetVariable(const_cast<char*>("PCT_CurrCell"), text);
  } else if (!std::strcmp("PCT_DPAD_DOWN", name)) {
    if (m_cell / 6 != (m_cell + 1u) / 6) std::snprintf(text, sizeof text, "%d", 4), SetVariable(const_cast<char*>("PCT_Scroll"), text);
    else std::snprintf(text, sizeof text, "%d", m_cell + 1), SetVariable(const_cast<char*>("PCT_CurrCell"), text);
  }
}

// 801D3D68
char* PCTTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  if (!out) return nullptr;
  *out = 0;
  int value = 0;
  const unsigned int columns = static_cast<unsigned int>(m_cells.size() / 6 + (m_cells.size() % 6 != 0));
  if (!std::strcmp("PCT_GetItemCount", name)) {
    value = static_cast<int>(m_cells.size());
  } else if (!std::strcmp("PCT_ShowObjDisp2D", name)) {
    value = m_show2D != 0;
  } else if (!std::strcmp("PCT_ShowObjDisp3D", name)) {
    value = m_show3D != 0;
  } else if (!std::strcmp("PCT_ShowArrowL", name)) {
    value = 2 < columns && m_page != 0;
  } else if (!std::strcmp("PCT_ShowArrowR", name)) {
    value = 2 < columns && m_page < columns - 2;
  } else if (!std::strcmp("PCT_ShowDesc", name)) {
    value = 1;
    if (m_tab < m_tabCount) {
      const int type = m_tabs[m_tab].type;
      if (type != 1 && 0 < type) value = 4 < type;
    }
  } else if (!std::strcmp("PCT_CanAffordObject", name)) {
    value = m_canAfford != 0;
  } else if (!std::strcmp("PCT_CancelEnabled", name)) {
    m_closing = 1;
    value = !(m_help.m_active || m_fireDialog1 || m_fireDialog2 || m_fireDialog3 || m_dialogActive);
  } else if (!std::strcmp("PCT_CatalogMode", name)) {
    std::strcpy(out, m_mode == 1 ? "1" : "0");
    return out;
  } else if (!std::strncmp("PCT_GetItemLocked", name, 17)) {  // PCT_GetItemLockedPrefixLen
    int cell = 0;
    if (std::sscanf(name, "PCT_GetItemLocked%d", &cell) == 1)
      if (PCTTarget__cCellInfo* info = get_cell_info_at_gridindex(m_cell)) value = info->m_locked != 0;  // GC: the selected cell's
  } else if (!std::strcmp("PCT_GetTabCount", name)) {
    value = static_cast<int>(m_tabCount);
  } else if (!std::strcmp("PCT_RotateItems", name)) {
    if (m_tab >= m_tabCount) return out;
    value = m_tabs[m_tab].type == 3 ? 3 : 0;
  } else {
    if (!std::strcmp("PCT_exists", name)) std::strcpy(out, "loaded");
    return out;
  }
  std::snprintf(out, 0x20, "%d", value);
  return out;
}

// 801D4170
unsigned short* PCTTarget::GetLocalizable(char* name) {
  static unsigned short text[0x1000];
  std::memset(text, 0, sizeof text);
  const unsigned short* const* slot = nullptr;
  if (!std::strcmp("PCT_ObjName", name)) {
    PCTTarget__cCellInfo* info = get_cell_info_at_gridindex(m_cell);
    if (!info) return nullptr;
    if (g_catalogFileNames && info->m_type == 1) {  // DAT_80476190: the object file names
      const char* file = info->m_selector ? info->m_selector->m_fileName : nullptr;
      if (!file) return nullptr;
      unsigned short* d = text;
      while (*file) *d++ = static_cast<unsigned char>(*file++);
    } else if (!info->GetName(text)) {
      return nullptr;
    }
  } else if (!std::strcmp("PCT_ObjDescription", name)) {
    PCTTarget__cCellInfo* info = get_cell_info_at_gridindex(m_cell);
    if (!info) return nullptr;
    if (!info->m_locked || info->m_type != 1) {
      info->GetDesc(text);
    } else {
      // Locked: "costs n aspiration points" (its unlock level x100 into UIDB "unlock_aspiration_points").
      const ObjDefinition* def = info->m_selector ? info->m_selector->m_def : nullptr;
      if (!def) return nullptr;
      static const char kRed[] = "<COLOR_FFFF0000>";
      unsigned short points[64] = {};
      for (int i = 0; kRed[i]; ++i) points[i] = static_cast<unsigned char>(kRed[i]);
      UIObjectBase::makeMoneyString(ERQuickdata::S16(def, 0xc) * 100, points + Len(points), false);
      UIDB::UIDBSetString(const_cast<char*>("unlock_aspiration_points"), points);
      slot = _globals.GetPDAString("cat_locked_msg");
      if (!slot) return nullptr;
      CopySlot(text, slot);
    }
  } else if (!std::strcmp("PCT_ObjCost", name)) {
    PCTTarget__cCellInfo* info = get_cell_info_at_gridindex(m_cell);
    if (!info) return nullptr;
    const int price = info->GetPrice();
    UIObjectBase::makeMoneyString(price, text, false);
    if (m_bbh) m_bbh->SetCurrentPrice(price);
  } else if (!std::strncmp("PCT_MotiveValue", name, 0xf)) {
    int i = 0;
    if (std::sscanf(name, "PCT_MotiveValue%d", &i) != 1 || i > 2 || !get_motive_rating_value(static_cast<unsigned int>(i), text))
      return nullptr;
  } else if (!std::strcmp("PCT_TabName", name)) {
    if (m_tab >= m_tabCount || !(slot = _globals.GetPDAString(m_tabs[m_tab].name))) return nullptr;
    CopySlot(text, slot);
  } else if (!std::strcmp("PCT_Money", name)) {
    const unsigned short* money = UIDB::UIDBGetString(const_cast<char*>("simoleons"));
    if (!money) return nullptr;
    CopyText(text, money);
  } else if (!std::strcmp("PCT_Points", name)) {
    NeighborhoodImpl* hood = Globs_pNeighborhood;
    auto* family = hood ? dynamic_cast<PlayerFamilyImpl*>(hood->GetFamily(1)) : nullptr;  // vtable 0x124
    if (!family) return nullptr;
    UIObjectBase::makeMoneyString(family->m_totalAspirationScore, text, false);  // vtable 0x16C
  } else if (!std::strcmp("PCT_PlaceButton", name)) {
    if (m_tab >= m_tabCount) return nullptr;
    const int type = m_tabs[m_tab].type;
    if (type == 1) slot = _globals.GetPDAString("inv_callout_grab_mode");
    else if (type >= 1 && type <= 4) slot = _globals.GetPDAString("inv_callout_tool_mode");
    if (!slot) return nullptr;
    CopySlot(text, slot);
  } else {
    return nullptr;
  }
  doUIQDStringSubstitution(text);
  const std::size_t n = Len(text);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, text, (n + 1) * 2);
  return out;
}

// 801D464C
// The grab mode's action menu, the build/buy HUD; a fire code warning due (from a placement) as a
// dialog with "ok".
void PCTTarget::Update() {
  if (m_actionMenu) m_actionMenu->Run();
  PageWithCStick();
  if (m_bbh && m_bbh->IsBuildBuyHUDActive()) m_bbh->UpdateBuildBuyHUD();
  if (!m_fireDialog1 && !m_fireDialog2 && !m_fireDialog3) return;
  auto text = [](const unsigned short* const* handle) {
    std::vector<unsigned short> v;
    for (const unsigned short* s = handle ? *handle : nullptr; s && *s; ++s) v.push_back(*s);
    return v;
  };
  m_dialog.m_preselect = 0;
  m_dialog.m_title = text(_globals.GetNewUIString("firecode_dialog_title"));
  if (m_fireDialog1) {
    m_dialog.m_body = text(_globals.GetNewUIString("firecode_text_1"));
    m_fireDialog1 = 0;
  } else if (m_fireDialog2) {
    m_dialog.m_body = text(_globals.GetNewUIString("firecode_text_2"));
    m_fireDialog2 = 0;
  } else {
    m_dialog.m_body = text(_globals.GetNewUIString("firecode_text_3"));
    m_fireDialog3 = 0;
  }
  m_dialog.m_numSelections = 1;
  m_dialog.m_sel[0] = text(_globals.GetNewUIString("ok"));  // DAT_803EF83C
  m_dialog.m_onSelection = [this](int selection) { buy_fail_dialog_selection_callback(selection); };
  m_dialog.m_onZeroInput = [this] { ZeroInputCallback(); };
  m_dialog.m_onClosed = [this] { DialogClosedCallback(); };
  play_sound(0x3804219f);
  m_dialogActive = 1;
  m_dialog.SpawnDialog();
}

// PC mod: mods. The C stick left/right pages the shown catalogue four columns back/on (the decorative tab
// holds hundreds of entries with the converted PC objects; the GameCube's movie only moves a column at a
// time). Only with a mod folder; the GameCube's catalogue ignores the C stick.
void PCTTarget::PageWithCStick() {
  if (!simmy::mods_enabled() || !m_visible || m_closing || m_tab >= m_tabCount || m_dialogActive) return;
  static int wait = 0;
  if (wait > 0) {
    --wait;
    return;
  }
  EController* pad = _pCtrlMan ? _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(m_player))) : nullptr;
  if (!pad) return;
  const double x = pad->GetStick(1, 0, ~0u);
  if (x > -0.6 && x < 0.6) return;
  const unsigned int n = static_cast<unsigned int>(m_cells.size());
  const unsigned int columns = n / 6 + (n % 6 != 0);
  if (columns <= 2) return;
  const unsigned int last = columns - 2;  // two columns show
  const unsigned int page = x < 0 ? (m_page >= 4 ? m_page - 4 : 0) : (m_page + 4 <= last ? m_page + 4 : last);
  wait = 12;
  if (page == m_page) return;
  m_page = page;
  while (m_cell > 0 && get_cell_info_array_index(m_cell) >= n) --m_cell;
  update_grid_shaders();
  on_selection_changed();
  play_sound(0x93914889);  // PCT_SndScroll
}

// 801D49A4
void PCTTarget::Draw(ERC* rc) {
  if (m_actionMenu) m_actionMenu->Draw(rc);
}

// 801D49D0
unsigned int PCTTarget::get_cell_info_array_index(unsigned int cell) const { return m_page * 6 + cell; }

// 801D49E0
PCTTarget__cCellInfo* PCTTarget::get_cell_info_at_gridindex(unsigned int cell) const {
  const unsigned int i = get_cell_info_array_index(cell);
  return i < m_cells.size() ? const_cast<PCTTarget__cCellInfo*>(&m_cells[i]) : nullptr;
}

// 801D4A2C
// The tab's cells by its kind, its icon lit and the others not, back to the first column and cell.
int PCTTarget::on_tab_changed() {
  if (m_tab > m_tabCount) return 0;
  m_cells.clear();
  switch (m_tabs[m_tab].type) {
    case 1: on_tab_changed_objects(); break;
    case 2: on_tab_changed_wall(); break;
    case 3: on_tab_changed_wallpaper(); break;
    case 4: on_tab_changed_floor(); break;
    default: break;
  }
  for (unsigned int i = 0; i < m_tabCount; ++i)
    InstallShader(this, Indexed("PCT_TabIcon", static_cast<int>(i)).c_str(), m_tab == i ? m_tabs[i].selectedIcon : m_tabs[i].icon);
  std::printf("[buy] catalogue tab %u (%s): %d entries\n", m_tab, m_tabs[m_tab].name, static_cast<int>(m_cells.size()));
  m_page = 0;
  update_grid_shaders();
  m_cell = 0;
  return on_selection_changed();
}

// 801D4C54
// A cell per selector the tab's filter takes, sorted (by name or price).
int PCTTarget::on_tab_changed_objects() {
  auto* tab = const_cast<PCTTarget__PCTTabInfo*>(&m_tabs[m_tab]);
  for (ObjSelector* s : m_selectors)
    if (filter_object(tab, s)) {
      m_cells.emplace_back();
      m_cells.back().Init(s, &m_unlocks);
    }
  if (m_sort == 1) {
    std::stable_sort(m_cells.begin(), m_cells.end(), [](const PCTTarget__cCellInfo& a, const PCTTarget__cCellInfo& b) {
      const unsigned short* x = CatalogNameText(reinterpret_cast<ObjSelector*>(a.m_selector->GetMasterSelector()));
      const unsigned short* y = CatalogNameText(reinterpret_cast<ObjSelector*>(b.m_selector->GetMasterSelector()));
      if (x && *x == '"') ++x;
      if (y && *y == '"') ++y;
      return Compare(x, y) < 0;  // sort_selectors_by_name (801D1B7C)
    });
  } else if (m_sort == 2) {
    std::stable_sort(m_cells.begin(), m_cells.end(), [](const PCTTarget__cCellInfo& a, const PCTTarget__cCellInfo& b) {
      return a.GetPrice() < b.GetPrice();  // sort_cellinfo_by_cost (801D1C40)
    });
  }
  return 1;
}

namespace {
// The fences (EGlobal +0xDC "FenceSet") as native FenceData (PC: see PCTFenceData), and the wall builder
// (wallbuilder_13710, made once: shader 0xDF1A8E83, price 1, "inv_wall_catalog_name", style -1).
const PCTFenceData& WallBuilder() {
  static PCTFenceData builder;
  static bool made = false;
  if (!made) {
    made = true;
    builder.shader = 0xdf1a8e83;
    builder.price = 1;
    builder.name = _globals.GetNewUIString("inv_wall_catalog_name");
    builder.field10 = 0xffff;
    builder.style = -1;
  }
  return builder;
}
const std::vector<PCTFenceData>& Fences() {
  static std::vector<PCTFenceData> fences;
  static const void* from = nullptr;
  if (from != _globals.m_fenceSet) {
    from = _globals.m_fenceSet;
    fences.clear();
    const int n = ibuild::SetCount(_globals.m_fenceSet);
    for (int i = 0; i < n; ++i) {
      const void* row = ibuild::SetRow(_globals.m_fenceSet, i);
      PCTFenceData f;
      if (row) {
        f.shader = ERQuickdata::U32(row, 0);
        f.price = ERQuickdata::S32(row, 4);
        f.name = ERQuickdata::LocSlot(row, 8);
        f.style = ERQuickdata::S32(row, 0xc);
      }
      fences.push_back(f);
    }
  }
  return fences;
}
int CompareText(const unsigned short* const* a, const unsigned short* const* b) {
  const unsigned short* x = a ? *a : nullptr;
  const unsigned short* y = b ? *b : nullptr;
  return Compare(x, y);
}
// 801D1C40 sort_cellinfo_by_cost
bool ByCost(const PCTTarget__cCellInfo& a, const PCTTarget__cCellInfo& b) { return a.GetPrice() < b.GetPrice(); }

// 801D4F88
// A wallpaper the player may buy (+0x10), or any with the unlock-all cheat.
bool filter_wallpaper(const void* row) {
  if (!row) return false;
  if (g_unlockAllCheat) return true;
  return ERQuickdata::U8(row, 0x10) != 0;
}
// 801D514C
// A floor pattern (not pattern 0 nor the house's grass) the player may buy (+0x14), any with the
// unlock-all cheat.
bool filter_floor(int index, unsigned int grass, const void* row) {
  if (!row) return false;
  if (g_unlockAllCheat) return true;
  if (index == 0 || ERQuickdata::U32(row, 0xc) == grass) return false;
  return ERQuickdata::U8(row, 0x14) != 0;
}
}  // namespace

// 801D4DDC
// The wall builder first, then each fence (index from 1), sorted after the builder by name
// (sort_walls_by_name 801D1CA4) or price.
int PCTTarget::on_tab_changed_wall() {
  m_cells.emplace_back();
  m_cells.back().Init(-1, &WallBuilder());
  int i = 1;
  for (const PCTFenceData& f : Fences()) {
    m_cells.emplace_back();
    m_cells.back().Init(i++, &f);
  }
  if (m_sort == 1)
    std::stable_sort(m_cells.begin() + 1, m_cells.end(), [](const PCTTarget__cCellInfo& a, const PCTTarget__cCellInfo& b) {
      return CompareText(static_cast<const PCTFenceData*>(a.m_data)->name, static_cast<const PCTFenceData*>(b.m_data)->name) < 0;
    });
  else if (m_sort == 2)
    std::stable_sort(m_cells.begin() + 1, m_cells.end(), ByCost);
  return 1;
}
// 801D4FC8
// The wallpapers the filter takes (by their index in the set), sorted by name (sort_wallpaper_by_name
// 801D1D0C: the row's +0x04) or price.
int PCTTarget::on_tab_changed_wallpaper() {
  const int n = ibuild::SetCount(_globals.m_wallSet);
  for (int i = 0; i < n; ++i) {
    const void* row = ibuild::SetRow(_globals.m_wallSet, i);
    if (!filter_wallpaper(row)) continue;
    m_cells.emplace_back();
    m_cells.back().Init(i, static_cast<const WallTile*>(row));
  }
  if (m_sort == 1)
    std::stable_sort(m_cells.begin(), m_cells.end(), [](const PCTTarget__cCellInfo& a, const PCTTarget__cCellInfo& b) {
      return CompareText(ERQuickdata::LocSlot(a.m_data, 4), ERQuickdata::LocSlot(b.m_data, 4)) < 0;
    });
  else if (m_sort == 2)
    std::stable_sort(m_cells.begin(), m_cells.end(), ByCost);
  return 1;
}
// 801D51A0
// The floor patterns the filter takes (not the house's grass, GetHouseGrassShaderId), sorted by name
// (sort_floors_by_name 801D1D74) or price.
int PCTTarget::on_tab_changed_floor() {
  const char* house = _globals.m_house ? _globals.m_house->GetHouseName() : "";
  const unsigned int grass = static_cast<unsigned int>(_globals.GetHouseGrassShaderId(const_cast<char*>(house)));
  const int n = ibuild::SetCount(_globals.m_floorSet);
  for (int i = 0; i < n; ++i) {
    const void* row = ibuild::SetRow(_globals.m_floorSet, i);
    if (!filter_floor(i, grass, row)) continue;
    m_cells.emplace_back();
    m_cells.back().Init(i, static_cast<const FloorTile*>(row));
  }
  if (m_sort == 1)
    std::stable_sort(m_cells.begin(), m_cells.end(), [](const PCTTarget__cCellInfo& a, const PCTTarget__cCellInfo& b) {
      return CompareText(ERQuickdata::LocSlot(a.m_data, 4), ERQuickdata::LocSlot(b.m_data, 4)) < 0;
    });
  else if (m_sort == 2)
    std::stable_sort(m_cells.begin(), m_cells.end(), ByCost);
  return 1;
}

// 801D5370
// The selection's motive rating icons, its preview, the money icon, whether it is affordable, the
// texts; a new unlock looked at is remembered (no longer new once the catalogue closes); its price.
int PCTTarget::on_selection_changed() {
  calc_motive_ratings();
  for (int i = 0; i < 3; ++i)
    InstallShader(this, Indexed("PCT_MotiveIcon", i).c_str(),
                  m_ratingValues[i] && m_ratingKinds[i] >= 0 && m_ratingKinds[i] < 14 ? kRatingIconIds[m_ratingKinds[i]] : kBlank);
  PCTTarget__cCellInfo* info = get_cell_info_at_gridindex(m_cell);
  m_show3D = 0;
  m_show2D = 0;
  if (update_object_shader() && info) {
    if (info->m_type == 1) m_show3D = 1;
    else if (info->m_type > 1 && info->m_type < 5) m_show2D = 1;
  }
  InstallShader(this, "PCT_MoneyIcon", info ? kMoneyIcon : kBlank);
  m_canAfford = can_afford_selection();
  set_control_visible("PCT_ObjDisp2D", m_show2D != 0);
  if (!m_show2D && UI2D::Instance()->ContainsEntry(const_cast<char*>("PCT_ObjDisp2D")) && m_installed)
    UI2D::Instance()->UnInstallEntry(const_cast<char*>("PCT_ObjDisp2D"));
  set_control_visible("PCT_ObjDisp3D", m_show3D != 0);
  if (!m_show3D && UI3D::Instance()->ContainsEntry(const_cast<char*>("PCT_ObjDisp3D")) && m_installed)
    UI3D::Instance()->UnInstallEntry(const_cast<char*>("PCT_ObjDisp3D"));
  refresh_text_field("PCT_ObjName", 0x10);
  refresh_text_field("PCT_ObjDescription", 0xe);
  refresh_text_field("PCT_ObjCost", 0xe);
  refresh_text_field("PCT_MotiveValue0", 0xe);
  refresh_text_field("PCT_MotiveValue1", 0xe);
  refresh_text_field("PCT_MotiveValue2", 0xe);
  if (!info) {
    m_price = 0;
    return 1;
  }
  if (info->m_new) {
    const int index = m_unlocks.GetObjectIndexFromGuid(static_cast<int>(info->m_id));
    if (index != -1 && std::find(m_seenNew.begin(), m_seenNew.end(), static_cast<short>(index)) == m_seenNew.end())
      m_seenNew.push_back(static_cast<short>(index));
  }
  m_price = info->GetPrice();
  return 1;
}

// 801D581C
// 1 a column left, 3 a column right (not past the last), 2/4 (up/down) nothing.
int PCTTarget::on_PCT_Scroll(int direction) {
  if (direction == 2 || direction == 4) return 1;
  if (direction == 1) {
    if (static_cast<int>(get_cell_info_array_index(m_cell - 6)) < 0 || m_page == 0) return 1;
    --m_page;
  } else if (direction == 3) {
    const unsigned int n = static_cast<unsigned int>(m_cells.size());
    if ((n / 6 + (n % 6 != 0)) * 6 <= get_cell_info_array_index(m_cell + 6)) return 1;
    ++m_page;
  } else {
    return 0;
  }
  update_grid_shaders();
  on_selection_changed();
  return 1;
}

// 801D5910
// Square: the tab's tool (grab mode for objects, the build tools) with the catalogue hidden.
int PCTTarget::on_PCT_EnterToolMode() {
  if (m_tab >= m_tabCount) return 0;
  const int type = m_tabs[m_tab].type;
  if (type <= 0 || type >= 5) return 0;
  m_placeMode = 0;
  return set_dialog_visible(false);
}

// 801D597C
// A: below the fire code's limit (walls: the wall meter, fences: the fence meter, objects: the memory
// meter) the catalogue hides to place the entry (PCT_SetVisible 0 -> enter_place_mode); over it, the
// "too much" dialog. GC: the check for funds is the movie's (PCT_CanAffordObject).
int PCTTarget::on_PCT_EnterPlaceMode() {
  PCTTarget__cCellInfo* info = get_cell_info_at_gridindex(m_cell);
  if (!info || info->m_type <= 0 || info->m_type >= 5) return 0;
  double meter = 0.0;
  if (info->m_type == 2) {
    int rooms = 0;
    if (static_cast<int>(info->m_id) == -1) meter = ERoomWall::GetWallMeterValue(rooms);
    else meter = EIFenceWall::GetFenceMeterValue();
  } else if (info->m_type == 1) {
    meter = GetFireCodeValue(true);
  }
  if (meter < 1.0) {
    m_placeMode = 1;
    return set_dialog_visible(false);
  }
  m_fireDialog3 = 1;  // GC: spawns the firecode_text_3 dialog here (as Update does)
  return 0;
}

// 801D5C80
// Hidden: the build/buy HUD shows and the tool or the placement of the selected entry starts; shown:
// the HUD hides.
int PCTTarget::on_PCT_SetVisible(bool visible) {
  m_visible = visible;
  if (visible) {
    if (m_bbh) m_bbh->HideBuildBuyHUD();
    return 1;
  }
  if (m_bbh) m_bbh->ShowBuildBuyHUD();
  if (m_placeMode) return enter_place_mode(get_cell_info_at_gridindex(m_cell));
  if (m_tab >= m_tabCount) return 0;
  return enter_tool_mode(PCTTarget__ePCTType{m_tabs[m_tab].type}, false);
}

// 801D5D28
// The twelve cells' thumbnails and overlays (locked, new) for the page.
int PCTTarget::update_grid_shaders() {
  for (unsigned int cell = 0; cell < 12; ++cell) {
    const unsigned int i = get_cell_info_array_index(cell);
    unsigned int icon = kBlank, overlay = kBlank;
    if (i < m_cells.size()) {
      const PCTTarget__cCellInfo& info = m_cells[i];
      icon = info.m_shader;
      if (info.m_locked) overlay = kLockedOverlay;
      else if (info.m_new) overlay = kNewOverlay;
    }
    InstallShader(this, Indexed("PCT_Icon", static_cast<int>(cell)).c_str(), icon);
    InstallShader(this, Indexed("PCT_Overlay", static_cast<int>(cell)).c_str(), overlay);
  }
  return 1;
}

// 801D5E38
// The selection's preview: an object's model turning in "PCT_ObjDisp3D" (UI3D, from its ResData: the
// first model and the animation set), a wall, wallpaper or floor's thumbnail in "PCT_ObjDisp2D".
int PCTTarget::update_object_shader() {
  const unsigned int i = get_cell_info_array_index(m_cell);
  if (i >= m_cells.size()) return 0;
  const PCTTarget__cCellInfo& info = m_cells[i];
  if (info.m_type == 1) {
    auto* s = reinterpret_cast<ObjSelector*>(FindSelectorContainingResData(info.m_selector));
    if (!s) return 0;
    ObjectDefinition def;
    if (!GetObjSelector3dDisplayInfo(s, def)) return 0;
    if (!m_installed) return 0;
    return UI3D::Instance()->InstallEntry(const_cast<char*>("PCT_ObjDisp3D"), def.m_character, def.m_model, def.m_anim,
                                          const_cast<char*>(m_name), 0)
               ? 1
               : 0;
  }
  if (info.m_type < 2 || info.m_type > 4 || !info.m_shader || !m_installed) return 0;
  UI2D::Instance()->InstallEntry(const_cast<char*>("PCT_ObjDisp2D"), info.m_shader, const_cast<char*>(m_name), 0);
  return 1;
}

// 801D5F58
// The selected object's three highest catalogue ratings (skill flags counting as 99), highest first
// (GC: a multimap by value, greater first; equal values in kind order).
int PCTTarget::calc_motive_ratings() {
  for (int i = 0; i < 3; ++i) m_ratingKinds[i] = 0, m_ratingValues[i] = 0;
  const unsigned int index = get_cell_info_array_index(m_cell);
  if (index >= m_cells.size() || m_cells[index].m_type != 1 || !m_cells[index].m_selector) return 0;
  auto* master = reinterpret_cast<ObjSelector*>(m_cells[index].m_selector->GetMasterSelector());
  if (!master) return 0;
  std::vector<std::pair<int, int>> ratings;  // value, kind
  for (int kind = 0; kind < 0xe; ++kind) {
    int value = static_cast<int>(master->GetCatalogRating(kind));
    if (IsSkillCatalogRating(kind) && value) value = 99;
    ratings.emplace_back(value, kind);
  }
  std::stable_sort(ratings.begin(), ratings.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  for (int i = 0; i < 3; ++i) {
    m_ratingKinds[i] = ratings[static_cast<std::size_t>(i)].second;
    m_ratingValues[i] = ratings[static_cast<std::size_t>(i)].first;
  }
  return 1;
}

// 801D613C
int PCTTarget::get_motive_rating_label(unsigned int, unsigned short*) const { return 1; }

// 801D6144
// A motive rating's value as text (none for skill flags or 0).
int PCTTarget::get_motive_rating_value(unsigned int index, unsigned short* out) const {
  if (!m_ratingValues[index] || IsSkillCatalogRating(m_ratingKinds[index])) return 0;
  UIObjectBase::makeNumberString(m_ratingValues[index], out);
  return 1;
}

// 801D61AC
// Nothing on GC (the movie asks for the texts again).
int PCTTarget::refresh_text_field(const char*, unsigned int) { return 1; }
// 801D61B4
int PCTTarget::set_control_visible(const char*, bool) { return 1; }

// 801D61BC
// The movie's PCT_SetVisible("0" / "1"): it hides or shows the catalogue and then calls back.
int PCTTarget::set_dialog_visible(bool visible) {
  char arg[2] = {visible ? '1' : '0', 0};
  char* argv[1] = {arg};
  UIObjectBase::AptViewer_CallFunction(const_cast<char*>("PCT_SetVisible"), nullptr, nullptr, 1, argv);
  return 1;
}

// 801D6218
// Not locked and the family has the price (always with the free build/buy cheat).
bool PCTTarget::can_afford_selection() const {
  PCTTarget__cCellInfo* info = get_cell_info_at_gridindex(m_cell);
  if (!info) return false;
  bool ok = info->m_locked == 0 && info->GetPrice() <= static_cast<int>(GetSimFunds(0));
  if (g_freeBuildBuyCheat) ok = true;
  return ok;
}

// 801D6294
// The help for what is up: the catalogue's ("cat_buy_help" / "cat_build_cat_help"), else the tool's.
int PCTTarget::on_PCT_ShowHelp(int) {
  if (m_tab >= m_tabCount) return 0;
  const char* page = nullptr;
  if (m_visible) {
    page = m_mode == 1 ? "cat_build_cat_help" : "cat_buy_help";
  } else {
    const int type = m_tabs[m_tab].type;
    if (type == 1) page = "cat_grab_help";
    else if (type >= 2 && type <= 4) page = m_placeMode ? "cat_build_help" : "cat_removal_help";
    else return 0;
  }
  m_help.SpawnHelpDialog(m_player, page, nullptr, false, nullptr);
  return 1;
}

// 801D6370
bool PCTTarget::start_action_menu() {
  if (m_actionMenu) m_actionMenu->Start(_gd.m_hudTarget, m_player);
  return m_actionMenu != nullptr;
}

// 801D63B8
// Several objects under the grab cursor: a menu of them (each its menu name and icon, no interaction),
// shown at once; taking one grabs it (ActionMenu::AcceptMenu with the grab manipulator active).
int PCTTarget::CreateGrabObjectMenu(const std::vector<cXObjectImpl*>& objects) {
  if (objects.empty() || !m_actionMenu) return 0;
  for (cXObjectImpl* o : objects) {
    ERShader* thumbnail = nullptr;
    const unsigned int shader = GetShaderForObject(o, &thumbnail);
    m_actionMenu->AddMenuItem(o, nullptr, GetObjectMenuName(o), shader, thumbnail, false);
    if (thumbnail) thumbnail->DelRef();  // GetShaderForObject's reference (the menu row holds its own)
  }
  m_actionMenu->ShowMenu();
  return 1;
}

// 801D64A8
// Placing (or a tool): the player's camera to its free mode, out of the camera director's hands; back in
// the catalogue: the catalogue camera (mode 2). GC: DAT_80476254 = 8 / 9.
void PCTTarget::setup_interactor_camera(bool catalogue) {
  ESimsCam* cam = _gd.m_players[m_player & 1].m_cam;
  if (!cam) return;
  if (catalogue) {
    cam->SetMode(ESimsCam__CameraMode{2});
    return;
  }
  // GC: a camera director in control (InteractorModule::IsCameraDirectorInControl) lets go of the camera.
  if (auto* d = static_cast<CameraDirector*>(_globals.CameraDirectorOf(m_player)))
    if (d->m_state != CameraDirector::kIdle && d->m_state != CameraDirector::kGame)
      d->ReleaseCurrentCamera();
  cam->SetMode(ESimsCam__CameraMode{0});
}

// 801D6560
// The tab's tool with the player's interactor: objects the grab manipulator (2), walls the wall
// manipulator, wallpaper the wall painter, floors the floor painter.
int PCTTarget::enter_tool_mode(PCTTarget__ePCTType type, bool placed) {
  if (m_bbh) {
    m_bbh->SetCurrentPrice(0);
    m_bbh->SetSellingPig(false);
  }
  play_sound(0x66dede72);
  switch (static_cast<int>(type)) {
    case 1:
      // Grab mode: the player's grab manipulator (interactor 2) reporting to GrabManipulatorCallback,
      // with the place manipulator's cursor when an object was just placed.
      if (m_bbh) m_bbh->SetBBHMode(1);
      start_action_menu();
      s_placing = this;
      StartGrabManipulator(m_player, placed);
      setup_interactor_camera(false);
      return 1;
    case 2:
      return enter_wall_mode(nullptr, false);
    case 3:
      return enter_wallpaper_mode(nullptr, false);
    case 4:
      return enter_floor_mode(nullptr, false);
    default:
      return 0;
  }
}

// 801D6824
// 0 (dropped) / 1 (grab cancelled): the HUD's price and selling pig cleared. 2: the sell dialog
// ("cat_sell_dialog_title/body", yes/no; sell_dialog_selection_callback) for an object the player
// placed (GetWasPlacedFromInventory, vtable 0x464: set when bought) or with the free cheat, else the
// delete dialog ("inv_add_reject_delete_title/query"; delete_dialog_selection_callback). 3: grab mode ends and the catalogue comes back. GC: also
// SetUIStatusText(0), a debug line.
int PCTTarget::GrabManipulatorCallback(int command, cXObjectImpl* object) {
  if (command == 0 || command == 1) {
    if (m_bbh) {
      m_bbh->SetCurrentPrice(0);
      m_bbh->SetSellingPig(false);
    }
    return 1;
  }
  if (command == 3) {
    StopPlaceManipulator(m_player);  // ChangeActiveInteractorToNull
    if (s_placing == this) s_placing = nullptr;
    set_dialog_visible(true);
    setup_interactor_camera(true);
    play_sound(0x76d81c2a);
    return 1;
  }
  if (command != 2) return 0;
  if (!object) return 1;
  int sell = ObjGetWasPlacedFromInventory(object);  // vtable 0x464
  if (g_freeBuildBuyCheat) sell = 1;
  auto text = [](const unsigned short* const* handle) {
    std::vector<unsigned short> v;
    for (const unsigned short* t = handle ? *handle : nullptr; t && *t; ++t) v.push_back(*t);
    return v;
  };
  m_dialog.m_preselect = 0;
  m_dialog.m_numSelections = 2;
  m_dialog.m_mode = 1;
  m_dialog.m_field14 = 1;
  if (!sell) {
    m_dialog.m_title = text(_globals.GetPDAString("inv_add_reject_delete_title"));
    m_dialog.m_body = text(_globals.GetPDAString("inv_add_reject_delete_query"));
    m_dialog.m_onSelection = [this](int selection) { delete_dialog_selection_callback(selection); };
  } else {
    m_dialog.m_title = text(_globals.GetPDAString("cat_sell_dialog_title"));
    m_dialog.m_body = text(_globals.GetPDAString("cat_sell_dialog_body"));
    m_dialog.m_onSelection = [this](int selection) { sell_dialog_selection_callback(selection); };
  }
  m_dialog.m_sel[0] = text(_globals.GetNewUIString("yes"));  // DAT_803EFAC0
  m_dialog.m_sel[1] = text(_globals.GetNewUIString("no"));   // DAT_803EFAC4
  m_dialog.m_onZeroInput = [this] { ZeroInputCallback(); };
  m_dialog.m_onClosed = [this] { DialogClosedCallback(); };
  play_sound(0xcd34abdc);
  m_dialogActive = 1;
  m_dialog.SpawnDialog();
  return 1;
}

// 801D6DE0
void PCTTarget::buy_fail_dialog_selection_callback(int) { m_dialogActive = 0; }

// 801D6DEC
// Yes (0): the object in hand destroyed (GrabManipulator::DestroyObjectInHand), the HUD cleared.
void PCTTarget::delete_dialog_selection_callback(int selection) {
  m_dialogActive = 0;
  if (selection != 0) return;
  GrabDestroyObjectInHand(m_player);
  if (m_bbh) {
    m_bbh->SetSellingPig(false);
    m_bbh->SetCurrentPrice(0);
  }
  GetFireCodeValue(true);
}

// 801D6E68
// Yes (0): the object in hand sold: the sale sound, its value (GetCurrentValue) back to the family's
// funds (not with the free cheat), the affordability updated, then it is destroyed (a light source:
// the lighting grid again).
void PCTTarget::sell_dialog_selection_callback(int selection) {
  m_dialogActive = 0;
  if (selection != 0) return;
  cXObjectImpl* object = GrabObjectInHand(m_player);
  if (!object) return;
  play_sound(0x590d3a50);
  const int funds = static_cast<int>(GetSimFunds(0));
  const int value = ObjGetCurrentValue(object);
  if (!g_freeBuildBuyCheat) SetSimFunds(0, funds + value);
  m_canAfford = can_afford_selection();
  std::printf("[grab] player %d sold %s for %d: funds %d\n", m_player + 1, object->DebugName(), value,
              static_cast<int>(GetSimFunds(0)));
  const bool light = object->GetObjectLightSource() == 1;
  GrabDestroyObjectInHand(m_player);
  if (light) RecomputeLightingGrid(nullptr);  // InteractorModule::RecomputeLightingGrid(0)
  if (m_bbh) {
    m_bbh->SetSellingPig(false);
    m_bbh->SetCurrentPrice(0);
  }
  GetFireCodeValue(true);
}

// 801D6F88
// (A sim died: mode 2.) The catalogue hides and the player places the tombstone (GUID 0xCCF16080) with
// the place manipulator (interactor 6), reporting to PlaceModeCallback; the camera is the game's.
int PCTTarget::enter_place_tombstone_mode() {
  m_placeMode = 1;
  set_dialog_visible(false);
  s_placing = this;
  if (!StartPlaceManipulator(m_player, 0xccf16080u)) s_placing = nullptr;
  setup_interactor_camera(false);
  GetFireCodeValue(true);
  return 1;
}

// 801D7150
// An object: the place manipulator (interactor 6) with its GUID for the player, reporting to
// PlaceModeCallback, the camera free, the build/buy HUD shown (place mode, no selling pig); walls,
// wallpaper and floors their tools.
int PCTTarget::enter_place_mode(PCTTarget__cCellInfo* cell) {
  if (m_mode == 2 || !cell) return 0;
  if (m_bbh) {
    m_bbh->ShowBuildBuyHUD();
    m_bbh->SetSellingPig(false);
  }
  play_sound(0x374bdcea);
  switch (cell->m_type) {
    case 1:
      if (m_bbh) m_bbh->SetBBHMode(0);
      s_placing = this;
      if (!StartPlaceManipulator(m_player, cell->m_id)) {
        s_placing = nullptr;
        return 0;
      }
      setup_interactor_camera(false);
      return 1;
    case 2:
      return enter_wall_mode(cell, true);
    case 3:
      return enter_wallpaper_mode(cell, true);
    case 4:
      return enter_floor_mode(cell, true);
    default:
      return 0;
  }
}

// 801D1E70
void PCTTarget::static_PlaceModeCallback(InteractorModule__Interactor__CallbackData*) {}

// 801D73BC
// The HUD's selling pig and price for the object picked up: its value when it can be sold
// (GetWasPlacedFromInventory, vtable 0x464), else no pig.
int PCTTarget::PickedUpObject(cXObjectImpl* object) {
  bool pig = true;
  int value = 0;
  if (object) {
    if (!ObjGetWasPlacedFromInventory(object)) pig = false;
    else value = ObjGetCurrentValue(object);
  }
  if (m_bbh) {
    m_bbh->SetSellingPig(pig);
    m_bbh->SetCurrentPrice(value);
  }
  return 1;
}

// 801D745C
// 3 (cancelled): no interactor, the catalogue back, the cancel sound. 0 (placed): the want/fear events
// for the purchase (its worth and category; lights and chairs bought), the price from the family's
// funds (not with the free cheat), the purchase sound, whether the next one is affordable, the fire
// code warnings due; then the grab tool.
int PCTTarget::PlaceModeCallback(int command, unsigned int guid) {
  if (command == 3) {
    StopPlaceManipulator(m_player);
    if (s_placing == this) s_placing = nullptr;
    set_dialog_visible(true);
    setup_interactor_camera(true);
    play_sound(0x76d81c2a);
    return 1;
  }
  if (command != 0) return 0;
  ObjSelector* selector = Globs_pObjectFolder ? Globs_pObjectFolder->GetSelectorByGUID(static_cast<int>(guid)) : nullptr;
  auto* master = selector ? reinterpret_cast<ObjSelector*>(selector->GetMasterSelector()) : nullptr;
  const int price = master && master->m_def ? ERQuickdata::S16(master->m_def, 0x24) : 0;
  NotifyBuildBuyPurchase(master, price);  // the want/fear events (wantfear.cpp)
  const double fire = GetFireCodeValue(true);
  if (!g_freeBuildBuyCheat || m_mode == 2) {
    SetSimFunds(0, static_cast<int>(GetSimFunds(0)) - price);
    m_canAfford = can_afford_selection();
    if (m_mode == 2) m_mode = 0;
    else play_sound(0x87fa0f12);
  } else {
    m_canAfford = 1;
  }
  std::printf("[buy] player %d bought %s for %d: funds %d\n", m_player + 1, selector ? selector->m_fileName : "?", price,
              static_cast<int>(GetSimFunds(0)));
  if (0.66 < fire) {
    if (!m_fireWarned1) m_fireDialog2 = 1, m_fireWarned1 = 1;
    else if (0.95 < fire) {
      if (!m_fireWarned2) m_fireDialog3 = 1, m_fireWarned2 = 1;
      else m_fireOver = 1;
    }
  }
  if (m_bbh) m_bbh->SetCurrentPrice(0);
  enter_tool_mode(PCTTarget__ePCTType{1}, true);
  return 1;
}

// 801D1EA0
int PCTTarget::static_WallManipulatorCallback(InteractorModule__Interactor__CallbackData* data) {
  return data && data->m_user ? data->m_user->WallManipulatorCallback(data) : 0;
}
// 801D1ED0
int PCTTarget::static_WallPainterCallback(InteractorModule__Interactor__CallbackData* data) {
  return data && data->m_user ? data->m_user->WallPainterCallback(data) : 0;
}
// 801D1F00
int PCTTarget::static_FloorPainterCallback(InteractorModule__Interactor__CallbackData* data) {
  return data && data->m_user ? data->m_user->FloorPainterCallback(data) : 0;
}

// 801D77CC
// The wall manipulator (interactor 3) for the player: a fence cell its FenceData (the wall builder's
// cell, or none: walls); the stock the funds over the price (unlimited with the free cheat: GC's
// UNLIMITED_STOCK; none without a cell); the sell (destruct) tool unless `placed`. The build/buy HUD's
// mode (2 build, 5 the sell tool), the camera free.
int PCTTarget::enter_wall_mode(PCTTarget__cCellInfo* cell, bool placed) {
  BuildToolParams params;
  params.id = 3;
  params.player = m_player;
  params.user = this;
  params.callback = static_WallManipulatorCallback;
  if (!cell || static_cast<int>(cell->m_id) == -1) {
    params.data = nullptr;
    params.index = 1;
    params.fence = 0;
  } else {
    if (cell->m_type != 2 || !cell->m_data) return 0;
    params.data = cell->m_data;
    params.index = static_cast<int>(cell->m_id);
    params.fence = 1;
  }
  params.stock = 0;
  if (g_freeBuildBuyCheat) {
    params.stock = -1;  // InteractorModule::WallManipulator::UNLIMITED_STOCK
  } else if (cell) {
    const int price = cell->GetPrice();
    params.stock = price == 0 ? 0x7fffffff : static_cast<int>(GetSimFunds(0)) / price;
  }
  params.sell = placed ? 0 : 1;
  if (m_bbh) m_bbh->SetBBHMode(placed ? 2 : 5);
  setup_interactor_camera(false);
  StopPlaceManipulator(m_player);
  s_placing = this;
  return StartBuildTool(params) ? 1 : 0;
}

// 801D7A50
// 3: the tool left (no interactor), the catalogue back, the cancel sound. 0: walls bought (placing: the
// value from the funds, the purchase sound, the want/fear event kEventBuildWalls) or sold (the tool:
// the value back, the sale sound); the HUD's price cleared. 4: on to the wallpaper tool. 5: the
// selection's value on the HUD (the selling pig when it has one).
int PCTTarget::WallManipulatorCallback(InteractorModule__Interactor__CallbackData* data) {
  if (!data) return 0;
  switch (data->m_state) {
    case 3:
      StopBuildTool(m_player);
      if (s_placing == this) s_placing = nullptr;
      set_dialog_visible(true);
      setup_interactor_camera(true);
      play_sound(0x76d81c2a);
      return 1;
    case 0: {
      const int value = calc_wall_value(data);
      if (!m_placeMode) {
        SetSimFunds(0, static_cast<int>(GetSimFunds(0)) + value);
        play_sound(0x75f2dd8e);
      } else {
        SetSimFunds(0, static_cast<int>(GetSimFunds(0)) - value);
        play_sound(0x87fa0f12);
        // The residents' wants: kEventBuildWalls (NotifyAllResidents, vtable 0x1A4).
        if (Globs_pNeighborhood) {
          WantFear__Notification n;
          n.m_event = WantFearEvent::kBuildWalls;
          n.m_target = WantFearEvent::kGenericParam;
          n.m_count = WantFearEvent::kGenericParam;
          Globs_pNeighborhood->NotifyAllResidents(n, nullptr);
        }
      }
      std::printf("[build] walls %s for %d: funds %d\n", m_placeMode ? "bought" : "sold", value, static_cast<int>(GetSimFunds(0)));
      m_canAfford = can_afford_selection();
      if (m_bbh) {
        m_bbh->SetCurrentPrice(0);
        m_bbh->SetSellingPig(false);
      }
      return 1;
    }
    case 4:
      enter_wallpaper_mode(nullptr, false);
      return 1;
    case 5: {
      const int value = calc_wall_value(data);
      if (m_bbh) {
        m_bbh->SetCurrentPrice(value);
        m_bbh->SetSellingPig(value != 0);
      }
      return 1;
    }
    default:
      return 0;
  }
}

// 801D7CAC
// Selling: each sold style at its price (walls 1, a fence's FenceData price); placing: the stock used
// at the selection's price.
int PCTTarget::calc_wall_value(InteractorModule__Interactor__CallbackData* data) {
  if (!data) return 0;
  int value = 0;
  if (!m_placeMode) {
    for (const BuildSellCount& c : data->m_sells) {
      int price = 0;
      if (c.id == 1) {
        price = 1;
      } else {
        for (const PCTFenceData& f : Fences())
          if (f.style == c.id) {
            price = f.price;
            break;
          }
      }
      value += price * c.count;
    }
  } else {
    const int v = data->m_count * m_price;
    value = v < 0 ? -v : v;
  }
  return value;
}

// 801D7DA0
// The wall painter (interactor 4): the cell's paper (none: the set's first), the stock the funds over
// its price (unlimited with the free cheat or a free paper; none without a cell); the strip tool unless
// `placed`. The HUD's mode (4 paper, 7 strip).
int PCTTarget::enter_wallpaper_mode(PCTTarget__cCellInfo* cell, bool placed) {
  BuildToolParams params;
  params.id = 4;
  params.player = m_player;
  params.user = this;
  params.callback = static_WallPainterCallback;
  if (!cell) {
    params.data = ibuild::SetRow(_globals.m_wallSet, 0);
    params.index = 0;
  } else {
    params.index = static_cast<int>(cell->m_id);
    params.data = cell->m_data;
  }
  params.stock = 0;
  if (g_freeBuildBuyCheat) {
    params.stock = 0x7fffffff;
  } else if (cell) {
    const int price = cell->GetPrice();
    params.stock = price ? static_cast<int>(GetSimFunds(0)) / price : 0x7fffffff;
  }
  params.sell = placed ? 0 : 1;
  if (m_bbh) m_bbh->SetBBHMode(placed ? 4 : 7);
  setup_interactor_camera(false);
  StopPlaceManipulator(m_player);
  s_placing = this;
  return StartBuildTool(params) ? 1 : 0;
}

// 801D7FD8
// Stripping: each sold pattern at its "WallSet" price (+0x00); papering: the stock used at the price.
int PCTTarget::calc_wallpaper_value(InteractorModule__Interactor__CallbackData* data) {
  if (!data) return 0;
  if (!m_placeMode) {
    int value = 0;
    const int n = ibuild::SetCount(_globals.m_wallSet);
    for (const BuildSellCount& c : data->m_sells)
      if (c.id < n)
        if (const void* row = ibuild::SetRow(_globals.m_wallSet, c.id)) value += ERQuickdata::S32(row, 0) * c.count;
    return value;
  }
  const int v = data->m_count * m_price;
  return v < 0 ? -v : v;
}

// 801D8088
// 3: the tool left. 0: the value bought (from the funds) or sold (back), the purchase sound. 4: on to the
// floor tool. 5: the selection's value on the HUD.
int PCTTarget::WallPainterCallback(InteractorModule__Interactor__CallbackData* data) {
  if (!data) return 0;
  switch (data->m_state) {
    case 3:
      StopBuildTool(m_player);
      if (s_placing == this) s_placing = nullptr;
      set_dialog_visible(true);
      setup_interactor_camera(true);
      play_sound(0x76d81c2a);
      return 1;
    case 0: {
      int value = calc_wallpaper_value(data);
      if (value) {
        if (m_placeMode) value = -value;
        SetSimFunds(0, static_cast<int>(GetSimFunds(0)) + value);
        play_sound(0x87fa0f12);
      }
      std::printf("[build] wallpaper %s for %d: funds %d\n", m_placeMode ? "bought" : "sold", value < 0 ? -value : value,
                  static_cast<int>(GetSimFunds(0)));
      m_canAfford = can_afford_selection();
      if (m_bbh) {
        m_bbh->SetCurrentPrice(0);
        m_bbh->SetSellingPig(false);
      }
      return 1;
    }
    case 4:
      enter_floor_mode(nullptr, false);
      return 1;
    case 5: {
      const int value = calc_wallpaper_value(data);
      if (m_bbh) {
        m_bbh->SetCurrentPrice(value);
        m_bbh->SetSellingPig(value != 0);
      }
      return 1;
    }
    default:
      return 0;
  }
}

// 801D829C
// The floor painter (interactor 5): the cell's pattern (none: the set's first), the stock as the wall
// painter's; place mode when `placed`, else remove. The HUD's mode (3 place, 6 remove).
int PCTTarget::enter_floor_mode(PCTTarget__cCellInfo* cell, bool placed) {
  BuildToolParams params;
  params.id = 5;
  params.player = m_player;
  params.user = this;
  params.callback = static_FloorPainterCallback;
  if (!cell) {
    params.data = ibuild::SetRow(_globals.m_floorSet, 0);
    params.index = 0;
  } else {
    params.index = static_cast<int>(cell->m_id);
    params.data = cell->m_data;
  }
  params.stock = 0;
  if (g_freeBuildBuyCheat) {
    params.stock = 0x7fffffff;
  } else if (cell) {
    const int price = cell->GetPrice();
    params.stock = price ? static_cast<int>(GetSimFunds(0)) / price : 0x7fffffff;
  }
  params.place = placed ? 1 : 0;
  if (m_bbh) m_bbh->SetBBHMode(placed ? 3 : 6);
  setup_interactor_camera(false);
  StopPlaceManipulator(m_player);
  s_placing = this;
  return StartBuildTool(params) ? 1 : 0;
}

// 801D84CC
// 3: the tool left. 0: the tiles' value from the funds (painting) or back (taking up), with the purchase
// or sale sound. 4: on to the wall tool. 5: the selection's value on the HUD.
int PCTTarget::FloorPainterCallback(InteractorModule__Interactor__CallbackData* data) {
  if (!data) return 0;
  switch (data->m_state) {
    case 3:
      StopBuildTool(m_player);
      if (s_placing == this) s_placing = nullptr;
      set_dialog_visible(true);
      setup_interactor_camera(true);
      play_sound(0x76d81c2a);
      return 1;
    case 0: {
      int value = calc_floor_tile_value(data);
      if (value) {
        if (data->m_value == 0) {
          play_sound(0x87fa0f12);
        } else {
          value = -value;
          play_sound(0x75f2dd8e);
        }
        SetSimFunds(0, static_cast<int>(GetSimFunds(0)) - value);
        std::printf("[build] floor %s for %d: funds %d\n", data->m_value ? "sold" : "bought", value < 0 ? -value : value,
                    static_cast<int>(GetSimFunds(0)));
        m_canAfford = can_afford_selection();
        if (m_bbh) m_bbh->SetCurrentPrice(0);
      }
      return 1;
    }
    case 4:
      enter_wall_mode(nullptr, false);
      return 1;
    case 5: {
      const int value = calc_floor_tile_value(data);
      if (m_bbh) {
        m_bbh->SetCurrentPrice(value);
        m_bbh->SetSellingPig(value != 0);
      }
      return 1;
    }
    default:
      return 0;
  }
}

// 801D865C
// Taking up: each pattern taken at its "FloorSet" price (+0x00); else the tiles at the price.
int PCTTarget::calc_floor_tile_value(InteractorModule__Interactor__CallbackData* data) {
  if (!data) return 0;
  if (data->m_value != 0 && data->m_stock && !data->m_stock->empty()) {
    int value = 0;
    const int n = ibuild::SetCount(_globals.m_floorSet);
    for (const BuildSellCount& c : *data->m_stock)
      if (c.id < n)
        if (const void* row = ibuild::SetRow(_globals.m_floorSet, c.id)) value += ERQuickdata::S32(row, 0) * c.count;
    return value;
  }
  return data->m_count * m_price;
}

// 801D8750
void PCTTarget::ZeroInputCallback() { m_dialogActive = 0; }
// 801D875C
void PCTTarget::DialogClosedCallback() { m_dialogActive = 0; }
