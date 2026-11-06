// Port of game/wrapper.cpp: the Wrapper UI toolkit (panes of text, menu, button and icon items).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

ERFont* Wrapper::s_pDefaultFont = nullptr;    // 80500580
ItemStyle Wrapper::s_defaultButtonStyle = 0;  // 80500584
ItemStyle Wrapper::s_defaultTextStyle = 0;    // 80500588
ItemStyle Wrapper::s_defaultMenuStyle = 0;    // 8050058C
float Wrapper::s_fDefaultTextFontSize = 0.0f;    // 80500598
float Wrapper::s_fDefaultMenuFontSize = 0.0f;    // 8050059C
float Wrapper::s_fDefaultButtonFontSize = 0.0f;  // 805005A0

const char* WrapperPaneBase::s_pButtonName[ButtonType::Count] = {  // 80423E50
    "accept", "decline", "alt1", "alt2", "flipper1", "flipper2", "walls"};

namespace {
// The colour constants of game/global.cpp (_WHITE, _BLUE, ...), as its static initialiser sets them.
constexpr EVec4 kWhite{1.0f, 1.0f, 1.0f, 1.0f};
constexpr EVec4 kBlueBlack{0.171875f, 0.140625f, 0.390625f, 1.0f};
constexpr EVec4 kBlue{0.0f, 0.08235f, 0.435294f, 1.0f};
constexpr EVec4 kDarkGrey{0.25f, 0.25f, 0.25f, 1.0f};
constexpr EVec4 kLightGrey{0.75f, 0.75f, 0.75f, 1.0f};
constexpr EVec4 kYellow{0.86f, 0.86f, 0.16f, 1.0f};
constexpr EVec4 kOrange{0.992f, 0.773f, 0.259f, 1.0f};
}  // namespace

// 80086D40 (static initialisation of game/wrapper.cpp)
// 80089080 (its caller, the unit's global constructor)
TextBaseItem::Style TextBaseItem::s_style[ItemStyle::Count] = {  // 80485810
    {kWhite, 0}, {kBlueBlack, 0}, {kBlue, 0}, {kDarkGrey, 0}, {kLightGrey, 0},
    {kWhite, 1}, {kYellow, 1},    {kBlue, 0}, {kWhite, 0},    {kOrange, 0},
};
int TextBaseItem::s_bStyleInited = 0;

// 800842D8
// The shader stretched over a rectangle of `size` centred on `pos`, blended when not opaque.
void DrawIcon(ERC* rc, EVec2& pos, EVec2& size, EVec4& color, ERShader* shader) {
  EVec2 p0{pos.x - size.x * 0.5f, pos.y - size.y * 0.5f};
  shader->GetShader()->Select(rc, 0);
  if (color.w != 1.0f) rc->EnableRasterModes(0x40, 0);
  EVec2 p1{p0.x + size.x, p0.y + size.y};
  EVec2 uv0{0.0f, 1.0f}, uv1{1.0f, 0.0f};
  rc->Rect(p0, p1, uv0, uv1, color, 0.0f);
  if (color.w != 1.0f) rc->DisableRasterModes(0x40, 0);
}

// ---------------------------------------------------------------------------------------------------
// TextBaseItem

// 80086FF4
TextBaseItem::TextBaseItem(ItemType type) : PaneItem(type) {
  m_block.Clear();
  TextBaseItem::Startup();
  TextBaseItem::Reset();
}

// 80087088
TextBaseItem::~TextBaseItem() {
  if (m_block.m_color) MainHeap()->Free(m_block.m_color);
}

// 80084464
// Default font; centred on its position both ways, starting at the vertical middle.
void TextBaseItem::Startup() {
  m_block.m_font = Wrapper::s_pDefaultFont;
  m_block.SetDirty();
  m_block.m_lineGap = 0.00223f;
  m_block.m_flags = (m_block.m_flags & ~TextBlock::kAlignLeft) | TextBlock::kVerticalCenter | TextBlock::kPosIsCenterX |
                    TextBlock::kPosIsCenterY;
}

// 800844B8
// A selected item is drawn 1.178 times larger, and orange unless its style is 7.
void TextBaseItem::Draw(ERC* rc, float alpha) {
  if (!(m_flags & kEnabled)) return;
  const ItemStyle style = m_style;
  const float size = m_block.m_fontSize;
  if (m_flags & kSelected) {
    m_block.m_fontSize = size * 1.178f;
    MarkDirty();
    if (style != ItemStyle::Unhighlighted) SetStyle(ItemStyle::Highlight);
  }
  m_block.DrawText(rc, alpha);
  if (m_flags & kSelected) {
    m_block.m_fontSize = size;
    MarkDirty();
    SetStyle(style);
  }
}

// 80084594
void TextBaseItem::SetText(const unsigned short* text) {
  if (!text) return;
  m_block.m_text = text;
  MarkDirty();
}

// 800845DC
// GC: the first call copied the presets into s_style again (they are constant here).
void TextBaseItem::SetStyle(ItemStyle style) {
  m_style = style;
  s_bStyleInited = 1;
  *m_block.m_color = s_style[style].color;
  if (s_style[style].shadow) m_block.m_flags |= TextBlock::kDropShadow;
  else m_block.m_flags &= ~TextBlock::kDropShadow;
}

// 80087104
void TextBaseItem::Reset() { m_flags = kEnabled; }

// 80087110
void TextBaseItem::CalculateSize() {
  if (m_block.m_flags & TextBlock::kDirty) m_block.CalcSizes();
  SetSize(m_block.m_textSize);
}

