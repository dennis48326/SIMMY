// Port of RMDTarget (game/targets_s2c.cpp), the reward moment dialog, and of H2DTarget's calls for
// it (SpawnRewardMomentDialog, RewardMomentDialogClosed, its part of Update). See mditarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

void* SimsAptMalloc(unsigned int size);

namespace {
UIScreenManager* Screens() { return static_cast<UIScreenManager*>(_globals.m_uiScreenManager); }
AptViewer* Viewer() { return static_cast<AptViewer*>(_globals.m_aptViewer); }
G2DTarget* TheG2D() { return static_cast<G2DTarget*>(_globals.m_g2dTarget); }
const char* SkipDigit(const char* name) { return std::isdigit(static_cast<unsigned char>(*name)) ? name + 1 : name; }
unsigned int PushFilterAll(int player, const char* name) {  // UIObjectBase::pushAptButtonFilterALL
  AptViewer* v = Viewer();
  const unsigned int id = v ? v->PushAptButtonFilter(player, const_cast<char*>(name)) : 0;
  if (id)
    for (int c = 1; c < 0x32; ++c) v->AddCmdToAptButtonFilter(player, id, c);
  return id;
}
std::vector<unsigned short> Text(const char* name) {
  std::vector<unsigned short> v;
  const unsigned short* t = EGlobal::NewUIText(name);
  for (size_t i = 0; t && t[i]; ++i) v.push_back(t[i]);
  v.push_back(0);
  return v;
}
size_t Length(const std::vector<unsigned short>& t) {
  size_t n = 0;
  while (n < t.size() && t[n]) ++n;
  return n;
}

// The kinds (H2DTarget::SpawnRewardMomentDialog's first argument): 0 recipe, 1 fashion, 2 location,
// 3/4 congratulations, 5 object. RMD_Title_Strings (803F8058), RMD_Button_Strings (803F80B0),
// RMD_Audio_Samples (80439D04), RMD_Model_Ids (80439D1C: model, shader, animation).
const char* const kTitles[6] = {"reward_reciple_title", "reward_fashion_title", "reward_location_title",
                                "reward_congrats_1",    "reward_congrats_1",    "reward_object_title"};
const char* const kButtons[6] = {"reward_recipe", "reward_fashion", "reward_location",
                                 "reward_congrats_1", "reward_congrats_1", "reward_object"};
const unsigned int kSamples[6] = {0xcf76ca31, 0x1d4927f8, 0xa8e77567, 0x2c556600, 0x6019840c, 0x1d4927f8};
const unsigned int kModels[6][3] = {{0x074e8e8e, 0x074e8e8e, 0x074e8e8e}, {0xe1b59838, 0xe1b59838, 0xe1b59838},
                                    {0x31709330, 0x31709330, 0x31709330}, {0x31709330, 0x31709330, 0x31709330},
                                    {0x31709330, 0x31709330, 0x31709330}, {0x44f18a84, 0x44f18a84, 0x44f18a84}};


int g_rmdQuickClose = 0;     // DAT_80476120 (a debug switch: reward moments close after 1 s)
RMDTarget* s_rmd = nullptr;  // H2DTarget +0xCC
int s_pendingKind = 0;       // H2DTarget +0xD4
int s_pendingIndex = 0;      // H2DTarget +0xD8
int s_pending = 0;           // H2DTarget +0xDC a dialog to make
int s_closed = 0;            // H2DTarget +0xE0 the dialog has closed: delete it
}  // namespace

