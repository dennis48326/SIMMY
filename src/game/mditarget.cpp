// Port of MDITarget and ITBTarget (game/targets_s2c.cpp) and of H2DTarget's modeless-dialog and intro
// thought balloon calls; see mditarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

void* SimsAptMalloc(unsigned int size);
float AptAuxParagraphHeight(const unsigned short* text, float width, float fontSize);  // aptauxpceorgl.cpp

namespace {
G2DTarget* TheG2D() { return static_cast<G2DTarget*>(_globals.m_g2dTarget); }
UIScreenManager* Screens() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
size_t Length(const std::vector<unsigned short>& t) {
  size_t n = 0;
  while (n < t.size() && t[n]) ++n;
  return n;
}
// The text with the quickdata substitutions made, in an Apt-owned buffer (the GetLocalizable result).
unsigned short* AptText(const std::vector<unsigned short>* src) {
  std::vector<unsigned short> buf(0x1000, 0);
  if (src)
    for (size_t i = 0; i < Length(*src) && i < buf.size() - 1; ++i) buf[i] = (*src)[i];
  UIQDTarget::UIQDSubstitutionStrings(buf.data());
  size_t n = 0;
  while (buf[n]) ++n;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc(static_cast<unsigned int>((n + 1) * 2)));
  for (size_t i = 0; i < n; ++i) out[i] = buf[i];
  out[n] = 0;
  return out;
}
const char* SkipDigit(const char* name) { return std::isdigit(static_cast<unsigned char>(*name)) ? name + 1 : name; }
ITBTarget* s_itb = nullptr;  // H2DTarget +0xC4
int s_fastHints = 0;         // DAT_80476174 (a debug switch: every hint 0.5 s)
const unsigned int kNoIcon = 0x9f93ff74u;
}  // namespace

// ---------------------------------------------------------------------------------------------------
// MDITarget

// The shader references a queued dialog holds (GC: in its removal).
static void ReleaseIcons(MDITarget__ModelessDialog* d) {
  if (d->m_icon) d->m_icon->DelRef();
  if (d->m_iconBg) d->m_iconBg->DelRef();
  d->m_icon = d->m_iconBg = nullptr;
}

// 801C2FCC
MDITarget::MDITarget() {
  AttachTables("0MDITarget");
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  for (const char* n : {"0MDI_exists", "0MDI_swf_boxWidth", "0MDI_swf_boxHeight", "0MDI_swf_boxHeight2"})
    m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"0MDI_show_callback", "0MDI_hide_callback"}) m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"0MDI_title", "0MDI_body"}) m_locTable->InstallCommand(n, loc, this);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0MDI_icon_shdr"), kNoIcon, const_cast<char*>(m_name), 0);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0MDI_icon_bg_shdr"), kNoIcon, const_cast<char*>(m_name), 0);
  m_boxHeight = 0x40;
  if (UIScreenManager* usm = Screens()) usm->LoadUIScreen(0x42, 0);
}

// 801C35DC
MDITarget::~MDITarget() {
  DeleteAllModelessDialogs();
  if (UIScreenManager* usm = Screens()) usm->UnloadUIScreen(0x42);
  if (!m_installed) return;
  m_getTable->UnInstallAllCommands(this);
  m_setTable->UnInstallAllCommands(this);
  m_locTable->UnInstallAllCommands(this);
  m_updateTable->UnInstallEntry(this);
  UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
}

// 801C3734
// The show and hide tweens' ends (hidden: the box made invisible).
void MDITarget::SetVariable(char* name, char* /*value*/) {
  const char* n = SkipDigit(name);
  if (!std::strcmp("MDI_show_callback", n)) {
    m_tweening = 0;
  } else if (!std::strcmp("MDI_hide_callback", n)) {
    m_tweening = 0;
    UIReflow r;
    r.ResetVisibility("MDI", false, 0);
    m_visible = 0;
  }
}

