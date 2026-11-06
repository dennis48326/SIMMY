// Port of game/targets_s2c.cpp: RCPTarget, the recipe book of food creation (screen 0x1A "RCP"). See
// include/simmy/classes/rcptarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int PcPrivateScreenPlayer();  // uiscreens.cpp (PC co-op private screens)

namespace {

AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }

unsigned int PushFilterAll(int player, const char* name) {  // UIObjectBase::pushAptButtonFilterALL
  AptViewer* v = Viewer();
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}

const unsigned char* Row(const void* p) { return static_cast<const unsigned char*>(p); }
const unsigned short* SlotText(const unsigned short* const* slot) { return slot ? *slot : nullptr; }

std::size_t Len16(const unsigned short* s) {
  std::size_t n = 0;
  while (s && s[n]) ++n;
  return n;
}
void Append16(std::vector<unsigned short>& out, const unsigned short* s) {
  if (!out.empty() && out.back() == 0) out.pop_back();
  if (s) out.insert(out.end(), s, s + Len16(s));
  out.push_back(0);
}
void Append16(std::vector<unsigned short>& out, const char* s) {
  if (!out.empty() && out.back() == 0) out.pop_back();
  while (s && *s) out.push_back(static_cast<unsigned char>(*s++));
  out.push_back(0);
}

constexpr unsigned int kBlankShader = 0x9f93ff74u;

// 80439B24: the recipes (ingredient ids, process); locked until unlocked (init_recipes).
RCPTarget__Recipe sRecipeData[0x18] = {
    {{2, 0, 0, 0}, 3, 1, 0, nullptr},     {{2, 32, 26, 0}, 1, 1, 0, nullptr},   {{30, 48, 57, 0}, 4, 1, 0, nullptr},
    {{50, 23, 0, 0}, 6, 1, 0, nullptr},   {{51, 62, 0, 0}, 2, 1, 0, nullptr},   {{35, 65, 0, 0}, 1, 1, 0, nullptr},
    {{13, 23, 43, 56}, 1, 1, 0, nullptr}, {{10, 48, 61, 0}, 1, 1, 0, nullptr},  {{7, 44, 63, 0}, 1, 1, 0, nullptr},
    {{14, 17, 19, 0}, 5, 1, 0, nullptr},  {{17, 24, 55, 61}, 4, 1, 0, nullptr}, {{15, 27, 59, 0}, 5, 1, 0, nullptr},
    {{4, 20, 24, 37}, 4, 1, 0, nullptr},  {{2, 26, 47, 63}, 3, 1, 0, nullptr},  {{19, 35, 61, 0}, 6, 1, 0, nullptr},
    {{2, 18, 24, 38}, 5, 1, 0, nullptr},  {{7, 32, 53, 0}, 4, 1, 0, nullptr},   {{52, 23, 17, 0}, 4, 1, 0, nullptr},
    {{23, 59, 63, 0}, 2, 1, 0, nullptr},  {{20, 34, 0, 0}, 6, 1, 0, nullptr},   {{2, 18, 24, 64}, 3, 1, 0, nullptr},
    {{12, 48, 65, 0}, 2, 1, 0, nullptr},  {{22, 27, 30, 59}, 5, 1, 0, nullptr}, {{3, 22, 41, 55}, 5, 1, 0, nullptr},
};

// 803F7BFC: per tab its icon and its selected icon.
const unsigned int sTabShaderIDs[8][2] = {{0xb7a62aad, 0x2e7f88d7}, {0x2eaf7b17, 0x3cca2739}, {0x59a84b81, 0x8476405c},
                                          {0xc7ccde22, 0x19a178e5}, {0xb0cbeeb4, 0xa11d1f80}, {0x29c2bf0e, 0xb3a8b06e},
                                          {0x5ec58f98, 0x0b14d70b}, {0xce7a9209, 0x5377c75d}};

// 803F7CBC .. 803F7FD0: the screen's shader names.
const char* const RCP_TabIconNames[8] = {"RCP_TabIcon0", "RCP_TabIcon1", "RCP_TabIcon2", "RCP_TabIcon3",
                                         "RCP_TabIcon4", "RCP_TabIcon5", "RCP_TabIcon6", "RCP_TabIcon7"};