// 801DEDE8
// Its commands, the frame's shaders (RMD_bg, the title and button pieces), the strings of its kind,
// the 3D model (UI3D "0RMD_model"); it holds the game (the pad goes to it but the Start button,
// command 7) and loads its screen (0x43), lays out the widgets and fades in with the kind's sound.
RMDTarget::RMDTarget(int kind, int index) : m_kind(kind), m_index(index) {
  AttachTables("0RMDTarget");
  m_filters[0] = PushFilterAll(0, "RMDTarget");
  if (AptViewer* v = Viewer()) v->RemoveCmdFromAptButtonFilter(0, m_filters[0], 7);
  if (_gd.m_fieldEC) {
    m_filters[1] = PushFilterAll(1, "RMDTarget");
    if (AptViewer* v = Viewer()) v->RemoveCmdFromAptButtonFilter(1, m_filters[1], 7);
  }
  UIUpdateTable::InstallEntry(0x4b, [this] { Update(); }, m_name, this);
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  m_getTable->InstallCommand("0RMD_exists", get, this);
  for (const char* n : {"0RMD_SELECT_BUTTON", "0hide_tween_RMD", "0show_tween_RMD"}) m_setTable->InstallCommand(n, set, this);
  for (const char* n : {"0RMD_btn_text", "0RMD_title"}) m_locTable->InstallCommand(n, loc, this);
  UI2D* ui2d = UI2D::Instance();
  auto shader = [&](const char* id, unsigned int s) { ui2d->InstallEntry(const_cast<char*>(id), s, const_cast<char*>(m_name), 0); };
  shader("0RMD_bg", 0xae12e3c2);
  shader("0RMD_title_A1", 0x4b7b0c9c);
  shader("0RMD_title_A2", 0xa912c11b);
  shader("0RMD_title_A3", 0x35457f32);
  shader("0RMD_btn_A1", 0x4b7b0c9c);
  shader("0RMD_btn_A2", 0xa912c11b);
  shader("0RMD_btn_A3", 0x35457f32);
  const int k = kind >= 0 && kind < 6 ? kind : 5;
  m_title = Text(kTitles[k]);
  m_button = Text(kButtons[k]);
  if (m_installed)
    UI3D::Instance()->InstallEntry(const_cast<char*>("0RMD_model"), kModels[k][0], kModels[k][1], kModels[k][2],
                                   const_cast<char*>(m_name), 0);
  m_time = 60.0f;
  if (g_rmdQuickClose) m_time = 1.0f;  // DAT_80476120
  m_timedOut = 0;
  H2DTarget::IncPause(false);
  m_paused = 1;
  if (UIScreenManager* usm = Screens()) usm->LoadUIScreen(0x43, 2);
  SetupWidgets();
  StartShowTween();
  UIAUDIO::Instance()->PlaySound(kSamples[k]);
  std::string t;
  for (size_t i = 0; i < Length(m_title); ++i) t += m_title[i] < 0x80 ? static_cast<char>(m_title[i]) : '?';
  std::printf("[dialog] reward moment (kind %d, #%d): \"%s\"\n", kind, index, t.c_str());
}

