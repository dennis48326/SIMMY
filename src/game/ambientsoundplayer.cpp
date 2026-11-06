// Port of game/ambientsoundplayer.cpp: one playing ambient sound (see ambientscore.hpp). Start picks one of
// the sound's samples, its drift and its first position (a point, a random spot of the box or near the
// listener, the box's edge nearest the listener for looping sounds); every frame it moves, and its volume
// and pan come from the listeners (cBoxX::GetSurroundVol(AmbientScorePlayer*)), faded in and out over the
// first and last quarter hour of its hours (looping sounds), halved while a ducking event plays, and at
// 45% when no listener is in its room (room-checked sounds).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

#include <cmath>

namespace {
float SquareMag(float x, float y, float z) { return x * x + y * y + z * z; }  // PSVECSquareMag
// CTilePt::CTilePt(float, float, int) (800B4678) with level 1: a coordinate outside a signed byte is 0.
CTilePt AmbientTile(float x, float y) {
  const int ix = static_cast<int>(x), iy = static_cast<int>(y);
  const signed char cx = static_cast<signed char>(ix), cy = static_cast<signed char>(iy);
  return CTilePt(cx == ix ? cx : 0, cy == iy ? cy : 0, 1);
}
// rand % n, n an unsigned conversion of a float span as on GC. PC: 0 for an empty span (GC divided by 0).
unsigned int RandBelow(unsigned int n) {
  const unsigned int r = simmy_sound_rand();
  return n ? r % n : 0u;
}
}  // namespace

// 800A9270
AmbientSoundPlayer::AmbientSoundPlayer() = default;

// 800A92BC
AmbientSoundPlayer::~AmbientSoundPlayer() {
  if (m_snd) {
    m_snd->Release();
    m_snd = nullptr;
  }
  m_scorePlayer = nullptr;
  m_sound = nullptr;
}

// 800A9330
void AmbientSoundPlayer::SetScorePlayer(AmbientScorePlayer* score) { m_scorePlayer = score; }
// 800A9338
void AmbientSoundPlayer::SetAmbientSound(ERAmbientSound* sound) { m_sound = sound; }

// 800A9340
void AmbientSoundPlayer::Update() {
  UpdateSoundPosition();
  UpdatePanAndVolume();
}

// 800A9374
int AmbientSoundPlayer::Start() {
  const unsigned int count = static_cast<unsigned int>(m_sound->m_samples.size());
  if (count == 0) return 0;
  m_sample = m_sound->m_samples[simmy_sound_rand() % count];
  m_snd = g_pSndSys ? g_pSndSys->CreateSoundEffect(m_sample, 0.0f) : nullptr;
  if (m_sound->m_speed == 0) {
    m_vel[1] = 0.0f;
    m_vel[0] = 0.0f;
  } else {
    // The direction (0..255 of a turn) give or take the spread, in tenths of a degree.
    const unsigned int steps = static_cast<unsigned int>(static_cast<int>(static_cast<float>(m_sound->m_angleSpread) * 20.0f) + 1);
    const unsigned int r = simmy_sound_rand() % steps;
    const int base = static_cast<int>(static_cast<unsigned int>(m_sound->m_angle) * 0x168u / 0xFFu -
                                      static_cast<unsigned int>(m_sound->m_angleSpread));
    const float rad = (static_cast<float>(base) + static_cast<float>(static_cast<int>(r)) / 10.0f) * 0.017453292f;
    const float speed = static_cast<float>(static_cast<double>(static_cast<float>(m_sound->m_speed)) / 10.0);
    m_vel[1] = std::sin(rad) * speed;
    m_vel[0] = std::cos(rad) * speed;
  }
  SetInitialSoundPosition();
  if (!(m_sound->m_flags & ERAmbientSound::kRoomCheck)) {
    m_room = -1;
  } else {
    m_room = 0;
    if (1u < static_cast<unsigned int>(m_sound->m_type) - 3u && Globs_pFixedWorld && 0.0f <= m_pos.x &&
        m_pos.x < static_cast<float>(Globs_pFixedWorld->GetYSize()) && 0.0f <= m_pos.y &&
        m_pos.y < static_cast<float>(Globs_pFixedWorld->GetXSize())) {
      CTilePt tile = AmbientTile(m_pos.y, m_pos.x);
      m_room = Globs_pFixedWorld->GetRoomIdFromPoint(tile);
    }
  }
  simmy::SoundTrace("ambient sound: sample %08X at (%.1f, %.1f, %.1f), type %d flags %X, room %d", m_sample, m_pos.x,
                    m_pos.y, m_pos.z, m_sound->m_type, m_sound->m_flags, m_room);
  if (m_snd) {
    UpdatePanAndVolume();
    m_snd->SetPan(m_pan);
    UpdateSoundVolume();
    m_snd->Play();
  }
  return 1;
}

