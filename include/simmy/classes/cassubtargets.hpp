// @class CasEventChangeSimS2C after CASTarget CasEvent
// @class CasEventMorphSimS2C after CASTarget CasEvent
// @class CasGenetics after CASTarget CasSimDescriptionS2C
// @class CasGenetics__Grandparent after CASTarget CasSimDescriptionS2C
// @class CASBodyTarget after CASTarget UIObjectBase CasListener
// @class CASMiscTarget after CASTarget UIObjectBase CasListener
// @class CASFashionTarget after CASTarget UIObjectBase CasListener
// @class CASGeneticsTarget after CASTarget UIObjectBase CasListener CasSimDescriptionS2C CasGenetics UIScreenID PlumbBobModel
// @class CASSelectionTarget after CASTarget UIObjectBase CasListener
// @class CASTattooTarget after CASTarget UIObjectBase CasListener
// @class CASMorphTarget after CASTarget UIObjectBase CasListener UIScreenID CasSimDescriptionS2C
// @class CASPersonalTarget after CASTarget UIObjectBase CasListener UIScreenID
// @class CASRoommateTarget after CASTarget UIObjectBase CasListener PlumbBobModel
// Create-a-Sim's screens (game/targets_cas.cpp): each CAS screen's own commands and sub-nav items, as
// targets that also listen to the CAS mediator (GC: the CasListener part at +0x84). The CAS target
// (castarget.cpp) creates them and hands each screen's sub-nav ("CAS_subnav") to its target, which
// posts change events (CasEventChangeSimS2C) the edited sim's state applies and its renderer shows.
// Also the genetics (casgenetics.cpp): the grandparents and the blending of a new sim from them.
#pragma once
#include <vector>

// A change of the edited sim (CasSimState::HandleEventChangeSim): part/option `m_change` to the
// next (mode 0) or previous (1) available value, or set (2) to `m_value` (the state posts the set
// after applying a next/prev, with the old value, for the renderer).
struct CasEventChangeSimS2C : public CasEvent {
  CasEventChangeSimS2C() : CasEvent(kChangeSim) {}
  int m_change = 0;  // +0x08
  int m_mode = 0;    // +0x0C 0 next, 1 previous, 2 set
  int m_value = 0;   // +0x10 (the low byte is the value)
  int m_old = 0;     // +0x14
};

// A body/face morph value (sliders): part 0..7, value.
struct CasEventMorphSimS2C : public CasEvent {
  CasEventMorphSimS2C() : CasEvent(kMorphSim) {}
  int m_part = 0;        // +0x08
  float m_value = 0.0f;  // +0x0C
};

struct CasGenetics__Grandparent {
  CasGenetics__Grandparent() = default;  // 80161984
  ~CasGenetics__Grandparent();  // 80161998
  CasSimDescriptionS2C* m_desc = nullptr;  // +0x00
  ERTexture* m_portrait = nullptr;         // +0x04
};

// The grandmas (index 0) and grandpas (1): 26 each, from "grandparents" (binary 0x29b32cf6) or made.
struct CasGenetics {
  CasGenetics() = default;  // 801603EC
  ~CasGenetics();  // 80160424
  void BlendSimDescriptions(CasSimDescriptionS2C& a, CasSimDescriptionS2C& b, CasSimDescriptionS2C& out,
                            bool male);  // 801604A8
  signed char BlendSkinColor(signed char a, signed char b);  // 80160694
  signed char BlendBodyType(signed char a, signed char b);  // 80160788
  float BlendMorphValues(float a, float b);  // 801608A0
  void ChoosePersonalityTraits(CasSimDescriptionS2C& desc);  // 80160900
  void ChooseRandomBodyPart(eBodyPartS2C part, CasSimDescriptionS2C& desc, int keepChance);  // 80160974
  void ClearGrandparentsList();  // 80160D78
  int LoadGrandparentList(bool reload);  // 80160E38
  bool SaveGrandparentList();  // 80160F44
  void DoStream(ReconBuffer *, int);  // 80161090
  void CreateNewGrandparentList();  // 801614BC
  int LoadGrandparentPortraits();  // 8016170C
  CasGenetics__Grandparent* GetGrandparent(bool male, unsigned int index);  // 801617FC
  int UpdateGrandparentDescription(bool male, unsigned int index, CasSimDescriptionS2C& desc);  // 80161840
  std::string GetGrandparentPortraitName(bool male, unsigned int index);  // 801618F0
  int FemaleInnerLayerTorsoLocked(signed char index);  // 80161A00
  int FemaleFacialFeatureLocked(signed char index);  // 80161A20

