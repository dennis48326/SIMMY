// Port of the dialog box (game/targets_s2c.cpp: G2DTarget, UIDialog) and the reflow/tween blocks it
// sends (game/uireflow.cpp, the rest of UIReflow is in uiscreens.cpp).
#include "simmy/types_gen.hpp"
#include "simmy/platform.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

void* SimsAptMalloc(unsigned int size);
float AptAuxParagraphHeight(const unsigned short* text, float width, float fontSize);  // aptauxpceorgl.cpp
void SimsAptFree(void* p);
int PcPrivateScreenPlayer();  // uiscreens.cpp (PC co-op private screens)

namespace {
G2DTarget* TheG2D() { return static_cast<G2DTarget*>(_globals.m_g2dTarget); }
UIScreenManager* TheScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
void PlayUISound(unsigned int sound) { UIAUDIO::Instance()->PlaySound(sound); }
unsigned short* MallocText(const std::vector<unsigned short>& text) {
  size_t n = 0;
  while (n < text.size() && text[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc(static_cast<unsigned int>((n + 1) * 2)));
  for (size_t i = 0; i < n; ++i) out[i] = text[i];
  out[n] = 0;
  return out;
}
size_t TextLength(const std::vector<unsigned short>& text) {
  size_t n = 0;
  while (n < text.size() && text[n]) ++n;
  return n;
}
const float kDialogTimeout = 60000.0f;  // 804FE284 g_g2dDialogTimeout (ms)
}  // namespace

int G2DTarget::m_bInitComplete = 0;

// ---------------------------------------------------------------------------------------------------
// UIReflow (tweens and positions)

// 8017E138
// "<tween><id>..</id><dur>ms</dur><method>m</method><cb>callback</cb>" (player digit on ids without one).
void UIReflow::OpenTweenBlock(const char* id, unsigned int ms, unsigned int method, const char* callback, int player) {
  if (m_open) return;
  m_open = true;
  m_block.clear();
  char buf[32];
  AddToBlock("<tween>");
  AddToBlock("<id>");
  if (static_cast<unsigned>(static_cast<unsigned char>(id[0]) - '0') > 1) AddToBlock(player ? "1" : "0");
  AddToBlock(id);
  AddToBlock("</id>");
  AddToBlock("<dur>");
  std::snprintf(buf, sizeof buf, "%d", ms);
  AddToBlock(buf);
  AddToBlock("</dur>");
  AddToBlock("<method>");
  std::snprintf(buf, sizeof buf, "%d", method);
  AddToBlock(buf);
  AddToBlock("</method>");
  AddToBlock("<cb>");
  if (static_cast<unsigned>(static_cast<unsigned char>(callback[0]) - '0') > 1) AddToBlock(player ? "1" : "0");
  AddToBlock(callback);
  AddToBlock("</cb>");
}

// 8017E318
void UIReflow::SubmitTweenBlock() {
  if (!m_open) return;
  AddToBlock("</tween>");
  if (auto* v = static_cast<AptViewer*>(_globals.m_aptViewer))
    v->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "Tween", m_block.c_str());
  m_block.clear();
  m_open = false;
}

namespace {
void Tag(UIReflow* r, const char* open, const char* close, int v) {
  char buf[32];
  r->AddToBlock(open);
  std::snprintf(buf, sizeof buf, "%d", v);
  r->AddToBlock(buf);
  r->AddToBlock(close);
}
}  // namespace

