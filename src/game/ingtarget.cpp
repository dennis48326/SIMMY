// Port of food creation: INGTarget (game/targets_s2c.cpp, screen 0x19 "ING") and the ingredient tables
// (game/inventory.cpp: IngredientManager and the family inventory calls it makes). See ingtarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/inventory_pc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern int INGTarget_m_sRefrigeratorLevel;  // INGTarget::m_sRefrigeratorLevel (gamedata.cpp)
RET_UNKNOWN GetSimFunds(unsigned int player);  // pcttarget.cpp
bool SetSimFunds(unsigned int player, int funds);
bool PcCoopPrivateScreens();                 // uiscreens.cpp (PC co-op private screens)
void PcSetPrivateScreenPlayer(int player);
void PcXamDialogDone(bool made);             // ingamecas.cpp
void PcXamDialogClosed();

namespace {
// ---------------------------------------------------------------------------------------------------
// The rows (big-endian quickdata, self-relative pointers)

const unsigned char* Row(const void* p) { return static_cast<const unsigned char*>(p); }

// IngredientType (0x14 bytes): +0 id, +4 name (a string slot), +8 the tab's selected shader, +0xC its shader.
short TypeId(const IngredientType* t) { return ERQuickdata::S16(Row(t), 0); }
unsigned int TypeSelectedShader(const IngredientType* t) { return ERQuickdata::U32(Row(t), 8); }
unsigned int TypeShader(const IngredientType* t) { return ERQuickdata::U32(Row(t), 0xc); }

// Ingredient (0x50 bytes): +0 id (the inventory GUID), +2 cost (0: one the family grows or buys
// elsewhere, held in its inventory), +4 its IngredientType, +8 the icon, +0xC the large icon, +0x10 the
// name, +0x14 the description, +0x18..+0x3C ints (motive and attribute changes, see configure_object),
// +0x28 the refrigerator level it needs.
short IngId(const Ingredient* i) { return ERQuickdata::S16(Row(i), 0); }
short IngCost(const Ingredient* i) { return ERQuickdata::S16(Row(i), 2); }
const IngredientType* IngType(const Ingredient* i) { return static_cast<const IngredientType*>(ERQuickdata::Ptr(Row(i), 4)); }
unsigned int IngIcon(const Ingredient* i) { return ERQuickdata::U32(Row(i), 8); }
unsigned int IngLargeIcon(const Ingredient* i) { return ERQuickdata::U32(Row(i), 0xc); }
int IngInt(const Ingredient* i, unsigned int off) { return ERQuickdata::S32(Row(i), off); }
int IngFridgeLevel(const Ingredient* i) { return IngInt(i, 0x28); }

// FoodResult (0x48 bytes): +0 id, +4..+0x10 four IngredientTypes (or null), +0x14.. shorts for the
// food object's attributes.
const IngredientType* FoodType(const FoodResult* f, int n) {
  return static_cast<const IngredientType*>(ERQuickdata::Ptr(Row(f), 4 + 4 * n));
}
short FoodShort(const FoodResult* f, int n) { return ERQuickdata::S16(Row(f), 2 * n); }

// EGlobal +0xF8 (DAT_80475EC4): the ingredient quickdata, 0xc8c09e94 (EGlobal::SetDefaults; PC: here).
ERQuickdata* IngredientData() {
  static ERQuickdata* s_data = nullptr;
  if (!s_data) s_data = static_cast<ERQuickdata*>(_quickdataman.EResourceManager::AddRef(0xc8c09e94u, nullptr, 0));
  return s_data;
}
const void* Table(const char* name) {
  ERQuickdata* q = IngredientData();
  return q ? q->getTable(name) : nullptr;
}

// The family inventory: simmy/inventory_pc.hpp (inventory.cpp).
using namespace inventory;

// ---------------------------------------------------------------------------------------------------
// Strings (16-bit)

std::size_t Len16(const unsigned short* s) {
  std::size_t n = 0;
  if (s)
    while (s[n]) ++n;
  return n;
}
void Copy16(unsigned short* dst, const unsigned short* src) {
  std::size_t n = 0;
  if (src)
    for (; src[n]; ++n) dst[n] = src[n];
  dst[n] = 0;
}
void Cat16(unsigned short* dst, const unsigned short* src) { Copy16(dst + Len16(dst), src); }
void CatAscii(unsigned short* dst, const char* src) {
  unsigned short* d = dst + Len16(dst);
  while (*src) *d++ = static_cast<unsigned char>(*src++);
  *d = 0;
}
const unsigned short* SlotText(const unsigned short* const* slot) { return slot ? *slot : nullptr; }

int SortShorts(const void* a, const void* b) {  // qsort's form of less_int16_t
  return static_cast<int>(less_int16_t(const_cast<void*>(a), const_cast<void*>(b)));
}
AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }

unsigned int PushFilterAll(int player, const char* name) {  // UIObjectBase::pushAptButtonFilterALL
  AptViewer* v = Viewer();
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}

const char* const kTabShaderNames[12] = {"ING_TabIcon0", "ING_TabIcon1", "ING_TabIcon2", "ING_TabIcon3",
                                         "ING_TabIcon4", "ING_TabIcon5", "ING_TabIcon6", "ING_TabIcon7",
                                         "ING_TabIcon8", "ING_TabIcon9", "ING_TabIcon10", "ING_TabIcon11"};  // 803F6B98
const char* const kGridShaderNames[12] = {"ING_IngIcon1", "ING_IngIcon2", "ING_IngIcon3", "ING_IngIcon4",
                                          "ING_IngIcon5", "ING_IngIcon6", "ING_IngIcon7", "ING_IngIcon8",
                                          "ING_IngIcon9", "ING_IngIcon10", "ING_IngIcon11", "ING_IngIcon12"};  // 803F6C88
