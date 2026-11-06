// @class HUDTarget after UIObjectBase UIReflow ActionMenu
// @class H2DTarget after UIObjectBase MODTarget MOTTarget SKLTarget WAFTarget ActionMenu CSMTarget ACTTarget INTTarget
// @class MODTarget after UIObjectBase UIReflow
// @class MOTTarget after UIObjectBase UIReflow
// @class SKLTarget after UIObjectBase UIReflow
// @class WAFTarget after UIObjectBase UIReflow
// The live-mode HUD (game/hudtarget.cpp; H2DTarget and the HUD widget targets in targets_s2c.cpp).
// HUDTarget (GameData +0x44) is the game's side of the HUD: the "HUD_*" variables (need bars, job
// mode, reputation, the HUD's show/hide state the movie reports), the clock in UIDB, the speed
// controls, and it makes H2DTarget (GameData +0x48), the 2D HUD: the "H2D" widget screen takes
// the d-pad, Start and B/Y buttons, and per player it makes the HUD widgets (MOD mood meter with
// time and money and the d-pad icons, MOT motives, SKL skills, WAF wants and fears, ACT action
// queue, CSM sim menu, INT interest meter). Everything is per player ("0..."/"1..." variable
// names, screen ids n / n + 1) for split-screen; the second player's set exists when GameData
// +0xEC (two players) is set.
//
// Not ported yet: the action queues (ActionQueueHUD), the ACT/INT/MDI widget targets (null), the
// reward and intro dialogs, reputation titles. The action menus (ActionMenu, actionmenu.hpp) and the
// CSM widget (csmtarget.hpp) are ported.
#pragma once

#include <string>
#include <vector>

// 0x8B8 bytes.
struct HUDTarget : UIObjectBase {
  HUDTarget();  // 8018591C
  ~HUDTarget() override;  // 801869FC
  void AttachControllers();  // 80186C48
  void ReleaseControllers();  // 80186D2C
  void IncPause(bool sound);  // 80186DD4
  void ReleasePause(bool sound);  // 80186E04
  void SetVariable(char* name, char* value) override;  // 80186E34
  void SetPlayerVariable(int player, char* name, char* value);  // 80186EA0
  char* GetVariable(char* name) override;  // 801871C0
  char* GetPlayerVariable(int player, char* name);  // 80187224
  unsigned short* GetLocalizable(char* name) override;  // 801876E0
  unsigned short* GetPlayerLocalizable(int player, char* name);  // 80187744
  void InitRepValues();  // 80187A44
  int GetReputationValue(int player);  // 80187B00
  int GetNextRepUnlockValue(int player);  // 80187BC4
  void OnReadBar(int player, unsigned int bar);  // 80187C9C
  void ShowHUD();  // 80187E3C
  void HideHUD();  // 80187EB4
  void OnPlayerHUDShow(int player);  // 80187F24
  void OnPlayerHUDHide(int player);  // 80187F54
  void OnPlayerHUDHideStart(int player);  // 80187F80
  void OnHUDShow();  // 80187FFC
  void OnHUDHideStart();  // 80188000
  void OnHUDHide();  // 80188004
  void InstallHUDIcons(int player);  // 80188008
  int ShouldHUDBeVisible(int player) const;  // 80188348
  void UpdateTutorialLocks();  // 80188464
  void Update() override;  // 801884B4
  void UpdateClock();  // 801885AC
  void UpdateActivePlayer(int player, bool attach);  // 801886C0
  void UpdateSpeedControls();  // 801887AC
  void Draw(ERC* rc) override;  // 8018901C
  void ResetJobHudProperties();  // 801890CC
  void AddMenu(int player, cXObjectImpl* object, const std::u16string& name, unsigned int shaderId, ERShader* shader);  // 80188BF4
  void AddMenuItem(int player, cXObjectImpl* object, const MenuInteraction* interaction, const std::u16string& name,
                   unsigned int shaderId, ERShader* shader, int unused, bool disabled);  // 80188C54
  void NotifyMenuRemoveObj(cXObjectImpl* object);  // 80188CC4
  void CompleteMenu(int player);  // 80188D3C
  int ShowMenu(int player);  // 80188D7C
  int IsMenuAvailable(int player) const;  // 80188DBC
  void CloseMenu(int player);  // 80188E34
  int IsHUDVisible() const;  // 80189A9C
  int IsPlayerHUDVisible(int player) const;  // 80189AB0