const char* const RCP_ItemLockedIconNames[3] = {"RCP_ItemLocked0", "RCP_ItemLocked1", "RCP_ItemLocked2"};
const char* const RCP_ItemIngIconNames[12] = {"RCP_ItemIng00", "RCP_ItemIng10", "RCP_ItemIng20", "RCP_ItemIng30",
                                              "RCP_ItemIng01", "RCP_ItemIng11", "RCP_ItemIng21", "RCP_ItemIng31",
                                              "RCP_ItemIng02", "RCP_ItemIng12", "RCP_ItemIng22", "RCP_ItemIng32"};
const char* const RCP_ItemBackIconNames[12] = {"RCP_ItemBack00", "RCP_ItemBack10", "RCP_ItemBack20", "RCP_ItemBack30",
                                               "RCP_ItemBack01", "RCP_ItemBack11", "RCP_ItemBack21", "RCP_ItemBack31",
                                               "RCP_ItemBack02", "RCP_ItemBack12", "RCP_ItemBack22", "RCP_ItemBack32"};
const char* const RCP_ItemPlusIconNames[6] = {"RCP_ItemPlus00", "RCP_ItemPlus10", "RCP_ItemPlus01",
                                              "RCP_ItemPlus11", "RCP_ItemPlus02", "RCP_ItemPlus12"};
const char* const RCP_ItemProcIconNames[6] = {"RCP_ItemProc00", "RCP_ItemProc10", "RCP_ItemProc01",
                                              "RCP_ItemProc11", "RCP_ItemProc02", "RCP_ItemProc12"};

// 801DCE90 GetRecipeName
// The dish's name for the recipe's process (FoodResult +0x24.. one name per process).
const unsigned short* RecipeName(const RCPTarget__Recipe* recipe) {
  if (!recipe || !recipe->m_result) return nullptr;
  if (recipe->m_process < 0 || recipe->m_process > 6) return nullptr;
  return ERQuickdata::LocString(Row(recipe->m_result), 0x24 + 4 * recipe->m_process);
}

// 801DDA9C sum_attribute
// The sum of one attribute over the recipe's ingredients (0 hunger, 1 energy, 2 comfort, 3 bladder,
// 7, 8, 9, 10, 12: the others).
int SumAttribute(int attribute, const RCPTarget__Recipe* recipe) {
  IngredientManager* mgr = GetIngredientManagerInstance();
  if (!recipe || !mgr) return 0;
  int offset = -1;
  switch (attribute) {
    case 0: offset = 0x18; break;
    case 1: offset = 0x1c; break;
    case 2: offset = 0x20; break;
    case 3: offset = 0x24; break;
    case 7: offset = 0x38; break;
    case 8: offset = 0x3c; break;
    case 9: offset = 0x2c; break;
    case 10: offset = 0x34; break;
    case 12: offset = 0x30; break;
    default: return 0;
  }
  int sum = 0;
  for (unsigned int i = 0; i < 4; ++i) {
    if (!recipe->m_ingredients[i]) continue;
    if (const Ingredient* ing = mgr->GetIngredient(recipe->m_ingredients[i])) sum += ERQuickdata::S32(Row(ing), offset);
  }
  return sum;
}

}  // namespace

