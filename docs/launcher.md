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
| [`launcher_main.cpp`](../src/launcher/launcher_main.cpp) | App, pages (home, co-op, options, network play, playing, error modal), mouse/keyboard/gamepad input with repeat, text fields, launching, failure detection, network play's end report, test switches |
| [`net_info.cpp`](../src/launcher/net_info.cpp) | The Network play page's LAN IPv4 addresses (`GetAdaptersAddresses` on Windows, `getifaddrs` elsewhere, plus a UDP "connect" for the default route's address) and the typed address's check |
| [`game_process.cpp`](../src/launcher/game_process.cpp) | Starts the game (`CreateProcessW` with handle-list inheritance and `CREATE_NO_WINDOW` / `posix_spawn` + `setsid`), environment changes, stdout/stderr to files, waits; the game keeps running if the launcher closes |
| [`gcm_disc.cpp`](../src/launcher/gcm_disc.cpp) | Its own small disc reader (independent of the game's): checks the image, reads `opening.bnr` (RGB5A3 banner, names), classifies RVZ/WIA/CISO/WBFS/GCZ/TGC/NKit/Wii/other game/truncated |
| [`launcher_ini.cpp`](../src/launcher/launcher_ini.cpp) | Line editor for simmy.ini: `iso=` before the first section, its own `[launcher]` section (skip_intro, quick_coop, coop_display, sounds, net_join) placed before `[graphics]`, graphics keys written like the game's F1 Save, `[net] port` (a new `[net]` section goes at the end; the game never appends into it), `[keys]` left alone |
| [`canvas.cpp`](../src/launcher/canvas.cpp), [`art.cpp`](../src/launcher/art.cpp), [`sfx.cpp`](../src/launcher/sfx.cpp) | `SDL_RenderGeometry` drawing with feathered AA shapes, stb_truetype text (Poppins, embedded), the plumbob/sky/disc art, synthesized UI sounds |
| `res/`, `fonts/`, `third_party/`, `embed_file.cmake` | Icon/.rc, fonts (OFL), stb_truetype, font embedding |

The game skips the `[launcher]` section when it reads simmy.ini (pc_settings.cpp). The launcher finds
simmy.ini by the same rule as the game (`SIMMY_INI`, else next to the launcher).

## Starting the game

- Play: `simmy.exe --iso "<disc>" --split=gc`; Co-op: `--split=<windows|wide|4x3|gc>`.
- Network play: `simmy.exe --iso "<disc>" --split=gc --host <port> --net-status <launcher dir>/net_status.txt`,
  or `... --join <host>:<port> --net-status ...`. `--split=gc`: a session shows each PC only its own
  player's half, full window, whatever `split=` says ([netplay.md](netplay.md) "Display"); `gc` keeps
  the one-window default size, so the player's own window/aspect settings apply. No quick co-op
  variables (the menus are shared; `SIMMY_TWO_PLAYER` on one PC only would only warn).
- Environment: `SIMMY_SKIP_INTRO=1` with "Skip intro movies" or quick co-op; quick co-op adds
  `SIMMY_TWO_PLAYER=1` and `SIMMY_CAS_ACCEPT=300`. **Every other `SIMMY_*` variable in the launcher's
  environment is removed** (prefix rule, so new test switches are covered) except `SIMMY_INI`; the
  removed names are logged. `SIMMY_LAUNCHER_KEEP_ENV=1` passes them through (development).
- Working directory: simmy.exe's folder. Output: `game_out.txt` (stdout) and `game_log.txt` (stderr) next
  to the launcher. Each launch and its exit code (decimal and hex) go to `simmy_launcher_log.txt`.
- While the game runs the launcher waits minimised and comes back when it ends.

## Network play page

Home -> **Network play** (`Page::Net`). Left: *Host a game* with this PC's LAN addresses (the default
route's first, then private ranges, virtual adapters last; refreshed every 10 s while the page is open),
the port, and firewall / port-forward tips. Right: *Join game* with the address field and on-screen
keys (1-9, 0, `.`, `:`, erase, Clear) for controllers. Bottom: Back and the port field.

- Typing: keyboard text goes to the *edit target* (address, or the port after clicking it) wherever the
  focus is (SDL text input is on while the page is open); Backspace erases (Ctrl+Backspace clears),
  Ctrl+V pastes, Esc goes back; Enter in the address field joins. Controller: A on a field moves to the
  keys, X erases, B goes back. Allowed characters: letters, digits, `.`, `:`, `-` (address, 64 max),
  digits (port, 5 max).
- The address is checked lightly before starting (`check_address`): an IPv4 address (four numbers
  0-255) or a host name, optional `:port` (1-65535); IPv6 is refused. The complaint shows under the field.
- Saved when leaving the page or starting: `[launcher] net_join = <address>` (the last one typed) and
  `[net] port = <port>` (only when not 27450 or already in the file). The launcher never writes
  `[net] host=` / `join=` (those would make every launch a session).
- After the game: `net_status.txt` (the game's `--net-status`, written during this run) decides the
  message: `refused` "Can't play together" + the game's reason (build, disc, mods...); `failed`
  "Couldn't reach the host" / "Couldn't start hosting"; `left` "Network play ended"; `lost`/`timeout`
  "The connection was lost"; `desync` "The games went out of step" with *Open session folder*. `quit`,
  `cancelled` (or still `playing` with exit code 0) show nothing. A crash report still wins. The
  result goes to `simmy_launcher_log.txt`. The page comes back (not home) after a network game.
- The Playing page shows the mode chip ("Network play · Hosting · Player 1") and the address to give
  the friend, or the address being joined.

## Failure reporting

A popup only when the exit code is nonzero or a `simmy_crash.txt` was written during this run ("The game
crashed and wrote a crash report" / "The game stopped unexpectedly: ..."), with the files to send and a
button opening their folder; the report's path, the minidump's path and the report's first 40 lines go to
`simmy_launcher_log.txt`. A clean exit (0) shows nothing; "Force quit" is not a failure; the watchdog
snapshot (`simmy_watchdog.txt`) is ignored. See [shutdown-and-crashes.md](shutdown-and-crashes.md).

## Test switches

`--screenshot <out.bmp> [--page home|coop|options|net|playing|hosting|joining|error] [--frames N] [--scale S]`
(`hosting`/`joining`: the Playing page of a network launch, without starting anything),
`--test-iso <file>` (alias `--iso`), `--game-exe <file>` (e.g. a stub game), `--fake-pads N`, `--focus N`,
`--mouse X,Y`, `--auto-launch play|coop|host|join:<address>` (network: as if *Host a game* / *Join game*
were pressed on the Network play page), `--net-addr <text>` (typed into the address field),
`--show-status <net_status.txt>` (the message a network game that ended so would get), `--choose <file>`, `--set key=value` (writes simmy.ini through the
editor), `--write-icon <dir>` (renders the plumbob icon; `res/simmy_launcher.ico` is made from it). Linux
runs use SDL's offscreen video driver and software renderer.

## Gotchas

- `iso=` values containing `;` or `#` would be cut by the game's ini comment rule; the launcher always
  passes `--iso` on the command line, so launches are unaffected.
- The Win32 paths (process creation, environment enumeration, file dialog, DPI on real monitors, the
  LAN addresses: Winsock + `GetAdaptersAddresses`, linked with `ws2_32` and `iphlpapi`) were written
  and reviewed on Linux; verify on Windows when changing them.
- `build_simmy.bat` lives outside the repo (next to the distributed zip) and may still pass `--data`.
