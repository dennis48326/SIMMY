// Port of CRDTarget (game/targets_s2c.cpp): the credits screen; see crdtarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
UIScreenManager* ScreenManager() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
constexpr size_t kTextChars = 0x2a02 / 2;  // the text buffer (0x1501 characters)
const unsigned short kKatrina[] = {'K', 'a', 't', 'r', 'i', 'n', 'a', 0};

size_t Len16(const unsigned short* s) {
  size_t n = 0;
  while (s && s[n]) ++n;
  return n;
}
}  // namespace

// 801A4070
// The movie's commands, the screen's pictures and the "0CRD_credits_box" draw callback.
CRDTarget::CRDTarget() {
  AttachTables("0CRDTarget");
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  for (const char* n : {"0CRD_exists", "0CRD_credits_done", "0CRD_flipbook_mode"}) m_getTable->InstallCommand(n, get, this);
  for (const char* n : {"0CRD_show_callback", "0CRD_hide_callback", "0CRD_start_credits", "0CRD_end_credits",
                        "0CRD_X_BUTTON"})
    m_setTable->InstallCommand(n, set, this);
  UI2D* ui2d = UI2D::Instance();
  ui2d->InstallEntry(const_cast<char*>("0CRD_background"), 0x20b8427du, const_cast<char*>(m_name), 0);
  ui2d->InstallEntry(const_cast<char*>("0CRD_team1"), 0x303f738cu, const_cast<char*>(m_name), 0);
  ui2d->InstallEntry(const_cast<char*>("0CRD_team2"), 0xa9362236u, const_cast<char*>(m_name), 0);
  ui2d->InstallEntry(const_cast<char*>("0CRD_team3"), 0xde3112a0u, const_cast<char*>(m_name), 0);
  ui2d->InstallEntry(const_cast<char*>("0CRD_team4"), 0x40558703u, const_cast<char*>(m_name), 0);
  UIUserDrawCBTable::Instance()->InstallEntry(
      "0CRD_credits_box", [this](ERC* rc, structDrawCBparams* p) { ScrollingCallback(rc, p); }, m_name);
  m_window = new EWindow;
}

// 801A4848
CRDTarget::~CRDTarget() {
  if (m_photoOn) {
    HidePhoto();
    m_photoOn = 0;
  }
  delete m_window;
  m_window = nullptr;
  if (m_running) EndCredits();
  if (m_shown)
    if (UIScreenManager* usm = ScreenManager()) usm->UnloadUIScreen(0x2a);
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UIUserDrawCBTable::Instance()->UnInstallAllName(m_name);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801A4A04
// The fade-in tween's callback starts the scrolling, the fade-out's tells the menu; the movie's end
// and X button fade out.
void CRDTarget::SetVariable(char* name, char*) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  if (!std::strcmp("CRD_show_callback", name)) {
    m_tweening = 0;
    StartCredits();
  } else if (!std::strcmp("CRD_hide_callback", name)) {
    m_tweening = 0;
    if (MMUTarget* mmu = MMUTarget::GetSingleton()) mmu->OnCreditsClose();
  } else if (!std::strcmp("CRD_end_credits", name) || !std::strcmp("CRD_X_BUTTON", name)) {
    HideCredits();
  }
}

// 801A4AE4
char* CRDTarget::GetVariable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("CRD_exists", name)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  } else if (!std::strcmp("CRD_credits_done", name)) {
    std::snprintf(out, 0x20, "%d", m_done);
  } else if (!std::strcmp("CRD_flipbook_mode", name)) {
    std::snprintf(out, 0x20, "%d", 0);
  }
  return out;
}

// 801A4BE8
// Nothing is localised here (an empty string, tokens substituted).
unsigned short* CRDTarget::GetLocalizable(char*) {
  auto* s = static_cast<unsigned short*>(SimsAptMalloc(0x2000));
  *s = 0;
  UIQDTarget::UIQDSubstitutionStrings(s);
  const size_t n = Len16(s);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  std::memcpy(out, s, (n + 1) * 2);
  SimsAptFree(s);
  return out;
}

// 801A4C6C
void CRDTarget::Draw(ERC*) {}

// 801A4C70
// The cheat (9) toggles a photo sliding across the screen over a minute.
void CRDTarget::Update() {
  if (!m_exists || !m_visible) return;
  const int cheat = _globals.IsPlayerCheatOn(9);
  if (cheat != m_cheatOn) {
    m_photoOn ^= 1;
    if (m_photoOn) ShowPhoto();
    else HidePhoto();
    m_cheatOn = cheat;
  }
  if (!m_photoOn) return;
  const float t = static_cast<float>(m_photoFrame++) / 3600.0f;
  if (t <= 1.0f) {
    UIReflow r;
    r.ResetPosition("CRD_photo", static_cast<int>((1.0f - t) * 640.0f + t * -1600.0f), 0x18, 0);
  } else {
    m_photoOn = 0;
    HidePhoto();
  }
}

