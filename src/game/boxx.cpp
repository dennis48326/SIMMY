// Port of game/boxx.cpp: cBoxX, the sound director (g_pBoxX), and cSoundModeManager, the game-mode music
// and volume manager. cBoxX::Event is the director's one entry point (see gamesound.hpp for the messages).
//
// Positional sounds: each player's live-mode camera is a listener (GetListenerPosAndDir: 2 units along the
// view from the eye, 2 below it, not below the ground). For every source of a positional event the
// listeners' surround gains (left, right, front, back; 0x400 = full) are summed and turned into one volume
// and pan (ConvertSurroundToVolPan), so in split screen a sound near either player's camera is heard, as
// on the GameCube. The event's range (+0x1F/+0x20, quarter tiles) sets full volume inside the minimum and
// silence beyond the maximum; a listener whose sim is in social mode hears the others at g_social_duck.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>

cBoxX* g_pBoxX = nullptr;
int g_bBoxXIsInitted = 0;
float g_social_duck = 0.45f;  // 804FDBB4
float g_cam_height = 2.0f;    // 804FDBB8
float g_cam_dist = 2.0f;      // 804FDBBC
int g_demoMusic = 0;          // 804FDBB0
const char* s_frontEndTracks[4] = {"music_48_title_track", "music_24_bbcas1", "music_24_bbcas2",
                                   "music_24_bbcas3"};  // 8042401C

ELiveMode* LiveModeForSound();           // DAT_80475C48 (game/livemode.cpp)
int BackgroundSaveInProgressForSound();  // g_backgroundSaveInProgress (game/memcardscreens.cpp)

