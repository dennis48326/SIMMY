// @class EGlobal after ERFont ERShader
// The game's global object (game/global.cpp, `_globals`): UI string tables, the game font, the Apt
// viewer, cheats, camera, the current house and selection. Most fields are still known only by offset.
#pragma once
#include <cstring>
#include <string>

struct ERQuickdata;

struct EGlobal {
    ~EGlobal();  // 8003E1F0
    EGlobal();  // 8003E218
    void SetAuthorModeCheats();  // 8003E538
    static void CreateAnimator();  // 8003E560
    static RET_UNKNOWN GetGameFontID();  // 8003E5B4
    bool IsObjectInUseByPlayer(int, cXObject *);  // 8003E5C0
    void LoadIntroRequirements();  // 8003E6B4
    void LoadPreGlobalRequirements();  // 8003ECA8
    void SetDefaults();  // 8003ECF4
    static void SetupScratchHeap();  // 8003F0CC
    static void FreeScratchHeap();  // 8003F0D0
    static void SetupSubstitutionStrings();  // 8003F0D4
    void Reset();  // 8003F450
    void SetCurHouse(int);  // 8003F8E0
    void ClearCurHouse();  // 8003F968
    void SelectWin(ERC *);  // 8003F9AC
    RET_UNKNOWN GetWin();  // 8003F9FC
    void SetCam(ESimsCam *);  // 8003FA14
    ESimsCam* GetCam();  // 8003FA1C
    void SetCameraDirector(CameraDirector *);  // 8003FA24
    RET_UNKNOWN TransformToScreen(EVec3 &, EVec2 &);  // 8003FA2C
    void TransformToWorld(EVec2 &, EVec3 &);  // 8003FAAC
    static void GetLiveModeMenuUIString(char *);  // 8003FAFC
    const unsigned short* const* HouseNameText(const char* name);  // 8003FB0C
    const unsigned short* const* HouseNameCapsText(const char* name);  // 8003FB7C
    static void GetScriptSubstitutionString(char *);  // 8003FBEC
    unsigned int GetHouseLevelId(const char* name);  // 8003FBFC
    unsigned int GetHouseGrassShaderId(const char* name);  // 8003FC60
    const unsigned short* const* GetHUDUIString(const char* name);  // 8003FCC4
    // A NewUIStrings row's text handle (GC: returned a wchar_t** through a hidden pointer); "*handle"
    // is the UTF-16 text. Unknown names give "deprecated_string_message", or an empty string.
    const unsigned short* const* GetNewUIString(const char* name);  // 8003FD34
    static const unsigned short* NewUIText(const char* name);  // PC: *GetNewUIString(name), or u""
    const unsigned short* const* GetPDAString(const char* name);  // 8003FDC8
    RET_UNKNOWN GetWantFearCategoryString(int, int);  // 8003FE38
    RET_UNKNOWN GetRelationshipTableRow(unsigned int);  // 8003FF0C
    RET_UNKNOWN GetMessagesTableRow(unsigned int);  // 8003FF90
    RET_UNKNOWN GetUrbzButtonsRow(unsigned int);  // 80040030
    RET_UNKNOWN GetRepTitleTableRow(unsigned int);  // 800400B0
    void GetGoalsLegendRow(unsigned int);  // 80040168
    const unsigned short* const* GetMemCardString(const char* name);  // 800402BC
    // A neighbourhood family name that is a key ("#...") loses its "#" (GetNghFamilyName); a house
    // key becomes its "HouseData" row's name, or loses its "#" without a row (GetNghHouseName).
    void GetNghFamilyName(std::u16string& name);  // 8004032C
    void GetNghHouseName(std::u16string& name);  // 80040414
    void ConvertUnicodeToShiftJIS(unsigned short *, unsigned short *, unsigned int);  // 80040550
    RET_UNKNOWN GetFloorIndex(FloorTile *);  // 800406C4
    RET_UNKNOWN GetWallIndex(WallTile *);  // 8004070C
    RET_UNKNOWN GetFenceIndex(FenceData *);  // 80040754
    cXPerson* GetSelectedPerson(int player);  // 8004079C
    bool SimInUseByOtherPlayer(int, cXPerson *);  // 800407AC
    void SetSelectedPerson(int player, cXPerson* person);  // 800407DC
    RET_UNKNOWN IsPlayerInSocialMode(int);  // 800409D8
    int GetFamilyIndexOfPerson(ObjectModuleImpl* module, Family* family, cXPerson* person);  // 80040A20
    bool SelectNextInFamily(Family* family, int index, int player);  // 80040AE0
    void AdvanceSelectedPerson(int);  // 80040CC8
    bool SelectPrevInFamily(Family* family, int index, int player);  // 80040E7C
    void ReverseSelectedPerson(int);  // 80041068
    void PlaceObjectInHouse(cXObject *);  // 80041214
    void PickUpInHouseObject(cXObject *);  // 80041218
    void GetCursorPosAsFtile(FTilePt &);  // 8004121C
    void DestroyInstance(IBaseSimInstance **);  // 80041358
    void AllocInstance(cXObject* object);  // 8004145C
    void AllocPersonInstance(cXPerson* person);  // 80041538
    static void AllocSpriteRenderer(cXObject *);  // 80041694
    static void FreeSpriteRenderer(cXObject *);  // 800416B8
    static void UpdateSpriteRenderer(SpriteSlot *);  // 800416E4
    void ConvertSpriteIdToResId(unsigned int);  // 80041708
    RET_UNKNOWN GetCounterModelTable();  // 80041758
    void LoadSelectorData(ObjSelector *, bool);  // 80041788
    void UnloadSelectorData(ObjSelector *);  // 800417B0
    RET_UNKNOWN CreateThumbnail(ObjSelector *);  // 800417DC
    static void RecalcFloor();  // 80041A0C
    static void RecalcWalls();  // 80041A2C
    void RecalcObjects();  // 80041A4C
    void RecalcHouse();  // 80041A7C
    static void BeginSaveGame();  // 80041AC0
    static void EndSaveGame();  // 80041B6C
    RET_UNKNOWN CheckForZeroExtentOverride(CTilePt &, cXObject *);  // 80041F00
    bool CheckForZeroExtentOverride(cXObject *);  // 80041F98
    static void SetBackgroundColor(float, float, float);  // 80041FE4
    static RET_UNKNOWN IsBuildHouseMode();  // 800420AC
    void SwapSelectedSims();  // 800420B4
    RET_UNKNOWN ListenForController();  // 80042194
    RET_UNKNOWN GetSkillFromName(char *);  // 8004219C
    RET_UNKNOWN GetNameFromSkill(AnimRef *);  // 800421A4
    RET_UNKNOWN GetPropFromName(char *);  // 8004227C
    RET_UNKNOWN GetNameFromProp(PropRef *);  // 80042284
    static void UseOptionsScreenAdjustXY();  // 8004228C
    static bool InLevelState();  // 80042320
    unsigned short GetPlayerCheats() const;  // 80042338
    int IsPlayerCheatOn(int cheat) const;  // 80042358
    void TogglePlayerCheat(int);  // 80042388
    void InitPlayerCheats();  // 8004246C
    int TryAutoMapPlayer(int player);  // 80042614
    static RET_UNKNOWN IsSaveGameEnabled();  // 80042718