// 801C37E4
char* MDITarget::GetVariable(char* name) {
  const char* n = SkipDigit(name);
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  if (!std::strcmp("MDI_exists", n)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  } else if (!std::strcmp("MDI_swf_boxWidth", n)) {
    std::snprintf(out, 0x20, "%i", 0x1d4);
  } else if (!std::strcmp("MDI_swf_boxHeight", n)) {
    std::snprintf(out, 0x20, "%i", m_boxHeight);
  } else if (!std::strcmp("MDI_swf_boxHeight2", n)) {
    std::strcpy(out, "0");
  }
  return out;
}

// 801C3918
unsigned short* MDITarget::GetLocalizable(char* name) {
  const char* n = SkipDigit(name);
  const std::vector<unsigned short>* src = nullptr;
  if (!std::strcmp("MDI_body", n) && m_head) src = &m_head->m_body;
  else if (!std::strcmp("MDI_title", n) && m_head) src = &m_head->m_title;
  return AptText(src);
}

// 801C3A24
// Fades the box in when a dialog is to be shown and out when none is; the shown dialog's time runs
// down while no modal dialog is up, then the next one is set up (none: hidden).
void MDITarget::Update() {
  if (m_exists && !m_tweening) {
    if (!m_show) {
      if (m_visible) {
        UIReflow r;
        r.OpenTweenBlock("MDI", 0xfa, 0, "MDI_hide_callback", 0);
        r.Tween_Alpha(0);
        r.SubmitTweenBlock();
        m_tweening = 1;
      }
    } else if (!m_visible) {
      UIReflow r;
      r.ResetVisibility("MDI", true, 0);
      r.OpenTweenBlock("MDI", 0xfa, 0, "MDI_show_callback", 0);
      r.Tween_Alpha(0xff);
      r.SubmitTweenBlock();
      m_visible = 1;
      m_tweening = 1;
    }
  }
  if (!m_head) {
    HideModelessDialog();
    return;
  }
  G2DTarget* g2d = TheG2D();
  if (g2d && g2d->IsG2DSpawned()) return;
  if (!m_show) {
    SetupModelessDialog();
    ShowModelessDialog();
    return;
  }
  m_head->m_timeMs -= static_cast<int>(_dt * 1000.0f);
  if (m_head->m_timeMs >= 0) return;
  ModelessDialog* done = m_head;
  m_head = done->m_next;
  ReleaseIcons(done);
  delete done;
  if (!m_head) HideModelessDialog();
  else SetupModelessDialog();
}

// 801C3C84
void MDITarget::ShowModelessDialog() { m_show = 1; }
// 801C3C90
void MDITarget::HideModelessDialog() { m_show = 0; }
// 801C3C9C
void MDITarget::UpdateModelessDialogs() {}
// 801C3CA0
int MDITarget::IsModelessDialogActive() { return m_visible; }

// 801C3CA8
// A copy of the dialog's body, title and icon queued; shown for its text's time (twice it when the
// dialog gives a time of its own).
void MDITarget::SpawnModelessDialog(UIDialog* dialog) {
  auto* d = new ModelessDialog;
  ++m_count;
  d->m_body = dialog->m_body;
  d->m_title = dialog->m_title;
  d->m_icon = dialog->m_icon;  // GC: else the dialog's shader id (+0x40), AddRefAsync (PC: none given)
  if (d->m_icon) d->m_icon->AddRef();  // released with the dialog (ReleaseIcons)
  d->m_iconW = dialog->m_iconW;
  d->m_iconH = dialog->m_iconH;
  d->m_timeMs = dialog->m_timeoutMs == -1 ? 1 : 2;
  AddToList(d);
}

// 801C3DA0
void MDITarget::AddToList(ModelessDialog* dialog) {
  m_count = 1;
  if (!m_head) {
    m_head = dialog;
    return;
  }
  ModelessDialog* d = m_head;
  for (m_count = 2; d->m_next; d = d->m_next) ++m_count;
  d->m_next = dialog;
  dialog->m_next = nullptr;
}