namespace {
std::recursive_mutex g_eventMutex;  // 80485DE0 (an EThreadMutex)
int lSimSpeed = 0;                  // 804FDBCC lSimSpeed.7593

// The options' volumes (OptionsRecon, DAT_80475F14): +0x1C SFX, +0x1D music, +0x28 voices, +0x29 ambience
// (0..10). PC: without OptionsRecon (not made yet) the default 10.
int Option(int offset) {
  const auto* options = static_cast<const signed char*>(_globals.m_optionsRecon);
  return options ? options[offset] : 10;
}

// The listener of player `i` (its live-mode camera), or null.
ESimsCam* ListenerCam(int i) { return _gd.m_players[i].m_cam; }

// GetPersonData(0x28) of player `i`'s selected sim: in social mode.
int SocialMode(int i) {
  cXPerson* p = _globals.m_selectedPerson[i];
  return p ? p->GetPersonData(0x28) : 0;
}

float Mag(const EVec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
void Normalize(EVec3& v) {
  const float m = Mag(v);
  if (m != 0.0f) {
    v.x /= m;
    v.y /= m;
    v.z /= m;
  }
}
}  // namespace

// 800AD628
// The track VM's source data requests (cAudioInfo fields 0..0x14).
bool BoxxGlobalGetSourceParamValue(int source, int field, int* value) {
  if (field < 0x15) *value = cAudioInfo::GetObjectData(source, field);
  return field < 0x15;
}

// 800AD670
cBoxX::cBoxX() { g_pBoxX = this; }

// 800AD724
cBoxX::~cBoxX() {
  g_bBoxXIsInitted = 0;
  if (m_qdata) {
    m_qdata->DelRef();
    m_qdata = nullptr;
  }
  m_instances.clear();
}

// 800AD7D8
// The sound tables (quickdata 0x0C33DB41), the track scheduler, the ambient score player and the timer.
int cBoxX::Init() {
  m_qdata = _quickdataman.AddRef(0x0C33DB41u, nullptr, 0);
  g_pHitMan = new cHitMan;
  g_pHitMan->Init();
  LoadQdataTables();
  g_pHitMan->RegisterSourceDataRequestHandler(BoxxGlobalGetSourceParamValue);
  m_score = new AmbientScorePlayer(false);
  m_timer = new cHitTimer;
  m_timer->m_count = 0;
  m_timer->m_running = 1;
  m_timer->Update();
  m_quietTime = 0;
  m_enabled = 1;
  g_bBoxXIsInitted = 1;
  return 1;
}

// 800AD898
void cBoxX::LoadQdataTables() {}

// 800AD89C
int cBoxX::Shutdown() {
  if (m_score) {
    m_score->Shutdown();
    delete m_score;
    m_score = nullptr;
  }
  if (g_pHitMan) {
    g_pHitMan->Shutdown();
    delete g_pHitMan;
    g_pHitMan = nullptr;
  }
  if (m_timer) {
    m_timer->m_running = 0;
    m_timer->Stop();
    delete m_timer;
    m_timer = nullptr;
  }
  g_bBoxXIsInitted = 0;
  return 1;
}

// 800AD950
// A tick: the timer and the tracks; in live mode (or CAS) the ambient score follows the listeners and the
// simulation's time of day; then every positional event's volume and pan.
void cBoxX::Update(unsigned int) {
  if (!g_bBoxXIsInitted) return;
  if (m_timer->m_running) ++m_timer->m_count;
  g_pHitMan->TimerCallback();
  const int mode = cSoundPlayer::GetGameMode();
  ELiveMode* live = LiveModeForSound();
  if ((mode == cSoundPlayer::kLive || mode == cSoundPlayer::kCAS) && m_score && live && live->m_paused == 0) {
    int listeners = 0;
    const int players = _gd.m_fieldEC ? 2 : 1;
    for (int i = 0; i < players; ++i) {
      if (!ListenerCam(i)) continue;
      EVec3 pos, dir;
      GetListenerPosAndDir(ListenerCam(i), pos, dir);
      m_score->UpdateListener(static_cast<unsigned int>(listeners), pos, dir);
      ++listeners;
    }
    m_score->SetListenerCount(listeners);
    if (Globs_pSimulator) {
      const float hour = static_cast<float>(Globs_pSimulator->GetGlobal(cSimulatorImpl::kHour));
      const float minute = static_cast<float>(Globs_pSimulator->GetGlobal(cSimulatorImpl::kMinute));
      m_score->SetTime(minute * 0.016667f + hour);
    }
    m_score->Update();
  }
  UpdateAllSndobVolPan();
}

// 800ADB18
void cBoxX::UpdateGameMode() {
  if (g_bBoxXIsInitted) m_modes.Update();
}

// 800ADB48
// The ambient score's version of GetSurroundVol: summed over the score's own listeners (cBoxX::Update gives
// it the players' cameras), ducked for a listener whose sim is in social mode; the ranges are at least 1
// and 2 tiles.
int cBoxX::GetSurroundVol(AmbientScorePlayer* score, EVec3& pos, float minRange, float maxRange, int& left, int& right,
                          int& front, int& back) {
  left = right = front = back = 0;
  int heard = 0;
  if (minRange < 1.0f) minRange = 1.0f;
  if (maxRange < minRange + 1.0f) maxRange = minRange + 1.0f;
  std::vector<AmbientScorePlayer::ListenerInfo>& listeners = score->GetListenerInfos();
  for (unsigned int i = 0; i < listeners.size(); ++i) {
    EVec3 ear = listeners[i].m_pos;
    EVec3 dir = listeners[i].m_dir;
    if (dir.x != 0.0f || dir.y != 0.0f || dir.z != 0.0f) Normalize(dir);
    int l = 0, r = 0, f = 0, b = 0;
    if (!CalculateSurroundVol(ear, dir, pos, minRange, maxRange, l, r, f, b)) continue;
    // GC: _globals' selected person per listener (two), vtable 0x13C GetPersonData(0x28).
    if (i < 2 && SocialMode(static_cast<int>(i))) {
      left += static_cast<int>(static_cast<float>(l) * g_social_duck);
      right += static_cast<int>(static_cast<float>(r) * g_social_duck);
      front += static_cast<int>(static_cast<float>(f) * g_social_duck);
      back += static_cast<int>(static_cast<float>(b) * g_social_duck);
    } else {
      left += l;
      right += r;
      front += f;
      back += b;
    }
    heard = 1;
  }
  if (0x400 < right) right = 0x400;
  if (0x400 < left) left = 0x400;
  return heard;
}

// 800ADEA0
// The surround gains of a sound at `pos` summed over both players' listeners (ducked for a listener whose
// sim is in social mode, unless `source` is that sim or what it talks with). False when no listener hears
// it, and while the live mode is paused.
int cBoxX::GetSurroundVol(EVec3& pos, float minRange, float maxRange, int& left, int& right, int& front, int& back,
                          int source) {
  left = right = front = back = 0;
  if (LiveModeForSound() && LiveModeForSound()->m_paused != 0) return 0;
  if (minRange < 2.0f) minRange = 2.0f;
  if (maxRange < minRange + 2.0f) maxRange = minRange + 2.0f;
  int heard = 0;
  for (int i = 0; i < 2; ++i) {
    if (!ListenerCam(i)) continue;
    EVec3 ear, dir;
    GetListenerPosAndDir(ListenerCam(i), ear, dir);
    int l = 0, r = 0, f = 0, b = 0;
    if (!CalculateSurroundVol(ear, dir, pos, minRange, maxRange, l, r, f, b)) continue;
    heard = 1;
    const int social = SocialMode(i);
    cXPerson* person = _globals.m_selectedPerson[i];
    bool full = social == 0 || social == source;
    if (!full && person) {
      // GC: the listener's sim itself (vtable 0x32C, its object id) is not ducked either.
      cXObject* self = person->AsObject();
      full = self && self->GetID() == source;
    }
    if (full) {
      left += l;
      right += r;
      front += f;
      back += b;
    } else {
      left += static_cast<int>(static_cast<float>(l) * g_social_duck);
      right += static_cast<int>(static_cast<float>(r) * g_social_duck);
      front += static_cast<int>(static_cast<float>(f) * g_social_duck);
      back += static_cast<int>(static_cast<float>(b) * g_social_duck);
    }
  }
  if (0x400 < right) right = 0x400;
  if (0x400 < left) left = 0x400;
  return heard;
}

// 800AE1D4
// One listener's gains: full inside `minRange`, falling linearly to silence at `maxRange`; the direction
// in the ground plane, relative to the view, spreads them over the four speakers (equal power).
int cBoxX::CalculateSurroundVol(EVec3& ear, EVec3& dir, EVec3& pos, float minRange, float maxRange, int& left,
                                int& right, int& front, int& back) {
  back = 0;
  front = 0;
  left = 0;
  right = 0;
  if (minRange < 1.0f) minRange = 1.0f;
  if (maxRange < minRange + 1.0f) maxRange = minRange + 1.0f;
  EVec3 d(pos.x - ear.x, pos.y - ear.y, pos.z - ear.z);
  const float dist = Mag(d);
  float cutoff = maxRange + maxRange;
  if (cutoff < maxRange + 12.0f) cutoff = maxRange + 12.0f;
  if (cutoff <= dist) return 0;
  if (maxRange <= dist) return 1;  // in hearing range of the director, but silent
  if (dist <= 1.0f) {
    left = right = front = back = 0x400;
    return 1;
  }
  float g = 1.0f - (dist - minRange) / (maxRange - minRange);
  if (1.0f < g) g = 1.0f;
  if (g < 0.001f) return 1;
  float vx = dir.x, vy = dir.y;
  float n = std::sqrt(vx * vx + vy * vy);
  if (n != 0.0f) {
    vx /= n;
    vy /= n;
  }
  float fwd = d.x * vx + d.y * vy;
  float side = d.x * vy - d.y * vx;
  n = std::sqrt(side * side + fwd * fwd);
  if (n != 0.0f) {
    side /= n;
    fwd /= n;
  }
  g *= 1024.0f;
  right += static_cast<int>(g * std::sqrt((side + 1.0f) * 0.5f));
  left += static_cast<int>(g * std::sqrt((1.0f - side) * 0.5f));
  front += static_cast<int>(g * std::sqrt((fwd + 1.0f) * 0.5f));
  back += static_cast<int>(g * std::sqrt((1.0f - fwd) * 0.5f));
  return 1;
}

// 800AE4BC
// A source object's gains for `track` (a source < 1 is everywhere at full volume).
int cBoxX::GetInstanceSurroundVol(int source, cTrack* track, int& left, int& right, int& front, int& back) {
  left = right = front = back = 0;
  if (source < 1) {
    left = right = front = 0x400;
    return 0;
  }
  cAudioWorldCoord where;
  if (!cAudioInfo::GetObjectPosition(source, where)) return 0;
  EVec3 pos(where.x, where.y, 0.0f);
  const float minRange = static_cast<float>(track->GetMinRange()) * 0.25f;
  float maxRange = static_cast<float>(track->GetMaxRange()) * 0.25f;
  if (maxRange <= minRange) maxRange = minRange + 2.0f;
  int social = 0;
  cXObject* object = Globs_pObjectModule ? Globs_pObjectModule->GetObjectFromID(source) : nullptr;
  if (object) {
    if (auto* person = dynamic_cast<cXPerson*>(object))
      if (person->GetPersonData(0x28) != 0) social = source;
  }
  return GetSurroundVol(pos, minRange, maxRange, left, right, front, back, social);
}

// 800AE6A8
// A music source's volume: the nearest listener's distance only (no pan).
int cBoxX::GetInstanceVolNoPan(int source, cTrack* track, int& volume) {
  volume = 0;
  if (source < 1) {
    volume = 0x400;
    return 0;
  }
  if (LiveModeForSound() && LiveModeForSound()->m_paused != 0) return 0;
  cAudioWorldCoord where;
  if (!cAudioInfo::GetObjectPosition(source, where)) return 0;
  EVec3 pos(where.x, where.y, 0.0f);
  float minRange = static_cast<float>(track->GetMinRange()) * 0.25f;
  float maxRange = static_cast<float>(track->GetMaxRange()) * 0.25f;
  if (maxRange <= minRange) maxRange = minRange + 1.0f;
  if (minRange < 2.0f) minRange = 2.0f;
  if (maxRange < minRange + 2.0f) maxRange = minRange + 2.0f;
  for (int i = 0; i < 2; ++i) {
    if (!ListenerCam(i)) continue;
    EVec3 ear, dir;
    GetListenerPosAndDir(ListenerCam(i), ear, dir);
    EVec3 d(pos.x - ear.x, pos.y - ear.y, pos.z - ear.z);
    const float dist = Mag(d);
    if (maxRange + maxRange <= dist) continue;
    float g = 1.0f;
    if (minRange < dist) {
      if (maxRange <= minRange) maxRange = minRange + 1.0f;
      g = 1.0f - (dist - minRange) / (maxRange - minRange);
    }
    if (SocialMode(i)) g = g * g_social_duck * g_social_duck;
    if (0.001f <= g) volume += static_cast<int>(g * 1024.0f);
  }
  if (0x400 < volume) volume = 0x400;
  return 1;
}

// 800AE948
// The louder of left/right is the volume, their ratio the pan (0x200 centre); a sound behind (back louder
// than front) pans past the side (0..0x400 reflected beyond), which cGZSnd plays phase-flipped.
void cBoxX::ConvertSurroundToVolPan(int left, int right, int front, int back, int& volume, int& pan) {
  if (left + right == 0) {
    volume = 0;
    pan = 0x200;
    return;
  }
  if (left < right) {
    volume = right;
    pan = static_cast<int>((1.0f - static_cast<float>(left) / static_cast<float>(right)) * 1024.0f * 0.5f);
    if (front < back) pan = 0x400 - pan;
  } else {
    volume = left;
    const int p = static_cast<int>((1.0f - static_cast<float>(right) / static_cast<float>(left)) * 1024.0f * 0.5f);
    pan = -p;
    if (front < back) pan = p - 0x400;
  }
  pan += 0x200;
  if (0x400 < volume) volume = 0x400;
  if (volume < 0) volume = 0;
}

// 800AEADC
int cBoxX::MappedEvent(ERSoundEvent* event, int source, float time) {
  return Event(event->m_type, 0, source, event, time);
}

// 800AEB0C
// The director's messages (see gamesound.hpp). `param` is the message's value, or for the getters
// (0x28..0x2A, 0x2D) where to store it.
int cBoxX::Event(int msg, intptr_t param, int source, ERSoundEvent* event, float time) {
  std::lock_guard<std::recursive_mutex> lock(g_eventMutex);
  if (msg == 0 || (m_enabled == 0 && msg != 0x22) || _pApp->m_movieState != 0) return 1;
  if (!g_bBoxXIsInitted) return 0;
  g_pHitMan->m_cache->CleanupIdleTracks();
  g_pHitMan->m_cache->UpdateDuckingPriorities();
  if (msg == 0x17) {  // kill the source's sounds
    if (source == 0) return 0;
    KillSource(source);
    return 1;
  }
  if (m_modes.m_mode != cSoundPlayer::kLoading) {
    const int speed = cAudioInfo::CurrentSimSpeed();
    if (speed != lSimSpeed) lSimSpeed = speed;
  }
  auto get = [&](int value) { *reinterpret_cast<int*>(param) = value; };
  auto groupVolume10 = [](int group) {
    int v = g_pHitMan->ControlGroup(group)->m_volume * 10;
    if (v < 0) v += 0x3FF;
    return v >> 10;
  };
  switch (msg) {
    case 0x1E:  // kill the source's sounds, then play
      if (!event) return 1;
      KillSource(source);
      // fall through
    case 0x01: {  // play
      if (!event) return 1;
      cTrack* track = g_pHitMan->m_cache->GetTrackObject(event, true);
      if (!track) break;
      int paused;
      if (track->GetControlGroup() == 2) {
        paused = m_musicPaused;
      } else {
        bool social = false;
        for (int i = 0; i < 2; ++i)
          if (SocialMode(i)) {
            social = true;
            break;
          }
        if (Globs_pSimulator && Globs_pSimulator->GetSpeed() != 0 && social) return 1;
        paused = m_sfxPaused;
      }
      if (paused != 0) break;
      if (track->m_playing == 0) m_instances.erase(event);  // a stopped event's old sources
      simmy::SoundTrace("event %s (%08X) plays: group %d, source %d", event->GetName(), event->m_id,
                        event->m_group, source);
      if (track->GetArgsType() == 0 || source == 0) {
        track->PlayPause(time, source, 0x400, 0x200);
      } else {
        int l = 0, r = 0, f = 0, b = 0, volume = 0, pan = 0x200;
        GetSndobSurroundVols(event, l, r, f, b);
        ConvertSurroundToVolPan(l, r, f, b, volume, pan);
        AddToInstanceMap(event, source);
        track->PlayPause(time, source, volume, pan);
        track->SetVolume(volume);
        track->SetPan(pan);
      }
      track->Unpause();
      break;
    }
    case 0x13: _pActualAudio->StopMusic(); break;
    case 0x14:  // quiet all
      simmy::SoundTrace("quiet all (mode %d)", m_modes.m_mode);
      g_pHitMan->m_cache->KillAll();
      if (Globs_pSound) Globs_pSound->KillAllEvent();
      m_instances.clear();
      if (m_score) m_score->Stop();
      m_quietTime = m_timer->m_count;
      UpdateAllSndobVolPan();
      break;
    case 0x15: Pause(); break;
    case 0x16: Unpause(); break;
    case 0x22:  // enable
      m_enabled = param != 0 ? 1 : 0;
      if (!m_enabled) g_pHitMan->m_cache->KillAll();
      break;
    case 0x24: {  // game mode
      const int old = m_modes.m_mode;
      simmy::SoundTrace("game mode %d -> %d", old, static_cast<int>(param));
      // PC: the effects fade-out a load starts (cSoundModeManager::UpdateLoadMusic, ~22 frames) ends with
      // "quiet all" (0x14), which also stops the ambient score. On GC the loading screen outlasted the fade;
      // here a house can load in fewer frames, and the late "quiet all" would stop the score live mode has
      // just started. So the fade is finished first, in the loading mode, as it was on GC.
      if (static_cast<int>(param) == cSoundPlayer::kLive && old == cSoundPlayer::kLoading)
        for (int i = 0; i < 0x40 && m_modes.m_sfxFadingOut; ++i) m_modes.UpdateSFXVolume();
      m_modes.SetMode(static_cast<int>(param));
      SetAmbientScore(old);
      break;
    }
    case 0x25: {  // SFX volume (0..10)
      const int v = static_cast<int>((param << 10) / 10);
      g_pHitMan->ControlGroup(1)->SetVolume(v);
      if (m_score) m_score->SetVolume(v);
      break;
    }
    case 0x26:  // music volume
      g_pHitMan->ControlGroup(2)->SetVolume(static_cast<int>((param << 10) / 10));
      _pActualAudio->SetMusicVolume(static_cast<float>(param) / 10.0f);
      break;
    case 0x27: g_pHitMan->ControlGroup(3)->SetVolume(static_cast<int>((param << 10) / 10)); break;  // VOX
    case 0x28: get(groupVolume10(1)); break;
    case 0x29: get(groupVolume10(2)); break;
    case 0x2A: get(groupVolume10(3)); break;
    case 0x2B: g_pHitMan->ControlGroup(1)->SetVolume(static_cast<int>((param << 10) / 10)); break;  // old FX
    case 0x2C:
      if (m_score) m_score->SetVolume(static_cast<int>((param << 10) / 10));
      break;
    case 0x2D:
      if (m_score) {
        int v = static_cast<int>(m_score->GetVolume()) * 10;
        if (v < 0) v += 0x3FF;
        get(v >> 10);
      }
      break;
    case 0x2E: return 1;
    case 0x2F: PauseAmbient(); break;
    case 0x30: UnpauseAmbient(); break;
    case 0x31: PauseSFX(); break;
    case 0x32: UnpauseSFX(); break;
    case 0x33: PauseMusic(); break;
    case 0x34: UnpauseMusic(); break;
    default: break;
  }
  UpdateAllSndobVolPan();
  return 1;
}

// 800AF4B4
// The ambient score follows the game mode: started in live mode ("score<house number>"), paused in the
// silent mode, resumed after it, stopped elsewhere.
void cBoxX::SetAmbientScore(int oldMode) {
  const int mode = m_modes.m_mode;
  if (oldMode == mode) return;
  if (mode == cSoundPlayer::kCAS) {
    if (oldMode == cSoundPlayer::kSilent) UnpauseAmbient();
    return;
  }
  if (mode == cSoundPlayer::kLive) {
    if (oldMode == cSoundPlayer::kSilent) {
      UnpauseAmbient();
      return;
    }
    if (!_globals.m_house || !m_score) return;
    char name[36];
    std::snprintf(name, sizeof name, "score%d", _globals.m_house->m_house);
    m_score->Load(name);
    m_score->Start();
    m_score->SetPause(m_ambientPaused != 0);
    return;
  }
  if (mode == cSoundPlayer::kSilent) {
    if (oldMode == cSoundPlayer::kLive || oldMode == cSoundPlayer::kCAS) PauseAmbient();
    return;
  }
  if (m_score) m_score->Shutdown();
}

// 800AF5BC
void cBoxX::SetAmbientScoreVolume(int volume) {
  if (m_score) m_score->SetVolume(volume);
}

// 800AF5E8
int cBoxX::GetAmbientScoreVolume() { return m_score ? static_cast<int>(m_score->GetVolume()) : 0; }

// 800AF61C
std::multimap<ERSoundEvent*, int>::iterator cBoxX::FindSndobInstancePair(ERSoundEvent* event, int source) {
  auto range = m_instances.equal_range(event);
  for (auto it = range.first; it != range.second; ++it)
    if (it->second == source) return it;
  return m_instances.end();
}

// 800AF72C
void cBoxX::AddToInstanceMap(ERSoundEvent* event, int source) {
  if (FindSndobInstancePair(event, source) == m_instances.end()) m_instances.insert({event, source});
}

// 800AF7A4
// In live mode, re-pans every positional event from all its sources; then caps the number of sounds.
void cBoxX::UpdateAllSndobVolPan() {
  if (cSoundPlayer::GetGameMode() == cSoundPlayer::kLive) {
    for (auto it = m_instances.begin(); it != m_instances.end();) {
      auto last = m_instances.upper_bound(it->first);
      UpdateSndobVolPan(it, last);
      it = last;
    }
  }
  CheckForTooManySounds();
}

// 800AF8D8
// More than 23 running tracks: the quietest non-music one stops.
void cBoxX::CheckForTooManySounds() {
  if (static_cast<int>(g_pHitMan->m_updateList.size()) <= 0x17) return;
  cTrack* quietest = nullptr;
  int volume = 0x400;
  for (cTrack* t : g_pHitMan->m_updateList) {
    if (!g_pHitMan->m_cache->IsInMemory(t) || t->GetControlGroup() == 2) continue;
    const int v = t->CalculateCurrentVolume();
    if (v < volume) {
      quietest = t;
      volume = v;
    }
  }
  if (quietest) quietest->Stop();
}

// 800AF9F8
// Forgets the source's positional instances (re-panning or stopping those events) and its queued events.
void cBoxX::KillSource(int source) {
  for (auto it = m_instances.begin(); it != m_instances.end();) {
    if (it->second == source) {
      ERSoundEvent* event = it->first;
      it = m_instances.erase(it);
      UpdateSndobVolPan(event);
    } else {
      ++it;
    }
  }
  if (Globs_pSound) Globs_pSound->KillSourceEvent(source);
}

// 800AFB00
// The event's track stops when it has no source left, else it is re-panned.
void cBoxX::UpdateSndobVolPan(ERSoundEvent* event) {
  auto range = m_instances.equal_range(event);
  if (range.first == range.second) {
    if (cTrack* track = g_pHitMan->m_cache->GetTrackObject(event, false)) track->Stop();
  } else {
    UpdateSndobVolPan(range.first, range.second);
  }
}

// 800AFBD8
// (Music is left alone while the mode manager fades it.)
void cBoxX::UpdateSndobVolPan(std::multimap<ERSoundEvent*, int>::iterator first,
                              std::multimap<ERSoundEvent*, int>::iterator) {
  if (cSoundPlayer::GetGameMode() != cSoundPlayer::kLive) return;
  ERSoundEvent* event = first->first;
  cTrack* track = g_pHitMan->m_cache->GetTrackObject(event, false);
  if (!track) return;
  int l = 0, r = 0, f = 0, b = 0, volume = 0, pan = 0x200;
  GetSndobSurroundVols(event, l, r, f, b);
  ConvertSurroundToVolPan(l, r, f, b, volume, pan);
  const bool fading = m_modes.m_musicFadingOut != 0 || m_modes.m_musicFadingIn != 0;
  if (!fading || track->GetControlGroup() != 2) {
    track->SetVolume(volume);
    track->SetPan(pan);
  }
}

// 800AFCC4
// The event's gains summed over its sources (music: by distance only, centred).
int cBoxX::GetSndobSurroundVols(ERSoundEvent* event, int& left, int& right, int& front, int& back) {
  left = right = front = back = 0;
  auto range = m_instances.equal_range(event);
  cTrack* track = g_pHitMan->m_cache->GetTrackObject(event, false);
  if (!track) return 0;
  for (auto it = range.first; it != range.second; ++it) {
    int l = 0, r = 0, f = 0, b = 0;
    if (track->GetControlGroup() == 2) {
      GetInstanceVolNoPan(it->second, track, l);
      if (l < 1) l = 1;
      front = 0x400;
      r = l;
      back = 0;
    } else {
      GetInstanceSurroundVol(it->second, track, l, r, f, b);
    }
    left += l;
    right += r;
    front += f;
    back += b;
  }
  if (left + right + front + back == 0) {
    front = 1;
    right = 1;
    left = 1;
  }
  return 1;
}

// 800AFEE8
void cBoxX::Pause() {
  PauseAmbient();
  PauseSFX();
  PauseMusic();
}

// 800AFF24
void cBoxX::Unpause() {
  UnpauseAmbient();
  UnpauseSFX();
  UnpauseMusic();
}

// 800AFF60
void cBoxX::PauseSFX() {
  ++m_sfxPaused;
  g_pHitMan->m_cache->PauseGroup(0);
  g_pHitMan->m_cache->PauseGroup(1);
  g_pHitMan->m_cache->PauseGroup(3);
  g_pHitMan->DuckMapRemoveAll();
}

// 800AFFC0
void cBoxX::UnpauseSFX() {
  if (m_sfxPaused == 0 || --m_sfxPaused == 0) {
    g_pHitMan->m_cache->UnpauseGroup(0);
    g_pHitMan->m_cache->UnpauseGroup(1);
    g_pHitMan->m_cache->UnpauseGroup(3);
  }
}

// 800B0028
void cBoxX::PauseAmbient() {
  ++m_ambientPaused;
  if (m_score) m_score->SetPause(true);
}

// 800B0068
void cBoxX::UnpauseAmbient() {
  if ((m_ambientPaused == 0 || --m_ambientPaused == 0) && m_score) m_score->SetPause(false);
}

// 800B00B4
void cBoxX::PauseMusic() {
  ++m_musicPaused;
  g_pHitMan->m_cache->PauseGroup(2);
  m_modes.Pause();
}

// 800B0100
void cBoxX::UnpauseMusic() {
  if (m_musicPaused == 0 || --m_musicPaused == 0) {
    g_pHitMan->m_cache->UnpauseGroup(2);
    m_modes.Unpause();
  }
}

// 800B015C
// The ear: g_cam_dist along the view from the eye and g_cam_height below it (not below the ground); the
// view direction. Without a camera: the origin, looking along +y.
void cBoxX::GetListenerPosAndDir(ESimsCam* cam, EVec3& pos, EVec3& dir) {
  if (!cam) {
    pos = EVec3(0.0f, 0.0f, 0.0f);
    dir = EVec3(0.0f, 1.0f, 0.0f);
    return;
  }
  EVec3 v(cam->m_target.x - cam->m_eye.x, cam->m_target.y - cam->m_eye.y, cam->m_target.z - cam->m_eye.z);
  if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f) Normalize(v);
  dir = v;
  pos.x = cam->m_eye.x + v.x * g_cam_dist;
  pos.y = cam->m_eye.y + v.y * g_cam_dist;
  pos.z = cam->m_eye.z + v.z * g_cam_dist - g_cam_height;
  if (pos.z < 0.0f) pos.z = 0.0f;
}

