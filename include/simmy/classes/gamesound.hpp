// @class SoundEventInfo
// @class cSoundPlayer after SoundEventInfo
// @class cHitTimer
// @class cSoundModeManager
// @class cBoxX after cSoundModeManager cHitTimer
// @class cIGZSndSys
// @class cSoundCacheItem
// @class cIGZSnd
// @class cGZSnd after cIGZSnd
// @class cGZMusic after cIGZSnd
// @class cGZSndSys after cIGZSndSys cSoundCacheItem cGZSnd cGZMusic
// @class TrackDataReader
// @class cHitControlGroup
// @class cSoundCache
// @class cHitMan after cHitControlGroup cHitTimer cSoundCache
// @class SndEvtHitPatch
// @class cSamplePatch after SndEvtHitPatch
// @class cSampleChannel after cSamplePatch
// @class cTrack after TrackDataReader SndEvtHitPatch
// @class cTrackCacheHandle
// @class cAudioWorldCoord
// @class cAudioInfo after cAudioWorldCoord
// The game's sound director, "BoxX" (game/gamesound.cpp, boxx.cpp, gzsndsys.cpp, patch.cpp, audioinfo.cpp).
//
// The game plays *sound events* (ERSoundEvent, soundeve.arc) for a source object: cSoundPlayer::PlayBySource
// queues the event, cBoxX::Event plays it as a cTrack, a tiny program (ERSoundTrackData) that picks a hit
// patch (a sample or a music stream) from the event's lists and plays it through cSamplePatch/cSampleChannel
// on a cGZSnd (an EAudio voice) or a cGZMusic (EAudio's music stream). cHitMan schedules the running tracks
// (one tick per frame), groups their volumes (1 SFX, 2 music, 3 VOX/Simlish) and ducks them; cSoundCache
// keeps one cTrack per event (64). Positional sounds take their volume and pan from every player's camera
// (cBoxX::GetSurroundVol: two listeners in split screen, summed). cSoundModeManager plays the front-end,
// CAS and build/buy music and fades music and effects with the game mode (1 live, 2 front end, 3 loading,
// 4 CAS, 5 silent, 6 build/buy).
//
// Volumes are 0..0x400 (1.0), pans 0..0x400 (0x200 centre) throughout. GC containers (STLport lists, maps
// and multimaps with node pools) are std containers here; vtable calls through EAudio (_pActualAudio) are
// ordinary virtual calls.
#pragma once
#include <cstdint>
#include <list>
#include <map>
#include <vector>

// ---------------------------------------------------------------------------------------------------
// cSoundPlayer (game/gamesound.cpp, Globs::pSound)

// A queued event (list node 0x14: next, prev, +8 source, +0xC event id, +0x10 time).
struct SoundEventInfo {
  int m_source = 0;            // +0x08 the source object id (0: none)
  unsigned int m_event = 0;    // +0x0C soundevents id (one AddRefAsync held)
  float m_time = 0.0f;         // +0x10 the clock when it was queued
};

// 0x20 bytes.
struct cSoundPlayer {
  // The game modes (GC: snd::eMode).
  enum Mode { kLive = 1, kFrontEnd = 2, kLoading = 3, kCAS = 4, kSilent = 5, kBuildBuy = 6 };
  cSoundPlayer();  // 800B7264
  ~cSoundPlayer();  // 800B72CC
  void Initialize();  // 800B7368
  void Shutdown();  // 800B7440
  void KillSourceEvent(int source);  // 800B74C4
  void KillAllEvent();  // 800B75BC
  void Update();  // 800B76A8
  void SetGameMode(int mode);  // 800B7814 (the GC's snd::eMode)
  static int GetGameMode();  // 800B7898
  void EnableSound(bool on);  // 800B78B0
  void QuietAll();  // 800B7904
  void PlayBySource(unsigned int event, short source);  // 800B7968
  void PlayBySource(char* event, short source);  // 800B7A60
  void PlayBySource(ERSoundEvent* event, short source);  // 800B7ACC
  void QuietBySourceID(int source);  // 800B7B20
  void PauseSFX();  // 800B7B74
  void ResumeSFX();  // 800B7BE4
  void PauseMusic();  // 800B7C54
  void ResumeMusic();  // 800B7CC4
  void PauseSounds();  // 800B7D34
  void ResumeSounds();  // 800B7DC8
  void NotifyViewChange();  // 800B7E5C
  void NotifyHourChange();  // 800B7EB0
  static int GetSFXVolume();  // 800B7F04
  static int GetMusicVolume();  // 800B7F18
  static int GetVOXVolume();  // 800B7F2C
  static int GetAmbientVolume();  // 800B7F40
  void SetOldFXVolume(int volume);  // 800B7F54 (0..10, the options' scale)
  void SetMusicVolume(int volume);  // 800B7FDC
  void SetSFXVolume(int volume);  // 800B803C
  void SetVOXVolume(int volume);  // 800B809C
  void SetAmbientVolume(int volume);  // 800B80FC