  std::vector<CasGenetics__Grandparent*> m_grandparents[2];  // +0x00 grandmas, +0x10 grandpas
};

// Screen 5 (body: skin, body type, hair, ...).
struct CASBodyTarget : public UIObjectBase, public CasListener {
  CASBodyTarget();  // 801964BC
  ~CASBodyTarget() override;  // 801968F4
  void SetVariable(char* name, char* value) override;  // 80196A50
  void SetVariableSubnav(int item, int option);  // 80196A54
  void GetLocalizableSubnav(int item, unsigned short* text);  // 80196AB8
  void HandleHairSubnav(int option);  // 80196CA8
  void HandleBodyTypeSubnav(int option);  // 80196D74
  void HandleSkinColorSubnav(int option);  // 80196E48
  char* GetVariable(char* name) override;  // 80196F1C
};

// Screens 4, 11..14 (the CAS menu, accessories, tops, head, bottoms): only their item names.
struct CASMiscTarget : public UIObjectBase, public CasListener {
  CASMiscTarget();  // 8019BC1C
  ~CASMiscTarget() override;  // 8019C054
  void SetVariable(char* name, char* value) override;  // 8019C1B0
  char* GetVariable(char* name) override;  // 8019C1B4
  void GetLocalizableSubnav(UIScreenID screen, int item, unsigned short* text);  // 8019C1E0
};

// Screens 10 (fashion) and 17 (customizing a part: its styles, colour and material).
struct CASFashionTarget : public UIObjectBase, public CasListener {
  CASFashionTarget();  // 8019ADBC
  ~CASFashionTarget() override;  // 8019B254
  void SetVariable(char* name, char* value) override;  // 8019B3B0
  void RandomizeFashion();  // 8019B3F4
  void HandleEventChangeFocus(CasEventChangeFocus& event) override;  // 8019B3F8
  void SetVariableSubnav(UIScreenID screen, int item, int option);  // 8019B404
  static int ConvertPartUIFocusOptionToChangeEvent(int partFocus, int item, int& change);  // 8019B464
  void HandleCFCSubnav(int option, int change);  // 8019B6EC
  char* GetVariable(char* name) override;  // 8019B7B0
  unsigned short* GetLocalizable(char* name) override;  // 8019B82C
  void GetLocalizableSubnav(UIScreenID screen, int item, unsigned short* text);  // 8019B900

  int m_partFocus = 0;  // +0x8C the part screen 17 customizes (the previous part focus)
};

// Screen 3: a new sim from two pairs of grandparents (Generate), by gender.
struct CASGeneticsTarget : public UIObjectBase, public CasListener {
  enum : unsigned int { kGenerateDefaultSim = 0x10, kWarnOnGenerate = 0x20 };
  CASGeneticsTarget();  // 80196F48
  ~CASGeneticsTarget() override;  // 801976C4
  void SetVariable(char* name, char* value) override;  // 80197958
  void SetVariableSubnav(int item, int option);  // 80197B4C
  char* GetVariable(char* name) override;  // 80197C84
  void GetLocalizableSubnav(int item, unsigned short* text);  // 80197CFC
  int SetGrandparentIndex(unsigned int which, int index);  // 80197DF0
  int ShowGrandparentTexture(unsigned int which, int index);  // 80197E60
  void ClearGrandparentTexture(unsigned int which);  // 80197EFC
  int CreateParentTexture(unsigned int which);  // 80197F70
  void ClearParentTexture(unsigned int which);  // 801980E0
  void GetDefaultSim(bool male, CasSimDescriptionS2C& desc);  // 8019815C
  void SetDoneButtonState();  // 80198214
  int BeginGenerate();  // 8019828C
  static void HandleGenerateStageBG();  // 8019843C
  void Update() override;  // 80198614
  void Draw(ERC* rc) override;  // 801989B8
  CasGenetics* GetCasGenetics();  // 801A08B8
  void SetWarnOnGenerate(bool warn);  // 801A08C0
  bool GetWarnOnGenerate();  // 801A08E8
  void SetGenerateDefaultSim(bool generate);  // 801A08F4
  static bool IsGenerating();  // 801A091C