  // The button-code cheats (InitPlayerCheats 8004246C; PlayerCheats::Capture reads them).
  unsigned short m_cheatButtons = 0;  // +0x00 every button any code uses (_globals)
  unsigned short m_cheatCodes[11][6] = {};  // +0x02 the codes: up to six EA button bits, 0-terminated
  unsigned short m_cheatCodeMask[11] = {};  // +0x86 the buttons each code uses
  unsigned short m_playerCheats = 0;  // +0x9C DAT_80475E68: bit n = cheat n is on (cleared by
                                      // TheSimsMainMenuState::Startup; trees read it as globals 0x2E/0x2F)
  unsigned short m_field9E = 0;  // +0x9E
  int m_fieldA0 = 0;  // +0xA0
  int m_fieldA4 = 0;  // +0xA4
  int m_fieldA8 = 0;  // +0xA8
  int m_fieldAC = 0;  // +0xAC
  int m_fieldB0 = 0;  // +0xB0
  void* m_aptViewer = nullptr;  // +0xB4 AptViewer (the Flash UI)
  unsigned char m_usedSimInteractor[2] = {};  // +0xB8 DAT_80475E84/85: the player last used the classic
                                              // control (SimInteractor::OnStop 1, DirectInteractor::OnStop 0)
  unsigned char m_fieldBA[2] = {};  // +0xBA
  cXPerson* m_selectedPerson[2] = {};  // +0xBC each player's selected sim (DAT_80475E88)
  int m_fieldC4 = 0;  // +0xC4
  EHouse* m_house = nullptr;  // +0xC8 the current house (DAT_80475E94)
  int m_fieldCC = 0;  // +0xCC the player being drawn (DAT_80475E98: highlights)
  ESimsCam* m_cam = nullptr;  // +0xD0 the camera being drawn/updated (SetCam, DAT_80475E9C)
  const void* m_floorSet = nullptr;  // +0xD4 the rows of quickdata 0x19a16f2d's "FloorSet" table: an array of row pointers (DAT_80475EA0)
  const void* m_wallSet = nullptr;   // +0xD8 "WallSet"
  const void* m_fenceSet = nullptr;  // +0xDC "FenceSet"
  CameraDirector* m_cameraDirector = nullptr;  // +0xE0 the director of the player being updated/drawn (SetCameraDirector, DAT_80475EAC)
  ERQuickdata* m_quickdataA = nullptr;  // +0xE4 quickdata 0xa173a1ee
  ERQuickdata* m_strings = nullptr;  // +0xE8 quickdata 0x37a8cfb9 (UI string tables)
  int m_fieldEC = 0;  // +0xEC
  ERQuickdata* m_buildSets = nullptr;  // +0xF0 quickdata 0x19a16f2d (wall, floor and fence sets)
  int m_fieldF4 = 0;  // +0xF4
  int m_fieldF8 = 0;  // +0xF8
  ERShader* m_globalShader = nullptr;  // +0xFC shader 0x1a18ca65 (fallback background)
  int m_field100 = 0;  // +0x100
  ERFont* m_font = nullptr;  // +0x104 the game font (GetGameFontID)
  int m_field108 = 0;  // +0x108
  int m_field10C = 0;  // +0x10C
  EVibrate* m_vibrate = nullptr;  // +0x110 (DAT_80475EDC)
  void* m_cheats = nullptr;  // +0x114 ECheats
  CameraDirector* m_cameraDirectors[2] = {};  // +0x118 one per player (SetDefaults)
  CameraDirector* CameraDirectorOf(int player) const { return m_cameraDirectors[player]; }
  int m_field120 = 0;  // +0x120
  void* m_uiQDTarget = nullptr;  // +0x124 UIQDTarget
  void* m_uiDBTarget = nullptr;  // +0x128 UIDBTarget
  void* m_uiRouter = nullptr;    // +0x12C UIRouter (the Apt message handler)
  void* m_uiAudioTarget = nullptr;  // +0x130 UIAUDIOTarget (the movies' "audio:" sounds)
  void* m_uiAnalog = nullptr;  // +0x134 UIAnalog (the sticks for the movies)
  int m_field138 = 0;  // +0x138
  void* m_g2dTarget = nullptr;  // +0x13C G2DTarget (the dialog box, DAT_80475F08)
  int m_field140 = 0;  // +0x140
  void* m_uiScreenManager = nullptr;  // +0x144 UIScreenManager (DAT_80475F10)
  void* m_optionsRecon = nullptr;  // +0x148 OptionsRecon
  int m_field14C = 0;  // +0x14C
  int m_field150 = 0;  // +0x150
  int m_field154 = 0;  // +0x154
  int m_field158 = 0;  // +0x158
  int m_field15C = 0;  // +0x15C
  int m_field160 = 0;  // +0x160
  int m_field164 = 0;  // +0x164
  // A 32-bit value in the config block, by its offset in EGlobal (e.g. 0x43C).
  int ConfigInt(unsigned int offset) const {
    int v;
    std::memcpy(&v, m_config + (offset - 0x168), 4);
    return v;
  }
  unsigned char m_config[0x2E0] = {};  // +0x168 EOR cheats / build config block (Globs::pEORCheats)
  int m_field448 = 0;  // +0x448
  int m_field44C = 0;  // +0x44C
  int m_field450 = 0;  // +0x450
  int m_field454 = 0;  // +0x454
  int m_field458 = 0;  // +0x458
  int m_field45C = 0;  // +0x45C
  int m_field460 = 0;  // +0x460
  int m_field464 = 0;  // +0x464
  int m_field468 = 0;  // +0x468
  int m_field46C = 0;  // +0x46C
  int m_field470 = 0;  // +0x470
  int m_field474 = 0;  // +0x474
  int m_field478 = 0;  // +0x478
  int m_field47C = 0;  // +0x47C
  int m_field480 = 0;  // +0x480
  int m_field484 = 0;  // +0x484
  int m_field488 = 0;  // +0x488
  int m_field48C = 0;  // +0x48C
  void* m_casCostumes = nullptr;  // +0x490 CasCostumes
};
