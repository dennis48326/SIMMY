// Port of game/patch.cpp: the sound event VM. A cTrack runs its event's program (ERSoundTrackData) one tick
// per frame under cHitMan; the program chooses hit patches (cSamplePatch) and plays them on a channel
// (cSampleChannel -> cGZSnd / cGZMusic). cSoundCache holds the tracks, one per event (64 slots).
//
// Instructions are 4 bytes: opcode, A, B, C (B..C an unsigned 16-bit immediate for some). Registers: 1..8
// general (the event's arguments: 1 the source object, 2 volume, 3 pan), 9 volume, 10 pan, 11 the patch
// (setting it makes the channel), 12 fading on, 13 kill when the fade ends, 14 the faded register, 15 its
// target, 16 the step per tick, 17 the duck priority. Opcodes: 2 note on, 4/5 A = imm, 6/0x4B A = B,
// 9 wait A ticks, 0xB wait while the note plays, 0xC end (loop to the start if the event loops), 0xD jump
// +imm, 0xE compare A with 0, 0x10..0x13 A = A +-*/ B, 0x14 compare A with B, 0x18 A = random B..C,
// 0x20 back to the mark, 0x21 set the mark, 0x27 A = a random patch of the hit list, 0x3E..0x43
// conditional jumps (==, !=, >, <, >=, <=), 0x44 hit list = A, 0x45 take sequence group A (fading out the
// group's previous event), 0x48 A = source data (B, C), 0x4D wait until A == B, 0x53/0x55/0x59 A = hit list
// 0 / imm / B, 0x54 A = the hit list the source's data field 0 selects (Simlish: the voice), 0x58 A = a
// random patch of a random list. Others are no-ops or "Illegal Instruction".
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

#include <cstring>

cHitMan* g_pHitMan = nullptr;
int cTrack::s_maxSfxDuckPri = 0;    // 804FDD08
int cTrack::s_maxMusicDuckPri = 0;
int cTrack::s_maxVoxDuckPri = 0;
int cTrack::s_superDuckEnabled = 0;  // 804FDD04
int cTrack::s_activeDuckPri = 0;     // 804FDD00 (s_activeDuckPri)
static float g_musicMinVolume = 0.15f;  // 804FDD0C

namespace {
const unsigned char kNop[4] = {0x0C, 0, 0, 0};  // nopInst (an "end")
inline int S8(unsigned char b) { return static_cast<signed char>(b); }
inline unsigned int Imm16(const unsigned char* ins) { return static_cast<unsigned int>(ins[2] << 8 | ins[3]); }
int Tick() { return g_pHitMan->m_timer.m_count; }
}  // namespace

// ---------------------------------------------------------------------------------------------------
// TrackDataReader

// 801162E0
TrackDataReader::TrackDataReader() = default;
// 801162F4
TrackDataReader::~TrackDataReader() { m_program = nullptr; }
// 80116324
void TrackDataReader::operator=(TrackDataReader& other) {
  m_program = other.m_program;
  m_pc = other.m_pc;
}
// 8011633C
void TrackDataReader::operator=(ERSoundTrackData* program) {
  m_program = program;
  m_pc = 0;
}
// 80116350
void TrackDataReader::operator--() { --m_pc; }
// 80116364
void TrackDataReader::operator+=(int n) { m_pc += static_cast<unsigned int>(n); }

// 80116378
// The next instruction; past the end the last one again (without a program: an "end").
const unsigned char* TrackDataReader::ReadCommand() {
  if (!m_program || m_program->GetProgramSize() == 0) {
    Trace();
    return kNop;
  }
  if (m_pc < m_program->GetProgramSize()) return m_program->GetInstruction(m_pc++);
  Trace();
  return m_program->GetInstruction(m_program->GetProgramSize() - 1);
}
// 80116424
bool TrackDataReader::IsValid() const { return m_program != nullptr; }
// 8011643C
void TrackDataReader::JumpToEnd() { m_pc = m_program->GetProgramSize() - 1; }
// 8011645C
void TrackDataReader::Trace() const {}

// ---------------------------------------------------------------------------------------------------
// cHitMan, cHitControlGroup

// 80116460
// Logs and kills the track (GC: only traced the instruction).
void cTrack::HandleTrackFlowError(const char* message) {
  if (m_event) m_last.Trace();
  simmy::SoundTrace("track %s: %s", m_event ? m_event->GetName() : "?", message);
  Kill();
}

// 801164A4
int cHitMan::RegisterSourceDataRequestHandler(bool (*handler)(int, int, int*)) {
  m_sourceData = handler;
  return 1;
}
// 801164B0
int cHitMan::GetSourceDataField(int source, int field, int* value) {
  return m_sourceData && m_sourceData(source, field, value) ? 1 : 0;
}

