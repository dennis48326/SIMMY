// @class cRCPEventHandler
// @class INGTarget__IngInfo
// @class INGTarget__TabInfo
// @class IngredientManager
// @class INGTarget after UIObjectBase HelpDialog INGTarget__IngInfo INGTarget__TabInfo cRCPEventHandler
// Food creation (game/targets_s2c.cpp INGTarget, screen 0x19 "ING", s2c_ing_foodcreation; flow 0x19
// "MixIngredients"): a fridge's "Get Ingredients" (or "Group Meal") runs the generic sim call 0x27
// (GameData::GotoXamMode, modes 0xC / 0xD). The screen shows ten tabs of ingredient types, each a grid
// of up to twelve ingredients (those to buy, priced, or those the family holds, counted), and four mix
// slots. A dish is made when the mix's ingredient types match a recipe (IngredientManager::MixIngredients,
// the "FoodResult" table). Done: the food object (the call's temp 2) takes the recipe and the
// ingredients' attributes (configure_object), the held ingredients are used up, the cost is paid, and
// the tree hears 1 (cXObjectImpl::overrideDialogReturnCode); cancelled: 0.
//
// The ingredient tables are rows of quickdata 0xc8c09e94 (EGlobal +0xF8): "IngredientType" (0x14 bytes),
// "Ingredient" (0x50) and "FoodResult" (0x48). Ingredient, IngredientType and FoodResult stay opaque: a
// pointer to one is its row (big-endian, self-relative pointers; ERQuickdata's accessors).
#pragma once

// The recipe book's owner, told when the book closes (INGTarget +0x84; vtable 0xC RCPOnCancel).
struct cRCPEventHandler {
  virtual ~cRCPEventHandler() = default;
  virtual int RCPOnCancel() = 0;
};

// 0x8 bytes: an ingredient and how many there are (1000000 for one bought from the fridge).
struct INGTarget__IngInfo {
  INGTarget__IngInfo();  // 801B9434
  void erase();  // 801B9448
  const Ingredient* m_ingredient = nullptr;  // +0
  unsigned int m_count = 0;                  // +4
};

// 0xC bytes: a tab's ingredient type; +4 the type id the tab lists instead (0: the type's own);
// +8 the tab lists what the family holds (free) instead of what is bought.
struct INGTarget__TabInfo {
  INGTarget__TabInfo();  // 801B941C
  const IngredientType* m_type = nullptr;  // +0
  short m_typeId = 0;                      // +4
  int m_held = 0;                          // +8
};

// The ingredient tables (game/inventory.cpp), 1 byte.
struct IngredientManager {
  IngredientManager();  // 8004B29C
  ~IngredientManager();  // 8004B308
  bool GetIngredientTypes(const IngredientType** out, unsigned int max) const;  // 8004B330
  bool GetIngredientsByType(short type, const Ingredient** out, unsigned int max) const;  // 8004B3E8
  bool GetIngredients(const Ingredient** out, unsigned int max) const;  // 8004B4D0
  const Ingredient* GetIngredient(short id) const;  // 8004B6E4
  const FoodResult* GetFoodResult(short id) const;  // 8004B758
  const FoodResult* MixIngredients(short a, short b, short c, short d) const;  // 8004B7C8
};
IngredientManager* GetIngredientManagerInstance();  // 8004B260 GetIngredientManager

// 0x468 bytes.
struct INGTarget : UIObjectBase, cRCPEventHandler {  // +0x84 the recipe book's handler
  using IngInfo = INGTarget__IngInfo;
  using TabInfo = INGTarget__TabInfo;

  static int SetRefrigeratorLevel(int level);  // 801B9458 (gamedata.cpp)
  INGTarget();  // 801B9468
  ~INGTarget() override;  // 801BA500
  void SetVariable(char* name, char* value) override;  // 801BA6A8
  char* GetVariable(char* name) override;  // 801BAF84
  unsigned short* GetLocalizable(char* name) override;  // 801BB068
  void Update() override;  // 801BB6C0
  int end_dialog(bool made);  // 801BB7DC
  int update_current_items(unsigned int tab);  // 801BB918
  int update_item_states();  // 801BBA58
  int install_current_item_shaders();  // 801BBAB8
  IngInfo* get_current_inginfo();  // 801BBB60
  IngInfo* get_current_mix_inginfo();  // 801BBBA8
  const Ingredient* get_current_ingredient();  // 801BBBF0
  int add_mix_ing(IngInfo* info, unsigned int count);  // 801BBC24
  int get_ingredient_type(const IngInfo& info) const;  // 801BBCCC
  bool query_enabled(const IngInfo* info) const;  // 801BBCF8
  const FoodResult* mix_ingredients();  // 801BBD84
  unsigned int get_blank_shader_id() const;  // 801BBE14
  int get_mix_ing_name(unsigned int slot, unsigned short* out) const;  // 801BBE20
  int get_total_cost(unsigned int* count) const;  // 801BBF0C
  int create_edit_buffer();  // 801BBF6C
  IngInfo* get_grid_ingredient(short id);  // 801BC114
  IngInfo* get_mix_ingredient(short id);  // 801BC154
  int set_cell_enabled(unsigned int cell, bool enabled);  // 801BC190
  int set_done_button_state(bool enabled);  // 801BC228
  bool update_mix_ing_name(unsigned int slot);  // 801BC284
  int configure_object(short object, const Ingredient* a, const Ingredient* b, const Ingredient* c,
                       const Ingredient* d);  // 801BC2F4
  int play_error_sound();  // 801BC938
  bool is_group_meal() const;  // 801BC984
  int RCPOnCancel() override;  // 801BC998
  void onHelpDialogSelection(int selection);  // 801BC9A8
  void exit_ingredients_dialog_selection_callback(int selection);  // 801BCA10

  int m_mode = 1;                 // +0x8C ING_ModeP1/P2: 1 a meal, 2 a group meal
  int m_player = 0;               // +0x94 PlayerNumber
  int m_object = 0;               // +0x98 INV_LotNumber: the food object's id (the call's temp 2)
  unsigned int m_filters[2] = {};  // +0x9C, +0xA0 Apt button filters
  HelpDialog m_help;              // +0xA4 the help pages and the exit prompt
  unsigned int m_helpPage = 0;    // +0x134
  TabInfo m_tabs[10];             // +0x138
  IngInfo* m_grid[12] = {};       // +0x1B0 the current tab's cells (into m_items)
  IngInfo m_mix[4];               // +0x1E0 the mix slots
  PlayerFamilyImpl* m_inventory = nullptr;  // +0x200 the player family's BBI::InventoryItems
  IngInfo m_items[0x48];          // +0x204 the family's ingredients, then the fridge's
  int m_funds = 0;                // +0x444 the funds left after the mix
  unsigned int m_curTab = 0;      // +0x448
  unsigned int m_curCell = 0;     // +0x44C
  unsigned int m_curMix = 0;      // +0x450
  unsigned int m_portions = 1;    // +0x454 1, 3 for a group meal
  int m_recipeRequest = 0;        // +0x458 1 open the recipe book (RCPTarget), 2 close it
  UIObjectBase* m_recipes = nullptr;  // +0x45C RCPTarget
  float m_idleTime = 600.0f;      // +0x460 seconds left before the screen closes by itself
  int m_timedOut = 0;             // +0x464
  bool m_pcPrivate = false;       // PC co-op: the screen is this player's own (uiscreens.cpp)
};