// 801DCF40
// For `player`: the commands, the screen's shaders (all blank), sounds and button filters (both
// players' in two-player games), the recipes (init_recipes), then the screen.
RCPTarget::RCPTarget(cRCPEventHandler* handler, int player) : m_player(player), m_handler(handler) {
  AttachTables("RCPTarget");
  m_recipes = sRecipeData;
  m_recipeCount = 0x18;
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  // GC: both players' pads (DAT_80475C70: two players). PC co-op: opened from a player's own food
  // creation, only theirs (uiscreens.cpp, PcCoopPrivateScreens).
  if (PcPrivateScreenPlayer() >= 0 && PcPrivateScreenPlayer() == m_player) {
    simmy::coop_set_private_screen("RCP", m_player);
    m_filters[m_player & 1] = PushFilterAll(m_player & 1, "RCPTarget");
  } else {
    m_filters[0] = PushFilterAll(0, "RCPTarget");
    m_filters[1] = _gd.m_fieldEC ? PushFilterAll(1, "RCPTarget") : 0;
  }
  init_recipes();
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* n) { return GetLocalizable(n); };
  for (const char* n : {"RCP_TabsReady", "RCP_TabChanged", "RCP_SelectionChanged", "RCP_Accept", "RCP_Cancel", "RCP_ShowHelp"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"RCP_exists", "RCP_CancelEnabled", "RCP_GetTabCount"}) m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"RCP_DialogName", "RCP_RecipeName0", "RCP_RecipeName1", "RCP_RecipeName2", "RCP_RecipeNum0",
                        "RCP_RecipeNum1", "RCP_RecipeNum2"})
    m_locTable->InstallCommand(n, loc, this);
  UI2D* ui2d = UI2D::Instance();
  auto blank = [&](const char* name) { ui2d->InstallEntry(const_cast<char*>(name), kBlankShader, const_cast<char*>(m_name), 0); };
  for (const char* n : RCP_TabIconNames) blank(n);
  for (const char* n : RCP_ItemLockedIconNames) blank(n);
  for (const char* n : RCP_ItemIngIconNames) blank(n);
  for (const char* n : RCP_ItemBackIconNames) blank(n);
  for (const char* n : RCP_ItemPlusIconNames) blank(n);
  for (const char* n : RCP_ItemProcIconNames) blank(n);
  UIAUDIO* audio = UIAUDIO::Instance();
  const struct {
    const char* name;
    unsigned int id;
  } sounds[] = {{"RCP_SndTabChanged", 0x649e20f6}, {"RCP_SndBrowse", 0x6a7a1517}, {"RCP_SndError", 0x3804219f},
                {"RCP_SndSelect", 0x5d5565e8},     {"RCP_SndCancel", 0x76d81c2a}};
  for (const auto& s : sounds) audio->InstallEntry(const_cast<char*>(s.name), s.id, const_cast<char*>(m_name));
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x1a, m_player);
  std::printf("[rcp] recipe book for player %d: %u recipes\n", m_player + 1, m_recipeCount);
}

// 801DD904
RCPTarget::~RCPTarget() {
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
  if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x1a);
  if (AptViewer* v = Viewer()) {
    if (m_filters[0]) v->PopAptButtonFilter(0, m_filters[0]);
    if (m_filters[1]) v->PopAptButtonFilter(1, m_filters[1]);
  }
  m_filters[0] = m_filters[1] = 0;
  simmy::coop_set_private_screen("RCP", -1);
}

// 801DDBD0
// "hunger: n\nbladder: n\nenergy: n" over the recipe's ingredients. 0 without a dish.
int RCPTarget::make_recipe_text(Recipe* recipe, std::vector<unsigned short>& out) {
  out.assign(1, 0);
  if (!recipe || !recipe->m_result) return 0;
  const char* names[3] = {"hunger", "bladder", "energy"};
  const int attributes[3] = {0, 3, 1};
  for (int i = 0; i < 3; ++i) {
    unsigned short number[0x100] = {};
    Append16(out, SlotText(_globals.GetNewUIString(names[i])));
    Append16(out, ": ");  // 803F5254
    UIObjectBase::makeNumberString(SumAttribute(attributes[i], recipe), number);
    Append16(out, number);
    if (i < 2) Append16(out, "\n");  // 803F525C
  }
  return 1;
}

