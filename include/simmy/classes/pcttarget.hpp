// @class PCTTarget__ePCTMode
// @class PCTTarget__ePCTType
// @class PCTTarget__PCTTabInfo
// @class PCTTarget__cCellInfo after PCTTarget__ePCTType InteractorModule__Interactor__CallbackData
// @class PCTTarget after UIObjectBase UIDialog HelpDialog BBHTarget PCTTarget__cCellInfo PCTTarget__PCTTabInfo GoalUnlock InteractorModule__InteractorInfo
// The pause menu's buy and build catalogue (game/targets_s2c.cpp PCTTarget, screen 0x1E "PCT"; flows
// 0x24 "PAUSE_BUY" (mode 0) and 0x25 "PAUSE_BUILD" (mode 1) from the pause menu's fifth and sixth tabs,
// 0x2A/0x2B the tombstone (mode 2)). A row of tabs (sPCT_TabInfo_Buy: seating, appliances, surfaces,
// plumbing, electronics, lighting, decorative, skill, misc; sPCT_TabInfo_Build: walls, wallpaper, floors,
// doors, windows, fireplaces, landscaping), each a grid of 2 x 6 cells (scrolled a column of six at a
// time) of the catalogue entries the tab's filter takes (object selectors by their function flags or
// build sub-sort, or the wall, wallpaper and floor sets), sorted by price. The selected cell's name,
// price, description, its three best motive ratings, whether the family can afford it (or it is locked:
// an unlock not yet earned, with its aspiration price), the family's money and aspiration points.
// A ("PCT_Accept") on an affordable entry hides the catalogue and starts placing it
// (enter_place_mode: the player's place manipulator, interactor 6); the manipulator's callback
// (PlaceModeCallback) takes the price from the family's funds, then grab mode (enter_tool_mode 1: the
// grab manipulator, interactor 2; GrabManipulatorCallback: the sell/delete dialogs) until B brings the
// catalogue back. The build/buy HUD (BBHTarget) shows the mode, price and money meanwhile.
// PC: not ported: the interactor manager (placemanipulator.cpp runs the manipulators per player), the
// build tools (wall manipulator, wall and floor painters), the 3D object preview (UI3D), the build tabs'
// wall, wallpaper and floor sets, the grab-mode action menu.
#pragma once

#include <vector>

struct PCTTarget__ePCTMode {
  SIMMY_ENUM_BODY(PCTTarget__ePCTMode)
};
// A catalogue entry's kind: 1 object, 2 wall/fence, 3 wallpaper, 4 floor.
struct PCTTarget__ePCTType {
  SIMMY_ENUM_BODY(PCTTarget__ePCTType)
};

// A tab (0x14 bytes, sPCT_TabInfo_Buy 803F783C / sPCT_TabInfo_Build 803F7968).
struct PCTTarget__PCTTabInfo {
  int type;                    // +0x00 ePCTType
  short subsort;               // +0x04 objects: the definition's build sub-sort (+0x8A), 0 none
  unsigned short functions;    // +0x06 objects: function flags (definition +0x50)
  unsigned int icon;           // +0x08 the tab icon's shader
  unsigned int selectedIcon;   // +0x0C lit
  const char* name;            // +0x10 its PDA string
};

// The catalogue's FenceData (GC: the "FenceSet" quickdata rows: +0x00 the shader, +0x04 the price, +0x08
// the name, +0x0C the wall style; and on_tab_changed_wall's static wall builder entry: shader 0xDF1A8E83,
// price 1, "inv_wall_catalog_name", style -1). PC: a native copy of each.
struct PCTFenceData {
  unsigned int shader = 0;                     // +0x00
  int price = 0;                               // +0x04
  const unsigned short* const* name = nullptr; // +0x08
  int style = -1;                              // +0x0C
  unsigned short field10 = 0xffff;             // +0x10
};

// A catalogue cell (0x18 bytes).
struct PCTTarget__cCellInfo {
  PCTTarget__cCellInfo();  // 801D1F30
  // An object: its GUID, thumbnail (the definition's ResData +0x1C), locked (an unlock not yet
  // granted, unless the unlock-all cheat is on) and new (recently unlocked). 1: it has a thumbnail.
  int Init(ObjSelector* selector, GoalUnlock* unlocks);  // 801D1F54
  // Walls and fences (type 2), wallpaper (3), floors (4): the entry's index (-1 the wall builder) and
  // data (a FenceData, a "WallSet" / "FloorSet" row); its shader the thumbnail (or 0xD59C7BB5).
  int Init(int id, const PCTFenceData* fence);  // 801D2074
  int Init(int id, const WallTile* row);  // 801D20B8
  int Init(int id, const FloorTile* row);  // 801D20FC
  int GetPrice() const;  // 801D2140
  int GetName(unsigned short* out) const;  // 801D21F4
  int GetDesc(unsigned short* out) const;  // 801D22FC

