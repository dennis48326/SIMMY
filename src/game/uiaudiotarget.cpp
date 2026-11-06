// Port of UIAUDIOTarget (game/uisupport.cpp): the movies' "audio:" variables play UI sounds.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

#include <cstring>

void InstallHUDSounds(const char* group) {
  static const struct {
    const char* name;
    unsigned int id;
  } kSounds[] = {
      {"HUD_UI_BACK_SAMPLE", 0x48ae94fu},           {"HUD_UI_SPEED_0_TO_1_SAMPLE", 0xe1327588u},
      {"HUD_UI_SPEED_1_TO_2_SAMPLE", 0xb367f797u},  {"HUD_UI_SPEED_2_TO_1_SAMPLE", 0xacfad483u},
      {"HUD_UI_SPEED_1_TO_0_SAMPLE", 0x5d6996bbu},  {"HUD_UI_CAMERA_ZOOM_IN_SAMPLE", 0xef15b7b1u},
      {"HUD_UI_CAMERA_ZOOM_OUT_SAMPLE", 0xaf365be5u}, {"HUD_UI_PLUMBOB_LOCK_SAMPLE", 0x8a675108u},
      {"HUD_UI_PLUMBOB_RETURN_SAMPLE", 0x249fbaa5u}, {"HUD_UI_BROWSE_SIM_SAMPLE", 0x4332346au},
      {"HUD_UI_SELECT_SAMPLE", 0x374bdceau},        {"HUD_UI_OPEN_SAMPLE", 0xcd34abdcu},
      {"HUD_UI_BROWSE_SAMPLE", 0x64ac15u},          {"HUD_UI_OUT_OF_BOUNDS_SAMPLE", 0x6e2778b7u},
      {"HUD_UI_MONEY_UP_SAMPLE", 0x75f2dd8eu},      {"HUD_UI_MONEY_DOWN_SAMPLE", 0x87fa0f12u},
      {"HUD_UI_REP_UP_SAMPLE", 0xec9cb000u},        {"HUD_UI_MOOD_UP_SAMPLE", 0x9cc04cd6u},
      {"HUD_UI_MOOD_DOWN_SAMPLE", 0x729092aeu},     {"HUD_UI_MOOD_LOW_SAMPLE", 0x968c0a16u}};
  for (const auto& s : kSounds)
    UIAUDIO::Instance()->InstallEntry(const_cast<char*>(s.name), s.id, const_cast<char*>(group));
}

// 80180348
// Fetches the shared tables (and makes UIAUDIO) and installs "audio:".
UIAUDIOTarget::UIAUDIOTarget() {
  AttachTables("UIAUDIOTarget");
  UIAUDIO::Instance();
  m_setTable->InstallCommand("audio:", [this](char* n, char* v) { SetVariable(n, v); }, this);
}

// 8018078C
UIAUDIOTarget::~UIAUDIOTarget() {
  if (!m_installed) return;
  m_getTable->UnInstallAllCommands(this);
  m_setTable->UnInstallAllCommands(this);
  m_locTable->UnInstallAllCommands(this);
  UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
}

// 801808D0
// "audio:NAME[:...]": the sound installed as NAME, if any. (The value is not used.)
void UIAUDIOTarget::SetVariable(char* name, char* value) {
  const size_t n = name ? std::strlen(name) : 0;
  if (n == 0 || !value) return;
  char sound[132];
  sound[0] = 0;
  int colons = 0, len = 0;
  for (size_t i = 0; i < n; ++i) {
    if (name[i] == ':') {
      ++colons;
    } else if (colons == 1 && len < static_cast<int>(sizeof sound) - 1) {  // PC: bounded (GC: 132 bytes)
      sound[len++] = name[i];
      sound[len] = 0;
    }
  }
  const unsigned int id = UIAUDIO::Instance()->GetSound(sound);
  simmy::SoundTrace("ui audio:%s -> %08X%s", sound, id, id ? "" : " (not installed)");
  if (id) UIAUDIO::Instance()->PlaySound(id);
}
