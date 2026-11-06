// @class ItemType after UIObjectBase ERFont ERShader EVec2 EVec4 TextBlock
// @class ItemStyle after ItemType
// @class WrapperPaneBase__ButtonType after ItemType
// @class WrapperPaneBase__PaneState after ItemType
// @class PaneItem after ItemType
// @class TextBaseItem after ItemType
// @class TextItem after ItemType
// @class MenuItem after ItemType
// @class ButtonItem after ItemType
// @class IconItem after ItemType
// @class WrapperPaneBase after ItemType
// @class BackgroundPane after ItemType
// @class DialogPaneBase after ItemType
// @class MenuPane after ItemType
// @class MenuDialogPane after ItemType
// @class Wrapper after ItemType
// The game's small UI toolkit beside Apt (game/wrapper.cpp): a Wrapper holds panes, a pane holds
// items (text, menu entries, buttons, icons) and an optional full-pane background shader, and draws
// them at the pane's alpha. Button prompts (accept, decline, ...) are text items that can also have an
// Apt button created for them through the Flash front end ("createButton" and friends). GC vtables
// sat after the members (PaneItem +0x1C, WrapperPaneBase +0x5C, Wrapper +0x80).
#pragma once

#include <vector>

#include "simmy/enum.hpp"

struct ItemType {
  SIMMY_ENUM_BODY(ItemType)
  static constexpr int Text = 0;
  static constexpr int Menu = 1;
  static constexpr int Button = 2;
  static constexpr int Icon = 3;
};
// Colour presets (TextBaseItem::s_style): 0 white, 1 blue-black, 2 blue, 3 dark grey, 4 light grey,
// 5 white with shadow, 6 yellow with shadow, 7 blue, 8 white, 9 orange (the highlighted item).
struct ItemStyle {
  SIMMY_ENUM_BODY(ItemStyle)
  static constexpr int White = 0;
  static constexpr int WhiteShadow = 5;
  static constexpr int Unhighlighted = 7;  // keeps its colour when selected
  static constexpr int Highlight = 9;
  static constexpr int Count = 10;
};
// The pane's Apt button prompts, named in WrapperPaneBase::s_pButtonName.
struct WrapperPaneBase__ButtonType {
  SIMMY_ENUM_BODY(WrapperPaneBase__ButtonType)
  static constexpr int Accept = 0;
  static constexpr int Decline = 1;
  static constexpr int Alt1 = 2;
  static constexpr int Alt2 = 3;
  static constexpr int Flipper1 = 4;
  static constexpr int Flipper2 = 5;
  static constexpr int Walls = 6;
  static constexpr int Count = 7;
};
struct WrapperPaneBase__PaneState {
  SIMMY_ENUM_BODY(WrapperPaneBase__PaneState)
  static constexpr int Running = 0;
  static constexpr int Accepted = 1;
  static constexpr int Declined = 2;
  static constexpr int Alt1 = 3;
};

// One item of a pane. Its position and size are in window coordinates.
struct PaneItem {
  // m_flags
  enum : unsigned int {
    kEnabled = 0x1,          // drawn and counted by the pane's lookups
    kSizeDirty = 0x2,
    kSelected = 0x4,
    kFirstPass = 0x8,        // drawn in the pane's first item pass (else the second)
    kAptButtonDirty = 0x800000,     // the Apt button needs a "resetButton"
    kAptButtonXSet = 0x1000000,     // Apt has read/written the button's x
    kAptButtonYSet = 0x2000000,
    kPosXChanged = 0x10000000,
    kPosYChanged = 0x20000000,
    kWidthChanged = 0x40000000,
  };
  explicit PaneItem(ItemType type) : m_type(type) {}
  virtual ~PaneItem() {  // 803A091C (GC: called its Startup and Reset)
    PaneItem::Startup();
    PaneItem::Reset();
  }
  virtual void Startup() {}  // 803A0970
  virtual void Shutdown() {}  // 803A07B8
  virtual void Reset() { m_flags = kEnabled; }  // 803A07BC
  virtual void CalculateSize() {}  // 803A07C8
  virtual void Draw(ERC*, float) {}  // 803A0974
  virtual void SetPos(EVec2& pos) { m_pos = pos; }  // 803A07CC
  virtual EVec2* GetPos() { return &m_pos; }  // 803A07E0
  virtual void SetPosX(float x) { m_pos.x = x; }  // 803A07E8
  virtual float GetPosX() const { return m_pos.x; }  // 803A07F0
  virtual void SetPosY(float y) { m_pos.y = y; }  // 803A07F8
  virtual float GetPosY() const { return m_pos.y; }  // 803A0800
  virtual void SetSize(EVec2& size) {  // 803A0808
    m_flags &= ~kSizeDirty;
    m_size = size;
  }
  virtual EVec2* GetSize() {  // 803A0828
    CalculateSize();
    return &m_size;
  }
  virtual void SetSizeX(float x) {  // 803A086C
    m_size.x = x;
    m_flags &= ~kSizeDirty;
  }
  virtual float GetSizeX() {  // 803A0880
    CalculateSize();
    return m_size.x;
  }
  virtual void SetSizeY(float y) {  // 803A08C4
    m_size.y = y;
    m_flags &= ~kSizeDirty;
  }
  virtual float GetSizeY() {  // 803A08D8
    CalculateSize();
    return m_size.y;
  }