  // not ported yet
  void ReadMoodBar(int, unsigned int);  // 80187D10
  RET_UNKNOWN IsActionQueueAvailable(int) const;  // 80188724
  void AddAction(int, Interaction *, BString2 &, unsigned int, ERShader *, unsigned int);  // 80188E60
  RET_UNKNOWN RemoveAction(int, Interaction *);  // 80188F70
  void RemoveAllActions(int);  // 80188FC8
  void DrawMotiveValues(ERC *, int);  // 801890C8
  RET_UNKNOWN RequestHUD(short, short, short);  // 801891C0
  static RET_UNKNOWN AddHUDControl(short, short, short &);  // 801892A8
  RET_UNKNOWN RemoveHUDControl(short, short);  // 801892D4
  static RET_UNKNOWN ManageHUD(short, short, short);  // 801892FC
  RET_UNKNOWN SetHUDData(short, short);  // 80189360
  RET_UNKNOWN SetJobBarInputRange(int, int, int);  // 80189408
  RET_UNKNOWN ApplyMotiveCurveArray(unsigned int, short);  // 80189444
  RET_UNKNOWN CalculateJobScore();  // 801895AC
  RET_UNKNOWN GetHUDData(short, short &);  // 801897EC
  void AnimateHUDControl(short, HUDTarget__EControlAnimType);  // 80189864
  void GotoCAS(unsigned short, unsigned short, int, int);  // 801898E4
  RET_UNKNOWN GotoMainMenu();  // 801899E8
  void RequestQuitGame(bool);  // 80189A18
  RET_UNKNOWN GetJobBarCount();  // 80189A34
  RET_UNKNOWN GetActionQueueHUD(int);  // 80189A50
  RET_UNKNOWN GetHUDMode() const;  // 80189A60
  bool HUDHideComplete() const;  // 80189A78
  bool HideInProgress() const;  // 80189A88
  RET_UNKNOWN GetHudTransitionTarget();  // 80189AC8

  unsigned char m_clockCountdown = 1;   // +0x84 frames until UpdateClock (every 30)
  unsigned char m_bars[2][6] = {};      // +0xA4 the need bars the movie reads (0..100)
  unsigned short m_mode = 0;            // +0xC8 job HUD mode (1 sushi .. 9 bartender)
  unsigned short m_jobLevel = 0;        // +0xCA
  int m_initialized = 0;                // +0xE4 the movie set HUD_initialized
  unsigned char m_hudState = 0;         // +0xE8 0 hidden, 1 hiding, 2 shown (HUD_state)
  unsigned char m_playerState[2] = {};  // +0xE9 per player, the same (HUD_playerState)
  int m_tutorial[2] = {};               // +0xEC tutorial stage (GetTutorialLocks)
  unsigned char m_speedPaused = 0;      // +0xF7 the speed controls paused the game
  unsigned char m_quit = 0;             // +0x100 1 quit requested, 2+ going
  int m_activePlayer = 0;               // +0x104 the job HUD's player
  int m_pauseCount = 0;                 // (GC: IncPause/ReleasePause go to H2DTarget)
  ActionMenu m_menus[2];                // +0x740 the players' interaction menus (0xB4 each)
  short m_baseRep[2] = {};              // +0x8A8
  short m_rep[2] = {};                  // +0x8AC
  H2DTarget* m_h2d = nullptr;           // +0x8B4
  unsigned int m_filters[2] = {};       // AttachControllers' controller filters
};

// The mood widget ("MOD", screens 0x34/0x35): mood meter, the d-pad's side panel icons, time and
// money (0x100 bytes).
struct MODTarget : UIObjectBase {
  explicit MODTarget(int player);  // 801C87EC
  ~MODTarget() override;  // 801C9178
  void SetVariable(char* name, char* value) override;  // 801C92F0
  char* GetVariable(char* name) override;  // 801C93F4
  unsigned short* GetLocalizable(char* name) override;  // 801C94C0
  void Update() override;  // 801C95D4
  void ShowMood();  // 801C9AE4
  void HideMood();  // 801C9AF0
  void UpdateMood();  // 801C9AFC
  int IsMoodActive() const;  // 801C9C84
  void SetDpadIcons(int icon);  // 801CA648 (MODTarget::eSidePanelIcon)
  void DisplayPaused();  // 801CA948
  void DisplayFastForward();  // 801CA9B0
  void TriggerAspirationChestDisplay();  // 801CAA04
  void SetTextButtonPressed(bool pressed);  // 801CAAB4

