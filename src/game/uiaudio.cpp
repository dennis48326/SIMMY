// Port of game/uiaudio.cpp: UIAUDIO, the front end's sound player. PlaySound loads a sample into the
// next of five voice slots and plays it at the options' sound-effects volume once it has loaded
// (Update polls the slots), stopping the slot after it so that the oldest sound gives way.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

UIAUDIO* UIAUDIO::s_pInstance = nullptr;  // 804FD558

namespace {
// The options' sound-effects volume, 0..10 (OptionsRecon +0x1C, a signed char). PC: OptionsRecon is
// created by EGlobal::LoadIntroRequirements, which is not ported yet; until then its default (10).
float SfxVolume() {
  const auto* options = static_cast<const signed char*>(_globals.m_optionsRecon);
  return static_cast<float>(options ? options[0x1C] : 10) / 10.0f;
}
// _globals +0x434 (config block): sounds are enabled.
bool SoundsEnabled() { return _globals.ConfigInt(0x434) != 0; }
}  // namespace

// 801770C0
UIAUDIO::UIAUDIO() {
  UIAUDIORecord* prev = nullptr;
  for (int i = 0; i < kRecords; ++i) {
    auto* r = new UIAUDIORecord;
    if (!m_free) {
      m_free = r;
    } else {
      prev->m_next = r;
      r->m_prev = prev;
    }
    prev = r;
  }
  for (int i = 0; i < kVoices; ++i) {
    m_voices[i] = _pActualAudio->AudioAllocVoice();
    m_samples[i] = nullptr;
    m_sampleIds[i] = 0;
    m_volumeLeft[i] = 0.0f;
    m_volumeRight[i] = 0.0f;
  }
}

// 801771D8
UIAUDIO::~UIAUDIO() {
  UIAUDIORecord* r = m_free;
  if (m_head) {  // put the installed list in front of the free list
    m_tail->m_next = m_free;
    if (m_free) m_free->m_prev = m_tail;
    r = m_head;
  }
  while (r) {
    UIAUDIORecord* next = r->m_next;
    delete r;
    r = next;
  }
  for (int i = 0; i < kVoices; ++i) {
    if (m_voices[i]) {
      StopSound(i);
      _pActualAudio->FreeVoice(m_voices[i]);
      m_voices[i] = nullptr;
    }
  }
}

// 801772D4
void UIAUDIO::AddEntry(char* name, unsigned int sound, char* group) {
  UIAUDIORecord* r = m_free;
  m_free = r->m_next;
  if (m_free) m_free->m_prev = nullptr;
  if (!m_head) {
    m_head = r;
    r->m_prev = nullptr;
  } else {
    m_tail->m_next = r;
    r->m_prev = m_tail;
  }
  m_tail = r;
  r->m_next = nullptr;
  r->m_group = group;
  r->m_name = name;
  r->m_sound = sound;
}

// 8017733C
void UIAUDIO::RemoveEntry(UIAUDIORecord* r) {
  if (r->m_prev) r->m_prev->m_next = r->m_next;
  if (r->m_next) r->m_next->m_prev = r->m_prev;
  if (m_head == r) m_head = r->m_next;
  if (m_tail == r) m_tail = r->m_prev;
  r->m_next = nullptr;
  r->m_prev = nullptr;
  if (m_free) {
    r->m_next = m_free;
    m_free->m_prev = r;
  }
  m_free = r;
  r->m_name = nullptr;
  r->m_sound = 0;
  r->m_group = nullptr;
}

// 801773D4
// Replaces an entry of the same name. False when no record is free.
bool UIAUDIO::InstallEntry(char* name, unsigned int sound, char* group) {
  if (!m_free) return false;
  bool removed = false;
  for (UIAUDIORecord* r = m_head; r && !removed;) {
    UIAUDIORecord* next = r->m_next;
    if (std::strcmp(r->m_name, name) == 0) {
      RemoveEntry(r);
      removed = true;
    }
    r = next;
  }
  AddEntry(name, sound, group);
  return true;
}

