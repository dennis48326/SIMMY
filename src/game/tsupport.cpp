// Port of game/tsupport.cpp (partial): SerializeUIGameData, the players' UI settings saved with the game
// as the 'Uixx' registered sections of the save (four 0x400-byte sub-sections, 'Uix1'..'Uix4'): the
// phone's ringtone, the PDA skin, the message flags and the PDA's open messages, each a little-endian
// word (the GameCube swapped them, Swizzle4).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

unsigned short MSGTarget_s_playerMessages[2][0x14] = {};  // 80439D90 MSGTarget::s_playerMessages
unsigned char PDATarget_m_pdaOpenMessages[2][0xe] = {};   // 80439DE0 PDATarget::m_pdaOpenMessages

namespace {
unsigned int* m_pLoadBuffers[4] = {};  // 80436C10 SerializeUIGameData::m_pLoadBuffers (0x400 bytes each)
unsigned int* m_pBuffer = nullptr;     // 804FDE48 SerializeUIGameData::m_pBuffer

// 'Uix1'..'Uix4' -> 0..3; -1 for others.
int SectionIndex(int id) {
  const unsigned int v = static_cast<unsigned int>(id);
  return v >= 0x55697831u && v <= 0x55697834u ? static_cast<int>(v - 0x55697831u) : -1;
}
unsigned int LE(unsigned int v) {
  unsigned char b[4] = {static_cast<unsigned char>(v), static_cast<unsigned char>(v >> 8), static_cast<unsigned char>(v >> 16),
                        static_cast<unsigned char>(v >> 24)};
  unsigned int out;
  std::memcpy(&out, b, 4);
  return out;
}
unsigned int FromLE(unsigned int w) {
  unsigned char b[4];
  std::memcpy(b, &w, 4);
  return b[0] | b[1] << 8 | b[2] << 16 | static_cast<unsigned int>(b[3]) << 24;
}
}  // namespace

// 80190464
void SerializeUIGameData::Load(NghResFile__SectionID section, NghResFile__SaveRecord& record) {
  const int i = SectionIndex(section);
  if (i < 0 || !m_pLoadBuffers[i]) return;
  std::memcpy(m_pLoadBuffers[i], record.m_data, record.m_size < 0x400 ? record.m_size : 0x400);
}

// 80190514
// Both players' settings from the loaded data (GC: both from the first buffer).
void SerializeUIGameData::SetupPlayerUIData() {
  if (m_pLoadBuffers[0]) SerializeUIData(0, m_pLoadBuffers[0], SerializeUIGameData__eUIGameData_Direction{2}, false);
  if (m_pLoadBuffers[0]) SerializeUIData(1, m_pLoadBuffers[0], SerializeUIGameData__eUIGameData_Direction{2}, false);
}

// 80190574
void SerializeUIGameData::AllocateLoadBuffers() {
  for (unsigned int*& b : m_pLoadBuffers) {
    delete[] b;
    b = new unsigned int[0x100]();
  }
}

// 801905E8
// 'Uix1' is player 1's settings now; the others are passed through as they were loaded.
void SerializeUIGameData::Save(NghResFile__SectionID section, NghResFile__SaveRecord& record) {
  m_pBuffer = new unsigned int[0x100]();
  const int i = SectionIndex(section);
  if (i == 0) SerializeUIData(0, m_pBuffer, SerializeUIGameData__eUIGameData_Direction{1}, false);
  else if (m_pLoadBuffers[i < 0 ? 0 : i]) std::memcpy(m_pBuffer, m_pLoadBuffers[i < 0 ? 0 : i], 0x400);
  record.m_size = 0x400;
  record.m_data = m_pBuffer;
}

// 80190728
void SerializeUIGameData::SaveComplete(NghResFile__SectionID, NghResFile__SaveRecord&) {
  delete[] m_pBuffer;
  m_pBuffer = nullptr;
}

// 80190764
// Direction 1 saves player `player`'s settings into `data`, 2 loads them; `reset` writes or loads the
// defaults (ringtone sample 0x457536C3, everything else 0).
void SerializeUIGameData::SerializeUIData(int player, unsigned int* data, SerializeUIGameData__eUIGameData_Direction direction,
                                          bool reset) {
  const bool save = direction == 1;
  auto word = [&](unsigned int* w, int value, int resetValue) -> int {  // one setting through `w`
    if (save) {
      *w = LE(reset ? static_cast<unsigned int>(resetValue) : static_cast<unsigned int>(value));
      return value;
    }
    return reset ? resetValue : static_cast<int>(FromLE(*w));
  };
  char* ringtone = const_cast<char*>(player == 0 ? "P1RingtoneSample" : "P2RingtoneSample");
  const int tone = word(&data[0], save ? UIDB::UIDBGetInt(ringtone) : 0, 0x457536c3);
  if (!save) UIDB::UIDBSetInt(ringtone, tone);
  char* skin = const_cast<char*>(player == 0 ? "PDA_skin" : "PDA_skinP2");
  const int pda = word(&data[1], save ? UIDB::UIDBGetInt(skin) : 0, 0);
  if (!save) UIDB::UIDBSetInt(skin, pda);
  unsigned int* w = data + 2;
  for (unsigned short& m : MSGTarget_s_playerMessages[player & 1]) {
    const int v = word(w++, m, 0);
    if (!save) m = static_cast<unsigned short>(v);
  }
  for (unsigned char& m : PDATarget_m_pdaOpenMessages[player & 1]) {
    const int v = word(w++, m, 0);
    if (!save) m = static_cast<unsigned char>(v);
  }
}