// 801A4DC0
// Loads the screen and fades it in (the background tinted 0x77) over 250 ms.
void CRDTarget::ShowCredits() {
  m_visible = 1;
  m_photoOn = 0;
  m_cheatOn = _globals.IsPlayerCheatOn(9) ? 1 : 0;
  if (m_shown || m_tweening) return;
  if (UIScreenManager* usm = ScreenManager()) usm->LoadUIScreen(0x2a, 0);
  UIReflow r;
  r.OpenReflowBlock();
  r.Reflow_Id("CRD", 0);
  r.Reflow_Alpha(0);
  r.Reflow_Id("CRD_background", 0);
  r.Reflow_ColorOverride(0x77);
  r.SubmitReflowBlock();
  r.OpenTweenBlock("CRD", 0xfa, 0, "CRD_show_callback", 0);
  r.Tween_Alpha(0xff);
  r.SubmitTweenBlock();
  m_tweening = 1;
  m_shown = 1;
}

// 801A4EEC
void CRDTarget::HideCredits() {
  UIReflow r;
  if (m_shown && !m_tweening) {
    r.OpenTweenBlock("CRD", 0xfa, 0, "CRD_hide_callback", 0);
    r.Tween_Alpha(0);
    r.SubmitTweenBlock();
    m_tweening = 1;
  }
}

// 801A4F80
void CRDTarget::UpdateCredits() {}

// 801A4F84
void CRDTarget::StartCredits() {
  if (m_running == 1) EndCredits();
  if (m_running == 0) m_startTime = static_cast<float>(_pClockMan->GetSec(&_sysclock));
  m_done = 0;
  m_running = 1;
}

// 801A5000
// Frees what SetupForCreditsRender made (the surface, shader and mask texture) and the text.
void CRDTarget::EndCredits() {
  m_done = 1;
  m_running = 0;
  if (m_surface) {
    _pGfx->Destroy(m_surface);
    m_surface = nullptr;
  }
  if (m_shader) {
    m_shader->DelRef();
    m_shader = nullptr;
  }
  if (m_maskTexture) {
    _pGfx->Destroy(m_maskTexture);
    m_maskTexture = nullptr;
  }
  delete[] m_text;
  m_text = nullptr;
}

// 801A566C
void CRDTarget::InitCreditsTextFetch() {
  m_text = new unsigned short[kTextChars]();
  const unsigned short* block = EGlobal::NewUIText("creditsblock0");
  const size_t n = Len16(block);
  for (size_t i = 0; i < n && i < kTextChars - 1; ++i) m_text[i] = block[i];
  m_nextBlock = 1;
  m_top = m_text;
}