  int m_initialised = 0;               // +0x00
  cIGZSndSys* m_sndSys = nullptr;      // +0x04 (g_pSndSys)
  int m_ownsBoxX = 0;                  // +0x08
  int m_paused = 0;                    // +0x0C PauseSounds: no BoxX/sound-system update
  std::list<SoundEventInfo>::iterator m_cursor;  // +0x10 Update's position (the kills stop there)
  int m_killedAll = 0;                 // +0x14
  std::list<SoundEventInfo> m_events;  // +0x18 (+0x1C the list head)
};

extern cSoundPlayer* Globs_pSound;  // Globs::pSound
void DiscErrorCallback(bool error);  // 800B720C
// PC: what the state machines did inline when leaving a screen for a load: fade the music and effects
// out over 32 frames (cSoundModeManager::FadeOutMusic/FadeOutSFX(0x20)), then set the game mode.
void SoundFadeOutAndSetMode(int mode);

// ---------------------------------------------------------------------------------------------------
// cBoxX, cSoundModeManager (game/boxx.cpp)

// A frame counter (linkonce; 8 bytes).
struct cHitTimer {
  int Update() {  // 803A2D94
    if (m_running) ++m_count;
    return 1;
  }
  int Stop() {  // 803A2D84
    m_running = 0;
    return 1;
  }
  int m_running = 0;  // +0x00
  int m_count = 0;    // +0x04
};

// cBoxX +0x30 (0x34 bytes).
struct cSoundModeManager {
  cSoundModeManager();  // 800B0320
  static void Shutdown();  // 800B0364
  void Pause();  // 800B039C
  static void Unpause();  // 800B0400
  void FadeOutMusic(unsigned int frames);  // 800B0458
  void FadeInMusic(unsigned int frames);  // 800B0468
  void FadeOutSFX(unsigned int frames);  // 800B0478
  void FadeInSFX(unsigned int frames);  // 800B0488
  void SetMode(int mode);  // 800B0498
  void UpdateMusicVolume();  // 800B055C
  void UpdateSFXVolume();  // 800B08AC
  void UpdateLoadMusic();  // 800B0B00
  const char* GetFrontEndMusicTrack(bool current);  // 800B0E44
  void NextFrontEndTrack();  // 800B0E70
  void PrevFrontEndTrack();  // 800B0E90
  void StartFrontEndMusic(bool current);  // 800B0EB0
  const char* GetDemoMusicTrack();  // 800B0EF0
  void NextDemoMusicTrack();  // 800B0F08
  void StartDemoMusic();  // 800B0F28
  void StartMusicTrack(char* name, float volume);  // 800B0F70
  void StartBuildBuyMusic();  // 800B104C
  void StartCASMusic();  // 800B10F0
  void Update();  // 800B1194

  int m_mode = 0;                  // +0x00 the GC's snd::eMode
  int m_musicFadingOut = 0;        // +0x04
  int m_musicFadingIn = 0;         // +0x08
  unsigned int m_musicFadeStep = 0;  // +0x0C (0x400 units per frame)
  int m_sfxFadingOut = 0;          // +0x10
  int m_sfxFadingIn = 0;           // +0x14
  int m_sfxFadeStep = 0;           // +0x18
  EVoice* m_loadVoice = nullptr;   // +0x1C the loading screen's loop
  ERSampledata* m_loadSample = nullptr;  // +0x20
  unsigned int m_loadSampleId = 0;       // +0x24 requested (AddRefAsync)
  int m_frontEndTrack = 0;         // +0x28
  int m_demoTrack = 1;             // +0x2C
  int m_bbcasTrack = 0;            // +0x30 CAS / build-buy (1..2, cycling)
};