// 800A9738
int AmbientSoundPlayer::Stop() {
  if (m_snd) {
    m_snd->Stop();
    m_snd->Release();
    m_snd = nullptr;
  }
  m_room = -1;
  return 1;
}

// 800A97B4
int AmbientSoundPlayer::Pause() {
  if (m_snd) m_snd->Pause();
  return 1;
}

// 800A97F8
int AmbientSoundPlayer::UnPause() {
  if (m_snd) m_snd->Unpause();
  return 1;
}

// 800A983C
void AmbientSoundPlayer::SetInitialSoundPosition() {
  const float* b = m_sound->m_box;
  std::vector<AmbientScorePlayer::ListenerInfo>& listeners = m_scorePlayer->GetListenerInfos();
  switch (m_sound->m_type) {
    case ERAmbientSound::kPoint:
      m_pos = EVec3(b[0], b[1], b[2]);
      return;
    case ERAmbientSound::kBox:
    case ERAmbientSound::kBox2:
      if (listeners.empty()) {
        // Anywhere in the box.
        m_pos.x = b[0] + static_cast<float>(static_cast<int>(RandBelow(static_cast<unsigned int>((b[3] - b[0]) + 1.0f))));
        m_pos.y = b[4] + static_cast<float>(static_cast<int>(RandBelow(static_cast<unsigned int>((b[1] - b[4]) + 1.0f))));
        m_pos.z = b[5] + static_cast<float>(static_cast<int>(RandBelow(static_cast<unsigned int>((b[2] - b[5]) + 1.0f))));
      } else if (!(m_sound->m_flags & ERAmbientSound::kLooping)) {
        // A random spot of the box within range of a listener, the nearest of the listeners' spots.
        float best = -1.0f;
        const float r = static_cast<float>(m_sound->m_range);
        for (const AmbientScorePlayer::ListenerInfo& l : listeners) {
          float x0 = l.m_pos.x - r;
          if (x0 < b[0]) x0 = b[0];
          float y1 = l.m_pos.y + r;
          if (b[1] < y1) y1 = b[1];
          float z1 = l.m_pos.z + r;
          if (b[2] < z1) z1 = b[2];
          float x1 = l.m_pos.x + r;
          if (b[3] < x1) x1 = b[3];
          float y0 = l.m_pos.y - r;
          if (y0 < b[4]) y0 = b[4];
          float z0 = l.m_pos.z - r;
          if (z0 < b[5]) z0 = b[5];
          if (!(x0 <= x1 && y0 <= y1 && z0 <= z1)) continue;
          const float x = x0 + static_cast<float>(static_cast<int>(RandBelow(static_cast<unsigned int>(static_cast<int>(x1 - x0) + 1))));
          const float y = y0 + static_cast<float>(static_cast<int>(RandBelow(static_cast<unsigned int>(static_cast<int>(y1 - y0) + 1))));
          const float z = z0 + static_cast<float>(static_cast<int>(RandBelow(static_cast<unsigned int>(static_cast<int>(z1 - z0) + 1))));
          const float d = SquareMag(x - l.m_pos.x, y - l.m_pos.y, z - l.m_pos.z);
          if (best == -1.0f || d < best) {
            m_pos = EVec3(x, y, z);
            best = d;
          }
        }
      } else {
        UpdateLoopingSoundPosition();
      }
      return;
    case ERAmbientSound::kAnywhere:
    case ERAmbientSound::kAnywhereNear:
      if (listeners.empty()) {
        m_pos.x = static_cast<float>(static_cast<int>(simmy_sound_rand() % 0x28u));
        m_pos.y = static_cast<float>(static_cast<int>(simmy_sound_rand() % 0x28u));
        m_pos.z = static_cast<float>(static_cast<int>(simmy_sound_rand() % 0x28u));
      } else {
        // Within range of each listener, the nearest of those spots.
        float best = -1.0f;
        for (const AmbientScorePlayer::ListenerInfo& l : listeners) {
          const int r = m_sound->m_range;
          const unsigned int n = static_cast<unsigned int>(r) * 2u + 1u;
          const float x = (l.m_pos.x + static_cast<float>(static_cast<int>(simmy_sound_rand() % n))) - static_cast<float>(r);
          const float y = (l.m_pos.y + static_cast<float>(static_cast<int>(simmy_sound_rand() % n))) - static_cast<float>(r);
          const float z = (l.m_pos.z + static_cast<float>(static_cast<int>(simmy_sound_rand() % n))) - static_cast<float>(r);
          const float d = SquareMag(x - l.m_pos.x, y - l.m_pos.y, z - l.m_pos.z);
          if (best == -1.0f || d < best) {
            m_pos = EVec3(x, y, z);
            best = d;
          }
        }
      }
      return;
    default: return;
  }
}