// 8017747C
bool UIAUDIO::UnInstallEntry(char* name) {
  for (UIAUDIORecord* r = m_head; r; r = r->m_next) {
    if (r->m_name == name || std::strcmp(r->m_name, name) == 0) {
      RemoveEntry(r);
      return true;
    }
  }
  return false;
}

// 801774F4
bool UIAUDIO::UnInstallAllEntries(char* group) {
  bool any = false;
  for (UIAUDIORecord* r = m_head; r;) {
    UIAUDIORecord* next = r->m_next;
    if (r->m_group == group) {
      RemoveEntry(r);
      any = true;
    }
    r = next;
  }
  return any;
}

// 80177554
bool UIAUDIO::ContainsEntry(char* name) {
  for (UIAUDIORecord* r = m_head; r; r = r->m_next)
    if (std::strcmp(r->m_name, name) == 0) return true;
  return false;
}

// 801775B4
// The entry's sample id (0 if there is none); the entry moves to the front of the list.
unsigned int UIAUDIO::GetSound(char* name) {
  UIAUDIORecord* r = m_head;
  while (r && std::strcmp(r->m_name, name) != 0) r = r->m_next;
  if (!r) return 0;
  if (r != m_head) {
    if (r == m_tail) m_tail = r->m_prev;
    else r->m_next->m_prev = r->m_prev;
    r->m_prev->m_next = r->m_next;
    r->m_prev = nullptr;
    r->m_next = m_head;
    m_head->m_prev = r;
    m_head = r;
  }
  return r->m_sound;
}

// 80177670
void UIAUDIO::PlaySound(unsigned int sound) {
  if (!SoundsEnabled()) return;
  const int i = m_nextVoice;
  if (!m_voices[i]) return;
  if (m_sampleIds[i]) {
    _pAudiosampleman->DelRefAsync(m_sampleIds[i]);
    m_samples[i] = nullptr;
  }
  m_sampleIds[i] = sound;
  _pAudiosampleman->AddRefAsync(m_sampleIds[i]);
  UpdateAudioLoad(i);
  m_nextVoice = i + 1 < kVoices ? i + 1 : 0;
  StopSound(m_nextVoice);
}

// 8017775C
void UIAUDIO::Update() {
  for (int i = 0; i < kVoices; ++i) UpdateAudioLoad(i);
}

// 801777A0
// Once the slot's sample has loaded: bind it and play it at the sound-effects volume.
void UIAUDIO::UpdateAudioLoad(int i) {
  if (!m_sampleIds[i] || m_samples[i]) return;
  m_samples[i] = static_cast<ERSampledata*>(_pAudiosampleman->GetRef(m_sampleIds[i]));
  if (!m_samples[i]) return;
  m_volumeLeft[i] = 1.0f;
  m_volumeRight[i] = 1.0f;
  m_volumeLeft[i] *= SfxVolume();
  m_volumeRight[i] *= SfxVolume();
  _pActualAudio->AudioBindVoice(m_voices[i], m_sampleIds[i]);
  EVoiceDesc desc;
  desc.m_mask = 0xF;
  desc.m_volumeLeft = m_volumeLeft[i];
  desc.m_volumeRight = m_volumeRight[i];
  desc.m_pitch = 1.0f;
  desc.m_play = 1;
  _pActualAudio->SetVoiceState(m_voices[i], desc);
}

// 80177948
void UIAUDIO::StopSound(int i) {
  if (!SoundsEnabled() || !m_voices[i] || !m_sampleIds[i]) return;
  _pAudiosampleman->DelRefAsync(m_sampleIds[i]);
  m_sampleIds[i] = 0;
  m_samples[i] = nullptr;
  _pActualAudio->UnbindVoice(m_voices[i]);
}

// 801779DC
UIAUDIO* UIAUDIO::Instance() {
  if (!s_pInstance) s_pInstance = new UIAUDIO;
  return s_pInstance;
}

// 80177A18
void UIAUDIO::Shutdown() {
  if (s_pInstance) {
    delete s_pInstance;
    s_pInstance = nullptr;
  }
}
