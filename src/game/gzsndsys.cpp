// Port of game/gzsndsys.cpp: the sound system under the BoxX director. cGZSnd plays a sample through an
// EAudio voice once the sample cache (cSoundCacheItem) has it loaded; cGZMusic plays an audiostreams entry
// as EAudio's music (one at a time, with fades). Volumes and pans are 0..0x400.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

cGZSndSys* _gzSndSys = nullptr;
cIGZSndSys* g_pSndSys = nullptr;
int _iNumWaitingOnLoad = 0;

namespace {
// timeGetTime: milliseconds.
// (On the game's clock: SIMMY_DET makes it virtual.)
unsigned int TimeGetTime() {
  static const unsigned long long start = simmy::game_clock_ns();
  return static_cast<unsigned int>((simmy::game_clock_ns() - start) / 1000000ull);
}
float Now() { return static_cast<float>(_pClockMan->GetSec(&_sysclock)); }

// PC: samples.arc may be absent (it is not on every copy of the data). Then no sample can load; the cache
// item gives up instead of waiting for ever, so tracks and UI voices do not stay "playing".
bool SamplesAvailable() {
  static const bool ok = [] {
    const bool open = _pAudiosampleman && _pAudiosampleman->GetArchiveFile() != nullptr;
    if (!open) std::fprintf(stderr, "[audio] samples.arc not found: sound effects and voices are silent\n");
    return open;
  }();
  return ok;
}

void SetVoiceVolume(EVoice* voice, float left, float right) {
  EVoiceDesc desc;
  desc.m_mask = 3;
  desc.m_volumeLeft = left;
  desc.m_volumeRight = right;
  _pActualAudio->SetVoiceState(voice, desc);
}
}  // namespace

// 800B8C88
cIGZSndSys* cIGZSndSys::CreateInstance() {
  _gzSndSys = new cGZSndSys;
  return _gzSndSys;
}

// 800B8E4C
cIGZSndSys::~cIGZSndSys() = default;

// ---------------------------------------------------------------------------------------------------
// cSoundCacheItem

// 800B8CB8
cSoundCacheItem::cSoundCacheItem(unsigned int id) : m_id(id) {}

// 800B8CC0
void cSoundCacheItem::ResetUsageTimer() { m_lastUsed = TimeGetTime(); }

// 800B8CF0
// Starts the load (at most four in flight), then waits for it; a sample that cannot be cached is
// released and tried again 15 frames later.
void cSoundCacheItem::Update() {
  if (m_loaded) return;
  if (!m_waiting) {
    if (m_retry < 0) return;  // PC: cannot load (no samples.arc)
    if (m_retry != 0) {
      m_retry = m_retry - 1;
      return;
    }
    if (_iNumWaitingOnLoad < 4) {
      if (!SamplesAvailable()) {
        m_retry = -16;
        return;
      }
      ++_iNumWaitingOnLoad;
      m_waiting = 1;
      _pAudiosampleman->AddRefAsync(m_id);
      return;
    }
  } else {
    EResource* sample = _pAudiosampleman->GetRef(m_id);
    if (!sample) {
      // PC: an id the archive does not hold never arrives.
      if (!_pAudiosampleman->IsValid(m_id)) {
        m_waiting = 0;
        --_iNumWaitingOnLoad;
        m_retry = -16;
      }
      return;
    }
    m_waiting = 0;
    --_iNumWaitingOnLoad;
    if (_pAudiosampleman->CanCache(sample)) {
      m_loaded = 1;
      simmy::SoundTrace("sample %08X loaded", m_id);
      return;
    }
    sample->DelRef();
  }
  m_retry = 0xF;
}

// 800B8DF0
cSoundCacheItem::~cSoundCacheItem() {
  if (m_loaded) _pAudiosampleman->DelRef(m_id, EResourceManager__DelRefMode{EResourceManager__DelRefMode::Cache});
}

// ---------------------------------------------------------------------------------------------------
// cGZSndSys

// 800B8E80
cGZSndSys::cGZSndSys() = default;

// 800B8EE8
// (GC: also flushed the resource loader, _pResLoader vtable 0x3C, before the last update.)
cGZSndSys::~cGZSndSys() {
  killDeadSounds();
  _gzSndSys = nullptr;
  m_musics = nullptr;
  m_sounds = nullptr;
  unloadMusic();
  Update();
  for (cSoundCacheItem*& bucket : m_buckets) {
    cSoundCacheItem* item = bucket;
    bucket = nullptr;
    while (item) {
      cSoundCacheItem* next = item->m_next;
      delete item;
      item = next;
    }
  }
}