// 8017E39C
void UIReflow::Tween_X(int x) { Tag(this, "<x>", "</x>", x); }
// 8017E408
void UIReflow::Tween_Y(int y) { Tag(this, "<y>", "</y>", y); }
// 8017E4E0
void UIReflow::Tween_H(unsigned int h) { Tag(this, "<h>", "</h>", static_cast<int>(h)); }
// 8017E54C
void UIReflow::Tween_W(unsigned int w) { Tag(this, "<w>", "</w>", static_cast<int>(w)); }
// 8017E5B8
// A scale (1: 100%) as a percentage.
void UIReflow::Tween_XScale(float scale) {
  char buf[128];
  AddToBlock("<xs>");
  std::snprintf(buf, sizeof buf, "%f", static_cast<double>(static_cast<float>(scale * 100.0)));
  AddToBlock(buf);
  AddToBlock("</xs>");
}
// 8017E638
void UIReflow::Tween_YScale(float scale) {
  char buf[128];
  AddToBlock("<ys>");
  std::snprintf(buf, sizeof buf, "%f", static_cast<double>(static_cast<float>(scale * 100.0)));
  AddToBlock(buf);
  AddToBlock("</ys>");
}
// 8017E6B8
// Degrees, one decimal.
void UIReflow::Tween_Rotation(float degrees) {
  char buf[128];
  AddToBlock("<rot>");
  std::snprintf(buf, sizeof buf, "%0.1f", static_cast<double>(degrees));
  AddToBlock(buf);
  AddToBlock("</rot>");
}
// 8017E72C
// Alpha 0..255 as a percentage.
void UIReflow::Tween_Alpha(unsigned int alpha) { Tag(this, "<alpha>", "</alpha>", static_cast<int>(alpha * 100 / 0xff)); }
// 8017E7A8
// RGBA: the colour, then the alpha.
void UIReflow::Tween_ColorOverride(unsigned int rgba) {
  char buf[16];
  AddToBlock("<clr>");
  AddToBlock("0x");
  std::snprintf(buf, sizeof buf, "%6.6x", rgba >> 8);
  AddToBlock(buf);
  AddToBlock("</clr>");
  Tween_Alpha(rgba & 0xff);
}
// 8017EAB0
void UIReflow::Reflow_X(int x) { Tween_X(x); }
// 8017EAD0
void UIReflow::Reflow_Y(int y) { Tween_Y(y); }
// 8017EB10
void UIReflow::Reflow_H(unsigned int h) { Tween_H(h); }
// 8017EB30
void UIReflow::Reflow_W(unsigned int w) { Tween_W(w); }
// 8017EB50
void UIReflow::Reflow_XScale(float scale) { Tween_XScale(scale); }
// 8017EB70
void UIReflow::Reflow_YScale(float scale) { Tween_YScale(scale); }
// 8017EB90
void UIReflow::Reflow_Rotation(float degrees) { Tween_Rotation(degrees); }
// 8017EC28
void UIReflow::Reflow_Alpha(unsigned int alpha) { Tween_Alpha(alpha); }
// 8017EC9C
void UIReflow::Reflow_ColorOverride(unsigned int rgba) { Tween_ColorOverride(rgba); }

// 8017E8B4
void UIReflow::Tween_FontSizeOverride(unsigned int size) { Tag(this, "<fnt>", "</fnt>", static_cast<int>(size)); }
// 8017ECDC
void UIReflow::Reflow_FontSizeOverride(unsigned int size) { Tween_FontSizeOverride(size); }

// 8017ECFC
void UIReflow::Reflow_SwfAnimation(char* animation) {
  AddToBlock("<swfAnim>");
  AddToBlock(animation);
  AddToBlock("</swfAnim>");
}

// 8017DDE8
// Restarts a widget swf's animation (a reflow of its own).
void UIReflow::ResetSwfAnimation(const char* id, const char* animation, int player) {
  if (m_open) return;
  std::string block = "<reflow><id>";
  block += player ? "1" : "0";
  block += id;
  block += "</id><swfAnim>";
  block += animation;
  block += "</swfAnim></reflow>";
  if (auto* v = static_cast<AptViewer*>(_globals.m_aptViewer))
    v->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 2, "Reflow", block.c_str());
}

// 8018003C
bool UIScreenManager::IsUIScreenUnloaded(int id) { return FindRecordIndexByID(id) == -1; }

// ---------------------------------------------------------------------------------------------------
// UIDialog

