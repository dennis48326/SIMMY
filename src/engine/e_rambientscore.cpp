// Port of engine/e_rambientscore.cpp and engine/e_ambientscoreman.cpp: ambient scores ("ambientscores",
// the neighbourhood ambience; see ambientscore.hpp). The data is read field by field, big-endian.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

// ---------------------------------------------------------------------------------------------------
// ERAmbientSound

// 80366998
ERAmbientSound::ERAmbientSound() = default;

// 80366A04
ERAmbientSound::~ERAmbientSound() = default;

// 80366A70
// The sample count, the box (6 floats), ten bytes (volume, flags, type, weight, range, hours, angle,
// spread, speed), then the sample ids.
void ERAmbientSound::Load(EFile* file) {
  const unsigned int count = simmy::read_u32(*file);
  for (float& f : m_box) f = simmy::read_float(*file);
  m_volume = simmy::read_u8(*file);
  m_flags = simmy::read_u8(*file);
  m_type = simmy::read_u8(*file);
  m_weight = simmy::read_u8(*file);
  m_range = simmy::read_u8(*file);
  m_startHour = simmy::read_u8(*file);
  m_endHour = simmy::read_u8(*file);
  m_angle = simmy::read_u8(*file);
  m_angleSpread = simmy::read_u8(*file);
  m_speed = simmy::read_u8(*file);
  m_samples.assign(count, 0u);
  for (unsigned int i = 0; i < count; ++i) m_samples[i] = simmy::read_u32(*file);
}

// ---------------------------------------------------------------------------------------------------
// ERAmbientScore

// 80366D50
ERAmbientScore::ERAmbientScore() = default;

// 80366DC0
ERAmbientScore::~ERAmbientScore() {
  delete[] m_name;
  m_name = nullptr;
}

// 80366E90
// The header words (version, 'SCOT', variant) are read raw (no EDataHeader check), then the name (its
// length first), the size, the playing counts and volume, and the sounds.
void ERAmbientScore::Load(EFile* file, unsigned int) {
  m_version = simmy::read_u32(*file);
  m_signature = simmy::read_u32(*file);
  m_variant = simmy::read_u32(*file);
  const unsigned int len = simmy::read_u32(*file);
  delete[] m_name;
  m_name = new char[len + 1];  // PC: one more, so the name is terminated whatever the data says
  file->Read(m_name, len);
  m_name[len] = 0;
  m_size = simmy::read_u32(*file);
  m_minPlaying = simmy::read_u8(*file);
  m_maxPlaying = simmy::read_u8(*file);
  m_volume = simmy::read_u8(*file);
  const unsigned int count = simmy::read_u32(*file);
  m_sounds.assign(count, ERAmbientSound());
  for (unsigned int i = 0; i < count; ++i) m_sounds[i].Load(file);
}

// 803670D4
void ERAmbientScore::Refresh(EFile* file) {
  if (file) Load(file, 0);
}

// 8036726C
void ERAmbientScore::RegisterType(unsigned short) {}
// 803672C0
void ERAmbientScore::CreateCopy() const {}

// 80367224 ERAmbientScore::GetTypeInfo
// 80367230 ERAmbientScore::GetTypeName
// 8036723C ERAmbientScore::GetTypeKey
// 80367248 ERAmbientScore::GetTypeVersion
// 803671E4 ERAmbientScore::SafeDelete
// 80367170 ERAmbientScore::New
// 803671A0 ERAmbientScore::Construct
// 803671C0 ERAmbientScore::Destruct
// 80367254 ERAmbientScore::GetTypeInfoStatic
// 80367260 ERAmbientScore::GetReadVersion
SIMMY_STORABLE_IMPL(ERAmbientScore, "ERAmbientScore", 1, EResource)

// ---------------------------------------------------------------------------------------------------
// EAmbientScoreManager (g_ambientscoreman)

// 80322BA0
EAHeap* EAmbientScoreManager::GetHeap() { return VMHeap(); }

// 80322BC0
EResource* EAmbientScoreManager::AllocateAndLoadResource(EFile* file, unsigned int size, unsigned int id) {
  auto* score = new ERAmbientScore;
  score->m_id = id;
  const int start = file->Tell();
  score->Load(file, 0);
  file->Seek(start + static_cast<int>(size), EFile__SeekType::Set);  // PC: past the resource whatever was read
  return score;
}

// 80322C84
ERAmbientScore* EAmbientScoreManager::AddRef(unsigned int id, EFile* file, int pos) {
  return static_cast<ERAmbientScore*>(EResourceManager::AddRef(id, file, pos));
}
// 80322CA4
ERAmbientScore* EAmbientScoreManager::AddRef(char* name, EFile* file, int pos) {
  return static_cast<ERAmbientScore*>(EResourceManager::AddRef(name, file, pos));
}
// 80322CC4
int EAmbientScoreManager::CanCache(EResource*) { return 1; }

// 80322CCC global constructors keyed to EAmbientScoreManager::GetHeap (g_ambientscoreman:
// src/platform/unported_globals.cpp)
// 80322CF8 global destructors keyed to EAmbientScoreManager::GetHeap