const char* const kMixShaderNames[4] = {"MixIngIcon1", "MixIngIcon2", "MixIngIcon3", "MixIngIcon4"};  // 803F6CE8
const char* const kHelpBodyIDs[2] = {"fc_help_summary_body", "fc_help_body"};  // 80500680 ING_HelpBodyIDs
// 803F18BC: the tabs' ingredient types, in tab order.
const short kTabTypes[10] = {1, 5, 6, 3, 4, 2, 7, 8, 9, 10};
}  // namespace

// ---------------------------------------------------------------------------------------------------
// IngredientManager (game/inventory.cpp)

// 8004B260
IngredientManager* GetIngredientManagerInstance() {
  static IngredientManager* s_manager = nullptr;  // spIngredientManager_2190
  if (!s_manager) s_manager = new IngredientManager;
  return s_manager;
}

// 8004B29C
IngredientManager::IngredientManager() {
  Table("IngredientType");
  Table("Ingredient");
  Table("FoodResult");
}
// 8004B308
IngredientManager::~IngredientManager() = default;

// 8004B330
bool IngredientManager::GetIngredientTypes(const IngredientType** out, unsigned int max) const {
  if (!out || !max) return false;
  std::memset(out, 0, max * sizeof *out);
  const void* t = Table("IngredientType");
  const unsigned int n = t ? ERQuickdata::TableRowCount(t) : 0;
  for (unsigned int i = 0; i < n && i < max; ++i)
    out[i] = reinterpret_cast<const IngredientType*>(ERQuickdata::TableRows(t) + i * 0x14);
  return true;
}

// 8004B3E8
bool IngredientManager::GetIngredientsByType(short type, const Ingredient** out, unsigned int max) const {
  if (!out || !max) return false;
  std::memset(out, 0, max * sizeof *out);
  const void* t = Table("Ingredient");
  const unsigned int n = t ? ERQuickdata::TableRowCount(t) : 0;
  unsigned int found = 0;
  for (unsigned int i = 0; i < n && found < max; ++i) {
    const auto* ing = reinterpret_cast<const Ingredient*>(ERQuickdata::TableRows(t) + i * 0x50);
    const IngredientType* it = IngType(ing);
    if (it && TypeId(it) == type) out[found++] = ing;
  }
  return true;
}

// 8004B4D0
// All the ingredients (true: they all fit).
bool IngredientManager::GetIngredients(const Ingredient** out, unsigned int max) const {
  if (!out || !max) return false;
  std::memset(out, 0, max * sizeof *out);
  const void* t = Table("Ingredient");
  const unsigned int n = t ? ERQuickdata::TableRowCount(t) : 0;
  for (unsigned int i = 0; i < n && i < max; ++i)
    out[i] = reinterpret_cast<const Ingredient*>(ERQuickdata::TableRows(t) + i * 0x50);
  return n <= max;
}

// 8004B6E4
const Ingredient* IngredientManager::GetIngredient(short id) const {
  const void* t = Table("Ingredient");
  const unsigned int n = t ? ERQuickdata::TableRowCount(t) : 0;
  for (unsigned int i = 0; i < n; ++i) {
    const auto* ing = reinterpret_cast<const Ingredient*>(ERQuickdata::TableRows(t) + i * 0x50);
    if (IngId(ing) == id) return ing;
  }
  return nullptr;
}

// 8004B758
const FoodResult* IngredientManager::GetFoodResult(short id) const {
  const void* t = Table("FoodResult");
  const unsigned int n = t ? ERQuickdata::TableRowCount(t) : 0;
  for (unsigned int i = 0; i < n; ++i) {
    const auto* f = reinterpret_cast<const FoodResult*>(ERQuickdata::TableRows(t) + i * 0x48);
    if (FoodShort(f, 0) == id) return f;
  }
  return nullptr;
}

// 8004B598
RET_UNKNOWN less_int16_t(void* a, void* b) {
  const short x = *static_cast<const short*>(a), y = *static_cast<const short*>(b);
  if (x == y) return 0;
  return y <= x ? 1 : -1;
}
// 8004B5BC
// A recipe's four ingredient type ids (0 for none).
RET_UNKNOWN get_ingredient_type_array(FoodResult* f, short* out) {
  for (int i = 0; i < 4; ++i) out[i] = 0;
  if (!f) return 0;
  for (int i = 0; i < 4; ++i)
    if (const IngredientType* t = FoodType(f, i)) out[i] = TypeId(t);
  return 1;
}
// 8004B638
// The mix's types without repeats, sorted.
RET_UNKNOWN make_ingredient_type_array(short a, short b, short c, short d, short* out) {
  if (!out) return 0;
  out[0] = a;
  out[1] = a != b ? b : 0;
  out[2] = out[0] != c && out[1] != c ? c : 0;
  out[3] = out[0] != d && out[1] != d && out[2] != d ? d : 0;
  std::qsort(out, 4, sizeof(short), SortShorts);
  return 1;
}

// 8004B7C8
// The recipe whose ingredient types (as a set) are the mix's, or null.
const FoodResult* IngredientManager::MixIngredients(short a, short b, short c, short d) const {
  short mix[4];
  make_ingredient_type_array(a, b, c, d, mix);
  const void* t = Table("FoodResult");
  const unsigned int n = t ? ERQuickdata::TableRowCount(t) : 0;
  for (unsigned int i = 0; i < n; ++i) {
    const auto* f = reinterpret_cast<const FoodResult*>(ERQuickdata::TableRows(t) + i * 0x48);
    short types[4];
    get_ingredient_type_array(const_cast<FoodResult*>(f), types);
    std::qsort(types, 4, sizeof(short), SortShorts);
    if (!std::memcmp(types, mix, sizeof mix)) return f;
  }
  return nullptr;
}

