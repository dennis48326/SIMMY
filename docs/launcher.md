# SimmyLauncher

## Overview

`SimmyLauncher.exe` (`src/launcher/`, target `simmy_launcher`, built next to `simmy.exe`) is how players
start the game: pick the disc image once, play alone or in co-op, edit `simmy.ini`, and get a friendly
report if the game failed. It is a separate SDL3 program (its own 2D renderer, no aurora, no game code)
that starts `simmy.exe` as a child process and waits. User-facing behaviour is in the repo
[README](../README.md) ("Playing: SimmyLauncher"); this page is the internals.

## Files

| File | Role |
|---|---|
| [`launcher_main.cpp`](../src/launcher/launcher_main.cpp) | App, pages (home, co-op, options, playing, error modal), mouse/keyboard/gamepad input with repeat, launching, failure detection, test switches |
| [`game_process.cpp`](../src/launcher/game_process.cpp) | Starts the game (`CreateProcessW` with handle-list inheritance and `CREATE_NO_WINDOW` / `posix_spawn` + `setsid`), environment changes, stdout/stderr to files, waits; the game keeps running if the launcher closes |
| [`gcm_disc.cpp`](../src/launcher/gcm_disc.cpp) | Its own small disc reader (independent of the game's): checks the image, reads `opening.bnr` (RGB5A3 banner, names), classifies RVZ/WIA/CISO/WBFS/GCZ/TGC/NKit/Wii/other game/truncated |
| [`launcher_ini.cpp`](../src/launcher/launcher_ini.cpp) | Line editor for simmy.ini: `iso=` before the first section, its own `[launcher]` section (skip_intro, quick_coop, coop_display, sounds) placed before `[graphics]`, graphics keys written like the game's F1 Save, `[keys]` left alone |
| [`canvas.cpp`](../src/launcher/canvas.cpp), [`art.cpp`](../src/launcher/art.cpp), [`sfx.cpp`](../src/launcher/sfx.cpp) | `SDL_RenderGeometry` drawing with feathered AA shapes, stb_truetype text (Poppins, embedded), the plumbob/sky/disc art, synthesized UI sounds |
| `res/`, `fonts/`, `third_party/`, `embed_file.cmake` | Icon/.rc, fonts (OFL), stb_truetype, font embedding |

The game skips the `[launcher]` section when it reads simmy.ini (pc_settings.cpp). The launcher finds
simmy.ini by the same rule as the game (`SIMMY_INI`, else next to the launcher).

## Starting the game

- Play: `simmy.exe --iso "<disc>" --split=gc`; Co-op: `--split=<windows|wide|4x3|gc>`.
- Environment: `SIMMY_SKIP_INTRO=1` with "Skip intro movies" or quick co-op; quick co-op adds
  `SIMMY_TWO_PLAYER=1` and `SIMMY_CAS_ACCEPT=300`. **Every other `SIMMY_*` variable in the launcher's
  environment is removed** (prefix rule, so new test switches are covered) except `SIMMY_INI`; the
  removed names are logged. `SIMMY_LAUNCHER_KEEP_ENV=1` passes them through (development).
- Working directory: simmy.exe's folder. Output: `game_out.txt` (stdout) and `game_log.txt` (stderr) next
  to the launcher. Each launch and its exit code (decimal and hex) go to `simmy_launcher_log.txt`.
- While the game runs the launcher waits minimised and comes back when it ends.

## Failure reporting

A popup only when the exit code is nonzero or a `simmy_crash.txt` was written during this run ("The game
crashed and wrote a crash report" / "The game stopped unexpectedly: ..."), with the files to send and a
button opening their folder; the report's path, the minidump's path and the report's first 40 lines go to
`simmy_launcher_log.txt`. A clean exit (0) shows nothing; "Force quit" is not a failure; the watchdog
snapshot (`simmy_watchdog.txt`) is ignored. See [shutdown-and-crashes.md](shutdown-and-crashes.md).

## Test switches

`--screenshot <out.bmp> [--page home|coop|options|playing|error] [--frames N] [--scale S]`,
`--test-iso <file>` (alias `--iso`), `--game-exe <file>` (e.g. a stub game), `--fake-pads N`, `--focus N`,
`--mouse X,Y`, `--auto-launch play|coop`, `--choose <file>`, `--set key=value` (writes simmy.ini through the
editor), `--write-icon <dir>` (renders the plumbob icon; `res/simmy_launcher.ico` is made from it). Linux
runs use SDL's offscreen video driver and software renderer.

## Gotchas

- `iso=` values containing `;` or `#` would be cut by the game's ini comment rule; the launcher always
  passes `--iso` on the command line, so launches are unaffected.
- The Win32 paths (process creation, environment enumeration, file dialog, DPI on real monitors) were
  written and reviewed on Linux; verify on Windows when changing them.
- `build_simmy.bat` lives outside the repo (next to the distributed zip) and may still pass `--data`.
