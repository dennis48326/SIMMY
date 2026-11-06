// @class ERAmbientSound after EVec3 EResource EResourceManager CTilePt
// @class ERAmbientScore after EResource ERAmbientSound
// @class EAmbientScoreManager after EResourceManager ERAmbientScore
// @class AmbientScorePlayer after ERAmbientScore EVec3
// @class AmbientSoundPlayer after AmbientScorePlayer EVec3
// The neighbourhood ambience (engine/e_rambientscore.cpp, e_ambientscoreman.cpp; game/ambientscoreplayer.cpp,
// ambientsoundplayer.cpp). An ambient score ('SCOT', ambientscores: "score<house number>" per lot) lists
// ambient sounds: each a set of samples played now and then (by probability weight) between two hours of
// the simulation's day, somewhere in a box, at a point or anywhere near the listener, optionally drifting
// (angle and speed) or following the listener along the box's edge (looping sounds). cBoxX keeps one
// AmbientScorePlayer (live mode); its listeners are the players' cameras (cBoxX::Update), and every sound's
// volume and pan come from cBoxX::GetSurroundVol(AmbientScorePlayer*) over those listeners.
#pragma once
#include <vector>

struct cIGZSnd;
struct AmbientSoundPlayer;

// 0x34 bytes. The box: x m_box[0]..m_box[3], y m_box[4]..m_box[1], z m_box[5]..m_box[2] (as stored).
struct ERAmbientSound {
  enum : unsigned char { kLooping = 1, kRoomCheck = 2, kOutsideBox = 4, kNoFade = 8 };  // m_flags
  enum : unsigned char { kPoint = 0, kAnywhere = 1, kBox = 2, kAnywhereNear = 3, kBox2 = 4 };  // m_type
  ERAmbientSound();  // 80366998
  ~ERAmbientSound();  // 80366A04
  void Load(EFile* file);  // 80366A70

  float m_box[6] = {};              // +0x00
  unsigned char m_volume = 0;       // +0x18 percent
  unsigned char m_flags = 0;        // +0x19
  unsigned char m_type = 0;         // +0x1A
  unsigned char m_weight = 0;       // +0x1B probability weight
  unsigned char m_range = 0;        // +0x1C hearing range (point: radius; anywhere: spread)
  unsigned char m_startHour = 0;    // +0x1D
  unsigned char m_endHour = 0;      // +0x1E
  unsigned char m_angle = 0;        // +0x1F drift direction (0..255 = 0..360 degrees)
  unsigned char m_angleSpread = 0;  // +0x20 degrees either side
  unsigned char m_speed = 0;        // +0x21 tenths of a tile per second
  std::vector<unsigned int> m_samples;  // +0x24 sample ids
};

// 0x3C bytes.
struct ERAmbientScore : public EResource {
  ERAmbientScore();  // 80366D50
  ~ERAmbientScore() override;  // 80366DC0
  void Load(EFile* file, unsigned int);  // 80366E90
  using EResource::Load;
  void Refresh(EFile* file) override;  // 803670D4
  SIMMY_STORABLE_DECL(ERAmbientScore)
  static void RegisterType(unsigned short);  // 8036726C
  void CreateCopy() const;  // 803672C0

  unsigned int m_version = 0;       // +0x14
  unsigned int m_signature = 0;     // +0x18 'SCOT'
  unsigned int m_variant = 0;       // +0x1C
  char* m_name = nullptr;           // +0x20 (vec_new)
  unsigned int m_size = 0;          // +0x24 the resource's size
  unsigned char m_minPlaying = 0;   // +0x28 below this many sounds one is always added
  unsigned char m_maxPlaying = 0;   // +0x29 sound players
  unsigned char m_volume = 0;       // +0x2A percent
  std::vector<ERAmbientSound> m_sounds;  // +0x2C
};

struct EAmbientScoreManager : public EResourceManager {
  EAmbientScoreManager() = default;
  EAHeap* GetHeap() override;  // 80322BA0
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;  // 80322BC0
  ERAmbientScore* AddRef(unsigned int id, EFile* file, int pos);  // 80322C84
  ERAmbientScore* AddRef(char* name, EFile* file, int pos);  // 80322CA4
  int CanCache(EResource* res) override;  // 80322CC4
  void AddRef(EResource* res) { EResourceManager::AddRef(res); }  // PC: the base overload (MSVC: no using-declaration beside same-signature AddRefs)
};