  static int s_GenerateState;  // 0 idle, 3 blend, 1/2 parent portraits, 4 post the sim, 5 done

  CasGenetics* m_genetics = nullptr;      // +0x94
  unsigned int m_grandparent[4] = {};     // +0x9C mum's mum, mum's dad, dad's mum, dad's dad
  CasSimDescriptionS2C m_parents[2];      // +0xAC mum, +0x1D0 dad
  int m_male = 1;                         // +0x2F4 the gender to make
  PlumbBobModel* m_plumbBob = nullptr;    // +0x2F8 over the new sim (BG stage 4)
  SimModel* m_parentModel = nullptr;      // +0x2FC the parent being portrayed (BG stages 1, 2)
  unsigned int m_flags = 0x15;            // +0x300 grandparent genders (bits 0..3), kGenerate*, ...
  ERShader* m_grandparentShaders[4] = {}; // +0x304
  ERShader* m_parentShaders[2] = {};      // +0x314
  ELights m_lights;                       // +0x31C the plumb bob's, in the player's colour (CGE_Init)
};

// Screen 16: the grid of a part's styles (or tattoos), four columns a page, picked into the sim.
struct CASSelectionTarget : public UIObjectBase, public CasListener {
  CASSelectionTarget();  // 80198ABC
  ~CASSelectionTarget() override;  // 801993DC
  void SetVariable(char* name, char* value) override;  // 801995A0
  int ConvertGridStringToOptionArrayIdx(char* grid);  // 801997C0
  char* GetVariable(char* name) override;  // 80199808
  void GetLockStateString(char* out);  // 801998A4
  void ClearRecentlyUnlockedBitFlag(int option);  // 80199974
  unsigned short* GetLocalizable(char* name) override;  // 80199A90
  void ScrollPageLeft();  // 80199BAC
  void ScrollPageRight();  // 80199C18
  void ShiftShaderTexturesLeft();  // 80199C88
  void ShiftShaderTexturesRight();  // 80199D70
  int ConvertGridIdxToOptionIdx(unsigned int grid);  // 80199E5C
  int ConvertColRowToOptionIdx(char column, int row);  // 80199E70
  void InitScreen();  // 80199E9C
  void CreateSelectionShaders();  // 80199FF4
  void LoadGridTextures(unsigned int first, unsigned int last);  // 8019A200
  void SetupInitialSelectionPage();  // 8019A424
  void ClearTextures();  // 8019A488
  void ClearTextures(unsigned int first, unsigned int last);  // 8019A4B0
  void UpdateSelectionArrowState();  // 8019A5F4
  static int ConvertUiFocusToBodyPart(unsigned int focus, eBodyPartS2C* part);  // 8019A6B4
  static int ConvertUiFocusToBodyTextureType(unsigned int focus, eTattooTextureTypeS2C* type);  // 8019A7F8
  int ConvertCurUIFocusToChangeEvent(int& change);  // 8019A854
  int SetTitleShaderAccordingToCurUIFocus(unsigned int focus);  // 8019A9D0

  int m_count = 0;                   // +0x8C options
  int m_hilite = -1;                 // +0x90 the highlighted option
  unsigned char m_indexes[600] = {}; // +0x94 option -> part style (or tattoo) index
  unsigned char m_lockFlags[600] = {};  // +0x2EC 2 locked, 4 recently unlocked
  int m_pageCount = 1;               // +0x544
  int m_page = 1;                    // +0x548
  int m_focus = 0;                   // +0x54C the UI focus (part) shown
  int m_male = 0;                    // +0x550
  ERTexture* m_textures[16] = {};    // +0x554 the icons of the 4x4 slots (columns of the pages around)
  ERShader* m_shaders[16] = {};      // +0x594 "CAS_csl_itemShader<A..D><1..4>"
  ERShader* m_blankShader = nullptr; // +0x5D4 0x2df01676
  char m_rectPos[3] = {};            // +0x5D8 the slot ("B1")
};

// Screen 9: tattoos (the clothes come off while choosing).
struct CASTattooTarget : public UIObjectBase, public CasListener {
  CASTattooTarget();  // 8019F1EC
  ~CASTattooTarget() override;  // 8019F644
  void SetVariable(char* name, char* value) override;  // 8019F7A0
  void GetLocalizableSubnav(int item, unsigned short* text);  // 8019F7C8
  void RemoveClothesForTattoo();  // 8019F8D8
  void RestoreClothesForTattoo();  // 8019FA24

