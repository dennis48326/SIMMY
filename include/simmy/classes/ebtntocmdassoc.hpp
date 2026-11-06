// @class EBtnToCmdAssoc
// One entry of a controller command map: a game command and the button mask that triggers it.
// Maps are sorted by command so EController::FindFirstCommand can binary-search them.
#pragma once

struct EBtnToCmdAssoc {
  unsigned int m_command;
  unsigned int m_buttons;

  static int CompareControllerCommands(void* a, void* b);
  static int CompareControllerCommandToKey(void* key, void* entry);
};