// 80087170
void TextBaseItem::SetColor(EVec4& color) { *m_block.m_color = color; }
// 80087198
EVec4* TextBaseItem::GetColor() { return m_block.m_color; }
// 800871A0
void TextBaseItem::SetAlpha(float a) { m_block.m_color->w = a; }
// 800871AC
float TextBaseItem::GetAlpha() { return m_block.m_color->w; }
// 800871B8
void TextBaseItem::SetShadowAlpha(float a) { m_block.m_shadowAlpha = a; }
// 800871C0
float TextBaseItem::GetShadowAlpha() { return m_block.m_shadowAlpha; }
// 800871C8
void TextBaseItem::SetShadowSize(float size) { m_block.m_shadowSize = size; }
// 800871D0
float TextBaseItem::GetShadowSize() { return m_block.m_shadowSize; }
// 800871D8
void TextBaseItem::SetPos(EVec2& pos) { m_block.m_pos = pos; }
// 800871EC
EVec2* TextBaseItem::GetPos() { return &m_block.m_pos; }
// 800871F4
void TextBaseItem::SetPosX(float x) { m_block.m_pos.x = x; }
// 800871FC
float TextBaseItem::GetPosX() { return m_block.m_pos.x; }
// 80087204
void TextBaseItem::SetPosY(float y) { m_block.m_pos.y = y; }
// 8008720C
float TextBaseItem::GetPosY() { return m_block.m_pos.y; }

// 80087214
void TextBaseItem::SetDisplaySize(EVec2& size) {
  m_block.m_size = size;
  m_block.SetDirty();
}
// 8008724C
EVec2* TextBaseItem::GetDisplaySize() { return &m_block.m_size; }
// 80087254
float TextBaseItem::GetDisplaySizeX() { return m_block.m_size.x; }
// 8008725C
float TextBaseItem::GetDisplaySizeY() { return m_block.m_size.y; }
// 80087264
void TextBaseItem::SetDisplayPadSize(EVec2& size) {
  m_block.m_padSize = size;
  m_block.SetDirty();
}
// 8008729C
EVec2* TextBaseItem::GetDisplayPadSize() { return &m_block.m_padSize; }
// 800872A4
float TextBaseItem::GetDisplayPadSizeX() { return m_block.m_padSize.x; }
// 800872AC
float TextBaseItem::GetDisplayPadSizeY() { return m_block.m_padSize.y; }

// 800872B4
void TextBaseItem::SetVerticalCenter(bool on) {
  if (on) m_block.m_flags |= TextBlock::kVerticalCenter;
  else m_block.m_flags &= ~TextBlock::kVerticalCenter;
}
// 800872E0
void TextBaseItem::SetYPosCentered(bool on) {
  if (on) m_block.m_flags |= TextBlock::kPosIsCenterY;
  else m_block.m_flags &= ~TextBlock::kPosIsCenterY;
}

// 8008730C
const unsigned short* TextBaseItem::GetText() const { return m_block.m_text; }
// 80087314
void TextBaseItem::SetFont(ERFont* font) {
  m_block.m_font = font;
  MarkDirty();
}
// 80087354
ERFont* TextBaseItem::GetFont() const { return m_block.m_font; }
// 8008735C
void TextBaseItem::SetFontSize(float size) {
  m_block.m_fontSize = size;
  MarkDirty();
}
// 8008739C
float TextBaseItem::GetFontSize() const { return m_block.m_fontSize; }
// 800873A4
ItemStyle TextBaseItem::GetStyle() const { return m_style; }

// ---------------------------------------------------------------------------------------------------
// TextItem, MenuItem, ButtonItem: text items with their own default style and size.

// 800873AC
TextItem::TextItem() : TextBaseItem(ItemType::Text) {
  TextItem::Startup();
  m_flags = kEnabled;
}
// 80087458
TextItem::~TextItem() = default;
// 8008480C
void TextItem::Startup() {
  TextBaseItem::Startup();
  SetStyle(Wrapper::s_defaultTextStyle);
  m_block.m_fontSize = Wrapper::s_fDefaultTextFontSize;
  MarkDirty();
}

// 800874D4
MenuItem::MenuItem() : TextBaseItem(ItemType::Menu) {
  MenuItem::Startup();
  m_flags = kEnabled;
}
// 80087580
MenuItem::~MenuItem() = default;
// 80084864
void MenuItem::Startup() {
  TextBaseItem::Startup();
  SetStyle(Wrapper::s_defaultMenuStyle);
  m_block.m_fontSize = Wrapper::s_fDefaultMenuFontSize;
  MarkDirty();
}

// 800875FC
ButtonItem::ButtonItem() : TextBaseItem(ItemType::Button) {
  ButtonItem::Startup();
  m_flags = kEnabled;
}
// 800876AC
ButtonItem::~ButtonItem() = default;
// 800848BC
void ButtonItem::Startup() {
  TextBaseItem::Startup();
  SetStyle(Wrapper::s_defaultButtonStyle);
  m_block.m_fontSize = Wrapper::s_fDefaultButtonFontSize;
  MarkDirty();
}

// ---------------------------------------------------------------------------------------------------
// IconItem

// 80087728
IconItem::IconItem() : PaneItem(ItemType::Icon) {
  IconItem::Startup();
  m_flags = kEnabled;
}
// 800877C4
IconItem::~IconItem() = default;
// 80084914
void IconItem::Startup() { m_shader = nullptr; }
// 80084920
void IconItem::Shutdown() { ReleaseShader(); }

// 80084940
void IconItem::Draw(ERC* rc, float alpha) {
  if (!(m_flags & kEnabled) || !m_shader || !rc) return;
  CalculateSize();
  EVec4 color{m_color.x, m_color.y, m_color.z, m_color.w * alpha};
  if (color.w != 0.0f) DrawIcon(rc, m_pos, m_size, color, m_shader);
}

// 80084A0C
void IconItem::SetShader(unsigned int id) { m_shader = static_cast<ERShader*>(_shaderman.AddRef(id, nullptr, 0)); }
// 80084A4C
void IconItem::ReleaseShader() {
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
}
// 80087818
void IconItem::SetColor(EVec4& color) { m_color = color; }
// 80087840
EVec4 IconItem::GetColor() { return m_color; }
// 800878A4
void IconItem::SetAlpha(float a) { m_color.w = a; }
// 800878AC
float IconItem::GetAlpha() { return m_color.w; }
// 800878B4
void IconItem::SetShader(ERShader* shader) { m_shader = shader; }
// 800878BC
ERShader* IconItem::GetShader() { return m_shader; }

// ---------------------------------------------------------------------------------------------------
// WrapperPaneBase