  CasSimDescriptionS2C m_saved;  // +0x8C the outfit while tattooing
};

// Screens 6 and 8: body and head morph sliders.
struct CASMorphTarget : public UIObjectBase, public CasListener {
  CASMorphTarget();  // 8019C740
  ~CASMorphTarget() override;  // 8019CC34
  void SetVariable(char* name, char* value) override;  // 8019CD90
  void SetVariableSubnav(UIScreenID screen, int item, int option);  // 8019CF0C
  void GetLocalizableSubnav(UIScreenID screen, int item, unsigned short* text);  // 8019CFC4
  char* GetVariable(char* name) override;  // 8019D1F8
  void EnterMorphScreen();  // 8019D3AC
  void ExitMorphScreenPrev();  // 8019D434
  void ExitMorphScreenDone();  // 8019D50C

  int m_part = 0;                 // +0x8C the slider's morph (CasEventMorphSimS2C part)
  int m_lastPart = 0;             // +0x90 the last posted
  float m_last = 0.0f;            // +0x94
  int m_field98 = 0;              // +0x98
  CasSimDescriptionS2C m_saved;   // +0x9C the sim when the screen opened
  int m_entered = 0;              // +0x1C0
};

// Screens 20 (aspirations) and 21 (personality): the five personality values the movie sets (kept
// here until the screen is done, CASTarget nav 0x12a), the aspiration, the star sign of the points,
// and the five high-score item icons of each aspiration.
struct CASPersonalTarget : public UIObjectBase, public CasListener {
  CASPersonalTarget();  // 8019D518
  ~CASPersonalTarget() override;  // 8019DFC8
  void SetVariable(char* name, char* value) override;  // 8019E124
  void HandlePersonalityAnimation(char* value);  // 8019E2BC
  short ConvertFromPersonalityTextToEnum(char* text);  // 8019E300
  char* GetVariable(char* name) override;  // 8019E444
  unsigned short* GetLocalizable(char* name) override;  // 8019E5FC
  void ParseSignText(short sign, unsigned short* text);  // 8019E77C
  void ParseDescriptionText(short aspiration, unsigned short* text);  // 8019E9F4
  void ParsePersonalityText(char* name, unsigned short* text);  // 8019EB2C
  void SetVariableSubnav(UIScreenID screen, int item, int option);  // 8019EDD8
  void GetLocalizableSubnav(UIScreenID screen, int item, unsigned short* text);  // 8019EF50
  static void CalcHighScoreAspShaderIds();  // 8019F094
  int GetSavedPersVal(signed char index);  // 8019F1DC

  // m_highScoreAspShaderIds (804FC080): per aspiration, its five high-score items' icons.
  static unsigned int m_highScoreAspShaderIds[25];

  // +0x8C the personality as set on the screen (outgoing, nice, playful, neat, active: the
  // description's order, +0x11C)
  signed char m_savedPers[5] = {};
};

// Screen 24 (the roommates, CAS mode 3): the family's four slots, which one is picked (a plumb bob
// over it), and which of add/edit/delete/done are enabled.
struct CASRoommateTarget : public UIObjectBase, public CasListener {
  CASRoommateTarget();  // 8019FB08
  ~CASRoommateTarget() override;  // 801A0048
  void SetVariable(char* name, char* value) override;  // 801A01C0
  void SetVariableSubnav(int item, int option);  // 801A0258
  void GetLocalizableSubnav(int item, unsigned short* text);  // 801A0318
  char* GetVariable(char* name) override;  // 801A0430
  int SelectFirstNonEmptySlot();  // 801A0498
  void SetButtonState();  // 801A0514
  void GetButtonStateString(char* out);  // 801A055C
  void Draw(ERC* rc) override;  // 801A0604
  int GetSelectedSlot() const;  // 801A0934

  int m_selecting = 0;                  // +0x8C between CAS_car_beginSimSelect and ..endSimSelect
  int m_slot = 0;                       // +0x90 the picked slot
  PlumbBobModel* m_plumbBob = nullptr;  // +0x94
  ELights m_lights;                     // +0x98 the plumb bob's, set up on the first draw (m_numDir 0 until then)
};