// 801C3DF0
int MDITarget::CountModelessDialogs() { return m_count; }

// 801C3DF8
void MDITarget::DeleteAllModelessDialogs() {
  while (m_head) {
    ModelessDialog* next = m_head->m_next;
    ReleaseIcons(m_head);
    delete m_head;
    m_head = next;
  }
  m_count = 0;
}

// 801C3EC4
void MDITarget::CleanupModelessDialogs() { DeleteAllModelessDialogs(); }

// 801C3EE4
// Lays out the shown dialog: the icon (8..256 px, else 32) on the left, the title (12 pt, 22 px high)
// and the body (12 pt) in the 428 px wide box at x 0x61; the box's height from the text's, and the
// dialog's time from its text's area (at least 4000; times its kind, at most 16000 ms).
void MDITarget::SetupModelessDialog() {
  ModelessDialog* d = m_head;
  if (d->m_icon) UI2D::Instance()->InstallEntry(const_cast<char*>("0MDI_icon_shdr"), d->m_icon, const_cast<char*>(m_name), 0);
  else UI2D::Instance()->InstallEntry(const_cast<char*>("0MDI_icon_shdr"), kNoIcon, const_cast<char*>(m_name), 0);
  if (d->m_iconBg) UI2D::Instance()->InstallEntry(const_cast<char*>("0MDI_icon_bg_shdr"), d->m_iconBg, const_cast<char*>(m_name), 0);
  else UI2D::Instance()->InstallEntry(const_cast<char*>("0MDI_icon_bg_shdr"), kNoIcon, const_cast<char*>(m_name), 0);
  UIReflow r;
  r.OpenReflowBlock();
  r.Reflow_Id("MDI", 0);
  r.Reflow_X(0x61);
  r.Reflow_Id("MDI_title", 0);
  r.Reflow_FontSizeOverride(0xc);
  r.Reflow_Id("MDI_body", 0);
  r.Reflow_FontSizeOverride(0xc);
  float iconW = 32.0f, iconH = 32.0f;
  const float w = static_cast<float>(d->m_iconH), h = static_cast<float>(d->m_iconW);
  if (w >= 8.0f && w <= 256.0f && h >= 8.0f && h <= 256.0f) iconW = w, iconH = h;
  float iconX = iconW * 0.5f + 24.0f;
  float bodyW = 428.0f, bodyX = 24.0f, bodyY = 24.0f;
  float titleX = 24.0f, titleY = 24.0f, titleW = 0.0f, titleH = 0.0f;
  if (Length(d->m_title)) {
    bodyY += 22.0f;
    titleW = bodyW;
    titleH = 22.0f;
  }
  if (d->m_icon) {
    const float shift = iconW + 16.0f;
    titleX += shift;
    titleW -= shift;
    bodyX += shift;
    bodyW -= shift;
  }
  // AptParagraph::Build at 12 points in the body's width: its height and print width.
  std::vector<unsigned short> body(d->m_body.begin(), d->m_body.begin() + static_cast<long>(Length(d->m_body)));
  body.push_back(0);
  float height = 0.0f, printWidth = 0.0f;
  if (AptAuxParagraphHeight(body.data(), bodyW, 12.0f) > 0.0f) {  // (0: no game font)
    AptParagraph p;
    EVec2 topLeft{0.0f, 0.0f}, bottomRight{bodyW, 94.0f};
    EVec4 white{1.0f, 1.0f, 1.0f, 1.0f};
    p.Build(topLeft, bottomRight, 12.0f, white, 0, body.data());
    height = p.GetParagraphHeight();
    printWidth = p.GetPrintWidth();
  }
  float area = printWidth * height;
  if (area < 4000.0f) area = 4000.0f;
  if (d->m_timeMs > 2) d->m_timeMs = 2;
  d->m_timeMs *= static_cast<int>(area);
  if (s_fastHints) d->m_timeMs = 500;
  if (d->m_timeMs > 16000) d->m_timeMs = 16000;
  float content = height + titleH;
  if (content < iconH) content = iconH;
  const float boxF = content + 24.0f + 16.0f - 64.0f;
  const unsigned int box = boxF > 0.0f ? static_cast<unsigned int>(boxF) : 0u;
  const float iconY = iconH * 0.5f + 22.0f;
  for (const char* id : {"MDI_icon_shdr", "MDI_icon_bg_shdr"}) {
    r.Reflow_Id(id, 0);
    r.Reflow_X(static_cast<int>(iconX));
    r.Reflow_Y(static_cast<int>(iconY));
    r.Reflow_W(static_cast<unsigned int>(iconW));
    r.Reflow_H(static_cast<unsigned int>(iconH));
  }
  r.Reflow_Id("MDI_title", 0);
  r.Reflow_X(static_cast<int>(titleX));
  r.Reflow_Y(static_cast<int>(titleY));
  r.Reflow_W(static_cast<unsigned int>(titleW));
  r.Reflow_Id("MDI_body", 0);
  r.Reflow_X(static_cast<int>(bodyX));
  r.Reflow_Y(static_cast<int>(bodyY));
  r.Reflow_W(static_cast<unsigned int>(bodyW));
  m_boxHeight = static_cast<int>(static_cast<float>(box + (box & 1) + 0x40) + 2.0f);
  r.Reflow_Id("MDI_swf", 0);
  r.Reflow_SwfAnimation(const_cast<char*>("a0"));
  r.SubmitReflowBlock();
  std::string text;
  for (unsigned short c : body) text += c && c < 0x80 ? static_cast<char>(c) : (c ? '?' : '\0');
  std::printf("[dialog] modeless hint shown for %d ms: \"%s\"\n", d->m_timeMs, text.c_str());
}