// 0x3C bytes.
struct AmbientScorePlayer {
  enum { kStopped = 0, kPlaying = 1, kPaused = 2 };
  // 0x1C bytes: a listener (camera) position, its direction and its room (-1 without a world).
  struct ListenerInfo {
    EVec3 m_pos;      // +0x00
    EVec3 m_dir;      // +0x0C
    int m_room = 0;   // +0x18
  };
  explicit AmbientScorePlayer(bool flag);  // 800A80B8
  ~AmbientScorePlayer();  // 800A813C
  void Update();  // 800A81D8
  void SetListenerCount(int count);  // 800A8524
  void UpdateListener(unsigned int index, EVec3& pos, EVec3& dir);  // 800A85A4
  void SetTime(float hours);  // 800A8764
  double GetTime();  // 800A876C
  int Load(char* name);  // 800A8774
  int Start();  // 800A88E0
  bool Stop();  // 800A891C
  int Shutdown();  // 800A8954
  int SetPause(bool pause);  // 800A8A18
  int SetVolume(int volume);  // 800A8B10
  int GetVolume();  // 800A8B1C
  ERAmbientScore* GetAmbientScore();  // 800A8B24
  int GetNumPlaying();  // 800A8B2C
  std::vector<ListenerInfo>& GetListenerInfos();  // 800A8B34
  int SoundRangeCoversCurTime(ERAmbientSound* sound);  // 800A8B3C
  int SoundAlreadyPlaying(ERAmbientSound* sound);  // 800A8C28
  int SoundInHearingDistance(ERAmbientSound* sound);  // 800A8CB0
  AmbientSoundPlayer* AddSoundBasedOnProbability(std::vector<ERAmbientSound*>& sounds, int total, bool force);  // 800A8ED8
  int KillAllAmbientSounds();  // 800A8F90
  AmbientSoundPlayer* AddNewAmbientSoundPlayer(ERAmbientSound* sound);  // 800A9048
  int RemoveAmbientSoundPlayer(ERAmbientSound* sound);  // 800A90DC
  void RemoveFinishedSounds();  // 800A9198

  static int s_duckAmbient;  // 804FDBA0 an event that ducks the ambience plays (cSoundCache::UpdateDuckingPriorities)

  int m_state = kStopped;                   // +0x00
  int m_field4 = 0;                         // +0x04
  ERAmbientScore* m_score = nullptr;        // +0x08 (one reference)
  AmbientSoundPlayer* m_players = nullptr;  // +0x0C m_playerCount (vec_new)
  int m_numPlaying = 0;                     // +0x10
  int m_volume = 0x400;                     // +0x14
  int m_minPlaying = 0;                     // +0x18 the score's
  int m_playerCount = 2;                    // +0x1C the score's
  std::vector<ListenerInfo> m_listeners;    // +0x20
  bool m_flag = false;                      // +0x30 (the constructor's argument; unused)
  float m_time = 0.0f;                      // +0x34 the simulation's hour (with minutes)
  unsigned int m_lastUpdate = 0;            // +0x38 timeGetTime (ms)
};

// 0x30 bytes: one playing ambient sound.
struct AmbientSoundPlayer {
  AmbientSoundPlayer();  // 800A9270
  ~AmbientSoundPlayer();  // 800A92BC
  void SetScorePlayer(AmbientScorePlayer* score);  // 800A9330
  void SetAmbientSound(ERAmbientSound* sound);  // 800A9338
  void Update();  // 800A9340
  int Start();  // 800A9374
  int Stop();  // 800A9738
  int Pause();  // 800A97B4
  int UnPause();  // 800A97F8
  void SetInitialSoundPosition();  // 800A983C
  void UpdateSoundPosition();  // 800AA050
  void UpdateLoopingSoundPosition();  // 800AA0A4
  void UpdatePanAndVolume();  // 800AA5B8
  void UpdateSoundVolume();  // 800AA904
  ERAmbientSound* GetAmbientSound();  // 800AA9B4
  int IsFinished();  // 800AA9BC
  int IsValid();  // 800AAA18

  AmbientScorePlayer* m_scorePlayer = nullptr;  // +0x00
  ERAmbientSound* m_sound = nullptr;            // +0x04 (null: free)
  cIGZSnd* m_snd = nullptr;                     // +0x08
  int m_volume = 0;                             // +0x0C
  int m_pan = 0;                                // +0x10
  EVec3 m_pos{0.0f, 0.0f, 0.0f};                // +0x14
  float m_vel[2] = {};                          // +0x20 x, y per second
  unsigned int m_sample = 0;                    // +0x28 the chosen sample
  int m_room = -1;                              // +0x2C (-1: no room check)
};