// 801E6F2C
UIDialog::UIDialog() = default;
// 801B7100
// Its icon and the icon's backdrop are let go.
UIDialog::~UIDialog() {
  if (m_icon) {
    m_icon->DelRef();
    m_icon = nullptr;
  }
  if (m_iconBg) {
    m_iconBg->DelRef();
    m_iconBg = nullptr;
  }
}
// 801B71B4
void UIDialog::SpawnDialog() {
  if (G2DTarget* g = TheG2D()) g->SpawnDialog(this);
}
// 801B71E0
void UIDialog::ChangeContinueType(int type) {
  if (G2DTarget* g = TheG2D()) g->ChangeContinueType(type);
}
// 801B7208
void UIDialog::HideDialog() {
  m_body.clear();
  for (auto& s : m_sel) s.clear();
  if (G2DTarget* g = TheG2D()) g->HideDialog();
}
// 801E7044
void UIDialog::SelectionCallback(int) {}
// 801E7048
void UIDialog::ZeroInputCallback() {}
// 801E704C
void UIDialog::DialogClosedCallback() {}

// ---------------------------------------------------------------------------------------------------
// G2DTarget

// 801B4490
G2DTarget::G2DTarget() {
  AttachTables("0G2DTarget");
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  for (const char* n : {"0G2D_exists", "0G2D_swf_boxWidth", "0G2D_swf_boxHeight", "0G2D_swf_boxHeight2"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"0G2D_show_callback", "0G2D_hide_callback", "0G2D_DPAD_UP", "0G2D_DPAD_DOWN", "0G2D_X_BUTTON",
                        "0G2D_SELECT_BUTTON", "0G2D_CANCEL_BUTTON"})
    m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"0G2D_title_text", "0G2D_body_text", "0G2D_sel0", "0G2D_sel1", "0G2D_sel2"})
    m_locTable->InstallCommand(n, loc, this);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0G2D_icon"), 0x9f93ff74u, const_cast<char*>(m_name), 0);
  static const unsigned short kDefault[] = {'D', 'e', 'f', 'a', 'u', 'l', 't', ' ', 'T', 'e', 'x', 't', 0};
  m_body.assign(kDefault, kDefault + 13);
  m_title = m_sel[0] = m_sel[1] = m_sel[2] = m_body;
  m_bInitComplete = 1;
}

// 801B4BE8
G2DTarget::~G2DTarget() {
  if (!m_installed) return;
  m_getTable->UnInstallAllCommands(this);
  m_setTable->UnInstallAllCommands(this);
  m_locTable->UnInstallAllCommands(this);
  m_updateTable->UnInstallEntry(this);
  UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
}

// 801B4D9C
// "G2D_exists": the movie is up; the dialog takes the pads, lays out and fades in.
char* G2DTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  char* out = static_cast<char*>(SimsAptMalloc(0x80));
  out[0] = 0;
  const std::string n = name;
  if (n == "G2D_exists") {
    std::strcpy(out, "loaded");
    m_active = 1;
    PushAptButtonFilters();
    m_savedAnalog = UIDB::UIDBGetInt(const_cast<char*>("DisableAnalogNavigation"));
    UIDB::UIDBSetInt(const_cast<char*>("DisableAnalogNavigation"), 0);
    SetupWidgets();
    m_reflow.OpenReflowBlock();
    m_reflow.Reflow_Id("G2D", 0);
    m_reflow.Reflow_Visibility(true);
    m_reflow.Reflow_Alpha(0);
    m_reflow.SubmitReflowBlock();
    m_reflow.OpenTweenBlock("G2D", 0xaf, 0, "G2D_show_callback", 0);
    m_reflow.Tween_Alpha(0xff);
    m_reflow.SubmitTweenBlock();
    m_visible = 1;
    m_state = 2;
  } else if (n == "G2D_swf_boxWidth") {
    std::snprintf(out, 0x80, "%i", 0x160);
  } else if (n == "G2D_swf_boxHeight") {
    std::snprintf(out, 0x80, "%i", m_boxHeight + 0x20);
  } else if (n == "G2D_swf_boxHeight2") {
    if (m_numSelections == 0) std::strcpy(out, "0");
    else std::snprintf(out, 0x80, "%i", m_boxHeight2 + 0x20);
  }
  return out;
}