// 800AA050
void AmbientSoundPlayer::UpdateSoundPosition() {
  const float dy = m_vel[1] * _dt;
  m_pos.x = m_vel[0] * _dt + m_pos.x;
  m_pos.y = dy + m_pos.y;
  if (m_sound->m_flags & ERAmbientSound::kLooping) UpdateLoopingSoundPosition();
}

// 800AA0A4
// A looping sound stays by the listener: sounds outside their box sit at a listener outside the box or
// on the face of the box nearest a listener inside it; box sounds at the box's point nearest a listener;
// "anywhere" sounds at the first listener.
void AmbientSoundPlayer::UpdateLoopingSoundPosition() {
  const float* b = m_sound->m_box;
  std::vector<AmbientScorePlayer::ListenerInfo>& listeners = m_scorePlayer->GetListenerInfos();
  if (m_sound->m_flags & ERAmbientSound::kOutsideBox) {
    float best = -1.0f;
    for (const AmbientScorePlayer::ListenerInfo& l : listeners) {
      const EVec3& p = l.m_pos;
      if (p.x < b[0] || b[3] < p.x || p.y < b[4] || b[1] < p.y || p.z < b[5] || b[2] < p.z) {
        m_pos = p;
        return;
      }
      float d;
      if ((b[3] + b[0]) * 0.5f <= p.x) {
        d = b[3] - p.x;
        if (best < 0.0f || d < best) {
          m_pos = p;
          m_pos.x = b[3];
          best = d;
        }
      } else {
        d = p.x - b[0];
        if (best < 0.0f || d < best) {
          m_pos = p;
          m_pos.x = b[0];
          best = d;
        }
      }
      // (GC: the y and z halves are taken from the box's size, not its middle.)
      if ((b[1] - b[4]) * 0.5f <= p.y) {
        d = b[1] - p.y;
        if (best < 0.0f || d < best) {
          m_pos = p;
          m_pos.y = b[1];
          best = d;
        }
      } else {
        d = p.y - b[4];
        if (best < 0.0f || d < best) {
          m_pos = p;
          m_pos.y = b[4];
          best = d;
        }
      }
      if ((b[2] - b[5]) * 0.5f <= p.z || b[5] <= 0.0f) {
        d = b[2] - p.z;
        if (best < 0.0f || d < best) {
          m_pos = p;
          m_pos.z = b[2];
          best = d;
        }
      } else {
        d = p.z - b[5];
        if (best < 0.0f || d < best) {
          m_pos = p;
          m_pos.z = b[5];
          best = d;
        }
      }
    }
    return;
  }
  switch (m_sound->m_type) {
    case ERAmbientSound::kBox:
    case ERAmbientSound::kBox2: {
      float best = -1.0f;
      for (const AmbientScorePlayer::ListenerInfo& l : listeners) {
        const EVec3& p = l.m_pos;
        float x = b[0];
        if (b[0] <= p.x) {
          x = b[3];
          if (p.x < b[3]) x = p.x;
        }
        float y = b[4];
        if (b[4] <= p.y) {
          y = b[1];
          if (p.y < b[1]) y = p.y;
        }
        float z = b[5];
        if (b[5] <= p.z) {
          z = b[2];
          if (p.z < b[2]) z = p.z;
        }
        const float d = SquareMag(p.x - x, p.y - y, p.z - z);
        if (best < 0.0f || d < best) {
          m_pos = EVec3(x, y, z);
          best = d;
        }
      }
      return;
    }
    case ERAmbientSound::kAnywhere:
    case ERAmbientSound::kAnywhereNear:
      if (!listeners.empty()) {
        // (GC: x is set twice, the second time to the listener's y; y keeps its value.)
        m_pos.x = listeners[0].m_pos.x;
        m_pos.x = listeners[0].m_pos.y;
        m_pos.z = listeners[0].m_pos.z;
      }
      return;
    default: return;
  }
}