// ---------------------------------------------------------------------------------------------------
// cSoundModeManager

// 800B0320
cSoundModeManager::cSoundModeManager() = default;

// 800B0364
void cSoundModeManager::Shutdown() { _pActualAudio->StopMusic(); }

// 800B039C
void cSoundModeManager::Pause() {
  if (m_mode != cSoundPlayer::kLoading) {
    _pActualAudio->PauseMusic();
    _pActualAudio->Flush(true);
  }
}

// 800B0400
void cSoundModeManager::Unpause() {
  _pActualAudio->ResumeMusic();
  _pActualAudio->Flush(true);
}

// 800B0458
void cSoundModeManager::FadeOutMusic(unsigned int frames) {
  m_musicFadeStep = frames;
  m_musicFadingOut = 1;
}
// 800B0468
void cSoundModeManager::FadeInMusic(unsigned int frames) {
  m_musicFadeStep = frames;
  m_musicFadingIn = 1;
}
// 800B0478
void cSoundModeManager::FadeOutSFX(unsigned int frames) {
  m_sfxFadeStep = static_cast<int>(frames);
  m_sfxFadingOut = 1;
}
// 800B0488
void cSoundModeManager::FadeInSFX(unsigned int frames) {
  m_sfxFadeStep = static_cast<int>(frames);
  m_sfxFadingIn = 1;
}