// The Flash front end's button functions. PC: skipped while there is no Apt viewer.
void WrapperPaneBase::AptCall(const char* function, int argc, const char* arg0, const char* arg1) {
  auto* viewer = static_cast<AptViewer*>(_globals.m_aptViewer);
  if (!viewer) return;
  if (argc == 0) viewer->CallFunction(const_cast<char*>(function), nullptr, nullptr, 0);
  else if (argc == 1) viewer->CallFunction(const_cast<char*>(function), nullptr, nullptr, 1, arg0);
  else viewer->CallFunction(const_cast<char*>(function), nullptr, nullptr, 2, arg0, arg1);
}
void WrapperPaneBase::AptSetButtonContext() { AptCall("setButtonContext", 1, m_buttonContext ? m_buttonContext : ""); }

// 800878C4
WrapperPaneBase::WrapperPaneBase(int id, int items) : m_id(id), m_items(static_cast<std::size_t>(items), nullptr) {
  WrapperPaneBase::Startup();
}

// 800879E0
WrapperPaneBase::~WrapperPaneBase() { WrapperPaneBase::Shutdown(); }

// 80084A8C
void WrapperPaneBase::Startup() {
  m_items.clear();
  for (TextBaseItem*& button : m_buttons) button = nullptr;
}

// 80084AB4
// Releases the background, deletes the items and destroys the pane's Apt buttons.
void WrapperPaneBase::Shutdown() {
  if (m_background) {
    m_background->DelRef();
    m_background = nullptr;
  }
  DeleteItemList();
  if (m_flags & kAptButtonMask) {
    AptSetButtonContext();
    for (int i = 0; i < ButtonType::Count; ++i) {
      m_buttons[i] = nullptr;
      if (m_flags & (kAptButtonCreated << i)) AptCall("destroyButton", 1, s_pButtonName[i]);
    }
    AptCall("resetButtonContext", 0);
  }
}

// 80087A64
void WrapperPaneBase::Reset() {}

// 80084BF8
// Resets Apt buttons that moved since Apt last placed them, then draws the background shader over
// the background rectangle at the pane's alpha.
void WrapperPaneBase::DrawBackground(ERC* rc) {
  constexpr unsigned int kReset = PaneItem::kAptButtonDirty | PaneItem::kAptButtonXSet | PaneItem::kAptButtonYSet;
  for (int i = 0; i < ButtonType::Count; ++i) {
    TextBaseItem* button = m_buttons[i];
    if (button && (m_flags & (kAptButtonCreated << i)) && (button->m_flags & kReset) == kReset) {
      AptSetButtonContext();
      AptCall("resetButton", 1, s_pButtonName[i]);
      AptCall("resetButtonContext", 0);
      button->m_flags &= ~PaneItem::kAptButtonDirty;
    }
  }
  if (!(m_flags & kDrawEnabled) || !m_background) return;
  m_background->GetShader()->Select(rc, 0);
  const float alpha = m_alpha * m_backgroundAlpha;
  if (alpha <= 0.0f) return;
  const bool blend = alpha != 1.0f;
  if (blend) rc->EnableRasterModes(0x40, 0);
  EVec2 uv0{0.0f, 1.0f}, uv1{1.0f, 0.0f};
  EVec4 color{1.0f, 1.0f, 1.0f, alpha};
  rc->Rect(m_backgroundP0, m_backgroundP1, uv0, uv1, color, 0.0f);
  if (blend) rc->DisableRasterModes(0x40, 0);
}

// 80084E84
void WrapperPaneBase::DrawItems(ERC* rc, bool firstPass) {
  if (!(m_flags & kDrawEnabled) || m_alpha == 0.0f) return;
  for (PaneItem* item : m_items)
    if (((item->m_flags & PaneItem::kFirstPass) != 0) == firstPass) item->Draw(rc, m_alpha);
}

// 80084F30
// Selects the index-th enabled item of the type and deselects the others.
void WrapperPaneBase::SetSelected(ItemType type, int index) {
  int n = 0;
  for (PaneItem* item : m_items) {
    if (item->m_type != type) continue;
    const unsigned int flags = item->m_flags;
    item->m_flags = flags & ~PaneItem::kSelected;
    if (flags & PaneItem::kEnabled) {
      if (n == index) item->m_flags |= PaneItem::kSelected;
      ++n;
    }
  }
}

// 80084F98
PaneItem* WrapperPaneBase::GetSelectedItem(ItemType type) {
  for (PaneItem* item : m_items)
    if (item->m_type == type && (item->m_flags & PaneItem::kSelected)) return item;
  return nullptr;
}

// 80084FDC
// The position among the enabled items of the type of the one with this value, or -1.
int WrapperPaneBase::GetEnabledItemIndexWithValue(ItemType type, int value) {
  int n = 0;
  for (PaneItem* item : m_items) {
    if (item->m_type == type && (item->m_flags & PaneItem::kEnabled)) {
      if (item->m_value == value) return n;
      ++n;
    }
  }
  return -1;
}

// 80085034
PaneItem* WrapperPaneBase::GetItem(ItemType type, int index) {
  int n = 0;
  for (PaneItem* item : m_items) {
    if (item->m_type == type) {
      if (n == index) return item;
      ++n;
    }
  }
  return nullptr;
}

namespace {
TextBaseItem* AsText(PaneItem* item) { return static_cast<TextBaseItem*>(item); }
}  // namespace