  ItemType m_type;                // +0x00
  unsigned int m_flags = kEnabled;  // +0x04
  int m_value = 0;                // +0x08 menu entries: the value reported when chosen
  EVec2 m_pos;                    // +0x0C
  EVec2 m_size;                   // +0x14
};

// A text item: a TextBlock with a colour style. Position and display size live in the TextBlock.
// GetPosX/GetPosY are new non-const virtuals here; PaneItem's const ones still read PaneItem::m_pos.
struct TextBaseItem : PaneItem {
  struct Style {
    EVec4 color;
    int shadow;
  };
  static Style s_style[ItemStyle::Count];  // 80485810
  static int s_bStyleInited;               // set once SetStyle has filled s_style

  explicit TextBaseItem(ItemType type);  // 80086FF4
  ~TextBaseItem() override;  // 80087088
  void Startup() override;  // 80084464
  void Draw(ERC* rc, float alpha) override;  // 800844B8
  void Reset() override;  // 80087104
  void CalculateSize() override;  // 80087110
  void SetPos(EVec2& pos) override;  // 800871D8
  EVec2* GetPos() override;  // 800871EC
  void SetPosX(float x) override;  // 800871F4
  void SetPosY(float y) override;  // 80087204
  virtual void SetColor(EVec4& color);  // 80087170
  virtual EVec4* GetColor();  // 80087198
  virtual void SetAlpha(float a);  // 800871A0
  virtual float GetAlpha();  // 800871AC
  virtual void SetShadowAlpha(float a);  // 800871B8
  virtual float GetShadowAlpha();  // 800871C0
  virtual void SetShadowSize(float size);  // 800871C8
  virtual float GetShadowSize();  // 800871D0
  virtual float GetPosX();  // 800871FC
  virtual float GetPosY();  // 8008720C
  using PaneItem::GetPosX;
  using PaneItem::GetPosY;

  void SetText(const unsigned short* text);  // 80084594
  void SetStyle(ItemStyle style);  // 800845DC
  void SetDisplaySize(EVec2& size);  // 80087214
  EVec2* GetDisplaySize();  // 8008724C
  float GetDisplaySizeX();  // 80087254
  float GetDisplaySizeY();  // 8008725C
  void SetDisplayPadSize(EVec2& size);  // 80087264
  EVec2* GetDisplayPadSize();  // 8008729C
  float GetDisplayPadSizeX();  // 800872A4
  float GetDisplayPadSizeY();  // 800872AC
  void SetVerticalCenter(bool on);  // 800872B4
  void SetYPosCentered(bool on);  // 800872E0
  const unsigned short* GetText() const;  // 8008730C
  void SetFont(ERFont* font);  // 80087314
  ERFont* GetFont() const;  // 80087354
  void SetFontSize(float size);  // 8008735C
  float GetFontSize() const;  // 8008739C
  ItemStyle GetStyle() const;  // 800873A4
  // Inline in the original: a font or size change re-measures the text.
  void MarkDirty() {
    m_block.SetDirty();
    m_flags |= kSizeDirty;
  }