namespace {
// 801B938C
const Ingredient* find_ingredient(const Ingredient* const* list, unsigned int n, short id) {
  for (unsigned int i = 0; i < n && list[i]; ++i)
    if (IngId(list[i]) == id) return list[i];
  return nullptr;
}
// 801B93D4
const IngredientType* find_ingredient_type(const IngredientType* const* list, unsigned int n, short id) {
  for (unsigned int i = 0; i < n && list[i]; ++i)
    if (TypeId(list[i]) == id) return list[i];
  return nullptr;
}
}  // namespace

// ---------------------------------------------------------------------------------------------------
// INGTarget

// 801B941C
INGTarget__TabInfo::INGTarget__TabInfo() = default;
// 801B9434
INGTarget__IngInfo::INGTarget__IngInfo() = default;
// 801B9448
void INGTarget__IngInfo::erase() {
  m_ingredient = nullptr;
  m_count = 0;
}

// 801B9468
// For PlayerNumber's player: the mode (a group meal makes three portions), the food object, the
// commands, sounds and button filters; the tabs (the ingredient types of 803F18BC: the last three list
// what the family holds, three types standing for others), the funds, the family's inventory and the
// edit buffer, blank mix and large icons; then the screen.
INGTarget::INGTarget() {
  AttachTables("INGTarget");
  m_player = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
  m_mode = UIDB::UIDBGetInt(const_cast<char*>(m_player == 0 ? "ING_ModeP1" : "ING_ModeP2"));
  if (m_mode == 2) {
    m_portions = 3;
  } else {
    m_portions = 1;
    m_mode = 1;
  }
  m_object = UIDB::UIDBGetInt(const_cast<char*>("INV_LotNumber"));
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"ING_exists", "ING_GetTabCount1", "ING_GetTabCount2", "ING_GetItemCount", "ING_GetIngState1",
                        "ING_GetIngState2", "ING_GetIngState3", "ING_GetIngState4", "ING_GetIngState5", "ING_GetIngState6",
                        "ING_GetIngState7", "ING_GetIngState8", "ING_GetIngState9", "ING_GetIngState10",
                        "ING_GetIngState11", "ING_GetIngState12", "ING_GetMixIngState1", "ING_GetMixIngState2",
                        "ING_GetMixIngState3", "ING_GetMixIngState4", "ING_GetMixSlot", "ING_GetMixOK"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"ING_TabsReady", "ING_CurrTab", "ING_CurrCell", "ING_SelectIng", "ING_CurrMixIng",
                        "ING_UnselectIng", "ING_EndDialog", "ING_ScreenReady", "ING_Scroll", "ING_ShowHelp",
                        "ING_ShowRecipes"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"ING_TabName", "ING_IngName", "ING_IngCost", "ING_IngDescription", "ING_IngCount",
                        "ING_PriceLabel", "ING_IngModName1", "ING_IngModName2", "ING_IngModName3", "ING_IngModValue1",
                        "ING_IngModValue2", "ING_IngModValue3", "ING_MixIngName1", "ING_MixIngName2", "ING_MixIngName3",
                        "ING_MixIngName4", "ING_MixIngCount1", "ING_MixIngCount2", "ING_MixIngCount3",
                        "ING_MixIngCount4", "ING_TotalCost", "ING_HelpCallout", "ING_Done"})
    m_locTable->InstallCommand(n, loc, this);
  UIAUDIO* audio = UIAUDIO::Instance();
  const struct {
    const char* name;
    unsigned int id;
  } sounds[] = {{"ING_SndTabChanged", 0x649e20f6}, {"ING_SndBrowse", 0x6a7a1517}, {"ING_SndError", 0x3804219f},
                {"ING_SndSelect", 0x5d5565e8},     {"ING_SndUnselect", 0x76d81c2a}, {"ING_SndMix", 0x5d5565e8},
                {"ING_SndCancel", 0x76d81c2a}};
  for (const auto& s : sounds) audio->InstallEntry(const_cast<char*>(s.name), s.id, const_cast<char*>(m_name));
  // GC: both players' pads (DAT_80475C70: two players). PC co-op: only this player's; the screen is
  // theirs (uiscreens.cpp, PcCoopPrivateScreens).
  m_pcPrivate = PcCoopPrivateScreens();
  if (m_pcPrivate) {
    PcSetPrivateScreenPlayer(m_player);
    simmy::coop_set_private_screen("ING", m_player);
    m_filters[m_player & 1] = PushFilterAll(m_player & 1, "INGTarget");
  } else {
    m_filters[0] = PushFilterAll(0, "INGTarget");
    if (_gd.m_fieldEC) m_filters[1] = PushFilterAll(1, "INGTarget");
  }
  if (IngredientManager* mgr = GetIngredientManagerInstance()) {
    const IngredientType* types[10];
    mgr->GetIngredientTypes(types, 10);
    for (int i = 0; i < 10; ++i) {
      m_tabs[i].m_type = find_ingredient_type(types, 10, kTabTypes[i]);
      m_tabs[i].m_held = i > 6 ? 1 : 0;
      if (kTabTypes[i] == 9) m_tabs[i].m_typeId = 6;
      else if (kTabTypes[i] == 8) m_tabs[i].m_typeId = 1;
      else if (kTabTypes[i] == 10) m_tabs[i].m_typeId = 5;
    }
  }
  m_funds = static_cast<int>(GetSimFunds(0));
  // GC: Neighborhood::GetFamily(DAT_80475C5C) -> vtable 0x144 GetInventory.
  if (Globs_pNeighborhood) m_inventory = dynamic_cast<PlayerFamilyImpl*>(Globs_pNeighborhood->GetFamily(_gd.m_fieldD8));
  create_edit_buffer();
  for (const char* n : kMixShaderNames) UI2D::Instance()->InstallEntry(const_cast<char*>(n), get_blank_shader_id(), const_cast<char*>(m_name), 0);
  UI2D::Instance()->InstallEntry(const_cast<char*>("ING_LargeIcon"), get_blank_shader_id(), const_cast<char*>(m_name), 0);
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x19, m_player);
  std::printf("[ing] food creation for player %d (mode %d, food object %d, refrigerator level %d, funds %d)\n",
              m_player + 1, m_mode, m_object, INGTarget_m_sRefrigeratorLevel, m_funds);
}