extern const char* s_frontEndTracks[4];  // 8042401C
extern int g_demoMusic;                  // 804FDBB0

// 0x7C bytes (g_pBoxX). +0x78 the vtable.
struct cBoxX {
  cBoxX();  // 800AD670
  virtual ~cBoxX();  // 800AD724
  int Init();  // 800AD7D8
  static void LoadQdataTables();  // 800AD898
  int Shutdown();  // 800AD89C
  virtual void Update(unsigned int);  // 800AD950 (vtable 0x14)
  void UpdateGameMode();  // 800ADB18
  static int GetSurroundVol(AmbientScorePlayer* score, EVec3& pos, float minRange, float maxRange, int& left,
                     int& right, int& front, int& back);  // 800ADB48
  int GetSurroundVol(EVec3& pos, float minRange, float maxRange, int& left, int& right, int& front, int& back,
                     int source);  // 800ADEA0
  static int CalculateSurroundVol(EVec3& ear, EVec3& dir, EVec3& pos, float minRange, float maxRange, int& left,
                                  int& right, int& front, int& back);  // 800AE1D4
  int GetInstanceSurroundVol(int source, cTrack* track, int& left, int& right, int& front, int& back);  // 800AE4BC
  int GetInstanceVolNoPan(int source, cTrack* track, int& volume);  // 800AE6A8
  static void ConvertSurroundToVolPan(int left, int right, int front, int back, int& volume, int& pan);  // 800AE948
  int MappedEvent(ERSoundEvent* event, int source, float time);  // 800AEADC
  int Event(int msg, intptr_t param, int source, ERSoundEvent* event, float time);  // 800AEB0C
  void SetAmbientScore(int oldMode);  // 800AF4B4
  void SetAmbientScoreVolume(int volume);  // 800AF5BC
  int GetAmbientScoreVolume();  // 800AF5E8
  std::multimap<ERSoundEvent*, int>::iterator FindSndobInstancePair(ERSoundEvent* event, int source);  // 800AF61C
  void AddToInstanceMap(ERSoundEvent* event, int source);  // 800AF72C
  void UpdateAllSndobVolPan();  // 800AF7A4
  static void CheckForTooManySounds();  // 800AF8D8
  void KillSource(int source);  // 800AF9F8
  void UpdateSndobVolPan(ERSoundEvent* event);  // 800AFB00
  void UpdateSndobVolPan(std::multimap<ERSoundEvent*, int>::iterator first,
                         std::multimap<ERSoundEvent*, int>::iterator last);  // 800AFBD8
  int GetSndobSurroundVols(ERSoundEvent* event, int& left, int& right, int& front, int& back);  // 800AFCC4
  void Pause();  // 800AFEE8
  void Unpause();  // 800AFF24
  void PauseSFX();  // 800AFF60
  void UnpauseSFX();  // 800AFFC0
  void PauseAmbient();  // 800B0028
  void UnpauseAmbient();  // 800B0068
  void PauseMusic();  // 800B00B4
  void UnpauseMusic();  // 800B0100
  static void GetListenerPosAndDir(ESimsCam* cam, EVec3& pos, EVec3& dir);  // 800B015C

  int m_enabled = 1;                                 // +0x04 EnableSound
  int m_field08 = 0, m_field0C = 0;                  // +0x08
  cHitTimer* m_timer = nullptr;                      // +0x10
  int m_quietTime = 0;                               // +0x14 the timer at the last QuietAll
  int m_field18 = 0;                                 // +0x18
  std::multimap<ERSoundEvent*, int> m_instances;     // +0x20 playing positional events and their sources
  cSoundModeManager m_modes;                         // +0x30
  AmbientScorePlayer* m_score = nullptr;             // +0x64
  EResource* m_qdata = nullptr;                      // +0x68 quickdata 0x0C33DB41 (the sound tables)
  int m_sfxPaused = 0;                               // +0x6C
  int m_musicPaused = 0;                             // +0x70
  int m_ambientPaused = 0;                           // +0x74
};