  TextBlock m_block;      // +0x20
  ItemStyle m_style = 0;  // +0x6C
};

struct TextItem : TextBaseItem {
  TextItem();  // 800873AC
  ~TextItem() override;  // 80087458
  void Startup() override;  // 8008480C
};
struct MenuItem : TextBaseItem {
  MenuItem();  // 800874D4
  ~MenuItem() override;  // 80087580
  void Startup() override;  // 80084864
};
struct ButtonItem : TextBaseItem {
  ButtonItem();  // 800875FC
  ~ButtonItem() override;  // 800876AC
  void Startup() override;  // 800848BC
};

// A shader drawn as a rectangle of m_size centred on m_pos.
struct IconItem : PaneItem {
  IconItem();  // 80087728
  ~IconItem() override;  // 800877C4
  void Startup() override;  // 80084914
  void Shutdown() override;  // 80084920
  void Draw(ERC* rc, float alpha) override;  // 80084940
  virtual void SetColor(EVec4& color);  // 80087818
  virtual EVec4 GetColor();  // 80087840
  void SetShader(unsigned int id);  // 80084A0C
  void ReleaseShader();  // 80084A4C
  void SetAlpha(float a);  // 800878A4
  float GetAlpha();  // 800878AC
  void SetShader(ERShader* shader);  // 800878B4
  ERShader* GetShader();  // 800878BC

  EVec4 m_color{1.0f, 1.0f, 1.0f, 1.0f};  // +0x20
  ERShader* m_shader = nullptr;            // +0x30
};

struct WrapperPaneBase {
  using ButtonType = ::WrapperPaneBase__ButtonType;
  using PaneState = ::WrapperPaneBase__PaneState;
  // m_flags
  enum : unsigned int {
    kDrawEnabled = 0x1,
    kAptButtonCreated = 0x100,  // << ButtonType: the button exists in the Flash front end
    kAptButtonMask = 0xFF00,
  };
  static const char* s_pButtonName[ButtonType::Count];  // 80423E50

  // `items` slots are reserved (Startup empties the list again).
  WrapperPaneBase(int id, int items);  // 800878C4
  virtual ~WrapperPaneBase();  // 800879E0
  virtual void Startup();  // 80084A8C
  virtual void Shutdown();  // 80084AB4
  virtual void Reset();  // 80087A64
  virtual void DrawBackground(ERC* rc);  // 80084BF8
  virtual void DrawItems(ERC* rc, bool firstPass);  // 80084E84