// 801DDD8C
// "RCP_Cancel" closes the book (not after the idle time); "RCP_TabChanged" the tab (its recipes'
// icons); "RCP_SelectionChanged" the recipe on the tab (a recently unlocked one is seen: no longer
// new); "RCP_Accept" an unlocked recipe's name and its dish's motives in the dialog box (a locked one:
// the error sound); "RCP_ShowHelp" the help page. Every command but Cancel restarts the idle time.
void RCPTarget::SetVariable(char* name, char* value) {
  if (!std::strcmp("RCP_Cancel", name)) {
    if (!m_timedOut) {
      m_cancelled = 1;
      if (m_handler) m_handler->RCPOnCancel();
    }
    return;
  }
  if (!std::strcmp("RCP_TabsReady", name)) {
    // GC: nothing to do here (the tab's icons come with RCP_TabChanged).
  } else if (!std::strcmp("RCP_TabChanged", name)) {
    if (!m_tabCount) return;
    unsigned int tab = static_cast<unsigned int>(std::atoi(value));
    if (tab >= m_tabCount) tab = m_tabCount - 1;
    m_curTab = tab;
    m_first = tab * 3;
    install_tab_shaders();
    install_lock_shaders();
    install_ing_shaders();
  } else if (!std::strcmp("RCP_SelectionChanged", name)) {
    if (!m_recipeCount) return;
    m_selection = static_cast<unsigned int>(std::atoi(value));
    if (m_selection > 2) m_selection = 2;
    const unsigned int index = m_selection + m_first;
    if (index < m_recipeCount && !m_recipes[index].m_locked && m_recipes[index].m_recent && m_unlocks.m_bits)
      m_unlocks.SetRecentlyUnlocked(IGoalUnlock__UnlockType{0}, static_cast<short>(index + 1), false);  // vtable 0x4C
  } else if (!std::strcmp("RCP_Accept", name)) {
    const unsigned int index = m_first + m_selection;
    if (index >= m_recipeCount) return;
    Recipe* recipe = &m_recipes[index];
    if (recipe->m_locked) {
      UIAUDIO::Instance()->PlaySound(0x3804219f);
      return;
    }
    m_help.m_preselect = 0;
    m_help.m_title.assign(1, 0);
    Append16(m_help.m_title, RecipeName(recipe));
    make_recipe_text(recipe, m_help.m_body);
    m_help.m_numSelections = 1;
    m_help.m_field14 = 1;
    m_help.m_sel[0].assign(1, 0);
    Append16(m_help.m_sel[0], SlotText(_globals.GetNewUIString("ok")));  // 803EF83C
    HelpDialog* help = &m_help;
    m_help.m_onSelection = [help](int sel) { help->SelectionCallback(sel); };
    m_help.m_onZeroInput = [help] { help->ZeroInputCallback(); };
    m_help.m_onClosed = [help] { help->DialogClosedCallback(); };
    m_help.m_field24 = 0;
    m_help.SpawnDialog();
  } else if (!std::strcmp("RCP_ShowHelp", name)) {
    if (!m_cancelled) m_help.SpawnHelpDialog(m_player, "fc_recipe_help", nullptr, false, nullptr);
    return;
  } else {
    return;
  }
  m_idleTime = 600.0f;
}

// 801DE1E8
// "RCP_GetTabCount" (a tab per three recipes, at least one), "RCP_CancelEnabled", "RCP_exists".
char* RCPTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  if (!out) return nullptr;
  *out = 0;
  if (!std::strcmp("RCP_GetTabCount", name)) {
    m_tabCount = m_recipeCount / 3;
    if (m_recipeCount != m_tabCount * 3) ++m_tabCount;
    if (!m_tabCount) m_tabCount = 1;
    std::snprintf(out, 0x20, "%d", m_tabCount);
  } else if (!std::strcmp("RCP_CancelEnabled", name)) {
    std::snprintf(out, 0x20, "%d", m_timedOut == 0);
  } else if (!std::strcmp("RCP_exists", name)) {
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801DE32C
// "RCP_RecipeNum<n>" the tab's n-th recipe's number (1-based); "RCP_RecipeName<n>" its dish's name;
// "RCP_DialogName" the book's title.
unsigned short* RCPTarget::GetLocalizable(char* name) {
  static unsigned short buf[0x1000];
  std::memset(buf, 0, sizeof buf);
  const unsigned short* text = nullptr;
  if (!std::strncmp(name, "RCP_RecipeNum", 0xd)) {
    const unsigned int n = static_cast<unsigned int>(std::atoi(name + 0xd));
    if (n > 2 || m_first + n >= m_recipeCount) return nullptr;
    UIObjectBase::makeNumberString(static_cast<int>(m_first + n + 1), buf);
  } else if (!std::strncmp(name, "RCP_RecipeName", 0xe)) {
    const unsigned int n = static_cast<unsigned int>(std::atoi(name + 0xe));
    if (n >= 3 || m_first + n >= m_recipeCount) return nullptr;
    text = RecipeName(&m_recipes[m_first + n]);
    if (!text) return nullptr;
  } else if (!std::strcmp("RCP_DialogName", name)) {
    text = SlotText(_globals.GetPDAString("fc_recipe_book_title"));
    if (!text) return nullptr;  // GC: copies a null string (an empty one)
  } else {
    return nullptr;
  }
  if (text) {
    const std::size_t n = Len16(text) < 0xfff ? Len16(text) : 0xfff;
    std::memcpy(buf, text, n * 2);
  }
  UIQDTarget::UIQDSubstitutionStrings(buf);
  const std::size_t n = Len16(buf);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, buf, (n + 1) * 2);
  return out;
}