// 801B4FAC
unsigned short* G2DTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  std::vector<unsigned short> text(0x1000, 0);
  const std::vector<unsigned short>* src = nullptr;
  if (std::strncmp("G2D_sel", name, 7) == 0) {
    const int i = std::atoi(name + 7);
    src = i == 1 ? &m_sel[1] : i == 2 ? &m_sel[2] : &m_sel[0];
  } else if (std::strcmp("G2D_body_text", name) == 0) {
    src = &m_body;
  } else if (std::strcmp("G2D_title_text", name) == 0) {
    src = &m_title;
  }
  if (src)
    for (size_t i = 0; i < TextLength(*src) && i < text.size() - 1; ++i) text[i] = (*src)[i];
  UIQDTarget::UIQDSubstitutionStrings(text.data());
  return MallocText(text);
}

// 801B511C
// While shown: up/down move the selection, X takes it, select only when allowed, cancel picks 3.
void G2DTarget::SetVariable(char* name, char* value) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  const std::string n = name;
  if (n == "G2D_X_BUTTON") {
    if (!CheckPlayerInput(value) || m_state != 3) return;
    PlayUISound(0xcc59742a);
  } else if (n == "G2D_SELECT_BUTTON") {
    if (!CheckPlayerInput(value) || m_state != 3 || !m_selectAllowed) return;
    PlayUISound(0xcc59742a);
  } else if (n == "G2D_CANCEL_BUTTON") {
    if (!CheckPlayerInput(value) || m_state != 3 || !m_cancelAllowed) return;
    m_selection = 3;
  } else if (n == "G2D_DPAD_UP" || n == "G2D_DPAD_DOWN") {
    if (!CheckPlayerInput(value) || m_state != 3) return;
    const int old = m_selection;
    m_timer = m_timeout;
    if (n == "G2D_DPAD_UP") m_selection = old - 1 == -1 ? m_numSelections - 1 : (old - 1) % m_numSelections;
    else m_selection = (old + 1) % m_numSelections;
    if (old == m_selection) return;
    ReflowButtonSelection();
    PlayUISound(0xcb2fa7cd);
    return;
  } else if (n == "G2D_show_callback") {
    m_state = 3;
    if (m_numSelections == 0) ExecuteZeroInputCallback();
    return;
  } else if (n == "G2D_hide_callback") {
    m_visible = 0;
    UnloadUI();
    return;
  } else {
    return;
  }
  HideDialogRunCallbacks();
}

// 801B5410
// Buttons, select highlights and texts follow the selection (only changes are sent).
void G2DTarget::ReflowButtonSelection() {
  m_reflow.OpenReflowBlock();
  unsigned int on[3];
  for (int i = 0; i < 3; ++i) on[i] = m_selection == i ? 1u : 0u;
  static const char* const kButtons[3] = {"G2D_btn0", "G2D_btn1", "G2D_btn2"};
  static const char* const kSelected[3] = {"G2D_btn0_select", "G2D_btn1_select", "G2D_btn2_select"};
  static const char* const kTexts[3] = {"G2D_text0", "G2D_text1", "G2D_text2"};
  for (int i = 0; i < 3; ++i) {
    unsigned int& last = m_selectAllowed ? m_shown[6 + i] : m_shown[i];
    if (last != on[i]) {
      last = on[i];
      m_reflow.Reflow_Id(m_selectAllowed ? kSelected[i] : kButtons[i], 0);
      m_reflow.Reflow_Visibility(on[i] != 0);
    }
  }
  for (int i = 0; i < 3; ++i)
    if (m_shown[3 + i] != on[i]) {
      m_shown[3 + i] = on[i];
      m_reflow.Reflow_Id(kTexts[i], 0);
      m_reflow.Reflow_ColorOverride(m_selection == i ? 0xffffffffu : 0x050671ffu);
    }
  m_reflow.SubmitReflowBlock();
}

