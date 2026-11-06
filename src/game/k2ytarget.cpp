// Port of K2YTarget (game/targets_s2c.cpp): the name keyboard; see k2ytarget.hpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

int K2YTarget::m_Mode = 0;
unsigned short K2YTarget::m_savedFirstName[32] = {};
unsigned short K2YTarget::m_savedLastName[32] = {};

namespace {
constexpr size_t kNameMax = 0x40;  // a UIDB string
size_t Len16(const unsigned short* s) {
  size_t n = 0;
  while (s && s[n]) ++n;
  return n;
}
void Append(unsigned short* name, const unsigned short* text) {
  size_t n = Len16(name);
  for (; text && *text && n < kNameMax - 1; ++text) name[n++] = *text;
  name[n] = 0;
}
unsigned short* Malloc16(const unsigned short* text) {
  const size_t n = Len16(text);
  auto* out = static_cast<unsigned short*>(SimsAptMalloc((n + 1) * 2));
  for (size_t i = 0; i < n; ++i) out[i] = text[i];
  out[n] = 0;
  return out;
}
}  // namespace

// 801BDA58
K2YTarget::K2YTarget() {
  AttachTables("K2YTarget");
  auto get = [this](char* v) { return GetVariable(v); };
  auto set = [this](char* n, char* v) { SetVariable(n, v); };
  auto loc = [this](char* v) { return GetLocalizable(v); };
  for (const char* n : {"K2Y_exists", "K2Y_name_length", "K2Y_player_num", "K2Y_mode"}) m_getTable->InstallCommand(n, get, this);
  for (int kb = 0; kb < 3; ++kb)
    for (int row = 0; row < 4; ++row)
      m_getTable->InstallCommand(("K2Y_kb" + std::to_string(kb) + "_row" + std::to_string(row)).c_str(), get, this);
  for (const char* n : {"K2Y_title", "K2Y_first_name", "K2Y_last_name"}) m_locTable->InstallCommand(n, loc, this);
  for (const char* n : {"K2Y_back", "K2Y_add", "K2Y_space", "K2Y_enter", "K2Y_exit"}) m_setTable->InstallCommand(n, set, this);
  const struct {
    const char* name;
    unsigned int id;
  } kSounds[] = {{"K2Y_UI_BROWSE_KBD_SAMPLE", 0xcb2fa7cdu}, {"K2Y_UI_BACK_SAMPLE", 0x48ae94fu},
                 {"K2Y_UI_SELECT_SAMPLE", 0x374bdceau},     {"K2Y_UI_OUT_OF_BOUNDS_SAMPLE", 0x6e2778b7u},
                 {"K2Y_UI_CANCEL_SAMPLE", 0x2aaff358u},     {"K2Y_UI_ROLLOVER_SAMPLE", 0x4ede7dc2u},
                 {"K2Y_UI_OPEN_SAMPLE", 0xcd34abdcu}};
  for (const auto& s : kSounds) UIAUDIO::Instance()->InstallEntry(const_cast<char*>(s.name), s.id, const_cast<char*>(m_name));
  std::memset(m_savedFirstName, 0, sizeof m_savedFirstName);
  std::memset(m_savedLastName, 0, sizeof m_savedLastName);
}

// 801BE304
K2YTarget::~K2YTarget() {
  if (m_installed) {
    m_getTable->UnInstallAllCommands(this);
    m_setTable->UnInstallAllCommands(this);
    m_locTable->UnInstallAllCommands(this);
    UIAUDIO::Instance()->UnInstallAllEntries(const_cast<char*>(m_name));
  }
}

