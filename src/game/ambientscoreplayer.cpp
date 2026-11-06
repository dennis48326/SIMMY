// Port of game/ambientscoreplayer.cpp: plays an ambient score (see ambientscore.hpp). Once a second the
// finished sounds are dropped and, while fewer than the score's maximum play, one more sound is chosen by
// weight among those whose hours cover the current time, that are not playing yet and that a listener can
// hear; below the score's minimum one is always chosen, otherwise only with the weights' total in 100.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

#include <chrono>

int AmbientScorePlayer::s_duckAmbient = 0;

namespace {
// timeGetTime: milliseconds.
// (On the game's clock: SIMMY_DET makes it virtual.)
unsigned int TimeGetTime() {
  static const unsigned long long start = simmy::game_clock_ns();
  return static_cast<unsigned int>((simmy::game_clock_ns() - start) / 1000000ull);
}
float SquareMag(float x, float y, float z) { return x * x + y * y + z * z; }  // PSVECSquareMag
// CTilePt::CTilePt(float, float, int) (800B4678) with level 1: a coordinate outside a signed byte is 0.
CTilePt AmbientTile(float x, float y) {
  const int ix = static_cast<int>(x), iy = static_cast<int>(y);
  const signed char cx = static_cast<signed char>(ix), cy = static_cast<signed char>(iy);
  return CTilePt(cx == ix ? cx : 0, cy == iy ? cy : 0, 1);
}
}  // namespace

// 800A80B8
AmbientScorePlayer::AmbientScorePlayer(bool flag) : m_flag(flag), m_lastUpdate(TimeGetTime()) {}

// 800A813C
AmbientScorePlayer::~AmbientScorePlayer() { Shutdown(); }

// 800A81D8
void AmbientScorePlayer::Update() {
  if (!m_score || !m_players) return;
  for (int i = 0; i < m_playerCount; ++i)
    if (m_players[i].IsValid()) m_players[i].Update();
  if (TimeGetTime() - m_lastUpdate - 1u <= 0x3E6u) return;  // about once a second
  m_lastUpdate = TimeGetTime();
  RemoveFinishedSounds();
  if (m_state != kPlaying) return;
  int total = 0;
  std::vector<ERAmbientSound*> candidates;
  for (ERAmbientSound& sound : m_score->m_sounds) {
    if (SoundRangeCoversCurTime(&sound) && !SoundAlreadyPlaying(&sound) && SoundInHearingDistance(&sound) &&
        !sound.m_samples.empty()) {
      total += sound.m_weight;
      candidates.push_back(&sound);
    }
  }
  if (total == 0 || candidates.empty()) return;
  int below = m_minPlaying - m_numPlaying;
  if (below < 0) below = 0;
  if (m_numPlaying < m_playerCount) {
    if (AmbientSoundPlayer* player = AddSoundBasedOnProbability(candidates, total, 0 < below)) player->Start();
  }
}

// 800A8524
void AmbientScorePlayer::SetListenerCount(int count) { m_listeners.resize(static_cast<unsigned int>(count)); }

// 800A85A4
// The listener's room: the world's room at its tile (the position's y is the tile's x), 0 off the lot.
void AmbientScorePlayer::UpdateListener(unsigned int index, EVec3& pos, EVec3& dir) {
  if (m_listeners.size() <= index) SetListenerCount(static_cast<int>(index + 1));
  ListenerInfo& l = m_listeners[index];
  l.m_pos = pos;
  l.m_dir = dir;
  if (!Globs_pFixedWorld) {
    l.m_room = -1;
    return;
  }
  if (0.0f <= l.m_pos.x && l.m_pos.x < static_cast<float>(Globs_pFixedWorld->GetYSize()) && 0.0f <= l.m_pos.y &&
      l.m_pos.y < static_cast<float>(Globs_pFixedWorld->GetXSize())) {
    CTilePt tile = AmbientTile(l.m_pos.y, l.m_pos.x);
    l.m_room = Globs_pFixedWorld->GetRoomIdFromPoint(tile);
    return;
  }
  l.m_room = 0;
}