// 801164E4
cHitControlGroup* cHitMan::ControlGroup(int id) {
  auto it = m_groups.find(id);
  if (it != m_groups.end()) return it->second;
  auto* group = new cHitControlGroup(id);
  m_groups[id] = group;
  return group;
}

// 80116650
void cHitMan::ClearControlGroups() {
  for (auto& g : m_groups) {
    delete g.second;
    g.second = nullptr;
  }
  m_groups.clear();
}

// 801167B4
cHitControlGroup::cHitControlGroup(int id) : m_id(id), m_volume(0x400) {}

// 801167C8
// Re-mixes the running tracks of the group.
int cHitControlGroup::SetVolume(int volume) {
  m_volume = volume;
  for (cTrack* t : g_pHitMan->m_updateList)
    if (g_pHitMan->m_cache->IsInMemory(t) && t->GetControlGroup() == m_id) t->UpdateVolPan();
  return 1;
}

// 80116888
cHitMan::cHitMan() {
  m_updateNext = m_updateList.end();
  m_cache = new cSoundCache;
}

// 80116994
int cHitMan::Init() {
  m_callbacks = 0;
  for (int g = -10; g < 0x401; ++g) SetSequenceGroupTrackId(g, nullptr);
  m_sndSys = g_pSndSys;
  g_pHitMan = this;
  m_timer.m_count = 0;
  m_timer.m_running = 1;
  m_timer.Update();
  m_timerOn = 1;
  return 1;
}

// 80116A14
cHitMan::~cHitMan() {
  delete m_cache;
  m_cache = nullptr;
}

// 80116B4C
int cHitMan::Shutdown() {
  StopCallbackTimer();
  m_cache->KillAll();
  m_cache->Shutdown();
  ClearControlGroups();
  m_timer.Stop();
  return 1;
}

// 80116D00
void cHitMan::UpdateActiveTrackVolumes() {
  cTrack* tracks[0x40];
  int n = 0;
  for (auto it = m_updateList.begin(); it != m_updateList.end() && n < 0x40; ++it) tracks[n++] = *it;
  for (int i = 0; i < n; ++i)
    if (tracks[i]) tracks[i]->UpdateVolPan();
}

// 80116DC4
void cHitMan::SetSequenceGroupTrackId(int group, ERSoundEvent* event) { m_sequenceGroups[group + 10] = event; }
// 80116DD8
ERSoundEvent* cHitMan::SequenceGroupTrackId(int group) { return m_sequenceGroups[group + 10]; }
// 80116DEC
int cHitMan::StartCallbackTimer() { return 0; }
// 80116DF4
int cHitMan::StopCallbackTimer() { return 0; }

// 80116DFC
// One tick (cBoxX::Update, once a frame): every running track runs the instructions that are due.
void cHitMan::TimerCallback() {
  if (!m_timerOn) return;
  ++m_callbacks;
  if (m_timer.m_running) ++m_timer.m_count;
  m_updateNext = m_updateList.begin();
  while (m_updateNext != m_updateList.end()) {
    auto it = m_updateNext;
    cTrack* t = *it;
    if (t && g_pHitMan->m_cache->IsInMemory(t)) t->HandleTimerCallback();
    if (m_updateNext == it) ++m_updateNext;
  }
}

// 80116EE8
int cHitMan::AddToUpdateList(cTrack* track) {
  RemoveFromUpdateList(track);
  m_updateList.push_back(track);
  return 1;
}

// 80116F50
int cHitMan::RemoveFromUpdateList(cTrack* track) {
  for (auto it = m_updateList.begin(); it != m_updateList.end(); ++it) {
    if (*it != track) continue;
    if (it == m_updateNext) ++m_updateNext;
    m_updateList.erase(it);
    return 1;
  }
  return 0;
}

// 80117034
void cHitMan::DuckMapSetSndobPri(cTrack* track, int priority) {
  if (priority == 0) m_duck.erase(track);
  else m_duck[track] = priority;
  cTrack::s_activeDuckPri = DuckMapMaxPri();
  UpdateActiveTrackVolumes();
}

// 801172C4
int cHitMan::DuckMapSndobPri(cTrack* track) {
  auto it = m_duck.find(track);
  return it == m_duck.end() ? 0 : it->second;
}

// 80117418
int cHitMan::DuckMapMaxPri() {
  int best = 0;
  for (const auto& d : m_duck)
    if (best < d.second) best = d.second;
  return best;
}

// 801174A8
void cHitMan::DuckMapRemoveAll() {
  m_duck.clear();
  cTrack::s_activeDuckPri = 0;
  UpdateActiveTrackVolumes();
}

// ---------------------------------------------------------------------------------------------------
// cTrack