extern cBoxX* g_pBoxX;
extern int g_bBoxXIsInitted;
extern float g_social_duck;  // 804FDBB4 0.45
extern float g_cam_height;   // 804FDBB8 2
extern float g_cam_dist;     // 804FDBBC 2
bool BoxxGlobalGetSourceParamValue(int source, int field, int* value);  // 800AD628

// ---------------------------------------------------------------------------------------------------
// cGZSndSys, cGZSnd, cGZMusic (game/gzsndsys.cpp)

struct cIGZSndSys {
  static cIGZSndSys* CreateInstance();  // 800B8C88
  virtual ~cIGZSndSys();  // 800B8E4C
  virtual int Initialize() = 0;                                                // vtable 0x14
  virtual void Update() = 0;                                                   // 0x1C
  virtual cIGZSnd* CreateSoundEffect(unsigned int sample, float time) = 0;    // 0x24
  virtual cIGZSnd* CreateAudioStream(unsigned int stream, bool loop, float time) = 0;  // 0x2C
};

// A sample in the sound system's cache (0x1C bytes): loaded asynchronously, at most 4 at a time.
struct cSoundCacheItem {
  explicit cSoundCacheItem(unsigned int id);  // 800B8CB8
  void ResetUsageTimer();  // 800B8CC0
  void Update();  // 800B8CF0
  ~cSoundCacheItem();  // 800B8DF0

  unsigned int m_id = 0;      // +0x00 samples id
  int m_users = 0;            // +0x04 cGZSnds using it
  unsigned int m_lastUsed = 0;  // +0x08 timeGetTime (ms)
  int m_waiting = 0;          // +0x0C AddRefAsync issued
  int m_loaded = 0;           // +0x10 one reference held
  int m_retry = 0;            // +0x14 frames before the next attempt (PC: -16, it cannot load)
  cSoundCacheItem* m_next = nullptr;  // +0x18 bucket chain
};

// PC: the interface cGZSnd and cGZMusic share (GC: two classes with the same vtable layout, called through
// it by cSamplePatch/cSampleChannel without knowing which one they hold). Slots: 0x0C Init, 0x14 AddRef,
// 0x1C Release, 0x24 Play, 0x2C IsPlaying, 0x34 Stop, 0x3C Pause, 0x44 Unpause, 0x4C Load, 0x54 Unload,
// 0x5C GetVolume, 0x64 SetVolume, 0x6C FadeVolume, 0x74 GetPan, 0x7C SetPan, 0x84 GetFrequency,
// 0x8C SetFrequency, 0x94 SetPosition, 0x9C the destructor.
struct cIGZSnd {
  virtual ~cIGZSnd() = default;
  virtual int Init() = 0;
  virtual void AddRef() = 0;
  virtual int Release() = 0;
  virtual int Play() = 0;
  virtual int IsPlaying() = 0;
  virtual int Stop() = 0;
  virtual int Pause() = 0;
  virtual int Unpause() = 0;
  virtual int Load() = 0;
  virtual int Unload() = 0;
  virtual int GetVolume() = 0;
  virtual void SetVolume(int volume) = 0;
  virtual int FadeVolume(int from, int to, unsigned int ms) = 0;
  virtual int GetPan() = 0;
  virtual int SetPan(int pan) = 0;
  virtual int GetFrequency() = 0;
  virtual int SetFrequency(int) = 0;
  virtual int SetPosition(unsigned int) = 0;
};

