// @class UIAUDIO after EVoice UIAUDIO__UIAUDIORecord
// @class UIAUDIO__UIAUDIORecord
// The front end's sound player (game/uiaudio.cpp, UIAUDIO::Instance()): five voices used round
// robin for one-shot UI samples, plus a list of named sound entries that the Apt UI installs.
#pragma once

// A named sound entry (0x18 bytes; vtable at +0x14).
struct UIAUDIO__UIAUDIORecord {
  virtual ~UIAUDIO__UIAUDIORecord() = default;
  UIAUDIO__UIAUDIORecord* m_prev = nullptr;  // +0x00
  UIAUDIO__UIAUDIORecord* m_next = nullptr;  // +0x04
  char* m_name = nullptr;                    // +0x08
  unsigned int m_sound = 0;                  // +0x0C sample id
  char* m_group = nullptr;                   // +0x10 owner, for UnInstallAllEntries
};

struct UIAUDIO {
  using UIAUDIORecord = ::UIAUDIO__UIAUDIORecord;
  static constexpr int kVoices = 5;
  static constexpr int kRecords = 0x46;
  UIAUDIO();  // 801770C0
  virtual ~UIAUDIO();  // 801771D8
  void AddEntry(char* name, unsigned int sound, char* group);  // 801772D4
  void RemoveEntry(UIAUDIO__UIAUDIORecord* record);  // 8017733C
  bool InstallEntry(char* name, unsigned int sound, char* group);  // 801773D4
  bool UnInstallEntry(char* name);  // 8017747C
  bool UnInstallAllEntries(char* group);  // 801774F4
  bool ContainsEntry(char* name);  // 80177554
  unsigned int GetSound(char* name);  // 801775B4
  void PlaySound(unsigned int sound);  // 80177670
  void Update();  // 8017775C
  void UpdateAudioLoad(int slot);  // 801777A0
  void StopSound(int slot);  // 80177948
  static UIAUDIO* Instance();  // 801779DC
  static void Shutdown();  // 80177A18

  static UIAUDIO* s_pInstance;

  UIAUDIORecord* m_head = nullptr;            // +0x00 installed entries, most recently used first
  UIAUDIORecord* m_tail = nullptr;            // +0x04
  UIAUDIORecord* m_free = nullptr;            // +0x08 unused records
  int m_nextVoice = 0;                        // +0x0C round-robin slot
  EVoice* m_voices[kVoices] = {};             // +0x10
  ERSampledata* m_samples[kVoices] = {};      // +0x24 the slot's sample once loaded
  unsigned int m_sampleIds[kVoices] = {};     // +0x38 0 = slot idle
  float m_volumeLeft[kVoices] = {};           // +0x4C
  float m_volumeRight[kVoices] = {};          // +0x60
};