  int m_type = 0;                  // +0x00 ePCTType
  unsigned int m_id = 0;           // +0x04 an object's GUID, else its index
  unsigned int m_shader = 0;       // +0x08 the thumbnail
  int m_locked = 0;                // +0x0C
  int m_new = 0;                   // +0x10
  union {
    ObjSelector* m_selector = nullptr;  // +0x14 an object's selector
    const void* m_data;                 // walls: a PCTFenceData; wallpaper / floors: their quickdata row
  };
};

// 0x244 bytes.
struct PCTTarget : UIObjectBase {
  static void static_GrabManipulatorCallback(InteractorModule__Interactor__CallbackData *);  // 801D1E40
  static void static_PlaceModeCallback(InteractorModule__Interactor__CallbackData *);  // 801D1E70
  static int static_WallManipulatorCallback(InteractorModule__Interactor__CallbackData* data);  // 801D1EA0
  static int static_WallPainterCallback(InteractorModule__Interactor__CallbackData* data);  // 801D1ED0
  static int static_FloorPainterCallback(InteractorModule__Interactor__CallbackData* data);  // 801D1F00
  PCTTarget(PCTTarget__ePCTMode mode, int player);  // 801D2388
  ~PCTTarget() override;  // 801D3334
  void SetVariable(char* name, char* value) override;  // 801D3618
  char* GetVariable(char* name) override;  // 801D3D68
  unsigned short* GetLocalizable(char* name) override;  // 801D4170
  void Update() override;  // 801D464C
  void Draw(ERC* rc) override;  // 801D49A4
  unsigned int get_cell_info_array_index(unsigned int cell) const;  // 801D49D0
  PCTTarget__cCellInfo* get_cell_info_at_gridindex(unsigned int cell) const;  // 801D49E0
  int on_tab_changed();  // 801D4A2C
  int on_tab_changed_objects();  // 801D4C54
  int on_tab_changed_wall();  // 801D4DDC
  int on_tab_changed_wallpaper();  // 801D4FC8
  int on_tab_changed_floor();  // 801D51A0
  int on_selection_changed();  // 801D5370
  int on_PCT_Scroll(int direction);  // 801D581C
  int on_PCT_EnterToolMode();  // 801D5910
  int on_PCT_EnterPlaceMode();  // 801D597C
  int on_PCT_SetVisible(bool visible);  // 801D5C80
  int update_grid_shaders();  // 801D5D28
  int update_object_shader();  // 801D5E38
  int calc_motive_ratings();  // 801D5F58
  int get_motive_rating_label(unsigned int, unsigned short*) const;  // 801D613C
  int get_motive_rating_value(unsigned int index, unsigned short* out) const;  // 801D6144
  int refresh_text_field(const char*, unsigned int);  // 801D61AC
  int set_control_visible(const char*, bool);  // 801D61B4
  int set_dialog_visible(bool visible);  // 801D61BC
  bool can_afford_selection() const;  // 801D6218
  int on_PCT_ShowHelp(int);  // 801D6294
  bool start_action_menu();  // 801D6370
  int CreateGrabObjectMenu(const std::vector<cXObjectImpl*>& objects);  // 801D63B8 (TNodeList<ISimInstance*>: their objects)
  void setup_interactor_camera(bool catalogue);  // 801D64A8
  int enter_tool_mode(PCTTarget__ePCTType type, bool placed);  // 801D6560
  // PC: the grab manipulator's report (GC: CallbackData): 0 dropped, 1 grab cancelled, 2 sell/delete
  // `object`, 3 the session ended.
  int GrabManipulatorCallback(int command, cXObjectImpl* object);  // 801D6824
  void buy_fail_dialog_selection_callback(int);  // 801D6DE0
  void delete_dialog_selection_callback(int);  // 801D6DEC
  void sell_dialog_selection_callback(int);  // 801D6E68
  int enter_place_tombstone_mode();  // 801D6F88
  int enter_place_mode(PCTTarget__cCellInfo* cell);  // 801D7150
  int PickedUpObject(cXObjectImpl* object);  // 801D73BC
  // PC: the place manipulator's report (GC: InteractorModule::Interactor::CallbackData): `command` 0
  // placed (`guid` the object), 3 cancelled.
  int PlaceModeCallback(int command, unsigned int guid);  // 801D745C
  // The build tools (interactors 3, 4, 5; see buildtools.hpp) for the cell (null: the tool's own; the
  // sell tool unless `placed`), reporting to the callbacks below.
  int enter_wall_mode(PCTTarget__cCellInfo* cell, bool placed);  // 801D77CC
  int WallManipulatorCallback(InteractorModule__Interactor__CallbackData* data);  // 801D7A50
  int calc_wall_value(InteractorModule__Interactor__CallbackData* data);  // 801D7CAC
  int enter_wallpaper_mode(PCTTarget__cCellInfo* cell, bool placed);  // 801D7DA0
  int calc_wallpaper_value(InteractorModule__Interactor__CallbackData* data);  // 801D7FD8
  int WallPainterCallback(InteractorModule__Interactor__CallbackData* data);  // 801D8088
  int enter_floor_mode(PCTTarget__cCellInfo* cell, bool placed);  // 801D829C
  int FloorPainterCallback(InteractorModule__Interactor__CallbackData* data);  // 801D84CC
  int calc_floor_tile_value(InteractorModule__Interactor__CallbackData* data);  // 801D865C
  void ZeroInputCallback();  // 801D8750
  void DialogClosedCallback();  // 801D875C