// 801B5724
void G2DTarget::Update() {
  if (m_state == 6) {
    UIScreenManager* usm = TheScreenManager();
    if (!usm || !usm->IsUIScreenUnloaded(0x2f)) return;
    UnloadDialog();
    if (m_numSelections > 0) {
      ExecuteSelectionCallback(m_selection);
      if (static_cast<unsigned int>(m_savedPlayerNumber) < 2) {
        UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), m_savedPlayerNumber);
        m_savedPlayerNumber = -1;
      }
    }
    if (!m_pending) {
      ExecuteDialogClosedCallback();
    } else {
      UIDialog* next = m_pending;
      m_pending = nullptr;
      SpawnDialog(next);
      UIReflow r;
      r.ResetSwfAnimation("G2D_swf", "a0", 0);
    }
  } else if (m_state == 4) {
    if (m_numSelections == 0 && m_frames < 0x2d) {
      ++m_frames;
    } else {
      m_state = 5;
      m_visible = 1;
      m_reflow.OpenTweenBlock("G2D", 0x7d, 0, "G2D_hide_callback", 0);
      m_reflow.Tween_Alpha(0);
      m_reflow.SubmitTweenBlock();
    }
  } else if (m_state == 3) {
    if (!m_pending) {
      if (!m_field94 && ++m_frames > 0x2d) m_field94 = 1;
      if (m_numSelections > 0 && m_fieldB8 && !m_fieldBC && m_timer > 0.0f) {
        m_timer -= _dt;
        if (m_timer <= 0.0f) {
          m_selection = 3;
          m_fieldBC = 1;
          HideDialogRunCallbacks();
        }
      }
    } else {
      SetupDialogData(m_pending);
      CalculateBackgroundSize();
      SetupWidgets();
      m_pending = nullptr;
    }
  }
}