// 8008507C
void WrapperPaneBase::SetItemText(ItemType type, int index, const unsigned short* text) {
  AsText(GetItem(type, index))->SetText(text);
}
// 800850B0
void WrapperPaneBase::SetItemFont(ItemType type, int index, ERFont* font) {
  TextBaseItem* item = AsText(GetItem(type, index));
  item->m_block.m_font = font;
  item->MarkDirty();
}
// 800850F8
void WrapperPaneBase::SetItemFontSize(ItemType type, int index, float size) {
  TextBaseItem* item = AsText(GetItem(type, index));
  item->m_block.m_fontSize = size;
  item->MarkDirty();
}
// 80085148
void WrapperPaneBase::SetItemStyle(ItemType type, int index, ItemStyle style) {
  AsText(GetItem(type, index))->SetStyle(style);
}
// 8008517C
void WrapperPaneBase::SetItemPos(ItemType type, int index, EVec2& pos) { GetItem(type, index)->SetPos(pos); }
// 800851C4
void WrapperPaneBase::SetItemPosX(ItemType type, int index, float x) { GetItem(type, index)->SetPosX(x); }
// 8008520C
void WrapperPaneBase::SetItemPosY(ItemType type, int index, float y) { GetItem(type, index)->SetPosY(y); }
// 80085254
void WrapperPaneBase::SetItemSize(ItemType type, int index, EVec2& size) { GetItem(type, index)->SetSize(size); }
// 8008529C
void WrapperPaneBase::SetItemSizeX(ItemType type, int index, float x) { GetItem(type, index)->SetSizeX(x); }
// 800852E4
void WrapperPaneBase::SetItemSizeY(ItemType type, int index, float y) { GetItem(type, index)->SetSizeY(y); }
// 8008532C
void WrapperPaneBase::SetItemShader(ItemType type, int index, unsigned int id) {
  auto* item = static_cast<IconItem*>(GetItem(type, index));
  item->ReleaseShader();
  item->SetShader(id);
}
// 8008536C
const unsigned short* WrapperPaneBase::GetItemText(ItemType type, int index) {
  return AsText(GetItem(type, index))->m_block.m_text;
}
// 80085390
ERFont* WrapperPaneBase::GetItemFont(ItemType type, int index) { return AsText(GetItem(type, index))->m_block.m_font; }
// 800853B4
float WrapperPaneBase::GetItemFontSize(ItemType type, int index) {
  return AsText(GetItem(type, index))->m_block.m_fontSize;
}
// 800853D8
ItemStyle WrapperPaneBase::GetItemStyle(ItemType type, int index) { return AsText(GetItem(type, index))->m_style; }
// 800853FC
EVec2* WrapperPaneBase::GetItemPos(ItemType type, int index) { return GetItem(type, index)->GetPos(); }
// 80085434
// (PaneItem's const GetPosX: for a text item this is PaneItem::m_pos, not the text's position)
float WrapperPaneBase::GetItemPosX(ItemType type, int index) {
  return static_cast<const PaneItem*>(GetItem(type, index))->GetPosX();
}
// 8008546C
float WrapperPaneBase::GetItemPosY(ItemType type, int index) {
  return static_cast<const PaneItem*>(GetItem(type, index))->GetPosY();
}
// 800854A4
EVec2* WrapperPaneBase::GetItemSize(ItemType type, int index) { return GetItem(type, index)->GetSize(); }
// 800854DC
float WrapperPaneBase::GetItemSizeX(ItemType type, int index) { return GetItem(type, index)->GetSizeX(); }
// 80085514
float WrapperPaneBase::GetItemSizeY(ItemType type, int index) { return GetItem(type, index)->GetSizeY(); }

// 8008554C
// The widest menu item, in screen pixels.
int WrapperPaneBase::CalculateMenuWidthPixels() {
  float width = 0.0f;
  for (PaneItem* item : m_items) {
    if (item->m_type != ItemType::Menu) continue;
    const float w = item->GetSize()->x;
    if (width < w) width = w;
  }
  return static_cast<int>(width * static_cast<float>(_pGfx->m_screenWidth));
}

// 80085638
void WrapperPaneBase::SetBackgroundShader(unsigned int id) {
  m_background = static_cast<ERShader*>(_shaderman.AddRef(id, nullptr, 0));
}
// 80085678
void WrapperPaneBase::ReleaseBackgroundShader() {
  if (m_background) {
    m_background->DelRef();
    m_background = nullptr;
  }
}

// 800856B8
void WrapperPaneBase::DeleteItemList() {
  for (PaneItem* item : m_items) {
    item->Shutdown();
    delete item;
  }
  std::vector<PaneItem*>().swap(m_items);
}

// 800857E0
// Apt reads and writes the button prompts' layout as "[context_]button_<name>_<x|y|width>". Returns
// the button (when it exists in Apt) and the field's first letter, or -1.
int WrapperPaneBase::GetVariableButtonIndex(char* name, char* field) {
  if (m_buttonContext && m_buttonContextLength != 0 &&
      std::strncmp(name, m_buttonContext, static_cast<std::size_t>(m_buttonContextLength)) != 0)
    return -1;
  name += m_buttonContextLength;
  if (m_buttonContextLength != 0) ++name;
  if (std::strncmp(name, "button_", 7) != 0) return -1;
  name += 7;
  char* sep = std::strchr(name, '_');
  if (!sep) return -1;
  const char* what = sep + 1;
  if (std::strcmp(what, "x") != 0 && std::strcmp(what, "y") != 0 && std::strcmp(what, "width") != 0) return -1;
  const std::size_t len = static_cast<std::size_t>(sep - name);
  for (int i = 0; i < ButtonType::Count; ++i) {
    if (std::strncmp(name, s_pButtonName[i], len) != 0) continue;
    if (!(m_flags & (kAptButtonCreated << i))) return -1;
    if (len != std::strlen(s_pButtonName[i])) return -1;
    *field = sep[1];
    return i;
  }
  return -1;
}

// 80085960
// The value as text in a new Apt-heap string ("%f"); reading x or y clears its changed flag.
char* WrapperPaneBase::GetVariable(char* name) {
  char field[8];
  const int i = GetVariableButtonIndex(name, field);
  if (i < 0) return nullptr;
  auto* text = static_cast<char*>(AptHeap()->Malloc(0x20, 0));  // SimsAptMalloc
  text[0] = 0;
  TextBaseItem* button = m_buttons[i];
  if (field[0] == 'x') {
    std::snprintf(text, 0x20, "%f", button->GetPosX());
    button->m_flags &= ~PaneItem::kPosXChanged;
  } else if (field[0] == 'y') {
    std::snprintf(text, 0x20, "%f", button->GetPosY());
    button->m_flags &= ~PaneItem::kPosYChanged;
  } else {
    std::snprintf(text, 0x20, "%f", button->GetSize()->x);
  }
  return text;
}