// ---------------------------------------------------------------------------------------------------
// ITBTarget

// 801BCA3C
ITBTarget::ITBTarget() {
  AttachTables("0ITBTarget");
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  m_getTable->InstallCommand("0ITB_exists", get, this);
  for (const char* n : {"0ITB_hide_callback", "0ITB_show_bubble_callback", "0ITB_show_big_bubble_callback",
                        "0ITB_show_text_callback", "0ITB_finish_text_callback"})
    m_setTable->InstallCommand(n, set, this);
  m_locTable->InstallCommand("0ITB_text", loc, this);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0ITB_text_shdr"), 0x63985181u, const_cast<char*>(m_name), 0);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0ITB_bubble_shdr"), 0x16e15214u, const_cast<char*>(m_name), 0);
  UI2D::Instance()->InstallEntry(const_cast<char*>("0ITB_bubble_shdr2"), 0x16e15214u, const_cast<char*>(m_name), 0);
  static const unsigned short kText[] = {'t', 'e', 'x', 't', 0};
  m_text.assign(kText, kText + 5);
  m_stage = 0;
  if (UIScreenManager* usm = Screens()) usm->LoadUIScreen(0x41, 0);
}

// 801BD088
ITBTarget::~ITBTarget() {
  if (UIScreenManager* usm = Screens()) usm->UnloadUIScreen(0x41);
  if (!m_installed) return;
  m_getTable->UnInstallAllCommands(this);
  m_setTable->UnInstallAllCommands(this);
  m_locTable->UnInstallAllCommands(this);
  m_updateTable->UnInstallEntry(this);
  UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
}

// 801BD1F8
// The grow-in tweens' ends move the balloon on a step; hidden: inactive.
void ITBTarget::SetVariable(char* name, char* /*value*/) {
  const char* n = SkipDigit(name);
  if (!std::strcmp("ITB_finish_text_callback", n)) {
    m_tweening = 0;
  } else if (!std::strcmp("ITB_show_text_callback", n)) {
    m_stage = 4;
    UIReflow r;
    r.OpenTweenBlock("ITB_text", 0x7d, 0, "NO_CALLBACK", 0);
    r.Tween_Alpha(0xff);
    r.SubmitTweenBlock();
  } else if (!std::strcmp("ITB_show_bubble_callback", n)) {
    m_stage = 2;
  } else if (!std::strcmp("ITB_show_big_bubble_callback", n)) {
    m_stage = 3;
  } else if (!std::strcmp("ITB_hide_callback", n)) {
    m_active = 0;
    m_tweening = 0;
  }
}