// 801DE4B4
// Ten idle minutes close the book.
void RCPTarget::Update() {
  if (m_timedOut || !(m_idleTime > 0.0f)) return;
  m_idleTime -= _dt;
  if (m_idleTime <= 0.0f) {
    m_timedOut = 1;
    if (m_handler) m_handler->RCPOnCancel();
  }
}

// 801DE534
void RCPTarget::Draw(ERC*) {}

// 801DE538
// Each recipe: locked unless the player family (family 1, the story family) has it unlocked (recipe
// unlocks are type 0, numbered from 1), recently unlocked, and the dish its ingredients' types mix to.
int RCPTarget::init_recipes() {
  if (!Globs_pNeighborhood || !m_recipeCount) return 0;
  IngredientManager* mgr = GetIngredientManagerInstance();
  if (!mgr) return 0;
  m_unlocks = FamilyGoalUnlock(1);  // GetFamily(1) vtable 0x124 -> GetGoalAndUnlockData (0x14C)
  if (!m_unlocks.m_bits) return 0;
  for (unsigned int i = 0; i < m_recipeCount; ++i) {
    Recipe& r = m_recipes[i];
    const IGoalUnlock__UnlockType recipes{0};
    r.m_locked = m_unlocks.IsUnlocked(recipes, static_cast<short>(i + 1)) ^ 1;  // vtable 0x2C
    // PC development aid: SIMMY_RECIPES_UNLOCKED=1 shows every recipe as unlocked (the family's unlocks
    // are not changed), =2 also as recently unlocked.
    const char* devUnlocked = std::getenv("SIMMY_RECIPES_UNLOCKED");
    if (devUnlocked) r.m_locked = 0;
    if (!r.m_locked) r.m_recent = m_unlocks.IsRecentlyUnlocked(recipes, static_cast<short>(i + 1));  // vtable 0x44
    if (devUnlocked && std::atoi(devUnlocked) == 2) r.m_recent = 1;
    short types[4];
    for (unsigned int j = 0; j < 4; ++j) {
      const Ingredient* ing = r.m_ingredients[j] ? mgr->GetIngredient(r.m_ingredients[j]) : nullptr;
      const void* type = ing ? ERQuickdata::Ptr(Row(ing), 4) : nullptr;
      types[j] = type ? ERQuickdata::S16(Row(type), 0) : 0;
    }
    r.m_result = mgr->MixIngredients(types[0], types[1], types[2], types[3]);
  }
  return 1;
}

// 801DE6F4
// The tabs' icons, the current one selected.
int RCPTarget::install_tab_shaders() {
  UI2D* ui2d = UI2D::Instance();
  for (unsigned int i = 0; i < m_tabCount && i < 8; ++i)
    if (m_installed && ui2d)
      ui2d->InstallEntry(const_cast<char*>(RCP_TabIconNames[i]), sTabShaderIDs[i][i == m_curTab ? 1 : 0],
                         const_cast<char*>(m_name), 0);
  return 1;
}

// 801DE798
// The tab's three lock icons: locked, recently unlocked, else none.
int RCPTarget::install_lock_shaders() {
  UI2D* ui2d = UI2D::Instance();
  for (unsigned int i = 0; i < 3; ++i) {
    unsigned int id = kBlankShader;
    if (m_first + i < m_recipeCount) {
      const Recipe& r = m_recipes[m_first + i];
      if (r.m_locked) id = 0x88ee512f;
      else if (r.m_recent) id = 0x332085a9;
    }
    if (m_installed && ui2d) ui2d->InstallEntry(const_cast<char*>(RCP_ItemLockedIconNames[i]), id, const_cast<char*>(m_name), 0);
  }
  return 1;
}

