// Port of game/gamesound.cpp: cSoundPlayer (Globs::pSound), the game's interface to the sound director.
// Sound events are queued by PlayBySource and handed to cBoxX by Update once their resource has loaded;
// the rest are cBoxX::Event messages. The volume setters also store the options (OptionsRecon).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

cSoundPlayer* Globs_pSound = nullptr;  // Globs::pSound
static int iSavedGameSoundMode = 0;     // iSavedGameSoundMode.4470

namespace {
// PC development aid: SIMMY_SOUND_EVENT="<frames>:<event>[@<source>];..." plays sound events through
// PlayBySource as the game does, <frames> after live mode starts (game mode 1). <event> is a name or
// 0x<id>; <source> an object id, "sim" for player 1's selected sim (default) or "none" (0).
// Example: SIMMY_SOUND_EVENT="200:stereo_pop@sim;600:v_sit_dislike@sim".
struct TestEvent {
  int frame;
  std::string event;
  std::string source;
};
void TestEvents(cSoundPlayer* player) {
  static std::vector<TestEvent> events;
  static bool parsed = false;
  static int liveFrames = -1;
  if (!parsed) {
    parsed = true;
    const char* env = std::getenv("SIMMY_SOUND_EVENT");
    std::string all = env ? env : "";
    for (size_t pos = 0; pos < all.size();) {
      size_t end = all.find(';', pos);
      if (end == std::string::npos) end = all.size();
      const std::string e = all.substr(pos, end - pos);
      pos = end + 1;
      const size_t colon = e.find(':'), at = e.find('@');
      if (colon == std::string::npos) continue;
      TestEvent t;
      t.frame = std::atoi(e.c_str());
      t.event = e.substr(colon + 1, at == std::string::npos ? std::string::npos : at - colon - 1);
      t.source = at == std::string::npos ? "sim" : e.substr(at + 1);
      events.push_back(t);
    }
  }
  if (events.empty()) return;
  if (cSoundPlayer::GetGameMode() == cSoundPlayer::kLive) ++liveFrames;
  for (const TestEvent& t : events) {
    if (t.frame != liveFrames) continue;
    int source = 0;
    if (t.source == "sim") {
      cXPerson* p = _globals.m_selectedPerson[0];
      source = p && p->AsObject() ? p->AsObject()->GetID() : 0;
    } else if (t.source != "none") {
      source = std::atoi(t.source.c_str());
    }
    const unsigned int id = t.event.rfind("0x", 0) == 0 ? static_cast<unsigned int>(std::strtoul(t.event.c_str() + 2, nullptr, 16))
                                                         : EResourceManager::CalcId(const_cast<char*>(t.event.c_str()));
    std::fprintf(stderr, "[sound-test] live frame %d: event %s (%08X) for source %d\n", liveFrames, t.event.c_str(), id, source);
    player->PlayBySource(id, static_cast<short>(source));
  }
}

signed char* Options() { return static_cast<signed char*>(_globals.m_optionsRecon); }
void SetOption(int offset, int value) {
  if (signed char* o = Options()) o[offset] = static_cast<signed char>(value);
}
int GetOption(int offset) {
  const signed char* o = Options();
  return o ? o[offset] : 10;
}
}  // namespace

// 800B720C
// The disc drive's errors silence the game (mode 5) and restore the mode after.
void DiscErrorCallback(bool error) {
  if (!g_pBoxX) return;
  if (!error) {
    Globs_pSound->SetGameMode(iSavedGameSoundMode);
  } else {
    iSavedGameSoundMode = cSoundPlayer::GetGameMode();
    Globs_pSound->SetGameMode(cSoundPlayer::kSilent);
  }
}

void SoundFadeOutAndSetMode(int mode) {
  if (g_pBoxX) {
    g_pBoxX->m_modes.FadeOutMusic(0x20);
    g_pBoxX->m_modes.FadeOutSFX(0x20);
  }
  if (Globs_pSound) Globs_pSound->SetGameMode(mode);
}

// 80014E30
// (game/bg.cpp) Back in live mode after a load: the live game mode, and sounds resume.
void BGCall_SetGameModeAndResumeSounds() {
  Globs_pSound->SetGameMode(cSoundPlayer::kLive);
  Globs_pSound->ResumeSounds();
}

// 800B7264
cSoundPlayer::cSoundPlayer() { m_cursor = m_events.end(); }

// 800B72CC
// (The queued events' references are not released, as on GC.)
cSoundPlayer::~cSoundPlayer() {
  if (m_ownsBoxX) {
    delete m_sndSys;
    g_pSndSys = nullptr;
    m_sndSys = nullptr;
  }
  m_events.clear();
}