// 800B8FBC
int cGZSndSys::Initialize() {
  preloadMusic();
  return 1;
}

// 800B8FE0
cIGZSnd* cGZSndSys::CreateSoundEffect(unsigned int sample, float time) {
  auto* snd = new cGZSnd;
  snd->m_sample = sample;
  snd->m_startTime = time;
  return snd;
}

// 800B9038
cIGZSnd* cGZSndSys::CreateAudioStream(unsigned int stream, bool loop, float time) {
  auto* music = new cGZMusic;
  music->m_stream = stream;
  music->m_loop = loop ? 1 : 0;
  music->m_startTime = time;
  return music;
}

// 800B9098
void cGZSndSys::addToList(cGZSnd* snd) {
  snd->m_next = m_sounds;
  m_sounds = snd;
}

// 800B90A8
void cGZSndSys::removeFromList(cGZSnd* snd) {
  cGZSnd* prev = nullptr;
  for (cGZSnd* s = m_sounds; s && s != snd; s = s->m_next) prev = s;
  if (!prev) m_sounds = snd->m_next;
  else prev->m_next = snd->m_next;
}

// 800B90E8
void cGZSndSys::addToList(cGZMusic* music) {
  music->m_next = m_musics;
  m_musics = music;
}

// 800B90F8
void cGZSndSys::removeFromList(cGZMusic* music) {
  cGZMusic* prev = nullptr;
  for (cGZMusic* m = m_musics; m && m != music; m = m->m_next) prev = m;
  if (!prev) m_musics = music->m_next;
  else prev->m_next = music->m_next;
  if (m_currentMusic == music) m_currentMusic = nullptr;
}

// 800B914C
void cGZSndSys::FreeUnusedSounds() {
  if (!_gzSndSys) return;
  _gzSndSys->killDeadSounds();
  _gzSndSys->ProcessCache(0);
}

// 800B9184
// Deletes the sounds nobody holds any more.
void cGZSndSys::killDeadSounds() {
  std::vector<cGZSnd*> dead;
  for (cGZSnd* s = m_sounds; s; s = s->m_next)
    if (s->m_refs == 0) dead.push_back(s);
  for (cGZSnd* s : dead) delete s;
}

// 800B93A8
// Unused samples (no sound and no load in flight) idle for `idleMs` are released; the others' loads are
// advanced. True when one finished loading.
int cGZSndSys::ProcessCache(unsigned int idleMs) {
  int loaded = 0;
  std::vector<cSoundCacheItem*> idle;
  const unsigned int now = TimeGetTime();
  for (cSoundCacheItem* bucket : m_buckets) {
    for (cSoundCacheItem* item = bucket; item; item = item->m_next) {
      if ((item->m_waiting == 0 || !_gzSndSys) && item->m_users == 0) {
        if (idleMs <= now - item->m_lastUsed) idle.push_back(item);
      } else if (!item->m_loaded) {
        item->Update();
        if (item->m_loaded) loaded = 1;
      }
    }
  }
  for (cSoundCacheItem* item : idle) {
    cSoundCacheItem*& head = m_buckets[item->m_id & 0xFF];
    if (head == item) {
      head = item->m_next;
    } else {
      for (cSoundCacheItem* p = head; p; p = p->m_next)
        if (p->m_next == item) {
          p->m_next = item->m_next;
          break;
        }
    }
    delete item;
  }
  return loaded;
}