// 801DE85C
// The tab's three recipes: per ingredient a backing and its icon (a question mark while locked), the
// two plus icons and the two process icons; empty rows blank.
int RCPTarget::install_ing_shaders() {
  IngredientManager* mgr = GetIngredientManagerInstance();
  if (!mgr) return 0;
  UI2D* ui2d = UI2D::Instance();
  auto install = [&](const char* name, unsigned int id) {
    if (m_installed && ui2d) ui2d->InstallEntry(const_cast<char*>(name), id, const_cast<char*>(m_name), 0);
  };
  unsigned int slot = 0;
  for (unsigned int row = 0, index = m_first; slot < 12; ++row, ++index) {
    if (index < m_recipeCount) {
      Recipe& r = m_recipes[index];
      for (unsigned int j = 0; j < 4; ++j, ++slot) {
        const unsigned char ing = r.m_ingredients[j];
        install(RCP_ItemBackIconNames[slot], ing ? 0x1be5dbc9u : kBlankShader);
        unsigned int icon = kBlankShader;
        if (ing) {
          if (r.m_locked) {
            icon = 0x2ee2d2eau;
          } else if (const Ingredient* i = mgr->GetIngredient(ing)) {
            icon = ERQuickdata::U32(Row(i), 8);
          }
        }
        install(RCP_ItemIngIconNames[slot], icon);
      }
      unsigned int a = 0, b = 0;
      get_plus_shaders(r, a, b);
      install(RCP_ItemPlusIconNames[row * 2], a);
      install(RCP_ItemPlusIconNames[row * 2 + 1], b);
      get_proc_shaders(r, a, b);
      install(RCP_ItemProcIconNames[row * 2], a);
      install(RCP_ItemProcIconNames[row * 2 + 1], b);
    } else {
      for (unsigned int j = 0; j < 4; ++j, ++slot) {
        install(RCP_ItemBackIconNames[slot], kBlankShader);
        install(RCP_ItemIngIconNames[slot], kBlankShader);
      }
      install(RCP_ItemPlusIconNames[row * 2], kBlankShader);
      install(RCP_ItemPlusIconNames[row * 2 + 1], kBlankShader);
      install(RCP_ItemProcIconNames[row * 2], kBlankShader);
      install(RCP_ItemProcIconNames[row * 2 + 1], kBlankShader);
    }
  }
  return 1;
}

// 801DEC18
// The plus icons between the ingredients by the process: 0 none, 4 / 5 both, 1..3 / 6 the first.
int RCPTarget::get_plus_shaders(Recipe& recipe, unsigned int& first, unsigned int& second) {
  switch (recipe.m_process) {
    case 0:
      first = second = kBlankShader;
      return 1;
    case 4:
    case 5:
      first = second = 0x9f932ad0u;
      return 1;
    case 1: case 2: case 3: case 6:
      first = 0x9f932ad0u;
      second = kBlankShader;
      return 1;
    default:
      first = second = kBlankShader;
      return 0;
  }
}

// 801DECB4
// The process icons (how the ingredients are cooked) by the process.
int RCPTarget::get_proc_shaders(Recipe& recipe, unsigned int& first, unsigned int& second) {
  switch (recipe.m_process) {
    case 0: first = second = kBlankShader; return 1;
    case 1: first = 0xff68c9a3u; second = kBlankShader; return 1;
    case 2: first = 0xbd9374a1u; second = kBlankShader; return 1;
    case 3: first = 0x0598fca0u; second = kBlankShader; return 1;
    case 4: first = 0xff68c9a3u; second = 0x0598fca0u; return 1;
    case 5: first = 0xbd9374a1u; second = 0x0598fca0u; return 1;
    case 6: first = 0xa34d844bu; second = kBlankShader; return 1;
    default: first = second = kBlankShader; return 0;
  }
}
