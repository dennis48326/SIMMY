// @class EAudio after EA_EVENT EMsgQueue
// @class EVoice
// @class EVoiceDesc
// @class EPMDesc
// The engine's audio interface (engine/e_ngcaudio.cpp holds EAudio's few bodies): music streams and
// sample voices. Every method is pure in EAudio; ENgcAudio (_pActualAudio) implements them. The order
// below is the GC vtable's (slot offsets in comments, as the game's callers used them).
#pragma once

// A sample voice handed out by AudioAllocVoice (0x20 bytes).
struct EVoice {
  void reset() {  // linkonce, 803C70A0
    m_disabled = 0;
    m_volumeLeft = 1.0f;
    m_bound = 0;
    m_pitch = 1.0f;
    m_volumeRight = 1.0f;
    m_playing = 0;
  }

  int m_bound = 0;                    // +0x00 set by AudioBindVoice
  ERSampledata* m_sample = nullptr;   // +0x04 the bound sample (one reference held)
  int m_handle = -1;                  // +0x08 ENgcSFXStreamer voice, -1 when none
  float m_volumeLeft = 1.0f;          // +0x0C
  float m_volumeRight = 1.0f;         // +0x10
  float m_pitch = 1.0f;               // +0x14
  int m_playing = 0;                  // +0x18 refreshed by GetVoiceState
  int m_disabled = 0;                 // +0x1C no streamer voice (audio off, or none free)
};

// What Get/SetVoiceState read or change (0x18 bytes).
struct EVoiceDesc {
  enum : unsigned int { kVolume = 1, kPitch = 4, kPlay = 8 };  // m_mask bits (GetVoiceState sets 0x1F)
  unsigned int m_mask = 0;     // +0x00
  float m_volumeLeft = 1.0f;   // +0x04
  float m_volumeRight = 1.0f;  // +0x08
  float m_pitch = 1.0f;        // +0x0C
  int m_play = 0;              // +0x10 play (1) or pause (0); playing, when read
  int m_looping = 0;           // +0x14 read only: the sample loops
};

// A music request for PlayMusic (0x14 bytes).
struct EPMDesc {
  EPMDesc(unsigned int id, bool loop);  // 8032B874
  EPMDesc(char* name, bool loop);       // 8032B898

  unsigned int m_id = 0;   // +0x00 audiostreams resource id
  int m_field04 = 0;       // +0x04
  int m_field08 = -1;      // +0x08
  int m_loop = 0;          // +0x0C
  int m_field10 = 0;       // +0x10
};

struct EAudio {
  EAudio();            // 8032CB70
  virtual ~EAudio();   // 8032B774  (0x08)
  virtual void InitAudio() = 0;                                     // 0x10
  virtual void Shutdown() = 0;                                      // 0x18
  virtual void Update() = 0;                                        // 0x20
  virtual void Flush(bool) = 0;                                     // 0x28
  virtual void AddEvent(EA_EVENT, EMsgQueue&, unsigned int) = 0;    // 0x30
  virtual void RemoveEvent(EA_EVENT, EMsgQueue&, unsigned int) = 0; // 0x38
  virtual void PlayMusic(EPMDesc& desc) = 0;                        // 0x40
  virtual void StopMusic() = 0;                                     // 0x48
  virtual void PauseMusic() = 0;                                    // 0x50
  virtual void ResumeMusic() = 0;                                   // 0x58
  virtual void SetMusicVolume(float volume) = 0;                    // 0x60
  virtual float GetMusicVolume() = 0;                               // 0x68
  virtual void SetMusicPan(float pan) = 0;                          // 0x70
  virtual float GetMusicPan() = 0;                                  // 0x78
  virtual bool IsPlayingMusic() = 0;                                // 0x80
  virtual EVoice* AudioAllocVoice() = 0;                            // 0x88
  virtual void FreeVoice(EVoice* voice) = 0;                        // 0x90
  virtual void AudioBindVoice(EVoice* voice, unsigned int sampleId) = 0;  // 0x98
  virtual void UnbindVoice(EVoice* voice) = 0;                      // 0xA0
  virtual void GetVoiceState(EVoice* voice, EVoiceDesc& desc) = 0;  // 0xA8
  virtual void SetVoiceState(EVoice* voice, EVoiceDesc& desc) = 0;  // 0xB0
  virtual unsigned char GetIndexFromVoice(EVoice* voice) = 0;       // 0xB8
  virtual EVoice* GetVoiceFromIndex(unsigned char index) = 0;       // 0xC0

  // Adds `delta` to the music volume, clamped to 0..1; returns true when it had to clamp.
  bool AdjustMusicVolume(float delta);  // 8032CB84
};