// A sample voice (0x40 bytes).
struct cGZSnd : public cIGZSnd {
  cGZSnd();  // 800B9FB8
  ~cGZSnd() override;  // 800BA034 (vtable 0x9C)
  int Init() override;  // 800BA090
  void AddRef() override;  // 800BA154
  int Release() override;  // 800BA098
  int Play() override;  // 800BA168
  int IsPlaying() override;  // 800BA3F8
  int Stop() override;  // 800BA498
  int Pause() override;  // 800BA558
  int Unpause() override;  // 800BA5D8
  int Load() override;  // 800BA658
  int Unload() override;  // 800BA660
  int GetVolume() override;  // 800BA668
  void SetVolume(int volume) override;  // 800BA670
  int FadeVolume(int from, int to, unsigned int ms) override;  // 800BA79C
  int GetPan() override;  // 800BA81C
  int SetPan(int pan) override;  // 800BA824
  int GetFrequency() override;  // 800BA8DC
  int SetFrequency(int) override;  // 800BA8E4
  int SetPosition(unsigned int) override;  // 800BA8EC
  int setVolume(int volume);  // 800BA6B0
  void reset();  // 800BA8F4
  void getLRVolume(int volume, float& left, float& right);  // 800BA96C

  int m_refs = 1;                  // +0x04
  int m_waitingToPlay = 0;         // +0x08 Play waits for the sample
  int m_playRequested = 0;         // +0x0C
  int m_paused = 0;                // +0x10
  unsigned int m_sample = 0;       // +0x14 samples id
  cSoundCacheItem* m_item = nullptr;  // +0x18
  EVoice* m_voice = nullptr;       // +0x1C
  int m_volume = 0x400;            // +0x20
  int m_pan = 0x200;               // +0x24
  float m_lastLeft = 1.0f;         // +0x28 the signs of the last gains
  float m_lastRight = 1.0f;        // +0x2C
  float m_startTime = 0.0f;        // +0x30 not before this clock time
  int m_fadeFrames = 0;            // +0x34
  int m_currentVolume = 0x400;     // +0x38
  cGZSnd* m_next = nullptr;        // +0x3C
};

// A music stream (0x28 bytes): only one plays at a time (cGZSndSys +0x40C).
struct cGZMusic : public cIGZSnd {
  cGZMusic();  // 800BAB28
  ~cGZMusic() override;  // 800BAB9C
  int Init() override;  // 800BABF8
  void AddRef() override;  // 800BAC00
  int Release() override;  // 800BAC14
  int Play() override;  // 800BAC94
  int IsPlaying() override;  // 800BADA4
  int Stop() override;  // 800BADAC
  int Pause() override;  // 800BADFC
  int Unpause() override;  // 800BAE54
  int Load() override;  // 800BAEAC
  int Unload() override;  // 800BAEB4
  int GetVolume() override;  // 800BAEBC
  void SetVolume(int volume) override;  // 800BAEC4
  int FadeVolume(int from, int to, unsigned int ms) override;  // 800BB024
  int GetPan() override;  // 800BB0D0
  int SetPan(int pan) override;  // 800BB0D8
  int GetFrequency() override;  // 800BB184
  int SetFrequency(int) override;  // 800BB18C
  int SetPosition(unsigned int) override;  // 800BB194
  int setVolume(int volume);  // 800BAEF8
  void reset();  // 800BB19C
  double getLRVolume();  // 800BB1F4
  double getPanSetting();  // 800BB234

  int m_refs = 1;               // +0x04
  int m_playing = 0;            // +0x08
  int m_loop = 0;               // +0x0C
  unsigned int m_stream = 0;    // +0x10 audiostreams id
  int m_volume = 0x400;         // +0x14
  int m_pan = 0x200;            // +0x18
  float m_startTime = 0.0f;     // +0x1C
  int m_fadeFrames = 1;         // +0x20 (1: the music volume ramps in)
  cGZMusic* m_next = nullptr;   // +0x24
};

// 0x41C bytes (_gzSndSys, g_pSndSys).
struct cGZSndSys : public cIGZSndSys {
  cGZSndSys();  // 800B8E80
  ~cGZSndSys() override;  // 800B8EE8
  int Initialize() override;  // 800B8FBC
  cIGZSnd* CreateSoundEffect(unsigned int sample, float time) override;  // 800B8FE0
  cIGZSnd* CreateAudioStream(unsigned int stream, bool loop, float time) override;  // 800B9038
  void addToList(cGZSnd* snd);  // 800B9098
  void removeFromList(cGZSnd* snd);  // 800B90A8
  void addToList(cGZMusic* music);  // 800B90E8
  void removeFromList(cGZMusic* music);  // 800B90F8
  static void FreeUnusedSounds();  // 800B914C
  void killDeadSounds();  // 800B9184
  int ProcessCache(unsigned int idleMs);  // 800B93A8
  void Update() override;  // 800B9824
  static void preloadMusic();  // 800B9FB0
  static void unloadMusic();  // 800B9FB4