// 801175E0
cTrack::cTrack(ERSoundEvent* event) {
  event->AddRef();
  m_event = event;
  m_time = 0.0f;
  m_hitList = nullptr;
  if (!event->m_hitLists.empty()) m_hitList = &event->m_hitLists[0];
  if (m_hitList && !m_hitList->empty()) m_patchReg = reinterpret_cast<intptr_t>(&(*m_hitList)[0]);
}

// 801176EC
cTrack::~cTrack() {
  OnEndPlaying();
  if (m_channel) {
    m_channel->Shutdown();
    delete m_channel;
    m_channel = nullptr;
  }
  g_pHitMan->m_cache->onTrackDelete(this);
  if (m_event) {
    m_event->DelRef();
    m_event = nullptr;
  }
}

// 801177B0
// The event's chance to play; a ducking event (SFX flag) takes its duck priority.
int cTrack::OnStartPlaying() {
  const unsigned int prob = m_event->m_prob;
  if (prob < 100) {
    if (prob == 0 || !(static_cast<unsigned int>(simmy_sound_rand()) % 100 < prob)) return 0;
  }
  if ((m_event->m_duckFlags & ERSoundEvent::kDuckSfx) && 1 < m_event->m_duckPri)
    SetRegister(kDuckPri, m_event->m_duckPri - 1);
  return 1;
}

// 8011784C
// True when the track ends (the event does not loop).
bool cTrack::OnEndPlaying() {
  if (m_event->m_duckFlags & ERSoundEvent::kDuckSfx) SetRegister(kDuckPri, 0);
  return m_event->m_loop == 0;
}

// 801178A0
ERSoundTrackData* cTrack::StartPos() { return m_event->m_program; }

// 801178AC
// One tick: the fade's step, then the instructions that are due (or the kill when a fade-out ends).
void cTrack::HandleTimerCallback() {
  if (!m_playing || m_paused) return;
  if (m_fading) {
    intptr_t v = RegisterVal(m_fadeReg);
    if (m_fadeStep == 0) m_fading = 0;
    const bool down = m_fadeStep < 1;
    if (v < m_fadeTarget) {
      if (down) m_fadeStep = -m_fadeStep;
    } else if (!down) {
      m_fadeStep = -m_fadeStep;
    }
    v += m_fadeStep;
    if ((0 < m_fadeStep && m_fadeTarget < v) || (m_fadeStep < 0 && v < m_fadeTarget)) {
      v = m_fadeTarget;
      m_fading = 0;
    }
    SetRegister(m_fadeReg, v);
  }
  if (m_killAfterFade == 0 || m_fading != 0) {
    const int now = Tick();
    if (now < 0) HandleTrackFlowError("Timer wrapped around while at breakpoint");
    int n = 0;
    while (m_nextTime != 0 && static_cast<int>(m_nextTime) <= now) {
      if (static_cast<int>(m_nextTime) < 0) HandleTrackFlowError("Instruction time wrapped around while at breakpoint");
      DoCommand();
      if (100 < n++) HandleTrackFlowError("Too many instructions executed without wait (infinite loop?)");
    }
  } else {
    Kill();
    m_killAfterFade = 0;
  }
}

// 80117A60
// Starts the event (or resumes it while paused; a playing "no restart" event stays as it is) with its
// arguments: the source, the volume and the pan. It is left paused: the caller unpauses it.
int cTrack::PlayPause(float time, int source, int volume, int pan) {
  m_time = time;
  if (m_playing) {
    if (m_event->m_noRestart == 1) return 1;
    if (m_paused) {
      m_paused = 1;
      return Unpause();
    }
  }
  const int group = GetControlGroup();
  cHitControlGroup* cg = nullptr;
  if (group == 2 || (cg = g_pHitMan->ControlGroup(group)) == nullptr || cg->m_volume != 0) {
    std::memset(m_regs, 0, sizeof m_regs);
    m_fading = 0;
    m_fadeReg = 0;
    m_fadeTarget = 0;
    m_fadeStep = 0;
    m_killAfterFade = 0;
    m_pan = pan;
    SetRegister(kSource, source);
    SetRegister(kVolumeArg, volume);
    SetRegister(kPanArg, pan);
    m_paused = 0;
    SetRegister(kVolume, volume);
    if (m_event->m_argsType != 0 && m_event->m_argsType < 3) m_instance = source;
    if (OnStartPlaying()) {
      SetPatch(reinterpret_cast<SndEvtHitPatch*>(m_patchReg), time);
      UpdateVolPan();
      m_reader = StartPos();
      m_mark = m_reader;
      m_playing = 1;
      m_nextTime = static_cast<unsigned int>(Tick());
      Pause();
    }
  }
  return 1;
}

// 80117C1C
int cTrack::Pause() {
  m_paused = 1;
  if (m_playing) {
    if (m_channel) m_channel->Pause();
    g_pHitMan->RemoveFromUpdateList(this);
  }
  return 1;
}