// 800B9824
// Starts sounds whose sample has just loaded, runs the sounds' volume fades (deleting released ones when
// faded out), and runs the music: its volume ramps, the stop fade, and restarting the music that wants to
// play once the stream is free.
void cGZSndSys::Update() {
  // GC: only after a load finished (ProcessCache true). PC: also once a sound's start time (its event's
  // time + 15 ms) has come: on GC the rest of the frame took longer than that between the event and this
  // update, here a fast frame can reach Play first and the sound would wait for some other sample's load.
  const bool loaded = ProcessCache(10) != 0;
  const float now = Now();
  for (cGZSnd* s = m_sounds; s; s = s->m_next)
    if (s->m_waitingToPlay && s->m_item && s->m_item->m_loaded && s->m_playRequested &&
        (loaded || s->m_startTime <= now))
      s->Play();
  std::vector<cGZSnd*> dead;
  for (cGZSnd* s = m_sounds; s; s = s->m_next) {
    if (!s->m_voice) continue;
    if (s->m_fadeFrames == 0) {
      if (s->m_refs == 0) dead.push_back(s);
      continue;
    }
    const int left = --s->m_fadeFrames;
    if (left < 1) {
      s->m_fadeFrames = 0;
      s->setVolume(s->m_volume);
      if (s->m_refs == 0) dead.push_back(s);
      continue;
    }
    const int diff = s->m_volume - s->m_currentVolume;
    if (diff == 0) {
      s->m_fadeFrames = 0;
      s->setVolume(s->m_volume);
      continue;
    }
    const int step = diff < 0 ? (diff + 1) - left : diff - 1 + left;
    s->m_currentVolume += step / s->m_fadeFrames;
    if (s->m_voice) {
      float l = 0.0f, r = 0.0f;
      s->getLRVolume(s->m_currentVolume, l, r);
      SetVoiceVolume(s->m_voice, l, r);
    }
  }
  for (cGZSnd* s : dead) delete s;

  if (m_musicFadeOut == 0 || (m_musicStopping == 0 && m_musics == nullptr)) {
    for (cGZMusic* m = m_musics; m; m = m->m_next) {
      if (m->m_fadeFrames == 0) continue;
      const int left = --m->m_fadeFrames;
      if (left < 1) {
        m->m_fadeFrames = 0;
        m->setVolume(m->m_volume);
      } else if (m == m_currentMusic) {
        const float current = _pActualAudio->GetMusicVolume();
        const float target = static_cast<float>(m->m_volume) * (1.0f / 1024.0f);
        float v;
        if (current <= target) {
          v = current + 0.66f;
          if (target < v) v = target;
        } else {
          v = current - 1.32f;
          if (v < target) v = target;
        }
        _pActualAudio->SetMusicVolume(v);
      }
    }
    if (m_currentMusic) {
      if (m_musicPaused == 0 && !_pActualAudio->IsPlayingMusic()) {
        m_currentMusic->m_playing = 0;
        m_currentMusic = nullptr;
      }
      if (m_currentMusic) return;
    }
    for (cGZMusic* m = m_musics; m; m = m->m_next) {
      if (m->m_playing) {
        m->m_fadeFrames = 1;
        m->Play();
        return;
      }
    }
  } else {
    if (--m_musicFadeOut < 1) {
      _pActualAudio->SetMusicVolume(0.0f);
      _pActualAudio->SetMusicPan(0.0f);
      _pActualAudio->StopMusic();
      m_musicFadeOut = 0;
      if (cGZMusic* m = m_currentMusic) {
        m->FadeVolume(0, m->m_volume, 0x32);
        m->Play();
      }
    } else {
      float v = _pActualAudio->GetMusicVolume() - 1.32f;
      if (v < 0.0f) v = 0.0f;
      _pActualAudio->SetMusicVolume(v);
    }
  }
}

// 800B9FB0
void cGZSndSys::preloadMusic() {}
// 800B9FB4
void cGZSndSys::unloadMusic() {}

// ---------------------------------------------------------------------------------------------------
// cGZSnd

// 800B9FB8
cGZSnd::cGZSnd() { _gzSndSys->addToList(this); }

// 800BA034
cGZSnd::~cGZSnd() {
  reset();
  _gzSndSys->removeFromList(this);
}

// 800BA090
int cGZSnd::Init() { return 1; }

// 800BA098
// The last release of a sound still playing fades it out first (cGZSndSys::Update deletes it).
int cGZSnd::Release() {
  if (m_refs == 1) {
    if (m_voice) {
      EVoiceDesc desc;
      desc.m_mask = 0;
      _pActualAudio->GetVoiceState(m_voice, desc);
      if (desc.m_play) {
        m_fadeFrames = 1;
        m_volume = 0;
        return --m_refs;
      }
    }
    delete this;
    return 0;
  }
  return --m_refs;
}

// 800BA154
void cGZSnd::AddRef() { ++m_refs; }