// 800A8764
void AmbientScorePlayer::SetTime(float hours) { m_time = hours; }
// 800A876C
double AmbientScorePlayer::GetTime() { return m_time; }

// 800A8774
// The named score (one reference) and its sound players.
int AmbientScorePlayer::Load(char* name) {
  if (!KillAllAmbientSounds()) return 0;
  if (m_score) {
    g_ambientscoreman.DelRef(m_score->m_id, EResourceManager__DelRefMode{EResourceManager__DelRefMode::Cache});
    m_score = nullptr;
  }
  m_score = g_ambientscoreman.AddRef(name, nullptr, 0);
  delete[] m_players;
  m_players = nullptr;
  if (m_score) {
    m_minPlaying = m_score->m_minPlaying;
    m_playerCount = m_score->m_maxPlaying;
    m_players = new AmbientSoundPlayer[static_cast<unsigned int>(m_playerCount)];
    for (int i = 0; i < m_playerCount; ++i) m_players[i].SetScorePlayer(this);
  }
  simmy::SoundTrace("ambient score %s: %s, %d sounds, %d..%d playing, volume %d%%", name, m_score ? "loaded" : "missing",
                    m_score ? static_cast<int>(m_score->m_sounds.size()) : 0, m_minPlaying, m_playerCount,
                    m_score ? m_score->m_volume : 0);
  return 1;
}

// 800A88E0
int AmbientScorePlayer::Start() {
  m_lastUpdate = TimeGetTime();
  m_state = kPlaying;
  return 1;
}

// 800A891C
bool AmbientScorePlayer::Stop() {
  m_state = kStopped;
  return KillAllAmbientSounds() != 0;
}

// 800A8954
int AmbientScorePlayer::Shutdown() {
  if (!Stop()) return 0;
  KillAllAmbientSounds();
  if (m_score) {
    g_ambientscoreman.DelRef(m_score->m_id, EResourceManager__DelRefMode{EResourceManager__DelRefMode::Cache});
    m_score = nullptr;
  }
  delete[] m_players;
  m_players = nullptr;
  m_numPlaying = 0;
  return 1;
}

// 800A8A18
// (As on GC the loop ends at the first sound that pauses or resumes, which is the first valid one.)
int AmbientScorePlayer::SetPause(bool pause) {
  m_state = pause ? kPaused : kPlaying;
  simmy::SoundTrace("ambient score %s", pause ? "paused" : "resumed");
  if (!m_players) return 1;
  for (int i = 0; i < m_playerCount; ++i) {
    if (!m_players[i].IsValid()) continue;
    if (pause ? m_players[i].Pause() : m_players[i].UnPause()) return 1;
  }
  return 1;
}

// 800A8B10
int AmbientScorePlayer::SetVolume(int volume) {
  m_volume = volume;
  return 1;
}
// 800A8B1C
int AmbientScorePlayer::GetVolume() { return m_volume; }
// 800A8B24
ERAmbientScore* AmbientScorePlayer::GetAmbientScore() { return m_score; }
// 800A8B2C
int AmbientScorePlayer::GetNumPlaying() { return m_numPlaying; }
// 800A8B34
std::vector<AmbientScorePlayer::ListenerInfo>& AmbientScorePlayer::GetListenerInfos() { return m_listeners; }

// 800A8B3C
// [start, end) of the day's hours; a range that wraps past midnight when start >= end.
int AmbientScorePlayer::SoundRangeCoversCurTime(ERAmbientSound* sound) {
  const float start = static_cast<float>(sound->m_startHour);
  const float end = static_cast<float>(sound->m_endHour);
  if (sound->m_startHour < sound->m_endHour) return start <= m_time && m_time < end;
  if (m_time < start && end <= m_time) return 0;
  return 1;
}

// 800A8C28
int AmbientScorePlayer::SoundAlreadyPlaying(ERAmbientSound* sound) {
  for (int i = 0; i < m_playerCount; ++i)
    if (m_players[i].IsValid() && m_players[i].GetAmbientSound() == sound) return 1;
  return 0;
}