// 800B0498
// Entering the live, loading or silent mode stops music that was fading out (or fades build/buy music
// out when going back to live mode).
void cSoundModeManager::SetMode(int mode) {
  const int old = m_mode;
  if (mode == old) return;
  m_mode = mode;
  if (mode != cSoundPlayer::kLoading && mode != cSoundPlayer::kLive && mode != cSoundPlayer::kSilent) return;
  if (m_musicFadingOut == 0) {
    if (old == cSoundPlayer::kBuildBuy && m_mode == cSoundPlayer::kLive) FadeOutMusic(0x1E);
  } else {
    _pActualAudio->StopMusic();
    _pActualAudio->Flush(true);
    m_musicFadingOut = 0;
  }
}

// 800B055C
// The music volume follows the option (group 2 and EAudio's music; CAS music at 65%), or steps the
// fades: out to silence (then the music stops) or in up to the option.
void cSoundModeManager::UpdateMusicVolume() {
  if (!_pActualAudio->IsPlayingMusic()) return;
  float volume;
  if (m_musicFadingOut == 0) {
    if (m_musicFadingIn == 0) {
      const int v = (Option(0x1D) << 10) / 10;
      if (g_pHitMan->ControlGroup(2)->m_volume == v) return;
      g_pHitMan->ControlGroup(2)->SetVolume(v);
      float f = static_cast<float>(v) * (1.0f / 1024.0f);
      if (m_mode == cSoundPlayer::kCAS) f *= 0.65f;
      _pActualAudio->SetMusicVolume(f);
      return;
    }
    volume = static_cast<float>(m_musicFadeStep) * (1.0f / 1024.0f) + _pActualAudio->GetMusicVolume();
    const int option = Option(0x1D);
    if (static_cast<float>(option) / 10.0f <= volume) {
      m_musicFadingIn = 0;
      volume = static_cast<float>(option << 10) / 10.0f;  // (sic: 0..0x400, clamped to 1 by EAudio)
    }
  } else {
    volume = _pActualAudio->GetMusicVolume() - static_cast<float>(m_musicFadeStep) * (1.0f / 1024.0f);
    if (volume <= 0.0f) {
      m_musicFadingOut = 0;
      _pActualAudio->StopMusic();
      _pActualAudio->Flush(true);
      return;
    }
  }
  _pActualAudio->SetMusicVolume(volume);
}