// 800BA168
// Finds (or adds) the sample's cache item; once the sample is loaded and the start time has come, binds a
// voice and plays it at the current volume and pan (frozen with pitch 0 while paused).
int cGZSnd::Play() {
  m_playRequested = 1;
  if (!m_item) {
    if (m_sample) {
      cSoundCacheItem*& head = _gzSndSys->m_buckets[m_sample & 0xFF];
      cSoundCacheItem* item = head;
      if (item && item->m_id != m_sample) {
        cSoundCacheItem* prev = item;
        for (item = item->m_next; item && item->m_id != m_sample; item = item->m_next) prev = item;
        if (item) {  // to the front
          prev->m_next = item->m_next;
          item->m_next = head;
          head = item;
        }
      }
      m_item = item;
      if (!item) {
        item = new cSoundCacheItem(m_sample);
        m_item = item;
        item->m_next = head;
        head = item;
      }
    }
    if (m_item) ++m_item->m_users;
  }
  m_waitingToPlay = 0;
  if (!m_item) return 1;
  m_waitingToPlay = (!m_item->m_loaded || Now() < m_startTime) ? 1 : 0;
  if (m_waitingToPlay) return 1;
  m_item->ResetUsageTimer();
  if (!m_voice) {
    m_voice = _pActualAudio->AudioAllocVoice();
    if (!m_voice) return 1;
    _pActualAudio->AudioBindVoice(m_voice, m_sample);
    if (!m_voice) return 1;
  }
  float l = 0.0f, r = 0.0f;
  getLRVolume(m_currentVolume, l, r);
  EVoiceDesc desc;
  desc.m_mask = 0xB;
  desc.m_volumeLeft = l;
  desc.m_volumeRight = r;
  desc.m_play = 1;
  if (m_paused) {
    desc.m_pitch = 0.0f;
    desc.m_mask |= 0xF;
  }
  simmy::SoundTrace("sample %08X plays: volume %d pan %d", m_sample, m_currentVolume, m_pan);
  _pActualAudio->SetVoiceState(m_voice, desc);
  return 1;
}

// 800BA3F8
// Playing: the voice says so; before it has one, while it waits for its sample (unless that failed).
int cGZSnd::IsPlaying() {
  if (!m_voice) {
    if (!m_item || (m_playRequested && m_item->m_retry < -15)) return 0;
    return m_playRequested;
  }
  EVoiceDesc desc;
  desc.m_mask = 0;
  _pActualAudio->GetVoiceState(m_voice, desc);
  return desc.m_play;
}

// 800BA498
int cGZSnd::Stop() {
  if (m_voice) {
    EVoiceDesc desc;
    desc.m_mask = EVoiceDesc::kPlay;
    desc.m_play = 0;
    _pActualAudio->SetVoiceState(m_voice, desc);
    _pActualAudio->UnbindVoice(m_voice);
    _pActualAudio->FreeVoice(m_voice);
    m_voice = nullptr;
  }
  m_playRequested = 0;
  return 1;
}

// 800BA558
int cGZSnd::Pause() {
  if (m_voice) {
    EVoiceDesc desc;
    desc.m_mask = EVoiceDesc::kPitch;
    desc.m_pitch = 0.0f;
    _pActualAudio->SetVoiceState(m_voice, desc);
  }
  m_paused = 1;
  return 1;
}

// 800BA5D8
int cGZSnd::Unpause() {
  if (m_voice) {
    EVoiceDesc desc;
    desc.m_mask = EVoiceDesc::kPitch;
    desc.m_pitch = 1.0f;
    _pActualAudio->SetVoiceState(m_voice, desc);
  }
  m_paused = 0;
  return 1;
}

// 800BA658
int cGZSnd::Load() { return 1; }
// 800BA660
int cGZSnd::Unload() { return 1; }
// 800BA668
int cGZSnd::GetVolume() { return m_volume; }

// 800BA670
// A playing sound reaches a new volume over one frame.
void cGZSnd::SetVolume(int volume) {
  if (volume != m_volume && m_voice) m_fadeFrames = 1;
  setVolume(volume);
}

// 800BA6B0
int cGZSnd::setVolume(int volume) {
  if (volume < 0) m_volume = 0;
  else if (volume < 0x401) m_volume = volume;
  else m_volume = 0x400;
  if (m_currentVolume != volume && m_fadeFrames == 0) {
    if (m_voice) {
      float l = 0.0f, r = 0.0f;
      getLRVolume(m_volume, l, r);
      SetVoiceVolume(m_voice, l, r);
    }
    m_currentVolume = m_volume;
  }
  return 1;
}

