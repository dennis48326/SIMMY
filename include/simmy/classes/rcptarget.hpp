// @class RCPTarget__Recipe
// @class RCPTarget after UIObjectBase UIDialog HelpDialog cRCPEventHandler RCPTarget__Recipe GoalUnlock
// The recipe book (game/targets_s2c.cpp RCPTarget, screen 0x1A "RCP"), opened from food creation
// (INGTarget "ING_ShowRecipes"): the 24 recipes of sRecipeData, three to a tab, each with its four
// ingredients' icons (the ingredient's, or a question mark while the recipe is locked), how they are
// combined (the plus / process icons by the recipe's process) and a lock icon (locked, or recently
// unlocked). A: an unlocked recipe's name and the dish's hunger / bladder / energy in the help dialog.
// B closes the book (the handler's RCPOnCancel, INGTarget's); ten idle minutes too.
#pragma once

// cRCPEventHandler (the owner told when the book closes): ingtarget.hpp.

// 0x14 bytes (sRecipeData 80439B24): the four ingredient ids (0: none), the process (0..6: how the
// ingredients are combined, which names the dish), +8 locked, +0xC recently unlocked (shown with the
// "new" lock icon), +0x10 the FoodResult the ingredients' types mix to (init_recipes).
struct RCPTarget__Recipe {
  unsigned char m_ingredients[4];
  int m_process;
  int m_locked;
  int m_recent;
  const FoodResult* m_result;
};

// 0x14C bytes.
struct RCPTarget : UIObjectBase {
  using Recipe = RCPTarget__Recipe;

  RCPTarget(cRCPEventHandler* handler, int player);  // 801DCF40
  ~RCPTarget() override;  // 801DD904
  int make_recipe_text(Recipe* recipe, std::vector<unsigned short>& out);  // 801DDBD0
  void SetVariable(char* name, char* value) override;  // 801DDD8C
  char* GetVariable(char* name) override;  // 801DE1E8
  unsigned short* GetLocalizable(char* name) override;  // 801DE32C
  void Update() override;  // 801DE4B4
  void Draw(ERC* rc) override;  // 801DE534
  int init_recipes();  // 801DE538
  int install_tab_shaders();  // 801DE6F4
  int install_lock_shaders();  // 801DE798
  int install_ing_shaders();  // 801DE85C
  int get_plus_shaders(Recipe& recipe, unsigned int& first, unsigned int& second);  // 801DEC18
  int get_proc_shaders(Recipe& recipe, unsigned int& first, unsigned int& second);  // 801DECB4

  int m_player = 0;                     // +0x84
  unsigned int m_filters[2] = {};       // +0x88, +0x8C Apt button filters
  HelpDialog m_help;                    // +0x90 the help page and a recipe's details
  cRCPEventHandler* m_handler = nullptr;  // +0x120
  Recipe* m_recipes = nullptr;          // +0x124 sRecipeData
  unsigned int m_recipeCount = 0;       // +0x128 24
  unsigned int m_tabCount = 0;          // +0x12C RCP_GetTabCount: a tab per three recipes
  unsigned int m_curTab = 0;            // +0x130
  unsigned int m_first = 0;             // +0x134 the tab's first recipe (3 x the tab)
  unsigned int m_selection = 0;         // +0x138 0..2 on the tab
  GoalUnlock m_unlocks;                 // +0x13C the player family's (story family 1) goals and unlocks
  float m_idleTime = 600.0f;            // +0x140 seconds left before the book closes by itself
  int m_timedOut = 0;                   // +0x144
  int m_cancelled = 0;                  // +0x148
};