// 801BD32C
char* ITBTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  if (!std::strcmp("ITB_exists", SkipDigit(name))) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801BD3C8
unsigned short* ITBTarget::GetLocalizable(char* name) {
  return AptText(!std::strcmp("ITB_text", SkipDigit(name)) ? &m_text : nullptr);
}

// 801BD490
// The balloon grows in: a small bubble, a bigger one, the text box, then the text; it fades out when
// hidden.
void ITBTarget::Update() {
  if (!m_exists) return;
  UIReflow r;
  switch (m_stage) {
    case 1:
      r.OpenReflowBlock();
      r.Reflow_Id("ITB", 0);
      r.Reflow_Visibility(true);
      r.Reflow_Alpha(0xff);
      r.Reflow_Id("ITB_text_shdr", 0);
      r.Reflow_Alpha(0);
      r.Reflow_Id("ITB_text", 0);
      r.Reflow_Alpha(0);
      r.Reflow_W(0x14a);
      r.Reflow_H(0x89);
      for (const char* id : {"ITB_bubble_shdr", "ITB_bubble_shdr2"}) {
        r.Reflow_Id(id, 0);
        r.Reflow_Alpha(0);
        r.Reflow_W(1);
        r.Reflow_H(1);
      }
      r.SubmitReflowBlock();
      r.OpenTweenBlock("ITB_bubble_shdr2", 0x7d, 0, "ITB_show_bubble_callback", 0);
      r.Tween_Alpha(0xff);
      r.Tween_W(0x24);
      r.Tween_H(0x24);
      r.SubmitTweenBlock();
      m_stage = 0;
      break;
    case 2:
      r.OpenTweenBlock("ITB_bubble_shdr2", 0x7d, 0, "NO_CALLBACK", 0);
      r.Tween_W(0x18);
      r.Tween_H(0x18);
      r.SubmitTweenBlock();
      r.OpenTweenBlock("ITB_bubble_shdr", 0x7d, 0, "ITB_show_big_bubble_callback", 0);
      r.Tween_Alpha(0xff);
      r.Tween_W(0x48);
      r.Tween_H(0x48);
      r.SubmitTweenBlock();
      m_stage = 0;
      break;
    case 3:
      r.OpenTweenBlock("ITB_bubble_shdr", 0x7d, 0, "NO_CALLBACK", 0);
      r.Tween_W(0x30);
      r.Tween_H(0x30);
      r.SubmitTweenBlock();
      r.OpenTweenBlock("ITB_text_shdr", 0x7d, 0, "ITB_show_text_callback", 0);
      r.Tween_Alpha(0xff);
      r.Tween_W(0x1ef);
      r.Tween_H(0xcd);
      r.SubmitTweenBlock();
      m_stage = 0;
      break;
    case 4:
      r.OpenTweenBlock("ITB_text_shdr", 0x7d, 0, "ITB_finish_text_callback", 0);
      r.Tween_W(0x17a);
      r.Tween_H(0x89);
      r.SubmitTweenBlock();
      m_stage = 0;
      break;
    default:
      break;
  }
  if (m_tweening) return;
  if (!m_show) {
    if (m_active) {
      UIReflow h;
      h.OpenTweenBlock("ITB", 0xfa, 0, "ITB_hide_callback", 0);
      h.Tween_Alpha(0);
      h.SubmitTweenBlock();
      m_tweening = 1;
    }
  } else if (!m_active) {
    m_active = 1;
    m_stage = 1;
    m_tweening = 1;
  }
}