  static void DrawMoodHeadCallback(ERC* rc, structDrawCBparams* params);  // 801C9A0C
  // MODTarget::eSidePanelId: 0 / 1 the up / down arrows, 2 up (skills), 3 left (motives), 4 right
  // (wants and fears), 5 down, 6 none.
  void SelectDpadArrow(int arrow);  // 801C9C8C
  void SelectDpadButton(int panel);  // 801C9E4C
  void SetWarning(int panel, bool on, bool want);  // 801CA1E8

  // not ported yet
  void SetMeterValue(float, bool);  // 801CA44C

  int m_exists = 0;          // +0x84 the movie asked MOD_exists
  int m_shown = 0;           // +0x88
  int m_tweening = 0;        // +0x8C a show/hide tween is running
  int m_wantShown = 0;       // +0x90 ShowMood/HideMood
  int m_player = 0;          // +0x94
  int m_warningFrames = 0;   // +0x9C
  int m_frame = 0;           // +0xA0 UpdateMood calls
  int m_fieldA4 = 0;         // +0xA4
  int m_arrows = 0;          // +0xA8 the action queue's up/down arrows replace the up/down icons
  int m_fieldAC = 0;         // +0xAC
  int m_fieldB0 = 0;         // +0xB0
  int m_upIcon = 0;          // +0xB4 each icon: 0 normal, 1 selected, 2 warning
  int m_leftIcon = 0;        // +0xB8
  int m_rightIcon = 0;       // +0xBC
  int m_downIcon = 0;        // +0xC0
  int m_dpadState = 5;       // +0xC4 the selected side panel (5 none)
  UIReflow m_reflow;         // +0xC8
  unsigned int m_meter = 0;  // +0xDC the mood meter's step shown (0..16)
  int m_pauseFrames = 0;     // +0xE0 frames the pause icon stays
  int m_ffFrames = 0;        // +0xE4 frames the fast-forward icon stays
  int m_chestFrames = 0;     // +0xE8 frames the aspiration chest stays open
  int m_aspiration = 0;      // +0xEC
  int m_aspirationShown = 0; // +0xF0
  int m_textTweening = 0;    // +0xF4
  int m_textWanted = 0;      // +0xF8 SetTextButtonPressed
  int m_textShown = 0;       // +0xFC
};

// The motives side panel ("MOT", screens 0x36/0x37: d-pad left): eight need bars with their icons,
// read from the player's selected sim every sixth H2D update (0x16C bytes).
struct MOTTarget : UIObjectBase {
  explicit MOTTarget(int player);  // 801CAABC
  ~MOTTarget() override;  // 801CB5A4
  void SetVariable(char* name, char* value) override;  // 801CB710
  char* GetVariable(char* name) override;  // 801CB828
  unsigned short* GetLocalizable(char* name) override;  // 801CB8C0
  void Update() override;  // 801CB944
  void ShowMotives();  // 801CBB58
  void HideMotives();  // 801CBB64
  void UpdateMotives();  // 801CBB74
  void UpdateMotiveBars();  // 801CBB78
  int IsMotivesActive();  // 801CBFC0
  int ReadMotiveBar(int bar);  // 801CBFC8
  void ResetRealMotiveValues();  // 801CC198
  void SelectedPersonChanged(cXPerson* person);  // 801CC2A0
  void SetTextButtonPressed(bool pressed);  // 801CC2AC

