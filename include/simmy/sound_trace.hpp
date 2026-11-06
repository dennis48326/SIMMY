// PC development aid: SIMMY_SOUND_TRACE=1 logs the game's sound events on stderr as "[snd] ..." (UI sounds,
// sound events queued and played, game modes, music, sample notes with volume and pan).
#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace simmy {
inline bool SoundTraceOn() {
  static const bool on = [] {
    const char* e = std::getenv("SIMMY_SOUND_TRACE");
    return e && *e && *e != '0';
  }();
  return on;
}
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
inline void SoundTrace(const char* fmt, ...) {
  if (!SoundTraceOn()) return;
  std::va_list ap;
  va_start(ap, fmt);
  std::fputs("[snd] ", stderr);
  std::vfprintf(stderr, fmt, ap);
  std::fputc('\n', stderr);
  va_end(ap);
}
}  // namespace simmy