// 801BD870
void ITBTarget::ShowIntroThoughtBalloon() { m_show = 1; }
// 801BD87C
void ITBTarget::HideIntroThoughtBalloon() { m_show = 0; }

// 801BD888
// Once a frame (H2DTarget::Update): the read time runs down (1/30 s a frame, to -60); a shown balloon
// read to its end is hidden.
void ITBTarget::UpdateIntroThoughtBalloon() {
  if (m_readTime <= -60.0f) return;
  m_readTime -= 0.033333335f;
  if (m_active && !m_tweening && m_readTime < 0.0f) HideIntroThoughtBalloon();
}

// 801BD8F4
int ITBTarget::IsIntroThoughtBalloonActive() { return m_active; }

// 801BD8FC
// The dialog's body as the balloon's text (" " or "_": the balloon goes away), readable for 4 s plus
// 1/40 s a letter.
void ITBTarget::SpawnIntroThoughtBalloon(UIDialog* dialog) {
  m_text = dialog->m_body;
  const size_t n = Length(m_text);
  const bool blank = n == 1 && (m_text[0] == u' ' || m_text[0] == u'_');
  if (blank || n == 0) {
    HideIntroThoughtBalloon();
    m_readTime = 0.0f;
    return;
  }
  UIReflow r;
  r.ResetFontSizeOverride("ITB_text", 0xc, 0);
  m_readTime = static_cast<float>(n) * 0.025f + 4.0f;
  ShowIntroThoughtBalloon();
  std::string text;
  for (size_t i = 0; i < n; ++i) text += m_text[i] < 0x80 ? static_cast<char>(m_text[i]) : '?';
  std::printf("[dialog] intro thought balloon: \"%s\"\n", text.c_str());
}

// 801BDA50
double ITBTarget::GetUserReadTime() { return m_readTime; }

// ---------------------------------------------------------------------------------------------------
// H2DTarget's parts

// 801B8D0C
void H2DTarget::SpawnIntroThoughtBalloon(UIDialog* dialog) {
  if (!s_itb) s_itb = new ITBTarget;
  s_itb->SpawnIntroThoughtBalloon(dialog);
}

// 801B8D60
RET_UNKNOWN H2DTarget::IsUserReadingIntroThoughtBalloon() { return s_itb && s_itb->GetUserReadTime() > 0.0 ? 1 : 0; }

// 801B8DB0
// GC: a member (the H2DTarget passed); PC: the game's H2DTarget.
void H2DTarget::SpawnModelessDialog(UIDialog* dialog) {
  H2DTarget* h2d = _gd.m_h2dTarget;
  if (h2d && h2d->m_mdi) static_cast<MDITarget*>(h2d->m_mdi)->SpawnModelessDialog(dialog);
}

// 801B8DDC
void H2DTarget::CleanupModelessDialogs() {
  if (m_mdi) static_cast<MDITarget*>(m_mdi)->CleanupModelessDialogs();
}

// 801B8E08
RET_UNKNOWN H2DTarget::CountModelessDialogs() { return m_mdi ? static_cast<MDITarget*>(m_mdi)->CountModelessDialogs() : 0; }

// Part of 801B8484 (H2DTarget::Update): the balloon's read time each frame; once read for 5 s past its
// time it is deleted.
void H2DUpdateIntroThoughtBalloon() {
  H2DUpdateRewardMoment();  // the next part of Update: the reward moment dialog (rmdtarget.cpp)
  if (!s_itb) return;
  s_itb->UpdateIntroThoughtBalloon();
  if (s_itb->GetUserReadTime() < -5.0) {
    delete s_itb;
    s_itb = nullptr;
  }
}

// Part of 801B7B18 (H2DTarget::~H2DTarget): the dialog targets go with it.
void H2DDeleteDialogTargets(H2DTarget* h2d) {
  delete static_cast<MDITarget*>(h2d->m_mdi);
  h2d->m_mdi = nullptr;
  delete s_itb;
  s_itb = nullptr;
  H2DDeleteRewardMoment();
}