// 80085AA0
// Apt placing a button: the position is taken unless the game moved the button since.
bool WrapperPaneBase::SetVariable(char* name, char* value) {
  char field[8];
  const int i = GetVariableButtonIndex(name, field);
  if (i < 0) return false;
  const float v = static_cast<float>(std::atof(value));
  TextBaseItem* button = m_buttons[i];
  if (field[0] == 'x') {
    if (!(button->m_flags & PaneItem::kPosXChanged)) {
      button->SetPosX(v);
      UpdateAptButtonPosX(i);
    }
    m_buttons[i]->m_flags |= PaneItem::kAptButtonXSet;
  } else if (field[0] == 'y') {
    if (!(button->m_flags & PaneItem::kPosYChanged)) {
      button->SetPosY(v);
      UpdateAptButtonPosY(i);
    }
    m_buttons[i]->m_flags |= PaneItem::kAptButtonYSet;
  }
  return true;
}

// 80085B9C
// Creates the button item (style 5) on first use, sets its text and optionally creates its Apt button.
void WrapperPaneBase::AddButton(ButtonType type, bool createApt, const unsigned short* text) {
  if (!m_buttons[type]) {
    auto* button = new ButtonItem;
    m_buttons[type] = button;
    button->m_flags = PaneItem::kEnabled;
    button->SetStyle(ItemStyle::WhiteShadow);
    m_items.push_back(m_buttons[type]);
    if (!m_buttons[type]) return;
  }
  m_buttons[type]->SetText(text);
  if (createApt && !(m_flags & (kAptButtonCreated << type))) {
    AptSetButtonContext();
    AptCall("createButton", 2, s_pButtonName[type], "center");
    AptCall("resetButtonContext", 0);
    m_flags |= kAptButtonCreated << type;
  }
}

// 80085EBC
void WrapperPaneBase::ShowEnabledButtons() {
  if (!(m_flags & kAptButtonMask)) return;
  AptSetButtonContext();
  for (int i = 0; i < ButtonType::Count; ++i) {
    TextBaseItem* button = m_buttons[i];
    if (button && (button->m_flags & PaneItem::kEnabled) && (m_flags & (kAptButtonCreated << i)))
      AptCall("showButton", 1, s_pButtonName[i]);
  }
  AptCall("resetButtonContext", 0);
}

// 80085FF0
void WrapperPaneBase::HideEnabledButtons() {
  if (!(m_flags & kAptButtonMask)) return;
  AptSetButtonContext();
  for (int i = 0; i < ButtonType::Count; ++i) {
    TextBaseItem* button = m_buttons[i];
    if (button && (button->m_flags & PaneItem::kEnabled) && (m_flags & (kAptButtonCreated << i)))
      AptCall("hideButton", 1, s_pButtonName[i]);
  }
  AptCall("resetButtonContext", 0);
}

// 80086124
void WrapperPaneBase::ShowButton(ButtonType type) {
  m_buttons[type]->m_flags |= PaneItem::kEnabled;
  if (!(m_flags & (kAptButtonCreated << type))) return;
  AptSetButtonContext();
  AptCall("showButton", 1, s_pButtonName[type]);
  AptCall("resetButtonContext", 0);
}

// 80086230
void WrapperPaneBase::HideButton(ButtonType type) {
  m_buttons[type]->m_flags &= ~PaneItem::kEnabled;
  if (!(m_flags & (kAptButtonCreated << type))) return;
  AptSetButtonContext();
  AptCall("hideButton", 1, s_pButtonName[type]);
  AptCall("resetButtonContext", 0);
}

// 8008633C
void WrapperPaneBase::UpdateAptButtonPosX(ButtonType type) {
  if (m_flags & (kAptButtonCreated << type)) m_buttons[type]->m_flags |= PaneItem::kPosXChanged | PaneItem::kAptButtonDirty;
}
// 8008636C
void WrapperPaneBase::UpdateAptButtonPosY(ButtonType type) {
  if (m_flags & (kAptButtonCreated << type)) m_buttons[type]->m_flags |= PaneItem::kPosYChanged | PaneItem::kAptButtonDirty;
}
// 8008639C
void WrapperPaneBase::UpdateAptButtonWidth(ButtonType type) {
  if (m_flags & (kAptButtonCreated << type)) m_buttons[type]->m_flags |= PaneItem::kWidthChanged | PaneItem::kAptButtonDirty;
}

// 80087A5C
int WrapperPaneBase::GetPaneId() { return m_id; }
// 80087A68
void WrapperPaneBase::EnableDraw() {
  m_flags |= kDrawEnabled;
  ShowEnabledButtons();
}
// 80087A98
void WrapperPaneBase::DisableDraw() {
  m_flags &= ~kDrawEnabled;
  HideEnabledButtons();
}
// 80087AC8
unsigned int WrapperPaneBase::IsDrawEnabled() { return m_flags & kDrawEnabled; }
// 80087AD4
void WrapperPaneBase::SetAlpha(float a) { m_alpha = a; }
// 80087ADC
float WrapperPaneBase::GetAlpha() { return m_alpha; }
// 80087AE4
void WrapperPaneBase::SetState(PaneState state) { m_state = state; }
// 80087AEC
void WrapperPaneBase::SetStateRunning() { m_state = PaneState::Running; }
// 80087AF8
void WrapperPaneBase::SetStateAccepted() { m_state = PaneState::Accepted; }
// 80087B04
void WrapperPaneBase::SetStateDeclined() { m_state = PaneState::Declined; }
// 80087B10
void WrapperPaneBase::SetStateAlt1() { m_state = PaneState::Alt1; }
// 80087B1C
WrapperPaneBase::PaneState WrapperPaneBase::GetState() const { return m_state; }
// 80087B24
bool WrapperPaneBase::IsStateRunning() const { return m_state == PaneState::Running; }
// 80087B34
bool WrapperPaneBase::IsStateAccepted() const { return m_state == PaneState::Accepted; }
// 80087B48
bool WrapperPaneBase::IsStateDeclined() const { return m_state == PaneState::Declined; }
// 80087B5C
bool WrapperPaneBase::IsStateAlt1() const { return m_state == PaneState::Alt1; }