// 800B7368
// With sounds enabled (the build config, EGlobal +0x434): the sound system and the director.
void cSoundPlayer::Initialize() {
  if (m_initialised) return;
  if (_globals.ConfigInt(0x434) == 0) {
    _pActualAudio->StopMusic();
  } else {
    cIGZSndSys* sys = cIGZSndSys::CreateInstance();
    g_pSndSys = sys;
    m_sndSys = sys;
    sys->Initialize();
    if (!m_initialised && m_sndSys) {
      m_ownsBoxX = 1;
      g_pBoxX = new cBoxX;
      g_pBoxX->Init();
      m_initialised = 1;
    }
  }
  _ngcAudio.SetDiscErrorCallback(DiscErrorCallback);
}

// 800B7440
void cSoundPlayer::Shutdown() {
  if (!m_ownsBoxX || !m_initialised) return;
  m_initialised = 0;
  if (g_pBoxX) {
    g_pBoxX->Shutdown();
    delete g_pBoxX;
    g_pBoxX = nullptr;
  }
}

// 800B74C4
// Drops the source's queued events (those before Update's position).
void cSoundPlayer::KillSourceEvent(int source) {
  for (auto it = m_events.begin(); it != m_cursor && it != m_events.end();) {
    if (it->m_source == source) {
      g_soundeventman.DelRefAsync(it->m_event);
      it = m_events.erase(it);
    } else {
      ++it;
    }
  }
  m_cursor = m_events.end();
}

// 800B75BC
void cSoundPlayer::KillAllEvent() {
  m_killedAll = 1;
  for (auto it = m_events.begin(); it != m_cursor && it != m_events.end();) {
    g_soundeventman.DelRefAsync(it->m_event);
    it = m_events.erase(it);
  }
  m_cursor = m_events.end();
}

// 800B76A8
// Plays the queued events that have loaded (15 ms after they were queued), then the game mode and,
// unless sounds are paused, the director's tick and the sound system.
void cSoundPlayer::Update() {
  TestEvents(this);
  for (auto it = m_events.begin(); it != m_events.end();) {
    auto next = std::next(it);
    auto* event = static_cast<ERSoundEvent*>(g_soundeventman.GetRef(it->m_event));
    if (event) {
      m_cursor = next;
      const float time = it->m_time;
      const int source = it->m_source;
      m_events.erase(it);
      g_pBoxX->MappedEvent(event, source, time + 0.015f);
      event->DelRef();
    } else if (!g_soundeventman.IsValid(it->m_event)) {
      // PC: an id the archive does not hold would wait for ever (the GC data had them all).
      simmy::SoundTrace("event %08X not in soundevents", it->m_event);
      m_events.erase(it);
    }
    it = next;
  }
  if (m_initialised && m_ownsBoxX) {
    g_pBoxX->UpdateGameMode();
    if (m_paused == 0) {
      g_pBoxX->Update(0);
      m_sndSys->Update();
    }
  }
}

// 800B7814
// (Not while a movie plays. The silent mode bypasses the director's message.)
void cSoundPlayer::SetGameMode(int mode) {
  if (!m_ownsBoxX || !m_initialised || !g_pBoxX || _pApp->m_movieState != 0) return;
  if (mode == cSoundPlayer::kSilent) g_pBoxX->m_modes.SetMode(cSoundPlayer::kSilent);
  else g_pBoxX->Event(0x24, mode, 0, nullptr, 0.0f);
}

// 800B7898
int cSoundPlayer::GetGameMode() { return g_pBoxX ? g_pBoxX->m_modes.m_mode : cSoundPlayer::kLoading; }

// 800B78B0
void cSoundPlayer::EnableSound(bool on) {
  if (m_initialised && m_ownsBoxX) g_pBoxX->Event(0x22, on ? 1 : 0, 0, nullptr, 0.0f);
}

// 800B7904
void cSoundPlayer::QuietAll() {
  if (!m_initialised || !m_ownsBoxX) return;
  m_cursor = m_events.end();
  g_pBoxX->Event(0x14, 0, 0, nullptr, 0.0f);
}

// 800B7968
// Queues `event` (soundevents id) for `source` (an object id; -2: none at all).
void cSoundPlayer::PlayBySource(unsigned int event, short source) {
  if (!m_initialised || !m_ownsBoxX || event == 0 || source == -2) return;
  g_soundeventman.AddRefAsync(event);
  SoundEventInfo info;
  info.m_source = source;
  info.m_event = event;
  info.m_time = static_cast<float>(_pClockMan->GetSec(&_sysclock));
  simmy::SoundTrace("queue event %08X for source %d", event, source);
  m_events.push_back(info);
}

// 800B7A60
void cSoundPlayer::PlayBySource(char* event, short source) {
  if (!m_initialised || !m_ownsBoxX || !event || source == -2) return;
  simmy::SoundTrace("queue event \"%s\"", event);
  PlayBySource(EResourceManager::CalcId(event), source);
}