// 80117C78
int cTrack::Unpause() {
  m_paused = 0;
  if (m_playing) {
    if (m_channel) m_channel->Unpause();
    m_nextTime = static_cast<unsigned int>(Tick());
    g_pHitMan->AddToUpdateList(this);
  }
  return 1;
}

// 80117CDC
// Ends at once when nothing sounds, otherwise fades the volume out (100 a tick) and then ends.
int cTrack::Stop() {
  if (m_playing) {
    if (!m_channel || !m_channel->IsPlaying()) {
      Kill();
    } else {
      SetRegister(kFadeTarget, 0);
      SetRegister(kFadeReg, kVolume);
      SetRegister(kFadeStep, 100);
      m_killAfterFade = 1;
      SetRegister(kFade, 1);
    }
  }
  return 1;
}

// 80117D84
int cTrack::Kill() {
  m_playing = 0;
  NoteOff();
  SetPatch(nullptr, 0.0f);
  m_reader = StartPos();
  m_nextTime = 0;
  g_pHitMan->RemoveFromUpdateList(this);
  g_pHitMan->DuckMapSetSndobPri(this, 0);
  std::memset(m_regs, 0, sizeof m_regs);
  m_killAfterFade = 0;
  m_fading = 0;
  m_fadeReg = 0;
  m_fadeTarget = 0;
  m_fadeStep = 0;
  return 1;
}

// 80117E2C
intptr_t cTrack::RegisterVal(int reg) {
  switch (reg) {
    case kFade: return m_fading;
    case kVolume: return m_volume;
    case kPan: return m_pan;
    case kPatch: return m_patchReg;
    case kKillAfterFade: return m_killAfterFade;
    case kFadeReg: return m_fadeReg;
    case kFadeTarget: return m_fadeTarget;
    case kFadeStep: return m_fadeStep;
    case kDuckPri: return s_activeDuckPri;
    default: break;
  }
  if (0 < reg && reg < 9) return m_regs[reg - 1];
  return 1;
}

// 80117EFC
int cTrack::SetRegister(int reg, intptr_t value) {
  SetCompareFlags(value, 0);
  int ok = 1;
  switch (reg) {
    case kFade: m_fading = (m_playing && value != 0) ? 1 : 0; break;
    case kVolume:
      m_volume = static_cast<int>(value);
      ok = UpdateVolPan();
      break;
    case kPan:
      m_pan = static_cast<int>(value);
      ok = UpdateVolPan();
      break;
    case kPatch:
      m_patchReg = value;
      ok = SetPatch(reinterpret_cast<SndEvtHitPatch*>(value), m_time);
      if (ok) UpdateVolPan();
      break;
    case kKillAfterFade: m_killAfterFade = static_cast<int>(value); break;
    case kFadeReg: m_fadeReg = static_cast<int>(value); break;
    case kFadeTarget: m_fadeTarget = static_cast<int>(value); break;
    case kFadeStep: m_fadeStep = static_cast<int>(value); break;
    case kDuckPri: g_pHitMan->DuckMapSetSndobPri(this, static_cast<int>(value)); break;
    default:
      if (0 < reg && reg < 9) m_regs[reg - 1] = value;
      break;
  }
  if (ok) return ok;
  HandleTrackFlowError("Could not set register");
  return ok;
}

// 80118074
// The event's hit list `index` (clamped to the lists there are).
intptr_t cTrack::GetVHitList(intptr_t index) {
  const intptr_t n = static_cast<intptr_t>(m_event->m_hitLists.size());
  if (index < 0) index = 0;
  if (n <= index) index = n - 1;
  if (index < 0) return 0;  // PC: an event without lists (GC read before the array)
  return reinterpret_cast<intptr_t>(&m_event->m_hitLists[static_cast<size_t>(index)]);
}

namespace {
// The random choice of the playing instructions: avoids repeating the last choice (up to 4 tries).
int Choose(unsigned int n, int& last) {
  int r = static_cast<int>(static_cast<unsigned int>(simmy_sound_rand()) % n);
  if (1 < static_cast<int>(n)) {
    for (int tries = 0; r == last && tries < 4; ++tries)
      r = static_cast<int>(static_cast<unsigned int>(simmy_sound_rand()) % n);
    last = r;
  }
  return r;
}
}  // namespace