// 800AA5B8
void AmbientSoundPlayer::UpdatePanAndVolume() {
  int left = 0, right = 0, front = 0, back = 0;
  int volume = 0, pan = 0x200;
  // (GC: only while Globs::pEORGlobals is set, which it always is: &_globals.)
  if (cBoxX::GetSurroundVol(m_scorePlayer, m_pos, 0.0f, static_cast<float>(m_sound->m_range), left, right, front, back)) {
    cBoxX::ConvertSurroundToVolPan(left, right, front, back, volume, pan);
  } else {
    volume = 0;
    m_pan = 0x200;
    pan = 0x200;
  }
  if (pan != m_pan && m_snd) {
    m_pan = pan;
    m_snd->SetPan(pan);
  }
  const ERAmbientSound* s = m_sound;
  if ((s->m_flags & ERAmbientSound::kLooping) && !(s->m_flags & ERAmbientSound::kNoFade) && s->m_startHour != s->m_endHour) {
    // The first and last quarter hour of the sound's hours fade it in and out.
    const double t = m_scorePlayer->GetTime();
    float f = static_cast<float>(t - static_cast<double>(static_cast<float>(s->m_startHour)));
    bool fade = !(0.25f <= f || f < 0.0f);
    if (!fade) {
      f = static_cast<float>(static_cast<double>(static_cast<float>(s->m_endHour)) - t);
      fade = !(0.25f <= f || f < 0.0f);
    }
    if (fade) volume = static_cast<int>(static_cast<float>(volume) * f * 4.0f);
  }
  if (AmbientScorePlayer::s_duckAmbient) volume = cTrack::s_superDuckEnabled ? 0 : volume / 2;
  if (s->m_flags & ERAmbientSound::kRoomCheck) {
    bool heard = false;
    for (const AmbientScorePlayer::ListenerInfo& l : m_scorePlayer->GetListenerInfos())
      if (l.m_room == m_room) {
        heard = true;
        break;
      }
    if (!heard) volume = static_cast<int>(static_cast<unsigned int>(static_cast<float>(volume) * 0.45f));
  }
  if (volume != m_volume) {
    m_volume = volume;
    UpdateSoundVolume();
  }
}

// 800AA904
// The sound's volume: its own percentage, the score's and the score player's volume (0..0x400).
void AmbientSoundPlayer::UpdateSoundVolume() {
  if (!m_snd) return;
  int v = ((m_volume * static_cast<int>(m_sound->m_volume)) / 100 * static_cast<int>(m_scorePlayer->GetAmbientScore()->m_volume)) /
          100 * m_scorePlayer->GetVolume();
  if (v < 0) v += 0x3FF;
  m_snd->SetVolume(v >> 10);
}

// 800AA9B4
ERAmbientSound* AmbientSoundPlayer::GetAmbientSound() { return m_sound; }

// 800AA9BC
int AmbientSoundPlayer::IsFinished() {
  if (m_snd && m_snd->IsPlaying()) return 0;
  return 1;
}

// 800AAA18
int AmbientSoundPlayer::IsValid() { return m_scorePlayer && m_sound ? 1 : 0; }