// 800B7ACC
void cSoundPlayer::PlayBySource(ERSoundEvent* event, short source) {
  if (!m_initialised || !m_ownsBoxX || !event || source == -2) return;
  g_pBoxX->MappedEvent(event, source, 0.0f);
}

// 800B7B20
void cSoundPlayer::QuietBySourceID(int source) {
  if (m_initialised && m_ownsBoxX) g_pBoxX->Event(0x17, 0, source, nullptr, 0.0f);
}

// 800B7B74
void cSoundPlayer::PauseSFX() {
  if (!m_initialised || !m_ownsBoxX) return;
  if (g_pBoxX) g_pBoxX->Event(0x31, 0, 0, nullptr, 0.0f);
  NotifyViewChange();
}
// 800B7BE4
void cSoundPlayer::ResumeSFX() {
  if (!m_initialised || !m_ownsBoxX) return;
  if (g_pBoxX) g_pBoxX->Event(0x32, 0, 0, nullptr, 0.0f);
  NotifyViewChange();
}
// 800B7C54
void cSoundPlayer::PauseMusic() {
  if (!m_initialised || !m_ownsBoxX) return;
  if (g_pBoxX) g_pBoxX->Event(0x33, 0, 0, nullptr, 0.0f);
  NotifyViewChange();
}
// 800B7CC4
void cSoundPlayer::ResumeMusic() {
  if (!m_initialised || !m_ownsBoxX) return;
  if (g_pBoxX) g_pBoxX->Event(0x34, 0, 0, nullptr, 0.0f);
  NotifyViewChange();
}

// 800B7D34
void cSoundPlayer::PauseSounds() {
  m_paused = 1;
  if (!m_initialised || !m_ownsBoxX) return;
  if (g_pBoxX) {
    g_pBoxX->Event(0x2F, 0, 0, nullptr, 0.0f);
    g_pBoxX->Event(0x15, 0, 0, nullptr, 0.0f);
  }
  NotifyViewChange();
}

// 800B7DC8
void cSoundPlayer::ResumeSounds() {
  if (m_initialised && m_ownsBoxX) {
    if (g_pBoxX) {
      g_pBoxX->Event(0x30, 0, 0, nullptr, 0.0f);
      g_pBoxX->Event(0x16, 0, 0, nullptr, 0.0f);
    }
    NotifyViewChange();
  }
  m_paused = 0;
}

// 800B7E5C
void cSoundPlayer::NotifyViewChange() {
  if (m_initialised && m_ownsBoxX) g_pBoxX->Event(0x2E, 0, 0, nullptr, 0.0f);
}
// 800B7EB0
void cSoundPlayer::NotifyHourChange() {
  if (m_initialised && m_ownsBoxX) g_pBoxX->Event(0x2E, 0, 0, nullptr, 0.0f);
}

// 800B7F04
int cSoundPlayer::GetSFXVolume() { return GetOption(0x1C); }
// 800B7F18
int cSoundPlayer::GetMusicVolume() { return GetOption(0x1D); }
// 800B7F2C
int cSoundPlayer::GetVOXVolume() { return GetOption(0x28); }
// 800B7F40
int cSoundPlayer::GetAmbientVolume() { return GetOption(0x29); }

// 800B7F54
// The options' sound volume sets both the effects and the voices.
void cSoundPlayer::SetOldFXVolume(int volume) {
  simmy::SoundTrace("volume: effects and voices %d", volume);
  SetOption(0x1C, volume);
  if (!m_initialised || !m_ownsBoxX) return;
  g_pBoxX->Event(0x25, volume, 0, nullptr, 0.0f);
  g_pBoxX->Event(0x27, volume, 0, nullptr, 0.0f);
}
// 800B7FDC
void cSoundPlayer::SetMusicVolume(int volume) {
  simmy::SoundTrace("volume: music %d", volume);
  SetOption(0x1D, volume);
  if (m_initialised && m_ownsBoxX) g_pBoxX->Event(0x26, volume, 0, nullptr, 0.0f);
}
// 800B803C
void cSoundPlayer::SetSFXVolume(int volume) {
  SetOption(0x1C, volume);
  if (m_initialised && m_ownsBoxX) g_pBoxX->Event(0x2B, volume, 0, nullptr, 0.0f);
}
// 800B809C
void cSoundPlayer::SetVOXVolume(int volume) {
  simmy::SoundTrace("volume: voices %d", volume);
  SetOption(0x28, volume);
  if (m_initialised && m_ownsBoxX) g_pBoxX->Event(0x27, volume, 0, nullptr, 0.0f);
}
// 800B80FC
void cSoundPlayer::SetAmbientVolume(int volume) {
  simmy::SoundTrace("volume: ambience %d", volume);
  SetOption(0x29, volume);
  if (m_initialised && m_ownsBoxX) g_pBoxX->Event(0x2C, volume, 0, nullptr, 0.0f);
}
