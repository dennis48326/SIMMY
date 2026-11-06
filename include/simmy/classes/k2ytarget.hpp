// @class K2YTarget after UIObjectBase
// The name keyboard (game/targets_s2c.cpp, UI screen 0x17): the movie lays out three keyboards of four
// rows of twelve keys from the "k2y_<kb>_<row>_<key>" strings and types into the UIDB name being
// entered (m_Mode: 2/3 player 1's first/last name, 4/5 player 2's, 6 a family name, 7/8 a family
// member's first/last name; 0/1 finished/cancelled).
#pragma once

struct K2YTarget : UIObjectBase {
  K2YTarget();  // 801BDA58
  ~K2YTarget() override;  // 801BE304
  static unsigned short* GetName();  // 801BE460
  static bool IsModeValid();  // 801BE548
  void SetVariable(char* name, char* value) override;  // 801BE56C
  char* GetVariable(char* name) override;  // 801BE81C
  unsigned short* GetLocalizable(char* name) override;  // 801BEB00
  unsigned int GetKeybits(int keyboard, int row);  // 801BED54
  static void SetStartMode(int mode);  // 801BEF40
  static void SaveName();  // 801BEFA0
  static int GetMode();  // 801E7098
  static unsigned short* GetSavedFirstName();  // 801E70A0
  static unsigned short* GetSavedLastName();  // 801E70AC

  static int m_Mode;
  static unsigned short m_savedFirstName[32];  // the names before the keyboard (to cancel)
  static unsigned short m_savedLastName[32];
};