// 801BA500
INGTarget::~INGTarget() {
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
  delete m_recipes;
  m_recipes = nullptr;
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x19);
  if (AptViewer* v = Viewer()) {
    if (m_filters[0]) v->PopAptButtonFilter(0, m_filters[0]);
    if (_gd.m_fieldEC && m_filters[1]) v->PopAptButtonFilter(1, m_filters[1]);
  }
  SetRefrigeratorLevel(0);
  if (m_pcPrivate) {
    PcSetPrivateScreenPlayer(-1);
    simmy::coop_set_private_screen("ING", -1);
    PcXamDialogClosed();
  }
}

// 801BA6A8
// Any command restarts the idle time. "ING_CurrTab" the tab (its icons; the cells refilled);
// "ING_CurrCell" the cell (1-based; the large icon); "ING_CurrMixIng" the mix slot (1-based);
// "ING_SelectIng" the cell's ingredient into the mix (enough held and money, the fridge's level, not in
// the mix yet); "ING_UnselectIng" the mix slot's ingredient back; "ING_TabsReady" the tab icons;
// "ING_ShowHelp" the help's pages; "ING_ShowRecipes" the recipe book; "ING_EndDialog" 1 done, 0 asks
// whether to leave ("fc_exit_prompt", yes / no); "ING_ScreenReady" the cells' states. A refused
// select or unselect sounds the error.
void INGTarget::SetVariable(char* name, char* value) {
  m_idleTime = 600.0f;
  UI2D* ui2d = UI2D::Instance();
  auto install = [&](const char* shaderName, unsigned int id) {
    if (m_installed && ui2d) ui2d->InstallEntry(const_cast<char*>(shaderName), id, const_cast<char*>(m_name), 0);
  };
  if (!std::strcmp("ING_CurrTab", name)) {
    std::sscanf(value, "%d", reinterpret_cast<int*>(&m_curTab));
    for (unsigned int i = 0; i < 10; ++i) {
      const IngredientType* t = m_tabs[i].m_type;
      install(kTabShaderNames[i], !t ? get_blank_shader_id() : i == m_curTab ? TypeSelectedShader(t) : TypeShader(t));
    }
    update_current_items(m_curTab);
    install_current_item_shaders();
    return;
  }
  if (!std::strcmp("ING_CurrCell", name)) {
    std::sscanf(value, "%d", reinterpret_cast<int*>(&m_curCell));
    --m_curCell;
    IngInfo* info = get_current_inginfo();
    install("ING_LargeIcon", info && info->m_ingredient ? IngLargeIcon(info->m_ingredient) : get_blank_shader_id());
    return;
  }
  if (!std::strcmp("ING_CurrMixIng", name)) {
    std::sscanf(value, "%d", reinterpret_cast<int*>(&m_curMix));
    --m_curMix;
    return;
  }
  auto refresh = [this] {
    update_item_states();
    set_done_button_state(mix_ingredients() != nullptr);
  };
  if (!std::strcmp("ING_SelectIng", name)) {
    IngInfo* info = get_current_inginfo();
    if (info && info->m_count != 0 && m_portions <= info->m_count) {
      const Ingredient* ing = info->m_ingredient;
      if (IngCost(ing) * static_cast<int>(m_portions) <= m_funds && IngFridgeLevel(ing) <= INGTarget_m_sRefrigeratorLevel &&
          !get_mix_ingredient(IngId(ing))) {
        const int slot = add_mix_ing(info, m_portions);
        if (slot != -1) {
          install(kMixShaderNames[slot], IngIcon(ing));
          update_mix_ing_name(static_cast<unsigned int>(slot));
          if (AptViewer* v = Viewer()) v->NewCallFunction(const_cast<char*>("UpdateItemCount"), nullptr, nullptr, 0);
          refresh();
          return;
        }
      }
    }
    play_error_sound();
    return;
  }
  if (!std::strcmp("ING_UnselectIng", name)) {
    IngInfo* mix = get_current_mix_inginfo();
    if (mix && mix->m_ingredient && mix->m_count) {
      // The mix slot's ingredient back into its edit buffer entry.
      unsigned int i = 0;
      for (; i < 0x48; ++i)
        if (m_items[i].m_ingredient && IngId(m_items[i].m_ingredient) == IngId(mix->m_ingredient)) break;
      if (i < 0x48) {
        mix->m_count -= m_portions;
        if (IngCost(mix->m_ingredient) == 0) m_items[i].m_count += m_portions;
        m_funds += IngCost(m_items[i].m_ingredient) * static_cast<int>(m_portions);
        int cell = 0;
        bool shown = false;
        for (; cell < 12; ++cell) {
          IngInfo* g = m_grid[cell];
          if (g && g->m_ingredient && IngId(g->m_ingredient) == IngId(mix->m_ingredient)) {
            shown = true;
            break;
          }
        }
        if (shown && cell == static_cast<int>(m_curCell))
          if (AptViewer* v = Viewer()) v->NewCallFunction(const_cast<char*>("UpdateItemCount"), nullptr, nullptr, 0);
        if (mix->m_count == 0) {
          mix->m_ingredient = nullptr;
          install(kMixShaderNames[m_curMix < 4 ? m_curMix : 0], get_blank_shader_id());
          update_mix_ing_name(m_curMix);
        }
        refresh();
        return;
      }
    }
    play_error_sound();
    return;
  }
  if (!std::strcmp("ING_TabsReady", name)) {
    for (unsigned int i = 0; i < 10; ++i)
      install(kTabShaderNames[i], m_tabs[i].m_type ? TypeShader(m_tabs[i].m_type) : get_blank_shader_id());
    return;
  }
  if (!std::strcmp("ING_ShowHelp", name)) {
    if (m_helpPage > 1) m_helpPage = 0;
    m_help.SpawnHelpDialog(m_player, "fc_help_title", kHelpBodyIDs[m_helpPage],
                           [this](int choice) { onHelpDialogSelection(choice); }, false, m_helpPage != 1 ? "ok" : nullptr);
    return;
  }
  if (!std::strcmp("ING_ShowRecipes", name)) {
    if (!m_recipeRequest) m_recipeRequest = 1;
    return;
  }
  if (!std::strcmp("ING_EndDialog", name)) {
    int done = 0;
    if (std::sscanf(value, "%d", &done) != 1) return;
    if (done) {
      end_dialog(true);
      return;
    }
    // "Leave without cooking?" yes / no, on the help dialog.
    auto vec = [](const unsigned short* s) {
      std::vector<unsigned short> v;
      if (s) v.assign(s, s + Len16(s));
      v.push_back(0);
      return v;
    };
    m_help.m_numSelections = 2;
    m_help.m_preselect = 0;
    m_help.m_field14 = 1;
    m_help.m_title = vec(nullptr);
    m_help.m_body = vec(SlotText(_globals.GetPDAString("fc_exit_prompt")));
    m_help.m_sel[0] = vec(SlotText(_globals.GetNewUIString("yes")));
    m_help.m_sel[1] = vec(SlotText(_globals.GetNewUIString("no")));
    m_help.m_onSelection = [this](int sel) { exit_ingredients_dialog_selection_callback(sel); };
    HelpDialog* help = &m_help;
    m_help.m_onZeroInput = [help] { help->ZeroInputCallback(); };
    m_help.m_onClosed = [help] { help->DialogClosedCallback(); };
    m_help.m_callback = nullptr;  // +0x130
    m_help.m_field24 = 0;         // +0xC8
    // GC: the prompt keeps the dialog's players (whoever the last help page was for, else either
    // player). PC co-op: this player's, so it is theirs alone like the screen (g2dtarget.cpp).
    if (m_pcPrivate) m_help.m_players = m_player == 1 ? 2 : 1;
    m_help.SpawnDialog();
    return;
  }
  if (!std::strcmp("ING_ScreenReady", name)) update_item_states();
}