  void SetSelected(ItemType type, int index);  // 80084F30
  PaneItem* GetSelectedItem(ItemType type);  // 80084F98
  int GetEnabledItemIndexWithValue(ItemType type, int value);  // 80084FDC
  PaneItem* GetItem(ItemType type, int index);  // 80085034
  void SetItemText(ItemType type, int index, const unsigned short* text);  // 8008507C
  void SetItemFont(ItemType type, int index, ERFont* font);  // 800850B0
  void SetItemFontSize(ItemType type, int index, float size);  // 800850F8
  void SetItemStyle(ItemType type, int index, ItemStyle style);  // 80085148
  void SetItemPos(ItemType type, int index, EVec2& pos);  // 8008517C
  void SetItemPosX(ItemType type, int index, float x);  // 800851C4
  void SetItemPosY(ItemType type, int index, float y);  // 8008520C
  void SetItemSize(ItemType type, int index, EVec2& size);  // 80085254
  void SetItemSizeX(ItemType type, int index, float x);  // 8008529C
  void SetItemSizeY(ItemType type, int index, float y);  // 800852E4
  void SetItemShader(ItemType type, int index, unsigned int id);  // 8008532C
  const unsigned short* GetItemText(ItemType type, int index);  // 8008536C
  ERFont* GetItemFont(ItemType type, int index);  // 80085390
  float GetItemFontSize(ItemType type, int index);  // 800853B4
  ItemStyle GetItemStyle(ItemType type, int index);  // 800853D8
  EVec2* GetItemPos(ItemType type, int index);  // 800853FC
  float GetItemPosX(ItemType type, int index);  // 80085434
  float GetItemPosY(ItemType type, int index);  // 8008546C
  EVec2* GetItemSize(ItemType type, int index);  // 800854A4
  float GetItemSizeX(ItemType type, int index);  // 800854DC
  float GetItemSizeY(ItemType type, int index);  // 80085514
  int CalculateMenuWidthPixels();  // 8008554C
  void SetBackgroundShader(unsigned int id);  // 80085638
  void ReleaseBackgroundShader();  // 80085678
  void DeleteItemList();  // 800856B8
  int GetVariableButtonIndex(char* name, char* field);  // 800857E0
  char* GetVariable(char* name);  // 80085960
  bool SetVariable(char* name, char* value);  // 80085AA0
  void AddButton(ButtonType type, bool createApt, const unsigned short* text);  // 80085B9C
  void ShowEnabledButtons();  // 80085EBC
  void HideEnabledButtons();  // 80085FF0
  void ShowButton(ButtonType type);  // 80086124
  void HideButton(ButtonType type);  // 80086230
  void UpdateAptButtonPosX(ButtonType type);  // 8008633C
  void UpdateAptButtonPosY(ButtonType type);  // 8008636C
  void UpdateAptButtonWidth(ButtonType type);  // 8008639C
  int GetPaneId();  // 80087A5C
  void EnableDraw();  // 80087A68
  void DisableDraw();  // 80087A98
  unsigned int IsDrawEnabled();  // 80087AC8
  void SetAlpha(float a);  // 80087AD4
  float GetAlpha();  // 80087ADC
  void SetState(PaneState state);  // 80087AE4
  void SetStateRunning();  // 80087AEC
  void SetStateAccepted();  // 80087AF8
  void SetStateDeclined();  // 80087B04
  void SetStateAlt1();  // 80087B10
  PaneState GetState() const;  // 80087B1C
  bool IsStateRunning() const;  // 80087B24
  bool IsStateAccepted() const;  // 80087B34
  bool IsStateDeclined() const;  // 80087B48
  bool IsStateAlt1() const;  // 80087B5C
  PaneItem* AddItem(PaneItem* item);  // 80087B70
  PaneItem* GetTextItem(int index);  // 80087CC4
  PaneItem* GetButtonItem(int index);  // 80087CEC
  PaneItem* GetMenuItem(int index);  // 80087D14
  PaneItem* GetIconItem(int index);  // 80087D3C
  void SetTextItemText(int index, const unsigned short* text);  // 80087D64
  void SetTextItemFont(int index, ERFont* font);  // 80087D90
  void SetTextItemFontSize(int index, float size);  // 80087DBC
  void SetTextItemStyle(int index, ItemStyle style);  // 80087DE4
  void SetTextItemPos(int index, EVec2& pos);  // 80087E10
  void SetTextItemPosX(int index, float x);  // 80087E3C
  void SetTextItemPosY(int index, float y);  // 80087E64
  void SetButtonItemText(int index, const unsigned short* text);  // 80087E8C
  void SetButtonItemFont(int index, ERFont* font);  // 80087EB8
  void SetButtonItemFontSize(int index, float size);  // 80087EE4
  void SetButtonItemStyle(int index, ItemStyle style);  // 80087F0C
  void SetButtonItemPos(int index, EVec2& pos);  // 80087F38
  void SetButtonItemPosX(int index, float x);  // 80087F64
  void SetButtonItemPosY(int index, float y);  // 80087F8C
  void SetMenuItemText(int index, const unsigned short* text);  // 80087FB4
  void SetMenuItemFont(int index, ERFont* font);  // 80087FE0
  void SetMenuItemFontSize(int index, float size);  // 8008800C
  void SetMenutItemStyle(int index, ItemStyle style);  // 80088034
  void SetMenuItemPos(int index, EVec2& pos);  // 80088060
  void SetMenuItemPosX(int index, float x);  // 8008808C
  void SetMenuItemPosY(int index, float y);  // 800880B4
  void SetIconItemShader(int index, unsigned int id);  // 800880DC
  void SetIconItemPos(int index, EVec2& pos);  // 80088108
  void SetIconItemPosX(int index, float x);  // 80088134
  void SetIconItemPosY(int index, float y);  // 8008815C
  void SetIconItemSize(int index, EVec2& size);  // 80088184
  void SetIconItemSizeX(int index, float x);  // 800881B0
  void SetIconItemSizeY(int index, float y);  // 800881D8
  ERShader* GetBackgroundShader();  // 80088200
  void SetBackgroundRect(EVec2* p0, EVec2* p1);  // 80088208
  void SetBackgroundAlpha(float a);  // 8008822C
  void SetButtonContext(char* context);  // 80088234
  TextBaseItem* GetButton(ButtonType type);  // 8008827C
  const unsigned short* GetButtonText(ButtonType type) const;  // 8008828C
  ERFont* GetButtonFont(ButtonType type) const;  // 800882A0
  float GetButtonFontSize(ButtonType type) const;  // 800882B4
  EVec2* GetButtonPos(ButtonType type) const;  // 800882C8
  float GetButtonPosX(ButtonType type) const;  // 80088308
  float GetButtonPosY(ButtonType type) const;  // 80088348
  void SetButtonText(ButtonType type, const unsigned short* text);  // 80088388
  void SetButtonFont(ButtonType type, ERFont* font);  // 800883D4
  void SetButtonFontSize(ButtonType type, float size);  // 80088434
  void SetButtonPos(ButtonType type, EVec2& pos);  // 80088494
  void SetButtonPosX(ButtonType type, float x);  // 80088500
  void SetButtonPosY(ButtonType type, float y);  // 8008855C
  TextBaseItem* GetAcceptButton();  // 800885B8
  void SetAcceptText(const unsigned short* text);  // 800885C0
  void SetAcceptFont(ERFont* font);  // 800885FC
  void SetAcceptFontSize(float size);  // 8008864C
  void SetAcceptPos(EVec2& pos);  // 800886A0
  void SetAcceptPosX(float x);  // 800886FC
  void SetAcceptPosY(float y);  // 8008874C
  TextBaseItem* GetDeclineButton();  // 8008879C
  void SetDeclineText(const unsigned short* text);  // 800887A4
  void SetDeclineFont(ERFont* font);  // 800887E0
  void SetDeclineFontSize(float size);  // 80088830
  void SetDeclinePos(EVec2& pos);  // 80088884
  void SetDeclinePosX(float x);  // 800888E0
  void SetDeclinePosY(float y);  // 80088930
  TextBaseItem* GetAlt1Button();  // 80088980
  void SetAlt1Text(const unsigned short* text);  // 80088988
  void SetAlt1Font(ERFont* font);  // 800889C4
  void SetAlt1FontSize(float size);  // 80088A14
  void SetAlt1Pos(EVec2& pos);  // 80088A68
  void SetAlt1PosX(float x);  // 80088AC4
  void SetAlt1PosY(float y);  // 80088B14
  std::vector<PaneItem*>* GetItemList();  // 80088B64