  int m_exists = 0;            // +0x84 the movie asked MOT_exists
  int m_shown = 0;             // +0x88
  int m_player = 0;            // +0x8C
  int m_textTweening = 0;      // +0x90
  int m_textWanted = 0;        // +0x94
  int m_textShown = 0;         // +0x98
  int m_bar[8] = {};           // +0x9C each bar's value (0..10)
  int m_shownBar[8] = {};      // +0xBC the value the bar moves through
  int m_frame[8] = {};         // +0xDC the bar's animation frame ("a<n>": value + 0 rising, 11 falling, 22 steady)
  int m_countdown[8] = {};     // +0xFC updates until the bar is read again
  int m_real[8] = {};          // +0x11C the motive (-100..100) last time it moved by 4 or more
  int m_trend[8] = {};         // +0x13C 0 steady, 1 rising, 2 falling
  int m_tweening = 0;          // +0x15C
  int m_wantShown = 0;         // +0x160 ShowMotives/HideMotives
  cXPerson* m_person = nullptr;  // +0x164 the selected sim read
  int m_personChanged = 1;     // +0x168
};

// The skills side panel (d-pad up; game/targets_s2c.cpp, 0xE0 bytes): seven skill levels of the
// selected sim (text "skill<n>_SKL", the skill points / 100), a changed one highlighted for 10 updates.
struct SKLTarget : UIObjectBase {
  explicit SKLTarget(int player);  // 801E02DC
  ~SKLTarget() override;  // 801E0DC8
  void SetVariable(char* name, char* value) override;  // 801E0F34
  char* GetVariable(char* name) override;  // 801E104C
  unsigned short* GetLocalizable(char* name) override;  // 801E10E4
  void Update() override;  // 801E140C
  void ShowSkills();  // 801E1628
  void HideSkills();  // 801E1634
  void UpdateSkillsBar();  // 801E1644
  int IsSkillsActive();  // 801E19BC
  void SetTextButtonPressed(bool pressed);  // 801E19C4

  int m_exists = 0;            // +0x84
  int m_shown = 0;             // +0x88
  int m_tweening = 0;          // +0x8C
  int m_wantShown = 0;         // +0x90
  int m_player = 0;            // +0x94
  int m_textTweening = 0;      // +0x98
  int m_textWanted = 0;        // +0x9C
  int m_textShown = 0;         // +0xA0
  cXPerson* m_person = nullptr;  // +0xA4 the sim read
  int m_skill[7] = {};         // +0xA8 its skill points (0..1000) last read
  int m_highlight[7] = {};     // +0xC4 updates left of a changed skill's highlight
};

// The wants and fears side panel (d-pad right; game/targets_s2c.cpp, 0x288 bytes): the aspiration
// meter (a bottom bar to 50, a top bar from 50 with tiers at 75 and 87, tweened through masks; it
// glows at a tier) and seven icons (four wants, three fears) with their text, which the sim's
// neighbour sets through H2DTarget::NewWantFearIcon (a slot-machine spin, a flip or at once).
struct WAFTarget : UIObjectBase {
  // WAFTarget::eWAFIconAnim
  enum IconAnim : int { kAnimNone = 0, kAnimSpin = 1, kAnimFlip = 2, kAnimSet = 3 };
  explicit WAFTarget(int player);  // 801E19CC
  ~WAFTarget() override;  // 801E2D10
  void SetVariable(char* name, char* value) override;  // 801E2EC0
  char* GetVariable(char* name) override;  // 801E30C8
  unsigned short* GetLocalizable(char* name) override;  // 801E3160
  void Update() override;  // 801E3278
  void ShowWantsAndFears();  // 801E3574
  void HideWantsAndFears();  // 801E3580
  void UpdateWantsAndFears();  // 801E3590
  int IsWantsAndFearsActive();  // 801E38F8
  void NewWantsAndFearsIcon(int slot, unsigned int base, unsigned int overlay, int anim, Neighbor* neighbor);  // 801E3900
  void StartAnimation(int slot);  // 801E3C9C
  void FinishSettingIcon(int slot);  // 801E3E60
  void SetAspirationMeterHeight(int height);  // 801E3FB0
  void BeginMeterTween();  // 801E4074
  void BeginBottomBarTween();  // 801E4140
  void BeginTopBarTween();  // 801E4344
  int OnTopBarTweenComplete();  // 801E4548
  int OnBottomBarTweenComplete();  // 801E45B8
  unsigned int GetTopBarColorAtTier();  // 801E460C
  float CalculateTweenTargetPosition();  // 801E46AC
  static float MapGameAspirationValueToMeterValue(float score);  // 801E4758
  int GlowEffectStart();  // 801E4858
  void GlowEffectAwayStart();  // 801E4970
  void OnGlowEffectComplete();  // 801E4A00
  unsigned int GetTopBarTweenColor();  // 801E4A0C
  void OnShow();  // 801E4AFC
  void UpdateAspirationMeter();  // 801E4BBC
  void SetTextButtonPressed(bool pressed);  // 801E4F24