// 801BAF84
// "ING_exists" "loaded"; the two tab rows' counts (3 and 7).
char* INGTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x80));
  std::memset(out, 0, 0x80);
  if (!std::strcmp(name, "ING_exists")) std::strcpy(out, "loaded");
  else if (!std::strcmp(name, "ING_GetTabCount1")) std::snprintf(out, 0x80, "%d", 3);
  else if (!std::strcmp(name, "ING_GetTabCount2")) std::snprintf(out, 0x80, "%d", 7);
  return out;
}

// 801BB068
// The current cell's name, description, cost (red when it cannot be afforded), held count, the price
// label, the three motive names ("energy", "hunger", "bladder") and changes; the tab's name; the mix
// slots' names ("name $cost [x portions]"); the total ("fc_total $n", only with something in the mix);
// "fc_done". A leading player digit is skipped.
unsigned short* INGTarget::GetLocalizable(char* name) {
  static unsigned short buf[0x1000];
  std::memset(buf, 0, sizeof buf);
  if (static_cast<unsigned char>(*name - '0') < 2) ++name;
  const unsigned short* text = nullptr;
  bool built = false;
  auto ing = [this] { return get_current_ingredient(); };
  if (!std::strcmp("ING_IngName", name)) {
    if (!ing()) return nullptr;
    text = ERQuickdata::LocString(Row(ing()), 0x10);
  } else if (!std::strcmp("ING_IngDescription", name)) {
    if (!ing()) return nullptr;
    text = ERQuickdata::LocString(Row(ing()), 0x14);
  } else if (!std::strcmp("ING_IngCost", name)) {
    const Ingredient* i = ing();
    if (!i || IngCost(i) == 0) return nullptr;
    std::size_t at = 0;
    IngInfo* info = get_current_inginfo();
    if (!query_enabled(info) && info && info->m_ingredient &&
        IngCost(info->m_ingredient) * static_cast<int>(m_portions) > m_funds) {
      CatAscii(buf, "<COLOR_FFFF0000>");
      at = Len16(buf);
    }
    UIObjectBase::makeMoneyString(IngCost(i) * static_cast<int>(m_portions), buf + at, true);
    built = true;
  } else if (!std::strcmp("ING_IngCount", name)) {
    IngInfo* info = get_current_inginfo();
    if (!info || !info->m_ingredient || IngCost(info->m_ingredient) != 0) return nullptr;
    UIObjectBase::makeNumberString(static_cast<int>(info->m_count), buf);
    built = true;
  } else if (!std::strcmp("ING_PriceLabel", name)) {
    const Ingredient* i = ing();
    if (!i || IngCost(i) == 0) return nullptr;
    text = SlotText(_globals.GetPDAString("fc_price"));
    if (!text) return nullptr;
  } else if (!std::strcmp("ING_IngModValue1", name)) {
    if (!ing()) return nullptr;
    UIObjectBase::makeNumberString(IngInt(ing(), 0x1c), buf);
    built = true;
  } else if (!std::strcmp("ING_IngModValue2", name)) {
    if (!ing()) return nullptr;
    UIObjectBase::makeNumberString(IngInt(ing(), 0x18), buf);
    built = true;
  } else if (!std::strcmp("ING_IngModValue3", name)) {
    if (!ing()) return nullptr;
    UIObjectBase::makeNumberString(IngInt(ing(), 0x24), buf);
    built = true;
  } else if (!std::strcmp("ING_TabName", name)) {
    const IngredientType* t = m_curTab < 10 ? m_tabs[m_curTab].m_type : nullptr;
    if (!t) return nullptr;
    text = ERQuickdata::LocString(Row(t), 4);
  } else if (!std::strcmp("ING_IngModName1", name) || !std::strcmp("ING_IngModName2", name) ||
             !std::strcmp("ING_IngModName3", name)) {
    if (!get_current_inginfo()) return nullptr;
    const char n = name[std::strlen(name) - 1];
    text = SlotText(_globals.GetNewUIString(n == '1' ? "energy" : n == '2' ? "hunger" : "bladder"));
  } else if (!std::strncmp("ING_MixIngName", name, 14) && name[14] >= '1' && name[14] <= '4' && !name[15]) {
    if (!get_mix_ing_name(static_cast<unsigned int>(name[14] - '1'), buf)) return nullptr;
    built = true;
  } else if (!std::strcmp("ING_TotalCost", name)) {
    unsigned int count = 0;
    const int total = get_total_cost(&count);
    if (!count) return nullptr;
    buf[0] = 0;
    std::size_t at = 0;
    if (const unsigned short* label = SlotText(_globals.GetPDAString("fc_total"))) {
      Copy16(buf, label);
      CatAscii(buf, " ");
      at = Len16(label) + 1;
    }
    UIObjectBase::makeMoneyString(total, buf + at, true);
    built = true;
  } else if (!std::strcmp("ING_Done", name)) {
    text = SlotText(_globals.GetPDAString("fc_done"));
    if (!text) return nullptr;
  } else {
    return nullptr;  // ING_MixIngCount*, ING_HelpCallout: nothing
  }
  if (!built) {
    if (!text) return nullptr;
    Copy16(buf, text);
  }
  UIQDTarget::UIQDSubstitutionStrings(buf);
  const std::size_t n = Len16(buf);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, buf, (n + 1) * 2);
  return out;
}