// 800BA79C
// Over `ms` (50 at least; a frame is 33 ms here).
int cGZSnd::FadeVolume(int, int to, unsigned int ms) {
  if (ms < 0x32) ms = 0x32;
  m_fadeFrames = static_cast<int>(static_cast<float>(ms) / 33.0f);
  setVolume(to);
  return 1;
}

// 800BA81C
int cGZSnd::GetPan() { return m_pan; }

// 800BA824
int cGZSnd::SetPan(int pan) {
  if (pan != m_pan) {
    m_pan = pan;
    if (m_voice && m_fadeFrames == 0) {
      float l = 0.0f, r = 0.0f;
      getLRVolume(m_volume, l, r);
      SetVoiceVolume(m_voice, l, r);
    }
  }
  return 1;
}

// 800BA8DC
int cGZSnd::GetFrequency() { return 0x5622; }
// 800BA8E4
int cGZSnd::SetFrequency(int) { return 1; }
// 800BA8EC
int cGZSnd::SetPosition(unsigned int) { return 1; }

// 800BA8F4
void cGZSnd::reset() {
  if (m_voice) {
    _pActualAudio->FreeVoice(m_voice);
    m_voice = nullptr;
  }
  if (m_item) {
    --m_item->m_users;
    m_item = nullptr;
  }
}

// 800BA96C
// Left and right gains for `volume` at the sound's pan. Pans beyond the sides (|p| > 1, a sound behind)
// reflect back and flip the sign of one side's gain, which then stays flipped while the sound remains
// behind (the signs of the last gains are kept).
void cGZSnd::getLRVolume(int volume, float& left, float& right) {
  right = 1.0f;
  left = 1.0f;
  bool flip = false;
  float p = static_cast<float>(m_pan - 0x200) * (1.0f / 512.0f);
  if (p < -1.0f) {
    p = -2.0f - p;
    flip = true;
  } else if (p > 1.0f) {
    p = 2.0f - p;
    flip = true;
  }
  if (p < 0.0f) right += p;
  else left -= p;
  float sl = m_lastLeft >= 0.0f ? 1.0f : -1.0f;
  float sr = m_lastRight >= 0.0f ? 1.0f : -1.0f;
  if (flip != (sl * sr < 0.0f)) {
    if (right < left) sr = -sr;
    else sl = -sl;
  }
  left *= sl;
  right *= sr;
  m_lastLeft = left;
  m_lastRight = right;
  const float g = static_cast<float>(volume) * (1.0f / 1024.0f);
  left *= g;
  right *= g;
}

// ---------------------------------------------------------------------------------------------------
// cGZMusic

// 800BAB28
cGZMusic::cGZMusic() { _gzSndSys->addToList(this); }

// 800BAB9C
cGZMusic::~cGZMusic() {
  reset();
  _gzSndSys->removeFromList(this);
}

// 800BABF8
int cGZMusic::Init() { return 1; }
// 800BAC00
void cGZMusic::AddRef() { ++m_refs; }

// 800BAC14
int cGZMusic::Release() {
  if (m_refs == 1) {
    Stop();
    delete this;
    return 0;
  }
  return --m_refs;
}

// 800BAC94
// Becomes the music when none plays (and the music volume option is not 0): EAudio's stream restarts at
// volume 0 and ramps up (cGZSndSys::Update).
int cGZMusic::Play() {
  m_playing = 1;
  if (_gzSndSys->m_musicPaused) _gzSndSys->m_currentMusic->Stop();
  const auto* options = static_cast<const signed char*>(_globals.m_optionsRecon);
  const int musicOption = options ? options[0x1D] : 10;
  if (!_gzSndSys->m_currentMusic && musicOption != 0 && _gzSndSys->m_musicFadeOut == 0) {
    _gzSndSys->m_currentMusic = this;
    EPMDesc desc(m_stream, false);
    _pActualAudio->StopMusic();
    desc.m_loop = m_loop;
    _pActualAudio->SetMusicVolume(0.0f);
    simmy::SoundTrace("music stream %08X starts%s", m_stream, m_loop ? " (loop)" : "");
    _pActualAudio->PlayMusic(desc);
    m_fadeFrames = 1;
  }
  return 1;
}

