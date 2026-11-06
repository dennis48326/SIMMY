// SimmyLauncher: little synthesized interface sounds (no recorded audio is shipped).
#pragma once

namespace launcher {

enum class Sfx { Hover, Click, Back, Toggle, Launch, Error, Disc };

// Opens the default playback device; harmless if there is none.
void sfx_init();
void sfx_shutdown();
void sfx_enable(bool on);
void sfx_play(Sfx sound);

}  // namespace launcher