// 801180A4
int cTrack::DoCommand() {
  m_last = m_reader;
  const unsigned char* ins = m_reader.ReadCommand();
  const int op = S8(ins[0]);
  const int a = S8(ins[1]), b = S8(ins[2]), c = S8(ins[3]);
  const unsigned int imm = Imm16(ins);
  auto jump = [&] { m_reader += static_cast<int>(imm); };
  auto setList = [&](intptr_t value) {
    SetRegister(a, value);
    m_less = 0;
    return 1;
  };
  switch (op) {
    case 0x02: NoteOn(); return 1;
    case 0x03: case 0x07: case 0x08: case 0x0F: case 0x15: case 0x16: case 0x17: case 0x19:
    case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F: case 0x22: case 0x46: case 0x47:
    case 0x49: case 0x4A: case 0x4C: case 0x4E: case 0x4F: case 0x50: case 0x51: case 0x52:
    case 0x56: case 0x57:
      return 1;
    case 0x04: case 0x05: SetRegister(a, static_cast<intptr_t>(imm)); return 1;
    case 0x06: case 0x4B: SetRegister(a, RegisterVal(b)); return 1;
    case 0x09: m_nextTime = static_cast<unsigned int>(Tick() + RegisterVal(a)); return 1;
    case 0x0B:
      m_nextTime = static_cast<unsigned int>(Tick() + 2);
      if (m_channel && m_channel->IsPlaying()) --m_reader;
      return 1;
    case 0x0C:
      if (OnEndPlaying()) {
        m_nextTime = static_cast<unsigned int>(Tick() + 2);
        Stop();
        return 1;
      }
      m_reader = StartPos();
      m_mark = m_reader;
      return 1;
    case 0x0D: jump(); return 1;
    case 0x0E: SetCompareFlags(RegisterVal(a), 0); return 1;
    case 0x10: SetRegister(a, RegisterVal(a) + RegisterVal(b)); return 1;
    case 0x11: SetRegister(a, RegisterVal(a) - RegisterVal(b)); return 1;
    case 0x12: {
      const intptr_t d = RegisterVal(b);
      SetRegister(a, d ? RegisterVal(a) / d : 0);  // PC: no division by zero
      return 1;
    }
    case 0x13: SetRegister(a, RegisterVal(a) * RegisterVal(b)); return 1;
    case 0x14: SetCompareFlags(RegisterVal(a), RegisterVal(b)); return 1;
    case 0x18: {
      const intptr_t lo = RegisterVal(b), hi = RegisterVal(c);
      const unsigned int span = static_cast<unsigned int>(hi - lo);
      const unsigned int r = static_cast<unsigned int>(simmy_sound_rand());
      SetRegister(a, (span ? r % span : 0) + lo);
      return 1;
    }
    case 0x20: m_reader = m_mark; return 1;
    case 0x21: m_mark = m_reader; return 1;
    case 0x27:
      if (m_hitList) {
        const unsigned int n = static_cast<unsigned int>(m_hitList->size());
        if (n == 0) {
          HandleTrackFlowError("Attempt to choose from empty track list");
          return 1;
        }
        const int r = Choose(n, m_lastChoice);
        SetRegister(a, reinterpret_cast<intptr_t>(&(*m_hitList)[static_cast<size_t>(r)]));
      }
      m_less = 0;
      return 1;
    case 0x3E: if (m_equal) jump(); return 1;
    case 0x3F: if (!m_equal) jump(); return 1;
    case 0x40: if (m_greater && !m_equal) jump(); return 1;
    case 0x41: if (m_less && !m_equal) jump(); return 1;
    case 0x42: if (m_greater || m_equal) jump(); return 1;
    case 0x43: if (m_less || m_equal) jump(); return 1;
    case 0x44:
      m_hitList = reinterpret_cast<const std::vector<SndEvtHitPatch>*>(RegisterVal(a));
      if (!m_hitList) HandleTrackFlowError("Undefined Hit List");
      return 1;
    case 0x45: {
      const intptr_t group = RegisterVal(a);
      if (0x400 < group) {
        HandleTrackFlowError("Bad group id");
        return 1;
      }
      ERSoundEvent* prev = g_pHitMan->SequenceGroupTrackId(static_cast<int>(group));
      if (prev && prev != m_event && g_pHitMan->m_cache->IsInMemory(prev)) {
        cTrack* other = g_pHitMan->m_cache->GetTrackObject(prev, true);
        if (other->RegisterVal(kVolume) == 0) {
          other->Kill();
        } else {
          if (other->RegisterVal(kFade) == 0 || other->RegisterVal(kFadeReg) != kVolume) {
            other->SetRegister(kFadeStep, 200);
            other->SetRegister(kFadeReg, kVolume);
            other->SetRegister(kFade, 1);
          }
          m_reader = m_last;  // again in two ticks, once the group's last event has faded
          m_nextTime = static_cast<unsigned int>(Tick() + 2);
        }
      }
      g_pHitMan->SetSequenceGroupTrackId(static_cast<int>(group), m_event);
      return 1;
    }
    case 0x48: {
      int value = 0;
      if (g_pHitMan->GetSourceDataField(static_cast<int>(RegisterVal(b)), static_cast<int>(RegisterVal(c)), &value)) {
        SetRegister(a, value);
        return 1;
      }
      HandleTrackFlowError("Illegal source parameter request");
      return 1;
    }
    case 0x4D:
      if (RegisterVal(a) == RegisterVal(b)) return 1;
      --m_reader;
      m_nextTime = static_cast<unsigned int>(Tick() + 2);
      return 1;
    case 0x53: return setList(GetVHitList(0));
    case 0x54: {
      int field = 0;
      if (!g_pHitMan->GetSourceDataField(static_cast<int>(RegisterVal(kSource)), 0, &field)) {
        HandleTrackFlowError("Illegal source parameter request");
        return 1;
      }
      return setList(GetVHitList(field));
    }
    case 0x55: return setList(GetVHitList(static_cast<intptr_t>(imm)));
    case 0x58: {
      const unsigned int lists = static_cast<unsigned int>(m_event->m_hitLists.size());
      if (lists == 0) {
        HandleTrackFlowError("Attempt to choose from empty list of sound lists");
        return 1;
      }
      const auto& list = m_event->m_hitLists[static_cast<unsigned int>(simmy_sound_rand()) % lists];
      const unsigned int n = static_cast<unsigned int>(list.size());
      if (n == 0) {
        HandleTrackFlowError("Attempt to choose from empty sound list");
        return 1;
      }
      const int r = Choose(n, m_lastChoice);
      return setList(reinterpret_cast<intptr_t>(&list[static_cast<size_t>(r)]));
    }
    case 0x59: return setList(GetVHitList(RegisterVal(b)));
    default:
      if (0x28 <= op && op < 0x34) return 1;
      break;
  }
  HandleTrackFlowError("Illegal Instruction");
  return 1;
}

