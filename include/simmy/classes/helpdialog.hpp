// @class HelpDialog after UIObjectBase UIDialog
// A help page in the dialog box (game/helpdialog.cpp): a title and body from the PDA strings (or the
// UI strings), one button ("close_help" unless another is given); the owner hears the button
// (HelpDialogCallback, PC: a function) and can chain the next page from it.
#pragma once

#include <functional>

// 0x90 bytes (UIDialog + vtable +0x84, +0x88 active, +0x8C callback).
struct HelpDialog : UIDialog {
  HelpDialog();  // 801759E4
  ~HelpDialog() override;  // 80175B10
  // `player` 0 or 1; `title`/`body` string names; `uiStrings` NewUIStrings instead of PDA strings;
  // `button` the button's UI string (null: "close_help"). 0: one is already up.
  int SpawnHelpDialog(int player, const char* title, const char* body, std::function<void(int)> callback, bool uiStrings,
                      const char* button);  // 80175B40
  // "<name>_title" / "<name>_body".
  void SpawnHelpDialog(int player, const char* name, std::function<void(int)> callback, bool uiStrings,
                       const char* button);  // 80175EDC
  void SelectionCallback(int selection) override;  // 80175F68

  int m_active = 0;                         // +0x88 the dialog is up
  std::function<void(int)> m_callback;      // +0x8C HelpDialogCallback
};
