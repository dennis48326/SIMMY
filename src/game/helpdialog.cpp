// Port of game/helpdialog.cpp: HelpDialog. See helpdialog.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>

namespace {
std::vector<unsigned short> Text(const unsigned short* const* handle) {
  std::vector<unsigned short> v;
  for (const unsigned short* s = handle ? *handle : nullptr; s && *s; ++s) v.push_back(*s);
  return v;
}
}  // namespace

// 801759E4
HelpDialog::HelpDialog() {
  m_players = 3;
  m_field14 = 1;
  m_timeoutMs = -1;
}

// 80175B10
HelpDialog::~HelpDialog() = default;

// 80175B40
int HelpDialog::SpawnHelpDialog(int player, const char* title, const char* body, std::function<void(int)> callback,
                                bool uiStrings, const char* button) {
  if (m_active) return 0;
  m_callback = std::move(callback);
  m_active = 1;
  m_iconW = 0;
  m_iconH = 0;
  m_title = Text(uiStrings ? _globals.GetNewUIString(title) : _globals.GetPDAString(title));
  m_body = Text(uiStrings ? _globals.GetNewUIString(body) : _globals.GetPDAString(body));
  if (!button) {
    m_sel[0] = Text(_globals.GetNewUIString("close_help"));
    m_field24 = 1;
  } else {
    m_sel[0] = Text(_globals.GetNewUIString(button));
    m_field24 = 0;
  }
  m_sel[1].clear();
  m_sel[2].clear();
  m_continueType = 0;
  m_mode = 3;
  m_numSelections = 1;
  m_preselect = 0;
  m_field10 = 1;
  m_field14 = 1;
  m_players = player == 0 ? 1 : 2;
  m_onSelection = [this](int selection) { SelectionCallback(selection); };
  m_onZeroInput = [this] { ZeroInputCallback(); };
  m_onClosed = [this] { DialogClosedCallback(); };
  SpawnDialog();  // GC: the dialog's debug name "Help Dialog"
  return 1;
}

// 80175EDC
void HelpDialog::SpawnHelpDialog(int player, const char* name, std::function<void(int)> callback, bool uiStrings,
                                 const char* button) {
  char title[256], body[256];
  std::snprintf(title, sizeof title, "%s_title", name);
  std::snprintf(body, sizeof body, "%s_body", name);
  SpawnHelpDialog(player, title, body, std::move(callback), uiStrings, button);
}

// 80175F68
// The page is closed; the owner hears it (HelpDialogCallback vtable 0xC).
void HelpDialog::SelectionCallback(int selection) {
  m_active = 0;
  if (m_callback) m_callback(selection);
}
