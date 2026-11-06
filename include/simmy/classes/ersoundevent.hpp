// @class ERSoundTrackData after EResource
// @class ERSoundEvent after EResource ERSoundTrackData SndEvtHitPatch
// @class ESoundEventManager after EResourceManager
// @class ESoundTrackDataManager after EResourceManager
// Sound events and their track programs (engine/e_rsoundevent.cpp, e_rsoundtrackdata.cpp,
// e_soundeventman.cpp, e_soundtrackdataman.cpp). An event ('SEVT', soundeve.arc) says how a sound is played
// (group, volume, probability, range, ducking) and lists hit patches (samples or music streams) to choose
// from; its program ('STRD', soundtra.arc) is run by a cTrack (game/patch.cpp, see gamesound.hpp).
#pragma once
#include <string>
#include <vector>

// A track program (0x24 bytes): 32-bit instructions, kept as stored (big-endian bytes: opcode, A, B, C;
// B..C is a 16-bit immediate for some opcodes). Registers 0x32 and 0x37 of older data are renumbered at load.
struct ERSoundTrackData : public EResource {
  ERSoundTrackData();  // 8036A144
  ~ERSoundTrackData() override;  // 8036A194
  void Load(EFile* file);  // 8036A278
  using EResource::Load;
  SIMMY_STORABLE_DECL(ERSoundTrackData)
  static void RegisterType(unsigned short);  // 8036A650
  void CreateCopy() const;  // 8036A6A4
  const unsigned char* GetInstruction(unsigned int index);  // 8036A6C4
  unsigned int GetProgramSize();  // 8036A6D4

  std::vector<unsigned char> m_program;  // +0x14 (GC: vector<SndInstruction>, 4 bytes each)
};

// 0x34 bytes.
struct ERSoundEvent : public EResource {
  enum : unsigned char { kDuckSfx = 1, kDuckMusic = 2, kDuckAmbient = 4, kDuckVox = 8 };  // m_duckFlags
  ERSoundEvent();  // 803699BC
  ~ERSoundEvent() override;  // 80369A00
  void Load(EFile* file);  // 80369A68
  using EResource::Load;
  void Refresh(EFile* file) override;  // 80369DE0
  SIMMY_STORABLE_DECL(ERSoundEvent)
  static void RegisterType(unsigned short);  // 80369F78
  void CreateCopy() const;  // 80369FCC
  int GetEventType();  // 8036A050
  int GetVolume();  // 8036A058
  int GetProb();  // 8036A060
  void SetVolume(int volume);  // 8036A068
  void SetProb(int prob);  // 8036A070
  const char* GetName() const { return m_name.c_str(); }  // PC: the data header's name (traces)

  unsigned char m_type = 0;        // +0x14 the cBoxX::Event message (1 play, 0x1E kill the source's and play, ...)
  unsigned char m_argsType = 0;    // +0x15 0 none, 1..2 positional (the source object)
  unsigned char m_duckPri = 0;     // +0x16
  unsigned char m_group = 0;       // +0x17 control group: 1 SFX, 2 music, 3 VOX
  unsigned short m_volume = 0;     // +0x18 0..0x400
  unsigned char m_prob = 0;        // +0x1A % chance to play (100+: always)
  unsigned char m_loop = 0;        // +0x1B
  unsigned char m_duckFlags = 0;   // +0x1C
  unsigned char m_noRestart = 0;   // +0x1D 1: playing again does not restart it
  unsigned char m_superDuck = 0;   // +0x1E
  unsigned char m_minRange = 0;    // +0x1F quarter tiles
  unsigned char m_maxRange = 0;    // +0x20
  std::vector<std::vector<SndEvtHitPatch>> m_hitLists;  // +0x24 (+0x28 the count)
  ERSoundTrackData* m_program = nullptr;                // +0x30 (one reference)
  std::string m_name;                                   // PC
};

struct ESoundEventManager : public EResourceManager {
  ESoundEventManager() = default;
  EAHeap* GetHeap() override;  // 80328374
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;  // 80328394
  ERSoundEvent* AddRef(unsigned int id, EFile* file, int pos);  // 80328458
  ERSoundEvent* AddRef(char* name, EFile* file, int pos);  // 80328478
  int CanCache(EResource* res) override;  // 80328498
  void AddRef(EResource* res) { EResourceManager::AddRef(res); }  // PC: the base overload (MSVC: no using-declaration beside same-signature AddRefs)
};

struct ESoundTrackDataManager : public EResourceManager {
  ESoundTrackDataManager() = default;
  EResource* AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) override;  // 803284F8
  ERSoundTrackData* AddRef(unsigned int id, EFile* file, int pos);  // 803285B8
  ERSoundTrackData* AddRef(char* name, EFile* file, int pos);  // 803285D8
  int CanCache(EResource* res) override;  // 803285F8
  void AddRef(EResource* res) { EResourceManager::AddRef(res); }  // PC: the base overload (MSVC: no using-declaration beside same-signature AddRefs)
};