// 80087B70
PaneItem* WrapperPaneBase::AddItem(PaneItem* item) {
  m_items.push_back(item);
  return item;
}

// 80087CC4
PaneItem* WrapperPaneBase::GetTextItem(int index) { return GetItem(ItemType::Text, index); }
// 80087CEC
PaneItem* WrapperPaneBase::GetButtonItem(int index) { return GetItem(ItemType::Button, index); }
// 80087D14
PaneItem* WrapperPaneBase::GetMenuItem(int index) { return GetItem(ItemType::Menu, index); }
// 80087D3C
PaneItem* WrapperPaneBase::GetIconItem(int index) { return GetItem(ItemType::Icon, index); }

// 80087D64
void WrapperPaneBase::SetTextItemText(int index, const unsigned short* text) { SetItemText(ItemType::Text, index, text); }
// 80087D90
void WrapperPaneBase::SetTextItemFont(int index, ERFont* font) { SetItemFont(ItemType::Text, index, font); }
// 80087DBC
void WrapperPaneBase::SetTextItemFontSize(int index, float size) { SetItemFontSize(ItemType::Text, index, size); }
// 80087DE4
void WrapperPaneBase::SetTextItemStyle(int index, ItemStyle style) { SetItemStyle(ItemType::Text, index, style); }
// 80087E10
void WrapperPaneBase::SetTextItemPos(int index, EVec2& pos) { SetItemPos(ItemType::Text, index, pos); }
// 80087E3C
void WrapperPaneBase::SetTextItemPosX(int index, float x) { SetItemPosX(ItemType::Text, index, x); }
// 80087E64
void WrapperPaneBase::SetTextItemPosY(int index, float y) { SetItemPosY(ItemType::Text, index, y); }
// 80087E8C
void WrapperPaneBase::SetButtonItemText(int index, const unsigned short* text) {
  SetItemText(ItemType::Button, index, text);
}
// 80087EB8
void WrapperPaneBase::SetButtonItemFont(int index, ERFont* font) { SetItemFont(ItemType::Button, index, font); }
// 80087EE4
void WrapperPaneBase::SetButtonItemFontSize(int index, float size) { SetItemFontSize(ItemType::Button, index, size); }
// 80087F0C
void WrapperPaneBase::SetButtonItemStyle(int index, ItemStyle style) { SetItemStyle(ItemType::Button, index, style); }
// 80087F38
void WrapperPaneBase::SetButtonItemPos(int index, EVec2& pos) { SetItemPos(ItemType::Button, index, pos); }
// 80087F64
void WrapperPaneBase::SetButtonItemPosX(int index, float x) { SetItemPosX(ItemType::Button, index, x); }
// 80087F8C
void WrapperPaneBase::SetButtonItemPosY(int index, float y) { SetItemPosY(ItemType::Button, index, y); }
// 80087FB4
void WrapperPaneBase::SetMenuItemText(int index, const unsigned short* text) { SetItemText(ItemType::Menu, index, text); }
// 80087FE0
void WrapperPaneBase::SetMenuItemFont(int index, ERFont* font) { SetItemFont(ItemType::Menu, index, font); }
// 8008800C
void WrapperPaneBase::SetMenuItemFontSize(int index, float size) { SetItemFontSize(ItemType::Menu, index, size); }
// 80088034
void WrapperPaneBase::SetMenutItemStyle(int index, ItemStyle style) { SetItemStyle(ItemType::Menu, index, style); }
// 80088060
void WrapperPaneBase::SetMenuItemPos(int index, EVec2& pos) { SetItemPos(ItemType::Menu, index, pos); }
// 8008808C
void WrapperPaneBase::SetMenuItemPosX(int index, float x) { SetItemPosX(ItemType::Menu, index, x); }
// 800880B4
void WrapperPaneBase::SetMenuItemPosY(int index, float y) { SetItemPosY(ItemType::Menu, index, y); }
// 800880DC
void WrapperPaneBase::SetIconItemShader(int index, unsigned int id) { SetItemShader(ItemType::Icon, index, id); }
// 80088108
void WrapperPaneBase::SetIconItemPos(int index, EVec2& pos) { SetItemPos(ItemType::Icon, index, pos); }
// 80088134
void WrapperPaneBase::SetIconItemPosX(int index, float x) { SetItemPosX(ItemType::Icon, index, x); }
// 8008815C
void WrapperPaneBase::SetIconItemPosY(int index, float y) { SetItemPosY(ItemType::Icon, index, y); }
// 80088184
void WrapperPaneBase::SetIconItemSize(int index, EVec2& size) { SetItemSize(ItemType::Icon, index, size); }
// 800881B0
void WrapperPaneBase::SetIconItemSizeX(int index, float x) { SetItemSizeX(ItemType::Icon, index, x); }
// 800881D8
void WrapperPaneBase::SetIconItemSizeY(int index, float y) { SetItemSizeY(ItemType::Icon, index, y); }

// 80088200
ERShader* WrapperPaneBase::GetBackgroundShader() { return m_background; }
// 80088208
void WrapperPaneBase::SetBackgroundRect(EVec2* p0, EVec2* p1) {
  m_backgroundP0 = *p0;
  m_backgroundP1 = *p1;
}
// 8008822C
void WrapperPaneBase::SetBackgroundAlpha(float a) { m_backgroundAlpha = a; }
// 80088234
void WrapperPaneBase::SetButtonContext(char* context) {
  m_buttonContext = context;
  m_buttonContextLength = context ? static_cast<int>(std::strlen(context)) : 0;
}