  cSoundCacheItem* m_buckets[0x100] = {};  // +0x004 by the id's low byte
  cGZSnd* m_sounds = nullptr;              // +0x404
  cGZMusic* m_musics = nullptr;            // +0x408
  cGZMusic* m_currentMusic = nullptr;      // +0x40C
  int m_musicFadeOut = 1;                  // +0x410 frames left of the stop fade
  int m_musicStopping = 0;                 // +0x414
  int m_musicPaused = 0;                   // +0x418
};

extern cGZSndSys* _gzSndSys;
extern cIGZSndSys* g_pSndSys;
extern int _iNumWaitingOnLoad;

// ---------------------------------------------------------------------------------------------------
// The track VM (game/patch.cpp)

// A position in a track program (8 bytes).
struct TrackDataReader {
  TrackDataReader();  // 801162E0
  ~TrackDataReader();  // 801162F4
  void operator=(TrackDataReader& other);  // 80116324
  void operator=(ERSoundTrackData* program);  // 8011633C
  void operator--();  // 80116350
  void operator+=(int n);  // 80116364
  const unsigned char* ReadCommand();  // 80116378 (4 bytes: opcode, A, B, C / imm16)
  bool IsValid() const;  // 80116424
  void JumpToEnd();  // 8011643C
  void Trace() const;  // 8011645C

  ERSoundTrackData* m_program = nullptr;  // +0x00
  unsigned int m_pc = 0;                  // +0x04 instruction index
};

// A volume group (8 bytes): 1 SFX, 2 music, 3 VOX.
struct cHitControlGroup {
  explicit cHitControlGroup(int id);  // 801167B4
  int SetVolume(int volume);  // 801167C8
  int m_id = 0;          // +0x00
  int m_volume = 0x400;  // +0x04
};

// The track slots (0x104 bytes): one cTrack per sound event.
struct cSoundCache {
  cSoundCache();  // 80119470
  ~cSoundCache();  // 801194B0
  void onTrackDelete(cTrack* track);  // 801194F0
  void Shutdown();  // 80119530
  void CleanupIdleTracks();  // 8011959C
  void UpdateDuckingPriorities();  // 801199FC
  void KillAll();  // 80119AD0
  void PauseGroup(int group);  // 80119B18 (-1: all)
  void UnpauseGroup(int group);  // 80119B9C
  cTrack* GetTrackObject(ERSoundEvent* event, bool create);  // 80119C20
  bool IsInMemory(cTrack* track) const;  // 80119D48
  bool IsInMemory(ERSoundEvent* event) const;  // 80119D6C
  static void printTrackList();  // 80119DA4

  cTrack* m_tracks[0x40] = {};  // +0x000
  int m_next = 0;               // +0x100 round-robin slot
};

// The track scheduler (0x1080 bytes, g_pHitMan).
struct cHitMan {
  int RegisterSourceDataRequestHandler(bool (*handler)(int, int, int*));  // 801164A4
  int GetSourceDataField(int source, int field, int* value);  // 801164B0
  cHitControlGroup* ControlGroup(int id);  // 801164E4
  void ClearControlGroups();  // 80116650
  cHitMan();  // 80116888
  int Init();  // 80116994
  ~cHitMan();  // 80116A14
  int Shutdown();  // 80116B4C
  void UpdateActiveTrackVolumes();  // 80116D00
  void SetSequenceGroupTrackId(int group, ERSoundEvent* event);  // 80116DC4
  ERSoundEvent* SequenceGroupTrackId(int group);  // 80116DD8
  static int StartCallbackTimer();  // 80116DEC
  static int StopCallbackTimer();  // 80116DF4
  void TimerCallback();  // 80116DFC
  int AddToUpdateList(cTrack* track);  // 80116EE8
  int RemoveFromUpdateList(cTrack* track);  // 80116F50
  void DuckMapSetSndobPri(cTrack* track, int priority);  // 80117034
  int DuckMapSndobPri(cTrack* track);  // 801172C4
  int DuckMapMaxPri();  // 80117418
  void DuckMapRemoveAll();  // 801174A8