// 800A8CB0
// Whether a listener is within the sound's range: of its point, of its box (or, for sounds outside the
// box, anywhere but deep inside it); "anywhere" sounds always.
int AmbientScorePlayer::SoundInHearingDistance(ERAmbientSound* sound) {
  const float* b = sound->m_box;
  for (const ListenerInfo& l : m_listeners) {
    const float* p = &l.m_pos.x;
    switch (sound->m_type) {
      case ERAmbientSound::kPoint:
        if (SquareMag(p[0] - b[0], p[1] - b[1], p[2] - b[2]) < static_cast<float>(sound->m_range * sound->m_range))
          return 1;
        break;
      case ERAmbientSound::kAnywhere:
      case ERAmbientSound::kAnywhereNear: return 1;
      case ERAmbientSound::kBox:
      case ERAmbientSound::kBox2: {
        const float r = static_cast<float>(sound->m_range);
        if (!(sound->m_flags & ERAmbientSound::kOutsideBox)) {
          if (b[0] - r < p[0] && p[0] < b[3] + r && b[4] - r < p[1] && p[1] < b[1] + r && b[5] - r < p[2] &&
              p[2] < b[2] + r)
            return 1;
        } else {
          if (p[0] < b[0] + r || b[3] - r < p[0] || p[1] < b[4] + r || b[1] - r < p[1] || p[2] < b[5] + r ||
              b[2] - r < p[2])
            return 1;
        }
        break;
      }
      default: break;
    }
  }
  return 0;
}

// 800A8ED8
// A sound by weight: a number below the weights' total (or below 100 when the total is less and no sound
// is forced, so the weights are percentages), the sound whose share holds it.
AmbientSoundPlayer* AmbientScorePlayer::AddSoundBasedOnProbability(std::vector<ERAmbientSound*>& sounds, int total,
                                                                   bool force) {
  const unsigned int r = simmy_sound_rand();
  int n;
  if (total < 100 && !force) n = static_cast<int>(r % 100u);
  else n = static_cast<int>(r % static_cast<unsigned int>(total));
  if (total < n) return nullptr;
  for (ERAmbientSound* sound : sounds) {
    const int w = sound->m_weight;
    if (n <= w) return AddNewAmbientSoundPlayer(sound);
    n -= w;
  }
  return nullptr;
}

// 800A8F90
int AmbientScorePlayer::KillAllAmbientSounds() {
  if (!m_players) return 1;
  int ok = 1;
  for (int i = 0; i < m_playerCount; ++i) {
    if (!m_players[i].IsValid()) continue;
    if (!m_players[i].Stop()) {
      ok = 0;
    } else {
      m_players[i].SetAmbientSound(nullptr);
      --m_numPlaying;
    }
  }
  return ok;
}

// 800A9048
AmbientSoundPlayer* AmbientScorePlayer::AddNewAmbientSoundPlayer(ERAmbientSound* sound) {
  for (int i = 0; i < m_playerCount; ++i) {
    if (m_players[i].IsValid()) continue;
    m_players[i].SetAmbientSound(sound);
    ++m_numPlaying;
    return &m_players[i];
  }
  return nullptr;
}

// 800A90DC
int AmbientScorePlayer::RemoveAmbientSoundPlayer(ERAmbientSound* sound) {
  for (int i = 0; i < m_playerCount; ++i) {
    if (!m_players[i].IsValid() || m_players[i].GetAmbientSound() != sound) continue;
    if (!m_players[i].Stop()) return 0;
    m_players[i].SetAmbientSound(nullptr);
    --m_numPlaying;
    return 1;
  }
  return 0;
}

// 800A9198
// Finished sounds, and sounds with flags whose hours are over.
void AmbientScorePlayer::RemoveFinishedSounds() {
  for (int i = 0; i < m_playerCount; ++i) {
    AmbientSoundPlayer& p = m_players[i];
    if (!p.IsValid()) continue;
    if (p.IsFinished() || (p.GetAmbientSound()->m_flags != 0 && !SoundRangeCoversCurTime(p.GetAmbientSound()))) {
      p.Stop();
      p.SetAmbientSound(nullptr);
      --m_numPlaying;
    }
  }
}