// 80118A64
void cTrack::SetCompareFlags(intptr_t a, intptr_t b) {
  m_less = a < b ? 1 : 0;
  m_equal = a == b ? 1 : 0;
  m_greater = b < a ? 1 : 0;
}

// 80118A90
int cTrack::NoteOn() {
  if (!m_channel) {
    HandleTrackFlowError("Attempt to play note with no channel");
    return 0;
  }
  if (!m_patch) {
    HandleTrackFlowError("Attempt to play note with no patch");
    return 0;
  }
  simmy::SoundTrace("note %s: %s %08X, volume %d pan %d (source %d)", m_event->GetName(),
                    m_patch->m_sample ? "sample" : "stream", m_patch->m_sample ? m_patch->m_sample : m_patch->m_stream,
                    CalculateCurrentVolume(), m_pan, static_cast<int>(m_regs[0]));
  m_channel->NoteOn();
  UpdateVolPan();
  return 1;
}

// 80118B0C
// Replaces the patch and its channel (none for a null patch).
int cTrack::SetPatch(SndEvtHitPatch* patch, float time) {
  if (m_patch) {
    NoteOff();
    delete m_patch;
    m_patch = nullptr;
  }
  if (m_channel) {
    m_channel->Shutdown();
    delete m_channel;
    m_channel = nullptr;
  }
  if (patch) {
    m_patch = new cSamplePatch(patch, time);
    m_patch->SetLooped(m_event->m_loop);
    m_channel = m_patch->CreateChannel();
    UpdateVolPan();
  }
  return 1;
}

// 80118BF0
int cTrack::NoteOff() { return m_channel ? m_channel->NoteOff() : 1; }
// 80118C24
void cTrack::SetVolume(int volume) { SetRegister(kVolume, volume); }
// 80118C4C
void cTrack::SetPan(int pan) { SetRegister(kPan, pan); }

// 80118C74
// The event's volume, the track's and the group's; music never below the minimum share of the music
// option; halved (or silent with super ducking) under a louder-priority ducking event.
int cTrack::CalculateCurrentVolume() {
  int v = static_cast<int>(m_event->m_volume) * m_volume;
  if (v < 0) v += 0x3FF;
  v >>= 10;
  const int group = GetControlGroup();
  if (group != 0) {
    v *= g_pHitMan->ControlGroup(group)->m_volume;
    if (v < 0) v += 0x3FF;
    v >>= 10;
  }
  if (group == 2) {
    const auto* options = static_cast<const signed char*>(_globals.m_optionsRecon);
    const float music = static_cast<float>(options ? options[0x1D] : 10);
    const int floor = static_cast<int>((g_musicMinVolume * 1024.0f * music) / 10.0f);
    if (v < floor) v = floor;
  }
  const int pri = m_event->m_duckPri;
  if ((group == 1 && pri < s_maxSfxDuckPri) || (group == 2 && pri < s_maxMusicDuckPri) ||
      (group == 3 && pri < s_maxVoxDuckPri))
    v = s_superDuckEnabled ? 0 : v / 2;
  return v;
}