// 800BADA4
int cGZMusic::IsPlaying() { return m_playing; }

// 800BADAC
// The current music fades out over a frame (cGZSndSys::Update).
int cGZMusic::Stop() {
  cGZSndSys* sys = _gzSndSys;
  if (sys->m_currentMusic == this) {
    if (m_playing) {
      sys->m_currentMusic = nullptr;
      sys->m_musicStopping = 1;
      sys->m_musicFadeOut = 1;
      simmy::SoundTrace("music stream %08X stops", m_stream);
    }
    sys->m_musicPaused = 0;
  }
  m_playing = 0;
  return 1;
}

// 800BADFC
int cGZMusic::Pause() {
  if (_gzSndSys->m_currentMusic == this) {
    _pActualAudio->PauseMusic();
    _gzSndSys->m_musicPaused = 1;
  }
  return 1;
}

// 800BAE54
int cGZMusic::Unpause() {
  if (_gzSndSys->m_currentMusic == this) {
    _pActualAudio->ResumeMusic();
    _gzSndSys->m_musicPaused = 0;
  }
  return 1;
}

// 800BAEAC
int cGZMusic::Load() { return 1; }
// 800BAEB4
int cGZMusic::Unload() { return 1; }
// 800BAEBC
int cGZMusic::GetVolume() { return m_volume; }

// 800BAEC4
void cGZMusic::SetVolume(int volume) {
  if (volume != m_volume) m_fadeFrames = 1;
  setVolume(volume);
}

// 800BAEF8
// (The comparison with EAudio's 0..1 music volume is the original's.)
int cGZMusic::setVolume(int volume) {
  if (static_cast<float>(volume) != _pActualAudio->GetMusicVolume()) {
    if (volume < 0) m_volume = 0;
    else if (volume < 0x401) m_volume = volume;
    else m_volume = 0x400;
    if (_gzSndSys->m_currentMusic == this && _gzSndSys->m_musicFadeOut == 0 && m_fadeFrames == 0) {
      _pActualAudio->SetMusicVolume(static_cast<float>(getLRVolume()));
      _pActualAudio->SetMusicPan(static_cast<float>(getPanSetting()));
    }
  }
  return 1;
}

// 800BB024
int cGZMusic::FadeVolume(int from, int to, unsigned int ms) {
  m_fadeFrames = 0;
  setVolume(from);
  if (to < 0) m_volume = 0;
  else if (to < 0x401) m_volume = to;
  else m_volume = 0x400;
  m_fadeFrames = static_cast<int>(static_cast<float>(ms) / 33.0f);
  return 1;
}

// 800BB0D0
int cGZMusic::GetPan() { return 0x200; }

// 800BB0D8
int cGZMusic::SetPan(int pan) {
  m_pan = pan;
  cGZMusic* current = _gzSndSys->m_currentMusic;
  if (current == this && _gzSndSys->m_musicFadeOut == 0 && current->m_fadeFrames == 0) {
    _pActualAudio->SetMusicVolume(static_cast<float>(current->getLRVolume()));
    _pActualAudio->SetMusicPan(static_cast<float>(current->getPanSetting()));
  }
  return 1;
}

// 800BB184
int cGZMusic::GetFrequency() { return 0x5622; }
// 800BB18C
int cGZMusic::SetFrequency(int) { return 1; }
// 800BB194
int cGZMusic::SetPosition(unsigned int) { return 1; }

// 800BB19C
void cGZMusic::reset() {
  cGZSndSys* sys = _gzSndSys;
  if (sys && sys->m_currentMusic == this) {
    sys->m_currentMusic->m_playing = 0;
    sys->m_musicPaused = 0;
    sys->m_currentMusic = nullptr;
    _pActualAudio->StopMusic();
  }
}

// 800BB1F4
double cGZMusic::getLRVolume() { return static_cast<float>(m_volume) * (1.0f / 1024.0f); }
// 800BB234
double cGZMusic::getPanSetting() { return static_cast<float>(m_pan - 0x200) * (1.0f / 512.0f); }

// 800BB278 cGZSnd::operator new, 800BB2B4 cGZSnd::operator delete, 800BB2E8 cGZMusic::operator new,
// 800BB324 cGZMusic::operator delete: MainHeap calloc/free (the global heap here; members start zeroed by
// their initialisers).