  // One icon slot (0x3C bytes from +0xA4).
  struct Icon {
    unsigned int base = 0;           // +0x04 the shaders shown
    unsigned int overlay = 0;        // +0x08
    unsigned int newBase = 0;        // +0x0C the shaders to show once the animation turns
    unsigned int newOverlay = 0;     // +0x10
    float spin = 0.0f;               // +0x14 seconds of slot-machine spin left
    float flip = 0.0f;               // +0x18 seconds until the new icon is settled
    float delay = 0.0f;              // +0x1C seconds until a change made while hidden shows
    const char* swf = nullptr;       // +0x20 "WAF_swf<n>" (the icon's backdrop swf)
    const char* baseName = nullptr;  // +0x24 "<p>WAF_shdr<n>0"
    const char* overlayName = nullptr;  // +0x28 "<p>WAF_shdr<n>1"
    int type = 0;                    // +0x2C WAF_SwfTypes: 0 want, 1 fear, 2 gold, 3 platinum
    int anim = kAnimSpin;            // +0x30 IconAnim
    std::vector<unsigned short> text;  // +0x34 BString2
    Neighbor* dialog = nullptr;      // +0x38 the neighbour whose want/fear dialog follows the flip
  };

  int m_exists = 0;            // +0x84
  int m_shown = 0;             // +0x88
  int m_tweening = 0;          // +0x8C
  int m_wantShown = 0;         // +0x90
  int m_player = 0;            // +0x94
  int m_textTweening = 0;      // +0x98
  int m_textWanted = 0;        // +0x9C
  int m_textShown = 0;         // +0xA0
  Icon m_icons[7];             // +0xA4
  float m_time = 0.0f;         // +0x248
  int m_iconsInitialized = 0;  // +0x24C the sim's icons were asked for
  UIReflow m_reflow;           // +0x250
  int m_barTweening = 0;       // +0x264
  int m_settled = 1;           // +0x268 the meter reached its target
  int m_rising = 1;            // +0x26C
  int m_glowing = 0;           // +0x270
  float m_target = 0.0f;       // +0x274 the meter value (0..100) to show
  float m_meter = 0.0f;        // +0x278 the value shown
  float m_tweenTo = 0.0f;      // +0x27C the running tween's end
  int m_glowFull = 0;          // +0x280 the glow reached full
  cXPerson* m_person = nullptr;  // +0x284 the sim read
};

// PC accessors for the world side (GC: the simulator's wants and fears, WantFearManager, not in this
// tree). One active want or fear of a neighbour (Neighbor +0x198, slots 0..3 wants, 4..6 fears):
// WantFear::Bookmark::GetType (0 none, 1, 4 or 0x80 fear), GetBaseShader, GetOverlayShader, GetText.
struct WantFearSlotInfo {
  int type = 0;
  unsigned int base = 0;
  unsigned int overlay = 0;
  std::vector<unsigned short> text;  // UCS-2, no terminator
};
// False (and an empty slot) while the wants and fears are not ported (SIMMY_STUB).
bool NeighborWantFearSlot(Neighbor* neighbor, int slot, WantFearSlotInfo* out);
// cXPersonImpl::InitWantFearIcons (8012C08C, cXPerson vtable 0x3B4): the person's neighbour sends
// its seven icons to the HUD (Neighbor::InitWantFearIcons -> UpdateUIifNeeded(slot, 3) ->
// H2DTarget::NewWantFearIcon). SIMMY_STUB until the wants and fears are ported.
void PersonInitWantFearIcons(cXPerson* person);
// The person's 3D head drawn into the mood widget's "MOD_head" rectangle (GC: the person's ESim
// (cXObject +0x14) +0x628 ESims3DHead, vtable 0x1C on its +0x3B0 part: Draw2D). SIMMY_STUB until the
// ESim side makes its ESims3DHead.
void DrawPersonMoodHead(cXPerson* person, ERC* rc, structDrawCBparams* params);
// The person's neighbour (GC: Neighborhood FindNeighborByID(person vtable 0x1B4)); null for none.
Neighbor* PersonNeighbor(cXPerson* person);