  cIGZSndSys* m_sndSys = nullptr;              // +0x00
  std::list<cTrack*> m_updateList;             // +0x04 the running tracks
  std::list<cTrack*>::iterator m_updateNext;   // +0x0C TimerCallback's position
  cHitTimer m_timer;                           // +0x10 (+0x14 the tick count)
  int m_field18 = 0;                           // +0x18
  int m_timerOn = 0;                           // +0x1C
  std::map<int, cHitControlGroup*> m_groups;   // +0x20
  bool (*m_sourceData)(int, int, int*) = nullptr;  // +0x30
  std::list<unsigned int> m_list34;            // +0x34
  int m_callbacks = 0;                         // +0x3C TimerCallback calls
  std::map<cTrack*, int> m_duck;               // +0x40 duck priorities
  cSoundCache* m_cache = nullptr;              // +0x50
  ERSoundEvent* m_sequenceGroups[0x40B] = {};  // +0x54 groups -10..0x400
};

extern cHitMan* g_pHitMan;

// A hit patch of an event's lists (8 bytes): a sample (m_sample != 0) or a music stream (m_stream).
struct SndEvtHitPatch {
  unsigned int m_stream = 0;  // +0x00
  unsigned int m_sample = 0;  // +0x04
};

// 0x1C bytes.
struct cSamplePatch {
  cSamplePatch(SndEvtHitPatch* patch, float time);  // 80118E58
  ~cSamplePatch();  // 80118E84
  void SetVolume(int volume);  // 80118EC4
  void SetPan(int pan);  // 80118EE8
  void SetLooped(int loop);  // 80118F0C
  int IsPlaying();  // 80118F14
  int CreateSnd();  // 80118F5C
  int FreeSnd();  // 80118FF8
  cSampleChannel* CreateChannel();  // 80119050

  cSampleChannel* m_channel = nullptr;  // +0x04
  unsigned int m_sample = 0;            // +0x08
  unsigned int m_stream = 0;            // +0x0C
  int m_loop = 0;                       // +0x10 the event's loop byte (2: loop the stream)
  float m_time = 0.0f;                  // +0x14
  cIGZSnd* m_snd = nullptr;             // +0x18 a cGZSnd, or a cGZMusic for a stream patch
};

// 8 bytes.
struct cSampleChannel {
  explicit cSampleChannel(cSamplePatch* patch);  // 80119088
  ~cSampleChannel();  // 801190C4
  int SetVolume(int volume);  // 80119110
  int Shutdown();  // 80119158
  int SetPatch(cSamplePatch* patch);  // 8011917C
  void SetPan(int pan);  // 80119218
  cIGZSnd* Snd(int);  // 80119278
  void NoteOn();  // 80119284
  int NoteOff();  // 801192E8
  int IsPlaying();  // 80119340
  int Pause();  // 80119388
  int Unpause();  // 801193D8

  cSamplePatch* m_patch = nullptr;  // +0x00
  cIGZSnd* m_snd = nullptr;         // +0x04 (one reference)
};