// 801DF648
RMDTarget::~RMDTarget() {
  for (int p = 0; p < 2; ++p)
    if (m_filters[p]) {
      if (AptViewer* v = Viewer()) v->PopAptButtonFilter(p, m_filters[p]);
      m_filters[p] = 0;
    }
  if (m_paused) {
    H2DTarget::ReleasePause(false);
    m_paused = 0;
  }
  if (m_exists) {
    if (m_installed) UI3D::Instance()->UnInstallEntry(const_cast<char*>("0RMD_model"));
    if (UIScreenManager* usm = Screens()) usm->UnloadUIScreen(0x43);
    m_exists = 0;
  }
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    UI3D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801DF838
// A: hides it (once shown); the show tween's end shows the model and the text boxes; the hide tween's
// end unloads it.
void RMDTarget::SetVariable(char* name, char*) {
  const char* n = SkipDigit(name);
  if (!std::strcmp("RMD_SELECT_BUTTON", n)) {
    if (m_state == 2) StartHideTween();
  } else if (!std::strcmp("show_tween_RMD", n)) {
    m_state = 2;
    UIReflow r;
    r.OpenReflowBlock();
    for (const char* id : {"RMD_model", "RMD_btn_container", "RMD_title_container"}) {
      r.Reflow_Id(id, 0);
      r.Reflow_Visibility(true);
    }
    r.SubmitReflowBlock();
  } else if (!std::strcmp("hide_tween_RMD", n)) {
    UIReflow r;
    r.ResetVisibility("RMD", false, 0);
    UnloadDialog();
  }
}

// 801DF994
char* RMDTarget::GetVariable(char* name) {
  const char* n = SkipDigit(name);
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  out[0] = 0;
  if (!std::strcmp("RMD_exists", n)) {
    m_exists = 1;
    std::strcpy(out, "loaded");
  }
  return out;
}

// 801DFA2C
unsigned short* RMDTarget::GetLocalizable(char* name) {
  const char* n = SkipDigit(name);
  std::vector<unsigned short> buf(0x1000, 0);
  const std::vector<unsigned short>* src = nullptr;
  if (!std::strcmp("RMD_title", n)) src = &m_title;
  else if (!std::strcmp("RMD_btn_text", n)) src = &m_button;
  if (src)
    for (size_t i = 0; i < Length(*src) && i < buf.size() - 1; ++i) buf[i] = (*src)[i];
  UIQDTarget::UIQDSubstitutionStrings(buf.data());
  size_t len = 0;
  while (buf[len]) ++len;
  auto* out = static_cast<unsigned short*>(SimsAptMalloc(static_cast<unsigned int>((len + 1) * 2)));
  for (size_t i = 0; i < len; ++i) out[i] = buf[i];
  out[len] = 0;
  return out;
}

// 801DFB1C
// Once shown it closes by itself when its time runs out.
void RMDTarget::Update() {
  if (!m_exists || m_state != 2 || m_timedOut) return;
  if (m_time > 0.0f) {
    m_time -= _dt;
    if (m_time <= 0.0f) {
      m_timedOut = 1;
      StartHideTween();
    }
  }
}

// 801DFB98
// The title's and the button's boxes sized to their texts (14-point paragraphs 640 wide: the title
// at least 160, the button at least 48, both even) and centred on the 640-wide screen at y 340 / 382.
void RMDTarget::SetupWidgets() {
  UIReflow r;
  r.OpenReflowBlock();
  EVec4 white{1.0f, 1.0f, 1.0f, 1.0f};
  auto width = [&](std::vector<unsigned short>& text) {
    AptParagraph p;
    EVec2 tl{0.0f, 0.0f}, br{640.0f, 0.0f};
    p.Build(tl, br, 14.0f, white, 0, text.data());
    return p.GetPrintWidth();
  };
  int w = static_cast<int>(width(m_title) - 22.0f);
  if (w < 0xa0) w = 0xa0;
  w = w * 2 - (w - (w >> 31) & ~1);  // GC: rounds down to even
  r.Reflow_Id("RMD_title_container", 0);
  r.Reflow_X(0x140 - static_cast<int>(static_cast<unsigned int>(w + 0x2e) >> 1));
  r.Reflow_Y(0x154);
  r.Reflow_Id("RMD_title_A2", 0);
  r.Reflow_Rotation(0.0f);
  r.Reflow_X(static_cast<int>((static_cast<unsigned int>(w + 8) >> 1) + 0x13));
  r.Reflow_H(static_cast<unsigned int>(w + 8));
  r.Reflow_Rotation(270.0f);
  r.Reflow_Id("RMD_title_A3", 0);
  r.Reflow_X(w + 0x24);
  r.Reflow_Id("RMD_title", 0);
  r.Reflow_W(static_cast<unsigned int>(w + 0x26));
  int b = static_cast<int>((width(m_button) + 36.0f) - 28.0f);
  if (b < 0x30) b = 0x30;
  b = b * 2 - (b - (b >> 31) & ~1);
  r.Reflow_Id("RMD_btn_container", 0);
  r.Reflow_X(0x140 - static_cast<int>(static_cast<unsigned int>(b + 0x34) >> 1));
  r.Reflow_Y(0x17e);
  r.Reflow_Id("RMD_btn_A2", 0);
  r.Reflow_Rotation(0.0f);
  r.Reflow_X(static_cast<int>((static_cast<unsigned int>(b + 8) >> 1) + 0x16));
  r.Reflow_H(static_cast<unsigned int>(b + 8));
  r.Reflow_Rotation(270.0f);
  r.Reflow_Id("RMD_btn_A3", 0);
  r.Reflow_X(b + 0x29);
  r.Reflow_Id("RMD_btn_text", 0);
  r.Reflow_W(static_cast<unsigned int>(b));
  r.SubmitReflowBlock();
}

// 801DFFA4
// The pad filters and the pause go, the commands and the screen; H2DTarget is told (it deletes it).
void RMDTarget::UnloadDialog() {
  m_state = 4;
  for (int p = 0; p < 2; ++p)
    if (m_filters[p]) {
      if (AptViewer* v = Viewer()) v->PopAptButtonFilter(p, m_filters[p]);
      m_filters[p] = 0;
    }
  if (m_paused) {
    H2DTarget::ReleasePause(false);
    m_paused = 0;
  }
  if (!m_exists) return;
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    m_updateTable->UnInstallEntry(this);
    UI2D::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
    m_installed = 0;
  }
  if (UIScreenManager* usm = Screens()) usm->UnloadUIScreen(0x43);
  m_exists = 0;
  if (_gd.m_h2dTarget) _gd.m_h2dTarget->RewardMomentDialogClosed();
}

