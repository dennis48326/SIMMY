// Port of the behaviour primitives that play sounds (game/objectsim.cpp): 0x17 Play Sound and 0x30 Kill
// Sounds. A tree names a sound by its id in the object file's 'SndInfo' list (QuickDataSoundInfo: the
// ResFile row's +0x18, {id, sound event id} 8 bytes each, sorted), else the global file's; the event plays
// for the stack object or for the object itself.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"
#include "simmy/sound_trace.hpp"

namespace {
// GC: NamespaceSelector::GetSoundInfo (800C7EF4) -> GetCacheItem -> QuickDataSoundInfo::LoadFromDataID
// (801456E0) with FindRes<SndInfo> (803ADF68): the event id of sound `id` in `file`'s list, or 0.
unsigned int SoundInfoEvent(const unsigned char* file, int id) {
  if (!file) return 0;
  const auto* list = static_cast<const unsigned char*>(ERQuickdata::Ptr(file, 0x18));
  if (!list) return 0;
  const unsigned int n = ERQuickdata::U32(list - 4, 0);
  for (unsigned int i = 0; i < n; ++i)
    if (ERQuickdata::S32(list + i * 8, 0) == id) return ERQuickdata::U32(list + i * 8, 4);
  return 0;
}
}  // namespace

// 80109BEC
// Operand +0: 0 my sounds, else the stack object's.
RET_UNKNOWN cXObjectImpl::TryKillSounds(StackElem* frame, KillSoundsParam* param) {
  const auto* p = reinterpret_cast<const unsigned char*>(param);
  const short source = simmy::be16(p) == 0 ? m_id : frame->m_stackObject;
  if (Globs_pSound) Globs_pSound->QuietBySourceID(source);
  return 1;
}

// 80109C30
// Operand +0 the sound id, +4 flags (bit 1: for the stack object; 0xFF: for me).
RET_UNKNOWN cXObjectImpl::TryPlaySound(StackElem* frame, PlaySoundParam* param) {
  const auto* p = reinterpret_cast<const unsigned char*>(param);
  const short id = static_cast<short>(simmy::be16(p));
  const unsigned char flags = p[4];
  const short source = (flags == 0xFF || !(flags & 2)) ? m_id : frame->m_stackObject;
  Behavior* behavior = frame->m_behavior;
  unsigned int event = behavior ? SoundInfoEvent(behavior->m_resFile, id) : 0;
  if (!event && behavior) event = SoundInfoEvent(behavior->m_globalResFile, id);
  simmy::SoundTrace("object %d plays sound %d -> event %08X (source %d)", m_id, id, event, source);
  if (event && Globs_pSound) Globs_pSound->PlayBySource(event, source);
  return 1;
}