  // PC: the PCTTarget the place manipulator reports to (GC: the callback data's +0x8).
  static PCTTarget* s_placing;

  int m_player = 0;                    // +0x84
  unsigned int m_filters[2] = {};      // +0x88 pushAptButtonFilterALL per player
  int m_mode = 0;                      // +0x90 0 buy, 1 build, 2 tombstone
  int m_closeOnCancel = 0;             // +0x94 cancel deletes the catalogue instead of going to the pause menu
  int m_closing = 0;                   // +0x98 cancelled (no more input)
  int m_sort = 2;                      // +0x9C 1 by name, 2 by price
  std::vector<ObjSelector*> m_selectors;  // +0xA0 count, +0xA4 every selector of the database
  const PCTTarget__PCTTabInfo* m_tabs = nullptr;  // +0xA8
  unsigned int m_tabCount = 0;         // +0xAC
  unsigned int m_tab = 0;              // +0xB0
  unsigned int m_page = 0;             // +0xB4 the first column shown (six cells a column)
  unsigned int m_cell = 0;             // +0xB8 the selected grid cell (0..11)
  std::vector<PCTTarget__cCellInfo> m_cells;  // +0xBC, +0xC0 count
  int m_show2D = 0;                    // +0xC4
  int m_show3D = 0;                    // +0xC8
  int m_ratingKinds[3] = {};           // +0xCC the selection's three best catalogue ratings
  int m_ratingValues[3] = {};          // +0xD8
  ActionMenu* m_actionMenu = nullptr;  // +0xE4 the player's ActionMenu (grab mode: the objects under the cursor)
  int m_placeMode = 0;                 // +0xE8 1 placing an entry, 0 the tool (grab / build tools)
  int m_visible = 1;                   // +0xEC the catalogue is shown
  int m_dialogActive = 0;              // +0xF0
  UIDialog m_dialog;                   // +0xF4 the fire code / buy failure dialog
  HelpDialog m_help;                   // +0x17C (its +0x88, PCT +0x204: the help is up)
  int m_canAfford = 0;                 // +0x20C
  int m_price = 0;                     // +0x210 the selection's
  BBHTarget* m_bbh = nullptr;          // +0x214 the build/buy HUD
  GoalUnlock m_unlocks;                // +0x218 family 1's (GC: a pointer)
  std::vector<short> m_seenNew;        // +0x21C the new unlocks looked at (no longer new when closed)
  int m_fireWarned1 = 0;               // +0x22C
  int m_fireWarned2 = 0;               // +0x230
  int m_fireDialog2 = 0;               // +0x234
  int m_fireDialog3 = 0;               // +0x238
  int m_fireDialog1 = 0;               // +0x23C
  int m_fireOver = 0;                  // +0x240
};

// The catalogue's cheats (UpdateCheats): DAT_8047618C every object unlocked, DAT_80476188 free build
// and buy, DAT_80476190 object file names instead of catalogue names.
extern int g_unlockAllCheat;
extern int g_freeBuildBuyCheat;
extern int g_catalogFileNames;

// PC stand-ins for the player's place manipulator (GC: InteractorManager::ChangeActiveInteractor to
// interactor 6, InteractorModule::PlaceManipulator; see placemanipulator.cpp): placing a new object of
// `guid`, reporting to PCTTarget::s_placing->PlaceModeCallback. false: not started.
bool StartPlaceManipulator(int player, unsigned int guid);
// GC: InteractorManager::ChangeActiveInteractorToNull.
void StopPlaceManipulator(int player);
// Each frame (PC: from UpdateDirectInteractors' caller): the active place manipulators. true: player
// `player` is placing (its direct-control interactor waits).
bool UpdatePlaceManipulator(int player, float dt);
// The place manipulator's cursor as the player's interactor info (the camera follows it). false: none.
bool PlaceManipulatorInfo(int player, InteractorModule__InteractorInfo& info);
// GC: ChangeActiveInteractor to interactor 2 (the grab manipulator), the cursor kept from the place
// manipulator when `keepCursor`. It reports to PCTTarget::s_placing->GrabManipulatorCallback.
bool StartGrabManipulator(int player, bool keepCursor);
// The player's grab manipulator's object in hand (null: none) and GrabManipulator::DestroyObjectInHand.
cXObjectImpl* GrabObjectInHand(int player);
// The player's grab manipulator, when it is the player's interactor (InteractorManager::GetPlayerInteractor(
// player, 2)): it takes object `id` in hand (GrabManipulator::TryGrabbingWorldObjectFromId).
bool GrabManipulatorActive(int player);
void GrabTryGrabbingWorldObjectFromId(int player, short id);
void GrabDestroyObjectInHand(int player);