// 800B08AC
// The SFX and VOX groups and the ambient score follow the options, or step the SFX fades (all quiet at
// the end of a fade-out).
void cSoundModeManager::UpdateSFXVolume() {
  const int sfx = g_pHitMan->ControlGroup(1)->m_volume;
  const int vox = g_pHitMan->ControlGroup(3)->m_volume;
  int ambient = g_pBoxX->GetAmbientScoreVolume();
  int newSfx = sfx, newVox = vox;
  bool change = false;
  if (m_sfxFadingOut == 0) {
    if (m_sfxFadingIn == 0) {
      const int optSfx = (Option(0x1C) << 10) / 10;
      const int optVox = (Option(0x28) << 10) / 10;
      newSfx = optSfx;
      newVox = optVox;
      change = vox != optVox || sfx != optSfx;
      const int optAmbient = (Option(0x29) << 10) / 10;
      if (ambient == optAmbient) {
        if (!change) return;
      } else {
        change = true;
        ambient = optAmbient;
      }
    } else {
      const int step = m_sfxFadeStep;
      int done = 0;
      const int optSfx = (Option(0x1C) << 10) / 10;
      newSfx = sfx + step;
      if (optSfx <= newSfx) {
        done |= 1;
        newSfx = optSfx;
      }
      const int optVox = (Option(0x28) << 10) / 10;
      newVox = vox + step;
      if (optVox <= newVox) {
        done |= 2;
        newVox = optVox;
      }
      const int optAmbient = (Option(0x29) << 10) / 10;
      ambient += step;
      if (optAmbient <= ambient) {
        done |= 4;
        ambient = optAmbient;
      }
      if (done == 7) m_sfxFadingIn = 0;
      change = true;
    }
  } else {
    const int step = m_sfxFadeStep;
    newSfx = sfx - step;
    if (newSfx < 1) newSfx = 0;
    newVox = vox - step;
    if (newVox < 1) newVox = 0;
    ambient -= step;
    if (ambient < 1) ambient = 0;
    if (newVox * 2 + ambient < 1) {
      m_sfxFadingOut = 0;
      simmy::SoundTrace("effects faded out (mode %d)", m_mode);
      g_pBoxX->Event(0x14, 0, 0, nullptr, 0.0f);
    }
    change = true;
  }
  if (change) {
    g_pHitMan->ControlGroup(1)->SetVolume(newSfx);
    g_pHitMan->ControlGroup(3)->SetVolume(newVox);
    g_pBoxX->SetAmbientScoreVolume(ambient);
  }
}