// One sound event's player (0x98 bytes; vtable at +0x94).
struct cTrack {
  enum Reg {
    kSource = 1, kVolumeArg = 2, kPanArg = 3, kVolume = 9, kPan = 10, kPatch = 11, kFade = 12, kKillAfterFade = 13,
    kFadeReg = 14, kFadeTarget = 15, kFadeStep = 16, kDuckPri = 17
  };
  void HandleTrackFlowError(const char* message);  // 80116460
  explicit cTrack(ERSoundEvent* event);  // 801175E0
  virtual ~cTrack();  // 801176EC
  int OnStartPlaying();  // 801177B0
  bool OnEndPlaying();  // 8011784C
  ERSoundTrackData* StartPos();  // 801178A0
  void HandleTimerCallback();  // 801178AC
  int PlayPause(float time, int source, int volume, int pan);  // 80117A60
  int Pause();  // 80117C1C
  int Unpause();  // 80117C78
  int Stop();  // 80117CDC
  int Kill();  // 80117D84
  intptr_t RegisterVal(int reg);  // 80117E2C
  int SetRegister(int reg, intptr_t value);  // 80117EFC
  intptr_t GetVHitList(intptr_t index);  // 80118074
  int DoCommand();  // 801180A4
  void SetCompareFlags(intptr_t a, intptr_t b);  // 80118A64
  int NoteOn();  // 80118A90
  int SetPatch(SndEvtHitPatch* patch, float time);  // 80118B0C
  int NoteOff();  // 80118BF0
  void SetVolume(int volume);  // 80118C24
  void SetPan(int pan);  // 80118C4C
  int CalculateCurrentVolume();  // 80118C74
  int UpdateVolPan();  // 80118DD4
  int GetMinRange();  // 80118E28
  int GetMaxRange();  // 80118E34
  int GetControlGroup();  // 80118E40
  int GetArgsType();  // 80118E4C
  int SetInstanceId(int id);  // 80119DB4
  int InstanceId();  // 80119DC0
  int IsPlaying();  // 80119DC8
  int IsPaused();  // 80119DD0
  int CheckedRegId(int reg);  // 80119DE8

  static int s_maxSfxDuckPri, s_maxMusicDuckPri, s_maxVoxDuckPri, s_superDuckEnabled;
  static int s_activeDuckPri;

  ERSoundEvent* m_event = nullptr;          // +0x00 (one reference)
  int m_instance = 0;                       // +0x04 the source (args types 1, 2)
  int m_field08 = 0;                        // +0x08
  cSamplePatch* m_patch = nullptr;          // +0x0C
  const std::vector<SndEvtHitPatch>* m_hitList = nullptr;  // +0x10
  int m_field14 = 0;                        // +0x14
  unsigned int m_nextTime = 0;              // +0x18 the tick of the next instruction (0: none)
  TrackDataReader m_reader;                 // +0x1C
  TrackDataReader m_mark;                   // +0x24 loop mark
  TrackDataReader m_last;                   // +0x2C the instruction being run
  int m_playing = 0;                        // +0x34
  int m_paused = 0;                         // +0x38
  int m_equal = 0, m_greater = 0, m_less = 0;  // +0x3C, +0x40, +0x44 compare flags
  int m_lastChoice = -1;                    // +0x48
  float m_time = 0.0f;                      // +0x4C
  cSampleChannel* m_channel = nullptr;      // +0x50
  intptr_t m_regs[8] = {};                  // +0x54 registers 1..8
  int m_volume = 0x400;                     // +0x74 register 9
  int m_pan = 0x200;                        // +0x78 register 10
  intptr_t m_patchReg = 0;                  // +0x7C register 11 (a SndEvtHitPatch*)
  int m_fading = 0;                         // +0x80 register 12
  int m_killAfterFade = 0;                  // +0x84 13
  int m_fadeReg = 0;                        // +0x88 14
  int m_fadeTarget = 0;                     // +0x8C 15
  int m_fadeStep = 0;                       // +0x90 16
};

// 4 bytes.
struct cTrackCacheHandle {
  cTrack* GetTrackObject();  // 80119428
  explicit cTrackCacheHandle(ERSoundEvent* event);  // 80119468
  ERSoundEvent* m_event = nullptr;
};

// ---------------------------------------------------------------------------------------------------
// cAudioInfo (game/audioinfo.cpp): what the director asks the simulation.

struct cAudioWorldCoord {
  float x = 0.0f, y = 0.0f;  // tiles
  int level = 1;
};

struct cAudioInfo {
  static int CurrentSimSpeed();  // 800AAC34
  static bool GetObjectPosition(int id, cAudioWorldCoord& out);  // 800AAC6C
  static int GetObjectData(int id, int field);  // 800AAD5C (cAudioInfo::DataIdx)
  static int OutdoorTileRatio();  // 800AB120
};
