// @class OptionsRecon
// The game's options (game/optionsrecon.cpp, _globals +0x148, 0xF4 bytes): the sound volumes, the
// vibration and other switches, saved with the game (the config block, 0x2000 bytes at 0x2000 in the
// save file) through the card cache. Fields keep their GameCube offsets: some readers index the
// object's bytes (the music volume at +0x1D).
#pragma once

struct OptionsRecon {
  OptionsRecon();  // 8005887C
  OptionsRecon(OptionsRecon& other);  // 800588D0
  OptionsRecon& operator=(OptionsRecon& other);  // 803A0380
  void ResetToDefaults();  // 8005892C
  void DoStream(ReconBuffer* buffer, int version);  // 8005898C
  int WriteOut();  // 80058BFC
  RET_UNKNOWN WriteToMemoryCard(char *, unsigned char);  // 80058D24
  int ReadIn(int card);  // 80058D2C
  RET_UNKNOWN ReadFromMemoryCard(char *, unsigned char);  // 80058E54
  void PreservePreferences();  // 80058E5C
  void PreservePrefsForOnline();  // 80058EE8
  void RestorePreferences();  // 80058EFC
  void RestorePrefsForOnline();  // 80058F80

  int m_opt00 = 1;            // +0x00 (bools, streamed in this order)
  int m_opt04 = 1;            // +0x04
  int m_opt08 = 1;            // +0x08
  int m_opt0C = 0;            // +0x0C
  int m_opt10 = 0;            // +0x10
  int m_opt14 = 1;            // +0x14
  int m_opt18 = 0;            // +0x18
  signed char m_vol1C = 10;   // +0x1C volumes 0..10
  signed char m_vol1D = 10;   // +0x1D the music volume
  signed char m_vol1E = 10;   // +0x1E
  signed char m_opt1F = 0;    // +0x1F
  signed char m_opt20 = 0;    // +0x20
  int m_opt24 = 0;            // +0x24
  signed char m_vol28 = 10;   // +0x28
  signed char m_vol29 = 10;   // +0x29
  // +0x2C..+0x4E: the preferences kept by PreservePreferences.
  int m_saved00 = 0, m_saved04 = 0, m_saved08 = 0, m_saved10 = 0, m_saved14 = 0, m_saved18 = 0, m_saved24 = 0;
  signed char m_saved1C = 0, m_saved1D = 0, m_saved1E = 0, m_saved1F = 0, m_saved20 = 0, m_saved28 = 0,
              m_saved29 = 0;
  int m_savedOnline10 = 0;    // +0x50 PreservePrefsForOnline
  signed char m_opt54 = -1;   // +0x54
  unsigned char m_pad55[3] = {};
  unsigned char m_str58Hdr[8] = {};           // +0x58 StringBuffer2 (GC: pointer and capacity)
  unsigned short m_str58[0x20] = {};          // +0x60
  unsigned char m_strA0Hdr[8] = {};           // +0xA0 StringBuffer2
  unsigned short m_strA0[0x20] = {};          // +0xA8
  int m_optE8 = 0;            // +0xE8
  int m_optEC = 0;            // +0xEC
  int m_optF0 = 0;            // +0xF0
};