// 800B0B00
// The loading screen's loop ("load_loop1".."load_loop10", or the elevator's for houses 3 and 12) on a
// voice of its own at the SFX volume, after the music and effects fade out; released when loading ends.
void cSoundModeManager::UpdateLoadMusic() {
  if (m_mode == cSoundPlayer::kLoading && !_app.IsMovieStarting() && !_app.IsMoviePlaying()) {
    if (!m_loadVoice) {
      if (_pActualAudio->IsPlayingMusic()) {
        FadeOutMusic(0x30);
        FadeOutSFX(0x30);
      }
      m_loadVoice = _pActualAudio->AudioAllocVoice();
      if (m_loadVoice && m_loadSampleId == 0) {
        const int house = _globals.m_house ? _globals.m_house->m_house : 1;
        const char* loop = (house == 3 || house == 0xC) ? "load_loop_elevator" : "load_loop";
        int r = simmy::ui_rand();
        if (r < 0) r += 3;
        char name[64];
        std::snprintf(name, sizeof name, "%s%d", loop, (r >> 2) % 10 + 1);
        m_loadSampleId = EResourceManager::CalcId(name);
        simmy::SoundTrace("loading loop %s", name);
        _pAudiosampleman->AddRefAsync(m_loadSampleId);
      }
    } else if (!m_loadSample) {
      m_loadSample = static_cast<ERSampledata*>(_pAudiosampleman->GetRef(m_loadSampleId));
      if (m_loadSample) {
        _pActualAudio->AudioBindVoice(m_loadVoice, m_loadSampleId);
        m_loadSampleId = 0;
        const float v = static_cast<float>(g_pHitMan->ControlGroup(1)->m_volume) * (1.0f / 1024.0f);
        EVoiceDesc desc;
        desc.m_mask = 0xB;
        desc.m_volumeLeft = v;
        desc.m_volumeRight = v;
        desc.m_play = 1;
        _pActualAudio->SetVoiceState(m_loadVoice, desc);
      }
    }
    return;
  }
  if (m_loadVoice) {
    _pActualAudio->UnbindVoice(m_loadVoice);
    _pActualAudio->FreeVoice(m_loadVoice);
    m_loadVoice = nullptr;
  }
  if (!m_loadSample) {
    if (m_loadSampleId) {
      _pAudiosampleman->DelRefAsync(m_loadSampleId);
      m_loadSampleId = 0;
    }
  } else {
    m_loadSample->DelRef();
    m_loadSample = nullptr;
  }
}