// 80118DD4
int cTrack::UpdateVolPan() {
  if (cSampleChannel* ch = m_channel) {
    ch->SetVolume(CalculateCurrentVolume());
    m_channel->SetPan(m_pan);
  }
  return 1;
}

// 80118E28
int cTrack::GetMinRange() { return m_event->m_minRange; }
// 80118E34
int cTrack::GetMaxRange() { return m_event->m_maxRange; }
// 80118E40
int cTrack::GetControlGroup() { return m_event->m_group; }
// 80118E4C
int cTrack::GetArgsType() { return m_event->m_argsType; }
// 80119DB4
int cTrack::SetInstanceId(int id) {
  m_instance = id;
  return 1;
}
// 80119DC0
int cTrack::InstanceId() { return m_instance; }
// 80119DC8
int cTrack::IsPlaying() { return m_playing; }
// 80119DD0
int cTrack::IsPaused() { return m_paused ? 1 : 0; }
// 80119DE8
int cTrack::CheckedRegId(int reg) { return reg; }

// ---------------------------------------------------------------------------------------------------
// cSamplePatch, cSampleChannel

// 80118E58
cSamplePatch::cSamplePatch(SndEvtHitPatch* patch, float time)
    : m_sample(patch->m_sample), m_stream(patch->m_stream), m_time(time) {}
// 80118E84
cSamplePatch::~cSamplePatch() { FreeSnd(); }
// 80118EC4
void cSamplePatch::SetVolume(int volume) { m_channel->SetVolume(volume); }
// 80118EE8
void cSamplePatch::SetPan(int pan) { m_channel->SetPan(pan); }
// 80118F0C
void cSamplePatch::SetLooped(int loop) { m_loop = loop; }
// 80118F14
int cSamplePatch::IsPlaying() { return m_snd ? m_snd->IsPlaying() : 0; }

// 80118F5C
// A sample patch makes a sound effect, a stream patch a music stream (looped when the event's loop byte
// is 2).
int cSamplePatch::CreateSnd() {
  if (m_sample == 0) m_snd = g_pHitMan->m_sndSys->CreateAudioStream(m_stream, m_loop == 2, m_time);
  else m_snd = g_pHitMan->m_sndSys->CreateSoundEffect(m_sample, m_time);
  return 0;
}

// 80118FF8
int cSamplePatch::FreeSnd() {
  if (m_snd) {
    m_snd->Release();
    m_snd = nullptr;
  }
  return 1;
}

// 80119050
cSampleChannel* cSamplePatch::CreateChannel() { return new cSampleChannel(this); }

// 80119088
cSampleChannel::cSampleChannel(cSamplePatch* patch) { SetPatch(patch); }
// 801190C4
cSampleChannel::~cSampleChannel() {
  Shutdown();
  m_snd = nullptr;
  m_patch = nullptr;
}
// 80119110
int cSampleChannel::SetVolume(int volume) {
  if (!m_snd) return 0;
  m_snd->SetVolume(volume);
  return 1;
}
// 80119158
int cSampleChannel::Shutdown() {
  NoteOff();
  return 1;
}
// 8011917C
int cSampleChannel::SetPatch(cSamplePatch* patch) {
  cIGZSnd* old = m_snd;
  m_patch = patch;
  if (old) {
    old->Release();
    m_snd = nullptr;
  }
  m_snd = patch->m_snd;
  if (!m_snd) {
    patch->CreateSnd();
    m_snd = patch->m_snd;
  }
  m_snd->AddRef();
  return 1;
}
// 80119218
void cSampleChannel::SetPan(int pan) {
  if (!m_snd) SetPatch(m_patch);
  m_snd->SetPan(pan);
}
// 80119278
cIGZSnd* cSampleChannel::Snd(int) { return m_patch->m_snd; }
// 80119284
void cSampleChannel::NoteOn() {
  if (!m_snd) SetPatch(m_patch);  // PC: a channel whose note was turned off (GC used the null sound)
  m_snd->SetPosition(0);
  m_snd->Play();
}
// 801192E8
int cSampleChannel::NoteOff() {
  if (m_snd) {
    m_snd->Release();
    m_snd = nullptr;
  }
  return 1;
}
// 80119340
int cSampleChannel::IsPlaying() { return m_snd ? m_snd->IsPlaying() : 0; }
// 80119388
int cSampleChannel::Pause() {
  if (m_patch && m_patch->m_snd) m_patch->m_snd->Pause();
  return 1;
}
// 801193D8
int cSampleChannel::Unpause() {
  if (m_patch && m_patch->m_snd) m_patch->m_snd->Unpause();
  return 1;
}

// ---------------------------------------------------------------------------------------------------
// cTrackCacheHandle, cSoundCache