  // PC: the Apt calls go through here ("setButtonContext" with this pane's context first).
  void AptSetButtonContext();
  static void AptCall(const char* function, int argc, const char* arg0 = nullptr, const char* arg1 = nullptr);

  int m_id;                                  // +0x00
  unsigned int m_flags = 0;                  // +0x04
  std::vector<PaneItem*> m_items;            // +0x08
  ERShader* m_background = nullptr;          // +0x18
  EVec2 m_backgroundP0{0.0f, 0.0f};          // +0x1C
  EVec2 m_backgroundP1{1.0f, 1.0f};          // +0x24
  float m_backgroundAlpha = 1.0f;            // +0x2C
  float m_alpha = 1.0f;                      // +0x30
  PaneState m_state = 0;                     // +0x34
  const char* m_buttonContext = nullptr;     // +0x38 prefix of this pane's Apt variable names
  int m_buttonContextLength = 0;             // +0x3C
  TextBaseItem* m_buttons[ButtonType::Count] = {};  // +0x40
};

// A pane with `texts` text items (inline in game/thesims.cpp, e.g. the Maxis screen).
struct BackgroundPane : WrapperPaneBase {
  BackgroundPane(int id, int texts) : WrapperPaneBase(id, texts) {
    WrapperPaneBase::Startup();
    for (int i = 0; i < texts; ++i) {
      auto* item = new TextItem;
      item->m_flags = PaneItem::kEnabled;
      m_items.push_back(item);
    }
  }
  ~BackgroundPane() override = default;  // 803A1988
};