// 801BE460
// The UIDB string being typed into (edited in place).
unsigned short* K2YTarget::GetName() {
  const char* row = nullptr;
  switch (m_Mode) {
    case 2: row = "Player1FirstName"; break;
    case 3: row = "Player1LastName"; break;
    case 4: row = "Player2FirstName"; break;
    case 5: row = "Player2LastName"; break;
    case 6: row = "FamilyName"; break;
    case 7: row = "FamilyFirstName"; break;
    case 8: row = "FamilyLastName"; break;
    default: break;
  }
  const unsigned short* name = row ? UIDB::UIDBGetString(const_cast<char*>(row)) : nullptr;
  static unsigned short s_none[kNameMax] = {' ', 0};  // DAT_803F1990
  return name ? const_cast<unsigned short*>(name) : s_none;
}

// 801BE548
bool K2YTarget::IsModeValid() { return m_Mode > 1 && m_Mode < 9; }

// 801BE56C
// Keys: "K2Y_back" deletes a letter, "K2Y_add" types the key's string (a NewUI string id), "K2Y_space"
// an underscore; "K2Y_enter" goes from the first name to the last (or finishes), "K2Y_exit" back.
void K2YTarget::SetVariable(char* name, char* value) {
  if (!std::strcmp("K2Y_back", name)) {
    if (!IsModeValid()) return;
    unsigned short* s = GetName();
    const size_t n = Len16(s);
    if (n) {
      s[n - 1] = 0;
      UIAUDIO::Instance()->PlaySound(0x48ae94fu);
    } else {
      UIAUDIO::Instance()->PlaySound(0x3804219fu);
    }
    return;
  }
  if (!std::strcmp("K2Y_add", name)) {
    if (!IsModeValid()) return;
    unsigned short* s = GetName();
    const unsigned short* key = EGlobal::NewUIText(value);
    if (!key) return;
    Append(s, key);
  } else if (!std::strcmp("K2Y_space", name)) {
    if (!IsModeValid()) return;
    static const unsigned short kUnderscore[2] = {'_', 0};
    Append(GetName(), kUnderscore);
  } else if (!std::strcmp("K2Y_enter", name)) {
    if (m_Mode == 4) m_Mode = 5;
    else if (m_Mode == 2) m_Mode = 3;
    else if (m_Mode == 7) m_Mode = 8;
    else m_Mode = 0;
  } else if (!std::strcmp("K2Y_exit", name)) {
    if (m_Mode == 5) m_Mode = 4;
    else if (m_Mode == 3) m_Mode = 2;
    else if (m_Mode == 8) m_Mode = 7;
    else m_Mode = 1;
  } else {
    return;
  }
  UIAUDIO::Instance()->PlaySound(0x374bdceau);
}

// 801BE81C
// "K2Y_name_length" (15 once the name is wider than 140 pixels at 12 points, ending input),
// "K2Y_kb<k>_row<r>" (which of its 12 keys exist, as "0"/"1"), "K2Y_mode" ("0" first name, "1" last
// name, "2" family name, "4" cancelled, "3" otherwise).
char* K2YTarget::GetVariable(char* name) {
  auto* out = static_cast<char*>(SimsAptMalloc(0x20));
  *out = 0;
  if (!std::strcmp("K2Y_exists", name)) {
    std::strcpy(out, "loaded");
  } else if (!std::strcmp("K2Y_player_num", name)) {
    std::snprintf(out, 0x20, "%d", 0);
  } else if (!std::strcmp("K2Y_name_length", name)) {
    if (IsModeValid()) {
      unsigned short* s = GetName();
      size_t n = Len16(s);
      auto* font = static_cast<ERFont*>(_globals.m_font);
      if (font) {
        font->SetSize(12.0f, 1.0f, true);
        if (font->DoGetStringSize(s, true, false, nullptr).x > 140.0f) n = 0xf;
      }
      std::snprintf(out, 0x20, "%d", static_cast<int>(n));
    }
  } else if (!std::strncmp("K2Y_kb", name, 6)) {
    const int keyboard = name[6] - '0';
    const int row = std::atoi(name + 0xb);
    unsigned int bits = GetKeybits(keyboard, row);
    for (int i = 0; i < 12; ++i, bits >>= 1) std::strcat(out, bits & 1 ? "1" : "0");
    out[0xc] = 0;
  } else if (!std::strcmp("K2Y_mode", name)) {
    char c = '3';
    switch (m_Mode) {
      case 2: case 4: case 7: c = '0'; break;
      case 3: case 5: case 8: c = '1'; break;
      case 6: c = '2'; break;
      case 1: c = '4'; break;
      default: break;
    }
    out[0] = c;
    out[1] = 0;
  }
  out[0x1f] = 0;
  return out;
}

