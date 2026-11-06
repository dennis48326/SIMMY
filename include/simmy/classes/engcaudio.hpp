// @class ENgcAudio after EAudio EMutex NGCStreamInfo
// @class NGCStreamInfo
// @class ENgcSFXStreamer
// @class ENGCAudioStreamer
// The platform audio (engine/e_ngcaudio.cpp, _ngcAudio = _pActualAudio): music streams from
// audiostr.arc through ENGCAudioStreamer, sample voices through ENgcSFXStreamer. On the GameCube both
// drove AX voices and MIX channels (e_ngcstream.cpp, e_ngcsamplestream.cpp); on PC they drive the
// mixer in platform/pc_audio.cpp, keeping their interfaces.
#pragma once

// One mono stream to play (0x44 bytes): where its DSP-ADPCM data sits in audiostr.arc and the
// decoder set-up from its 0x60-byte header. ENgcAudio keeps a table of 64.
struct NGCStreamInfo {
  enum : unsigned short { kLoop = 0x8000 };  // m_flags
  unsigned int m_id = 0xFFFFFFFFu;   // +0x00 audiostreams resource id, -1 = free slot
  int m_field04 = -1;                // +0x04
  unsigned int m_offset = 0;         // +0x08 data position in the archive (after the header)
  unsigned int m_size = 0;           // +0x0C data bytes (rounded down to 32)
  EFile* m_file = nullptr;           // +0x10 the archive
  short m_coef[16] = {};             // +0x14 ADPCM coefficients
  unsigned short m_gain = 0;         // +0x34
  unsigned short m_predScale = 0;    // +0x36
  short m_yn1 = 0, m_yn2 = 0;        // +0x38
  unsigned short m_sampleRate = 0;   // +0x3C low 16 bits of the header's rate
  unsigned short m_flags = 0;        // +0x3E
  unsigned int m_handle = 0xFFFFFFFFu;  // +0x40 ENGCAudioStreamer handle, -1 = not playing
};

// Sample playback (62 voices in the file's own table; the object itself is empty).
struct ENgcSFXStreamer {
  ENgcSFXStreamer();   // 8034C7B8
  ~ENgcSFXStreamer();  // 8034C858
  static void Update();  // 8034C880
  bool IsPlaying(unsigned int handle);  // 8034C92C
  int Allocate(ERSampledata* sample, unsigned char volume);  // 8034C984
  void Play(unsigned int handle, ERSampledata* sample);  // 8034CB00
  void Pause(unsigned int handle);  // 8034D058
  void Free(unsigned int handle);  // 8034D15C
  void SetPitchBend(unsigned int handle, float bend);  // 8034D274
  void SetLeftVolume(unsigned int handle, float volume);  // 8034D310
  void SetLeftVolume(unsigned int handle, int dB10);  // 8034D330
  void SetRightVolume(unsigned int handle, float volume);  // 8034D3A4
  void SetRightVolume(unsigned int handle, int dB10);  // 8034D43C
  void SetVolume(unsigned int handle, float volume);  // 8034D4B8
  void SetPan(unsigned int handle, float pan);  // 8034D54C
};

// Music streaming (two stream slots; the object itself is empty).
struct ENGCAudioStreamer {
  ENGCAudioStreamer();   // 8034E004
  ~ENGCAudioStreamer();  // 8034E0E8
  static void Update();  // 8034E174
  void Pause(unsigned int handle);  // 8034E248
  void Resume(unsigned int handle);  // 8034E2C8
  unsigned int Allocate(NGCStreamInfo* info, unsigned char volume, unsigned char pan);  // 8034E354
  void AllocateStereo(NGCStreamInfo* left, NGCStreamInfo* right, unsigned char volume);  // 8034E404
  bool Free(unsigned int handle);  // 8034E560
  bool IsActive(unsigned int handle);  // 8034E5E8
  static bool AnyIsActive();  // 8034E64C
  void SetVolume(unsigned int handle, unsigned char volume);  // 8034E6A4
  void SetPan(unsigned int handle, unsigned char pan);  // 8034E7D4
};

struct ENgcAudio : public EAudio {
  ENgcAudio();            // 8032B7A8
  ~ENgcAudio() override;  // 8032B818
  void InitAudio() override;  // 8032B8EC
  void AudioAlarmHandler(OSAlarm *, OSContext *);  // 8032B9F0
  void Shutdown() override;  // 8032B9F4
  void PlayMusic(EPMDesc& desc) override;  // 8032BA5C
  unsigned char GetIndexFromVoice(EVoice* voice) override;  // 8032BEDC
  EVoice* GetVoiceFromIndex(unsigned char index) override;  // 8032BEE4
  void Update() override;  // 8032BEEC
  void StopMusic() override;  // 8032C1FC
  void PauseMusic() override;  // 8032C2C8
  void ResumeMusic() override;  // 8032C338
  void SetMusicVolume(float volume) override;  // 8032C3A8
  float GetMusicVolume() override;  // 8032C468
  void SetMusicPan(float pan) override;  // 8032C470
  float GetMusicPan() override;  // 8032C534
  bool IsPlayingMusic() override;  // 8032C53C
  EVoice* AudioAllocVoice() override;  // 8032C5D4
  void FreeVoice(EVoice* voice) override;  // 8032C614
  void AudioBindVoice(EVoice* voice, unsigned int sampleId) override;  // 8032C65C
  void UnbindVoice(EVoice* voice) override;  // 8032C7F8
  void GetVoiceState(EVoice* voice, EVoiceDesc& desc) override;  // 8032C8E0
  void SetVoiceState(EVoice* voice, EVoiceDesc& desc) override;  // 8032C970
  static void SetSpeakerVolumes(EVoice* voice, int* dB10, unsigned int count);  // 8032CAA0
  bool IsPausedDueToDiscError();  // 8032CB0C
  void SetDiscErrorCallback(void (*callback)(bool));  // 8032CB14
  void Flush(bool) override;  // 8032CC1C
  void AddEvent(EA_EVENT, EMsgQueue&, unsigned int) override;  // 8032CC20
  void RemoveEvent(EA_EVENT, EMsgQueue&, unsigned int) override;  // 8032CC24

  float m_musicVolume = 1.0f;            // +0x04
  float m_musicPan = 0.5f;               // +0x08
  int m_musicPaused = 0;                 // +0x0C
  int m_pausedForDiscError = 0;          // +0x38
  void (*m_discErrorCallback)(bool) = nullptr;  // +0x3C
  EMutex m_streamMutex;                  // +0x40 guards the stream table
  EMutex m_voiceMutex;                   // +0x5C guards voice binding
};

void AudioFrameCallback();  // 8032C598
extern int gAudioEnabled;
extern ENgcAudio _ngcAudio;