// 801B5960
// Closes without the closed callback (unless it is enabled).
void G2DTarget::HideDialog() {
  if (m_state != 3) return;
  if (!m_closedEnabled) m_onClosed = nullptr;
  m_state = 4;
  m_onZeroInput = nullptr;
  m_onSelection = nullptr;
}
// 801B59C0
void G2DTarget::HideDialogRunCallbacks() {
  if (m_state == 3) m_state = 4;
}
// 801B5A04
int G2DTarget::IsG2DActive() { return m_active; }
// 801B5A0C
int G2DTarget::IsG2DSpawned() { return m_spawned; }
// 801B5A14
void G2DTarget::PushAptButtonFilters() {
  // PC co-op: a private screen's dialog takes only its player's pad.
  if (m_pcPrivate) {
    PushAptButtonFiltersForPlayer(m_player & 1);
    return;
  }
  PushAptButtonFiltersForPlayer(0);
  PushAptButtonFiltersForPlayer(1);
}
// 801B5A50
// UIObjectBase::pushAptButtonFilterALL (80177B44): a filter passing every command.
void G2DTarget::PushAptButtonFiltersForPlayer(int player) {
  if (player == 1 && !_gd.m_fieldEC) return;
  auto* v = static_cast<AptViewer*>(_globals.m_aptViewer);
  unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(m_name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  m_filters[player] = id;
}
// 801B5AAC
void G2DTarget::PopAptButtonFilters() {
  auto* v = static_cast<AptViewer*>(_globals.m_aptViewer);
  for (int p = 0; p < 2; ++p)
    if (m_filters[p] != 0xffffffffu) {
      if (v) v->PopAptButtonFilter(p, m_filters[p]);
      m_filters[p] = 0xffffffffu;
    }
}

// 801B5B14
// Loads the dialog screen with the dialog's data, or (a dialog already up) queues it if different.
void G2DTarget::SpawnDialog(UIDialog* dialog) {
  if (m_state != 0) {
    m_pending = dialog;
    return;
  }
  for (int i = 0; i < 9; ++i) m_shown[i] = 2;
  m_pending = nullptr;
  m_onZeroInput = nullptr;
  m_selection = dialog->m_preselect;
  m_onSelection = nullptr;
  // PC co-op: a dialog for the player whose private screen (food creation, its recipe book) is up is
  // theirs too (uiscreens.cpp, PcCoopPrivateScreens): drawn in their window / half, only their pad,
  // and the game is not paused for the other player.
  m_pcPrivate = PcPrivateScreenPlayer() >= 0 && dialog->m_players == PcPrivateScreenPlayer() + 1;
  if (m_pcPrivate) simmy::coop_set_private_screen("0G2D", PcPrivateScreenPlayer());
  // The game pauses under the dialog (the interactors see the pause count: DirectInteractor /
  // SimInteractor); GC: also each player's interactor drops its input (vtable 0x7C; PC: nothing held).
  if (!m_pcPrivate) H2DTarget::IncPause(false);
  if (std::getenv("SIMMY_UI_TRACE")) {  // PC: the dialog's text on stderr (pad script debugging)
    std::string t;
    auto add = [&t](const std::vector<unsigned short>& v) {
      for (unsigned short c : v) t += c < 0x80 ? static_cast<char>(c) : '?';
    };
    add(dialog->m_body);
    for (int i = 0; i < 3; ++i)
      if (!dialog->m_sel[i].empty()) t += " | ", add(dialog->m_sel[i]);
    std::fprintf(stderr, "[dialog] %s\n", t.c_str());
  }
  SetupDialogData(dialog);
  m_timeout = (dialog->m_timeoutMs == -1 ? kDialogTimeout : static_cast<float>(dialog->m_timeoutMs)) / 1000.0f;
  m_timer = m_timeout;
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  SetIcon(dialog->m_icon, dialog->m_iconW, dialog->m_iconH);
  CalculateBackgroundSize();
  if (UIScreenManager* usm = TheScreenManager()) usm->LoadUIScreen(0x2f, 2);
  PlayUISound(0xae80f928);
  m_state = 1;
  m_spawned = 1;
}

// 801B5E38
// The box: 320x288 around (320, 224); the body's height from its laid-out lines (GC: AptParagraph
// at 12 points), the title and the buttons add 30 each.
void G2DTarget::CalculateBackgroundSize() {
  m_box[0] = 320.0f;  // centre x
  m_box[1] = 224.0f;  // centre y
  m_box[2] = 288.0f;  // text width
  m_box[3] = 200.0f;  // text height
  m_box[6] = m_box[0] - m_box[2] * 0.5f;
  m_box[7] = m_box[1] - m_box[3] * 0.5f;
  m_box[4] = m_box[5] = 0.0f;
  if (m_icon) {
    m_box[5] = static_cast<float>(m_iconW);
    m_box[4] = static_cast<float>(m_iconH);
    m_box[2] -= m_box[4] + 8.0f;
  }
  // AptParagraph::Build/GetParagraphHeight: the body at 12 points in the text width.
  std::vector<unsigned short> body = m_body;
  body.push_back(0);
  float height = AptAuxParagraphHeight(body.data(), m_box[2], 12.0f);
  m_box[3] = height;
  if (height < m_box[5]) height = m_box[3] = m_box[5];
  if (TextLength(m_title)) {
    m_box[7] += 30.0f;
    height += 30.0f;
  }
  if (m_numSelections > 0) height += static_cast<float>(m_numSelections) * 30.0f + 22.0f;
  m_boxHeight = static_cast<int>(height + 24.0f);
  m_boxHeight2 = static_cast<int>(height - static_cast<float>(m_numSelections) * 30.0f);
}

// 801B6234
// Positions the box, title, icon, body and buttons and colours the selected text.
void G2DTarget::SetupWidgets() {
  UIReflow& r = m_reflow;
  r.OpenReflowBlock();
  r.Reflow_Id("G2D_swf_container", 0);
  r.Reflow_X(0x90);
  r.Reflow_Y(0xe0 - static_cast<int>((static_cast<unsigned int>(m_boxHeight) + 0x20) >> 1));
  for (int i = 0; i < 3; ++i) {
    const char* ids[3] = {"G2D_option0", "G2D_option1", "G2D_option2"};
    r.Reflow_Id(ids[i], 0);
    r.Reflow_Visibility(m_numSelections > i);
  }
  const float top = static_cast<float>(0xe0 - (static_cast<unsigned int>(m_boxHeight) >> 1)) + 12.0f;
  r.Reflow_Id("G2D_title", 0);
  r.Reflow_X(static_cast<int>(m_box[6]));
  r.Reflow_Y(static_cast<int>(top));
  r.Reflow_W(0x120);
  m_box[7] = TextLength(m_title) ? top + 30.0f : top;
  r.Reflow_Id("G2D_icon", 0);
  r.Reflow_X(static_cast<int>(m_box[4] * 0.5f + m_box[6]));
  r.Reflow_Y(static_cast<int>(m_box[5] * 0.5f + m_box[7]));
  r.Reflow_W(static_cast<unsigned int>(m_box[4]));
  r.Reflow_H(static_cast<unsigned int>(m_box[5]));
  r.Reflow_Id("G2D_body", 0);
  r.Reflow_X(static_cast<int>(m_box[4] <= 0.0f ? m_box[6] : m_box[6] + m_box[4] + 8.0f));
  r.Reflow_Y(static_cast<int>(m_box[7]));
  r.Reflow_W(static_cast<unsigned int>(m_box[2]));
  if (m_numSelections > 0) {
    const float y = (m_box[5] <= m_box[3] ? m_box[7] + m_box[3] : m_box[7] + m_box[5]) + 33.0f;
    r.Reflow_Id("G2D_options", 0);
    r.Reflow_X(static_cast<int>(m_box[0] - 160.0f - 32.0f + 28.0f));
    r.Reflow_Y(static_cast<int>(y));
    const char* texts[3] = {"G2D_text0", "G2D_text1", "G2D_text2"};
    for (int i = 0; i < m_numSelections && i < 3; ++i) {
      r.Reflow_Id(texts[i], 0);
      r.Reflow_ColorOverride(m_selection == i ? 0xffffffffu : 0x050671ffu);
    }
  }
  r.SubmitReflowBlock();
  ReflowButtonSelection();
}

// 801B690C
void G2DTarget::UnloadDialog() {
  if (m_state != 6) return;
  m_state = 0;
  if (!m_closedEnabled) m_onClosed = nullptr;
  m_active = 0;
  m_spawned = 0;
  UIDB::UIDBSetInt(const_cast<char*>("DisableAnalogNavigation"), m_savedAnalog);
  PopAptButtonFilters();
  m_body.clear();
  m_title.clear();
  for (auto& s : m_sel) s.clear();
  UI2D::Instance()->InstallEntry(const_cast<char*>("G2D_icon"), 0x9f93ff74u, const_cast<char*>(m_name), 0);
  if (m_icon) {
    m_icon->DelRef();
    m_icon = nullptr;
  }
  m_updateTable->UnInstallEntry(this);
  if (m_pcPrivate) {
    m_pcPrivate = false;
    simmy::coop_set_private_screen("0G2D", -1);
  } else {
    H2DTarget::ReleasePause(false);
  }
}

// PC co-op: the dialog box shows a private screen's dialog (uiscreens.cpp).
bool PcPrivateDialogShown() {
  G2DTarget* g = TheG2D();
  return g && g->m_pcPrivate;
}
// ... or a dialog both players see (the GC's: it pauses the game and takes both pads).
bool PcSharedDialogShown() {
  G2DTarget* g = TheG2D();
  return g && g->m_spawned && !g->m_pcPrivate;
}

// 801B6A60
void G2DTarget::SetIcon(ERShader* icon, short w, short h) {
  if (icon && icon->GetShader()) {
    m_iconW = w;
    m_iconH = h;
    UI2D::Instance()->InstallEntry(const_cast<char*>("G2D_icon"), icon, const_cast<char*>(m_name), 0);
    return;
  }
  UI2D::Instance()->InstallEntry(const_cast<char*>("G2D_icon"), 0x9f93ff74u, const_cast<char*>(m_name), 0);
}

// 801B6B24
void G2DTarget::ExecuteSelectionCallback(int selection) {
  if (auto* cb = m_onSelection) {
    m_onSelection = nullptr;
    if (*cb) (*cb)(selection);
  }
}
// 801B6B68
void G2DTarget::ExecuteZeroInputCallback() {
  if (auto* cb = m_onZeroInput) {
    m_onZeroInput = nullptr;
    if (*cb) (*cb)();
  }
}
// 801B6BAC
void G2DTarget::ExecuteDialogClosedCallback() {
  if (!m_onClosed) return;
  if (!m_closedEnabled) {
    m_onClosed = nullptr;
    return;
  }
  auto* cb = m_onClosed;
  m_onClosed = nullptr;
  if (*cb) (*cb)();
}

// 801B6C44
void G2DTarget::ChangeContinueType(int type) {
  if (m_numSelections == 0 && type == 0) HideDialogRunCallbacks();
  m_continueType = type;
}

// 801B6CC0
// Copies the dialog's texts, buttons, player and callbacks.
void G2DTarget::SetupDialogData(UIDialog* d) {
  m_mode = d->m_mode;
  m_players = d->m_players;
  if (m_players == 3) {
    m_player = 2;
  } else {
    m_savedPlayerNumber = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
    m_player = m_players == 1 ? 0 : 1;
    UIDB::UIDBSetInt(const_cast<char*>("PlayerNumber"), m_player);
  }
  m_numSelections = d->m_numSelections;
  m_continueType = d->m_continueType;
  if (m_numSelections == 0 && m_continueType != 1) m_continueType = 1;
  m_preselect = d->m_preselect;
  m_fieldB4 = d->m_field10 != 0;
  m_fieldB8 = d->m_field14 != 0;  // the timeout closes the dialog
  m_fieldBC = 0;
  m_cancelAllowed = d->m_field1C != 0;
  m_closedEnabled = d->m_field20 != 0;
  m_selectAllowed = d->m_field24 != 0;
  m_body = d->m_body;
  m_title = d->m_title;
  for (int i = 0; i < 3; ++i) m_sel[i] = d->m_sel[i];
  m_icon = d->m_icon;
  m_iconW = m_iconH = 0;
  if (m_icon) {
    m_iconW = d->m_iconW;
    m_iconH = d->m_iconH;
    m_icon->AddRef();
    UI2D::Instance()->InstallEntry(const_cast<char*>("0G2D_icon"), m_icon, const_cast<char*>(m_name), 0);
  }
  m_onClosed = &d->m_onClosed;
  m_onSelection = &d->m_onSelection;
  m_onZeroInput = &d->m_onZeroInput;
}

// 801B7000
void G2DTarget::UnloadUI() {
  if (m_state != 5) return;
  m_state = 6;
  if (UIScreenManager* usm = TheScreenManager()) usm->UnloadUIScreen(0x2f);
}

// 801B7040
// Only the dialog's player (or either, 2) may answer; the value is the player digit.
bool G2DTarget::CheckPlayerInput(const char* value) {
  if (m_numSelections == 0 || !value || static_cast<unsigned>(value[0] - '0') > 1) return false;
  return value[0] - '0' == m_player || m_player == 2;
}

// 801B708C
int G2DTarget::ServiceG2DShutdown() {
  int busy = 0;
  if (m_state >= 1 && m_state <= 6) {
    if (m_state == 3) HideDialogRunCallbacks();
    busy = 1;
  }
  if (m_pending) busy = 1;
  return busy;
}

// 801E7050
int G2DTarget::IsInitComplete() { return m_bInitComplete; }
// 801E7058
bool G2DTarget::HasPendingTransition() { return m_state != 0 && m_state != 3; }