// 801BEB00
unsigned short* K2YTarget::GetLocalizable(char* name) {
  if (std::isdigit(static_cast<unsigned char>(*name))) ++name;
  const char* row = nullptr;
  if (!std::strcmp("K2Y_title", name)) {
    switch (m_Mode) {
      case 2: case 4: row = "k2y_enter_first_name"; break;
      case 3: case 5: row = "k2y_enter_last_name"; break;
      case 6: row = "k2y_enter_family_only_name"; break;
      case 7: case 8: row = "k2y_enter_family_first_name"; break;
      default: break;
    }
  } else if (!std::strcmp("K2Y_first_name", name)) {
    row = "k2y_first_name";
  } else if (!std::strcmp("K2Y_last_name", name)) {
    row = "k2y_last_name";
  }
  unsigned short text[0x1000] = {};
  if (row)
    if (const unsigned short* t = EGlobal::NewUIText(row))
      for (size_t i = 0; t[i] && i < 0xfff; ++i) text[i] = t[i];
  UIQDTarget::UIQDSubstitutionStrings(text);
  return Malloc16(text);
}

// 801BED54
// Bit (11 - key) for each key "k2y_<kb>_<row>_<key>" that is one character other than '_', ' ', '<',
// '>' and '$' (the movie reads the bits lowest first).
unsigned int K2YTarget::GetKeybits(int keyboard, int row) {
  char prefix[32];
  std::snprintf(prefix, sizeof prefix, "k2y_%1d%1d", keyboard, row);
  unsigned int bits = 0;
  for (int key = 0xb; key >= 0; --key) {
    char id[40];
    std::snprintf(id, sizeof id, "%s%2.2d", prefix, key);
    const unsigned short* t = EGlobal::NewUIText(id);
    bits <<= 1;
    if (Len16(t) == 1 && t[0] != '_' && t[0] != ' ' && t[0] != '<' && t[0] != '>' && t[0] != '$') bits |= 1;
  }
  return bits;
}

// 801BEF40
// Starts on `mode` unless a last name is being entered (then back to its first name).
void K2YTarget::SetStartMode(int mode) {
  if (m_Mode == 5) m_Mode = 4;
  else if (m_Mode == 3) m_Mode = 2;
  else if (m_Mode == 8) m_Mode = 7;
  else if (m_Mode >= 0 && m_Mode < 8) m_Mode = mode;
  else m_Mode = 2;
}

// 801BEFA0
// The player's names as they are (restored if the keyboard is cancelled).
void K2YTarget::SaveName() {
  const int player = UIDB::UIDBGetInt(const_cast<char*>("PlayerNumber"));
  const char* first = player == 0 ? "Player1FirstName" : "Player2FirstName";
  const char* last = player == 0 ? "Player1LastName" : "Player2LastName";
  if (player != 0 && player != 1) return;
  auto copy = [](unsigned short* dst, const unsigned short* src) {
    size_t i = 0;
    for (; src && src[i] && i < 0x20; ++i) dst[i] = src[i];
    for (; i < 0x20; ++i) dst[i] = 0;
  };
  copy(m_savedFirstName, UIDB::UIDBGetString(const_cast<char*>(first)));
  copy(m_savedLastName, UIDB::UIDBGetString(const_cast<char*>(last)));
}

// 801E7098
int K2YTarget::GetMode() { return m_Mode; }
// 801E70A0
unsigned short* K2YTarget::GetSavedFirstName() { return m_savedFirstName; }
// 801E70AC
unsigned short* K2YTarget::GetSavedLastName() { return m_savedLastName; }