// A pane with a title and a body text item (items 0 and 1) before its own.
struct DialogPaneBase : WrapperPaneBase {
  DialogPaneBase(int id, int items);  // 80088B6C
  ~DialogPaneBase() override;  // 80088CA4
  void Startup() override;  // 800863CC
  void Shutdown() override;  // 80086778
  TextBaseItem* GetTitleTextItem();  // 80088D20
  TextBaseItem* GetBodyTextItem();  // 80088D28
  void SetTitleText(const unsigned short* text);  // 80088D30
  void SetTitleFont(ERFont* font);  // 80088D54
  void SetTitleFontSize(float size);  // 80088D94
  void SetTitlePos(EVec2 pos);  // 80088DD4
  void SetTitlePosX(float x);  // 80088E0C
  void SetTitlePosY(float y);  // 80088E44
  void SetBodyText(const unsigned short* text);  // 80088E7C
  void SetBodyFont(ERFont* font);  // 80088EA0
  void SetBodyFontSize(float size);  // 80088EE0
  void SetBodyPos(EVec2 pos);  // 80088F20
  void SetBodyPosX(float x);  // 80088F58
  void SetBodyPosY(float y);  // 80088F90
  void SetBodyDisplaySize(EVec2& size);  // 80088FC8
  EVec2* GetBodyDisplaySize();  // 80089000
  float GetBodyDisplaySizeX();  // 8008900C
  float GetBodyDisplaySizeY();  // 80089018
  void SetBodyDisplayPadSize(EVec2& size);  // 80089024
  EVec2* GetBodyDisplayPadSize();  // 8008905C
  float GetBodyDisplayPadSizeX();  // 80089068
  float GetBodyDisplayPadSizeY();  // 80089074

  TextBaseItem* m_title = nullptr;  // +0x60
  TextBaseItem* m_body = nullptr;   // +0x64
};

struct MenuPane : WrapperPaneBase {
  using WrapperPaneBase::WrapperPaneBase;
  int GetSelectedMenuItemValue();  // 80086798
};
struct MenuDialogPane : DialogPaneBase {
  using DialogPaneBase::DialogPaneBase;
  ItemStyle GetSelectedMenuItemStyle();  // 800867D0
  int GetSelectedMenuItemValue();  // 80086808
};

struct Wrapper : UIObjectBase {
  static ERFont* s_pDefaultFont;          // 80500580
  static ItemStyle s_defaultButtonStyle;  // 80500584
  static ItemStyle s_defaultTextStyle;    // 80500588
  static ItemStyle s_defaultMenuStyle;    // 8050058C
  static float s_fDefaultTextFontSize;    // 80500598
  static float s_fDefaultMenuFontSize;    // 8050059C
  static float s_fDefaultButtonFontSize;  // 805005A0

  Wrapper() = default;
  virtual ~Wrapper();  // 803A06B4
  void WrapperStartup(UIObjectBase* handler);  // 80086840
  void WrapperShutdown();  // 80086848
  static void WrapperRestoreMessageHandler();  // 80086984
  static void WrapperRemoveMessageHandler();  // 80086988
  static void WrapperReset();  // 8008698C
  static void WrapperUpdate();  // 80086990
  void WrapperDraw(ERC* rc);  // 80086994
  void WrapperCallFunction(char* name);  // 80086AE8
  unsigned int WrapperReadController(bool, int controller);  // 80086AEC the pressed-button mask
  WrapperPaneBase* AddPane(WrapperPaneBase* pane);  // 80086B1C
  void ReleasePaneBackgrounds();  // 80086C84
  WrapperPaneBase* FindPaneById(int id);  // 80086CD8
  WrapperPaneBase* FindPane(WrapperPaneBase* pane);  // 80086D10

  const char* m_field84 = "";                // +0x84 (an empty string; use not identified)
  UIObjectBase* m_messageHandler = nullptr;  // +0x88 removed from the Apt viewer at shutdown
  std::vector<WrapperPaneBase*> m_panes;     // +0x8C
  int m_field9C = 1;                         // +0x9C
  int m_drawSecondPass = 1;                  // +0xA0 also draw the items without kFirstPass
};

void DrawIcon(ERC* rc, EVec2& pos, EVec2& size, EVec4& color, ERShader* shader);  // 800842D8