// 0x11C bytes (GameData +0x48).
struct H2DTarget : UIObjectBase {
  H2DTarget();  // 801B727C
  ~H2DTarget() override;  // 801B7B18
  void SetVariable(char* name, char* value) override;  // 801B7EB0
  char* GetVariable(char* name) override;  // 801B836C
  unsigned short* GetLocalizable(char* name) override;  // 801B83FC
  void Draw(ERC* rc) override;  // 801B8480
  void Update() override;  // 801B8484
  int GetPausePlayer();  // 801B8A38
  static void IncPause(bool sound);  // 801B8A40
  static void ReleasePause(bool sound);  // 801B8ACC
  void CloseSidePanels(int player);  // 801B8B54
  void SetMoodMeterValue(int player, float value, bool sound);  // 801B9088
  int ShouldH2DBeVisible(int player) const;  // 801B90CC
  void DisplayPaused();  // 801B9260
  void DisplayFastForward();  // 801B928C
  void NewWantFearIcon(int player, int slot, unsigned int base, unsigned int overlay, int anim, Neighbor* neighbor);  // 801B8E3C

  // not ported yet
  void SelectedPersonChanged(int, cXPerson *);  // 801B8BA4
  RET_UNKNOWN SpawnRewardMomentDialog(int, int);  // 801B8C3C
  void ACTCancelModeExited(int player);  // 801B8C98
  void SpawnIntroThoughtBalloon(UIDialog *);  // 801B8D0C
  RET_UNKNOWN IsUserReadingIntroThoughtBalloon();  // 801B8D60
  static void SpawnModelessDialog(UIDialog *);  // 801B8DB0
  void CleanupModelessDialogs();  // 801B8DDC
  RET_UNKNOWN CountModelessDialogs();  // 801B8E08
  void AddAction(int player, Interaction* interaction, const std::u16string& name, unsigned int shader,
                 ERShader* resource, unsigned int insertBefore);  // 801B8E84
  void RemoveAction(int player, Interaction* interaction);  // 801B8EC8
  void RemoveAllActions(int player);  // 801B8F04
  int IsActionQueueAvailable(int player) const;  // 801B8F40
  void CSIMActive(int player, bool active);  // 801B8F8C
  void SKLInWarning(int, bool);  // 801B8FD8
  void WAFInWarning(int player, bool on, bool want);  // 801B9050
  void MOTInWarning(int player, bool on);  // 801B9014
  void RewardMomentDialogClosed();  // 801B90C0
  void SetInteractionDisplayValues(int player, unsigned int change, unsigned int relationship);  // 801B91E4
  void TriggerInteractionAnimation(int player, unsigned int daily, unsigned int lifetime, unsigned int bonus);  // 801B9220
  void CSIMSetIconDisable(int player, bool enabled);  // 801B92B8
  void INTMirrorUpArrow(int player, bool on);  // 801B92E8
  RET_UNKNOWN IsDialogActive();  // 801B9318

  static int m_nPauseRefCount;    // 8047630C
  MODTarget* m_mod[2] = {};       // +0x88
  CSMTarget* m_csm[2] = {};       // +0x90
  MOTTarget* m_mot[2] = {};       // +0x9C
  SKLTarget* m_skl[2] = {};       // +0xA4
  WAFTarget* m_waf[2] = {};       // +0xAC
  ACTTarget* m_act[2] = {};       // +0xB4 the action queue widgets
  INTTarget* m_int[2] = {};       // +0xBC the interest meters
  void* m_mdi = nullptr;          // +0xC8 MDITarget (not ported)
  int m_pausePlayer = 0;          // +0xD0 the player who pressed Start
  int m_textPressed[2] = {};      // +0xE4 the side panel's text button is held
  unsigned int m_buttonFilters[2] = {};  // +0xEC
  // PC: Update's B handling for player `p` (part of 801B8484).
  void UpdateCancelButton(int p);
  int m_cancelHeld[2] = {};       // +0xF4
  int m_cancelFrames[2] = {};     // +0xFC
  int m_cancelMode[2] = {};       // +0x104
  int m_cancelFilter[2] = {};     // +0x10C
  int m_sidePanel[2] = {};        // +0x114 0 none, 1 wants and fears, 2 motives, 3 skills
};