// 8008827C
TextBaseItem* WrapperPaneBase::GetButton(ButtonType type) { return m_buttons[type]; }
// 8008828C
const unsigned short* WrapperPaneBase::GetButtonText(ButtonType type) const { return m_buttons[type]->m_block.m_text; }
// 800882A0
ERFont* WrapperPaneBase::GetButtonFont(ButtonType type) const { return m_buttons[type]->m_block.m_font; }
// 800882B4
float WrapperPaneBase::GetButtonFontSize(ButtonType type) const { return m_buttons[type]->m_block.m_fontSize; }
// 800882C8
EVec2* WrapperPaneBase::GetButtonPos(ButtonType type) const { return m_buttons[type]->GetPos(); }
// 80088308
float WrapperPaneBase::GetButtonPosX(ButtonType type) const { return m_buttons[type]->GetPosX(); }
// 80088348
float WrapperPaneBase::GetButtonPosY(ButtonType type) const { return m_buttons[type]->GetPosY(); }

// 80088388
void WrapperPaneBase::SetButtonText(ButtonType type, const unsigned short* text) {
  m_buttons[type]->SetText(text);
  UpdateAptButtonWidth(type);
}
// 800883D4
void WrapperPaneBase::SetButtonFont(ButtonType type, ERFont* font) {
  m_buttons[type]->m_block.m_font = font;
  m_buttons[type]->MarkDirty();
  UpdateAptButtonWidth(type);
}
// 80088434
void WrapperPaneBase::SetButtonFontSize(ButtonType type, float size) {
  m_buttons[type]->m_block.m_fontSize = size;
  m_buttons[type]->MarkDirty();
  UpdateAptButtonWidth(type);
}
// 80088494
void WrapperPaneBase::SetButtonPos(ButtonType type, EVec2& pos) {
  m_buttons[type]->SetPos(pos);
  UpdateAptButtonPosX(type);
  UpdateAptButtonPosY(type);
}
// 80088500
void WrapperPaneBase::SetButtonPosX(ButtonType type, float x) {
  m_buttons[type]->SetPosX(x);
  UpdateAptButtonPosX(type);
}
// 8008855C
void WrapperPaneBase::SetButtonPosY(ButtonType type, float y) {
  m_buttons[type]->SetPosY(y);
  UpdateAptButtonPosY(type);
}

// 800885B8
TextBaseItem* WrapperPaneBase::GetAcceptButton() { return m_buttons[ButtonType::Accept]; }
// 800885C0
void WrapperPaneBase::SetAcceptText(const unsigned short* text) { SetButtonText(ButtonType::Accept, text); }
// 800885FC
void WrapperPaneBase::SetAcceptFont(ERFont* font) { SetButtonFont(ButtonType::Accept, font); }
// 8008864C
void WrapperPaneBase::SetAcceptFontSize(float size) { SetButtonFontSize(ButtonType::Accept, size); }
// 800886A0
void WrapperPaneBase::SetAcceptPos(EVec2& pos) { SetButtonPos(ButtonType::Accept, pos); }
// 800886FC
void WrapperPaneBase::SetAcceptPosX(float x) { SetButtonPosX(ButtonType::Accept, x); }
// 8008874C
void WrapperPaneBase::SetAcceptPosY(float y) { SetButtonPosY(ButtonType::Accept, y); }
// 8008879C
TextBaseItem* WrapperPaneBase::GetDeclineButton() { return m_buttons[ButtonType::Decline]; }
// 800887A4
void WrapperPaneBase::SetDeclineText(const unsigned short* text) { SetButtonText(ButtonType::Decline, text); }
// 800887E0
void WrapperPaneBase::SetDeclineFont(ERFont* font) { SetButtonFont(ButtonType::Decline, font); }
// 80088830
void WrapperPaneBase::SetDeclineFontSize(float size) { SetButtonFontSize(ButtonType::Decline, size); }
// 80088884
void WrapperPaneBase::SetDeclinePos(EVec2& pos) { SetButtonPos(ButtonType::Decline, pos); }
// 800888E0
void WrapperPaneBase::SetDeclinePosX(float x) { SetButtonPosX(ButtonType::Decline, x); }
// 80088930
void WrapperPaneBase::SetDeclinePosY(float y) { SetButtonPosY(ButtonType::Decline, y); }
// 80088980
TextBaseItem* WrapperPaneBase::GetAlt1Button() { return m_buttons[ButtonType::Alt1]; }
// 80088988
void WrapperPaneBase::SetAlt1Text(const unsigned short* text) { SetButtonText(ButtonType::Alt1, text); }
// 800889C4
void WrapperPaneBase::SetAlt1Font(ERFont* font) { SetButtonFont(ButtonType::Alt1, font); }
// 80088A14
void WrapperPaneBase::SetAlt1FontSize(float size) { SetButtonFontSize(ButtonType::Alt1, size); }
// 80088A68
void WrapperPaneBase::SetAlt1Pos(EVec2& pos) { SetButtonPos(ButtonType::Alt1, pos); }
// 80088AC4
void WrapperPaneBase::SetAlt1PosX(float x) { SetButtonPosX(ButtonType::Alt1, x); }
// 80088B14
void WrapperPaneBase::SetAlt1PosY(float y) { SetButtonPosY(ButtonType::Alt1, y); }
// 80088B64
std::vector<PaneItem*>* WrapperPaneBase::GetItemList() { return &m_items; }

// ---------------------------------------------------------------------------------------------------
// DialogPaneBase, MenuPane, MenuDialogPane

// 80088B6C
// Two extra item slots are reserved for the title and the body.
DialogPaneBase::DialogPaneBase(int id, int items) : WrapperPaneBase(id, items + 2) { DialogPaneBase::Startup(); }

// 80088CA4
DialogPaneBase::~DialogPaneBase() = default;

// 800863CC
void DialogPaneBase::Startup() {
  WrapperPaneBase::Startup();
  m_title = new TextItem;
  m_title->m_flags = PaneItem::kEnabled;
  m_title->SetStyle(ItemStyle::WhiteShadow);
  m_items.push_back(m_title);
  m_body = new TextItem;
  m_body->m_flags = PaneItem::kEnabled;
  m_body->SetStyle(ItemStyle::WhiteShadow);
  m_items.push_back(m_body);
}

// 80086778
void DialogPaneBase::Shutdown() { WrapperPaneBase::Shutdown(); }