// 80119428
cTrack* cTrackCacheHandle::GetTrackObject() {
  return m_event ? g_pHitMan->m_cache->GetTrackObject(m_event, true) : nullptr;
}
// 80119468
cTrackCacheHandle::cTrackCacheHandle(ERSoundEvent* event) : m_event(event) {}

// 80119470
cSoundCache::cSoundCache() = default;
// 801194B0
cSoundCache::~cSoundCache() { Shutdown(); }

// 801194F0
void cSoundCache::onTrackDelete(cTrack* track) {
  for (cTrack*& t : m_tracks)
    if (t == track) {
      t = nullptr;
      return;
    }
}

// 80119530
void cSoundCache::Shutdown() {
  for (cTrack*& t : m_tracks) {
    if (!t) continue;
    cTrack* track = t;
    t = nullptr;
    delete track;
  }
}

namespace {
// Removes the event's sources from the director's instance map, kills and deletes the slot's track.
void DropTrack(cTrack*& slot) {
  if (g_pBoxX) g_pBoxX->m_instances.erase(slot->m_event);
  slot->Kill();
  cTrack* track = slot;
  slot = nullptr;
  delete track;
}
}  // namespace

// 8011959C
// Deletes the tracks that have stopped; with more than 32 still playing, the oldest go too.
void cSoundCache::CleanupIdleTracks() {
  int playing = 0;
  for (cTrack*& t : m_tracks) {
    if (!t) continue;
    if (t->m_playing == 0) DropTrack(t);
    else ++playing;
  }
  if (0x20 < playing) {
    int i = m_next;
    do {
      cTrack*& t = m_tracks[i];
      i = (i + 1) % 0x40;
      if (t) {
        --playing;
        DropTrack(t);
      }
    } while (0x20 < playing);
  }
}

// 801199FC
// The highest duck priorities of the playing events per kind (and whether any super-ducks).
void cSoundCache::UpdateDuckingPriorities() {
  cTrack::s_maxVoxDuckPri = 0;
  AmbientScorePlayer::s_duckAmbient = 0;
  cTrack::s_maxSfxDuckPri = 0;
  cTrack::s_maxMusicDuckPri = 0;
  cTrack::s_superDuckEnabled = 0;
  for (cTrack* t : m_tracks) {
    if (!t || !t->m_playing) continue;
    const ERSoundEvent* e = t->m_event;
    const int flags = e->m_duckFlags;
    if ((flags & ERSoundEvent::kDuckSfx) && cTrack::s_maxSfxDuckPri < e->m_duckPri) cTrack::s_maxSfxDuckPri = e->m_duckPri;
    if ((flags & ERSoundEvent::kDuckMusic) && cTrack::s_maxMusicDuckPri < e->m_duckPri)
      cTrack::s_maxMusicDuckPri = e->m_duckPri;
    if ((flags & ERSoundEvent::kDuckVox) && cTrack::s_maxVoxDuckPri < e->m_duckPri) cTrack::s_maxVoxDuckPri = e->m_duckPri;
    if ((flags & ERSoundEvent::kDuckAmbient) && e->m_duckPri != 0) AmbientScorePlayer::s_duckAmbient = 1;
    if (e->m_superDuck) cTrack::s_superDuckEnabled = 1;
  }
}

// 80119AD0
void cSoundCache::KillAll() {
  for (cTrack* t : m_tracks)
    if (t) t->Kill();
}

// 80119B18
void cSoundCache::PauseGroup(int group) {
  for (cTrack* t : m_tracks)
    if (t && (group == -1 || (t->m_event && t->GetControlGroup() == group))) t->Pause();
}

// 80119B9C
void cSoundCache::UnpauseGroup(int group) {
  for (cTrack* t : m_tracks)
    if (t && (group == -1 || (t->m_event && t->GetControlGroup() == group))) t->Unpause();
}

// 80119C20
// The event's track, made in the next free slot when `create`.
cTrack* cSoundCache::GetTrackObject(ERSoundEvent* event, bool create) {
  for (cTrack* t : m_tracks) {
    if (t && t->m_event == event) {
      m_next = (m_next + 1) % 0x40;
      return t;
    }
  }
  if (create) {
    for (int tries = 0; tries < 0x40; ++tries) {
      const int i = m_next;
      m_next = (m_next + 1) % 0x40;
      if (!m_tracks[i]) {
        m_tracks[i] = new cTrack(event);
        return m_tracks[i];
      }
    }
    printTrackList();
  }
  return nullptr;
}

// 80119D48
bool cSoundCache::IsInMemory(cTrack* track) const { return IsInMemory(track->m_event); }
// 80119D6C
bool cSoundCache::IsInMemory(ERSoundEvent* event) const {
  for (cTrack* t : m_tracks)
    if (t && t->m_event == event) return true;
  return false;
}
// 80119DA4
void cSoundCache::printTrackList() {}