// 801BB6C0
// Ten idle minutes (no recipe book open) close the screen without a dish; the recipe book opens
// (1) or closes (2) as asked.
void INGTarget::Update() {
  if (!m_recipes && !m_timedOut && m_idleTime > 0.0f) {
    m_idleTime -= _dt;
    if (m_idleTime <= 0.0f) {
      m_timedOut = 1;
      end_dialog(false);
    }
  }
  if (m_recipeRequest == 1) {
    delete m_recipes;
    m_recipes = nullptr;
    m_recipes = new RCPTarget(this, m_player);  // this's cRCPEventHandler (+0x84); 0x14C bytes
    m_recipeRequest = 0;
  } else if (m_recipeRequest == 2) {
    delete m_recipes;
    m_recipes = nullptr;
    m_recipeRequest = 0;
  }
}

// 801BB7DC
// Done with a dish (the mix makes a recipe and the food object takes it): the held ingredients used
// are taken from the inventory and the cost is paid. The tree hears whether a dish was made; back to
// the game.
int INGTarget::end_dialog(bool made) {
  if (made) {
    made = configure_object(static_cast<short>(m_object), m_mix[0].m_ingredient, m_mix[1].m_ingredient,
                            m_mix[2].m_ingredient, m_mix[3].m_ingredient) != 0;
    if (made && m_inventory) {
      for (unsigned int i = 0; i < 0x48 && m_items[i].m_ingredient; ++i) {
        const Ingredient* ing = m_items[i].m_ingredient;
        if (IngCost(ing) != 0) continue;
        if (PlayerFamilyImpl::InventoryItem* item = GetItemByGuid(m_inventory, IngId(ing), 0)) {
          const int used = ItemCount(item) - static_cast<int>(m_items[i].m_count);
          if (used != 0) RemoveItemsByGuid(m_inventory, IngId(ing), used, 0);
        }
      }
      const int cost = get_total_cost(nullptr);
      SetSimFunds(0, static_cast<int>(GetSimFunds(0)) - cost);
    }
  }
  std::printf("[ing] food creation for player %d %s\n", m_player + 1, made ? "made a dish" : "left without a dish");
  // PC co-op: the answer goes to the sim that opened the screen (ingamecas.cpp), not through the
  // dialog code an object dialog of the other player's could take meanwhile.
  if (m_pcPrivate) PcXamDialogDone(made);
  else cXObjectImpl::overrideDialogReturnCode = made ? 1 : 0;
  UIObjectBase::UIRouter_FlowGoto(const_cast<char*>("ingame"));
  return 1;
}

// 801BB918
// The tab's cells: the edit buffer's ingredients of the tab's type (or its stand-in type), the bought
// ones (a cost) or, for the held tabs, the free ones; at most twelve.
int INGTarget::update_current_items(unsigned int tab) {
  std::memset(m_grid, 0, sizeof m_grid);
  if (tab >= 10 || !m_tabs[tab].m_type) return 0;
  const int type = m_tabs[tab].m_typeId ? m_tabs[tab].m_typeId : TypeId(m_tabs[tab].m_type);
  const bool held = m_tabs[tab].m_held != 0;
  unsigned int cells = 0;
  for (unsigned int i = 0; i < 0x48 && m_items[i].m_ingredient; ++i) {
    if (get_ingredient_type(m_items[i]) != type) continue;
    const bool free = IngCost(m_items[i].m_ingredient) == 0;
    if (held != free) continue;
    m_grid[cells++] = &m_items[i];
    if (cells > 0xb) return 1;
  }
  return 1;
}

// 801BBA58
int INGTarget::update_item_states() {
  for (unsigned int i = 0; i < 12; ++i) set_cell_enabled(i, query_enabled(m_grid[i]));
  return 1;
}