// 801A56FC
// Draws the credits into the box: from m_yBase down, each text line (CR, LF or CR LF ended) is laid
// out centred at 14 points in the box's width (cut back at a space or '|' and the rest moved to the
// next line when too long), faded near the top and bottom, until the box is full; the next text block
// is appended when the text runs out. Then scrolls up (a tenth of the box a second); a line scrolled
// off the top is dropped, and the credits end with the text.
void CRDTarget::ScrollingCallback(ERC* rc, structDrawCBparams* p) {
  if (!m_running || m_done || !p) return;
  auto* font = static_cast<ERFont*>(_globals.m_font);
  if (!font) return;
  font->SetSize(14.0f, 1.0f, true);
  const EVec4& sc = *p->scale;
  const EVec4& tr = *p->translate;
  EVec4 colour{sc.x + tr.x, sc.y + tr.y, sc.z + tr.z, sc.w + tr.w};
  font->m_color = colour;
  const float alpha = sc.w + tr.w;
  const EVec2 topLeft = *p->topLeft, bottomRight = *p->bottomRight;
  if (!m_text) {
    m_lineHeight = font->DoGetStringSize(const_cast<unsigned short*>(kKatrina), true, false, nullptr).y;
    m_yBase = bottomRight.y - m_lineHeight;
    InitCreditsTextFetch();
  }
  bool first = true;
  float firstHeight = 0.0f;
  // GC: a depth clear of the screen (ZClear's depth was an unset register) before each line.
  const float clearZ = 1.0f;
  rc->ZTest(false, 2, 0, 0);
  rc->ZClear(0.0f, 0.0f, 1.0f, 1.0f, clearZ);
  rc->Stencil(0, 0, 0);
  float y = m_yBase;
  unsigned short* line = m_top;
  EMat4 matrix;
  matrix.Id();
  const float bottom = bottomRight.y - m_lineHeight;
  while (y <= bottom) {
    unsigned short* q = line;
    bool text = false;
    if (*q != '\n') {
      if (*q != 0 && *q != '\r') {
        text = true;
        do ++q;
        while (*q && *q != '\n' && *q != '\r');
      }
      if (*q == 0) {  // out of text: append the next block and try again
        char name[16];
        std::snprintf(name, sizeof name, "creditsblock%c", static_cast<char>('0' + (m_nextBlock & 0xff)));
        m_nextBlock = (m_nextBlock + 1) % 10;
        const size_t keep = static_cast<size_t>(q - m_top);
        std::memmove(m_text, m_top, keep * 2);
        const unsigned short* block = EGlobal::NewUIText(name);
        size_t n = 0;
        for (; block && block[n] && keep + n < kTextChars - 1; ++n) m_text[keep + n] = block[n];
        m_text[keep + n] = 0;
        line = m_text + (line - m_top);
        m_top = m_text;
        if (n == 0) break;  // PC: an empty block would loop forever
        continue;
      }
    }
    if (text) {
      rc->ZClear(0.0f, 0.0f, 1.0f, 1.0f, clearZ);
      rc->Stencil(0, 0, 0);
      // Full alpha in the middle m_fade of the box, fading to 0 at its top and bottom.
      const float half = m_fade * 0.5f, range = bottom - topLeft.y;
      const float upper = (half + 0.5f) * range + topLeft.y, lower = (0.5f - half) * range + topLeft.y;
      float a = 1.0f;
      if (y > upper) a = 1.0f - (y - upper) / (bottom - upper);
      else if (y < lower) a = 1.0f - (lower - y) / (lower - topLeft.y);
      colour.w = a * alpha;
      EVec4 wordScale{1.0f, 1.0f, 1.0f, colour.w}, wordTranslate{0.0f, 0.0f, 0.0f, 0.0f};
      EVec2 boxTopLeft{0.0f, 0.0f}, boxBottomRight{bottomRight.x - topLeft.x, m_lineHeight};
      EVec3 at{topLeft.x, y, 0.0f};
      matrix.Translate(at);
      unsigned short saved = *q;
      *q = 0;
      auto* para = new AptParagraph;
      para->Build(boxTopLeft, boxBottomRight, 14.0f, colour, 2, line);
      *q = saved;
      if (para->m_first && para->m_first->m_next) {  // too long: cut back a word at a time
        while (q != line) {
          delete para;
          do --q;
          while (q != line && *q != ' ' && *q != '|');
          saved = *q;
          *q = 0;
          para = new AptParagraph;
          para->Build(boxTopLeft, boxBottomRight, 14.0f, colour, 2, line);
          *q = saved;
          if (!para->m_first || !para->m_first->m_next) break;
        }
        *q = '\n';
      }
      const float h = para->GetParagraphHeight();
      font->m_color = colour;
      font->Select(rc);
      para->Draw(rc, &matrix, &wordScale, &wordTranslate);
      delete para;
      y += h;
      if (first) firstHeight = h;
    } else {
      y += m_lineHeight;
      if (first) firstHeight = m_lineHeight;
    }
    first = false;
    if (q[0] == '\r' && q[1] == '\r' && q[2] == '\n') line = q + 3;
    else if (q[0] == '\r' && q[1] == '\n') line = q + 2;
    else line = q + 1;
  }

  // Scroll.
  float step = m_running == 2 ? 0.0f : _dt * (bottomRight.y - topLeft.y) * 0.1f * m_speed;
  const int index = _pCtrlMan->GetPlayerControllerIndex(0);
  if (index >= 0)
    if (EController* c = _pCtrlMan->GetController(static_cast<unsigned int>(index))) {
      if (c->GetBtnResult(c->GetBtnDownMask(0), false, 8)) step *= 5.0f;
      else if (c->GetBtnResult(c->GetBtnDownMask(0), false, 4)) step = 0.0f;
    }
  if (firstHeight < step) step = firstHeight;
  const float next = m_yBase - step;
  if (topLeft.y <= next) {
    m_yBase = next;
    return;
  }
  m_yBase = next + firstHeight;
  unsigned short* t = m_top;
  if (*t != '\r' && *t != '\n') {
    if (*t == 0) {
      EndCredits();
      return;
    }
    do ++t;
    while (*t && *t != '\r' && *t != '\n');
    m_top = t;
  }
  if (*t == 0) {
    EndCredits();
    return;
  }
  if (t[0] == '\r' && t[1] == '\r' && t[2] == '\n') t += 3;
  else if (t[0] == '\r' && t[1] == '\n') t += 2;
  else t += 1;
  m_top = t;
}

// 801A6108
void CRDTarget::ShowPhoto() {
  UIReflow r;
  r.OpenReflowBlock();
  r.Reflow_Id("CRD_photo", 0);
  r.Reflow_Visibility(true);
  r.Reflow_X(0x280);
  r.SubmitReflowBlock();
  m_photoFrame = 0;
}

// 801A6188
void CRDTarget::HidePhoto() {
  UIReflow r;
  r.ResetVisibility("CRD_photo", false, 0);
}
