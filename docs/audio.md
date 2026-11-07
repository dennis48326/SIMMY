# Audio

## Overview

Four layers, bottom up:

1. **PC mixer** ([`pc_audio.cpp`](../src/platform/pc_audio.cpp), [`pc_audio.hpp`](../include/simmy/pc_audio.hpp)):
   what the GC's AX/MIX/AI did, on an SDL3 audio stream: float stereo 48 kHz, 5 ms frames, DSP-ADPCM
   decode, linear resampling per voice, MIX-style fader/pan/mute, PC buses, a WAV recorder.
2. **Engine audio** (`ENgcAudio` = `_pActualAudio` [`e_ngcaudio.cpp`](../src/engine/e_ngcaudio.cpp),
   `ENgcSFXStreamer` 62 voices [`e_ngcsamplestream.cpp`](../src/engine/e_ngcsamplestream.cpp),
   `ENGCAudioStreamer` music [`e_ngcstream.cpp`](../src/engine/e_ngcstream.cpp), THP movie audio in
   [`e_ngcmovie.cpp`](../src/engine/e_ngcmovie.cpp)) and the audio resources (samples, streams, sound
   events, sound-track programs, ambient scores).
3. **Game sound director "BoxX"** ([`gamesound.cpp`](../src/game/gamesound.cpp), [`boxx.cpp`](../src/game/boxx.cpp),
   [`gzsndsys.cpp`](../src/game/gzsndsys.cpp), [`patch.cpp`](../src/game/patch.cpp), [`audioinfo.cpp`](../src/game/audioinfo.cpp),
   [`ambientscoreplayer.cpp`](../src/game/ambientscoreplayer.cpp), [`ambientsoundplayer.cpp`](../src/game/ambientsoundplayer.cpp),
   [`footsteps.cpp`](../src/game/footsteps.cpp), [`soundprims.cpp`](../src/game/soundprims.cpp)): events,
   positional sound per player camera, music by game mode, ambience.
4. **UI sounds** (`UIAUDIO`, [`uiaudio.cpp`](../src/game/uiaudio.cpp); `UIAUDIOTarget`
   [`uiaudiotarget.cpp`](../src/game/uiaudiotarget.cpp)): played directly on engine voices.

## Data (on the disc)

| Archive | Content |
|---|---|
| `samples.arc` | 7,695 DSP-ADPCM samples (all mono): effects, UI clicks, footsteps, Simlish. If missing the game runs silent for those and logs `[audio] samples.arc not found` once |
| `audiostr.arc` | 31 music streams (title track, CAS/build-buy music, stereo/DJ stations) |
| `soundeve.arc` | 2,364 sound events (`ERSoundEvent` 'SEVT'): group 1 SFX, 2 music, 3 VOX |
| `soundtra.arc` | 3 sound-track programs (`ERSoundTrackData` 'STRD'): "playfromhitlist", "playvox", "playhitpatch" |
| `ambients.arc` | Ambient scores (`ERAmbientScore`, "score%d" by house) |

## The sound director

- **`cSoundPlayer`** (`Globs_pSound`): the game's API. `PlayBySource(event, source object id)` queues an
  event; `Update` (from `ESimsApp::UpdateAudio` when no movie plays) plays queued events once their
  resource loaded, runs game-mode updates and the BoxX/`cGZSndSys` updates. `SetGameMode`: 1 live,
  2 front end, 3 loading, 4 CAS, 5 silent, 6 build/buy (enum in [`gamesound.hpp`](../include/simmy/classes/gamesound.hpp)).
- **`cBoxX`** (`g_pBoxX`): an `Event(msg, ...)` dispatcher (play, kill source, quiet all, pause, volumes,
  game mode). Re-pans/attenuates every playing instance each frame from **one listener per player camera**
  (eye/target of `_gd.m_players[i].m_cam`), summing the players' volumes into one stereo mix (GC behaviour:
  a sound is heard by whichever camera is close). A listener whose sim is in social mode hears the
  other sounds ducked (`g_social_duck` 0.45).
- **`cHitMan` / `cTrack`**: a tiny sound VM runs each event's program (registers for volume, pan, patch...).
  A patch with a sample id plays a `cGZSnd` (sample voice); otherwise a `cGZMusic` (stream, one at a time,
  fades).
- **`cSoundModeManager`**: per frame music volume/fades from the options, the loading loop, music per mode
  (front end: the title track; CAS and build/buy: `music_24_bbcas1..3`).
- **Callers**: animation notes (sim sounds, footsteps `footstep_<surface>_<shoe|heel|bare>_<step>`),
  object animation notes, primitives 0x17 Play Sound / 0x30 Kill Sounds, object deletion, game-mode
  changes in the state machines, the options screen's volumes (`SetOldFXVolume`, `SetMusicVolume`,
  `SetAmbientVolume`, `SetVOXVolume`).
- **Ambience**: `AmbientScorePlayer` / `AmbientSoundPlayer` play the lot's score; far sounds are inaudible
  by distance (GC).

UI sounds: C++ targets call `UIAUDIO::PlaySound(id)`; movies set `audio:NAME`, which `UIAUDIOTarget`
resolves through the names each target installed (`MMU_UI_BROWSE_SAMPLE`, `PAZ_SndBrowse`, ...).

## Volumes

The options reach the game through `cSoundPlayer` (BoxX group volumes and the music volume), as on GC.
The PC buses (`kBusSfx`, `kBusMusic`, `kBusAmbient`, `kBusVoice`, `kBusMovie`) are an extra stage that
stays at 1 unless a PC feature sets it (applying the options there too would apply them twice).

## The sound RNG

All sound code draws from **`simmy_sound_rand()`** ([`cassimstate.cpp`](../src/game/cassimstate.cpp)): the
same generator as the trees' `GetNextRandomNumber`, with its own state. On GC the sound code shared the
tree RNG, so whether a sound started (audio device, mixer progress in wall time, camera position) changed
the simulation's random sequence. Keep it separate: never draw the sim RNG in sound code, never the sound
RNG in simulation code ([determinism-and-netplay.md](determinism-and-netplay.md)).

## PC deviations (kept on purpose)

- `cGZSndSys::Update`: a loaded sound also plays once its start time has come (fast PC loads reached
  play before the event time otherwise).
- The effects fade-out a load starts is completed before live mode starts (on GC the loading screen
  outlasted it; on PC the closing "quiet all" stopped the new lot's ambience).
- `cSoundCacheItem` gives up when `samples.arc` is missing or an id is not in the index; queued events
  with unknown ids are dropped.
- Sample loading uses the GC mono test (one channel unless the resource is <= 0x61 bytes).

## Dev aids

`SIMMY_AUDIO=0` (no audio), `SIMMY_AUDIO_WAV` / `SIMMY_AUDIO_DUMP=<file.wav>` (record the final mix),
`SIMMY_AUDIO_TEST=<list>` (play streams/samples through the game's calls), `SIMMY_SOUND_TRACE=1` (`[snd]`
lines: events, notes, volume/pan, streams, modes), `SIMMY_SOUND_EVENT="<frames>:<event>[@<source>];..."`
(play events by name in live mode). Headless runs use SDL's "dummy" driver (wall-clock paced).

## Gaps

`AmbientScorePlayer::SetPause` stops at the first valid sound (GC quirk kept);
CAS has no score (silent on GC too).