// 801BBAB8
int INGTarget::install_current_item_shaders() {
  for (unsigned int i = 0; i < 12; ++i) {
    IngInfo* info = m_grid[i];
    const unsigned int id = info && info->m_ingredient ? IngIcon(info->m_ingredient) : get_blank_shader_id();
    if (m_installed) UI2D::Instance()->InstallEntry(const_cast<char*>(kGridShaderNames[i]), id, const_cast<char*>(m_name), 0);
  }
  update_item_states();
  return 1;
}

// 801BBB60
INGTarget::IngInfo* INGTarget::get_current_inginfo() {
  if (m_curCell > 0xb) return nullptr;
  IngInfo* info = m_grid[m_curCell];
  return info && info->m_ingredient ? info : nullptr;
}

// 801BBBA8
INGTarget::IngInfo* INGTarget::get_current_mix_inginfo() {
  if (m_curMix > 3) return nullptr;
  return m_mix[m_curMix].m_ingredient ? &m_mix[m_curMix] : nullptr;
}

// 801BBBF0
const Ingredient* INGTarget::get_current_ingredient() {
  IngInfo* info = get_current_inginfo();
  return info ? info->m_ingredient : nullptr;
}

// 801BBC24
// `count` of the ingredient into the first empty mix slot (a held one taken from its count), paid
// for; the slot, or -1.
int INGTarget::add_mix_ing(IngInfo* info, unsigned int count) {
  if (!info || info->m_count < count) return -1;
  for (int i = 0; i < 4; ++i) {
    if (m_mix[i].m_count != 0) continue;
    m_mix[i].m_ingredient = info->m_ingredient;
    m_mix[i].m_count = count;
    if (IngCost(info->m_ingredient) == 0) info->m_count -= count;
    m_funds -= IngCost(info->m_ingredient) * static_cast<int>(m_portions);
    return i;
  }
  return -1;
}

// 801BBCCC
int INGTarget::get_ingredient_type(const IngInfo& info) const {
  if (!info.m_ingredient) return 0;
  const IngredientType* t = IngType(info.m_ingredient);
  return t ? TypeId(t) : 0;
}

// 801BBCF8
// A cell can be picked: empty (true), or enough of it, affordable, within the fridge's level and not
// in the mix yet.
bool INGTarget::query_enabled(const IngInfo* info) const {
  if (!info || !info->m_ingredient) return true;
  const Ingredient* ing = info->m_ingredient;
  if (info->m_count == 0 || info->m_count < m_portions || m_funds < IngCost(ing) * static_cast<int>(m_portions) ||
      INGTarget_m_sRefrigeratorLevel < IngFridgeLevel(ing))
    return false;
  return const_cast<INGTarget*>(this)->get_mix_ingredient(IngId(ing)) == nullptr;
}

// 801BBD84
const FoodResult* INGTarget::mix_ingredients() {
  IngredientManager* mgr = GetIngredientManagerInstance();
  if (!mgr) return nullptr;
  return mgr->MixIngredients(static_cast<short>(get_ingredient_type(m_mix[0])), static_cast<short>(get_ingredient_type(m_mix[1])),
                             static_cast<short>(get_ingredient_type(m_mix[2])), static_cast<short>(get_ingredient_type(m_mix[3])));
}

// 801BBE14
unsigned int INGTarget::get_blank_shader_id() const { return 0x9f93ff74u; }

// 801BBE20
// "name $cost", and " x portions" for a group meal.
int INGTarget::get_mix_ing_name(unsigned int slot, unsigned short* out) const {
  if (!out || slot > 3 || !m_mix[slot].m_ingredient) return 0;
  const Ingredient* ing = m_mix[slot].m_ingredient;
  Copy16(out, ERQuickdata::LocString(Row(ing), 0x10));
  unsigned short money[128] = {};
  UIObjectBase::makeMoneyString(IngCost(ing), money, true);
  CatAscii(out, " ");
  Cat16(out, money);
  if (is_group_meal()) {
    unsigned short n[18] = {};
    UIObjectBase::makeNumberString(static_cast<int>(m_portions), n);
    CatAscii(out, " x ");
    Cat16(out, n);
  }
  return 1;
}

// 801BBF0C
// The mix's cost (and how many slots are used).
int INGTarget::get_total_cost(unsigned int* count) const {
  unsigned int used = 0;
  int total = 0;
  for (const IngInfo& m : m_mix) {
    if (!m.m_count || !m.m_ingredient) continue;
    ++used;
    total += IngCost(m.m_ingredient) * static_cast<int>(m.m_count);
  }
  if (count) *count = used;
  return total;
}

// 801BBF6C
// The ingredients to show: first the family's held ones (inventory subcategory 6), with their counts;
// then every ingredient with a cost the fridge's level allows (1000000 of each) and the free ones the
// family does not hold (none of them); at most 0x48.
int INGTarget::create_edit_buffer() {
  for (IngInfo& i : m_items) i.erase();
  if (!m_inventory) return 0;
  IngredientManager* mgr = GetIngredientManagerInstance();
  if (!mgr) return 0;
  const Ingredient* all[0x48];
  mgr->GetIngredients(all, 0x48);
  unsigned int n = 0;
  for (int i = 0; i < kContainerSize; ++i) {
    PlayerFamilyImpl::InventoryItem* item = GetItemAtIndex(m_inventory, i);
    if (!item || item->m_bytes[3] != 6) continue;
    m_items[n].erase();
    m_items[n].m_ingredient = find_ingredient(all, 0x48, static_cast<short>(item->m_word));
    if (m_items[n].m_ingredient) m_items[n++].m_count = static_cast<unsigned int>(ItemCount(item));
  }
  for (unsigned int k = 0; k < 0x48 && all[k] && n < 0x48; ++k) {
    const Ingredient* ing = all[k];
    if (IngCost(ing) == 0) {
      if (GetItemByGuid(m_inventory, IngId(ing), 0)) continue;
      m_items[n].m_ingredient = ing;
      m_items[n++].m_count = 0;
    } else if (IngFridgeLevel(ing) <= INGTarget_m_sRefrigeratorLevel) {
      m_items[n].m_ingredient = ing;
      m_items[n++].m_count = 1000000;
    }
  }
  return 1;
}