// 800B0E44
const char* cSoundModeManager::GetFrontEndMusicTrack(bool current) {
  return current ? s_frontEndTracks[m_frontEndTrack] : s_frontEndTracks[0];
}
// 800B0E70
void cSoundModeManager::NextFrontEndTrack() {
  if (++m_frontEndTrack >= 4) m_frontEndTrack = 0;
}
// 800B0E90
void cSoundModeManager::PrevFrontEndTrack() {
  if (--m_frontEndTrack < 0) m_frontEndTrack = 3;
}
// 800B0EB0
void cSoundModeManager::StartFrontEndMusic(bool current) {
  StartMusicTrack(const_cast<char*>(GetFrontEndMusicTrack(current)), 1.0f);
}
// 800B0EF0
const char* cSoundModeManager::GetDemoMusicTrack() { return s_frontEndTracks[m_demoTrack]; }
// 800B0F08
void cSoundModeManager::NextDemoMusicTrack() {
  if (++m_demoTrack >= 3) m_demoTrack = 1;
}
// 800B0F28
void cSoundModeManager::StartDemoMusic() {
  StartMusicTrack(const_cast<char*>(GetDemoMusicTrack()), 1.0f);
  NextDemoMusicTrack();
}

// 800B0F70
// Plays the stream once (not looped; Update starts the next when it ends) at `volume` of the music option
// (unless it fades in).
void cSoundModeManager::StartMusicTrack(char* name, float volume) {
  m_musicFadingOut = 0;
  EPMDesc desc(name, false);
  simmy::SoundTrace("mode %d music %s", m_mode, name);
  _pActualAudio->PlayMusic(desc);
  if (m_musicFadingIn == 0)
    _pActualAudio->SetMusicVolume((volume * static_cast<float>(Option(0x1D))) / 10.0f);
}