// 801E014C
void RMDTarget::StartShowTween() {
  if (m_state != 0) return;
  m_state = 1;
  UIReflow r;
  r.OpenReflowBlock();
  r.Reflow_Id("RMD", 0);
  r.Reflow_Alpha(0);
  r.Reflow_Visibility(true);
  r.Reflow_Id("RMD_model", 0);
  r.Reflow_Visibility(false);
  r.SubmitReflowBlock();
  r.OpenTweenBlock("RMD", 0xfa, 0, "show_tween_RMD", 0);
  r.Tween_Alpha(0xff);
  r.SubmitTweenBlock();
}

// 801E022C
void RMDTarget::StartHideTween() {
  if (m_state != 2) return;
  m_state = 3;
  UIReflow r;
  r.OpenReflowBlock();
  r.Reflow_Id("RMD_model", 0);
  r.Reflow_Visibility(false);
  r.SubmitReflowBlock();
  r.OpenTweenBlock("RMD", 0xfa, 0, "hide_tween_RMD", 0);
  r.Tween_Alpha(0);
  r.SubmitTweenBlock();
}

// ---------------------------------------------------------------------------------------------------
// H2DTarget's parts (GC: members +0xCC..+0xE0; PC: kept here)

// 801B8C3C
// A reward moment of `kind` for unlock `index`, made by the next Update (when no G2D dialog is up);
// it holds the game at once. False while one is up or waiting.
RET_UNKNOWN H2DTarget::SpawnRewardMomentDialog(int kind, int index) {
  if (s_rmd || s_pending) return 0;
  s_pendingIndex = index;
  s_pending = 1;
  s_pendingKind = kind;
  IncPause(false);
  return 1;
}

// 801B90C0
void H2DTarget::RewardMomentDialogClosed() { s_closed = 1; }

// Part of 801B8484 (H2DTarget::Update): a closed reward moment is deleted (and the spawn's pause
// released); a waiting one is made once no G2D dialog is spawned.
void H2DUpdateRewardMoment() {
  // PC dev aid: SIMMY_REWARD_TEST=<n>[:<kind>] spawns a reward moment (kind 5, the object, by default)
  // on the n-th HUD update.
  static const char* test = std::getenv("SIMMY_REWARD_TEST");
  static int updates = 0;
  if (test && ++updates == std::atoi(test) && _gd.m_h2dTarget) {
    const char* colon = std::strchr(test, ':');
    _gd.m_h2dTarget->SpawnRewardMomentDialog(colon ? std::atoi(colon + 1) : 5, 0);
  }
  if (s_closed) {
    delete s_rmd;
    s_rmd = nullptr;
    s_closed = 0;
    H2DTarget::ReleasePause(false);
  }
  G2DTarget* g2d = TheG2D();
  if (s_pending && !(g2d && g2d->IsG2DSpawned())) {
    s_rmd = new RMDTarget(s_pendingKind, s_pendingIndex);
    s_pending = 0;
  }
}

// Part of 801B9318 (H2DTarget::IsDialogActive): a reward moment is up.
bool H2DRewardMomentActive() { return s_rmd != nullptr; }

// Part of 801B7B18 (H2DTarget::~H2DTarget).
void H2DDeleteRewardMoment() {
  delete s_rmd;
  s_rmd = nullptr;
  s_pending = 0;
  s_closed = 0;
}
