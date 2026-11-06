// Port of UIAnalog (game/uisupport.cpp): the sticks as Apt variables.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>

// 80180994
UIAnalog::UIAnalog() {
  AttachTables("UIAnalog");
  auto get = [this](char* v) { return GetVariable(v); };
  for (const char* n : {"UIAnalog_exists", "0LeftAnalog_X", "0LeftAnalog_Y", "0RightAnalog_X", "0RightAnalog_Y",
                        "1LeftAnalog_X", "1LeftAnalog_Y", "1RightAnalog_X", "1RightAnalog_Y"})
    m_getTable->InstallCommand(n, get, this);
}

// 80180ED4
UIAnalog::~UIAnalog() {
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
  }
}

// 80181018
// "<player><Left|Right>Analog_<X|Y>": the stick's axis (-1..1) as "%f".
char* UIAnalog::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  const char c = *name;
  if (c == '0' || c == '1') ++name;
  const int player = c == '1' ? 1 : 0;
  EController* pad = _pCtrlMan->GetController(static_cast<unsigned int>(_pCtrlMan->GetPlayerControllerIndex(static_cast<unsigned int>(player))));
  const unsigned int filter = static_cast<AptViewer*>(_globals.m_aptViewer)->GetCurrentAptButtonFilter(player);
  int stick, axis;
  if (!std::strcmp("LeftAnalog_X", name)) stick = 0, axis = 0;
  else if (!std::strcmp("LeftAnalog_Y", name)) stick = 0, axis = 1;
  else if (!std::strcmp("RightAnalog_X", name)) stick = 1, axis = 0;
  else if (!std::strcmp("RightAnalog_Y", name)) stick = 1, axis = 1;
  else {
    if (!std::strcmp("UIAnalog_exists", name)) std::strcpy(out, "loaded");
    return out;
  }
  const double value = pad ? pad->GetStick(stick, axis, filter) : 0.0;
  std::snprintf(out, 0x20, "%f", value);
  return out;
}