// 800B104C
// music_24_bbcas1..2 in turn (the first one at random).
void cSoundModeManager::StartBuildBuyMusic() {
  if (m_bbcasTrack == 0) m_bbcasTrack = (simmy::ui_rand() % 0x60 >> 5) + 1;
  else if (2 < ++m_bbcasTrack) m_bbcasTrack = 1;
  StartMusicTrack(const_cast<char*>(s_frontEndTracks[m_bbcasTrack]), 1.0f);
}

// 800B10F0
void cSoundModeManager::StartCASMusic() {
  if (m_bbcasTrack == 0) m_bbcasTrack = (simmy::ui_rand() % 0x60 >> 5) + 1;
  else if (2 < ++m_bbcasTrack) m_bbcasTrack = 1;
  StartMusicTrack(const_cast<char*>(s_frontEndTracks[m_bbcasTrack]), 1.0f);
}

// 800B1194
// Once the game has started (GameData +0x88) and unless sounds are paused (build/buy pauses them but
// keeps its music): volumes, the loading loop, and the mode's music when none plays (front end: the
// title track; CAS and build/buy: bbcas; live: only the demo's). The silent mode and background saves
// stop music.
void cSoundModeManager::Update() {
  if (_gd.m_drawTopmost == 0) return;
  if (Globs_pSound && Globs_pSound->m_paused != 0 && m_mode != cSoundPlayer::kBuildBuy) return;
  UpdateMusicVolume();
  UpdateSFXVolume();
  UpdateLoadMusic();
  if (m_mode == cSoundPlayer::kSilent || BackgroundSaveInProgressForSound() != 0) {
    if (_pActualAudio->IsPlayingMusic()) _pActualAudio->StopMusic();
    return;
  }
  if (Option(0x1D) == 0 || _pActualAudio->IsPlayingMusic()) return;
  switch (m_mode) {
    case cSoundPlayer::kFrontEnd: StartFrontEndMusic(false); break;
    case cSoundPlayer::kLive:
      if (g_demoMusic != 0 && _globals.m_house && _globals.m_house->m_house == 9) StartDemoMusic();
      break;
    case cSoundPlayer::kCAS: StartCASMusic(); break;
    case cSoundPlayer::kBuildBuy: StartBuildBuyMusic(); break;
    default: break;
  }
}

// 800B131C __static_initialization_and_destruction_0 (g_eventMutex)
// 800B1394 global constructors keyed to g_demoMusic
// 800B13C0 global destructors keyed to g_demoMusic