// 80088D20
TextBaseItem* DialogPaneBase::GetTitleTextItem() { return m_title; }
// 80088D28
TextBaseItem* DialogPaneBase::GetBodyTextItem() { return m_body; }
// 80088D30
void DialogPaneBase::SetTitleText(const unsigned short* text) { m_title->SetText(text); }
// 80088D54
void DialogPaneBase::SetTitleFont(ERFont* font) { m_title->SetFont(font); }
// 80088D94
void DialogPaneBase::SetTitleFontSize(float size) { m_title->SetFontSize(size); }
// 80088DD4
void DialogPaneBase::SetTitlePos(EVec2 pos) { m_title->SetPos(pos); }
// 80088E0C
void DialogPaneBase::SetTitlePosX(float x) { m_title->SetPosX(x); }
// 80088E44
void DialogPaneBase::SetTitlePosY(float y) { m_title->SetPosY(y); }
// 80088E7C
void DialogPaneBase::SetBodyText(const unsigned short* text) { m_body->SetText(text); }
// 80088EA0
void DialogPaneBase::SetBodyFont(ERFont* font) { m_body->SetFont(font); }
// 80088EE0
void DialogPaneBase::SetBodyFontSize(float size) { m_body->SetFontSize(size); }
// 80088F20
void DialogPaneBase::SetBodyPos(EVec2 pos) { m_body->SetPos(pos); }
// 80088F58
void DialogPaneBase::SetBodyPosX(float x) { m_body->SetPosX(x); }
// 80088F90
void DialogPaneBase::SetBodyPosY(float y) { m_body->SetPosY(y); }
// 80088FC8
void DialogPaneBase::SetBodyDisplaySize(EVec2& size) { m_body->SetDisplaySize(size); }
// 80089000
EVec2* DialogPaneBase::GetBodyDisplaySize() { return &m_body->m_block.m_size; }
// 8008900C
float DialogPaneBase::GetBodyDisplaySizeX() { return m_body->m_block.m_size.x; }
// 80089018
float DialogPaneBase::GetBodyDisplaySizeY() { return m_body->m_block.m_size.y; }
// 80089024
void DialogPaneBase::SetBodyDisplayPadSize(EVec2& size) { m_body->SetDisplayPadSize(size); }
// 8008905C
EVec2* DialogPaneBase::GetBodyDisplayPadSize() { return &m_body->m_block.m_padSize; }
// 80089068
float DialogPaneBase::GetBodyDisplayPadSizeX() { return m_body->m_block.m_padSize.x; }
// 80089074
float DialogPaneBase::GetBodyDisplayPadSizeY() { return m_body->m_block.m_padSize.y; }

// 80086798
int MenuPane::GetSelectedMenuItemValue() {
  PaneItem* item = GetSelectedItem(ItemType::Menu);
  return item ? item->m_value : -1;
}
// 800867D0
ItemStyle MenuDialogPane::GetSelectedMenuItemStyle() {
  PaneItem* item = GetSelectedItem(ItemType::Menu);
  return item ? static_cast<TextBaseItem*>(item)->m_style : ItemStyle(ItemStyle::Unhighlighted);
}
// 80086808
int MenuDialogPane::GetSelectedMenuItemValue() {
  PaneItem* item = GetSelectedItem(ItemType::Menu);
  return item ? item->m_value : -1;
}

// ---------------------------------------------------------------------------------------------------
// Wrapper

// 803A06B4
Wrapper::~Wrapper() { WrapperShutdown(); }

// 80086840
void Wrapper::WrapperStartup(UIObjectBase* handler) { m_messageHandler = handler; }

// 80086848
// Shuts down and deletes every pane.
void Wrapper::WrapperShutdown() {
  for (WrapperPaneBase* pane : m_panes) {
    pane->Shutdown();
    delete pane;
  }
  std::vector<WrapperPaneBase*>().swap(m_panes);
  if (m_messageHandler && _globals.m_aptViewer)
    static_cast<AptViewer*>(_globals.m_aptViewer)->NewRemoveMessageHandler(m_messageHandler);
}

// 80086984
void Wrapper::WrapperRestoreMessageHandler() {}
// 80086988
void Wrapper::WrapperRemoveMessageHandler() {}
// 8008698C
void Wrapper::WrapperReset() {}
// 80086990
void Wrapper::WrapperUpdate() {}

// 80086994
// Without depth testing: each visible pane's background and first-pass items, then (if any pane was
// visible and m_drawSecondPass is set) the other items of every visible pane on top.
void Wrapper::WrapperDraw(ERC* rc) {
  rc->ZTest(false, 2, 0, 0);
  bool drawn = false;
  for (WrapperPaneBase* pane : m_panes) {
    if (pane->m_alpha > 0.0f) {
      drawn = true;
      pane->DrawBackground(rc);
      pane->DrawItems(rc, true);
    }
  }
  if (!drawn || !m_drawSecondPass) return;
  for (WrapperPaneBase* pane : m_panes)
    if (pane->m_alpha > 0.0f) pane->DrawItems(rc, false);
}

// 80086AE8
void Wrapper::WrapperCallFunction(char*) {}

// 80086AEC
// The controller's pressed buttons (unfiltered).
unsigned int Wrapper::WrapperReadController(bool, int controller) {
  return _pCtrlMan->GetController(static_cast<unsigned int>(controller))->GetBtnPressedMask(0);
}

// 80086B1C
WrapperPaneBase* Wrapper::AddPane(WrapperPaneBase* pane) {
  if (!FindPane(pane)) m_panes.push_back(pane);
  return pane;
}

// 80086C84
void Wrapper::ReleasePaneBackgrounds() {
  for (WrapperPaneBase* pane : m_panes) pane->ReleaseBackgroundShader();
}

// 80086CD8
WrapperPaneBase* Wrapper::FindPaneById(int id) {
  for (WrapperPaneBase* pane : m_panes)
    if (pane->m_id == id) return pane;
  return nullptr;
}

// 80086D10
WrapperPaneBase* Wrapper::FindPane(WrapperPaneBase* pane) {
  for (WrapperPaneBase* p : m_panes)
    if (p == pane) return p;
  return nullptr;
}