// 801BC114
INGTarget::IngInfo* INGTarget::get_grid_ingredient(short id) {
  for (IngInfo* g : m_grid)
    if (g && g->m_ingredient && IngId(g->m_ingredient) == id) return g;
  return nullptr;
}

// 801BC154
INGTarget::IngInfo* INGTarget::get_mix_ingredient(short id) {
  for (IngInfo& m : m_mix)
    if (m.m_ingredient && IngId(m.m_ingredient) == id) return &m;
  return nullptr;
}

// 801BC190
int INGTarget::set_cell_enabled(unsigned int cell, bool enabled) {
  if (cell >= 12) return 0;
  char n[36];
  std::snprintf(n, sizeof n, "%d", cell + 1);
  if (AptViewer* v = Viewer())
    v->NewCallFunction(const_cast<char*>("SetIngredientState"), nullptr, nullptr, 2, n, const_cast<char*>(enabled ? "1" : "0"));
  return 1;
}

// 801BC228
int INGTarget::set_done_button_state(bool enabled) {
  if (AptViewer* v = Viewer())
    v->NewCallFunction(const_cast<char*>("SetDoneButtonState"), nullptr, nullptr, 1, const_cast<char*>(enabled ? "1" : "0"));
  return 1;
}

// 801BC284
bool INGTarget::update_mix_ing_name(unsigned int slot) {
  if (slot < 4) {
    char n[32];
    std::snprintf(n, sizeof n, "%d", slot + 1);
    if (AptViewer* v = Viewer()) v->NewCallFunction(const_cast<char*>("UpdateMixIngredientName"), nullptr, nullptr, 1, n);
  }
  return slot < 4;
}

namespace {
// cXObjectImpl::SetAttr / GetAttr (800EAF1C / 800EB0FC, the object's attribute array; PC: read here
// as the world side has not ported them).
short GetAttr(cXObjectImpl* o, int i) { return i >= 0 && i < o->m_attrCount && o->m_attrs ? o->m_attrs[i] : 0; }
void SetAttr(cXObjectImpl* o, int i, short v) {
  if (i >= 0 && i < o->m_attrCount && o->m_attrs) o->m_attrs[i] = v;
}
}  // namespace

// 801BC2F4
// The food object `object` becomes the mix's recipe: attribute 0xE the recipe's id; attributes 0..3,
// 7..10 and 0xC start at 0 and add each ingredient's values (+0x18.. +0x3C); 0xF..0x1B (odd) the
// recipe's own values. 0: the mix makes no recipe or there is no object.
int INGTarget::configure_object(short object, const Ingredient* a, const Ingredient* b, const Ingredient* c,
                                const Ingredient* d) {
  IngredientManager* mgr = GetIngredientManagerInstance();
  if (!mgr || !Globs_pObjectModule) return 0;
  const Ingredient* mix[4] = {a, b, c, d};
  short types[4] = {};
  for (int i = 0; i < 4; ++i)
    if (mix[i] && IngType(mix[i])) types[i] = TypeId(IngType(mix[i]));
  const FoodResult* food = mgr->MixIngredients(types[0], types[1], types[2], types[3]);
  if (!food) return 0;
  cXObject* o = Globs_pObjectModule->GetObjectFromID(object);
  cXObjectImpl* impl = o ? o->GetObjectImplementation() : nullptr;
  if (!impl) return 0;
  SetAttr(impl, 0xe, FoodShort(food, 0));
  static const int kCleared[] = {0, 1, 2, 3, 7, 8, 9, 10, 0xc};
  for (int attr : kCleared) SetAttr(impl, attr, 0);
  static const struct {
    unsigned int off;
    int attr;
  } kAdds[] = {{0x18, 0}, {0x1c, 1}, {0x20, 2}, {0x24, 3}, {0x38, 7}, {0x3c, 8}, {0x2c, 9}, {0x34, 10}, {0x30, 0xc}};
  for (const Ingredient* ing : mix) {
    if (!ing) continue;
    for (const auto& add : kAdds) {
      const int v = IngInt(ing, add.off);
      if (v) SetAttr(impl, add.attr, static_cast<short>(GetAttr(impl, add.attr) + v));
    }
  }
  SetAttr(impl, 0xf, FoodShort(food, 10));
  SetAttr(impl, 0x11, FoodShort(food, 11));
  SetAttr(impl, 0x13, FoodShort(food, 12));
  SetAttr(impl, 0x15, FoodShort(food, 13));
  SetAttr(impl, 0x17, FoodShort(food, 14));
  SetAttr(impl, 0x19, FoodShort(food, 15));
  SetAttr(impl, 0x1b, FoodShort(food, 16));
  std::printf("[ing] %s takes recipe %d\n", impl->DebugName(), FoodShort(food, 0));
  return 1;
}

// 801BC938
int INGTarget::play_error_sound() {
  UIAUDIO::Instance()->PlaySound(0x3804219f);
  return 1;
}

// 801BC984
bool INGTarget::is_group_meal() const { return m_mode == 2; }

// 801BC998
int INGTarget::RCPOnCancel() {
  m_recipeRequest = 2;
  return 1;
}

// 801BC9A8
// The help's next page ("ING_ShowHelp" again), after the last back to the first.
void INGTarget::onHelpDialogSelection(int) {
  if (++m_helpPage < 2) SetVariable(const_cast<char*>("ING_ShowHelp"), const_cast<char*>(""));
  else m_helpPage = 0;
}

// 801BCA10
// "Leave?": yes (0) leaves without a dish.
void INGTarget::exit_ingredients_dialog_selection_callback(int selection) {
  if (selection == 0) end_dialog(false);
}
